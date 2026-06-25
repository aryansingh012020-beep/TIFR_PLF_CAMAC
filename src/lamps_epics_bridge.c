/* =========================================================================
 * lamps_epics_bridge.c — LAMPS → EPICS Channel Access publisher
 *
 * Standalone process (no GTK, no CAMAC).  Runs alongside the lamps GUI.
 *
 * What it does:
 *   1. Opens the POSIX shared memory segment written by the LAMPS DAQ engine.
 *   2. Polls at configurable rate (default 5 Hz).
 *   3. Publishes PVs via EPICS Channel Access (libca from EPICS base).
 *
 * PV naming convention:  <PREFIX>:<PVNAME>
 *   Default prefix:  LAMPS  (override with env var LAMPS_EPICS_PREFIX or -p)
 *
 * PV Table
 * ─────────────────────────────────────────────────────────────────────────
 * PV Name (with prefix)          Type      Description
 * ─────────────────────────────────────────────────────────────────────────
 * LAMPS:STATUS                   string    "RUNNING" | "PAUSED" | "STOPPED"
 * LAMPS:RUN_NAME                 string    Current run name
 * LAMPS:TOTAL_EVENTS             long      Cumulative events acquired
 * LAMPS:BUFS_ACQUIRED            long      Buffers read from hardware
 * LAMPS:BUFS_PROCESSED           long      Buffers processed by BuildSpectra
 * LAMPS:ELAPSED_SEC              double    Elapsed acquisition time (s)
 * LAMPS:EVENT_RATE               double    Events per second
 * LAMPS:SCALER<N>                long      Scaler value N=01..64
 * LAMPS:SPEC1D:<NAME>            waveform  8192-channel 32-bit spectrum
 *   (one PV per 1D spectrum, up to LAMPS_MAX_ONED_EPICS)
 * ─────────────────────────────────────────────────────────────────────────
 *
 * Build:
 *   Requires EPICS base. Set EPICS_BASE in environment or Makefile.
 *   $(CC) -o lamps_epics_bridge lamps_epics_bridge.c lamps_shm.c \
 *         -I$(EPICS_BASE)/include \
 *         -I$(EPICS_BASE)/include/os/Linux \
 *         -L$(EPICS_BASE)/lib/linux-x86_64 \
 *         -lca -lCom -lrt
 *
 * Run:
 *   ./lamps_epics_bridge [-p PREFIX] [-r RATE_HZ] [-v]
 *
 * Notes:
 *   - The bridge does NOT need to run as root.
 *   - Start the bridge AFTER the LAMPS GUI is running (SHM is created
 *     by the LAMPS process at startup).
 *   - EPICS ADDR_LIST / BEACON_ADDR_LIST set normally via environment.
 * ========================================================================= */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <stdint.h>
#include <inttypes.h>
#include <getopt.h>

#include "lamps_shm.h"

/* -------------------------------------------------------------------------
 * Conditional EPICS include.
 * When EPICS_BASE is not set we compile in stub mode for unit-testing
 * the SHM logic without an EPICS install.
 * -------------------------------------------------------------------------*/
#ifdef HAVE_EPICS
  /* EPICS Channel Access */
  #include "cadef.h"
  #include "db_access.h"
  #include "epicsThread.h"
  #include "epicsTime.h"

  typedef chid EpicsChid;
#else
  /* Stub types for build without EPICS — bridge prints to stdout only.     */
  typedef void * EpicsChid;
  #define ca_context_create(x)    (0)
  #define ca_create_channel(n,cb,p,pri,id) (*(id)=NULL, ECA_NORMAL)
  #define ca_flush_io()           (0)
  #define ca_pend_io(t)           (0)
  #define ca_put(type,chid,pval)  (0)
  #define ca_task_exit()          do{}while(0)
  #define ECA_NORMAL              0
  #define DBR_STRING              0
  #define DBR_LONG                1
  #define DBR_DOUBLE              6
  typedef long dbr_long_t;
  typedef double dbr_double_t;
  typedef char dbr_string_t[40];
#endif

/* =========================================================================
 * Configuration
 * ========================================================================= */
#define MAX_PV_NAME      80
#define DEFAULT_PREFIX   "LAMPS"
#define DEFAULT_RATE_HZ  5

static volatile sig_atomic_t s_running = 1;
static int s_verbose = 0;

static void handle_signal(int sig) { (void)sig; s_running = 0; }

/* =========================================================================
 * PV registry
 * ========================================================================= */
typedef struct {
    char       name[MAX_PV_NAME];
    EpicsChid  chid;
    int        connected;
} Pv;

/* Fixed PVs */
enum {
    PV_STATUS = 0,
    PV_RUN_NAME,
    PV_TOTAL_EVENTS,
    PV_BUFS_ACQUIRED,
    PV_BUFS_PROCESSED,
    PV_ELAPSED,
    PV_EVENT_RATE,
    PV_FIXED_COUNT
};

/* Dynamic: scalers + 1D spectra added at connect time                      */
#define MAX_SCALER_PVS  LAMPS_MAX_SCALER_EPICS
#define MAX_SPEC_PVS    LAMPS_MAX_ONED_EPICS
#define MAX_TOTAL_PVS   (PV_FIXED_COUNT + MAX_SCALER_PVS + MAX_SPEC_PVS)

static Pv    s_pvs[MAX_TOTAL_PVS];
static int   s_npv = 0;

static char  s_prefix[64] = DEFAULT_PREFIX;
static int   s_rate_hz    = DEFAULT_RATE_HZ;

/* =========================================================================
 * PV helpers
 * ========================================================================= */
static void pv_init(const char *suffix)
{
    if (s_npv >= MAX_TOTAL_PVS) {
        fprintf(stderr, "[BRIDGE] PV table full!\n");
        return;
    }
    snprintf(s_pvs[s_npv].name, MAX_PV_NAME, "%s:%s", s_prefix, suffix);
    s_pvs[s_npv].chid      = NULL;
    s_pvs[s_npv].connected = 0;
    ++s_npv;
}

static void connect_all_pvs(void)
{
#ifdef HAVE_EPICS
    int i;
    ca_context_create(ca_enable_preemptive_callback);
    for (i = 0; i < s_npv; ++i)
        ca_create_channel(s_pvs[i].name, NULL, NULL, 10, &s_pvs[i].chid);
    ca_pend_io(5.0);
    for (i = 0; i < s_npv; ++i)
        s_pvs[i].connected = (s_pvs[i].chid != NULL);
    if (s_verbose)
        fprintf(stderr, "[BRIDGE] %d PVs connected\n", s_npv);
#else
    if (s_verbose)
        fprintf(stderr, "[BRIDGE] stub mode — %d PVs would connect\n", s_npv);
#endif
}

static void put_string(int idx, const char *val)
{
    if (s_verbose)
        printf("  PUT %s = \"%s\"\n", s_pvs[idx].name, val);
#ifdef HAVE_EPICS
    if (s_pvs[idx].chid)
        ca_put(DBR_STRING, s_pvs[idx].chid, val);
#endif
}

static void put_long(int idx, long val)
{
    if (s_verbose)
        printf("  PUT %s = %ld\n", s_pvs[idx].name, val);
#ifdef HAVE_EPICS
    dbr_long_t v = (dbr_long_t)val;
    if (s_pvs[idx].chid)
        ca_put(DBR_LONG, s_pvs[idx].chid, &v);
#endif
}

static void put_double(int idx, double val)
{
    if (s_verbose)
        printf("  PUT %s = %.3f\n", s_pvs[idx].name, val);
#ifdef HAVE_EPICS
    dbr_double_t v = val;
    if (s_pvs[idx].chid)
        ca_put(DBR_DOUBLE, s_pvs[idx].chid, &v);
#endif
}

static void put_waveform_u32(int idx, const uint32_t *data, uint32_t nelem)
{
    if (s_verbose)
        printf("  PUT %s = [waveform %u ch]\n", s_pvs[idx].name, nelem);
#ifdef HAVE_EPICS
    if (s_pvs[idx].chid)
        ca_array_put(DBR_LONG, nelem, s_pvs[idx].chid, data);
#endif
}

/* =========================================================================
 * Publish one snapshot to EPICS
 * ========================================================================= */
static void publish(const LampsShmBlock *blk,
                    int scaler_pv_start,
                    int spec_pv_start)
{
    uint32_t i;
    const char *status;

    /* Status string */
    switch (blk->acq_state) {
    case LAMPS_ACQ_RUN:   status = "RUNNING";  break;
    case LAMPS_ACQ_PAUSE: status = "PAUSED";   break;
    default:              status = "STOPPED";  break;
    }

    put_string(PV_STATUS,          status);
    put_string(PV_RUN_NAME,        blk->run_name);
    put_long  (PV_TOTAL_EVENTS,    (long)blk->total_events);
    put_long  (PV_BUFS_ACQUIRED,   (long)blk->buffers_acquired);
    put_long  (PV_BUFS_PROCESSED,  (long)blk->buffers_processed);
    put_double(PV_ELAPSED,         blk->elapsed_seconds);
    put_double(PV_EVENT_RATE,      blk->event_rate);

    /* Scalers */
    for (i = 0; i < blk->n_scaler && i < MAX_SCALER_PVS; ++i)
        put_long(scaler_pv_start + i, (long)blk->scaler_val[i]);

    /* 1D spectra */
    for (i = 0; i < blk->n_oned && i < MAX_SPEC_PVS; ++i) {
        const uint32_t *data = &blk->oned_data[i * LAMPS_MAX_CHAN_EPICS];
        put_waveform_u32(spec_pv_start + i, data, blk->oned_chan[i]);
    }

#ifdef HAVE_EPICS
    ca_flush_io();
#endif
}

/* =========================================================================
 * main
 * ========================================================================= */
int main(int argc, char *argv[])
{
    int opt;
    int scaler_pv_start, spec_pv_start;
    unsigned long sleep_us;
    LampsShmBlock *snap;
    uint32_t i;

    /* Parse arguments */
    while ((opt = getopt(argc, argv, "p:r:v")) != -1) {
        switch (opt) {
        case 'p': strncpy(s_prefix, optarg, 63); s_prefix[63] = '\0'; break;
        case 'r': s_rate_hz = atoi(optarg); if (s_rate_hz < 1) s_rate_hz = 1; break;
        case 'v': s_verbose = 1; break;
        default:
            fprintf(stderr,
                    "Usage: %s [-p PREFIX] [-r RATE_HZ] [-v]\n", argv[0]);
            return 1;
        }
    }

    fprintf(stderr,
            "LAMPS EPICS Bridge — prefix=%s  rate=%d Hz\n",
            s_prefix, s_rate_hz);

    /* Signal handlers */
    signal(SIGINT,  handle_signal);
    signal(SIGTERM, handle_signal);

    /* Allocate snapshot buffer */
    snap = malloc(sizeof(LampsShmBlock));
    if (!snap) { perror("malloc"); return 1; }

    /* Open shared memory (reader) — wait until LAMPS creates it            */
    fprintf(stderr, "[BRIDGE] Waiting for LAMPS shared memory segment...\n");
    while (s_running) {
        if (lamps_shm_open_read() == 0) break;
        sleep(2);
    }
    if (!s_running) { free(snap); return 0; }

    /* Take an initial snapshot to discover how many spectra/scalers exist  */
    if (lamps_shm_snapshot(snap) < 0) {
        fprintf(stderr, "[BRIDGE] Snapshot failed — is LAMPS running?\n");
        lamps_shm_close();
        free(snap);
        return 1;
    }

    /* Register fixed PVs */
    s_npv = 0;
    pv_init("STATUS");
    pv_init("RUN_NAME");
    pv_init("TOTAL_EVENTS");
    pv_init("BUFS_ACQUIRED");
    pv_init("BUFS_PROCESSED");
    pv_init("ELAPSED_SEC");
    pv_init("EVENT_RATE");
    /* assert: s_npv == PV_FIXED_COUNT */

    /* Register scaler PVs */
    scaler_pv_start = s_npv;
    for (i = 0; i < snap->n_scaler && i < MAX_SCALER_PVS; ++i) {
        char suf[MAX_PV_NAME];
        if (strlen(snap->scaler_name[i]) > 0)
            snprintf(suf, sizeof(suf), "SCALER:%s", snap->scaler_name[i]);
        else
            snprintf(suf, sizeof(suf), "SCALER%02u", i + 1);
        pv_init(suf);
    }

    /* Register 1D spectrum waveform PVs */
    spec_pv_start = s_npv;
    for (i = 0; i < snap->n_oned && i < MAX_SPEC_PVS; ++i) {
        char suf[MAX_PV_NAME];
        if (strlen(snap->oned_name[i]) > 0)
            snprintf(suf, sizeof(suf), "SPEC1D:%s", snap->oned_name[i]);
        else
            snprintf(suf, sizeof(suf), "SPEC1D:%03u", i + 1);
        pv_init(suf);
    }

    fprintf(stderr,
            "[BRIDGE] %u scalers, %u 1D spectra → %d total PVs\n",
            snap->n_scaler, snap->n_oned, s_npv);

    /* Connect to Channel Access IOC */
    connect_all_pvs();

    /* -----------------------------------------------------------------------
     * Main poll loop
     * ---------------------------------------------------------------------- */
    sleep_us = (unsigned long)(1000000UL / (unsigned long)s_rate_hz);
    fprintf(stderr, "[BRIDGE] Polling at %d Hz (sleep %lu µs)\n",
            s_rate_hz, sleep_us);

    while (s_running) {
        if (lamps_shm_snapshot(snap) == 0) {
            publish(snap, scaler_pv_start, spec_pv_start);
            if (s_verbose)
                fprintf(stderr,
                        "[BRIDGE] %s  evts=%" PRIu64 "  rate=%.1f/s\n",
                        snap->run_name, snap->total_events, snap->event_rate);
        } else {
            if (s_verbose)
                fprintf(stderr, "[BRIDGE] SHM read failed — LAMPS gone?\n");
        }
        usleep(sleep_us);
    }

    fprintf(stderr, "[BRIDGE] Shutting down.\n");
#ifdef HAVE_EPICS
    ca_task_exit();
#endif
    lamps_shm_close();
    free(snap);
    return 0;
}
