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
#include "lamps_cmd.h"  /* Phase 3: remote command channel */

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
  #define ca_add_event(t,ch,cb,p,id) (0)   /* Phase 3 stub */
  #define ca_task_exit()          do{}while(0)
  #define ECA_NORMAL              0
  #define DBR_STRING              0
  #define DBR_LONG                1
  #define DBR_DOUBLE              6
  typedef long dbr_long_t;
  typedef double dbr_double_t;
  typedef char dbr_string_t[40];
  typedef void * evid;
  typedef struct { void *dbr; } struct_event_handler_args;
  typedef struct_event_handler_args event_handler_args;
#endif

/* =========================================================================
 * Configuration
 * ========================================================================= */
#define MAX_PV_NAME        80
#define DEFAULT_PREFIX     "LAMPS"
#define DEFAULT_RATE_HZ    5

/* If the SHM generation counter hasn't advanced for this many seconds,
 * LAMPS has likely stopped and restarted (unlinked+recreated the segment).
 * The bridge will close its stale mapping and reconnect.                   */
#define STALE_TIMEOUT_SEC  3

/* Retry interval while waiting for LAMPS SHM to appear (or reappear).     */
#define RETRY_INTERVAL_US  200000   /* 200 ms */

/* How often to print a "still waiting" message during reconnect.          */
#define RECONNECT_LOG_INTERVAL  10  /* every 10 retries (~2 s)             */

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

/* Fixed monitoring PVs (read from SHM, published to EPICS) */
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

/* Phase 3/4: command PVs (monitored from EPICS, relayed to LAMPS via cmd SHM) */
enum {
    PV_CMD_START = 0,
    PV_CMD_STOP,
    PV_CMD_RESET,
    PV_CMD_COUNT
};

static Pv   s_cmd_pvs[PV_CMD_COUNT];  /* bo records: START, STOP, RESET     */
static Pv   s_run_name_pv;            /* Phase 4: stringout CMD:RUN_NAME    */

/* Phase 4: local cache of the run name — updated whenever the PV changes.  */
static char s_run_name[LAMPS_CMD_NAME_LEN] = "Dummy";

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
    double event_rate;

    /* Status string */
    switch (blk->acq_state) {
    case LAMPS_ACQ_RUN:   status = "RUNNING";  break;
    case LAMPS_ACQ_PAUSE: status = "PAUSED";   break;
    default:              status = "STOPPED";  break;
    }

    /* Safety net: never publish a non-zero rate when not actively running.
     * This guards against stale SHM values that survived a restart.      */
    event_rate = (blk->acq_state == LAMPS_ACQ_RUN) ? blk->event_rate : 0.0;

    put_string(PV_STATUS,          status);
    put_string(PV_RUN_NAME,        blk->run_name);
    put_long  (PV_TOTAL_EVENTS,    (long)blk->total_events);
    put_long  (PV_BUFS_ACQUIRED,   (long)blk->buffers_acquired);
    put_long  (PV_BUFS_PROCESSED,  (long)blk->buffers_processed);
    put_double(PV_ELAPSED,         blk->elapsed_seconds);
    put_double(PV_EVENT_RATE,      event_rate);

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
 * push_stopped() — publish STATUS=STOPPED, preserving final run counts.
 *
 * Called when acquisition has ended cleanly (AcqSignal=Stop) and the SHM
 * generation counter is no longer advancing.  LAMPS is still alive — we
 * preserve the final event count and run name so operators can read the
 * end-of-run numbers from Phoebus without restarting anything.
 *
 * Only EVENT_RATE is zeroed (a rate is meaningless for a stopped run).
 * Spectra and scalers are left at their last published values.
 * ========================================================================= */
static void push_stopped(const LampsShmBlock *blk,
                         int scaler_pv_start, int spec_pv_start)
{
    (void)scaler_pv_start; (void)spec_pv_start;
    put_string(PV_STATUS,          "STOPPED");
    put_string(PV_RUN_NAME,        blk->run_name);
    put_long  (PV_TOTAL_EVENTS,    (long)blk->total_events);
    put_long  (PV_BUFS_ACQUIRED,   (long)blk->buffers_acquired);
    put_long  (PV_BUFS_PROCESSED,  (long)blk->buffers_processed);
    put_double(PV_ELAPSED,         blk->elapsed_seconds);
    put_double(PV_EVENT_RATE,      0.0);
#ifdef HAVE_EPICS
    ca_flush_io();
#endif
}

/* =========================================================================
 * push_offline() — zero out all PVs and set STATUS=OFFLINE.
 *
 * Called ONLY when the bridge has lost contact with the LAMPS SHM segment
 * entirely (LAMPS process crashed or was killed).  All PVs are zeroed so
 * operators immediately know the data source is gone, not merely idle.
 * ========================================================================= */
static void push_offline(int scaler_pv_start, int spec_pv_start)
{
    uint32_t i;
    put_string(PV_STATUS,         "OFFLINE");
    put_string(PV_RUN_NAME,       "");
    put_long  (PV_TOTAL_EVENTS,   0);
    put_long  (PV_BUFS_ACQUIRED,  0);
    put_long  (PV_BUFS_PROCESSED, 0);
    put_double(PV_ELAPSED,        0.0);
    put_double(PV_EVENT_RATE,     0.0);
    for (i = 0; i < (uint32_t)(s_npv - spec_pv_start); ++i) {
        uint32_t zeros[1] = {0};
        put_waveform_u32(spec_pv_start + i, zeros, 1);
    }
    for (i = 0; i < (uint32_t)(spec_pv_start - scaler_pv_start); ++i)
        put_long(scaler_pv_start + i, 0);
#ifdef HAVE_EPICS
    ca_flush_io();
#endif
}

/* =========================================================================
 * Phase 3/4: Command PV subscription callbacks
 *
 * cmd_event_cb: fires when CMD:START/STOP/RESET transitions to 1.
 *   - For STOP/RESET: calls lamps_cmd_post().
 *   - For START:      calls lamps_cmd_post_start() with the cached run name.
 *
 * run_name_event_cb: fires when CMD:RUN_NAME changes — just caches the value.
 * ========================================================================= */
static void run_name_event_cb(struct event_handler_args args)
{
#ifdef HAVE_EPICS
    if (!args.dbr) return;
    strncpy(s_run_name, (const char *)args.dbr, LAMPS_CMD_NAME_LEN - 1);
    s_run_name[LAMPS_CMD_NAME_LEN - 1] = '\0';
    if (s_verbose)
        fprintf(stderr, "[BRIDGE] CMD:RUN_NAME cached as \"%s\"\n", s_run_name);
#else
    (void)args;
#endif
}

static void cmd_event_cb(struct event_handler_args args)
{
#ifdef HAVE_EPICS
    if (!args.dbr) return;
    dbr_long_t val = *(const dbr_long_t *)args.dbr;
    if (val != 1) return;   /* ignore the auto-reset-to-0 transition */

    uint32_t cmd = LAMPS_CMD_NONE;
    const char *cmd_name = "?";

    if      (args.chid == s_cmd_pvs[PV_CMD_START].chid) { cmd = LAMPS_CMD_START; cmd_name = "START"; }
    else if (args.chid == s_cmd_pvs[PV_CMD_STOP].chid)  { cmd = LAMPS_CMD_STOP;  cmd_name = "STOP";  }
    else if (args.chid == s_cmd_pvs[PV_CMD_RESET].chid) { cmd = LAMPS_CMD_RESET; cmd_name = "RESET"; }

    if (cmd != LAMPS_CMD_NONE) {
        int rc;
        fprintf(stderr, "[BRIDGE] Remote command received: %s", cmd_name);
        if (cmd == LAMPS_CMD_START) {
            fprintf(stderr, " (run_name=\"%s\")\n", s_run_name);
            rc = lamps_cmd_post_start(s_run_name);
        } else {
            fprintf(stderr, "\n");
            rc = lamps_cmd_post(cmd);
        }
        if (rc < 0)
            fprintf(stderr,
                    "[BRIDGE] WARNING: cmd SHM not ready — command dropped\n");
    }
#else
    (void)args;
#endif
}

static void connect_cmd_pvs(void)
{
    int i;
    const char *suffixes[PV_CMD_COUNT] = {"CMD:START", "CMD:STOP", "CMD:RESET"};

    for (i = 0; i < PV_CMD_COUNT; ++i) {
        snprintf(s_cmd_pvs[i].name, MAX_PV_NAME, "%s:%s", s_prefix, suffixes[i]);
        s_cmd_pvs[i].chid      = NULL;
        s_cmd_pvs[i].connected = 0;
    }
    snprintf(s_run_name_pv.name, MAX_PV_NAME, "%s:CMD:RUN_NAME", s_prefix);
    s_run_name_pv.chid      = NULL;
    s_run_name_pv.connected = 0;

#ifdef HAVE_EPICS
    /* Connect all command channels */
    for (i = 0; i < PV_CMD_COUNT; ++i)
        ca_create_channel(s_cmd_pvs[i].name, NULL, NULL, 10, &s_cmd_pvs[i].chid);
    ca_create_channel(s_run_name_pv.name, NULL, NULL, 10, &s_run_name_pv.chid);
    ca_pend_io(5.0);

    /* Subscribe: bo records use DBR_LONG, stringout uses DBR_STRING */
    for (i = 0; i < PV_CMD_COUNT; ++i) {
        if (s_cmd_pvs[i].chid) {
            ca_add_event(DBR_LONG, s_cmd_pvs[i].chid,
                         cmd_event_cb, NULL, (evid *)NULL);
            s_cmd_pvs[i].connected = 1;
        }
    }
    if (s_run_name_pv.chid) {
        ca_add_event(DBR_STRING, s_run_name_pv.chid,
                     run_name_event_cb, NULL, (evid *)NULL);
        s_run_name_pv.connected = 1;
    }
    ca_flush_io();
    fprintf(stderr,
            "[BRIDGE] Phase 4: subscribed to %d cmd PVs + CMD:RUN_NAME\n",
            PV_CMD_COUNT);
#else
    fprintf(stderr, "[BRIDGE] Phase 4: stub mode — command PVs not subscribed\n");
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
        usleep(RETRY_INTERVAL_US);   /* 200 ms — was sleep(2) */
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

    /* Connect monitoring PVs to Channel Access IOC */
    connect_all_pvs();

    /* Phase 3: open command SHM (bridge is the writer) and subscribe to
     * command PVs so we receive START/STOP/RESET from EPICS immediately. */
    if (lamps_cmd_open_write() < 0)
        fprintf(stderr, "[BRIDGE] WARNING: could not create cmd SHM"
                " — remote control disabled\n");
    connect_cmd_pvs();

    /* -----------------------------------------------------------------------
     * Main poll loop
     * ---------------------------------------------------------------------- */
    sleep_us = (unsigned long)(1000000UL / (unsigned long)s_rate_hz);
    fprintf(stderr, "[BRIDGE] Polling at %d Hz (sleep %lu µs)\n",
            s_rate_hz, sleep_us);

    {
        /* State tracking for diagnostics and robustness.
         *   last_gen      — generation counter at last poll
         *   last_gen_time — wall-clock time it last advanced
         *   is_offline    — true if we have declared OFFLINE
         *   stopped_logged— suppresses repeated "acquisition stopped" lines
         *   prev_state    — tracks state transitions for logging          */
        uint64_t      last_gen       = (uint64_t)-1;
        time_t        last_gen_time  = time(NULL);
        int           is_offline     = 0;
        int           stopped_logged = 0;
        LampsAcqState prev_state     = LAMPS_ACQ_STOP;

        while (s_running) {
            if (lamps_shm_snapshot(snap) == 0) {

                /* -------------------------------------------------------
                 * Generation-stall logic.
                 *
                 * Case A — generation frozen AND acq_state is STOP/PAUSE:
                 *   Acquisition ended cleanly.  Publish STATUS=STOPPED
                 *   with the final event count (do NOT zero any PVs).
                 *   Stay attached — wait for generation to advance again
                 *   (meaning LAMPS started a new acquisition).
                 *
                 * Case B — generation frozen AND acq_state is RUN:
                 *   Hardware or software crash while acquiring.
                 *   Wait STALE_TIMEOUT_SEC, then close+reconnect and
                 *   push STATUS=OFFLINE.
                 * ------------------------------------------------------- */
                int acq_active = (snap->acq_state == LAMPS_ACQ_RUN);

                if (snap->generation != last_gen) {
                    /* Generation is advancing — live data.               */
                    last_gen      = snap->generation;
                    last_gen_time = time(NULL);
                    stopped_logged = 0;

                    /* Log state transitions (once per transition).       */
                    if (snap->acq_state != prev_state) {
                        const char *st =
                            (snap->acq_state == LAMPS_ACQ_RUN)   ? "RUNNING" :
                            (snap->acq_state == LAMPS_ACQ_PAUSE)  ? "PAUSED"  :
                                                                     "STOPPED";
                        fprintf(stderr,
                                "[BRIDGE] State → %s  run=%s  evts=%"
                                PRIu64 "\n", st, snap->run_name,
                                snap->total_events);
                        prev_state = snap->acq_state;
                    }
                    if (is_offline) {
                        is_offline = 0;
                        fprintf(stderr,
                                "[BRIDGE] SHM live — generation %" PRIu64 "\n",
                                last_gen);
                    }

                } else if (!acq_active) {
                    /* Case A: acquisition stopped cleanly, gen frozen.
                     * Publish STOPPED with final counts — do NOT zero.  */
                    if (!stopped_logged) {
                        fprintf(stderr,
                                "[BRIDGE] Acquisition stopped — "
                                "holding final counts "
                                "(run=%s  evts=%" PRIu64 "  elapsed=%.1fs)\n",
                                snap->run_name, snap->total_events,
                                snap->elapsed_seconds);
                        stopped_logged = 1;
                    }
                    push_stopped(snap, scaler_pv_start, spec_pv_start);
                    usleep(sleep_us);
                    continue;

                } else if (difftime(time(NULL), last_gen_time) >= STALE_TIMEOUT_SEC) {
                    /* Case B: was RUNNING but generation froze — crash?  */
                    if (!is_offline) {
                        fprintf(stderr,
                                "[BRIDGE] SHM stale for %d s while RUNNING"
                                " — LAMPS crashed? Reconnecting...\n",
                                STALE_TIMEOUT_SEC);
                        is_offline     = 1;
                        stopped_logged = 0;
                        prev_state     = LAMPS_ACQ_STOP;
                    }
                    push_offline(scaler_pv_start, spec_pv_start);
                    lamps_shm_close();
                    {
                        int rc = 0;
                        while (s_running) {
                            if (lamps_shm_open_read() == 0) break;
                            if ((++rc % RECONNECT_LOG_INTERVAL) == 0)
                                fprintf(stderr,
                                        "[BRIDGE] Waiting for LAMPS SHM"
                                        " (%d retries, %.0fs)...\n",
                                        rc, rc * RETRY_INTERVAL_US / 1e6);
                            usleep(RETRY_INTERVAL_US);
                        }
                        if (s_running)
                            fprintf(stderr, "[BRIDGE] SHM reconnected.\n");
                    }
                    last_gen      = (uint64_t)-1;
                    last_gen_time = time(NULL);
                    continue;
                }

                publish(snap, scaler_pv_start, spec_pv_start);
                if (s_verbose)
                    fprintf(stderr,
                            "[BRIDGE] %s  evts=%" PRIu64 "  rate=%.1f/s\n",
                            snap->run_name, snap->total_events,
                            snap->event_rate);

            } else {
                /* lamps_shm_snapshot() failed — SHM segment is gone.
                 * This covers: LAMPS killed, SHM unlinked externally,
                 * or bad magic (segment replaced with incompatible ver). */
                if (!is_offline) {
                    fprintf(stderr,
                            "[BRIDGE] SHM snapshot failed — LAMPS gone?"
                            " Reconnecting...\n");
                    is_offline     = 1;
                    stopped_logged = 0;
                    prev_state     = LAMPS_ACQ_STOP;
                    push_offline(scaler_pv_start, spec_pv_start);
                    lamps_shm_close();
                }
                {
                    int rc = 0;
                    while (s_running) {
                        if (lamps_shm_open_read() == 0) {
                            fprintf(stderr,
                                    "[BRIDGE] SHM reconnected after"
                                    " snapshot failure.\n");
                            is_offline    = 0;
                            last_gen      = (uint64_t)-1;
                            last_gen_time = time(NULL);
                            break;
                        }
                        if ((++rc % RECONNECT_LOG_INTERVAL) == 0)
                            fprintf(stderr,
                                    "[BRIDGE] Waiting for LAMPS SHM"
                                    " (%d retries, %.0fs)...\n",
                                    rc, rc * RETRY_INTERVAL_US / 1e6);
                        usleep(RETRY_INTERVAL_US);
                    }
                }
            }
            usleep(sleep_us);
        }
    }


    fprintf(stderr, "[BRIDGE] Shutting down.\n");
#ifdef HAVE_EPICS
    ca_task_exit();
#endif
    lamps_cmd_close();   /* Phase 3: remove command SHM */
    lamps_shm_close();
    free(snap);
    return 0;
}
