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
#include <time.h>    /* clock_gettime, CLOCK_MONOTONIC */

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

    if (!s_shm) return;

    /* -----------------------------------------------------------------------
     * Resolve acq_state FIRST — before the rate-limiter — so that a state
     * transition (e.g. RUN→STOP) can bypass the rate-limit and write
     * through immediately.  Without this, stopping acquisition within 100ms
     * of the last update leaves the old event_rate value in SHM.
     * --------------------------------------------------------------------- */
#define LAMPS_SHM_MIN_INTERVAL_NS  100000000ULL   /* 100 ms in nanoseconds */
    LampsAcqState cur_state_rl;
    switch (AcqSignal) {
    case 0 /*Stop*/:  cur_state_rl = LAMPS_ACQ_STOP;  break;
    case 1 /*Pause*/: cur_state_rl = LAMPS_ACQ_PAUSE; break;
    default:          cur_state_rl = LAMPS_ACQ_RUN;   break;
    }

    /* Fix 2B: rate-limit, but bypass on any state transition.              */
    {
        static uint64_t    s_last_update_ns  = 0;
        static LampsAcqState s_prev_state_rl = LAMPS_ACQ_STOP;
        struct timespec ts;
        uint64_t now_ns;
        int state_changed = (cur_state_rl != s_prev_state_rl);

        clock_gettime(CLOCK_MONOTONIC, &ts);
        now_ns = (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;

        if (!state_changed &&
            (now_ns - s_last_update_ns < LAMPS_SHM_MIN_INTERVAL_NS))
            return;   /* too soon and no state change — skip expensive copy */

        s_last_update_ns  = now_ns;
        s_prev_state_rl   = cur_state_rl;
    }

    /* -----------------------------------------------------------------------
     * Fix 2C: Instantaneous event rate using Δevents / Δtime.
     * Reuses cur_state_rl resolved above — no second AcqSignal switch.
     * --------------------------------------------------------------------- */
    {
        static uint64_t      s_prev_events  = 0;
        static uint64_t      s_prev_time_ns = 0;
        static LampsAcqState s_prev_state   = LAMPS_ACQ_STOP;

        struct timespec ts_now;
        uint64_t now_ns;
        double   event_rate_val = 0.0;

        clock_gettime(CLOCK_MONOTONIC, &ts_now);
        now_ns = (uint64_t)ts_now.tv_sec * 1000000000ULL
               + (uint64_t)ts_now.tv_nsec;

        /* Reset delta baseline when a fresh run starts */
        if (cur_state_rl == LAMPS_ACQ_RUN &&
            s_prev_state  != LAMPS_ACQ_RUN) {
            s_prev_events  = (uint64_t)XTotEvts;
            s_prev_time_ns = now_ns;
        }

        /* Compute instantaneous rate from delta since last update.
         * Returns 0.0 when not running (STOP or PAUSE).               */
        if (cur_state_rl == LAMPS_ACQ_RUN && s_prev_time_ns > 0) {
            uint64_t delta_evts = (uint64_t)XTotEvts - s_prev_events;
            uint64_t delta_ns   = now_ns - s_prev_time_ns;
            if (delta_ns > 0)
                event_rate_val = (double)delta_evts
                               / ((double)delta_ns * 1e-9);
        }

        /* Advance the delta baseline for next call */
        s_prev_events = (uint64_t)XTotEvts;
        s_prev_time_ns = now_ns;
        s_prev_state   = cur_state_rl;

        /* --- Begin seqlock critical section -------------------------------- */
        atomic_fetch_add_explicit((_Atomic uint64_t *)&s_shm->generation, 1,
                                  memory_order_release);

        /* Metadata */
        strncpy(s_shm->run_name, RunName, 39);
        s_shm->run_name[39] = '\0';
        s_shm->acq_state = cur_state_rl;

        s_shm->total_events       = (uint64_t)XTotEvts;
        s_shm->buffers_acquired   = (uint64_t)BuffersAcquired;
        s_shm->buffers_processed  = (uint64_t)BuffersProcessed;

        {
            /* Elapsed time: wall clock from StartTime */
            double now_wall = (double)time(NULL);
            double elapsed  = (StartTime > 0.0) ? (now_wall - StartTime) : 0.0;
            s_shm->elapsed_seconds = elapsed;
        }

        s_shm->event_rate = event_rate_val;
    }

    /* Scaler values — runs under the seqlock begun above */
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

    /* --- End seqlock critical section: mark generation even ------------- */
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
