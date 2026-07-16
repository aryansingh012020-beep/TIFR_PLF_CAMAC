#!/bin/bash
# =========================================================================
# run_bridge.sh — Start the LAMPS EPICS Bridge
#
# Usage:
#   ./run_bridge.sh            # default: prefix=LAMPS, rate=5 Hz
#   ./run_bridge.sh -v         # verbose: print every PV update to stdout
#   ./run_bridge.sh -p MYEXP   # use a custom PV prefix (e.g. MYEXP:STATUS)
#   ./run_bridge.sh -r 10      # poll at 10 Hz instead of 5
#
# Prerequisites:
#   1. softIoc must be running (cd src && ./run_ioc.sh) — in a separate terminal
#   2. LAMPS must be running (./lamps) — creates the shared memory segment
#   3. This bridge must be built: cd src && make EPICS_BASE=~/epics/base bridge
#
# EPICS environment:
#   The script auto-detects EPICS_BASE from the environment or falls back
#   to ~/epics/base (the default install location from setup_environment.sh).
# =========================================================================

EPICS_BASE="${EPICS_BASE:-$HOME/epics/base}"
EPICS_ARCH="${EPICS_HOST_ARCH:-linux-x86_64}"

BRIDGE="$(dirname "$0")/lamps_epics_bridge"

if [ ! -x "$BRIDGE" ]; then
    echo "ERROR: Bridge binary not found at $BRIDGE"
    echo "       Build it first: cd src && make EPICS_BASE=$EPICS_BASE bridge"
    exit 1
fi

if [ ! -d "$EPICS_BASE/lib/$EPICS_ARCH" ]; then
    echo "ERROR: EPICS libs not found at $EPICS_BASE/lib/$EPICS_ARCH"
    echo "       Check EPICS_BASE is set correctly."
    exit 1
fi

echo "=========================================="
echo " LAMPS EPICS Bridge"
echo "=========================================="
echo "  EPICS_BASE : $EPICS_BASE"
echo "  Bridge     : $BRIDGE"
echo "  Args       : $*"
echo "=========================================="
echo "  Make sure softIoc and LAMPS are running."
echo "  Ctrl-C to stop."
echo ""

export LD_LIBRARY_PATH="$EPICS_BASE/lib/$EPICS_ARCH${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
export EPICS_CA_ADDR_LIST="${EPICS_CA_ADDR_LIST:-localhost}"
export EPICS_CA_AUTO_ADDR_LIST="${EPICS_CA_AUTO_ADDR_LIST:-YES}"

exec "$BRIDGE" "$@"
