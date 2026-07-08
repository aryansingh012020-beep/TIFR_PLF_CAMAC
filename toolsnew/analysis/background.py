"""
background.py — Background estimation methods for gamma-ray spectra.

Algorithms:
    SNIP   : Statistics-sensitive Non-linear Iterative Peak-clipping
             Morháč et al., NIM A 401 (1997) — as in CERN ROOT TSpectrum
    TRAP   : Trapezoidal interpolation (IAEA / Canberra Genie-2000 standard)
    POLY   : Local polynomial baseline (used at ORNL, IAEA Safeguards)

All functions accept and return plain NumPy float64 arrays.
"""
from __future__ import annotations
import numpy as np
from typing import Literal

__all__ = ["snip", "trapezoidal_roi", "polynomial_roi", "roi_net_area"]

# ---------------------------------------------------------------------------
# SNIP Background (full spectrum)
# ---------------------------------------------------------------------------

def snip(
    data: np.ndarray,
    fwhm_ch: float = 10.0,
    n_iterations: int = 20,
    decreasing: bool = True,
) -> np.ndarray:
    """Estimate the continuous background across the entire spectrum using SNIP.

    Parameters
    ----------
    data         : 1-D array of raw counts (float64 or uint32).
    fwhm_ch      : Expected peak FWHM in channels — sets the clipping window.
    n_iterations : Number of clipping passes (10–20 recommended).
    decreasing   : If True use the two-stage increasing-then-decreasing window
                   variant recommended by Morháč for better separation.

    Returns
    -------
    bg : 1-D float64 array, same length as data, representing the background.
    """
    y = np.maximum(data, 0).astype(np.float64)
    N = len(y)

    # Transform: w = log(log(sqrt(y+1)+1)+1)  → makes Poisson ≈ Gaussian
    w = np.log(np.log(np.sqrt(y + 1.0) + 1.0) + 1.0)
    w_orig = w.copy()

    # Half-window sequence: 1, 2, … p  then p-1, … 1  (if decreasing)
    p = max(1, int(fwhm_ch / 2.0))
    windows = list(range(1, p + 1))
    if decreasing:
        windows = windows + list(range(p - 1, 0, -1))
    windows = windows * max(1, n_iterations // max(1, len(windows)))

    for half_w in windows:
        w_new = w.copy()
        for ch in range(half_w, N - half_w):
            v = (w[ch - half_w] + w[ch + half_w]) * 0.5
            w_new[ch] = min(w[ch], v)
        w = w_new

    # Inverse transform
    bg = (np.exp(np.exp(w) - 1.0) - 1.0) ** 2 - 1.0
    bg = np.maximum(bg, 0.0)
    return bg


# ---------------------------------------------------------------------------
# ROI-scoped methods  (operate only within [lo_ch, hi_ch])
# ---------------------------------------------------------------------------

def trapezoidal_roi(
    data: np.ndarray,
    lo_ch: int,
    hi_ch: int,
) -> np.ndarray:
    """Return a per-channel background array for the ROI using linear interpolation.

    The background at each channel is the straight-line value between
    data[lo_ch] and data[hi_ch-1], i.e. the trapezoidal approximation
    standard in Canberra Genie-2000 and IAEA measurement guides.

    Returns
    -------
    bg_roi : 1-D float64 array of length (hi_ch - lo_ch).
    """
    n = hi_ch - lo_ch
    if n <= 0:
        return np.zeros(0, dtype=np.float64)
    y_lo = float(data[lo_ch])
    y_hi = float(data[min(hi_ch - 1, len(data) - 1)])
    return np.linspace(y_lo, y_hi, n)


def polynomial_roi(
    data: np.ndarray,
    lo_ch: int,
    hi_ch: int,
    degree: int = 1,
    side_width: int = 5,
) -> np.ndarray:
    """Fit a polynomial to the spectrum flanks on either side of the ROI.

    Samples `side_width` channels on each side of [lo_ch, hi_ch], fits a
    polynomial of the given degree, and evaluates it across the ROI.
    Used at ORNL and Livermore for peaks on curved Compton plateaux.

    Returns
    -------
    bg_roi : 1-D float64 array of length (hi_ch - lo_ch).
    """
    N = len(data)
    n = hi_ch - lo_ch
    if n <= 0:
        return np.zeros(0, dtype=np.float64)

    # Flanking samples
    lo_range = range(max(0, lo_ch - side_width), lo_ch)
    hi_range = range(hi_ch, min(N, hi_ch + side_width))
    ch_side  = np.array(list(lo_range) + list(hi_range), dtype=np.float64)
    y_side   = data[ch_side.astype(int)].astype(np.float64)

    if len(ch_side) < degree + 1:
        # Fall back to trapezoidal if not enough flanking points
        return trapezoidal_roi(data, lo_ch, hi_ch)

    coeffs = np.polyfit(ch_side, y_side, degree)
    x_roi  = np.arange(lo_ch, hi_ch, dtype=np.float64)
    return np.polyval(coeffs, x_roi)


# ---------------------------------------------------------------------------
# Unified ROI net-area calculation (IAEA formula with Poisson uncertainty)
# ---------------------------------------------------------------------------

def roi_net_area(
    data: np.ndarray,
    lo_ch: int,
    hi_ch: int,
    method: Literal["trapezoid", "polynomial", "snip", "none"] = "trapezoid",
    snip_bg: np.ndarray | None = None,
    poly_degree: int = 1,
) -> dict:
    """Compute gross area, background area, net area, and Poisson uncertainty.

    Standard formula (IAEA-TECDOC-1401):
        N_gross = Σ y[lo:hi]
        N_bg    = Σ bg[lo:hi]          (from chosen method)
        N_net   = N_gross - N_bg
        σ_net   = √(N_gross + N_bg)    (Poisson propagation)
        σ_net_ext = √(N_gross + N_bg · (1 + n_roi/n_bg_samp))

    Returns
    -------
    dict with keys: gross, background, net, sigma_net, centroid_ch, n_ch
    """
    lo_ch = max(0, lo_ch)
    hi_ch = min(len(data), hi_ch)
    n_ch  = hi_ch - lo_ch

    if n_ch <= 0:
        return dict(gross=0.0, background=0.0, net=0.0,
                    sigma_net=0.0, centroid_ch=None, n_ch=0)

    region = data[lo_ch:hi_ch].astype(np.float64)
    gross  = float(np.sum(region))

    if method == "none":
        bg_arr = np.zeros(n_ch, dtype=np.float64)
    elif method == "trapezoid":
        bg_arr = trapezoidal_roi(data, lo_ch, hi_ch)
    elif method == "polynomial":
        bg_arr = polynomial_roi(data, lo_ch, hi_ch, degree=poly_degree)
    elif method == "snip":
        if snip_bg is None:
            bg_arr = trapezoidal_roi(data, lo_ch, hi_ch)   # safe fallback
        else:
            bg_arr = snip_bg[lo_ch:hi_ch].astype(np.float64)
    else:
        bg_arr = trapezoidal_roi(data, lo_ch, hi_ch)

    bg_arr = np.maximum(bg_arr, 0.0)
    background = float(np.sum(bg_arr))
    net        = max(0.0, gross - background)
    sigma_net  = float(np.sqrt(max(gross + background, 1.0)))

    # Background-subtracted centroid
    net_region     = np.maximum(0.0, region - bg_arr)
    net_region_sum = float(np.sum(net_region))
    if net_region_sum > 0:
        channels    = np.arange(lo_ch, hi_ch, dtype=np.float64)
        centroid_ch = float(np.dot(channels, net_region) / net_region_sum)
    else:
        centroid_ch = (lo_ch + hi_ch) / 2.0

    return dict(
        gross=gross,
        background=background,
        net=net,
        sigma_net=sigma_net,
        centroid_ch=centroid_ch,
        n_ch=n_ch,
    )


# ---------------------------------------------------------------------------
# Currie detection limits (Anal. Chem. 40, 586, 1968)
# ---------------------------------------------------------------------------

def currie_limits(n_bg: float, alpha: float = 0.05) -> tuple[float, float]:
    """Compute the critical level (L_C) and detection limit (L_D) for a peak.

    Parameters
    ----------
    n_bg  : Estimated background counts under the peak region.
    alpha : Type-I error rate (default 0.05 → 95% confidence).

    Returns
    -------
    (L_C, L_D) — both in net counts.
    """
    import scipy.stats as st
    z = st.norm.ppf(1.0 - alpha)         # 1.645 for α=0.05
    L_C = z * np.sqrt(2.0 * max(n_bg, 0.5))
    L_D = L_C + z**2 + 2.0 * z * np.sqrt(z**2 * 0.25 + max(n_bg, 0.5))
    return float(L_C), float(L_D)
