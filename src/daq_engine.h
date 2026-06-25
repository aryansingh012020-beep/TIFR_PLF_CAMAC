/* daq_engine.h — DAQ Engine API
 *
 * Phase 2: Incremental extraction of DAQ logic from control.c.
 * This header exposes the public interface of the DAQ engine.
 * The GTK layer (control.c) calls into these functions.
 *
 * Migration strategy: Wrapper-First.
 *   Functions here initially delegate to original control.c logic.
 *   Over time, they will grow to own the implementation entirely.
 *
 * Do NOT include gtk headers here. This file must remain GTK-free.
 */

#ifndef DAQ_ENGINE_H
#define DAQ_ENGINE_H

#include <stdio.h>
#include <glib.h>   /* for gboolean, gint, gushort, glong — glib types only, no GTK */

/* -----------------------------------------------------------------------
 * Error callback: registered by the GTK layer so the DAQ engine can
 * report errors without calling GTK directly.
 * Usage: daq_set_error_cb(my_gtk_show_error_fn);
 * ----------------------------------------------------------------------- */
typedef void (*DaqErrorCb)(const char *message);
void daq_set_error_cb(DaqErrorCb cb);

/* -----------------------------------------------------------------------
 * Status callback: registered by the GTK layer to receive acquisition
 * lifecycle events from the DAQ engine (start, stop, pause).
 *
 * Fields the GTK layer receives:
 *   event    — "start" | "stop" | "pause" | "resume"
 *   run_name — current run name string
 *   hh/mm/ss — start time components
 *   prev_run — name of the previous run (for the "Prev." label)
 *
 * This replaces the gdk_threads_enter/gtk_label_set_text blocks
 * that are currently embedded inside AcquireData.
 * ----------------------------------------------------------------------- */
typedef struct {
    const char *event;      /* "start", "stop", "pause", "resume" */
    const char *run_name;
    const char *prev_run;   /* previous run — used by "Prev." status label */
    int         hh, mm, ss; /* acquisition start time */
} DaqStatusEvent;

typedef void (*DaqStatusCb)(const DaqStatusEvent *ev);
void daq_set_status_cb(DaqStatusCb cb);
/* Call from DAQ thread to notify GTK layer: */
void daq_notify_status(const char *event, const char *run_name,
                       const char *prev_run, int hh, int mm, int ss);

/* -----------------------------------------------------------------------
 * Phase 2 Step 1: Extracted from control.c
 *
 * CheckLGates() — check list-mode gates for a single event.
 * Returns TRUE if the event passes all configured list-mode gates.
 * ----------------------------------------------------------------------- */
gboolean daq_check_lgates(gushort *Par);

/* -----------------------------------------------------------------------
 * Phase 2 Step 1: Extracted from control.c
 *
 * CompressToDisk() — compress and write one buffer of decoded events
 * to the open list-mode file Fp.
 * Returns TRUE on success, FALSE on disk error (triggers stop).
 *
 * Required context (globals still used during Phase 2):
 *   Setup         — compression format, LLD/ULD, gate config
 *   XEvents       — number of events in the current AcqBuf
 *   NPar1, NPar2  — parameter split per crate
 *   XWds          — total word count
 *   BytesWritten  — incremented here (statistics)
 *   AcqSignal     — set to Stop on unsupported format
 *   DrvFd         — used by ZlsError → StopNicely → camac_write_short
 * ----------------------------------------------------------------------- */
gint daq_compress_to_disk(FILE *Fp, gushort *AcqBuf);

/* -----------------------------------------------------------------------
 * Phase 2 Step 2: Extracted from control.c
 *
 * daq_print_buffer() — debug dump of a raw CAMAC word buffer.
 * Only active when Setup.Print.Yes is set and BuffersAcquired matches.
 * Globals: Setup (Print.Yes, Print.BufferNo, Print.Wds)
 * ----------------------------------------------------------------------- */
void daq_print_buffer(guint *CBuf, gint CWds);

/* -----------------------------------------------------------------------
 * Phase 2 Step 2: Extracted from control.c
 *
 * daq_decode_sparse_data() — parse one raw CMC100 CAMAC word buffer (CBuf)
 * into decoded per-event parameter arrays (AcqBuf).
 *
 * This is the CAMAC sparse-readout decode engine:
 *   PHILLIPS_START/END   → sparse ADC hits
 *   SEQUENTIAL_START/END → sequential ADC data
 *   SCALERS_START/END    → live scaler values
 *   CLEAR_START/END      → marks event boundaries
 *
 * Globals written: XEvents, XPtr, XWdsLeft, ScalerCurr[]
 * Globals read:    Setup, BuffersAcquired
 * No GTK calls.
 * ----------------------------------------------------------------------- */
void daq_decode_sparse_data(guint *CBuf, gint CWds, gushort *AcqBuf);

/* -----------------------------------------------------------------------
 * Phase 2 Step 3: Extracted from control.c
 *
 * daq_build_spectra() — iterates through the events in the decoded buffer
 * (AcqBuf) and processes them.
 *
 * Globals read: XEvents, NPar1
 * Globals written: BuffersProcessed
 * Calls: ProcessEvent() (which still lives in readfile.c for now)
 * ----------------------------------------------------------------------- */
void daq_build_spectra(gushort *AcqBuf);

/* -----------------------------------------------------------------------
 * Phase 2 Step 4: Extracted from control.c
 *
 * daq_acquire_data() — the complete acquisition thread body.
 *
 * Contains:
 *   - Hardware init (camac_naf, ExecuteClrCommands, ExecuteExtraCommands)
 *   - List-mode file open/close
 *   - Acquisition hot loops (normal CAMAC and sparse CMC100 mode)
 *   - Periodic display updates via UpdateDisplay()
 *   - Safety, logging, spectrum auto-save on stop
 * ----------------------------------------------------------------------- */
void *daq_acquire_data(gpointer Data);

/* -----------------------------------------------------------------------
 * Phase 3: DAQ Control API
 * GTK uses these to control the engine instead of direct pthread calls.
 * ----------------------------------------------------------------------- */
void daq_start(void);

/* -----------------------------------------------------------------------
 * Phase 4: Remote control & status API
 *
 * daq_stop() — signal the acquisition thread to stop cleanly.
 *   Equivalent to setting AcqSignal=Stop and disabling hardware LAMs.
 *   Safe to call from any thread (EPICS bridge, CLI, etc.).
 *   Does nothing if acquisition is not currently running.
 *
 * daq_get_status() — return current acquisition state.
 *   0 = STOPPED, 1 = PAUSED, 2 = RUNNING
 * ----------------------------------------------------------------------- */
void daq_stop(void);
int  daq_get_status(void);   /* 0=STOPPED 1=PAUSED 2=RUNNING */

/* -----------------------------------------------------------------------
 * Phase 4: Spectrum mutex
 *
 * Protects Oned16/Oned32/Twod16/Twod32 between:
 *   - The DAQ acquisition thread (writer via ProcessEvent)
 *   - lamps_shm_update() (reader, called from the same thread after
 *     BuildSpectra, but also snapshottable from bridge process)
 *
 * Lock is a recursive-capable spin-free pthread_mutex.
 * Must be held by daq_build_spectra() for the entire inner loop,
 * and by lamps_shm_update() for the entire copy.
 * ----------------------------------------------------------------------- */
void daq_spec_lock(void);
void daq_spec_unlock(void);

#endif /* DAQ_ENGINE_H */
