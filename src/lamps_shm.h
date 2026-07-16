/* =========================================================================
 * lamps_shm.h — LAMPS shared-memory ring for EPICS bridge
 *
 * Provides a POSIX shared memory segment that the DAQ engine writes
 * after every buffer, and the lamps_epics_bridge reads at ~5 Hz to
 * publish spectrum waveforms to EPICS.
 *
 * Design constraints:
 *   - No GTK in this header.  Usable from both lamps and the bridge.
 *   - Fixed layout: change LAMPS_SHM_VERSION if the struct changes.
 *   - Single writer (DAQ thread), single reader (bridge process).
 *     No mutex: bridge reads a generation counter and detects stale data.
 *
 * Usage in LAMPS (writer):
 *   lamps_shm_open_write();     // called at startup
 *   lamps_shm_update();         // called by daq_engine after each buffer
 *   lamps_shm_close();          // called at shutdown
 *
 * Usage in bridge (reader):
 *   lamps_shm_open_read();
 *   lamps_shm_snapshot(...);    // copies out one consistent snapshot
 *   lamps_shm_close();
 * ========================================================================= */

#ifndef LAMPS_SHM_H
#define LAMPS_SHM_H

#include <stdint.h>
#include <stddef.h>

/* -------------------------------------------------------------------------
 * Layout constants — must match on both sides.
 * -------------------------------------------------------------------------*/
#define LAMPS_SHM_NAME     "/lamps_daq_shm"
#define LAMPS_SHM_VERSION  1

/* Maximum number of 1D spectra exported over EPICS.
 * The actual number in use is shm->n_oned (≤ LAMPS_MAX_ONED_EPICS).
 * 64 is a practical limit; most experiments have 1–32.                     */
#define LAMPS_MAX_ONED_EPICS   64

/* Maximum channels per spectrum exported.
 * LAMPS supports up to 65536 channels, but 8192 is the DAQ_DATAFLOW target.*/
#define LAMPS_MAX_CHAN_EPICS   8192

/* Maximum number of scalers exported.                                        */
#define LAMPS_MAX_SCALER_EPICS 64

/* -------------------------------------------------------------------------
 * Acquisition state enum (mirrors AcqSignal in lamps.h without GTK types)
 * -------------------------------------------------------------------------*/
typedef enum {
    LAMPS_ACQ_STOP   = 0,
    LAMPS_ACQ_PAUSE  = 1,
    LAMPS_ACQ_RUN    = 2
} LampsAcqState;

/* -------------------------------------------------------------------------
 * Shared memory layout.
 * Writer fills this, then increments generation.  Reader checks that
 * generation is the same before and after copying — standard seqlock pattern.
 * -------------------------------------------------------------------------*/
typedef struct {
    /* Protocol guard */
    uint32_t  magic;          /* 0xDAC00001                                  */
    uint32_t  version;        /* LAMPS_SHM_VERSION                           */

    /* Seqlock counter: odd while writer is updating, even when stable.      */
    volatile uint64_t generation;

    /* Acquisition metadata */
    char      run_name[40];
    LampsAcqState acq_state;
    uint64_t  total_events;
    uint64_t  buffers_acquired;
    uint64_t  buffers_processed;
    double    elapsed_seconds;
    double    event_rate;         /* events/s computed by DAQ engine          */
    double    data_rate_kbps;

    /* Scaler values */
    uint32_t  n_scaler;
    char      scaler_name[LAMPS_MAX_SCALER_EPICS][32];
    uint32_t  scaler_val[LAMPS_MAX_SCALER_EPICS];

    /* 1D spectrum directory */
    uint32_t  n_oned;
    char      oned_name[LAMPS_MAX_ONED_EPICS][32];
    uint32_t  oned_chan[LAMPS_MAX_ONED_EPICS];       /* channels in this spectrum */
    uint32_t  oned_wsz[LAMPS_MAX_ONED_EPICS];        /* 1=16-bit, 2=32-bit        */

    /* Flattened 32-bit spectrum data.
     * oned_data[i * LAMPS_MAX_CHAN_EPICS] is the start of spectrum i.
     * Always stored as uint32 regardless of wsz (bridge converts).         */
    uint32_t  oned_data[LAMPS_MAX_ONED_EPICS * LAMPS_MAX_CHAN_EPICS];

} LampsShmBlock;

#define LAMPS_SHM_MAGIC  0xDAC00001UL

/* -------------------------------------------------------------------------
 * API — implemented in lamps_shm.c
 * -------------------------------------------------------------------------*/
int  lamps_shm_open_write(void);     /* mmap and initialise (writer side)    */
int  lamps_shm_open_read(void);      /* mmap read-only (reader / bridge)     */
void lamps_shm_update(void);         /* writer: push current spectra + stats */
int  lamps_shm_snapshot(LampsShmBlock *dst); /* reader: copy with seqlock    */
void lamps_shm_close(void);          /* unmap; writer also unlinks           */

#endif /* LAMPS_SHM_H */
