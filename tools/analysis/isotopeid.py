"""
isotopeid.py — Isotope identification via multi-line confidence scoring.

Algorithm (inspired by Canberra GENIE-2000 and ORTEC GammaVision):
    For each candidate isotope in the library:
        score = Σᵢ I_γ(i) × G(|E_detected − E_library[i]|, σ=tolerance/2)
    where G is a Gaussian weight that falls to 0.5 at the tolerance boundary.
    Peaks that have already been matched reduce the unmatched-line penalty.

Library data source: LARA (IAEA) and NuDat 3.0 databases.
"""
from __future__ import annotations
import json
import os
import numpy as np
from dataclasses import dataclass
from typing import Optional

__all__ = ["IsotopeMatch", "identify_isotopes", "load_library", "BUILTIN_LIBRARY"]

# ---------------------------------------------------------------------------
# Built-in library (inline fallback)
# ---------------------------------------------------------------------------

BUILTIN_LIBRARY: dict[str, list[tuple[float, float]]] = {
    # name: [(energy_keV, intensity_pct), ...]
    # intensity_pct > 100 indicates cascade / annihilation summing
    "Am-241":  [(26.34, 2.4), (59.54, 35.9)],
    "Ba-133":  [(53.16, 2.2), (79.61, 2.6), (80.99, 32.9), (276.40, 7.16),
                (302.85, 18.34), (356.01, 62.05), (383.85, 8.94)],
    "Co-57":   [(122.06, 85.6), (136.47, 10.7)],
    "Co-60":   [(1173.24, 99.97), (1332.50, 99.98)],
    "Cs-137":  [(661.66, 85.1)],
    "Eu-152":  [(121.78, 28.37), (244.70, 7.55), (344.28, 26.59),
                (411.12, 2.24), (443.96, 3.12), (778.90, 12.94),
                (867.37, 4.23), (964.06, 14.63), (1085.84, 10.21),
                (1112.08, 13.64), (1408.01, 20.87)],
    "K-40":    [(1460.82, 10.66)],
    "Mn-54":   [(834.85, 99.98)],
    "Na-22":   [(511.00, 180.76), (1274.54, 99.94)],
    "Ra-226":  [(186.21, 3.28)],
    "Bi-214":  [(609.31, 45.49), (768.36, 4.89), (934.06, 3.03),
                (1120.29, 15.00), (1238.11, 5.83), (1377.67, 3.98),
                (1764.49, 15.28), (2204.21, 4.99)],
    "Tl-208":  [(277.36, 6.6), (510.77, 22.6), (583.19, 84.5),
                (860.56, 12.5), (2614.51, 99.75)],
    "Pb-212":  [(238.63, 43.3), (300.09, 3.3)],
    "Ac-228":  [(209.25, 3.9), (270.24, 3.5), (328.00, 2.95),
                (338.32, 11.27), (463.00, 4.4), (794.95, 4.35),
                (911.21, 25.8), (969.01, 15.8)],
    "Zn-65":   [(511.00, 50.6), (1115.54, 50.60)],
    "I-131":   [(80.18, 2.6), (284.30, 6.06), (364.49, 81.5),
                (637.0, 7.27), (722.91, 1.77)],
    "Tc-99m":  [(140.51, 89.07)],
    "F-18":    [(511.00, 193.46)],
    "Ga-67":   [(93.31, 37.0), (184.58, 20.4), (300.22, 16.8)],
    "In-111":  [(171.28, 90.7), (245.35, 94.1)],
}


# ---------------------------------------------------------------------------
# Result container
# ---------------------------------------------------------------------------

@dataclass
class IsotopeMatch:
    isotope: str
    score: float              # higher = better match (normalised 0–100)
    n_matched: int            # number of library lines within tolerance
    n_library: int            # total number of lines for this isotope
    matched_lines: list[tuple[float, float, float]]  # (E_lib, I_lib, E_det)


# ---------------------------------------------------------------------------
# Library loader (JSON + builtin fallback)
# ---------------------------------------------------------------------------

def load_library(json_path: Optional[str] = None) -> dict:
    """Load isotope library from a JSON file, falling back to the builtin library."""
    if json_path and os.path.isfile(json_path):
        try:
            with open(json_path, "r") as f:
                data = json.load(f)
            # Expected format: {"Co-60": [[1173.2, 99.97], [1332.5, 99.98]], ...}
            return {k: [tuple(line) for line in v] for k, v in data.items()}
        except Exception:
            pass
    return BUILTIN_LIBRARY


# ---------------------------------------------------------------------------
# Core scoring engine
# ---------------------------------------------------------------------------

def _gaussian_weight(delta_E: float, tolerance: float) -> float:
    """Gaussian weight that equals 1.0 at delta_E=0 and 0.5 at delta_E=tolerance."""
    sigma = tolerance / np.sqrt(2.0 * np.log(2.0))   # FWHM = tolerance
    return float(np.exp(-0.5 * (delta_E / sigma) ** 2))


def identify_isotopes(
    detected_energies: list[float],
    library: Optional[dict] = None,
    tolerance_kev: float = 5.0,
    top_n: int = 5,
) -> list[IsotopeMatch]:
    """Multi-line isotope identification.

    Parameters
    ----------
    detected_energies : List of fitted peak energies in keV.
    library           : Isotope library dict. Uses BUILTIN_LIBRARY if None.
    tolerance_kev     : Energy matching window (keV).
    top_n             : Return only the top_n best matches.

    Returns
    -------
    List of IsotopeMatch, sorted by score descending.
    """
    if library is None:
        library = BUILTIN_LIBRARY
    if not detected_energies:
        return []

    det_arr = np.array(detected_energies, dtype=np.float64)
    matches = []

    for isotope, lines in library.items():
        if not lines:
            continue

        total_intensity = sum(I for _, I in lines)
        if total_intensity <= 0:
            continue

        score      = 0.0
        matched    = []
        n_lib      = len(lines)

        for E_lib, I_lib in lines:
            # Find closest detected peak
            diffs = np.abs(det_arr - E_lib)
            idx   = int(np.argmin(diffs))
            delta = float(diffs[idx])

            if delta <= tolerance_kev:
                w = _gaussian_weight(delta, tolerance_kev)
                score += (I_lib / total_intensity) * w * 100.0
                matched.append((E_lib, I_lib, float(det_arr[idx])))

        if score > 0.0:
            matches.append(IsotopeMatch(
                isotope=isotope,
                score=score,
                n_matched=len(matched),
                n_library=n_lib,
                matched_lines=matched,
            ))

    matches.sort(key=lambda m: m.score, reverse=True)
    return matches[:top_n]


def best_match_for_peak(
    energy_kev: float,
    library: Optional[dict] = None,
    tolerance_kev: float = 8.0,
) -> Optional[IsotopeMatch]:
    """Return the single best isotope match for one peak energy, or None."""
    results = identify_isotopes([energy_kev], library=library,
                                tolerance_kev=tolerance_kev, top_n=1)
    return results[0] if results else None
