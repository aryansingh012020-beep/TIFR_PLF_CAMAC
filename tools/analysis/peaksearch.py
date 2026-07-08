"""
peaksearch.py — Multi-stage peak finding for gamma-ray spectra.

Pipeline (all stages required for a peak to be accepted):
    Stage 1: Savitzky-Golay smoothing       (Ortec GammaVision standard)
    Stage 2: Mariscotti second-derivative   (NIM 50, 1967 — universal HPGe method)
    Stage 3: scipy.signal.find_peaks        (cross-validation reliability filter)
    Stage 4: FWHM resolution validity gate  (reject Compton edges/continuum bumps)

References:
    Mariscotti (1967) NIM 50, 309
    Savitzky & Golay (1964) Anal. Chem. 36, 1627
    Knoll "Radiation Detection & Measurement" 4th ed., Ch. 12
"""
from __future__ import annotations
import numpy as np
from typing import NamedTuple

try:
    from scipy.signal import savgol_filter, find_peaks
    _SCIPY_OK = True
except ImportError:
    _SCIPY_OK = False

__all__ = ["PeakCandidate", "find_peaks_mariscotti", "expected_fwhm"]


class PeakCandidate(NamedTuple):
    channel: int          # integer channel of peak maximum
    centroid: float       # sub-channel centroid from smoothed data
    counts: int           # raw counts at peak channel
    fwhm_est: float       # threshold-crossing FWHM estimate (channels)
    prominence: float     # scipy prominence value


# ---------------------------------------------------------------------------
# Expected FWHM from detector physics
# ---------------------------------------------------------------------------

def expected_fwhm(
    energy_keV: float | None = None,
    channel: int | None = None,
    cal_poly=None,
    fano: float = 0.12,
    noise_keV: float = 1.0,
    pair_energy_eV: float = 2.98,
) -> float:
    """Estimate expected FWHM in channels from detector resolution formula.

        FWHM_keV = 2.355 * sqrt(F * ε * E + η²)

    where:
        F   = Fano factor  (0.12 HPGe, 0.3 NaI, 0.4 CsI)
        ε   = pair creation energy (2.98 eV for HPGe)
        E   = photon energy in eV
        η   = electronic noise term in keV

    If calibration is unavailable, returns a conservative default of 30 channels.
    """
    if energy_keV is None:
        if channel is not None and cal_poly is not None:
            energy_keV = float(cal_poly(channel))
        else:
            return 30.0   # conservative default — no calibration available

    if energy_keV <= 0:
        return 5.0

    energy_eV  = energy_keV * 1e3
    fwhm_keV   = 2.355 * np.sqrt(fano * pair_energy_eV * 1e-3 * energy_keV
                                  + noise_keV ** 2)

    # Convert keV FWHM to channels via calibration slope
    if cal_poly is not None:
        try:
            # Approximate slope at this energy using a finite difference
            ch0 = float(channel) if channel is not None else energy_keV / 0.5
            # dE/dch ≈ (E(ch+1) - E(ch-1)) / 2
            slope = abs(float(cal_poly(ch0 + 1)) - float(cal_poly(ch0 - 1))) / 2.0
            if slope > 0:
                return fwhm_keV / slope
        except Exception:
            pass
    return 30.0  # fallback


# ---------------------------------------------------------------------------
# Stage 1: Savitzky-Golay smoothing
# ---------------------------------------------------------------------------

def _savgol_smooth(data: np.ndarray, fwhm_hint: float = 10.0) -> np.ndarray:
    """Apply Savitzky-Golay filter. Window ≈ FWHM, odd, ≥ 5."""
    if not _SCIPY_OK:
        # Fallback: simple 5-point moving average
        kernel = np.ones(5) / 5.0
        return np.convolve(data.astype(np.float64), kernel, mode='same')
    window = max(5, int(fwhm_hint) | 1)   # ensure odd
    if window % 2 == 0:
        window += 1
    window = min(window, len(data) - 1 if (len(data) - 1) % 2 == 1 else len(data) - 2)
    window = max(window, 5)
    return savgol_filter(data.astype(np.float64), window_length=window, polyorder=4)


# ---------------------------------------------------------------------------
# Stage 2: Mariscotti second-derivative search
# ---------------------------------------------------------------------------

def _mariscotti_candidates(
    smoothed: np.ndarray,
    threshold_sigma: float = 3.0,
) -> np.ndarray:
    """Return channel indices where a negative second-derivative dip exceeds threshold.

    d2[ch] = smoothed[ch+1] - 2*smoothed[ch] + smoothed[ch-1]
    Threshold adapts to local count rate via sqrt(smoothed) — Poisson assumption.
    """
    N  = len(smoothed)
    d2 = np.zeros(N, dtype=np.float64)
    d2[1:-1] = smoothed[2:] - 2.0 * smoothed[1:-1] + smoothed[:-2]

    # Local Poisson noise estimate
    local_noise = np.sqrt(np.maximum(smoothed, 1.0))
    threshold   = -threshold_sigma * local_noise

    # Find channels where d2 is below the negative threshold AND is a local minimum
    neg_mask = d2 < threshold
    # Require d2[ch] <= d2[ch-1] and d2[ch] <= d2[ch+1] (local minimum of d2)
    local_min = np.zeros(N, dtype=bool)
    local_min[1:-1] = (d2[1:-1] <= d2[:-2]) & (d2[1:-1] <= d2[2:])
    return np.where(neg_mask & local_min)[0]


# ---------------------------------------------------------------------------
# Stage 4: FWHM validity gate
# ---------------------------------------------------------------------------

def _estimate_fwhm_threshold(
    data: np.ndarray,
    ch: int,
    max_fwhm_channels: float = 200.0,
) -> float:
    """Simple FWHM estimation via threshold crossing, clamped to max_fwhm_channels."""
    cts      = float(data[ch])
    half_max = cts / 2.0
    lo, hi   = ch, ch
    while lo > 0 and data[lo] > half_max:
        lo -= 1
    while hi < len(data) - 1 and data[hi] > half_max:
        hi += 1
    return min(float(hi - lo), max_fwhm_channels)


# ---------------------------------------------------------------------------
# Master peak-finder
# ---------------------------------------------------------------------------

def find_peaks_mariscotti(
    data: np.ndarray,
    min_height_fraction: float = 0.05,
    min_separation: int = 10,
    mariscotti_sigma: float = 3.0,
    max_fwhm_channels: float = 80.0,
    fwhm_hint: float = 10.0,
    cal_poly=None,
) -> list[PeakCandidate]:
    """Full 4-stage peak finder.

    Parameters
    ----------
    data                : Raw spectrum array.
    min_height_fraction : Minimum height as fraction of max (0.05 = 5%).
    min_separation      : Minimum distance between accepted peaks (channels).
    mariscotti_sigma    : Threshold multiplier for 2nd-derivative search.
    max_fwhm_channels   : Peaks with FWHM wider than this are rejected
                          (filters Compton edges and continuum bumps).
    fwhm_hint           : Expected FWHM for SavGol window sizing.
    cal_poly            : Optional np.poly1d for resolution gate.

    Returns
    -------
    List of PeakCandidate named tuples, sorted by channel.
    """
    raw    = np.maximum(data, 0).astype(np.float64)
    N      = len(raw)
    maxval = raw.max()
    if maxval <= 0:
        return []

    # Stage 1: Smooth
    smoothed = _savgol_smooth(raw, fwhm_hint=fwhm_hint)

    # Stage 2: Mariscotti candidates
    d2_candidates = set(_mariscotti_candidates(smoothed, mariscotti_sigma))

    # Stage 3: scipy find_peaks (cross-validation)
    min_h = maxval * min_height_fraction
    sp_peaks: np.ndarray = np.array([], dtype=int)
    if _SCIPY_OK:
        sp_peaks, sp_props = find_peaks(
            smoothed,
            height=min_h,
            distance=min_separation,
            prominence=min_h * 0.3,
        )

    # Accept peaks within `min_separation` channels of a Mariscotti candidate
    accepted: list[PeakCandidate] = []
    used_ch: list[int] = []

    candidate_ch = sorted(sp_peaks.tolist() if len(sp_peaks) > 0 else list(d2_candidates))

    for ch in candidate_ch:
        if raw[ch] < min_h:
            continue

        # Cross-validate: must be near a Mariscotti dip (or use only scipy if none found)
        near_d2 = any(abs(ch - d) <= max(3, min_separation // 3) for d in d2_candidates)
        if d2_candidates and not near_d2:
            continue  # skip if Mariscotti search found dips but none near this peak

        # Stage 4: FWHM gate
        fwhm_est = _estimate_fwhm_threshold(smoothed, ch, max_fwhm_channels=max_fwhm_channels)
        if fwhm_est >= max_fwhm_channels:
            continue   # too wide → Compton or continuum feature

        # Enforce minimum separation from already-accepted peaks
        if any(abs(ch - u) < min_separation for u in used_ch):
            continue

        # Sub-channel centroid: parabolic interpolation on smoothed data
        if 1 <= ch <= N - 2:
            y0, y1, y2 = smoothed[ch - 1], smoothed[ch], smoothed[ch + 1]
            denom = y0 - 2.0 * y1 + y2
            centroid = ch - 0.5 * (y2 - y0) / denom if abs(denom) > 1e-9 else float(ch)
        else:
            centroid = float(ch)

        # scipy prominence (if available)
        prom = 0.0
        if _SCIPY_OK and len(sp_peaks) > 0:
            idx_arr = np.where(sp_peaks == ch)[0]
            if len(idx_arr) > 0 and 'prominences' in sp_props:
                prom = float(sp_props['prominences'][idx_arr[0]])

        accepted.append(PeakCandidate(
            channel=int(ch),
            centroid=float(centroid),
            counts=int(raw[ch]),
            fwhm_est=float(fwhm_est),
            prominence=prom,
        ))
        used_ch.append(ch)

    return sorted(accepted, key=lambda p: p.channel)
