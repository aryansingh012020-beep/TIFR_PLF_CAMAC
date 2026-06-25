/* =========================================================================
 * lamps_shm.c — LAMPS shared-memory implementation
 *
 * Implements the writer side (called from daq_engine.c after each buffer)
 * and the reader side (called from lamps_epics_bridge.c).
 *
 * Build: compiled into both lamps and lamps_epics_bridge.
 *        Add lamps_shm.o to SETS in Makefile.
 * ========================================================================= */

#include "lamps_shm.h"

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdatomic.h>

/* -------------------------------------------------------------------------
 * Internal state
 * -------------------------------------------------------------------------*/
static LampsShmBlock *s_shm   = NULL;
static int            s_is_writer = 0;
static size_t         s_shm_size  = sizeof(LampsShmBlock);

/* -------------------------------------------------------------------------
 * External globals needed from LAMPS (writer side only).
 * These are declared here as extern and resolved at link time against lamps.
 * The bridge does NOT link these — it only uses lamps_shm_open_read().
 * -------------------------------------------------------------------------*/
#ifdef LAMPS_SHM_WRITER_SIDE

#include <gtk/gtk.h>  /* must come first: lamps.h uses GtkWidget */
#include <glib.h>
#include "lamps.h"

extern struct Setup    Setup;
extern guint16        *Oned16;
extern guint32        *Oned32;
extern gint            Off1d[];
extern gulong          XTotEvts;
extern glong           BuffersAcquired, BuffersProcessed;
extern gulong          ScalerCurr[];
extern gdouble         StartTime;
extern gchar           RunName[];
extern enum AcqSignal  AcqSignal;

/* Provided by daq_engine.c — event rate computed from elapsed time.        */
extern gdouble         StartTime;

#endif /* LAMPS_SHM_WRITER_SIDE */

/* =========================================================================
 * lamps_shm_open_write()
 * Create and mmap the shared memory segment (writer / LAMPS process).
 * Returns 0 on success, -1 on error.
 * ========================================================================= */
int lamps_shm_open_write(void)
{
    int fd;

    shm_unlink(LAMPS_SHM_NAME);   /* clean up any stale segment             */

    fd = shm_open(LAMPS_SHM_NAME,
                  O_CREAT | O_RDWR,
                  S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    if (fd < 0) {
        perror("[LAMPS SHM] shm_open(write)");
        return -1;
    }

    if (ftruncate(fd, (off_t)s_shm_size) < 0) {
        perror("[LAMPS SHM] ftruncate");
        close(fd);
        return -1;
    }

    s_shm = mmap(NULL, s_shm_size, PROT_READ | PROT_WRITE,
                 MAP_SHARED, fd, 0);
    close(fd);

    if (s_shm == MAP_FAILED) {
        perror("[LAMPS SHM] mmap(write)");
        s_shm = NULL;
        return -1;
    }

    /* Initialise header */
    memset(s_shm, 0, s_shm_size);
    s_shm->magic      = LAMPS_SHM_MAGIC;
    s_shm->version    = LAMPS_SHM_VERSION;
    s_shm->generation = 0;
    s_is_writer       = 1;

    fprintf(stderr, "[LAMPS SHM] shared memory segment created (%zu bytes)\n",
            s_shm_size);
    return 0;
}

/* =========================================================================
 * lamps_shm_open_read()
 * Open existing segment read-only (bridge / reader process).
 * Returns 0 on success, -1 if the segment doesn't exist yet.
 * ========================================================================= */
int lamps_shm_open_read(void)
{
    int fd;

    fd = shm_open(LAMPS_SHM_NAME, O_RDONLY, 0);
    if (fd < 0) {
        perror("[LAMPS BRIDGE] shm_open(read)");
        return -1;
    }

    s_shm = mmap(NULL, s_shm_size, PROT_READ, MAP_SHARED, fd, 0);
    close(fd);

    if (s_shm == MAP_FAILED) {
        perror("[LAMPS BRIDGE] mmap(read)");
        s_shm = NULL;
        return -1;
    }

    if (s_shm->magic != LAMPS_SHM_MAGIC) {
        fprintf(stderr, "[LAMPS BRIDGE] bad magic 0x%08X\n", s_shm->magic);
        munmap(s_shm, s_shm_size);
        s_shm = NULL;
        return -1;
    }

    fprintf(stderr, "[LAMPS BRIDGE] shared memory attached (v%u)\n",
            s_shm->version);
    return 0;
}

/* =========================================================================
 * lamps_shm_update()
 * Called by the DAQ engine after processing each buffer.
 * Pushes current spectra and statistics into shared memory.
 * Uses seqlock: increment generation to odd (writer active),
 * update data, then increment to even (reader safe).
 * ========================================================================= */
void lamps_shm_update(void)
{
#ifdef LAMPS_SHM_WRITER_SIDE
    int    i, ch, n;
    time_t now_t;
    double now, elapsed;

    if (!s_shm) return;

    /* --- Begin critical section ----------------------------------------- */
    /* Mark generation odd: tells readers an update is in progress.         */
    atomic_fetch_add_explicit((_Atomic uint64_t *)&s_shm->generation, 1,
                              memory_order_release);

    /* Metadata */
    strncpy(s_shm->run_name, RunName, 39);
    s_shm->run_name[39] = '\0';

    switch (AcqSignal) {
    case 0 /*Stop*/:  s_shm->acq_state = LAMPS_ACQ_STOP;  break;
    case 1 /*Pause*/: s_shm->acq_state = LAMPS_ACQ_PAUSE; break;
    default:          s_shm->acq_state = LAMPS_ACQ_RUN;   break;
    }

    s_shm->total_events       = (uint64_t)XTotEvts;
    s_shm->buffers_acquired   = (uint64_t)BuffersAcquired;
    s_shm->buffers_processed  = (uint64_t)BuffersProcessed;

    now = (double)time(NULL);
    elapsed = (StartTime > 0.0) ? (now - StartTime) : 0.0;
    s_shm->elapsed_seconds = elapsed;
    s_shm->event_rate = (elapsed > 0.0)
                        ? (double)XTotEvts / elapsed
                        : 0.0;

    /* Scaler values */
    n = Setup.Scaler.NSc;
    if (n > LAMPS_MAX_SCALER_EPICS) n = LAMPS_MAX_SCALER_EPICS;
    s_shm->n_scaler = (uint32_t)n;
    for (i = 0; i < n; ++i) {
        strncpy(s_shm->scaler_name[i], Setup.Scaler.Name[i], 31);
        s_shm->scaler_name[i][31] = '\0';
        s_shm->scaler_val[i] = (uint32_t)ScalerCurr[i];
    }

    /* 1D spectra */
    n = Setup.Oned.N;
    if (n > LAMPS_MAX_ONED_EPICS) n = LAMPS_MAX_ONED_EPICS;
    s_shm->n_oned = (uint32_t)n;

    for (i = 0; i < n; ++i) {
        int     nchan = Setup.Oned.Chan[i];
        uint32_t *dst  = &s_shm->oned_data[i * LAMPS_MAX_CHAN_EPICS];

        if (nchan > LAMPS_MAX_CHAN_EPICS) nchan = LAMPS_MAX_CHAN_EPICS;

        strncpy(s_shm->oned_name[i], Setup.Oned.Name[i], 31);
        s_shm->oned_name[i][31] = '\0';
        s_shm->oned_chan[i] = (uint32_t)nchan;
        s_shm->oned_wsz[i]  = (uint32_t)Setup.Oned.WSz;

        if (Setup.Oned.WSz == 1) {
            /* 16-bit: upcast to 32-bit for the bridge                     */
            const guint16 *src = Oned16 + Off1d[i];
            for (ch = 0; ch < nchan; ++ch)
                dst[ch] = (uint32_t)src[ch];
        } else {
            /* 32-bit: direct copy                                          */
            const guint32 *src = Oned32 + Off1d[i];
            for (ch = 0; ch < nchan; ++ch)
                dst[ch] = src[ch];
        }
    }

    /* --- End critical section: mark generation even -------------------- */
    atomic_fetch_add_explicit((_Atomic uint64_t *)&s_shm->generation, 1,
                              memory_order_release);
#endif /* LAMPS_SHM_WRITER_SIDE */
}

/* =========================================================================
 * lamps_shm_snapshot()
 * Reader: copy a consistent snapshot using the seqlock protocol.
 * Returns 0 on success, -1 if LAMPS is not running or segment gone.
 * ========================================================================= */
int lamps_shm_snapshot(LampsShmBlock *dst)
{
    uint64_t gen1, gen2;

    if (!s_shm) return -1;

    /* Seqlock read: retry until we catch a stable (even) generation.       */
    do {
        gen1 = atomic_load_explicit((_Atomic uint64_t *)&s_shm->generation,
                                    memory_order_acquire);
        if (gen1 & 1) {
            /* Writer active — yield and retry                               */
            usleep(500);
            continue;
        }
        memcpy(dst, (const void *)s_shm, s_shm_size);
        gen2 = atomic_load_explicit((_Atomic uint64_t *)&s_shm->generation,
                                    memory_order_acquire);
    } while (gen1 != gen2);

    if (dst->magic != LAMPS_SHM_MAGIC) return -1;
    return 0;
}

/* =========================================================================
 * lamps_shm_close()
 * Writer unlinks the segment. Reader just unmaps.
 * ========================================================================= */
void lamps_shm_close(void)
{
    if (s_shm) {
        munmap(s_shm, s_shm_size);
        s_shm = NULL;
    }
    if (s_is_writer) {
        shm_unlink(LAMPS_SHM_NAME);
        s_is_writer = 0;
        fprintf(stderr, "[LAMPS SHM] shared memory removed\n");
    }
}
