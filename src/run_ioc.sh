#!/bin/bash
# =========================================================================
# run_ioc.sh — Start the LAMPS EPICS softIoc
#
# Requires EPICS base to be installed and `softIoc` in the PATH.
# =========================================================================

PREFIX="LAMPS:"

if ! command -v softIoc &> /dev/null; then
    echo "Error: softIoc not found in PATH."
    echo "Make sure EPICS base is installed and sourced (e.g., . /usr/local/epics/base/set_epics_env.sh)"
    exit 1
fi

echo "Starting LAMPS EPICS softIoc with prefix ${PREFIX}..."
softIoc -m "P=${PREFIX}" -d lamps.db
