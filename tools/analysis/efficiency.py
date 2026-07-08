"""
efficiency.py — Photopeak detection efficiency calibration.

Models:
    Model A — Log-quadratic  : ln(ε) = a₀ + a₁·ln(E) + a₂·(ln(E))²
    Model B — ANSI N42.14   : ln(ε) = Σᵢ aᵢ·(ln(E))ⁱ  (degree 3–5)
    Model C — Cubic Spline   : non-parametric, GSI/GANIL standard

Workflow:
    1. User provides calibration source: activity A₀ (Bq), reference date.
    2. Decay-corrected activity:  A(t) = A₀ · exp(-ln2 · Δt / T½)
    3. Efficiency at each energy: ε(E) = N_net / (A(t) · I_γ · t_live)
    4. Fit chosen model to (E, ε) data.
    5. Activity of unknown peak: act = N_net / (ε(E) · I_γ · t_live)
"""
from __future__ import annotations
import numpy as np
from dataclasses import dataclass
from typing import Optional

try:
    from scipy.optimize import curve_fit
    from scipy.interpolate import CubicSpline
    _SCIPY_OK = True
except ImportError:
    _SCIPY_OK = False

__all__ = [
    "EfficiencyResult", "decay_correct_activity",
    "compute_efficiency_points", "fit_efficiency",
    "efficiency_at", "activity_from_peak",
]


@dataclass
class EfficiencyResult:
    model: str
    success: bool
    energies: np.ndarray      # calibration energies (keV)
    efficiencies: np.ndarray  # measured efficiencies
    coeffs: np.ndarray        # model coefficients
    rms: float                # RMS residual in ln(ε)
    r_squared: float
    message: str = ""
    _spline: object = None
    _poly_fn: object = None

    def efficiency(self, energy_kev: float | np.ndarray) -> np.ndarray:
        """Return efficiency at given energy(s) in keV."""
        E = np.asarray(energy_kev, dtype=np.float64)
        lnE = np.log(np.maximum(E, 1e-6))
        if self._spline is not None:
            lnEps = self._spline(lnE)
        elif self._poly_fn is not None:
            lnEps = self._poly_fn(lnE)
        else:
            return np.zeros_like(E)
        return np.exp(lnEps)


_FAIL_EFF = lambda msg: EfficiencyResult(  # noqa: E731
    model="none", success=False,
    energies=np.array([]), efficiencies=np.array([]),
    coeffs=np.array([]), rms=999.0, r_squared=0.0,
    message=msg,
)


# ---------------------------------------------------------------------------
# Physical constants for common calibration sources
# ---------------------------------------------------------------------------

HALF_LIVES_YEARS = {
    "Am-241": 432.6,
    "Ba-133": 10.51,
    "Co-57":  0.7451,
    "Co-60":  5.2713,
    "Cs-137": 30.17,
    "Eu-152": 13.517,
    "Na-22":  2.6019,
    "Mn-54":  0.8553,
}


def decay_correct_activity(
    a0_bq: float,
    half_life_years: float,
    elapsed_years: float,
) -> float:
    """A(t) = A₀ · exp(-ln2 · t / T½)."""
    lam = np.log(2.0) / max(half_life_years, 1e-12)
    return float(a0_bq * np.exp(-lam * elapsed_years))


# ---------------------------------------------------------------------------
# Compute efficiency points from calibration source
# ---------------------------------------------------------------------------

def compute_efficiency_points(
    net_areas: list[float],          # net peak areas (counts)
    energies_kev: list[float],       # corresponding energies (keV)
    intensities: list[float],        # gamma emission intensities (fraction, 0–1)
    activity_bq: float,              # decay-corrected source activity (Bq)
    live_time_s: float,              # acquisition live time (seconds)
) -> tuple[np.ndarray, np.ndarray]:
    """Compute ε(E) = N_net / (A · I_γ · t_live) for each calibration line.

    Returns (energies, efficiencies) arrays, excluding non-physical values.
    """
    E   = np.asarray(energies_kev,   dtype=np.float64)
    N   = np.asarray(net_areas,      dtype=np.float64)
    I   = np.asarray(intensities,    dtype=np.float64)
    denom = activity_bq * I * live_time_s
    eps   = N / np.maximum(denom, 1e-30)

    # Remove non-physical values (ε ≤ 0 or > 1)
    mask = (eps > 0) & (eps < 1.0) & (N > 0)
    return E[mask], eps[mask]


# ---------------------------------------------------------------------------
# Fit models
# ---------------------------------------------------------------------------

def _log_quad(lnE, a0, a1, a2):
    return a0 + a1 * lnE + a2 * lnE**2


def _log_poly3(lnE, a0, a1, a2, a3):
    return a0 + a1*lnE + a2*lnE**2 + a3*lnE**3


def _log_poly4(lnE, a0, a1, a2, a3, a4):
    return a0 + a1*lnE + a2*lnE**2 + a3*lnE**3 + a4*lnE**4


def fit_efficiency(
    energies: np.ndarray,
    efficiencies: np.ndarray,
    model: str = "log_quad",
) -> EfficiencyResult:
    """Fit an efficiency model to (E, ε) calibration data.

    model options:
        "log_quad"   — 3-parameter log-quadratic      (standard HPGe)
        "ansi_n42"   — 4-parameter ANSI N42.14 log-poly  (field detectors)
        "ansi_n42_5" — 5-parameter ANSI N42.14 log-poly  (wide energy range)
        "spline"     — cubic spline in ln-ln space     (GSI/GANIL)
    """
    if not _SCIPY_OK and model != "spline":
        return _FAIL_EFF("scipy not available")

    E   = np.asarray(energies,     dtype=np.float64)
    eps = np.asarray(efficiencies, dtype=np.float64)
    n   = len(E)

    if n < 2:
        return _FAIL_EFF("Need ≥ 2 efficiency calibration points")

    lnE   = np.log(np.maximum(E, 1e-6))
    lnEps = np.log(np.maximum(eps, 1e-20))

    if model == "spline":
        if n < 3:
            return _FAIL_EFF("Need ≥ 3 points for spline")
        order  = np.argsort(lnE)
        cs     = CubicSpline(lnE[order], lnEps[order], bc_type='natural')
        fitted = cs(lnE)
        resids = lnEps - fitted
        rms    = float(np.sqrt(np.mean(resids**2)))
        return EfficiencyResult(
            model="spline", success=True,
            energies=E, efficiencies=eps, coeffs=np.array([]),
            rms=rms, r_squared=1.0, _spline=cs,
        )

    fn_map = {
        "log_quad":   (_log_quad,   3),
        "ansi_n42":   (_log_poly3,  4),
        "ansi_n42_5": (_log_poly4,  5),
    }
    fn, n_params = fn_map.get(model, (_log_quad, 3))

    if n < n_params:
        return _FAIL_EFF(f"Need ≥ {n_params} points for {model}")

    try:
        popt, _ = curve_fit(fn, lnE, lnEps, p0=[np.mean(lnEps)] + [0.0]*(n_params-1),
                            maxfev=10000)
    except Exception as e:
        return _FAIL_EFF(str(e))

    fitted  = fn(lnE, *popt)
    resids  = lnEps - fitted
    rms     = float(np.sqrt(np.mean(resids**2)))
    ss_res  = float(np.sum(resids**2))
    ss_tot  = float(np.sum((lnEps - lnEps.mean())**2))
    r2      = 1.0 - ss_res / max(ss_tot, 1e-30)

    poly_fn = lambda lnE_: fn(lnE_, *popt)  # noqa: E731

    return EfficiencyResult(
        model=model, success=True,
        energies=E, efficiencies=eps, coeffs=np.array(popt),
        rms=rms, r_squared=r2, _poly_fn=poly_fn,
    )


# ---------------------------------------------------------------------------
# Activity calculation
# ---------------------------------------------------------------------------

def efficiency_at(eff_result: EfficiencyResult, energy_kev: float) -> float:
    """Return efficiency at a given energy. Returns 0 if calibration invalid."""
    if not eff_result.success:
        return 0.0
    val = eff_result.efficiency(energy_kev)
    return float(np.clip(val, 0, 1))


def activity_from_peak(
    net_area: float,
    net_area_err: float,
    energy_kev: float,
    intensity: float,
    live_time_s: float,
    eff_result: EfficiencyResult,
) -> tuple[float, float]:
    """Calculate activity (Bq) and 1-sigma uncertainty.

        A = N_net / (ε(E) · I_γ · t_live)
        σ_A / A = σ_net / N_net     (efficiency uncertainty not yet propagated)

    Returns (activity_bq, sigma_activity_bq).
    """
    eps = efficiency_at(eff_result, energy_kev)
    if eps <= 0 or intensity <= 0 or live_time_s <= 0:
        return 0.0, 0.0
    denom    = eps * intensity * live_time_s
    activity = net_area / denom
    sigma    = net_area_err / denom
    return float(activity), float(sigma)
