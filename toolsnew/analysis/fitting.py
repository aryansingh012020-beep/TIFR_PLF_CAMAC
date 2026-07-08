"""
fitting.py — Peak fitting models for gamma-ray spectroscopy.

Models implemented:
    1. Gaussian + flat background          (isolated peaks, NaI detectors)
    2. Gaussian + linear slope background  (peaks on Compton continuum edge)
    3. Hypermet function                   (HPGe — ORNL/Phillips 1983, NIM A 218)
    4. Voigt profile                       (cyclotron labs, natural linewidth)
    5. Multiplet: simultaneous N-Gaussian  (unresolved doublets/triplets)
    6. Bayesian MAP (Poisson likelihood)   (weak peaks, Baker & Cousins 1984)

All fitters return a FitResult dataclass with standardised fields so the
dashboard can display results uniformly regardless of model chosen.
"""
from __future__ import annotations
import numpy as np
from dataclasses import dataclass, field
from typing import Literal, Optional

try:
    from scipy.optimize import curve_fit, minimize
    from scipy.special import voigt_profile, erfc
    _SCIPY_OK = True
except ImportError:
    _SCIPY_OK = False

__all__ = [
    "FitResult", "FitModel",
    "fit_gaussian", "fit_gaussian_slope", "fit_hypermet",
    "fit_voigt", "fit_multiplet", "fit_bayesian_map",
    "auto_fit",
]

FitModel = Literal["gaussian", "gaussian_slope", "hypermet", "voigt", "multiplet", "bayesian"]


# ---------------------------------------------------------------------------
# Result container
# ---------------------------------------------------------------------------

@dataclass
class FitResult:
    model: str
    success: bool
    centroid: float          # channel (sub-channel)
    centroid_err: float
    fwhm_ch: float
    fwhm_err: float
    area: float              # net Gaussian area (counts)
    area_err: float
    background: float        # local background level at centroid
    chi2_red: float          # reduced χ²  (goodness of fit)
    log_likelihood: float    # Poisson log-likelihood
    params: dict = field(default_factory=dict)  # full parameter dict
    message: str = ""

    # keV equivalents (set by caller after calibration)
    centroid_kev: float = 0.0
    fwhm_kev: float = 0.0


_FAIL = lambda msg: FitResult(  # noqa: E731
    model="none", success=False, centroid=0, centroid_err=0,
    fwhm_ch=0, fwhm_err=0, area=0, area_err=0, background=0,
    chi2_red=999, log_likelihood=-1e30, message=msg
)


# ---------------------------------------------------------------------------
# Helper: poisson log-likelihood
# ---------------------------------------------------------------------------

def _poisson_nll(y_obs: np.ndarray, y_model: np.ndarray) -> float:
    """−2 ln L for Poisson counting (Baker & Cousins 1984)."""
    mu   = np.maximum(y_model, 1e-10)
    nll  = 2.0 * np.sum(mu - y_obs * np.log(mu))
    return float(nll)


def _chi2_red(y_obs: np.ndarray, y_model: np.ndarray, n_params: int) -> float:
    dof = max(len(y_obs) - n_params, 1)
    return float(np.sum((y_obs - y_model) ** 2 / np.maximum(y_obs, 1.0)) / dof)


# ---------------------------------------------------------------------------
# Helper: extract fit window
# ---------------------------------------------------------------------------

def _window(data: np.ndarray, ch: int, fwhm_est: float, factor: float = 3.0):
    half_win = max(int(fwhm_est * factor), 6)
    lo = max(0, ch - half_win)
    hi = min(len(data) - 1, ch + half_win)
    x  = np.arange(lo, hi + 1, dtype=np.float64)
    y  = data[lo:hi + 1].astype(np.float64)
    return x, y, lo, hi


# ---------------------------------------------------------------------------
# 1. Gaussian + flat background
# ---------------------------------------------------------------------------

def _gauss_flat(x, A, mu, sigma, B):
    return A * np.exp(-0.5 * ((x - mu) / sigma) ** 2) + B


def fit_gaussian(
    data: np.ndarray,
    ch: int,
    fwhm_est: float,
) -> FitResult:
    """Standard Gaussian + flat background (4 parameters)."""
    if not _SCIPY_OK:
        return _FAIL("scipy not available")

    x, y, lo, hi = _window(data, ch, fwhm_est, factor=2.5)
    if len(x) < 5:
        return _FAIL("window too small")

    sigma0 = max(fwhm_est / 2.355, 1.0)
    p0     = [float(data[ch]), float(ch), sigma0, float(np.percentile(y, 10))]
    bounds = ([0, lo, 0.3, 0], [1e9, hi, fwhm_est * 3, float(y.max()) + 1])

    try:
        popt, pcov = curve_fit(_gauss_flat, x, y, p0=p0, bounds=bounds, maxfev=8000)
    except Exception as e:
        return _FAIL(str(e))

    A, mu, sigma, B = popt
    perr = np.sqrt(np.maximum(np.diag(pcov), 0))
    fwhm     = 2.355 * abs(sigma)
    fwhm_err = 2.355 * perr[2]
    area     = A * abs(sigma) * np.sqrt(2 * np.pi)
    area_err = area * np.sqrt((perr[0]/max(A,1e-9))**2 + (perr[2]/max(abs(sigma),1e-9))**2)

    y_fit  = _gauss_flat(x, *popt)
    return FitResult(
        model="gaussian", success=True,
        centroid=mu, centroid_err=perr[1],
        fwhm_ch=fwhm, fwhm_err=fwhm_err,
        area=area, area_err=area_err,
        background=B, chi2_red=_chi2_red(y, y_fit, 4),
        log_likelihood=_poisson_nll(y, y_fit),
        params=dict(A=A, mu=mu, sigma=sigma, B=B),
    )


# ---------------------------------------------------------------------------
# 2. Gaussian + linear slope background
# ---------------------------------------------------------------------------

def _gauss_slope(x, A, mu, sigma, B, m):
    return A * np.exp(-0.5 * ((x - mu) / sigma) ** 2) + B + m * (x - mu)


def fit_gaussian_slope(
    data: np.ndarray,
    ch: int,
    fwhm_est: float,
) -> FitResult:
    """Gaussian + linearly sloping background (5 parameters)."""
    if not _SCIPY_OK:
        return _FAIL("scipy not available")

    x, y, lo, hi = _window(data, ch, fwhm_est, factor=3.0)
    if len(x) < 6:
        return _FAIL("window too small")

    sigma0 = max(fwhm_est / 2.355, 1.0)
    p0     = [float(data[ch]), float(ch), sigma0, float(np.percentile(y, 10)), 0.0]
    bounds = ([0, lo, 0.3, 0, -1e6], [1e9, hi, fwhm_est * 3, float(y.max()) + 1, 1e6])

    try:
        popt, pcov = curve_fit(_gauss_slope, x, y, p0=p0, bounds=bounds, maxfev=8000)
    except Exception as e:
        return _FAIL(str(e))

    A, mu, sigma, B, m = popt
    perr = np.sqrt(np.maximum(np.diag(pcov), 0))
    fwhm = 2.355 * abs(sigma)
    area = A * abs(sigma) * np.sqrt(2 * np.pi)
    area_err = area * np.sqrt((perr[0]/max(A,1e-9))**2 + (perr[2]/max(abs(sigma),1e-9))**2)

    y_fit = _gauss_slope(x, *popt)
    return FitResult(
        model="gaussian_slope", success=True,
        centroid=mu, centroid_err=perr[1],
        fwhm_ch=fwhm, fwhm_err=2.355 * perr[2],
        area=area, area_err=area_err,
        background=float(B + m * 0),
        chi2_red=_chi2_red(y, y_fit, 5),
        log_likelihood=_poisson_nll(y, y_fit),
        params=dict(A=A, mu=mu, sigma=sigma, B=B, m=m),
    )


# ---------------------------------------------------------------------------
# 3. Hypermet (Phillips & Marlow, NIM A 218, 1983)
# ---------------------------------------------------------------------------

def _hypermet(x, A, mu, sigma, C, beta, S, B):
    """
    Hypermet peak shape:
        Gaussian core  + low-energy exponential tail + step + flat BG

        f(x) = A·G(x,μ,σ)
               + (C·A/2)·exp((x-μ)/β)·erfc((x-μ)/(σ√2) + σ/(β√2))
               + (S·A/2)·erfc((x-μ)/(σ√2))
               + B
    """
    t    = (x - mu) / (sigma * np.sqrt(2.0))
    gauss = A * np.exp(-0.5 * ((x - mu) / sigma) ** 2)

    # Low-energy tail (complete charge collection failure)
    tail_arg = t + sigma / (beta * np.sqrt(2.0))
    tail = (C * A / 2.0) * np.exp(np.clip((x - mu) / beta, -700, 0)) * erfc(tail_arg)

    # Step function (Compton scatter below the peak)
    step = (S * A / 2.0) * erfc(t)

    return gauss + tail + step + B


def fit_hypermet(
    data: np.ndarray,
    ch: int,
    fwhm_est: float,
) -> FitResult:
    """Hypermet function fit — gold standard for HPGe peak analysis (7 params)."""
    if not _SCIPY_OK:
        return _FAIL("scipy not available")

    x, y, lo, hi = _window(data, ch, fwhm_est, factor=3.5)
    if len(x) < 8:
        return _FAIL("window too small")

    sigma0 = max(fwhm_est / 2.355, 1.0)
    A0     = float(data[ch])
    B0     = float(np.percentile(y, 5))

    # [A, mu, sigma, C, beta, S, B]
    p0     = [A0, float(ch), sigma0, 0.05, sigma0 * 2.0, 0.01, B0]
    bounds = (
        [0,  lo, 0.3,   0,   0.1, 0,   0  ],
        [1e9, hi, fwhm_est*3, 0.5, sigma0*10, 0.5, float(y.max()) + 1],
    )

    try:
        popt, pcov = curve_fit(_hypermet, x, y, p0=p0, bounds=bounds, maxfev=20000,
                               method='trf')
    except Exception as e:
        # Hypermet may fail on simple spectra — fall back to Gaussian
        return fit_gaussian(data, ch, fwhm_est)

    A, mu, sigma, C, beta, S, B = popt
    perr = np.sqrt(np.maximum(np.diag(pcov), 0))
    fwhm = 2.355 * abs(sigma)
    # Net area = Gaussian core + tail contribution
    area_gauss = A * abs(sigma) * np.sqrt(2 * np.pi)
    area_tail  = C * A * abs(beta)  # approximate tail area
    area       = area_gauss + area_tail
    area_err   = area * (perr[0] / max(A, 1e-9))

    y_fit = _hypermet(x, *popt)
    return FitResult(
        model="hypermet", success=True,
        centroid=mu, centroid_err=perr[1],
        fwhm_ch=fwhm, fwhm_err=2.355 * perr[2],
        area=area, area_err=area_err,
        background=B, chi2_red=_chi2_red(y, y_fit, 7),
        log_likelihood=_poisson_nll(y, y_fit),
        params=dict(A=A, mu=mu, sigma=sigma, C=C, beta=beta, S=S, B=B),
    )


# ---------------------------------------------------------------------------
# 4. Voigt profile (scipy.special.voigt_profile)
# ---------------------------------------------------------------------------

def _voigt_model(x, A, mu, sigma_G, sigma_L, B):
    """Voigt line shape: A * voigt_profile(x-mu, sigma_G, sigma_L) + B."""
    return A * voigt_profile(x - mu, sigma_G, sigma_L) + B


def fit_voigt(
    data: np.ndarray,
    ch: int,
    fwhm_est: float,
) -> FitResult:
    """Voigt profile fit — separates Gaussian (detector) and Lorentzian (nuclear) widths."""
    if not _SCIPY_OK:
        return _FAIL("scipy not available")

    x, y, lo, hi = _window(data, ch, fwhm_est, factor=3.0)
    if len(x) < 6:
        return _FAIL("window too small")

    sigma0 = max(fwhm_est / 2.355, 1.0)
    A0_raw = float(data[ch])
    # Voigt is normalised, so A must be scaled
    vp_peak = float(voigt_profile(0, sigma0, 0.1))
    A0 = A0_raw / max(vp_peak, 1e-9)

    p0     = [A0, float(ch), sigma0, 0.1, float(np.percentile(y, 5))]
    bounds = ([0, lo, 0.1, 0.01, 0], [1e9, hi, fwhm_est * 3, fwhm_est * 2, float(y.max()) + 1])

    try:
        popt, pcov = curve_fit(_voigt_model, x, y, p0=p0, bounds=bounds, maxfev=12000)
    except Exception as e:
        return _FAIL(str(e))

    A, mu, sigma_G, sigma_L, B = popt
    perr = np.sqrt(np.maximum(np.diag(pcov), 0))
    # Approximate Voigt FWHM: Thompson et al. (1987) formula
    fG   = 2.355 * sigma_G
    fL   = 2.0   * sigma_L
    fwhm = (fG**5 + 2.69269*fG**4*fL + 2.42843*fG**3*fL**2
            + 4.47163*fG**2*fL**3 + 0.07842*fG*fL**4 + fL**5) ** 0.2

    # Area: integral of voigt_profile over all x = 1, so area = A
    area = A

    y_fit = _voigt_model(x, *popt)
    return FitResult(
        model="voigt", success=True,
        centroid=mu, centroid_err=perr[1],
        fwhm_ch=fwhm, fwhm_err=0.0,   # FWHM error from combined formula is complex
        area=area, area_err=perr[0],
        background=B, chi2_red=_chi2_red(y, y_fit, 5),
        log_likelihood=_poisson_nll(y, y_fit),
        params=dict(A=A, mu=mu, sigma_G=sigma_G, sigma_L=sigma_L, B=B),
    )


# ---------------------------------------------------------------------------
# 5. Multiplet: simultaneous N-Gaussian fit
# ---------------------------------------------------------------------------

def _multi_gauss(x, *params):
    """N Gaussians + linear background. params = [A1,mu1,s1, A2,mu2,s2, ..., B, m]."""
    n_peaks = (len(params) - 2) // 3
    B, m    = params[-2], params[-1]
    result  = B + m * (x - x[len(x)//2])
    for i in range(n_peaks):
        A, mu, sigma = params[3*i], params[3*i+1], params[3*i+2]
        result = result + A * np.exp(-0.5 * ((x - mu) / sigma) ** 2)
    return result


def fit_multiplet(
    data: np.ndarray,
    seed_channels: list[int],
    fwhm_estimates: list[float],
) -> list[FitResult]:
    """Fit N Gaussians simultaneously within a shared window.

    Parameters
    ----------
    seed_channels  : List of approximate peak channel positions.
    fwhm_estimates : Per-peak initial FWHM estimates (channels).

    Returns
    -------
    List of FitResult, one per peak in the multiplet.
    """
    if not _SCIPY_OK or len(seed_channels) == 0:
        return [_FAIL("scipy not available or no seeds")]

    N    = len(data)
    chs  = sorted(seed_channels)
    fwms = fwhm_estimates if len(fwhm_estimates) == len(chs) else [10.0] * len(chs)

    # Shared window spanning all peaks + 3 FWHM padding
    lo = max(0, chs[0] - int(3 * max(fwms)))
    hi = min(N - 1, chs[-1] + int(3 * max(fwms)))
    x  = np.arange(lo, hi + 1, dtype=np.float64)
    y  = data[lo:hi + 1].astype(np.float64)

    if len(x) < 5 * len(chs):
        return [_FAIL("window too small for multiplet")]

    # Build p0 and bounds
    p0     = []
    lo_b   = []
    hi_b   = []
    for ch, fw in zip(chs, fwms):
        sig = max(fw / 2.355, 1.0)
        A   = float(data[ch]) if ch < N else 1.0
        p0  += [A, float(ch), sig]
        lo_b += [0, ch - 3*fw, 0.3]
        hi_b += [1e9, ch + 3*fw, fw * 4]
    # background + slope
    B0 = float(np.percentile(y, 5))
    p0 += [B0, 0.0]
    lo_b += [0, -1e4]
    hi_b += [float(y.max()) + 1, 1e4]

    try:
        popt, pcov = curve_fit(_multi_gauss, x, y, p0=p0,
                               bounds=(lo_b, hi_b), maxfev=20000)
    except Exception as e:
        return [_FAIL(str(e))]

    perr = np.sqrt(np.maximum(np.diag(pcov), 0))
    B, m  = popt[-2], popt[-1]
    y_fit = _multi_gauss(x, *popt)
    chi2  = _chi2_red(y, y_fit, len(popt))
    nll   = _poisson_nll(y, y_fit)

    results = []
    for i, (ch, fw) in enumerate(zip(chs, fwms)):
        A, mu, sigma = popt[3*i], popt[3*i+1], popt[3*i+2]
        se_A, se_mu, se_s = perr[3*i], perr[3*i+1], perr[3*i+2]
        fwhm   = 2.355 * abs(sigma)
        area   = A * abs(sigma) * np.sqrt(2 * np.pi)
        area_e = area * np.sqrt((se_A/max(A,1e-9))**2 + (se_s/max(abs(sigma),1e-9))**2)
        results.append(FitResult(
            model="multiplet", success=True,
            centroid=mu, centroid_err=se_mu,
            fwhm_ch=fwhm, fwhm_err=2.355 * se_s,
            area=area, area_err=area_e,
            background=B, chi2_red=chi2,
            log_likelihood=nll,
            params=dict(A=A, mu=mu, sigma=sigma, B=B, m=m),
        ))
    return results


# ---------------------------------------------------------------------------
# 6. Bayesian MAP with Poisson log-likelihood (Baker & Cousins, NIM A 221, 1984)
# ---------------------------------------------------------------------------

def fit_bayesian_map(
    data: np.ndarray,
    ch: int,
    fwhm_est: float,
) -> FitResult:
    """Bayesian MAP fit using Poisson NLL as objective — correct for low-count peaks."""
    if not _SCIPY_OK:
        return _FAIL("scipy not available")

    x, y, lo, hi = _window(data, ch, fwhm_est, factor=3.0)
    if len(x) < 5:
        return _FAIL("window too small")

    sigma0 = max(fwhm_est / 2.355, 1.0)
    B0     = float(np.percentile(y, 5))
    A0     = max(float(data[ch]) - B0, 1.0)

    def nll(p):
        A, mu, sigma, B = p
        if A <= 0 or sigma <= 0 or B < 0:
            return 1e30
        model = _gauss_flat(x, A, mu, abs(sigma), abs(B))
        mu_m  = np.maximum(model, 1e-10)
        return 2.0 * float(np.sum(mu_m - y * np.log(mu_m)))

    result = minimize(
        nll, x0=[A0, float(ch), sigma0, B0],
        method='Nelder-Mead',
        options=dict(maxiter=20000, xatol=1e-4, fatol=1e-4),
    )

    if not result.success and result.fun > 1e10:
        return _FAIL("Bayesian MAP did not converge")

    A, mu, sigma, B = result.x
    sigma = abs(sigma)
    fwhm  = 2.355 * sigma
    area  = A * sigma * np.sqrt(2 * np.pi)

    # Approximate uncertainties from numerical Hessian (finite differences)
    try:
        from scipy.optimize import approx_fprime
        eps = np.array([A * 0.01, 0.1, sigma * 0.01, max(B * 0.01, 0.1)])
        hess_diag = np.zeros(4)
        x0 = result.x.copy()
        f0 = nll(x0)
        for i in range(4):
            dx = np.zeros(4); dx[i] = eps[i]
            hess_diag[i] = (nll(x0 + dx) - 2*f0 + nll(x0 - dx)) / eps[i]**2
        var_diag = 1.0 / np.maximum(hess_diag, 1e-30)
        perr = np.sqrt(np.maximum(var_diag, 0))
    except Exception:
        perr = np.array([0.0, 0.0, 0.0, 0.0])

    y_fit = _gauss_flat(x, A, mu, sigma, B)
    return FitResult(
        model="bayesian", success=True,
        centroid=mu, centroid_err=perr[1],
        fwhm_ch=fwhm, fwhm_err=2.355 * perr[2],
        area=area, area_err=area * perr[0] / max(A, 1e-9),
        background=B, chi2_red=_chi2_red(y, y_fit, 4),
        log_likelihood=float(result.fun),
        params=dict(A=A, mu=mu, sigma=sigma, B=B),
    )


# ---------------------------------------------------------------------------
# Auto-fit: select best model by chi2_red
# ---------------------------------------------------------------------------

def auto_fit(
    data: np.ndarray,
    ch: int,
    fwhm_est: float,
    preferred_model: FitModel = "gaussian",
) -> FitResult:
    """Run the preferred model; if chi²/dof > 2.0, also try gaussian_slope.
    Returns the result with the lower chi²/dof.
    """
    dispatch = {
        "gaussian":       fit_gaussian,
        "gaussian_slope": fit_gaussian_slope,
        "hypermet":       fit_hypermet,
        "voigt":          fit_voigt,
        "bayesian":       fit_bayesian_map,
    }
    fn  = dispatch.get(preferred_model, fit_gaussian)
    res = fn(data, ch, fwhm_est)

    # If chi² is poor, also try gaussian_slope
    if res.chi2_red > 2.0 and preferred_model not in ("gaussian_slope", "hypermet"):
        alt = fit_gaussian_slope(data, ch, fwhm_est)
        if alt.success and alt.chi2_red < res.chi2_red:
            return alt

    return res
