#!/usr/bin/env python3
"""
lamps_dashboard.py — LAMPS Live Monitoring & Control Dashboard

Phase 7.1: Live monitoring (status, metrics, spectrum)
Phase 7.2: Run control (START / STOP / RESET, run name entry)
Phase 7.3: Spectrum tools (ROI + area, log/lin scale, crosshair, auto-range)
Phase 7.4: Peak search (scipy.signal.find_peaks, markers, peak table)
Phase 7.5: Gaussian peak fitting (scipy.optimize.curve_fit, fit overlay, results)
Phase 7.6: Calibration (energy assignment, polynomial fit, keV axis, save/load)

Sim mode: Add --sim flag (or auto-detected when EPICS IOC unreachable).
  Generates the same realistic Gaussian spectrum as sim_shm.c:
    Peak A — Co-60  (1173 keV) at ch 512,  FWHM ~20 ch
    Peak B — Cs-137 (662  keV) at ch 1024, FWHM ~15 ch
    Peak C — K-40   (1461 keV) at ch 2048, FWHM ~25 ch
    + Compton continuum + flat noise floor
  START / STOP / RESET all work in sim mode.

Requirements:
  pip install pyqtgraph pyepics PyQt5

Usage:
  python3 lamps_dashboard.py [--prefix LAMPS] [--ioc localhost] [--sim]
"""

import sys
import argparse
import time
import numpy as np

from PyQt5.QtWidgets import (
    QApplication, QMainWindow, QWidget, QVBoxLayout, QHBoxLayout,
    QLabel, QComboBox, QFrame, QGroupBox, QPushButton, QLineEdit,
    QCheckBox, QSizePolicy, QDoubleSpinBox, QSpinBox,
    QTableWidget, QTableWidgetItem, QHeaderView, QSplitter,
    QDialog, QDialogButtonBox, QRadioButton, QButtonGroup,
    QFileDialog, QMessageBox, QTabWidget
)
from PyQt5.QtCore import QTimer, Qt, QThread, pyqtSignal
from PyQt5.QtGui import QFont, QColor

import pyqtgraph as pg

import analysis.background as bg_module
import analysis.peaksearch as peak_module
import analysis.fitting as fit_module
import analysis.calibration as cal_module
import analysis.isotopeid as iso_module
import analysis.efficiency as eff_module
import analysis.export as exp_module

# Load isotope library from JSON if present, else fallback
ISOTOPE_LIBRARY = iso_module.load_library()
SCIPY_AVAILABLE = True  # handled gracefully inside analysis modules

try:
    import epics
    EPICS_AVAILABLE = True
except ImportError:
    EPICS_AVAILABLE = False
    print("[WARN] pyepics not found — will use simulated data", file=sys.stderr)

# ---------------------------------------------------------------------------
# Configuration
# ---------------------------------------------------------------------------
DEFAULT_PREFIX  = "LAMPS"
DEFAULT_IOC     = "localhost"
REFRESH_MS      = 200
SPEC_REFRESH_MS = 500
MAX_DETECTORS   = 8
MAX_CHANNELS    = 8192

STATUS_COLORS = {
    "RUNNING":  ("#1aff6e", "#003010"),
    "PAUSED":   ("#ffcc00", "#2a2000"),
    "STOPPED":  ("#aaaaaa", "#1a1a1a"),
    "OFFLINE":  ("#ff4444", "#2a0000"),
    "SIM":      ("#00d4ff", "#001a22"),
    "--":       ("#555555", "#111111"),
}

ISOTOPE_LIBRARY = {
    "Co-60": [1173.2, 1332.5],
    "Cs-137": [661.7],
    "K-40": [1460.8],
    "Na-22": [511.0, 1274.5],
    "Am-241": [59.5],
}


# ---------------------------------------------------------------------------
# Simulated Data Backend
# Mirrors sim_shm.c exactly: same peaks, same Poisson noise, seeded PRNG.
# ---------------------------------------------------------------------------
class SimulatedBackend:
    """
    Self-contained simulation that generates the same realistic gamma spectrum
    as the sim_shm C program.  Thread-safe: all mutable state is protected
    by a lock; the poller thread calls tick() and the GUI calls get_*().
    """
    N_CHAN  = 8192
    SIM_HZ  = 5      # ticks per second

    def __init__(self):
        import threading
        self._lock        = threading.Lock()
        self._rng_state   = 0xdeadbeefcafe1234  # same seed as sim_shm.c
        self._spectrum    = np.zeros(self.N_CHAN, dtype=np.uint32)
        self._events      = 0
        self._tick        = 0
        self._state       = "STOPPED"   # "RUNNING" | "STOPPED"
        self._run_name    = ""
        self._start_tick  = 0

    # ── Simple XOR-shift PRNG (same as sim_shm.c) ────────────────────────────
    def _rng_uniform(self):
        x = self._rng_state & 0xFFFFFFFFFFFFFFFF
        x ^= (x << 13) & 0xFFFFFFFFFFFFFFFF
        x ^= (x >> 7)  & 0xFFFFFFFFFFFFFFFF
        x ^= (x << 17) & 0xFFFFFFFFFFFFFFFF
        self._rng_state = x
        return (x & 0xFFFFFFFF) / 4294967296.0

    def _poisson(self, lam):
        """Poisson variate — Normal approx for lam > 30."""
        if lam <= 0.0:
            return 0
        if lam > 30.0:
            u1 = max(self._rng_uniform(), 1e-12)
            u2 = self._rng_uniform()
            z  = (-2.0 * np.log(u1)) ** 0.5 * np.cos(6.28318530 * u2)
            n  = int(lam + z * (lam ** 0.5) + 0.5)
            return max(0, n)
        k = 0
        L = np.exp(-lam)
        p = 1.0
        while True:
            p *= self._rng_uniform()
            k += 1
            if p <= L:
                break
        return k - 1

    @staticmethod
    def _gauss(ch_arr, mu, fwhm, amplitude):
        """Gaussian shape — vectorised over ch_arr."""
        sigma = fwhm / 2.3548
        return amplitude * np.exp(-0.5 * ((ch_arr - mu) / sigma) ** 2)

    # ── Called once per SIM_HZ tick from the poller thread ──────────────────
    def tick(self):
        with self._lock:
            if self._state != "RUNNING":
                return

            ch = np.arange(self.N_CHAN, dtype=np.float64)
            rate  = self._gauss(ch,  512,  20,  8.0)   # Co-60
            rate += self._gauss(ch, 1024,  15, 12.0)   # Cs-137
            rate += self._gauss(ch, 2048,  25,  5.0)   # K-40
            rate += 0.5 * np.exp(-ch / 1200.0)          # Compton
            rate += 0.05                                 # noise floor

            # Vectorised Poisson draws — same physics as sim_shm.c, 1000× faster
            counts = np.random.poisson(rate).astype(np.uint32)
            self._spectrum += counts
            new_evts = int(counts.sum())

            self._events += new_evts
            self._tick   += 1

    # ── Snapshot API (called from poller, safe under lock) ──────────────────
    def get_metrics(self):
        with self._lock:
            if self._state == "RUNNING":
                elapsed = (self._tick - self._start_tick) / self.SIM_HZ
                # event_rate: estimate from last tick
                ev_rate = float(self._events) / max(elapsed, 0.001)
            else:
                elapsed = getattr(self, '_stopped_elapsed', 0.0)
                ev_rate = 0.0
            return {
                'status':    self._state,
                'run_name':  self._run_name,
                'elapsed':   elapsed if self._state == "RUNNING" else getattr(self, '_stopped_elapsed', None),
                'total_ev':  self._events if self._events > 0 else None,
                'ev_rate':   ev_rate if self._state == "RUNNING" else None,
                'bufs_acq':  self._tick,
                'bufs_proc': self._tick,
                'cmd_rn':    self._run_name,
            }

    def get_spectrum(self, det=1):
        with self._lock:
            return self._spectrum.copy().astype(np.float32)

    # ── Control API ──────────────────────────────────────────────────────────
    def start(self, run_name):
        with self._lock:
            self._run_name   = run_name or "SIM_RUN"
            self._spectrum[:] = 0
            self._events      = 0
            self._start_tick  = self._tick
            self._state       = "RUNNING"
            print(f"[SIM] START  run_name={self._run_name}", flush=True)

    def stop(self):
        with self._lock:
            if self._state == "RUNNING":
                self._stopped_elapsed = (self._tick - self._start_tick) / self.SIM_HZ
            self._state = "STOPPED"
            print("[SIM] STOP", flush=True)

    def reset(self):
        with self._lock:
            self._state       = "STOPPED"
            self._spectrum[:] = 0
            self._events      = 0
            self._start_tick  = self._tick
            self._stopped_elapsed = 0.0
            print("[SIM] RESET", flush=True)

    @property
    def is_running(self):
        with self._lock:
            return self._state == "RUNNING"


# ---------------------------------------------------------------------------
# EPICS proxy
# ---------------------------------------------------------------------------
class EpicsProxy:
    def __init__(self, prefix: str, ioc: str):
        self.prefix = prefix
        self.ioc    = ioc
        self._pv_cache: dict = {}   # suffix → epics.PV object
        if EPICS_AVAILABLE:
            import os
            # YES = use local broadcast so softIoc is found automatically
            os.environ.setdefault("EPICS_CA_ADDR_LIST",      ioc)
            os.environ.setdefault("EPICS_CA_AUTO_ADDR_LIST", "YES")

    def _pv(self, suffix: str):
        """Return a cached, auto-connecting PV object (silent on disconnect)."""
        if not EPICS_AVAILABLE:
            return None
        if suffix not in self._pv_cache:
            self._pv_cache[suffix] = epics.PV(
                f"{self.prefix}:{suffix}",
                auto_monitor=False,       # poll-only, no CA monitors
                connection_timeout=0.0,   # don't block on construction
                verbose=False,            # suppress connection messages
            )
        return self._pv_cache[suffix]

    def get(self, suffix, as_string=False, default=None):
        pv = self._pv(suffix)
        if pv is None:
            return default
        try:
            v = pv.get(as_string=as_string, timeout=0.05, use_monitor=False)
            return v if v is not None else default
        except Exception:
            return default

    def put(self, suffix, value) -> bool:
        if not EPICS_AVAILABLE:
            print(f"[STUB] caput {self.prefix}:{suffix} = {value}")
            return True
        try:
            return bool(epics.caput(
                f"{self.prefix}:{suffix}", value, timeout=2.0
            ))
        except Exception as e:
            print(f"[WARN] caput {suffix} failed: {e}", file=sys.stderr)
            return False

    def get_waveform(self, suffix, nmax=MAX_CHANNELS):
        pv = self._pv(suffix)
        if pv is None:
            return np.zeros(nmax, dtype=np.float32)
        try:
            v = pv.get(timeout=0.15, use_monitor=False)
            # PV not yet connected or returned empty array
            if v is None or (hasattr(v, '__len__') and len(v) == 0):
                return np.zeros(nmax, dtype=np.float32)
            # DBR_LONG (int32) data from bridge — keep as float64,
            # clip any wrap-around negatives to 0 before returning.
            a = np.asarray(v, dtype=np.float64)
            a = np.maximum(a, 0)
            if len(a) >= nmax:
                return a[:nmax].astype(np.float32)
            else:
                return np.pad(a, (0, nmax - len(a))).astype(np.float32)
        except Exception:
            return np.zeros(nmax, dtype=np.float32)

    def probe_connection(self, timeout=1.0) -> bool:
        """Quick probe: try to read STATUS PV.  Returns True if connected."""
        if not EPICS_AVAILABLE:
            return False
        try:
            pv = epics.PV(
                f"{self.prefix}:STATUS",
                auto_monitor=False,
                connection_timeout=timeout,
                verbose=False,
            )
            v = pv.get(timeout=timeout, use_monitor=False)
            return v is not None
        except Exception:
            return False


# ---------------------------------------------------------------------------
# Background polling thread  (keeps the GUI thread free)
# Works with either live EPICS or the SimulatedBackend.
# ---------------------------------------------------------------------------
class EpicsPoller(QThread):
    """
    Polls either the EPICS proxy or the SimulatedBackend — never on the GUI
    thread.  Emits signals that Qt delivers safely across threads.
    """
    metrics_ready  = pyqtSignal(dict)       # fired every REFRESH_MS
    spectrum_ready = pyqtSignal(object)     # fired every SPEC_REFRESH_MS

    def __init__(self, proxy: 'EpicsProxy', sim: 'SimulatedBackend | None' = None,
                 parent=None):
        super().__init__(parent)
        self._proxy      = proxy
        self._sim        = sim            # None → use EPICS
        self._running    = True
        self._det        = 1
        self._tick       = 0
        self._spec_every = max(1, SPEC_REFRESH_MS // REFRESH_MS)  # 2

    def set_detector(self, det: int):
        self._det = det

    def stop_polling(self):
        self._running = False

    # ── Sim-mode ticker: advance simulation by one tick every SIM_HZ interval
    _sim_subtick   = 0
    _sim_hz_ratio  = max(1, 1000 // (SimulatedBackend.SIM_HZ * REFRESH_MS))  # ticks between sim advances

    def run(self):
        while self._running:
            if self._sim is not None:
                # ── Simulated backend ──────────────────────────────────────
                # Advance simulator at ~5 Hz regardless of poll rate
                self._sim_subtick = (self._sim_subtick + 1) % max(1, 1000 // (SimulatedBackend.SIM_HZ * REFRESH_MS))
                if self._sim_subtick == 0:
                    self._sim.tick()

                metrics = self._sim.get_metrics()
                self.metrics_ready.emit(metrics)

                self._tick = (self._tick + 1) % self._spec_every
                if self._tick == 0:
                    arr = self._sim.get_spectrum(self._det)
                    self.spectrum_ready.emit(arr)

            else:
                # ── Live EPICS backend ─────────────────────────────────────
                p = self._proxy
                metrics = {
                    'status':    (p.get('STATUS',       as_string=True, default='--') or '--').strip(),
                    'run_name':  (p.get('RUN_NAME',      as_string=True, default='')  or '').strip(),
                    'elapsed':    p.get('ELAPSED_SEC',   default=None),
                    'total_ev':   p.get('TOTAL_EVENTS',  default=None),
                    'ev_rate':    p.get('EVENT_RATE',    default=None),
                    'bufs_acq':   p.get('BUFS_ACQUIRED',  default=None),
                    'bufs_proc':  p.get('BUFS_PROCESSED', default=None),
                    'cmd_rn':    (p.get('CMD:RUN_NAME',  as_string=True, default='') or '').strip(),
                }
                self.metrics_ready.emit(metrics)

                self._tick = (self._tick + 1) % self._spec_every
                if self._tick == 0:
                    arr = p.get_waveform(f'SPEC1D:{self._det:03d}')
                    self.spectrum_ready.emit(arr)

            self.msleep(REFRESH_MS)


# ---------------------------------------------------------------------------
class StatusBadge(QLabel):
    def __init__(self):
        super().__init__("  --  ")
        self.setAlignment(Qt.AlignCenter)
        self.setFont(QFont("Monospace", 14, QFont.Bold))
        self.setMinimumWidth(180)
        self.setMinimumHeight(42)
        self._apply("--")

    def _apply(self, s):
        fg, bg = STATUS_COLORS.get(s, STATUS_COLORS["--"])
        self.setStyleSheet(f"color:{fg}; background:{bg}; border-radius:6px; padding:4px 16px;")

    def set_status(self, status: str):
        self.setText(f"  {status}  ")
        self._apply(status)


class MetricRow(QWidget):
    def __init__(self, label: str):
        super().__init__()
        row = QHBoxLayout(self)
        row.setContentsMargins(0, 2, 0, 2)
        lbl = QLabel(label)
        lbl.setFixedWidth(140)
        lbl.setStyleSheet("color:#888; font-size:9pt;")
        row.addWidget(lbl)
        self.val = QLabel("—")
        self.val.setFont(QFont("Monospace", 10, QFont.Bold))
        self.val.setStyleSheet("color:#eee;")
        row.addWidget(self.val)
        row.addStretch()

    def set(self, text: str):
        self.val.setText(text)


# ---------------------------------------------------------------------------
# Phase 7.2: Control Panel
# ---------------------------------------------------------------------------
class _EpicsPutWorker(QThread):
    """Fire-and-forget: run caput on a background thread so the GUI never stalls."""
    done = pyqtSignal(str, bool)   # (label, success)

    def __init__(self, epics_proxy, suffix, value, label):
        super().__init__()
        self._proxy = epics_proxy
        self._suffix = suffix
        self._value  = value
        self._label  = label

    def run(self):
        ok = self._proxy.put(self._suffix, self._value)
        self.done.emit(self._label, ok)


class ControlPanel(QGroupBox):
    """
    Run control panel: START / STOP / RESET buttons.

    Fixed issues vs. original:
      1. caput runs in a background QThread — GUI thread never blocks.
      2. 500 ms debounce prevents rapid double-fire.
      3. command_lockout_ms (2500 ms) — after a command the panel ignores
         poller status overrides while the IOC processes the request.
    """
    command_issued   = pyqtSignal(str)   # "START" | "STOP" | "RESET"
    DEBOUNCE_MS      = 500
    LOCKOUT_MS       = 2500   # suppress poller overrides after a command

    def __init__(self, epics_proxy: EpicsProxy,
                 sim: 'SimulatedBackend | None' = None):
        super().__init__("Run Control")
        self._epics = epics_proxy
        self._sim   = sim
        self._prefilled = False
        self._workers   = []    # keep worker references alive
        self._debounce_timer = QTimer(self)
        self._debounce_timer.setSingleShot(True)
        self._debounce_timer.setInterval(self.DEBOUNCE_MS)
        self._build()

    def _build(self):
        layout = QVBoxLayout(self)
        layout.setSpacing(8)

        rn_row = QHBoxLayout()
        lbl = QLabel("Run Name:")
        lbl.setFixedWidth(80)
        lbl.setStyleSheet("color:#aaa; font-size:9pt;")
        rn_row.addWidget(lbl)
        self.run_name_edit = QLineEdit()
        self.run_name_edit.setPlaceholderText("e.g.  run042")
        self.run_name_edit.setMaxLength(39)
        self.run_name_edit.setStyleSheet(
            "background:#1e1e1e; color:#eee; border:1px solid #333; "
            "border-radius:4px; padding:3px 6px;"
        )
        rn_row.addWidget(self.run_name_edit)
        layout.addLayout(rn_row)

        btn_row = QHBoxLayout(); btn_row.setSpacing(8)
        self.btn_start = self._btn("START",  "#00aa44", "#003318")
        self.btn_stop  = self._btn("STOP",   "#cc3300", "#330d00")
        self.btn_reset = self._btn("RESET",  "#aa6600", "#2a1800")
        self.btn_start.clicked.connect(self._do_start)
        self.btn_stop.clicked.connect(self._do_stop)
        self.btn_reset.clicked.connect(self._do_reset)
        for b in [self.btn_start, self.btn_stop, self.btn_reset]:
            btn_row.addWidget(b)
        layout.addLayout(btn_row)

        self.lbl_fb = QLabel("")
        self.lbl_fb.setStyleSheet("color:#666; font-size:8pt;")
        self.lbl_fb.setAlignment(Qt.AlignCenter)
        layout.addWidget(self.lbl_fb)

    @staticmethod
    def _btn(text, fg, bg):
        b = QPushButton(text)
        b.setMinimumHeight(34)
        b.setFont(QFont("Sans", 9, QFont.Bold))
        b.setStyleSheet(
            f"QPushButton{{color:{fg};background:{bg};border:1px solid {fg};"
            f"border-radius:5px;padding:4px 10px;}}"
            f"QPushButton:hover{{background:{fg};color:#000;}}"
            f"QPushButton:disabled{{color:#333;background:#111;border-color:#222;}}"
        )
        return b

    def update_from_status(self, status: str):
        running = (status == "RUNNING")
        stopped = (status in ("STOPPED", "OFFLINE", "--"))
        self.btn_start.setEnabled(stopped)
        self.btn_stop.setEnabled(running)

    def prefill_run_name(self, name: str):
        if not self._prefilled and name:
            self.run_name_edit.setText(name.strip())
            self._prefilled = True

    # ── Command handlers ────────────────────────────────────────────────────
    def _do_start(self):
        if self._debounce_timer.isActive():
            return
        self._debounce_timer.start()
        rn = self.run_name_edit.text().strip()
        if not rn:
            self._fb("Enter a run name first", "#ffcc00"); return
        # Immediately disable both buttons to prevent double-fire
        self.btn_start.setEnabled(False)
        self.btn_stop.setEnabled(False)
        self._fb("Sending START...", "#aaa")
        if self._sim is not None:
            self._sim.start(rn)
            self._fb(f"[SIM] STARTED  ({rn})", "#1aff6e")
            self.command_issued.emit("START")
        else:
            # Non-blocking: send RUN_NAME first, then START on worker thread
            self._epics.put("CMD:RUN_NAME", rn)   # usually fast string put
            w = _EpicsPutWorker(self._epics, "CMD:START", 1, f"START ({rn})")
            w.done.connect(self._on_put_done)
            self._workers.append(w)
            w.start()
            self.command_issued.emit("START")

    def _do_stop(self):
        if self._debounce_timer.isActive():
            return
        self._debounce_timer.start()
        self.btn_start.setEnabled(False)
        self.btn_stop.setEnabled(False)
        self._fb("Sending STOP...", "#aaa")
        if self._sim is not None:
            self._sim.stop()
            self._fb("[SIM] STOPPED", "#ffcc00")
            self.command_issued.emit("STOP")
        else:
            w = _EpicsPutWorker(self._epics, "CMD:STOP", 1, "STOP")
            w.done.connect(self._on_put_done)
            self._workers.append(w)
            w.start()
            self.command_issued.emit("STOP")

    def _do_reset(self):
        if self._debounce_timer.isActive():
            return
        self._debounce_timer.start()
        self.btn_start.setEnabled(False)
        self.btn_stop.setEnabled(False)
        self._fb("Sending RESET...", "#aaa")
        if self._sim is not None:
            self._sim.reset()
            self._fb("[SIM] RESET", "#ff8800")
            self.command_issued.emit("RESET")
        else:
            w = _EpicsPutWorker(self._epics, "CMD:RESET", 1, "RESET")
            w.done.connect(self._on_put_done)
            self._workers.append(w)
            w.start()
            self.command_issued.emit("RESET")

    def _on_put_done(self, label: str, ok: bool):
        # Clean up finished worker references
        self._workers = [w for w in self._workers if w.isRunning()]
        if ok:
            self._fb(f"Sent: {label}", "#1aff6e")
        else:
            self._fb(f"FAILED: {label}", "#ff4444")
            # Re-enable buttons on failure so operator can retry
            self.btn_start.setEnabled(True)
            self.btn_stop.setEnabled(True)

    def _fb(self, msg, color):
        self.lbl_fb.setText(msg)
        self.lbl_fb.setStyleSheet(f"color:{color}; font-size:8pt;")
        QTimer.singleShot(4000, lambda: self.lbl_fb.setText(""))


# ---------------------------------------------------------------------------
# Phase 7.3: Spectrum Panel (plot + tools)
# ---------------------------------------------------------------------------
class SpectrumPanel(QGroupBox):
    """
    Live spectrum view with tools:
      Phase 7.1 — PyQtGraph PlotWidget, detector selector
      Phase 7.3 — LinearRegionItem ROI, crosshair, log/lin, auto-range
      Phase 7.4 — Peak search (scipy.signal.find_peaks), markers, peak table
      Phase 7.5 — Gaussian fitting (scipy.optimize.curve_fit), fit overlay
      Phase 7.6 — Calibration dialog (polynomial fit, keV axis, save/load)
    """
    def __init__(self, epics_proxy: EpicsProxy):
        super().__init__("Live Spectrum")
        self._epics = epics_proxy
        self._current_det = 1
        self._data       = np.zeros(MAX_CHANNELS, dtype=np.float32)
        self._log_mode   = False
        self._peak_lines = []   # Phase 7.4: vertical marker InfiniteLines
        self._fit_curves = []   # Phase 7.5: Gaussian fit PlotDataItems
        self._cal_poly   = None # Phase 7.6: np.poly1d object when calibrated
        self._build()

    # ── Build ───────────────────────────────────────────────────────────────
    def _build(self):
        layout = QVBoxLayout(self)
        layout.setSpacing(6)

        # ── Toolbar row 1: detector + scale + autorange ─────────────────
        row1 = QHBoxLayout()

        row1.addWidget(QLabel("Detector:"))
        self.det_combo = QComboBox()
        self.det_combo.setFixedWidth(130)
        for i in range(1, MAX_DETECTORS + 1):
            self.det_combo.addItem(f"SPEC1D:{i:03d}", i)
        self.det_combo.currentIndexChanged.connect(self._on_det_changed)
        row1.addWidget(self.det_combo)
        row1.addSpacing(16)

        self.btn_log = QPushButton("Log Y")
        self.btn_log.setCheckable(True)
        self.btn_log.setFixedWidth(60)
        self.btn_log.setStyleSheet(
            "QPushButton{color:#aaa;background:#1e1e1e;border:1px solid #333;"
            "border-radius:4px;padding:2px 6px;font-size:9pt;}"
            "QPushButton:checked{color:#00d4ff;border-color:#00d4ff;background:#001f2a;}"
        )
        self.btn_log.toggled.connect(self._toggle_log)
        row1.addWidget(self.btn_log)

        self.btn_auto = QPushButton("⊡ Auto")
        self.btn_auto.setFixedWidth(64)
        self.btn_auto.setStyleSheet(
            "QPushButton{color:#aaa;background:#1e1e1e;border:1px solid #333;"
            "border-radius:4px;padding:2px 6px;font-size:9pt;}"
            "QPushButton:hover{color:#fff;border-color:#aaa;}"
        )
        self.btn_auto.clicked.connect(self._auto_range)
        row1.addWidget(self.btn_auto)

        # Phase 7.6: Calibration button
        self.btn_cal = QPushButton("📐 Calibrate")
        self.btn_cal.setFixedWidth(90)
        self.btn_cal.setStyleSheet(
            "QPushButton{color:#bb88ff;background:#1a0d2a;border:1px solid #553388;"
            "border-radius:4px;padding:2px 6px;font-size:9pt;}"
            "QPushButton:hover{background:#2a1040;border-color:#bb88ff;}"
        )
        self.btn_cal.clicked.connect(self._open_calibration_dialog)
        row1.addWidget(self.btn_cal)

        # Calibration active indicator
        self.lbl_cal_active = QLabel("")
        self.lbl_cal_active.setStyleSheet(
            "color:#bb88ff; font-size:8pt; font-style:italic;"
        )
        row1.addWidget(self.lbl_cal_active)
        
        row1.addSpacing(16)
        
        # Phase 2: Export buttons
        self.btn_export_spe = QPushButton("📥 SPE")
        self.btn_export_spe.setFixedWidth(64)
        self.btn_export_spe.setStyleSheet(
            "QPushButton{color:#aaddff;background:#0d1a26;border:1px solid #336699;"
            "border-radius:4px;padding:2px 6px;font-size:9pt;}"
            "QPushButton:hover{background:#132639;border-color:#aaddff;}"
        )
        self.btn_export_spe.clicked.connect(self._export_spe)
        row1.addWidget(self.btn_export_spe)

        self.btn_export_csv = QPushButton("📥 CSV")
        self.btn_export_csv.setFixedWidth(64)
        self.btn_export_csv.setStyleSheet(
            "QPushButton{color:#aaddff;background:#0d1a26;border:1px solid #336699;"
            "border-radius:4px;padding:2px 6px;font-size:9pt;}"
            "QPushButton:hover{background:#132639;border-color:#aaddff;}"
        )
        self.btn_export_csv.clicked.connect(self._export_csv)
        row1.addWidget(self.btn_export_csv)

        row1.addStretch()

        # Channel + counts info (crosshair readout)
        self.lbl_cursor = QLabel("Ch: —   Cts: —")
        self.lbl_cursor.setStyleSheet("color:#555; font-size:9pt; font-family:monospace;")
        row1.addWidget(self.lbl_cursor)

        # Peak info
        self.lbl_peak = QLabel("Peak: —")
        self.lbl_peak.setStyleSheet("color:#777; font-size:9pt;")
        row1.addWidget(self.lbl_peak)
        layout.addLayout(row1)

        # ── Plot widget ─────────────────────────────────────────────────
        pg.setConfigOption("background", "#0d0d0d")
        pg.setConfigOption("foreground", "#888")
        self.plot = pg.PlotWidget()
        self.plot.setLabel("left",   "Counts")
        self.plot.setLabel("bottom", "Channel")
        self.plot.showGrid(x=True, y=True, alpha=0.15)
        self.plot.getPlotItem().setMenuEnabled(False)

        # Spectrum curve — PyQtGraph 0.13+: stepMode requires len(x)==len(y)
        self.curve = self.plot.plot(
            np.arange(MAX_CHANNELS, dtype=np.float32),
            np.zeros(MAX_CHANNELS, dtype=np.float32),
            pen=pg.mkPen("#00d4ff", width=1),
            stepMode="left"
        )

        # ── Phase 7.3: ROI (LinearRegionItem) ──────────────────────────
        self.roi = pg.LinearRegionItem(
            values=[100, 500],
            brush=pg.mkBrush(0, 180, 255, 25),
            pen=pg.mkPen("#00b4dd", width=1, style=Qt.DashLine),
            movable=True,
            swapMode="sort"
        )
        self.roi.sigRegionChanged.connect(self._update_roi_readout)
        self.plot.addItem(self.roi)

        # ── Phase 7.3: Crosshair ────────────────────────────────────────
        self._vline = pg.InfiniteLine(angle=90, movable=False,
                                      pen=pg.mkPen("#444", width=1))
        self._hline = pg.InfiniteLine(angle=0,  movable=False,
                                      pen=pg.mkPen("#444", width=1))
        self.plot.addItem(self._vline, ignoreBounds=True)
        self.plot.addItem(self._hline, ignoreBounds=True)

        self.plot.scene().sigMouseMoved.connect(self._on_mouse_move)

        layout.addWidget(self.plot)

        # ── Toolbar row 2: ROI readout ───────────────────────────────────
        row2 = QHBoxLayout()
        row2.addWidget(QLabel("ROI:"))

        self.lbl_roi_range = QLabel("ch 100 – 500")
        self.lbl_roi_range.setStyleSheet(
            "color:#00b4dd; font-size:9pt; font-family:monospace;"
        )
        row2.addWidget(self.lbl_roi_range)
        row2.addSpacing(20)

        row2.addWidget(QLabel("Gross Area:"))
        self.lbl_roi_gross = QLabel("—")
        self.lbl_roi_gross.setStyleSheet(
            "color:#888; font-size:9pt; font-family:monospace;"
        )
        row2.addWidget(self.lbl_roi_gross)
        row2.addSpacing(15)

        row2.addWidget(QLabel("Background:"))
        self.cmb_bg = QComboBox()
        self.cmb_bg.addItems(["Trapezoid", "SNIP", "Polynomial", "None"])
        self.cmb_bg.setStyleSheet("background:#1e1e1e; color:#ccc; border:1px solid #333; border-radius:3px;")
        self.cmb_bg.currentIndexChanged.connect(self._update_roi_readout)
        row2.addWidget(self.cmb_bg)
        row2.addSpacing(15)

        row2.addWidget(QLabel("Net Area:"))
        self.lbl_roi_area = QLabel("—")
        self.lbl_roi_area.setStyleSheet(
            "color:#00d4ff; font-size:9pt; font-weight:bold; font-family:monospace;"
        )
        row2.addWidget(self.lbl_roi_area)
        row2.addSpacing(20)

        row2.addWidget(QLabel("Centroid:"))
        self.lbl_roi_centroid = QLabel("—")
        self.lbl_roi_centroid.setStyleSheet(
            "color:#7fd4ff; font-size:9pt; font-family:monospace;"
        )
        row2.addWidget(self.lbl_roi_centroid)
        row2.addStretch()

        # ROI visibility toggle
        self.chk_roi = QCheckBox("Show ROI")
        self.chk_roi.setChecked(True)
        self.chk_roi.setStyleSheet("color:#666; font-size:9pt;")
        self.chk_roi.toggled.connect(self._toggle_roi)
        row2.addWidget(self.chk_roi)

        layout.addLayout(row2)

        # ── Phase 7.4: Peak Search toolbar ───────────────────────────────
        row3 = QHBoxLayout()
        row3.addWidget(QLabel("Peak Search:"))

        row3.addWidget(QLabel("Min height (%)"))
        self.spn_height = QDoubleSpinBox()
        self.spn_height.setRange(0.1, 100.0)
        self.spn_height.setValue(5.0)
        self.spn_height.setSingleStep(0.5)
        self.spn_height.setFixedWidth(70)
        self.spn_height.setToolTip(
            "Minimum peak height as % of the spectrum maximum")
        self.spn_height.setStyleSheet(
            "background:#1e1e1e; color:#ccc; border:1px solid #333; border-radius:3px;"
        )
        row3.addWidget(self.spn_height)

        row3.addWidget(QLabel("Min sep (ch)"))
        self.spn_sep = QSpinBox()
        self.spn_sep.setRange(1, 500)
        self.spn_sep.setValue(10)
        self.spn_sep.setFixedWidth(60)
        self.spn_sep.setToolTip("Minimum distance between peaks in channels")
        self.spn_sep.setStyleSheet(
            "background:#1e1e1e; color:#ccc; border:1px solid #333; border-radius:3px;"
        )
        row3.addWidget(self.spn_sep)

        self.btn_find = QPushButton("🔍 Find Peaks")
        self.btn_find.setFixedWidth(100)
        self.btn_find.setStyleSheet(
            "QPushButton{color:#ffd700;background:#1a1600;border:1px solid #554400;"
            "border-radius:4px;padding:2px 8px;font-size:9pt;}"
            "QPushButton:hover{background:#332a00;border-color:#ffd700;}"
            "QPushButton:disabled{color:#333;background:#111;border-color:#222;}"
        )
        self.btn_find.clicked.connect(self._run_peak_search)
        self.btn_find.setEnabled(SCIPY_AVAILABLE)
        row3.addWidget(self.btn_find)

        self.btn_clear_peaks = QPushButton("✕ Clear")
        self.btn_clear_peaks.setFixedWidth(60)
        self.btn_clear_peaks.setStyleSheet(
            "QPushButton{color:#888;background:#1e1e1e;border:1px solid #333;"
            "border-radius:4px;padding:2px 6px;font-size:9pt;}"
            "QPushButton:hover{color:#fff;border-color:#888;}"
        )
        self.btn_clear_peaks.clicked.connect(self._clear_peak_markers)
        row3.addWidget(self.btn_clear_peaks)

        self.lbl_npeaks = QLabel("")
        self.lbl_npeaks.setStyleSheet("color:#666; font-size:9pt;")
        row3.addWidget(self.lbl_npeaks)
        row3.addStretch()

        if not SCIPY_AVAILABLE:
            warn = QLabel("⚠ scipy missing")
            warn.setStyleSheet("color:#884400; font-size:8pt;")
            row3.addWidget(warn)

        layout.addLayout(row3)

        # ── Phase 7.4: Peak table ─────────────────────────────────────────
        self.peak_table = QTableWidget(0, 5)
        self.peak_table.setHorizontalHeaderLabels(
            ["Centroid", "Gross Counts", "Net Counts", "FWHM (ch)", "Isotope ID"]
        )
        self.peak_table.horizontalHeader().setSectionResizeMode(QHeaderView.Stretch)
        self.peak_table.setMaximumHeight(120)
        self.peak_table.setSelectionBehavior(QTableWidget.SelectRows)
        self.peak_table.setEditTriggers(QTableWidget.NoEditTriggers)
        self.peak_table.setAlternatingRowColors(True)
        self.peak_table.setStyleSheet(
            "QTableWidget { background:#0d0d0d; color:#ccc; "
            "  gridline-color:#222; font-size:9pt; border:none; }"
            "QTableWidget::item:alternate { background:#111111; }"
            "QTableWidget::item:selected { background:#003344; }"
            "QHeaderView::section { background:#1a1a1a; color:#888; "
            "  border:1px solid #222; padding:2px; font-size:8pt; }"
        )
        layout.addWidget(self.peak_table)

        # ── Phase 7.5: Fit results row ────────────────────────────────────────
        fit_row = QHBoxLayout()
        fit_lbl = QLabel("Fit:")
        fit_lbl.setStyleSheet("color:#ff9900; font-size:9pt; font-weight:bold;")
        fit_row.addWidget(fit_lbl)

        fit_row.addWidget(QLabel("Click a row in the table to fit →"))
        
        self.cmb_fit_model = QComboBox()
        self.cmb_fit_model.addItems(["Auto", "Gaussian", "Gaussian+Slope", "Hypermet", "Voigt"])
        self.cmb_fit_model.setStyleSheet("background:#1e1e1e; color:#ccc; border:1px solid #333; border-radius:3px;")
        fit_row.addWidget(self.cmb_fit_model)

        self.lbl_fit_centroid = self._fit_val("Centroid", "#ffcc44")
        self.lbl_fit_fwhm     = self._fit_val("FWHM",     "#ffaa22")
        self.lbl_fit_area     = self._fit_val("Area",     "#ff8800")
        self.lbl_fit_chi2     = self._fit_val("χ²/dof",   "#cc6600")
        for w in [self.lbl_fit_centroid, self.lbl_fit_fwhm,
                  self.lbl_fit_area, self.lbl_fit_chi2]:
            fit_row.addWidget(w)

        self.btn_clear_fits = QPushButton("✕ Fits")
        self.btn_clear_fits.setFixedWidth(56)
        self.btn_clear_fits.setStyleSheet(
            "QPushButton{color:#664400;background:#1e1e1e;border:1px solid #333;"
            "border-radius:4px;padding:2px 5px;font-size:9pt;}"
            "QPushButton:hover{color:#ff9900;border-color:#ff9900;}"
        )
        self.btn_clear_fits.clicked.connect(self._clear_fits)
        fit_row.addWidget(self.btn_clear_fits)
        fit_row.addStretch()
        layout.addLayout(fit_row)

        # Connect table click → fit that peak (Phase 7.5)
        self.peak_table.cellClicked.connect(self._on_table_click)

    # ── Public API used by the main window ──────────────────────────────────
    def update_spectrum(self, data: np.ndarray):
        """Set new spectrum data and refresh plot + ROI + peak markers."""
        # Guard: ignore empty arrays (PV not yet connected)
        if data is None or (hasattr(data, '__len__') and len(data) == 0):
            return
        self._data = np.asarray(data, dtype=np.float32)
        nonzero_idx = np.nonzero(self._data)[0]
        if nonzero_idx.size > 0:
            nz = int(nonzero_idx[-1]) + 1
        else:
            nz = 64
        nz = max(nz, 64)
        disp = self._data[:nz]
        x    = np.arange(nz, dtype=np.float32)
        self.curve.setData(x=x, y=disp)

        if disp.any():
            pk = int(np.argmax(disp))
            self.lbl_peak.setText(f"Peak: ch {pk}  ({int(disp[pk]):,} cts)")
        else:
            self.lbl_peak.setText("Peak: —")

        self._update_roi_readout()
        # Peak markers stay on plot; they are only re-run when user clicks Find.

    @property
    def current_detector(self) -> int:
        return self._current_det

    # ── Slot: detector changed ───────────────────────────────────────────────
    def _on_det_changed(self, idx):
        self._current_det = self.det_combo.itemData(idx)
        self._data = np.zeros(MAX_CHANNELS, dtype=np.float32)
        self.curve.setData(
            x=np.arange(MAX_CHANNELS, dtype=np.float32),
            y=np.zeros(MAX_CHANNELS, dtype=np.float32)
        )
        self.lbl_peak.setText("Peak: —")
        self.lbl_roi_gross.setText("—")
        self.lbl_roi_area.setText("—")
        self.lbl_roi_centroid.setText("—")

    # ── Slot: log/lin toggle ─────────────────────────────────────────────────
    def _toggle_log(self, checked: bool):
        self._log_mode = checked
        self.plot.setLogMode(y=checked)
        self.btn_log.setText("Lin Y" if checked else "Log Y")

    # ── Slot: auto-range ────────────────────────────────────────────────────
    def _auto_range(self):
        self.plot.autoRange()

    # ── Slot: ROI visibility ─────────────────────────────────────────────────
    def _toggle_roi(self, visible: bool):
        self.roi.setVisible(visible)

    # ── Slot: ROI moved/resized → update area + centroid readout ────────────
    def _update_roi_readout(self):
        lo, hi = self.roi.getRegion()
        lo_ch = max(0, int(lo))
        hi_ch = min(len(self._data), int(hi))
        self.lbl_roi_range.setText(f"ch {lo_ch} – {hi_ch}")

        if hi_ch <= lo_ch or not self._data.any():
            self.lbl_roi_gross.setText("—")
            self.lbl_roi_area.setText("—")
            self.lbl_roi_centroid.setText("—")
            return

        # Use analysis engine for ROI background & net area
        method_str = self.cmb_bg.currentText().lower()
        if method_str == "trapezoid":
            method = "trapezoid"
        elif method_str == "snip":
            method = "snip"
        elif method_str == "polynomial":
            method = "polynomial"
        else:
            method = "none"
            
        roi_res = bg_module.roi_net_area(
            self._data, lo_ch, hi_ch, method=method
        )
        
        gross_area = roi_res["gross"]
        net_area = roi_res["net"]
        self.lbl_roi_gross.setText(f"{int(gross_area):,}")
        self.lbl_roi_area.setText(f"{int(net_area):,}")

        # Compute Currie limits for display in tooltip
        Lc, Ld = bg_module.currie_limits(roi_res["background"])
        self.lbl_roi_area.setToolTip(f"Critical level (L_C): {Lc:.1f}\\nDetection limit (L_D): {Ld:.1f}")

        if net_area > 0:
            centroid = roi_res["centroid_ch"]
            if self._cal_poly is not None:
                centroid_kev = self._cal_poly(centroid)
                self.lbl_roi_centroid.setText(f"{centroid_kev:.2f} keV")
            else:
                self.lbl_roi_centroid.setText(f"ch {centroid:.2f}")
        else:
            self.lbl_roi_centroid.setText("—")

    # ── Slot: mouse move → crosshair + cursor readout ────────────────────────
    def _on_mouse_move(self, pos):
        vb = self.plot.getViewBox()
        if not self.plot.sceneBoundingRect().contains(pos):
            return
        mouse_pt = vb.mapSceneToView(pos)
        ch = int(mouse_pt.x())
        self._vline.setPos(mouse_pt.x())
        self._hline.setPos(mouse_pt.y())

        if 0 <= ch < len(self._data):
            cts = int(self._data[ch])
            self.lbl_cursor.setText(f"Ch: {ch:5d}   Cts: {cts:,}")
        else:
            self.lbl_cursor.setText("Ch: —   Cts: —")

    # ── Phase 7.4: Peak search ───────────────────────────────────────────────
    def _run_peak_search(self):
        """Run Mariscotti peak search, fit local Gaussians, and draw markers."""
        if not self._data.any():
            return

        self._clear_peak_markers()
        
        min_height = self.spn_height.value() / 100.0
        min_sep    = self.spn_sep.value()

        # 1. Run Mariscotti search via analysis engine
        peaks = peak_module.find_peaks_mariscotti(
            self._data,
            min_height_fraction=min_height,
            min_separation=min_sep,
            mariscotti_sigma=3.0,
            max_fwhm_channels=60.0,
            fwhm_hint=10.0,
            cal_poly=self._cal_poly
        )

        self.peak_table.setRowCount(0)

        for p in peaks:
            # 2. Fast Gaussian fit for table population
            fit_res = fit_module.fit_gaussian(self._data, p.channel, p.fwhm_est)
            
            if fit_res.success:
                centroid_val = fit_res.centroid
                fwhm_val = fit_res.fwhm_ch
                net_area_val = f"{int(fit_res.area):,}"
            else:
                centroid_val = p.centroid
                fwhm_val = p.fwhm_est
                net_area_val = "—"

            # 3. Energy & Isotope ID
            if self._cal_poly is not None:
                energy_val = self._cal_poly(centroid_val)
                ch_energy_str = f"{centroid_val:.1f} ({energy_val:.1f} keV)"
                match = iso_module.best_match_for_peak(energy_val, library=ISOTOPE_LIBRARY)
                isotope_name = match.isotope if match else "—"
                pos_val = energy_val
                if isotope_name != "—":
                    label_text = f"{isotope_name} ({energy_val:.1f} keV)"
                else:
                    label_text = f"{energy_val:.1f} keV"
            else:
                ch_energy_str = f"{centroid_val:.1f}"
                isotope_name = "—"
                pos_val = centroid_val
                label_text = f"ch {centroid_val:.1f}"

            # 4. Draw marker
            line = pg.InfiniteLine(
                pos=pos_val, angle=90, movable=False,
                pen=pg.mkPen("#ffd700", width=1, style=Qt.DashLine),
                label=label_text,
                labelOpts={
                    "position": 0.97,
                    "color": "#ffd700",
                    "fill": pg.mkBrush(0, 0, 0, 100),
                    "movable": False,
                }
            )
            self.plot.addItem(line)
            self._peak_lines.append(line)

            # 5. Populate table
            row = self.peak_table.rowCount()
            self.peak_table.insertRow(row)
            table_vals = [
                ch_energy_str,
                f"{p.counts:,}",
                net_area_val,
                f"{fwhm_val:.1f}",
                isotope_name
            ]
            for col, val in enumerate(table_vals):
                item = QTableWidgetItem(val)
                item.setTextAlignment(Qt.AlignCenter)
                if col == 4 and isotope_name != "—":
                    item.setForeground(QColor("#00d4ff"))
                    item.setFont(QFont("Sans", 9, QFont.Bold))
                self.peak_table.setItem(row, col, item)

        n = len(peaks)
        self.lbl_npeaks.setText(
            f"{n} peak{'s' if n != 1 else ''} found"
            if n > 0 else "No peaks found"
        )


    def _clear_peak_markers(self):
        """Remove all peak marker lines, fit curves, and clear the table."""
        for line in self._peak_lines:
            self.plot.removeItem(line)
        self._peak_lines.clear()
        self._clear_fits()
        self.peak_table.setRowCount(0)
        self.lbl_npeaks.setText("")

    # ── Phase 7.5 helpers ───────────────────────────────────────────────────
    @staticmethod
    def _fit_val(label: str, color: str) -> QLabel:
        """Create a styled label for a fit result field."""
        w = QLabel(f"{label}: —")
        w.setStyleSheet(
            f"color:{color}; font-size:9pt; font-family:monospace; "
            f"background:#1a1000; border:1px solid #332200; "
            f"border-radius:3px; padding:1px 6px;"
        )
        return w

    @staticmethod
    def _gaussian_bg(x, A, mu, sigma, B):
        """
        Gaussian + flat background:
          A * exp(-0.5 * ((x - mu) / sigma)^2) + B
        """
        return A * np.exp(-0.5 * ((x - mu) / sigma) ** 2) + B

    def _on_table_click(self, row: int, _col: int):
        """Fit the selected model to the peak in the clicked table row."""
        try:
            ch_item  = self.peak_table.item(row, 0)
            fwhm_item= self.peak_table.item(row, 3)
            if ch_item is None or fwhm_item is None:
                return
            ch_center = float(ch_item.text().split()[0])
            fwhm_est  = float(fwhm_item.text().split()[0])
        except (ValueError, AttributeError, IndexError):
            return

        model_str = self.cmb_fit_model.currentText().lower()
        if model_str == "auto":
            fit_res = fit_module.auto_fit(self._data, int(ch_center), fwhm_est)
        else:
            model_map = {
                "gaussian": "gaussian",
                "gaussian+slope": "gaussian_slope",
                "hypermet": "hypermet",
                "voigt": "voigt",
                "multiplet": "multiplet"
            }
            mapped = model_map.get(model_str, "gaussian")
            if mapped == "gaussian":
                fit_res = fit_module.fit_gaussian(self._data, int(ch_center), fwhm_est)
            elif mapped == "gaussian_slope":
                fit_res = fit_module.fit_gaussian_slope(self._data, int(ch_center), fwhm_est)
            elif mapped == "hypermet":
                fit_res = fit_module.fit_hypermet(self._data, int(ch_center), fwhm_est)
            elif mapped == "voigt":
                fit_res = fit_module.fit_voigt(self._data, int(ch_center), fwhm_est)
            else:
                fit_res = fit_module.fit_gaussian(self._data, int(ch_center), fwhm_est)

        if not fit_res.success:
            self._set_fit_labels(None)
            return

        # Draw smooth fit curve
        half_win = max(int(fwhm_est * 3.5), 6)
        lo = max(0, int(ch_center) - half_win)
        hi = min(len(self._data)-1, int(ch_center) + half_win)
        x_dense = np.linspace(lo, hi, 500)
        
        # We need to recreate the model evaluated at x_dense
        if fit_res.model == "gaussian":
            y_dense = fit_module._gauss_flat(x_dense, **fit_res.params)
        elif fit_res.model == "gaussian_slope":
            y_dense = fit_module._gauss_slope(x_dense, **fit_res.params)
        elif fit_res.model == "hypermet":
            y_dense = fit_module._hypermet(x_dense, **fit_res.params)
        elif fit_res.model == "voigt":
            y_dense = fit_module._voigt_model(x_dense, **fit_res.params)
        elif fit_res.model == "bayesian":
            y_dense = fit_module._gauss_flat(x_dense, **fit_res.params)
        else:
            y_dense = np.zeros_like(x_dense)

        fit_curve = self.plot.plot(
            x_dense, y_dense,
            pen=pg.mkPen("#ff9900", width=2)
        )
        self._fit_curves.append(fit_curve)

        # Update result labels
        self._set_fit_labels((fit_res.centroid, fit_res.fwhm_ch, fit_res.area, fit_res.chi2_red))

        # Update Net Counts and FWHM columns in the table
        net_item = QTableWidgetItem(f"{int(fit_res.area):,}")
        net_item.setTextAlignment(Qt.AlignCenter)
        net_item.setForeground(QColor("#ff9900"))
        self.peak_table.setItem(row, 2, net_item)

        fwhm_item_new = QTableWidgetItem(f"{fit_res.fwhm_ch:.1f}")
        fwhm_item_new.setTextAlignment(Qt.AlignCenter)
        fwhm_item_new.setForeground(QColor("#ff9900"))
        self.peak_table.setItem(row, 3, fwhm_item_new)

    def _set_fit_labels(self, results):
        """Update or clear the fit result labels."""
        if results is None:
            self.lbl_fit_centroid.setText("Centroid: —  (fit failed)")
            self.lbl_fit_fwhm.setText("FWHM: —")
            self.lbl_fit_area.setText("Area: —")
            self.lbl_fit_chi2.setText("χ²/dof: —")
            return
        mu, fwhm, area, chi2 = results
        self.lbl_fit_centroid.setText(f"Centroid: {mu:.3f}")
        self.lbl_fit_fwhm.setText(f"FWHM: {fwhm:.2f} ch")
        self.lbl_fit_area.setText(f"Area: {area:,.0f}")
        self.lbl_fit_chi2.setText(f"χ²/dof: {chi2:.2f}")

    def _clear_fits(self):
        """Remove all Gaussian fit curves from the plot."""
        for c in self._fit_curves:
            self.plot.removeItem(c)
        self._fit_curves.clear()
        self._set_fit_labels(None)
        # Reset table columns
        for r in range(self.peak_table.rowCount()):
            net_item = QTableWidgetItem("—")
            net_item.setTextAlignment(Qt.AlignCenter)
            self.peak_table.setItem(r, 2, net_item)
            fwhm_item = QTableWidgetItem("—")
            fwhm_item.setTextAlignment(Qt.AlignCenter)
            self.peak_table.setItem(r, 3, fwhm_item)

    # ── Phase 7.6: Calibration ──────────────────────────────────────────────────
    def _open_calibration_dialog(self):
        """Open the calibration dialog (Phase 7.6)."""
        dlg = CalibrationDialog(parent=self, spectrum_panel=self)
        dlg.exec_()

    def apply_calibration(self, poly: np.poly1d, degree: int):
        """Switch x-axis to keV using the given polynomial."""
        self._cal_poly = poly
        self.plot.setLabel("bottom", "Energy (keV)")
        self.lbl_cal_active.setText(f"[cal·{degree}]")
        self._clear_peak_markers()
        self._refresh_calibrated_axis()

    def remove_calibration(self):
        """Revert x-axis to channels."""
        self._cal_poly = None
        self.plot.setLabel("bottom", "Channel")
        self.lbl_cal_active.setText("")
        self._clear_peak_markers()
        # Re-render spectrum in channel space
        self.update_spectrum(self._data)

    def _refresh_calibrated_axis(self):
        """Redraw spectrum curve with calibrated x-axis."""
        if self._cal_poly is None:
            return
        data = self._data
        nz   = int(np.max(np.nonzero(data)[0])) + 1 if data.any() else 64
        nz   = max(nz, 64)
        ch   = np.arange(nz, dtype=np.float64)
        x_kev = self._cal_poly(ch)
        self.curve.setData(x=x_kev.astype(np.float32),
                           y=data[:nz].astype(np.float32))

    def get_last_fit_centroid(self):
        """Return the most recently fitted centroid (channel), or None."""
        txt = self.lbl_fit_centroid.text()
        # Format: "Centroid: 312.447"
        try:
            return float(txt.split(":")[1].strip().split()[0])
        except (IndexError, ValueError):
            return None

    def _export_spe(self):
        if not self._data.any():
            return
        path, _ = QFileDialog.getSaveFileName(self, "Export SPE", "spectrum.spe", "SPE files (*.spe);;All files (*)")
        if path:
            exp_module.export_spe(path, self._data, live_time=1.0, real_time=1.0, cal_poly=self._cal_poly)
            QMessageBox.information(self, "Export Successful", f"SPE file saved to:\\n{path}")
            
    def _export_csv(self):
        if not self._data.any():
            return
        path, _ = QFileDialog.getSaveFileName(self, "Export ROOT-CSV", "spectrum.csv", "CSV files (*.csv);;All files (*)")
        if path:
            exp_module.export_root_csv(path, self._data, cal_poly=self._cal_poly)
            QMessageBox.information(self, "Export Successful", f"ROOT-CSV file saved to:\\n{path}")

# ---------------------------------------------------------------------------
# Phase 7.6: Calibration Dialog
# ---------------------------------------------------------------------------
class CalibrationDialog(QDialog):
    """
    Energy calibration dialog.
    Workflow:
      1. Click 'Add from fit' to pull the last Gaussian centroid.
      2. Type the corresponding energy (keV) in the Energy column.
      3. Repeat for each peak (minimum 2 for linear, 3 for quadratic).
      4. Click 'Fit Calibration' to compute the polynomial.
      5. Click 'Apply to spectrum' to switch the x-axis to keV.
      6. Optionally save the calibration to a .cal JSON file.
    """
    DARK = """
        QDialog, QWidget { background:#141414; color:#ddd; }
        QGroupBox {
            border:1px solid #252525; border-radius:6px;
            margin-top:8px; font-size:9pt; color:#555;
        }
        QGroupBox::title { subcontrol-origin:margin; left:10px; padding:0 4px; }
        QTableWidget {
            background:#0d0d0d; color:#ccc; gridline-color:#222;
            font-size:9pt; border:none;
        }
        QTableWidget::item:alternate { background:#111111; }
        QTableWidget::item:selected  { background:#1a0028; }
        QHeaderView::section {
            background:#1a1a1a; color:#888; border:1px solid #222;
            padding:2px; font-size:8pt;
        }
        QLineEdit {
            background:#1e1e1e; color:#eee; border:1px solid #333;
            border-radius:3px; padding:2px 5px;
        }
        QRadioButton { color:#aaa; }
        QLabel { color:#ccc; }
    """

    def __init__(self, parent, spectrum_panel):
        super().__init__(parent)
        self._sp   = spectrum_panel   # SpectrumPanel reference
        self._poly = None
        self.setWindowTitle("Energy Calibration")
        self.setMinimumWidth(560)
        self.setMinimumHeight(480)
        self.setStyleSheet(self.DARK)
        self._build()

    # ── Build UI ────────────────────────────────────────────────────────────────────
    def _build(self):
        root = QVBoxLayout(self)
        root.setSpacing(10)
        root.setContentsMargins(14, 14, 14, 10)

        # ── Title
        title = QLabel(
            "<b style='color:#bb88ff'>📐 Energy Calibration</b>"
            "<span style='color:#444; font-size:9pt'> — assign keV to fitted peaks</span>"
        )
        title.setFont(QFont("Sans", 11))
        root.addWidget(title)

        sep = QFrame(); sep.setFrameShape(QFrame.HLine)
        sep.setStyleSheet("color:#222;"); root.addWidget(sep)

        # ── Calibration point table
        cal_grp = QGroupBox("Calibration Points")
        cg = QVBoxLayout(cal_grp)

        self.cal_table = QTableWidget(0, 3)
        self.cal_table.setHorizontalHeaderLabels(
            ["Centroid (ch)", "Energy (keV)", "Residual (keV)"]
        )
        self.cal_table.horizontalHeader().setSectionResizeMode(QHeaderView.Stretch)
        self.cal_table.setMinimumHeight(150)
        self.cal_table.setAlternatingRowColors(True)
        self.cal_table.setSelectionBehavior(QTableWidget.SelectRows)
        cg.addWidget(self.cal_table)

        btn_row = QHBoxLayout()
        self.btn_add = self._small_btn("+ Add from fit", "#bb88ff", "#1a0d2a")
        self.btn_add.clicked.connect(self._add_from_fit)
        btn_row.addWidget(self.btn_add)

        self.btn_add_manual = self._small_btn("+ Manual entry", "#888888", "#1a1a1a")
        self.btn_add_manual.clicked.connect(self._add_manual)
        btn_row.addWidget(self.btn_add_manual)

        self.btn_remove = self._small_btn("− Remove selected", "#883333", "#220000")
        self.btn_remove.clicked.connect(self._remove_selected)
        btn_row.addWidget(self.btn_remove)
        btn_row.addStretch()
        cg.addLayout(btn_row)
        root.addWidget(cal_grp)

        # ── Fit options
        opt_row = QHBoxLayout()
        opt_row.addWidget(QLabel("Polynomial degree:"))
        self._deg_grp = QButtonGroup(self)
        self.rad_lin  = QRadioButton("Linear  (E = a·ch + b)")
        self.rad_quad = QRadioButton("Quadratic  (E = a·ch² + b·ch + c)")
        self.rad_lin.setChecked(True)
        self._deg_grp.addButton(self.rad_lin,  1)
        self._deg_grp.addButton(self.rad_quad, 2)
        opt_row.addWidget(self.rad_lin)
        opt_row.addWidget(self.rad_quad)
        opt_row.addStretch()
        root.addLayout(opt_row)

        # Fit button + equation display
        fit_row = QHBoxLayout()
        self.btn_fit = self._small_btn("📈 Fit Calibration", "#44cc88", "#002211")
        self.btn_fit.setFixedWidth(130)
        self.btn_fit.clicked.connect(self._run_fit)
        fit_row.addWidget(self.btn_fit)

        self.lbl_equation = QLabel("")
        self.lbl_equation.setStyleSheet(
            "color:#44cc88; font-size:9pt; font-family:monospace;"
        )
        fit_row.addWidget(self.lbl_equation)
        fit_row.addStretch()

        self.lbl_rms = QLabel("")
        self.lbl_rms.setStyleSheet("color:#888; font-size:9pt;")
        fit_row.addWidget(self.lbl_rms)
        root.addLayout(fit_row)

        sep2 = QFrame(); sep2.setFrameShape(QFrame.HLine)
        sep2.setStyleSheet("color:#222;"); root.addWidget(sep2)

        # ── Apply + file buttons
        bottom = QHBoxLayout()

        self.btn_apply = self._small_btn("✅ Apply to spectrum", "#44cc88", "#002211")
        self.btn_apply.setEnabled(False)
        self.btn_apply.clicked.connect(self._apply)
        bottom.addWidget(self.btn_apply)

        self.btn_remove_cal = self._small_btn("❌ Remove calibration", "#883333", "#220000")
        self.btn_remove_cal.clicked.connect(self._remove_cal)
        bottom.addWidget(self.btn_remove_cal)

        bottom.addSpacing(20)

        self.btn_save = self._small_btn("💾 Save .cal", "#5588cc", "#001122")
        self.btn_save.setEnabled(False)
        self.btn_save.clicked.connect(self._save_cal)
        bottom.addWidget(self.btn_save)

        self.btn_load = self._small_btn("📂 Load .cal", "#5588cc", "#001122")
        self.btn_load.clicked.connect(self._load_cal)
        bottom.addWidget(self.btn_load)

        bottom.addStretch()

        self.btn_close = QPushButton("Close")
        self.btn_close.clicked.connect(self.accept)
        self.btn_close.setStyleSheet(
            "QPushButton{color:#666;background:#1a1a1a;border:1px solid #333;"
            "border-radius:4px;padding:3px 14px;}"
            "QPushButton:hover{color:#aaa;}"
        )
        bottom.addWidget(self.btn_close)
        root.addLayout(bottom)

        # Pre-populate from any existing calibration points (if dialog re-opened)
        self._populate_from_existing()

    @staticmethod
    def _small_btn(text, fg, bg):
        b = QPushButton(text)
        b.setStyleSheet(
            f"QPushButton{{color:{fg};background:{bg};border:1px solid {fg};"
            f"border-radius:4px;padding:3px 10px;font-size:9pt;}}"
            f"QPushButton:hover{{background:{fg};color:#000;}}"
            f"QPushButton:disabled{{color:#333;background:#111;border-color:#222;}}"
        )
        return b

    # ── Point management ──────────────────────────────────────────────────────────────
    def _add_from_fit(self):
        """Pull the last Gaussian centroid from the spectrum panel."""
        ch = self._sp.get_last_fit_centroid()
        if ch is None:
            QMessageBox.information(
                self, "No fit",
                "Fit a peak first (Phase 7.5) then click '+ Add from fit'."
            )
            return
        self._add_row(f"{ch:.3f}", "")

    def _add_manual(self):
        """Add a blank row for manual entry."""
        self._add_row("", "")

    def _add_row(self, centroid_str: str, energy_str: str):
        r = self.cal_table.rowCount()
        self.cal_table.insertRow(r)
        for col, val in enumerate([centroid_str, energy_str, ""]):
            item = QTableWidgetItem(val)
            item.setTextAlignment(Qt.AlignCenter)
            if col == 2:  # residual — not editable
                item.setFlags(item.flags() & ~Qt.ItemIsEditable)
                item.setForeground(QColor("#666"))
            self.cal_table.setItem(r, col, item)

    def _remove_selected(self):
        rows = sorted(
            {i.row() for i in self.cal_table.selectedItems()}, reverse=True
        )
        for r in rows:
            self.cal_table.removeRow(r)

    def _populate_from_existing(self):
        """If spectrum panel already has a calibration, pre-fill points."""
        # No persistent state yet — dialog starts empty each time
        pass

    # ── Calibration fit ───────────────────────────────────────────────────────────────
    def _collect_points(self):
        """Read centroid + energy from table. Returns (channels, energies) arrays."""
        chs, engs = [], []
        for r in range(self.cal_table.rowCount()):
            try:
                ch  = float(self.cal_table.item(r, 0).text())
                eng = float(self.cal_table.item(r, 1).text())
                chs.append(ch); engs.append(eng)
            except (ValueError, AttributeError):
                pass
        return np.array(chs), np.array(engs)

    def _run_fit(self):
        ch_arr, eng_arr = self._collect_points()
        deg = 2 if self.rad_quad.isChecked() else 1
        min_pts = deg + 1

        if len(ch_arr) < min_pts:
            QMessageBox.warning(
                self, "Not enough points",
                f"{deg}° polynomial needs at least {min_pts} points."
            )
            return

        coeffs = np.polyfit(ch_arr, eng_arr, deg)
        self._poly = np.poly1d(coeffs)

        # Residuals
        fitted  = self._poly(ch_arr)
        resids  = eng_arr - fitted
        rms     = float(np.sqrt(np.mean(resids**2)))

        # Update residual column
        for r in range(self.cal_table.rowCount()):
            try:
                ch  = float(self.cal_table.item(r, 0).text())
                eng = float(self.cal_table.item(r, 1).text())
            except (ValueError, AttributeError):
                continue
            res = eng - self._poly(ch)
            item = QTableWidgetItem(f"{res:+.4f}")
            item.setTextAlignment(Qt.AlignCenter)
            item.setForeground(
                QColor("#ff4444") if abs(res) > 0.5 else QColor("#44cc88")
            )
            item.setFlags(item.flags() & ~Qt.ItemIsEditable)
            self.cal_table.setItem(r, 2, item)

        # Show equation
        if deg == 1:
            a, b = coeffs
            eq = f"E = {a:.6g}·ch + {b:.6g} keV"
        else:
            a, b, c = coeffs
            eq = f"E = {a:.4g}·ch² + {b:.6g}·ch + {c:.6g} keV"
        self.lbl_equation.setText(eq)
        self.lbl_rms.setText(f"RMS: {rms:.4f} keV")

        self.btn_apply.setEnabled(True)
        self.btn_save.setEnabled(True)

    # ── Apply / remove ─────────────────────────────────────────────────────────────────
    def _apply(self):
        if self._poly is None:
            return
        deg = 2 if self.rad_quad.isChecked() else 1
        self._sp.apply_calibration(self._poly, deg)

    def _remove_cal(self):
        self._sp.remove_calibration()

    # ── Save / load ────────────────────────────────────────────────────────────────────
    def _save_cal(self):
        if self._poly is None:
            return
        path, _ = QFileDialog.getSaveFileName(
            self, "Save Calibration", "calibration.cal",
            "Calibration files (*.cal);;All files (*)"
        )
        if not path:
            return
        ch_arr, eng_arr = self._collect_points()
        deg = 2 if self.rad_quad.isChecked() else 1
        data = {
            "degree":    deg,
            "coeffs":    list(self._poly.coeffs.tolist()),
            "centroids": ch_arr.tolist(),
            "energies":  eng_arr.tolist(),
            "equation":  self.lbl_equation.text(),
            "rms_kev":   self.lbl_rms.text(),
        }
        import json
        with open(path, "w") as f:
            json.dump(data, f, indent=2)
        QMessageBox.information(self, "Saved", f"Calibration saved to:\n{path}")

    def _load_cal(self):
        path, _ = QFileDialog.getOpenFileName(
            self, "Load Calibration", "",
            "Calibration files (*.cal);;All files (*)"
        )
        if not path:
            return
        import json
        try:
            with open(path) as f:
                data = json.load(f)
            coeffs   = np.array(data["coeffs"])
            centroids = data["centroids"]
            energies  = data["energies"]
        except Exception as e:
            QMessageBox.critical(self, "Load failed", str(e))
            return

        self._poly = np.poly1d(coeffs)
        self.cal_table.setRowCount(0)
        for ch, eng in zip(centroids, energies):
            self._add_row(f"{ch:.3f}", f"{eng}")

        deg = data.get("degree", len(coeffs) - 1)
        if deg == 2:
            self.rad_quad.setChecked(True)
        else:
            self.rad_lin.setChecked(True)

        self.lbl_equation.setText(data.get("equation", ""))
        self.lbl_rms.setText(data.get("rms_kev", ""))
        self.btn_apply.setEnabled(True)
        self.btn_save.setEnabled(True)
        QMessageBox.information(
            self, "Loaded",
            f"Calibration loaded from:\n{path}\n"
            f"Click 'Apply to spectrum' to activate."
        )


# ---------------------------------------------------------------------------
# Phase 9: Waterfall / 2D time-evolution heatmap
# ---------------------------------------------------------------------------
class WaterfallPanel(QGroupBox):
    """
    Rolling 2D spectrogram using pyqtgraph.ImageItem.

    Stores the last ``HISTORY`` spectra in a circular buffer
    (shape: HISTORY x MAX_CHANNELS).  Every call to ``push(spectrum)``
    adds one row.  The colourmap follows CERN/CMS convention: inferno
    (perceptually uniform, unambiguous for colour-blind users).

    Axes:
      X  —  channel / energy  (same calibration as SpectrumPanel)
      Y  —  time  (0 = oldest, top = newest)
    """
    HISTORY = 200   # number of spectra to keep

    def __init__(self):
        super().__init__("Waterfall (Time Evolution)")
        self._n_chan     = MAX_CHANNELS
        self._buf        = np.zeros((self.HISTORY, MAX_CHANNELS), dtype=np.float32)
        self._write_idx  = 0          # next row to overwrite
        self._cal_poly   = None       # np.poly1d, set when calibrated
        self._build()

    # ── Build ──────────────────────────────────────────────────────────────
    def _build(self):
        layout = QVBoxLayout(self)
        layout.setSpacing(4)

        # Toolbar
        toolbar = QHBoxLayout()

        toolbar.addWidget(QLabel("History rows:"))
        self.spin_history = QSpinBox()
        self.spin_history.setRange(50, 1000)
        self.spin_history.setValue(self.HISTORY)
        self.spin_history.setFixedWidth(70)
        self.spin_history.setStyleSheet(
            "background:#1e1e1e; color:#eee; border:1px solid #333;"
            "border-radius:4px; padding:2px 4px;"
        )
        self.spin_history.valueChanged.connect(self._resize_history)
        toolbar.addWidget(self.spin_history)

        toolbar.addSpacing(12)
        toolbar.addWidget(QLabel("Colour scale:"))
        self.cmb_cmap = QComboBox()
        self.cmb_cmap.setFixedWidth(100)
        self.cmb_cmap.addItems(["inferno", "viridis", "plasma", "magma"])
        self.cmb_cmap.currentTextChanged.connect(self._apply_cmap)
        toolbar.addWidget(self.cmb_cmap)

        toolbar.addSpacing(12)
        self.btn_clear = QPushButton("Clear")
        self.btn_clear.setFixedWidth(56)
        self.btn_clear.setStyleSheet(
            "QPushButton{color:#aaa;background:#1e1e1e;border:1px solid #333;"
            "border-radius:4px;padding:2px 6px;font-size:9pt;}"
            "QPushButton:hover{background:#2a2a2a;}"
        )
        self.btn_clear.clicked.connect(self._clear)
        toolbar.addWidget(self.btn_clear)
        toolbar.addStretch()
        layout.addLayout(toolbar)

        # Plot widget
        self._pw = pg.PlotWidget(background="#0a0a0a")
        self._pw.setLabel("left",   "Time step (oldest → newest)")
        self._pw.setLabel("bottom", "Channel")
        self._pw.showGrid(x=True, y=True, alpha=0.15)

        self._img = pg.ImageItem()
        self._pw.addItem(self._img)

        # Colour bar
        self._cbar = pg.ColorBarItem(
            values=(0, 1), colorMap="inferno", label="Counts"
        )
        self._cbar.setImageItem(self._img, insert_in=self._pw.getPlotItem())

        layout.addWidget(self._pw, stretch=1)

        self._apply_cmap("inferno")

    # ── Public API ──────────────────────────────────────────────────────────
    def push(self, spectrum: np.ndarray):
        """Add one spectrum row to the circular buffer and refresh the view."""
        row = spectrum[:MAX_CHANNELS].astype(np.float32)
        if len(row) < MAX_CHANNELS:
            row = np.pad(row, (0, MAX_CHANNELS - len(row)))
        self._buf[self._write_idx] = row
        self._write_idx = (self._write_idx + 1) % len(self._buf)
        self._refresh()

    def set_calibration(self, cal_poly):
        """Pass a calibrated np.poly1d so the x-axis shows keV."""
        self._cal_poly = cal_poly
        if cal_poly is not None:
            self._pw.setLabel("bottom", "Energy (keV)")
        else:
            self._pw.setLabel("bottom", "Channel")

    # ── Private ─────────────────────────────────────────────────────────────
    def _refresh(self):
        # Reorder circular buffer so newest row is at the top
        ordered = np.roll(self._buf, -self._write_idx, axis=0)
        # Log-scale for better dynamic range (Compton background is huge)
        safe = np.where(ordered > 0, ordered, 0.1)
        display = np.log10(safe).T   # transpose: shape (channels, time)
        self._img.setImage(display, autoLevels=True)

    def _resize_history(self, n):
        self.HISTORY = n
        new_buf = np.zeros((n, MAX_CHANNELS), dtype=np.float32)
        old_len = len(self._buf)
        if old_len <= n:
            new_buf[:old_len] = self._buf
        else:
            # Keep the most recent n rows
            ordered = np.roll(self._buf, -self._write_idx, axis=0)
            new_buf = ordered[-n:]
        self._buf = new_buf
        self._write_idx = 0

    def _apply_cmap(self, name):
        try:
            self._img.setColorMap(name)
            self._cbar.setColorMap(name)
        except Exception:
            pass   # old pyqtgraph versions may not support setColorMap on cbar

    def _clear(self):
        self._buf[:] = 0
        self._write_idx = 0
        self._refresh()


# ---------------------------------------------------------------------------
# Phase 8: Dead-Time Gauge widget
# ---------------------------------------------------------------------------
class DeadTimeGauge(QWidget):
    """
    Colour-coded progress bar showing DAQ dead time.

    Thresholds follow IAEA and Knoll conventions:
      0–5 %   → green  (safe)
      5–20 %  → yellow (marginal)
      20–50 % → orange (caution)
      >50 %   → red    (critical)
    """
    def __init__(self):
        super().__init__()
        layout = QHBoxLayout(self)
        layout.setContentsMargins(0, 0, 0, 0)
        layout.setSpacing(6)

        lbl = QLabel("Dead Time:")
        lbl.setStyleSheet("color:#888; font-size:9pt;")
        lbl.setFixedWidth(80)
        layout.addWidget(lbl)

        self._bar = pg.PlotWidget(background="#111")
        self._bar.setFixedHeight(22)
        self._bar.setFixedWidth(200)
        self._bar.setXRange(0, 100, padding=0)
        self._bar.setYRange(0, 1,   padding=0)
        self._bar.getPlotItem().hideAxis("left")
        self._bar.getPlotItem().hideAxis("bottom")
        self._bar.getPlotItem().setContentsMargins(0, 0, 0, 0)
        self._fill = pg.BarGraphItem(x=[50], height=[1], width=[100],
                                     brush="#1aff6e")
        self._bar.addItem(self._fill)
        layout.addWidget(self._bar)

        self.lbl_pct = QLabel("—")
        self.lbl_pct.setStyleSheet("color:#eee; font-size:9pt; font-weight:bold;")
        self.lbl_pct.setFixedWidth(52)
        layout.addWidget(self.lbl_pct)

        self._pile_toggle = QCheckBox("Pile-up corr.")
        self._pile_toggle.setStyleSheet("color:#888; font-size:8pt;")
        layout.addWidget(self._pile_toggle)

        self._shaping_us = QDoubleSpinBox()
        self._shaping_us.setRange(0.1, 100.0)
        self._shaping_us.setValue(2.0)
        self._shaping_us.setSuffix(" µs")
        self._shaping_us.setFixedWidth(72)
        self._shaping_us.setToolTip("Shaping time constant for pile-up correction")
        self._shaping_us.setStyleSheet(
            "background:#1e1e1e; color:#eee; border:1px solid #333;"
            "border-radius:3px; padding:1px 3px; font-size:8pt;"
        )
        layout.addWidget(self._shaping_us)
        layout.addStretch()

    # ── Public API ──────────────────────────────────────────────────────────
    def update_dead_time(self, bufs_acq, bufs_proc, ev_rate=None):
        """
        Compute and display dead time.

        Parameters
        ----------
        bufs_acq, bufs_proc  : int or None  — EPICS buffer counters
        ev_rate              : float or None — measured count rate (evt/s)
        """
        dt_pct = None
        if bufs_acq is not None and bufs_proc is not None and bufs_acq > 0:
            dt_pct = 100.0 * (bufs_acq - bufs_proc) / bufs_acq

        if dt_pct is None:
            self.lbl_pct.setText("—")
            self._fill.setOpts(x=[50], width=[0])
            return

        # Colour thresholds
        if dt_pct < 5:
            colour = "#1aff6e"
        elif dt_pct < 20:
            colour = "#ffcc00"
        elif dt_pct < 50:
            colour = "#ff8800"
        else:
            colour = "#ff3333"

        self._fill.setOpts(
            x=[dt_pct / 2], width=[dt_pct], brush=colour
        )
        self.lbl_pct.setText(f"{dt_pct:5.1f}%")
        self.lbl_pct.setStyleSheet(f"color:{colour}; font-size:9pt; font-weight:bold;")

        # Optionally display pile-up corrected rate
        if (self._pile_toggle.isChecked() and ev_rate is not None
                and ev_rate > 0):
            tau_s = self._shaping_us.value() * 1e-6
            corr  = ev_rate / max(1.0 - ev_rate * tau_s, 1e-6)
            self.lbl_pct.setToolTip(
                f"Pile-up corrected rate: {corr:,.0f} evt/s\n"
                f"(shaping time = {self._shaping_us.value()} µs)"
            )


# ---------------------------------------------------------------------------
# Main window — assembles all panels
# ---------------------------------------------------------------------------
class LampsDashboard(QMainWindow):

    def __init__(self, proxy: EpicsProxy,
                 sim: 'SimulatedBackend | None' = None):
        super().__init__()
        self._epics = proxy
        self._sim   = sim
        self._rn_prefilled    = False
        self._cmd_lockout_until = 0.0   # epoch seconds: ignore poller status until this time
        mode_str = "[SIM MODE]" if sim is not None else f"{proxy.prefix} @ {proxy.ioc}"
        self.setWindowTitle(f"LAMPS Dashboard  —  {mode_str}")
        self.setMinimumSize(1100, 660)
        self._build_ui()
        self._apply_dark_theme()
        self._start_timers()

    def _build_ui(self):
        central = QWidget()
        self.setCentralWidget(central)
        root = QVBoxLayout(central)
        root.setSpacing(8)
        root.setContentsMargins(12, 12, 12, 8)

        # ── Top bar ─────────────────────────────────────────────────────────
        top = QHBoxLayout()
        title = QLabel(
            f"<b>LAMPS DAQ</b>"
            f"<span style='color:#444; font-size:10pt;'>"
            f"  {self._epics.prefix} @ {self._epics.ioc}</span>"
        )
        title.setFont(QFont("Sans", 13))
        top.addWidget(title)
        top.addStretch()
        self.status_badge = StatusBadge()
        top.addWidget(self.status_badge)
        root.addLayout(top)

        sep = QFrame(); sep.setFrameShape(QFrame.HLine)
        sep.setStyleSheet("color:#222;")
        root.addWidget(sep)

        # ── Body ─────────────────────────────────────────────────────────────
        body = QHBoxLayout(); body.setSpacing(10)

        # Left column: metrics + control
        left = QVBoxLayout(); left.setSpacing(8)

        metrics_box = QGroupBox("Run Statistics")
        metrics_box.setFixedWidth(270)
        ml = QVBoxLayout(metrics_box); ml.setSpacing(3)
        self.m_run_name   = MetricRow("Run Name")
        self.m_elapsed    = MetricRow("Elapsed")
        self.m_total_ev   = MetricRow("Total Events")
        self.m_event_rate = MetricRow("Event Rate")
        self.m_bufs_acq   = MetricRow("Bufs Acquired")
        self.m_bufs_proc  = MetricRow("Bufs Processed")
        for m in [self.m_run_name, self.m_elapsed, self.m_total_ev,
                  self.m_event_rate, self.m_bufs_acq, self.m_bufs_proc]:
            ml.addWidget(m)
        # Dead-time gauge (Phase 8)
        self.dt_gauge = DeadTimeGauge()
        ml.addSpacing(6)
        ml.addWidget(self.dt_gauge)
        ml.addStretch()
        self.lbl_ts = QLabel("—")
        self.lbl_ts.setStyleSheet("color:#333; font-size:8pt;")
        ml.addWidget(self.lbl_ts)
        left.addWidget(metrics_box)

        self.ctrl = ControlPanel(self._epics, sim=self._sim)
        self.ctrl.setFixedWidth(270)
        left.addWidget(self.ctrl)
        left.addStretch()
        body.addLayout(left)

        # Right column: tab widget with Spectrum + Waterfall (Phases 7 & 9)
        self._tabs = QTabWidget()
        self._tabs.setStyleSheet(
            "QTabWidget::pane{border:1px solid #222; border-radius:4px;}"
            "QTabBar::tab{background:#1a1a1a; color:#777; padding:5px 16px;"
            "border:1px solid #222; border-bottom:none; border-radius:4px 4px 0 0;}"
            "QTabBar::tab:selected{background:#141414; color:#eee;}"
            "QTabBar::tab:hover{color:#ccc;}"
        )

        self.spec_panel = SpectrumPanel(self._epics)
        self._tabs.addTab(self.spec_panel, "Spectrum")

        self.waterfall_panel = WaterfallPanel()
        self._tabs.addTab(self.waterfall_panel, "Waterfall")

        body.addWidget(self._tabs, stretch=1)
        root.addLayout(body)

        # ── Bottom bar ───────────────────────────────────────────────────────
        bot = QHBoxLayout()
        self.lbl_conn = QLabel("● Connecting…")
        self.lbl_conn.setStyleSheet("color:#333; font-size:8pt;")
        bot.addWidget(self.lbl_conn)
        bot.addStretch()
        if self._sim is not None:
            epics_txt = "SIM MODE ● Co-60 ch512 | Cs-137 ch1024 | K-40 ch2048"
            epics_col = "#00d4ff"
        elif EPICS_AVAILABLE:
            epics_txt = "pyepics ✓"
            epics_col = "#1a8a3e"
        else:
            epics_txt = "pyepics ✗ (sim fallback)"
            epics_col = "#884400"
        lbl_ep = QLabel(epics_txt)
        lbl_ep.setStyleSheet(f"color:{epics_col}; font-size:8pt;")
        bot.addWidget(lbl_ep)
        root.addLayout(bot)

    def _apply_dark_theme(self):
        self.setStyleSheet("""
            QMainWindow, QWidget { background:#141414; color:#ddd; }
            QGroupBox {
                border:1px solid #252525; border-radius:6px;
                margin-top:8px; font-size:9pt; color:#555;
            }
            QGroupBox::title { subcontrol-origin:margin; left:10px; padding:0 4px; }
            QComboBox {
                background:#1e1e1e; border:1px solid #333;
                color:#ccc; border-radius:4px; padding:2px 6px;
            }
            QComboBox QAbstractItemView {
                background:#1e1e1e; color:#ccc;
                selection-background-color:#003344;
            }
            QCheckBox { color:#888; }
            QCheckBox::indicator { width:14px; height:14px; }
        """)

    def _start_timers(self):
        """Start the background polling thread (EPICS or sim) and wire signals."""
        self._poller = EpicsPoller(self._epics, sim=self._sim, parent=self)
        self._poller.metrics_ready.connect(self._refresh_metrics)
        self._poller.spectrum_ready.connect(self._refresh_spectrum)
        self._poller.start()
        # Optimistic UI update: react to button presses immediately
        self.ctrl.command_issued.connect(self._on_command_issued)

    def closeEvent(self, event):
        """Stop the poller thread cleanly before the window closes."""
        self._poller.stop_polling()
        self._poller.wait(2000)
        super().closeEvent(event)

    # ── Optimistic UI update — fired IMMEDIATELY on button click ────────────
    def _on_command_issued(self, cmd: str):
        """
        Called the instant the user presses START / STOP / RESET.
        Sets a 2.5 s lockout so the poller cannot revert the badge
        while the IOC is still processing the command.
        """
        LOCKOUT_S = ControlPanel.LOCKOUT_MS / 1000.0
        self._cmd_lockout_until = time.monotonic() + LOCKOUT_S

        if cmd == "STOP":
            self.status_badge.set_status("STOPPED")
            self.ctrl.update_from_status("STOPPED")
            self.m_event_rate.set("(stopped)")

        elif cmd == "RESET":
            self.status_badge.set_status("STOPPED")
            self.ctrl.update_from_status("STOPPED")
            self.spec_panel.update_spectrum(np.zeros(MAX_CHANNELS, dtype=np.float32))
            self.m_total_ev.set("—")
            self.m_event_rate.set("—")
            self.m_elapsed.set("—")
            self.m_bufs_acq.set("—")
            self.m_bufs_proc.set("—")

        elif cmd == "START":
            rn = self.ctrl.run_name_edit.text().strip()
            self.status_badge.set_status("RUNNING")
            self.ctrl.update_from_status("RUNNING")
            self.m_run_name.set(rn or "—")

    # ── Refresh slots — called via pyqtSignal from EpicsPoller (GUI-thread safe)
    def _refresh_metrics(self, data: dict):
        status    = data.get('status',   '--')
        run_name  = data.get('run_name', '')
        elapsed   = data.get('elapsed')
        total_ev  = data.get('total_ev')
        ev_rate   = data.get('ev_rate')
        bufs_acq  = data.get('bufs_acq')
        bufs_proc = data.get('bufs_proc')

        # ---- Lockout: skip badge/button updates for LOCKOUT_MS after any command ----
        # This prevents the poller reading a stale EPICS status and reverting
        # the optimistic UI update before the IOC has acknowledged the command.
        locked = time.monotonic() < self._cmd_lockout_until
        if not locked:
            self.status_badge.set_status(status)
            self.ctrl.update_from_status(status)

        if not self._rn_prefilled:
            rn = data.get('cmd_rn', '')
            if rn:
                self.ctrl.prefill_run_name(rn)
                self._rn_prefilled = True

        self.m_run_name.set(run_name or "—")
        if elapsed is not None:
            h, m, s = int(elapsed)//3600, int(elapsed)%3600//60, int(elapsed)%60
            self.m_elapsed.set(f"{h:02d}:{m:02d}:{s:02d}")
        else:
            self.m_elapsed.set("—")
        self.m_total_ev.set(f"{int(total_ev):,}"       if total_ev  is not None else "—")
        self.m_event_rate.set(f"{ev_rate:,.1f} evt/s"  if ev_rate   is not None else "—")
        self.m_bufs_acq.set(str(int(bufs_acq))         if bufs_acq  is not None else "—")
        self.m_bufs_proc.set(str(int(bufs_proc))       if bufs_proc is not None else "—")

        # Phase 8: update dead-time gauge
        self.dt_gauge.update_dead_time(bufs_acq, bufs_proc, ev_rate=ev_rate)

        self.lbl_ts.setText(f"Updated: {time.strftime('%H:%M:%S')}")
        is_run = (status == "RUNNING")
        if self._sim is not None:
            self.lbl_conn.setText(
                f"● SIM  run={data.get('run_name','') or 'none'}"
            )
            self.lbl_conn.setStyleSheet(
                f"color:{'#1aff6e' if is_run else '#00d4ff'}; font-size:8pt;"
            )
        else:
            self.lbl_conn.setText(
                f"● {self._epics.prefix} @ {self._epics.ioc}"
                if EPICS_AVAILABLE else "● SIM (EPICS unavailable)"
            )
            self.lbl_conn.setStyleSheet(
                f"color:{'#1aff6e' if is_run else '#555'}; font-size:8pt;"
            )

    def _refresh_spectrum(self, data):
        # Keep poller's detector in sync with the combo box selection
        self._poller.set_detector(self.spec_panel.current_detector)
        self.spec_panel.update_spectrum(data)
        # Phase 9: feed waterfall — push only when tab is visible for performance
        self.waterfall_panel.push(data)
        # Sync calibration to waterfall
        self.waterfall_panel.set_calibration(self.spec_panel._cal_poly)


# ---------------------------------------------------------------------------
# Entry point
# ---------------------------------------------------------------------------
def main():
    parser = argparse.ArgumentParser(
        description="LAMPS Dashboard (Phase 7.1–7.6)",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=(
            "Examples:\n"
            "  python3 lamps_dashboard.py --sim          # simulated data, always works\n"
            "  python3 lamps_dashboard.py                # auto-detect EPICS; fall back to sim\n"
            "  python3 lamps_dashboard.py --ioc 192.168.1.10  # explicit IOC address\n"
        )
    )
    parser.add_argument("--prefix", default=DEFAULT_PREFIX,
                        help="EPICS PV prefix (default: LAMPS)")
    parser.add_argument("--ioc",    default=DEFAULT_IOC,
                        help="EPICS IOC address (default: localhost)")
    parser.add_argument("--sim",    action="store_true",
                        help="Force simulated data mode (skip EPICS)")
    args = parser.parse_args()

    app = QApplication(sys.argv)
    app.setApplicationName("LAMPS Dashboard")
    app.setStyle("Fusion")

    proxy = EpicsProxy(prefix=args.prefix, ioc=args.ioc)

    # ── Decide whether to use sim backend ─────────────────────────────────────
    sim = None
    if args.sim:
        print("[INFO] --sim flag: using SimulatedBackend", flush=True)
        sim = SimulatedBackend()
    elif not EPICS_AVAILABLE:
        print("[INFO] pyepics unavailable — using SimulatedBackend", flush=True)
        sim = SimulatedBackend()
    else:
        # Auto-detect: probe the IOC for 1 s before falling back to sim
        print("[INFO] Probing EPICS IOC at", args.ioc, "...", flush=True)
        if not proxy.probe_connection(timeout=1.0):
            print(
                "[INFO] IOC not reachable — falling back to SimulatedBackend.\n"
                "       Start softIoc + lamps_epics_bridge for live data.",
                flush=True
            )
            sim = SimulatedBackend()
        else:
            print("[INFO] EPICS IOC connected.", flush=True)

    window = LampsDashboard(proxy, sim=sim)
    window.show()
    sys.exit(app.exec_())


if __name__ == "__main__":
    main()
