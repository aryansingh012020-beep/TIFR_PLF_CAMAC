"""
calibration.py — Energy calibration for gamma-ray spectroscopy.

Methods:
    Polynomial  (linear / quadratic / cubic)  — standard for most detectors
    Cubic Spline (non-parametric)              — highly non-linear ADCs

Outputs full uncertainty propagation so the dashboard can overlay a
95% confidence band on the calibrated spectrum axis.
"""
from __future__ import annotations
import numpy as np
from dataclasses import dataclass
from typing import Literal, Optional

try:
    from scipy.interpolate import CubicSpline
    _SCIPY_OK = True
except ImportError:
    _SCIPY_OK = False

__all__ = ["CalibResult", "fit_polynomial", "fit_spline", "apply_calibration",
           "residuals", "energy_uncertainty"]

CalibMethod = Literal["linear", "quadratic", "cubic", "spline"]


@dataclass
class CalibResult:
    method: str
    success: bool
    coeffs: np.ndarray        # polynomial coefficients (highest power first) or []
    poly: object              # np.poly1d or None
    spline: object            # CubicSpline or None
    rms_kev: float
    r_squared: float
    n_points: int
    residuals_kev: np.ndarray
    cov: np.ndarray           # covariance matrix of polynomial coeffs (poly only)
    message: str = ""

    def energy(self, channel: float | np.ndarray) -> np.ndarray:
        """Convert channel(s) to energy (keV)."""
        ch = np.asarray(channel, dtype=np.float64)
        if self.spline is not None:
            return self.spline(ch)
        if self.poly is not None:
            return self.poly(ch)
        return np.full_like(ch, np.nan)

    def energy_uncertainty(self, channel: float | np.ndarray) -> np.ndarray:
        """95% confidence half-width (keV) at each channel, polynomial only."""
        if self.poly is None or len(self.cov) == 0:
            ch = np.asarray(channel, dtype=np.float64)
            return np.full_like(ch, np.nan)
        return energy_uncertainty(channel, self.coeffs, self.cov, confidence=0.95)


# ---------------------------------------------------------------------------
# Polynomial calibration
# ---------------------------------------------------------------------------

def fit_polynomial(
    channels: np.ndarray,
    energies: np.ndarray,
    degree: int = 1,
) -> CalibResult:
    """Fit an nth-degree polynomial E(ch) to calibration points.

    Uses numpy.polyfit with covariance estimation via the residual-based formula.
    Returns 1-sigma covariance matrix of the polynomial coefficients.
    """
    ch  = np.asarray(channels, dtype=np.float64)
    eng = np.asarray(energies,  dtype=np.float64)
    n   = len(ch)

    if n < degree + 1:
        return CalibResult(
            method=f"poly{degree}", success=False, coeffs=np.array([]),
            poly=None, spline=None, rms_kev=999, r_squared=0,
            n_points=n, residuals_kev=np.array([]), cov=np.array([[]]),
            message=f"Need ≥ {degree+1} points for degree-{degree} polynomial"
        )

    coeffs, cov = np.polyfit(ch, eng, degree, cov=True)
    poly        = np.poly1d(coeffs)
    fitted      = poly(ch)
    resids      = eng - fitted
    rms         = float(np.sqrt(np.mean(resids ** 2)))
    ss_res      = float(np.sum(resids ** 2))
    ss_tot      = float(np.sum((eng - eng.mean()) ** 2))
    r2          = 1.0 - ss_res / max(ss_tot, 1e-30)

    return CalibResult(
        method=f"poly{degree}", success=True,
        coeffs=coeffs, poly=poly, spline=None,
        rms_kev=rms, r_squared=r2,
        n_points=n, residuals_kev=resids,
        cov=cov,
    )


# ---------------------------------------------------------------------------
# Cubic spline calibration
# ---------------------------------------------------------------------------

def fit_spline(
    channels: np.ndarray,
    energies: np.ndarray,
) -> CalibResult:
    """Cubic spline interpolation through all calibration points (non-parametric)."""
    ch  = np.asarray(channels, dtype=np.float64)
    eng = np.asarray(energies,  dtype=np.float64)
    n   = len(ch)

    if not _SCIPY_OK:
        return CalibResult(
            method="spline", success=False, coeffs=np.array([]),
            poly=None, spline=None, rms_kev=999, r_squared=0,
            n_points=n, residuals_kev=np.array([]), cov=np.array([[]]),
            message="scipy not available"
        )

    if n < 3:
        return CalibResult(
            method="spline", success=False, coeffs=np.array([]),
            poly=None, spline=None, rms_kev=999, r_squared=0,
            n_points=n, residuals_kev=np.array([]), cov=np.array([[]]),
            message="Need ≥ 3 points for cubic spline"
        )

    # Sort by channel (spline requires monotone x)
    order = np.argsort(ch)
    ch_s  = ch[order]
    eng_s = eng[order]

    cs     = CubicSpline(ch_s, eng_s, bc_type='natural')
    fitted = cs(ch_s)
    resids = eng_s - fitted
    rms    = float(np.sqrt(np.mean(resids ** 2)))

    return CalibResult(
        method="spline", success=True,
        coeffs=np.array([]), poly=None, spline=cs,
        rms_kev=rms, r_squared=1.0,   # spline passes through all points
        n_points=n, residuals_kev=resids,
        cov=np.array([[]]),
    )


# ---------------------------------------------------------------------------
# Convenience wrappers
# ---------------------------------------------------------------------------

def apply_calibration(
    cal: CalibResult,
    channels: np.ndarray,
) -> np.ndarray:
    """Convert channel array to keV using a CalibResult."""
    return cal.energy(channels)


def residuals(cal: CalibResult) -> np.ndarray:
    """Return residuals in keV from a calibration fit."""
    return cal.residuals_kev


def energy_uncertainty(
    channels: float | np.ndarray,
    coeffs: np.ndarray,
    cov: np.ndarray,
    confidence: float = 0.95,
) -> np.ndarray:
    """Compute the propagated energy uncertainty (keV) at each channel.

    Uses the Jacobian of the polynomial with respect to the coefficients:
        σ²_E(ch) = J(ch)ᵀ · Cov · J(ch)
    where J[k] = ch^(deg-k), the derivative of E w.r.t. coeffs[k].

    Returns the half-width of the confidence interval (not ±1σ),
    scaled by the standard normal z-score for the chosen confidence level.
    """
    import scipy.stats as st
    ch  = np.asarray(channels, dtype=np.float64).ravel()
    deg = len(coeffs) - 1
    z   = st.norm.ppf(0.5 + confidence / 2.0)   # 1.96 for 95%

    sigma_E = np.zeros(len(ch))
    for i, c in enumerate(ch):
        J = np.array([c ** (deg - k) for k in range(deg + 1)], dtype=np.float64)
        var = float(J @ cov @ J)
        sigma_E[i] = z * np.sqrt(max(var, 0.0))

    return sigma_E
