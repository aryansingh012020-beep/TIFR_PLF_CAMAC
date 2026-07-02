#!/usr/bin/env python3
"""
lamps_dashboard.py — LAMPS Live Monitoring & Control Dashboard

Phase 7.1: Live monitoring (status, metrics, spectrum)
Phase 7.2: Run control (START / STOP / RESET, run name entry)
Phase 7.3: Spectrum tools (ROI + area, log/lin scale, crosshair, auto-range)
Phase 7.4: Peak search (scipy.signal.find_peaks, markers, peak table)
Phase 7.5: Gaussian peak fitting (scipy.optimize.curve_fit, fit overlay, results)
Phase 7.6: Calibration (energy assignment, polynomial fit, keV axis, save/load)

Requirements:
  pip install pyqtgraph pyepics PyQt5

Usage:
  python3 lamps_dashboard.py [--prefix LAMPS] [--ioc localhost]
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
    QFileDialog, QMessageBox
)
from PyQt5.QtCore import QTimer, Qt
from PyQt5.QtGui import QFont, QColor

import pyqtgraph as pg

try:
    from scipy.signal import find_peaks
    from scipy.optimize import curve_fit
    SCIPY_AVAILABLE = True
except ImportError:
    SCIPY_AVAILABLE = False
    print("[WARN] scipy not found — peak search/fitting disabled", file=sys.stderr)

try:
    import epics
    EPICS_AVAILABLE = True
except ImportError:
    EPICS_AVAILABLE = False
    print("[WARN] pyepics not found — running in stub mode", file=sys.stderr)

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
    "--":       ("#555555", "#111111"),
}


# ---------------------------------------------------------------------------
# EPICS proxy
# ---------------------------------------------------------------------------
class EpicsProxy:
    def __init__(self, prefix: str, ioc: str):
        self.prefix = prefix
        self.ioc    = ioc
        if EPICS_AVAILABLE:
            import os
            os.environ.setdefault("EPICS_CA_ADDR_LIST", ioc)
            os.environ.setdefault("EPICS_CA_AUTO_ADDR_LIST", "NO")

    def get(self, suffix, as_string=False, default=None):
        if not EPICS_AVAILABLE:
            return default
        try:
            v = epics.caget(f"{self.prefix}:{suffix}",
                            as_string=as_string, timeout=0.1)
            return v if v is not None else default
        except Exception:
            return default

    def put(self, suffix, value) -> bool:
        if not EPICS_AVAILABLE:
            print(f"[STUB] caput {self.prefix}:{suffix} = {value}")
            return True
        try:
            return bool(epics.caput(f"{self.prefix}:{suffix}", value, timeout=2.0))
        except Exception as e:
            print(f"[WARN] caput {suffix} failed: {e}", file=sys.stderr)
            return False

    def get_waveform(self, suffix, nmax=MAX_CHANNELS):
        if not EPICS_AVAILABLE:
            return np.zeros(nmax, dtype=np.uint32)
        try:
            v = epics.caget(f"{self.prefix}:{suffix}", timeout=0.2)
            if v is None:
                return np.zeros(nmax, dtype=np.uint32)
            a = np.asarray(v, dtype=np.uint64)
            return a[:nmax] if len(a) >= nmax else np.pad(a, (0, nmax - len(a)))
        except Exception:
            return np.zeros(nmax, dtype=np.uint32)


# ---------------------------------------------------------------------------
# Shared widgets
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

    def update(self, status: str):       # noqa: A003
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
class ControlPanel(QGroupBox):
    def __init__(self, epics_proxy: EpicsProxy):
        super().__init__("Run Control")
        self._epics = epics_proxy
        self._prefilled = False
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
        self.btn_start = self._btn("▶  START",  "#00aa44", "#003318")
        self.btn_stop  = self._btn("■  STOP",   "#cc3300", "#330d00")
        self.btn_reset = self._btn("↺  RESET",  "#aa6600", "#2a1800")
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

    def _do_start(self):
        rn = self.run_name_edit.text().strip()
        if not rn:
            self._fb("⚠  Enter a run name first", "#ffcc00"); return
        self._fb("Sending START…", "#aaa")
        ok = self._epics.put("CMD:RUN_NAME", rn) and self._epics.put("CMD:START", 1)
        self._fb(f"✓ START  ({rn})" if ok else "✗ caput failed", "#1aff6e" if ok else "#ff4444")

    def _do_stop(self):
        self._fb("Sending STOP…", "#aaa")
        ok = self._epics.put("CMD:STOP", 1)
        self._fb("✓ STOP sent" if ok else "✗ caput failed", "#ffcc00" if ok else "#ff4444")

    def _do_reset(self):
        self._fb("Sending RESET…", "#aaa")
        ok = self._epics.put("CMD:RESET", 1)
        self._fb("✓ RESET sent" if ok else "✗ caput failed", "#ff8800" if ok else "#ff4444")

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

        row2.addWidget(QLabel("Area:"))
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
        self.peak_table = QTableWidget(0, 4)
        self.peak_table.setHorizontalHeaderLabels(
            ["Channel", "Counts", "Est. FWHM", "Fit FWHM"]
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
        self._data = data.astype(np.float32)
        nz = int(np.max(np.nonzero(data)[0])) + 1 if data.any() else 64
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
            self.lbl_roi_area.setText("—")
            self.lbl_roi_centroid.setText("—")
            return

        region = self._data[lo_ch:hi_ch]
        area   = int(np.sum(region))
        self.lbl_roi_area.setText(f"{area:,}")

        # Weighted centroid
        if area > 0:
            channels  = np.arange(lo_ch, hi_ch, dtype=np.float64)
            centroid  = float(np.dot(channels, region.astype(np.float64)) / area)
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
        """Run scipy.signal.find_peaks on current spectrum and draw markers."""
        if not SCIPY_AVAILABLE or not self._data.any():
            return

        self._clear_peak_markers()

        data   = self._data.astype(np.float64)
        maxval = float(data.max())
        if maxval <= 0:
            self.lbl_npeaks.setText("(no data)")
            return

        min_height = maxval * (self.spn_height.value() / 100.0)
        min_sep    = self.spn_sep.value()

        peaks, props = find_peaks(
            data,
            height=min_height,
            distance=min_sep,
            width=1          # request width info for FWHM estimate
        )

        self.peak_table.setRowCount(0)

        for i, ch in enumerate(peaks):
            cts = int(data[ch])

            # Estimate FWHM: width at half-maximum (channels)
            half_max = cts / 2.0
            # Simple half-max crossing search ±50 channels
            lo, hi = int(ch), int(ch)
            while lo > 0       and data[lo] > half_max: lo -= 1
            while hi < len(data)-1 and data[hi] > half_max: hi += 1
            fwhm = hi - lo

            # Draw vertical marker
            line = pg.InfiniteLine(
                pos=ch, angle=90, movable=False,
                pen=pg.mkPen("#ffd700", width=1, style=Qt.DashLine),
                label=f"{ch}",
                labelOpts={
                    "position": 0.97,
                    "color": "#ffd700",
                    "fill": pg.mkBrush(0, 0, 0, 100),
                    "movable": False,
                }
            )
            self.plot.addItem(line)
            self._peak_lines.append(line)

            # Add to table
            row = self.peak_table.rowCount()
            self.peak_table.insertRow(row)
            for col, val in enumerate([str(ch), f"{cts:,}", f"{fwhm} ch", "—"]):
                item = QTableWidgetItem(val)
                item.setTextAlignment(Qt.AlignCenter)
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
        """Fit the Gaussian to the peak in the clicked table row."""
        if not SCIPY_AVAILABLE:
            return
        try:
            ch_item  = self.peak_table.item(row, 0)
            cts_item = self.peak_table.item(row, 1)
            fwhm_item= self.peak_table.item(row, 2)
            if ch_item is None:
                return
            ch_center = int(ch_item.text())
            cts_peak  = int(cts_item.text().replace(",", ""))
            fwhm_est  = int(fwhm_item.text().split()[0])
        except (ValueError, AttributeError):
            return

        data = self._data.astype(np.float64)
        N    = len(data)

        # Fit window: ±2.5 × estimated FWHM, minimum ±5 channels
        half_win = max(int(fwhm_est * 2.5), 5)
        lo = max(0,   ch_center - half_win)
        hi = min(N-1, ch_center + half_win)
        if hi - lo < 5:
            self._set_fit_labels(None)
            return

        x_fit = np.arange(lo, hi + 1, dtype=np.float64)
        y_fit = data[lo:hi + 1]

        # Initial guess: (amplitude, centroid, sigma, background)
        sigma_init = max(fwhm_est / 2.355, 1.0)
        p0 = [float(cts_peak), float(ch_center), sigma_init, float(np.min(y_fit))]
        bounds = (
            [0,      lo,    0.5,  0],
            [1e9,    hi,    half_win, float(np.max(y_fit)) + 1]
        )

        try:
            popt, pcov = curve_fit(
                self._gaussian_bg, x_fit, y_fit,
                p0=p0, bounds=bounds, maxfev=5000
            )
        except (RuntimeError, ValueError):
            self._set_fit_labels(None)
            return

        A, mu, sigma, B = popt
        perr = np.sqrt(np.diag(pcov))

        fwhm_fit  = 2.355 * abs(sigma)
        area_fit  = A * abs(sigma) * np.sqrt(2 * np.pi)  # integral of Gaussian

        # Reduced chi-squared
        y_model   = self._gaussian_bg(x_fit, *popt)
        residuals = y_fit - y_model
        dof       = max(len(x_fit) - 4, 1)
        chi2_red  = float(np.sum(residuals ** 2 / np.maximum(y_fit, 1)) / dof)

        # Draw smooth fit curve
        x_dense = np.linspace(lo, hi, 500)
        y_dense = self._gaussian_bg(x_dense, *popt)
        fit_curve = self.plot.plot(
            x_dense, y_dense,
            pen=pg.mkPen("#ff9900", width=2)
        )
        self._fit_curves.append(fit_curve)

        # Update result labels
        self._set_fit_labels((mu, fwhm_fit, area_fit, chi2_red))

        # Update "Fit FWHM" column in table
        item = QTableWidgetItem(f"{fwhm_fit:.1f} ch")
        item.setTextAlignment(Qt.AlignCenter)
        item.setForeground(QColor("#ff9900"))
        self.peak_table.setItem(row, 3, item)

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
        # Reset Fit FWHM column
        for r in range(self.peak_table.rowCount()):
            item = QTableWidgetItem("—")
            item.setTextAlignment(Qt.AlignCenter)
            self.peak_table.setItem(r, 3, item)

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
        self._refresh_calibrated_axis()

    def remove_calibration(self):
        """Revert x-axis to channels."""
        self._cal_poly = None
        self.plot.setLabel("bottom", "Channel")
        self.lbl_cal_active.setText("")
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
# Main window — assembles all panels
# ---------------------------------------------------------------------------
class LampsDashboard(QMainWindow):
    def __init__(self, proxy: EpicsProxy):
        super().__init__()
        self._epics = proxy
        self._rn_prefilled = False
        self.setWindowTitle(f"LAMPS Dashboard  —  {proxy.prefix}")
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
        ml.addStretch()
        self.lbl_ts = QLabel("—")
        self.lbl_ts.setStyleSheet("color:#333; font-size:8pt;")
        ml.addWidget(self.lbl_ts)
        left.addWidget(metrics_box)

        self.ctrl = ControlPanel(self._epics)
        self.ctrl.setFixedWidth(270)
        left.addWidget(self.ctrl)
        left.addStretch()
        body.addLayout(left)

        # Right column: spectrum panel (Phase 7.1 + 7.3)
        self.spec_panel = SpectrumPanel(self._epics)
        body.addWidget(self.spec_panel, stretch=1)
        root.addLayout(body)

        # ── Bottom bar ───────────────────────────────────────────────────────
        bot = QHBoxLayout()
        self.lbl_conn = QLabel("● Connecting…")
        self.lbl_conn.setStyleSheet("color:#333; font-size:8pt;")
        bot.addWidget(self.lbl_conn)
        bot.addStretch()
        epics_txt = "pyepics ✓" if EPICS_AVAILABLE else "pyepics ✗ (stub)"
        epics_col = "#1a8a3e"  if EPICS_AVAILABLE else "#884400"
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
        self._t_metrics = QTimer(self)
        self._t_metrics.timeout.connect(self._refresh_metrics)
        self._t_metrics.start(REFRESH_MS)

        self._t_spec = QTimer(self)
        self._t_spec.timeout.connect(self._refresh_spectrum)
        self._t_spec.start(SPEC_REFRESH_MS)

    # ── Refresh ──────────────────────────────────────────────────────────────
    def _refresh_metrics(self):
        status    = (self._epics.get("STATUS",       as_string=True, default="--") or "--").strip()
        run_name  = (self._epics.get("RUN_NAME",     as_string=True, default="—")  or "—").strip()
        elapsed   = self._epics.get("ELAPSED_SEC",   default=None)
        total_ev  = self._epics.get("TOTAL_EVENTS",  default=None)
        ev_rate   = self._epics.get("EVENT_RATE",    default=None)
        bufs_acq  = self._epics.get("BUFS_ACQUIRED",  default=None)
        bufs_proc = self._epics.get("BUFS_PROCESSED", default=None)

        self.status_badge.update(status)
        self.ctrl.update_from_status(status)

        if not self._rn_prefilled:
            rn = (self._epics.get("CMD:RUN_NAME", as_string=True, default="") or "").strip()
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

        self.lbl_ts.setText(f"Updated: {time.strftime('%H:%M:%S')}")
        is_run = (status == "RUNNING")
        self.lbl_conn.setText(
            f"● {self._epics.prefix} @ {self._epics.ioc}"
            if EPICS_AVAILABLE else "● STUB MODE"
        )
        self.lbl_conn.setStyleSheet(f"color:{'#1aff6e' if is_run else '#555'}; font-size:8pt;")

    def _refresh_spectrum(self):
        suffix = f"SPEC1D:{self.spec_panel.current_detector:03d}"
        data   = self._epics.get_waveform(suffix)
        self.spec_panel.update_spectrum(data)


# ---------------------------------------------------------------------------
# Entry point
# ---------------------------------------------------------------------------
def main():
    parser = argparse.ArgumentParser(description="LAMPS Dashboard (Phase 7.1–7.6)")
    parser.add_argument("--prefix", default=DEFAULT_PREFIX)
    parser.add_argument("--ioc",    default=DEFAULT_IOC)
    args = parser.parse_args()

    app = QApplication(sys.argv)
    app.setApplicationName("LAMPS Dashboard")
    app.setStyle("Fusion")

    proxy  = EpicsProxy(prefix=args.prefix, ioc=args.ioc)
    window = LampsDashboard(proxy)
    window.show()
    sys.exit(app.exec_())


if __name__ == "__main__":
    main()
