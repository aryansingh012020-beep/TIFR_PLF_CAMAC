import os
import copy
from typing import Dict, Any, List
from PyQt5.QtWidgets import (
    QWidget, QVBoxLayout, QHBoxLayout, QLabel, QPushButton, 
    QTableWidget, QTableWidgetItem, QComboBox, QHeaderView, 
    QMessageBox, QGroupBox, QSpinBox, QCheckBox, QFileDialog, QLineEdit
)
from PyQt5.QtCore import Qt
from PyQt5.QtGui import QFont, QColor

# ---------------------------------------------------------------------------
# Module Definitions (Matches legacy setup.c exactly)
# ---------------------------------------------------------------------------
MODULE_NAMES = [
    "Empty", "Ortec 413 ADC", "Ortec 811 ADC", "LeCroy ADC/QDC", "BARC CM60", 
    "Phillips 71xx", "CAEN TDC", "Silena 4418/Q", "BiRa Bit Register", 
    "BARC CM88", "Sync. Scaler", "Unknown", "LeCroy MTDC"
]

def format_line(label: str, value: Any, pad_len: int = 40) -> str:
    """Formats a key-value pair exactly as expected by the legacy C parser."""
    val_str = str(value)
    # The equals sign must be at index 40 (0-indexed). So label is padded to 40 chars.
    return f"{label:<{pad_len}}={val_str}\n"

# ---------------------------------------------------------------------------
# Setup Parser / Writer
# ---------------------------------------------------------------------------
class LampsSetupConfig:
    def __init__(self, filepath=".lamps_set"):
        self.filepath = filepath
        self.raw_lines = []
        self.parsed = False
        
        # General Settings
        self.list_on = 0
        self.compression = 1
        self.rate = 2
        self.num_crates = 1
        self.camac_mode = 0
        
        # Twod & Pseudo Settings
        self.num_twod = 0
        self.twod_spectra = []
        self.num_pseudo = 0
        self.pseudo_params = []
        
        # Station settings (46 stations possible: 2 crates * 23 slots)
        # 1-indexed to match LAMPS convention, store as dict of dicts
        self.stations: Dict[int, Dict[str, Any]] = {}
        for i in range(1, 47):
            self.stations[i] = {
                "module": 0, "lam": 0, "mode": 5, "lld": 50, "f": 0, "gain": 8192,
                "params": "<>", "paranames": "<>", "zsup_lld": 1, "zsup_uld": 8190,
                "silena_offsets": "127 127 127 127 127 127 127 127",
                "lower_thresholds": "1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1",
                "upper_thresholds": "4094 4094 4094 4094 4094 4094 4094 4094 4094 4094 4094 4094 4094 4094 4094 4094"
            }
            
    def load(self):
        """Parse the legacy .lamps_set file if it exists."""
        if not os.path.exists(self.filepath):
            print(f"[WARN] Config file {self.filepath} not found. Using defaults.")
            return False
            
        with open(self.filepath, "r") as f:
            self.raw_lines = f.readlines()
            
        # Parse the raw lines to extract key values
        current_stn = -1
        i = 0
        while i < len(self.raw_lines):
            line = self.raw_lines[i]
            line_str = line.strip('\n')
            
            if len(line_str) > 40 and line_str[40] == '=':
                label = line_str[:40].strip()
                value_str = line_str[41:].strip()
                
                if label == "ListOn":
                    self.list_on = int(value_str)
                elif label == "Compression":
                    self.compression = int(value_str)
                elif label == "Event Rate (0-4)":
                    self.rate = int(value_str)
                elif label == "Number of Crates":
                    self.num_crates = int(value_str)
                elif label == "Camac Mode":
                    self.camac_mode = int(value_str)
                elif label.startswith("Stn.No.") and "Module,Lam,Mode" in label:
                    # e.g. "Stn.No. 10  Module,Lam,Mode,LLD,F,Gain "
                    try:
                        parts = label.replace("Stn.No.", "").split()
                        stn_idx = int(parts[0])
                        current_stn = stn_idx
                        vals = value_str.split()
                        if len(vals) >= 6:
                            self.stations[stn_idx]["module"] = int(vals[0])
                            self.stations[stn_idx]["lam"] = int(vals[1])
                            self.stations[stn_idx]["mode"] = int(vals[2])
                            self.stations[stn_idx]["lld"] = int(vals[3])
                            self.stations[stn_idx]["f"] = int(vals[4])
                            self.stations[stn_idx]["gain"] = int(vals[5])
                    except Exception as e:
                        print(f"Error parsing station at line {i}: {e}")
                elif label == "Silena Offsets" and current_stn != -1:
                    self.stations[current_stn]["silena_offsets"] = value_str
                elif label == "Lower Thresholds" and current_stn != -1:
                    self.stations[current_stn]["lower_thresholds"] = value_str
                elif label == "Upper Thresholds" and current_stn != -1:
                    self.stations[current_stn]["upper_thresholds"] = value_str
                elif label == "Parameters, SubAddresses" and current_stn != -1:
                    self.stations[current_stn]["params"] = value_str
                elif label == "ParaNames, ZSupLLD, ZSupULD" and current_stn != -1:
                    idx = value_str.rfind('>')
                    if idx != -1:
                        self.stations[current_stn]["paranames"] = value_str[:idx+1].strip()
                        zvals = value_str[idx+1:].split()
                        if len(zvals) >= 2:
                            self.stations[current_stn]["zsup_lld"] = int(zvals[0])
                            self.stations[current_stn]["zsup_uld"] = int(zvals[1])
                    current_stn = -1 # End of block
                elif label == "No. of 2d Spectra":
                    self.num_twod = int(value_str)
                elif label.startswith("Spec. No.") and "XPr,Nx,YPr,Ny" in label:
                    vals = value_str.split()
                    if len(vals) >= 7:
                        self.twod_spectra.append({
                            "xpr": int(vals[0]), "nx": int(vals[1]),
                            "ypr": int(vals[2]), "ny": int(vals[3]),
                            "xsz": int(vals[4]), "ysz": int(vals[5]),
                            "name": vals[6]
                        })
                elif label == "No. of Pseudo Parameters":
                    self.num_pseudo = int(value_str)
                elif label.startswith("Pseudo No."):
                    vals = value_str.split()
                    if len(vals) >= 6:
                        self.pseudo_params.append({
                            "mode": int(vals[0]), "p1": int(vals[1]),
                            "p2": int(vals[2]), "n1": int(vals[3]),
                            "n2": int(vals[4]), "const": float(vals[5])
                        })
                        
            elif line_str.startswith("Twod Setup"):
                pass
            elif line_str.startswith("Pseudo Setup"):
                pass

            i += 1    
        self.parsed = True
        return True

    def save(self, filepath=None):
        """Write the modified data back into the raw lines, preserving format perfectly."""
        save_path = filepath or self.filepath
        
        # We process the file line by line, replacing values if they match our tracked keys.
        # This preserves all gates, spectra limits, and other things we aren't changing.
        out_lines = []
        i = 0
        while i < len(self.raw_lines):
            line = self.raw_lines[i]
            line_str = line.strip('\n')
            
            if len(line_str) > 40 and line_str[40] == '=':
                label = line_str[:40].strip()
                label_padded = line_str[:40] # Keep exact original padding
                
                if label == "ListOn":
                    out_lines.append(f"{label_padded}={self.list_on}\n")
                elif label == "Compression":
                    out_lines.append(f"{label_padded}={self.compression}\n")
                elif label == "Event Rate (0-4)":
                    out_lines.append(f"{label_padded}={self.rate}\n")
                elif label == "Number of Crates":
                    out_lines.append(f"{label_padded}={self.num_crates}\n")
                elif label == "Camac Mode":
                    out_lines.append(f"{label_padded}={self.camac_mode}\n")
                elif label.startswith("Stn.No.") and "Module,Lam,Mode" in label:
                    parts = label.replace("Stn.No.", "").split()
                    try:
                        stn_idx = int(parts[0])
                        stn = self.stations[stn_idx]
                        val_str = f"{stn['module']} {stn['lam']} {stn['mode']} {stn['lld']} {stn['f']} {stn['gain']}"
                        out_lines.append(f"{label_padded}={val_str}\n")
                        
                        # Dynamically inject Silena/Phillips blocks
                        if stn['module'] == 7:
                            out_lines.append(f"{'Silena Offsets':<40}={stn['silena_offsets']}\n")
                        if stn['module'] in (5, 7):
                            out_lines.append(f"{'Lower Thresholds':<40}={stn['lower_thresholds']}\n")
                            out_lines.append(f"{'Upper Thresholds':<40}={stn['upper_thresholds']}\n")

                        # Consume old station lines
                        while i + 1 < len(self.raw_lines):
                            next_line = self.raw_lines[i + 1].strip('\n')
                            if len(next_line) > 40 and next_line[40] == '=':
                                next_label = next_line[:40].strip()
                                if next_label in ("Silena Offsets", "Lower Thresholds", "Upper Thresholds"):
                                    i += 1
                                elif next_label == "Parameters, SubAddresses":
                                    out_lines.append(f"{next_line[:40]}={stn['params']}\n")
                                    i += 1
                                elif next_label == "ParaNames, ZSupLLD, ZSupULD":
                                    val_str = f"{stn['paranames']} {stn['zsup_lld']} {stn['zsup_uld']}"
                                    out_lines.append(f"{next_line[:40]}={val_str}\n")
                                    i += 1
                                    break # End of block
                                else:
                                    break
                            else:
                                break
                    except Exception:
                        out_lines.append(line)
                elif label in ("Silena Offsets", "Lower Thresholds", "Upper Thresholds", "Parameters, SubAddresses", "ParaNames, ZSupLLD, ZSupULD"):
                    pass # Handled by the Stn.No. block logic
                else:
                    out_lines.append(line)
            else:
                out_lines.append(line)
                
            i += 1
            
        with open(save_path, "w") as f:
            f.writelines(out_lines)
            
        return True


# ---------------------------------------------------------------------------
# Setup Panel UI
# ---------------------------------------------------------------------------
class SetupPanel(QWidget):
    def __init__(self, config_file=".lamps_set"):
        super().__init__()
        self.config_file = config_file
        self.config = LampsSetupConfig(config_file)
        self.loaded = self.config.load()
        self._build_ui()
        self._populate()
        
    def _load_new_file(self):
        _default_dir = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "set")
        if not os.path.exists(_default_dir):
            _default_dir = "/"

        path, _ = QFileDialog.getOpenFileName(
            self, "Open LAMPS Setup File", _default_dir,
            "LAMPS Setup (*.set *.lamps_set);;All files (*)"
        )
        if not path:
            return
        self.config_file = path
        self.lbl_filepath.setText(path)
        self.config = LampsSetupConfig(path)
        self.loaded = self.config.load()
        if self.loaded:
            self._populate()
            QMessageBox.information(self, "Loaded", f"Setup loaded from:\n{path}")
        else:
            QMessageBox.critical(self, "Load Failed",
                                 f"Could not parse setup file:\n{path}\n"
                                 "Make sure LAMPS has written it at least once.")
        
    def _build_ui(self):
        layout = QVBoxLayout(self)
        layout.setContentsMargins(10, 10, 10, 10)
        
        # -- File path bar --------------------------------------------------
        file_bar = QHBoxLayout()
        file_bar.addWidget(QLabel("Setup File:"))
        self.lbl_filepath = QLineEdit(self.config_file)
        self.lbl_filepath.setReadOnly(True)
        self.lbl_filepath.setStyleSheet(
            "background:#1a1a1a; color:#aaa; border:1px solid #333;"
            "border-radius:3px; padding:2px 6px; font-size:8pt; font-family:monospace;"
        )
        file_bar.addWidget(self.lbl_filepath, stretch=1)
        btn_open = QPushButton("Load Setup File...")
        btn_open.setFixedWidth(150)
        btn_open.setStyleSheet(
            "QPushButton { background:#1a2a1a; color:#aaffaa; border:1px solid #339966;"
            "border-radius:4px; padding:4px 8px; font-weight:bold; }"
            "QPushButton:hover { background:#224422; border-color:#aaffaa; }"
        )
        btn_open.clicked.connect(self._load_new_file)
        file_bar.addWidget(btn_open)
        layout.addLayout(file_bar)
        
        # -- Status label (shown when file not yet loaded) ------------------
        self.lbl_status = QLabel()
        if not self.loaded:
            self.lbl_status.setText(
                "Warning:  No .lamps_set found at the default path. "
                "Use 'Load Setup File...' to browse to your file, "
                "or run LAMPS once to generate it."
            )
            self.lbl_status.setStyleSheet(
                "color:#ffcc44; font-size:8pt; padding:4px;"
            )
        layout.addWidget(self.lbl_status)
        
        # Top Config Box
        top_box = QGroupBox("General Configuration")
        top_box.setStyleSheet("QGroupBox { border:1px solid #333; border-radius:5px; margin-top:1ex; color:#aaa; } "
                              "QGroupBox::title { subcontrol-origin:margin; left:10px; }")
        top_layout = QHBoxLayout(top_box)
        
        self.chk_list_on = QCheckBox("List Mode Active")
        self.chk_list_on.setStyleSheet("color: #ccc;")
        top_layout.addWidget(self.chk_list_on)
        
        top_layout.addSpacing(20)
        top_layout.addWidget(QLabel("Compression:"))
        self.cmb_compression = QComboBox()
        self.cmb_compression.addItems(["Candle", "Normal", "Advanced", "Exogam"])
        top_layout.addWidget(self.cmb_compression)
        
        top_layout.addSpacing(20)
        top_layout.addWidget(QLabel("Event Rate:"))
        self.cmb_rate = QComboBox()
        self.cmb_rate.addItems([">10K", "2K-10K", "500Hz-2K", "10Hz-500Hz", "Low"])
        top_layout.addWidget(self.cmb_rate)
        
        top_layout.addSpacing(20)
        top_layout.addWidget(QLabel("Crates:"))
        self.spn_crates = QSpinBox()
        self.spn_crates.setRange(1, 2)
        top_layout.addWidget(self.spn_crates)
        
        top_layout.addStretch()
        
        btn_save = QPushButton("Save")
        btn_save.setFixedWidth(80)
        btn_save.setStyleSheet("QPushButton { background-color: #004d00; color: white; border-radius: 4px; padding: 5px; font-weight: bold; }"
                               "QPushButton:hover { background-color: #006600; }")
        btn_save.clicked.connect(self._save_config)
        top_layout.addWidget(btn_save)

        btn_save_as = QPushButton("Save As...")
        btn_save_as.setFixedWidth(100)
        btn_save_as.setStyleSheet("QPushButton { background-color: #004d4d; color: white; border-radius: 4px; padding: 5px; font-weight: bold; }"
                                  "QPushButton:hover { background-color: #006666; }")
        btn_save_as.clicked.connect(self._save_config_as)
        top_layout.addWidget(btn_save_as)
        
        layout.addWidget(top_box)
        
        # Middle Grid (Stations)
        grid_box = QGroupBox("Hardware Stations (Crate 1: 1-23, Crate 2: 24-46)")
        grid_box.setStyleSheet("QGroupBox { border:1px solid #333; border-radius:5px; margin-top:1ex; color:#aaa; } "
                               "QGroupBox::title { subcontrol-origin:margin; left:10px; }")
        grid_layout = QVBoxLayout(grid_box)
        
        self.table = QTableWidget(46, 10)
        self.table.setHorizontalHeaderLabels([
            "Stn", "Module Type", "LAM", "Mode", "LLD", "Gain", 
            "F", "Params", "ParaNames", "ZSup (LLD, ULD)"
        ])
        self.table.horizontalHeader().setSectionResizeMode(QHeaderView.ResizeToContents)
        self.table.horizontalHeader().setSectionResizeMode(1, QHeaderView.Stretch)
        self.table.horizontalHeader().setSectionResizeMode(7, QHeaderView.Stretch)
        self.table.horizontalHeader().setSectionResizeMode(8, QHeaderView.Stretch)
        
        self.table.setStyleSheet(
            "QTableWidget { background-color: #121212; color: #ddd; gridline-color: #333; border: none; }"
            "QHeaderView::section { background-color: #1a1a1a; color: #888; border: 1px solid #333; padding: 4px; }"
            "QComboBox { background-color: #1e1e1e; color: #ccc; border: 1px solid #444; }"
        )
        
        grid_layout.addWidget(self.table)
        layout.addWidget(grid_box, stretch=1)
        
    def _populate(self):
        self.chk_list_on.setChecked(bool(self.config.list_on))
        self.cmb_compression.setCurrentIndex(max(0, min(self.config.compression, 3)))
        self.cmb_rate.setCurrentIndex(max(0, min(self.config.rate, 4)))
        self.spn_crates.setValue(self.config.num_crates)
        
        for stn_idx in range(1, 47):
            row = stn_idx - 1
            stn = self.config.stations[stn_idx]
            
            # Stn label
            item_stn = QTableWidgetItem(str(stn_idx))
            item_stn.setFlags(Qt.ItemIsSelectable | Qt.ItemIsEnabled)
            item_stn.setForeground(QColor("#777"))
            self.table.setItem(row, 0, item_stn)
            
            # Module Type Combo
            cmb_mod = QComboBox()
            cmb_mod.addItems(MODULE_NAMES)
            mod_idx = max(0, min(stn["module"], len(MODULE_NAMES)-1))
            cmb_mod.setCurrentIndex(mod_idx)
            self.table.setCellWidget(row, 1, cmb_mod)
            
            # Editable fields
            self.table.setItem(row, 2, QTableWidgetItem(str(stn["lam"])))
            self.table.setItem(row, 3, QTableWidgetItem(str(stn["mode"])))
            self.table.setItem(row, 4, QTableWidgetItem(str(stn["lld"])))
            self.table.setItem(row, 5, QTableWidgetItem(str(stn["gain"])))
            self.table.setItem(row, 6, QTableWidgetItem(str(stn["f"])))
            self.table.setItem(row, 7, QTableWidgetItem(str(stn["params"])))
            self.table.setItem(row, 8, QTableWidgetItem(str(stn["paranames"])))
            self.table.setItem(row, 9, QTableWidgetItem(f"{stn['zsup_lld']} {stn['zsup_uld']}"))
            
            # Fade out empty stations
            if mod_idx == 0:
                for col in range(2, 10):
                    if self.table.item(row, col):
                        self.table.item(row, col).setForeground(QColor("#444"))

    def _save_config_as(self):
        _default_dir = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "set")
        if not os.path.exists(_default_dir):
            _default_dir = "/"
            
        path, _ = QFileDialog.getSaveFileName(
            self, "Save LAMPS Setup File", _default_dir,
            "LAMPS Setup (*.set);;All files (*)"
        )
        if not path:
            return
            
        if not path.endswith('.set') and not path.endswith('.lamps_set'):
            path += '.set'
            
        self.config_file = path
        self.lbl_filepath.setText(path)
        self.config.filepath = path
        self.loaded = True # it will be once we save
        self._save_config()

    def _save_config(self):
        if not self.loaded:
            QMessageBox.warning(self, "No File Loaded",
                                "Please use 'Load Setup File...' to open a setup file first, "
                                "or use 'Save As...' to create a new one.")
            return
            
        self.config.list_on = 1 if self.chk_list_on.isChecked() else 0
        self.config.compression = self.cmb_compression.currentIndex()
        self.config.rate = self.cmb_rate.currentIndex()
        self.config.num_crates = self.spn_crates.value()
        
        for stn_idx in range(1, 47):
            row = stn_idx - 1
            cmb_mod = self.table.cellWidget(row, 1)
            self.config.stations[stn_idx]["module"] = cmb_mod.currentIndex()
            
            try:
                self.config.stations[stn_idx]["lam"] = int(self.table.item(row, 2).text())
                self.config.stations[stn_idx]["mode"] = int(self.table.item(row, 3).text())
                self.config.stations[stn_idx]["lld"] = int(self.table.item(row, 4).text())
                self.config.stations[stn_idx]["gain"] = int(self.table.item(row, 5).text())
                self.config.stations[stn_idx]["f"] = int(self.table.item(row, 6).text())
                self.config.stations[stn_idx]["params"] = self.table.item(row, 7).text()
                self.config.stations[stn_idx]["paranames"] = self.table.item(row, 8).text()
                
                zsup = self.table.item(row, 9).text().split()
                if len(zsup) >= 2:
                    self.config.stations[stn_idx]["zsup_lld"] = int(zsup[0])
                    self.config.stations[stn_idx]["zsup_uld"] = int(zsup[1])
            except ValueError:
                QMessageBox.warning(self, "Validation Error", f"Invalid integer value in Station {stn_idx}. Aborting save.")
                return
                
        try:
            self.config.save()
            QMessageBox.information(self, "Success", f"Configuration saved to {self.config_file}.\nUpdates will take effect on next run.")
            self.lbl_status.setText("") # Clear any error warnings
        except Exception as e:
            QMessageBox.critical(self, "Error", f"Failed to save file: {str(e)}")
