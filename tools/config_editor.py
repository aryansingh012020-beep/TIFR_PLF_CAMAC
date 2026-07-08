import os
import copy
from typing import Dict, Any, List
from PyQt5.QtWidgets import (
    QWidget, QVBoxLayout, QHBoxLayout, QLabel, QPushButton, 
    QTableWidget, QTableWidgetItem, QComboBox, QHeaderView, 
    QMessageBox, QGroupBox, QSpinBox, QCheckBox
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
        
        # Station settings (46 stations possible: 2 crates * 23 slots)
        # 1-indexed to match LAMPS convention, store as dict of dicts
        self.stations: Dict[int, Dict[str, Any]] = {}
        for i in range(1, 47):
            self.stations[i] = {
                "module": 0, "lam": 0, "mode": 5, "lld": 50, "f": 0, "gain": 8192,
                "params": "<>", "paranames": "<>", "zsup_lld": 1, "zsup_uld": 8190
            }
            
    def load(self):
        """Parse the legacy .lamps_set file if it exists."""
        if not os.path.exists(self.filepath):
            print(f"[WARN] Config file {self.filepath} not found. Using defaults.")
            return False
            
        with open(self.filepath, "r") as f:
            self.raw_lines = f.readlines()
            
        # Parse the raw lines to extract key values
        for i, line in enumerate(self.raw_lines):
            line_str = line.strip('\n')
            if len(line_str) <= 41 or line_str[40] != '=':
                continue
                
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
            elif label == "Parameters, SubAddresses":
                # Look backwards for the station number... slightly hacky but works for this strict file
                for k in range(i-1, max(0, i-5), -1):
                    if "Stn.No." in self.raw_lines[k]:
                        try:
                            stn_idx = int(self.raw_lines[k].strip('\n')[:40].replace("Stn.No.", "").split()[0])
                            self.stations[stn_idx]["params"] = value_str
                        except Exception:
                            pass
                        break
            elif label == "ParaNames, ZSupLLD, ZSupULD":
                for k in range(i-2, max(0, i-5), -1):
                    if "Stn.No." in self.raw_lines[k]:
                        try:
                            stn_idx = int(self.raw_lines[k].strip('\n')[:40].replace("Stn.No.", "").split()[0])
                            # e.g. value_str = "<Para01-04> 1 8190"
                            if ">" in value_str:
                                p_end = value_str.rfind(">") + 1
                                self.stations[stn_idx]["paranames"] = value_str[:p_end]
                                z_vals = value_str[p_end:].split()
                                if len(z_vals) >= 2:
                                    self.stations[stn_idx]["zsup_lld"] = int(z_vals[0])
                                    self.stations[stn_idx]["zsup_uld"] = int(z_vals[1])
                        except Exception:
                            pass
                        break
                        
        self.parsed = True
        return True

    def save(self, filepath=None):
        """Write the modified data back into the raw lines, preserving format perfectly."""
        save_path = filepath or self.filepath
        
        # We process the file line by line, replacing values if they match our tracked keys.
        # This preserves all gates, spectra limits, and other things we aren't changing.
        out_lines = []
        i = 0
        current_stn = -1
        
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
                        current_stn = int(parts[0])
                        stn = self.stations[current_stn]
                        val_str = f"{stn['module']} {stn['lam']} {stn['mode']} {stn['lld']} {stn['f']} {stn['gain']}"
                        out_lines.append(f"{label_padded}={val_str}\n")
                    except Exception:
                        out_lines.append(line)
                elif label == "Parameters, SubAddresses" and current_stn != -1:
                    stn = self.stations[current_stn]
                    out_lines.append(f"{label_padded}={stn['params']}\n")
                elif label == "ParaNames, ZSupLLD, ZSupULD" and current_stn != -1:
                    stn = self.stations[current_stn]
                    val_str = f"{stn['paranames']} {stn['zsup_lld']} {stn['zsup_uld']}"
                    out_lines.append(f"{label_padded}={val_str}\n")
                    current_stn = -1 # reset
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
        
    def _build_ui(self):
        layout = QVBoxLayout(self)
        layout.setContentsMargins(10, 10, 10, 10)
        
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
        
        btn_save = QPushButton("💾 Save Setup")
        btn_save.setFixedWidth(120)
        btn_save.setStyleSheet("QPushButton { background-color: #004d00; color: white; border-radius: 4px; padding: 5px; font-weight: bold; }"
                               "QPushButton:hover { background-color: #006600; }")
        btn_save.clicked.connect(self._save_config)
        top_layout.addWidget(btn_save)
        
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

    def _save_config(self):
        if not self.loaded:
            QMessageBox.warning(self, "Error", f"Could not find or load {self.config_file} initially. Cannot save.")
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
        except Exception as e:
            QMessageBox.critical(self, "Error", f"Failed to save file: {str(e)}")
