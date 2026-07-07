#!/bin/bash
# =============================================================================
# start_sim_dashboard.sh — Launch LAMPS Dashboard in Simulated Data Mode
#
# No EPICS, no softIoc, no bridge needed.
# The dashboard generates the same spectrum as sim_shm.c internally:
#   Co-60  (1173 keV) at ch  512,  FWHM ~20 ch
#   Cs-137 (662  keV) at ch 1024,  FWHM ~15 ch
#   K-40   (1461 keV) at ch 2048,  FWHM ~25 ch
#   + Compton continuum + flat noise floor
#
# START / STOP / RESET all work in this mode.
#
# Usage:
#   ./start_sim_dashboard.sh
# =============================================================================

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
DASHBOARD="$SCRIPT_DIR/tools/lamps_dashboard.py"

echo "======================================"
echo " LAMPS Dashboard — Simulated Data Mode"
echo "======================================"
echo "  Peaks:"
echo "    Co-60  at ch 512   (FWHM ~20 ch)"
echo "    Cs-137 at ch 1024  (FWHM ~15 ch)"
echo "    K-40   at ch 2048  (FWHM ~25 ch)"
echo "    + Compton + noise"
echo ""
echo "  No EPICS required — START/STOP/RESET work in sim mode."
echo "======================================"
echo ""

# Suppress the XDG_SESSION_TYPE warning on Wayland/GNOME systems
export QT_QPA_PLATFORM="${QT_QPA_PLATFORM:-xcb}"

python3 "$DASHBOARD" --sim "$@"
