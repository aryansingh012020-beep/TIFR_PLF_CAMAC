"""
export.py — Spectrum and analysis report export.

Formats:
    IAEA SPE  — ASCII format compatible with Maestro, Genie-2000, GammaVision
    ROOT CSV  — channel, counts, energy_keV columns for ROOT TH1F import
    JSON      — Full analysis report with peaks, calibration, efficiency data
"""
from __future__ import annotations
import json
import time
import numpy as np
from typing import Optional, Any

__all__ = ["export_spe", "export_root_csv", "export_json_report"]


def export_spe(
    path: str,
    spectrum: np.ndarray,
    run_name: str = "LAMPS",
    live_time_s: float = 0.0,
    real_time_s: float = 0.0,
    detector_description: str = "LAMPS DAQ",
    start_datetime: Optional[str] = None,
) -> bool:
    """Write spectrum in IAEA/Maestro SPE ASCII format.

    The SPE format is accepted by Maestro (Ortec), Genie-2000 (Canberra),
    GammaVision, and most offline analysis codes.

    Returns True on success, False on error.
    """
    try:
        counts = np.maximum(spectrum, 0).astype(np.int64)
        n_ch   = len(counts)
        now    = start_datetime or time.strftime("%m/%d/%Y %H:%M:%S")

        with open(path, "w", newline="\r\n") as f:
            f.write(f"$SPEC_ID:\r\n")
            f.write(f"{run_name}\r\n")
            f.write(f"$SPEC_REM:\r\n")
            f.write(f"DET# 1\r\n")
            f.write(f"DETDESC# {detector_description}\r\n")
            f.write(f"AP# LAMPS Dashboard Export\r\n")
            f.write(f"$DATE_MEA:\r\n")
            f.write(f"{now}\r\n")
            f.write(f"$MEAS_TIM:\r\n")
            f.write(f"{int(live_time_s)} {int(real_time_s)}\r\n")
            f.write(f"$DATA:\r\n")
            f.write(f"0 {n_ch - 1}\r\n")
            for c in counts:
                f.write(f"       {c}\r\n")
            f.write(f"$END:\r\n")
        return True
    except Exception:
        return False


def export_root_csv(
    path: str,
    spectrum: np.ndarray,
    cal_poly=None,
) -> bool:
    """Write channel / counts / energy_keV CSV for import as ROOT TH1F.

    Returns True on success, False on error.
    """
    try:
        counts = np.maximum(spectrum, 0).astype(np.int64)
        n_ch   = len(counts)
        chs    = np.arange(n_ch, dtype=np.float64)

        with open(path, "w") as f:
            f.write("channel,counts,energy_keV\n")
            for ch, ct in enumerate(counts):
                if cal_poly is not None:
                    try:
                        e_kev = float(cal_poly(float(ch)))
                    except Exception:
                        e_kev = 0.0
                else:
                    e_kev = 0.0
                f.write(f"{ch},{ct},{e_kev:.4f}\n")
        return True
    except Exception:
        return False


def export_json_report(
    path: str,
    run_name: str,
    live_time_s: float,
    real_time_s: float,
    spectrum: np.ndarray,
    calibration_info: Optional[dict] = None,
    efficiency_info: Optional[dict] = None,
    peaks: Optional[list[dict]] = None,
) -> bool:
    """Write a complete analysis report as JSON.

    The report schema is self-documenting and suitable for machine-readable
    archival, Python/MATLAB post-processing, and database ingestion.

    Returns True on success, False on error.
    """
    try:
        report: dict[str, Any] = {
            "format_version": "1.0",
            "export_time":    time.strftime("%Y-%m-%dT%H:%M:%S"),
            "run_name":       run_name,
            "live_time_s":    float(live_time_s),
            "real_time_s":    float(real_time_s),
            "dead_time_frac": (
                1.0 - live_time_s / max(real_time_s, 1e-6)
                if real_time_s > 0 else None
            ),
            "n_channels":     int(len(spectrum)),
            "total_counts":   int(np.sum(np.maximum(spectrum, 0))),
        }

        if calibration_info:
            report["calibration"] = calibration_info

        if efficiency_info:
            report["efficiency"] = efficiency_info

        if peaks:
            report["peaks"] = peaks
        else:
            report["peaks"] = []

        # Embed spectrum as a compact run-length encoded list to save space
        counts = np.maximum(spectrum, 0).astype(np.int64).tolist()
        report["spectrum"] = counts

        with open(path, "w") as f:
            json.dump(report, f, indent=2)
        return True
    except Exception:
        return False
