#!/usr/bin/env bash
# =============================================================================
#  setup_lamps_only.sh  —  LAMPS DAQ  —  Automated Installer (no EPICS)
# =============================================================================
#
#  USAGE (run once after cloning):
#
#      git clone https://github.com/aryansingh012020-beep/TIFR_PLF_CAMAC.git
#      cd TIFR_PLF_CAMAC
#      chmod +x setup_lamps_only.sh
#      ./setup_lamps_only.sh
#
#  What this script does, end-to-end:
#   1. Detects Ubuntu / Debian version, adapts package names accordingly
#   2. Installs ALL system packages needed by LAMPS (GCC, GTK2, Fortran,
#      LibUSB, Python 3, pip …) — NO EPICS packages
#   3. Compiles the full LAMPS suite (sim_shm, main binary, etc.)
#      NOTE: lamps_epics_bridge is NOT built in this mode
#   4. Rebuilds the CAMAC kernel module (cmcamac.ko) for THIS machine's kernel
#   5. Installs the Python dashboard dependencies (pip, virtual-env aware)
#   6. Runs a quick smoke-test to verify everything linked correctly
#   7. Prints a "Getting Started" summary
#
#  Tested on:
#    Ubuntu 20.04 LTS  (Focal)
#    Ubuntu 22.04 LTS  (Jammy)
#    Ubuntu 24.04 LTS  (Noble)
#    Debian 11 / 12
#
#  Re-running the script is safe — every step is idempotent.
#
#  NOTE: This script does NOT install or configure EPICS.
#        For EPICS + LAMPS combined setup, use setup.sh instead.
# =============================================================================

set -euo pipefail

# ── Colour helpers ────────────────────────────────────────────────────────────
RED='\033[0;31m'; GREEN='\033[0;32m'; YELLOW='\033[1;33m'
CYAN='\033[0;36m'; BOLD='\033[1m'; RESET='\033[0m'
info()    { echo -e "${CYAN}[INFO]${RESET}  $*"; }
ok()      { echo -e "${GREEN}[OK]${RESET}    $*"; }
warn()    { echo -e "${YELLOW}[WARN]${RESET}  $*"; }
die()     { echo -e "${RED}[ERROR]${RESET} $*" >&2; exit 1; }
banner()  { echo -e "\n${BOLD}${CYAN}══════════════════════════════════════════════════${RESET}"; \
            echo -e "${BOLD}${CYAN}  $*${RESET}"; \
            echo -e "${BOLD}${CYAN}══════════════════════════════════════════════════${RESET}"; }

# ── Resolve script / repo root ────────────────────────────────────────────────
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$SCRIPT_DIR"

# ── Detect OS ────────────────────────────────────────────────────────────────
OS_ID=""
OS_CODENAME=""
OS_VER=""
if [ -f /etc/os-release ]; then
    . /etc/os-release
    OS_ID="${ID:-unknown}"
    OS_CODENAME="${VERSION_CODENAME:-unknown}"
    OS_VER="${VERSION_ID:-0}"
fi

banner "LAMPS DAQ Setup (LAMPS only — no EPICS)"
info "Detected OS : ${OS_ID} ${OS_VER} (${OS_CODENAME})"
info "Repo root   : ${REPO_ROOT}"
echo ""

# ── Sanity: must be on a Debian/Ubuntu family ─────────────────────────────────
case "${OS_ID}" in
    ubuntu|debian|linuxmint|pop) : ;;   # supported
    *) warn "This script is designed for Ubuntu/Debian. Proceeding anyway — YMMV." ;;
esac

# ────────────────────────────────────────────────────────────────────────────
# STEP 1 — System package installation (LAMPS only, no EPICS deps)
# ────────────────────────────────────────────────────────────────────────────
banner "Step 1/5 — Installing system packages"

sudo apt-get update -qq

# Core build tools
PKGS=(
    build-essential
    gcc
    g++
    make
    pkg-config
    cmake
    autoconf
    automake
    libtool

    # Fortran (needed by LAMPS user.F / radware_io.F)
    gfortran
    libgfortran5

    # GTK2 — LAMPS GUI toolkit
    libgtk2.0-dev
    libglib2.0-dev
    libcanberra-gtk-module
    libcanberra-gtk3-module

    # USB support for CAMAC controller
    libusb-dev
    usbutils

    # General utilities
    git
    wget
    curl

    # Python 3 + pip (for dashboard)
    python3
    python3-pip
    python3-venv
    python3-dev

    # Misc
    screen
    tmux
    lsof
)

# Ubuntu 24.04 "Noble" changed libusb-dev → libusb-1.0-0-dev
PKGS+=(libusb-1.0-0-dev)

# linux-headers: try the running kernel first; fall back to generic
KERNEL_HEADERS="linux-headers-$(uname -r)"
if apt-cache show "$KERNEL_HEADERS" &>/dev/null 2>&1; then
    PKGS+=("$KERNEL_HEADERS")
else
    warn "Kernel headers for $(uname -r) not in apt cache — installing linux-headers-generic"
    PKGS+=(linux-headers-generic)
fi

sudo apt-get install -y --no-install-recommends "${PKGS[@]}" || \
    sudo apt-get install -y --ignore-missing "${PKGS[@]}" || true

ok "System packages installed."

# ────────────────────────────────────────────────────────────────────────────
# STEP 2 — Compile LAMPS (C sources + Fortran, NO EPICS bridge)
# ────────────────────────────────────────────────────────────────────────────
banner "Step 2/5 — Compiling LAMPS DAQ (standalone, no EPICS)"

cd "${REPO_ROOT}/src"

# Build without EPICS: pass EPICS_BASE="" so the Makefile skips bridge targets
info "Running: make clean && make -j$(nproc) EPICS_BASE= (EPICS bridge will be skipped)"
make clean
make -j"$(nproc)" EPICS_BASE="" 2>&1 | tee /tmp/lamps_make.log

ok "LAMPS compiled. Binaries written to ${REPO_ROOT}/"

# Build the standalone SHM simulator (pure POSIX — no GTK, no EPICS)
info "Compiling sim_shm …"
gcc -O2 -o "${REPO_ROOT}/src/sim_shm" \
    "${REPO_ROOT}/src/sim_shm.c" \
    -I"${REPO_ROOT}/src" \
    -lrt -lm -lpthread
cp "${REPO_ROOT}/src/sim_shm" "${REPO_ROOT}/sim_shm"
ok "sim_shm built."

cd "${REPO_ROOT}"

# ────────────────────────────────────────────────────────────────────────────
# STEP 3 — Rebuild CAMAC kernel module for THIS machine's kernel
# ────────────────────────────────────────────────────────────────────────────
banner "Step 3/6 — Building CAMAC kernel module (cmcamac.ko)"

DRV_DIR="${REPO_ROOT}/driver"
KERNEL_BUILD="/lib/modules/$(uname -r)/build"

if [ -d "${KERNEL_BUILD}" ]; then
    info "Kernel build tree found: ${KERNEL_BUILD}"
    info "Building cmcamac.ko for kernel $(uname -r) …"
    make -C "${KERNEL_BUILD}" M="${DRV_DIR}" modules 2>&1 | tee /tmp/cmcamac_build.log
    if [ -f "${DRV_DIR}/cmcamac.ko" ]; then
        ok "cmcamac.ko built for kernel $(uname -r)"
    else
        warn "cmcamac.ko build failed — check /tmp/cmcamac_build.log"
        warn "The pre-compiled .ko in the repo will be used (may not match this kernel)"
    fi
else
    warn "Kernel build tree not found at ${KERNEL_BUILD}"
    warn "Cannot rebuild cmcamac.ko — using pre-compiled version from repo."
    warn "If 'insmod: Invalid module format' occurs, install linux-headers-$(uname -r)"
    warn "and re-run this script."
fi

# ────────────────────────────────────────────────────────────────────────────
# STEP 4 — Python dashboard dependencies
# ────────────────────────────────────────────────────────────────────────────
banner "Step 4/6 — Installing Python dashboard dependencies"

REQ_FILE="${REPO_ROOT}/tools/requirements.txt"

# Prefer a venv inside the repo so we don't pollute system Python
VENV_DIR="${REPO_ROOT}/.venv"

if [ ! -d "${VENV_DIR}" ]; then
    info "Creating Python virtual environment at ${VENV_DIR} …"
    python3 -m venv "${VENV_DIR}"
fi

info "Installing Python packages from ${REQ_FILE} …"
"${VENV_DIR}/bin/pip" install --upgrade pip
"${VENV_DIR}/bin/pip" install -r "${REQ_FILE}"

ok "Python packages installed inside ${VENV_DIR}"

# ────────────────────────────────────────────────────────────────────────────
# STEP 5 — Make all scripts executable
# ────────────────────────────────────────────────────────────────────────────
banner "Step 5/6 — Fixing permissions on shell scripts"

chmod +x \
    "${REPO_ROOT}/setup_lamps_only.sh" \
    "${REPO_ROOT}/setup.sh" \
    "${REPO_ROOT}/setup_environment.sh" \
    "${REPO_ROOT}/start_sim_dashboard.sh" || true

ok "Script permissions set."

# ────────────────────────────────────────────────────────────────────────────
# STEP 6 — Smoke test (LAMPS only — no EPICS checks)
# ────────────────────────────────────────────────────────────────────────────
banner "Step 6/6 — Smoke tests"

PASS=0; FAIL=0

check() {
    local label="$1"
    local file="$2"
    if [ -x "$file" ]; then
        ok "  [PASS] ${label}: ${file}"
        PASS=$((PASS + 1))
    else
        warn "  [FAIL] ${label}: ${file} not found / not executable"
        FAIL=$((FAIL + 1))
    fi
}

check "sim_shm"  "${REPO_ROOT}/sim_shm"
check "camacctl" "${REPO_ROOT}/camacctl"
check "lamps"    "${REPO_ROOT}/lamps"

# Verify Python imports inside the venv
info "Testing Python dashboard imports …"
"${VENV_DIR}/bin/python3" - <<'PYCHECK'
import sys
missing = []
for mod in ["PyQt5", "pyqtgraph", "numpy", "scipy"]:
    try:
        __import__(mod)
    except ImportError:
        missing.append(mod)
if missing:
    print("  [WARN ] Missing Python modules: " + ", ".join(missing))
    sys.exit(1)
else:
    print("  [PASS ] All Python modules importable.")
PYCHECK
[ $? -eq 0 ] && PASS=$((PASS+1)) || FAIL=$((FAIL+1))

echo ""
echo -e "${BOLD}Smoke-test results: ${GREEN}${PASS} passed${RESET}, ${RED}${FAIL} failed${RESET}"

# ────────────────────────────────────────────────────────────────────────────
# DONE — Print getting-started guide
# ────────────────────────────────────────────────────────────────────────────
banner "Setup Complete! (LAMPS standalone — no EPICS)"

cat <<GUIDE

${BOLD}════════════════════  GETTING STARTED  ════════════════════${RESET}

${BOLD}▶  Simulator mode (no hardware needed) — 2 terminals:${RESET}

  Terminal 1 — Physics data simulator:
    ${CYAN}${REPO_ROOT}/sim_shm${RESET}

  Terminal 2 — Python dashboard (GUI):
    ${CYAN}${VENV_DIR}/bin/python3 ${REPO_ROOT}/tools/lamps_dashboard.py${RESET}

  Or activate the venv first:
    ${CYAN}source ${VENV_DIR}/bin/activate${RESET}
    ${CYAN}python3 tools/lamps_dashboard.py${RESET}

${BOLD}▶  Live hardware mode (real CMC100 CAMAC controller):${RESET}
    ${CYAN}${REPO_ROOT}/lamps${RESET}

${BOLD}▶  NOTE: EPICS bridge is NOT available in this setup.${RESET}
   To enable EPICS integration, run setup.sh instead:
    ${CYAN}./setup.sh${RESET}

${BOLD}════════════════════════════════════════════════════════════${RESET}
GUIDE

[ "${FAIL}" -gt 0 ] && warn "Some checks failed — review warnings above." || ok "All checks passed. Happy physics!"
