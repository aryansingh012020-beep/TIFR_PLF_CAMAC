"""
analysis package — LAMPS spectroscopy engine.

Modules:
    background   : SNIP, Trapezoidal, Polynomial background estimation
    peaksearch   : Savitzky-Golay + Mariscotti second-derivative peak finder
    fitting      : Gaussian, Hypermet, Voigt, Multiplet, Bayesian fitters
    calibration  : Polynomial + Spline energy calibration with uncertainties
    isotopeid    : Multi-line scoring isotope identification
    efficiency   : Log-poly / ANSI N42.14 / Spline efficiency curves
    export       : IAEA SPE, ROOT-CSV, JSON analysis reports
"""
__all__ = [
    "background", "peaksearch", "fitting",
    "calibration", "isotopeid", "efficiency", "export",
]
