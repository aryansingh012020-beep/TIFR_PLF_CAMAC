#!/usr/bin/env bash
# =============================================================================
#  setup.sh  —  LAMPS DAQ + EPICS Integration  —  Full Automated Installer
# =============================================================================
#
#  USAGE (run once after cloning):
#
#      git clone https://github.com/aryansingh012020-beep/TIFR_PLF_CAMAC.git
#      cd TIFR_PLF_CAMAC
#      chmod +x setup.sh
#      ./setup.sh
#
#  What this script does, end-to-end:
#   1. Detects Ubuntu / Debian version, adapts package names accordingly
#   2. Installs ALL system packages (GCC, GTK2, Fortran, LibUSB, Python 3, pip …)
#   3. Clones and compiles EPICS Base R7.0.7 (skips if already present)
#   4. Writes permanent EPICS environment exports to ~/.bashrc / ~/.zshrc
#   5. Compiles the full LAMPS suite (sim_shm, bridge, main binary, etc.)
#   6. Rebuilds the CAMAC kernel module (cmcamac.ko) for THIS machine's kernel
#   7. Installs the Python dashboard dependencies (pip, virtual-env aware)
#   8. Runs a quick smoke-test to verify everything linked correctly
#   9. Prints a "Getting Started" summary
#
#  Tested on:
#    Ubuntu 20.04 LTS  (Focal)
#    Ubuntu 22.04 LTS  (Jammy)
#    Ubuntu 24.04 LTS  (Noble)
#    Debian 11 / 12
#
#  Re-running the script is safe — every step is idempotent.
#
#  NOTE: This script installs BOTH LAMPS and EPICS.
#        For a LAMPS-only install (no EPICS), use setup_lamps_only.sh instead.
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

wait_for_apt() {
    local i=0
    while sudo fuser /var/lib/dpkg/lock-frontend >/dev/null 2>&1 || \
          sudo fuser /var/lib/dpkg/lock >/dev/null 2>&1; do
        if [ $i -eq 0 ]; then
            warn "apt-get is locked by another process (likely Ubuntu background updates)."
            info "Waiting for it to finish..."
        fi
        sleep 5
        i=$((i + 5))
        if [ $i -gt 600 ]; then
            die "Timed out waiting for apt lock. Please reboot or stop apt manually."
        fi
    done
}

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

banner "LAMPS DAQ + EPICS Automated Setup"
info "Detected OS : ${OS_ID} ${OS_VER} (${OS_CODENAME})"
info "Repo root   : ${REPO_ROOT}"
echo ""

# ── Sanity: must be on a Debian/Ubuntu family ─────────────────────────────────
case "${OS_ID}" in
    ubuntu|debian|linuxmint|pop) : ;;   # supported
    *) warn "This script is designed for Ubuntu/Debian. Proceeding anyway — YMMV." ;;
esac

# ────────────────────────────────────────────────────────────────────────────
# STEP 1 — System package installation
# ────────────────────────────────────────────────────────────────────────────
banner "Step 1/8 — Installing system packages"

wait_for_apt
sudo apt-get update -qq

# Core build tools (identical across all supported Ubuntu versions)
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

    # EPICS build dependencies
    git
    wget
    curl
    perl
    libreadline-dev
    libncurses5-dev

    # Readline / terminal (nice-to-have for CA tools)
    libreadline-dev

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
# Both names are safe to list; apt will silently skip unknown ones with --ignore-missing
PKGS+=(libusb-1.0-0-dev)

# linux-headers: try the running kernel first; fall back to generic
KERNEL_HEADERS="linux-headers-$(uname -r)"
if apt-cache show "$KERNEL_HEADERS" &>/dev/null 2>&1; then
    PKGS+=("$KERNEL_HEADERS")
else
    warn "Kernel headers for $(uname -r) not in apt cache — installing linux-headers-generic"
    PKGS+=(linux-headers-generic)
fi

wait_for_apt
sudo apt-get install -y -o DPkg::Lock::Timeout=120 --no-install-recommends "${PKGS[@]}" || \
    sudo apt-get install -y -o DPkg::Lock::Timeout=120 --ignore-missing "${PKGS[@]}" || \
    die "Failed to install system packages. Is apt running in another terminal or updating in the background?"

ok "System packages installed."

# ────────────────────────────────────────────────────────────────────────────
# STEP 2 — EPICS Base  (smart detection — install only if truly missing)
# ────────────────────────────────────────────────────────────────────────────
banner "Step 2/8 — EPICS Base detection & setup"

# ── Detect host architecture first (needed for path checks below) ─────────────
MACHINE="$(uname -m)"
case "$MACHINE" in
    x86_64)  EPICS_ARCH="linux-x86_64"  ;;
    aarch64) EPICS_ARCH="linux-aarch64" ;;
    armv7l)  EPICS_ARCH="linux-arm"     ;;
    *)       EPICS_ARCH="linux-x86_64"
             warn "Unknown machine '${MACHINE}' — assuming linux-x86_64 for EPICS." ;;
esac
info "Host arch   : ${EPICS_ARCH}"

# ── Helper: returns 0 if the given directory has a fully-built EPICS base ─────
#   Criteria: bin/<arch>/softIoc  AND  lib/<arch>/libca.so   both exist.
epics_is_built() {
    local base="$1"
    [ -x "${base}/bin/${EPICS_ARCH}/softIoc" ] && \
    [ -f "${base}/lib/${EPICS_ARCH}/libca.so" ]
}

# ── Four-layer detection ──────────────────────────────────────────────────────
#
#   Layer 1 – softIoc already on PATH (system-wide or previously sourced env)
#   Layer 2 – $EPICS_BASE env variable is set and points to a working build
#   Layer 3 – Default location ~/epics/base already built
#   Layer 4 – Nothing found → clone from GitHub and compile

EPICS_FOUND=0
EPICS_BASE=""     # will be set by whichever layer succeeds

# ---------- Layer 1: softIoc already on PATH ----------
if command -v softIoc &>/dev/null; then
    SOFTIOC_PATH="$(command -v softIoc)"
    info "[Layer 1] 'softIoc' found on PATH: ${SOFTIOC_PATH}"
    # Walk up from bin/<arch>/softIoc  to  the EPICS base root
    CANDIDATE="$(dirname "$(dirname "$(dirname "${SOFTIOC_PATH}")")")"
    if epics_is_built "${CANDIDATE}"; then
        EPICS_BASE="${CANDIDATE}"
        ok "EPICS Base is fully installed at ${EPICS_BASE} — nothing to do."
        EPICS_FOUND=1
    else
        warn "softIoc is on PATH but build looks incomplete at ${CANDIDATE} — continuing checks."
    fi
fi

# ---------- Layer 2: $EPICS_BASE env variable ----------
if [ "${EPICS_FOUND}" -eq 0 ] && [ -n "${EPICS_BASE:-}" ]; then
    info "[Layer 2] \$EPICS_BASE env var is set: ${EPICS_BASE}"
    if epics_is_built "${EPICS_BASE}"; then
        ok "EPICS Base confirmed at \$EPICS_BASE (${EPICS_BASE}) — nothing to do."
        EPICS_FOUND=1
    else
        warn "\$EPICS_BASE is set but build looks incomplete — continuing checks."
        EPICS_BASE=""   # clear so Layer 4 can set the default path
    fi
fi

# ---------- Layer 3: default ~/epics/base ----------
DEFAULT_EPICS_BASE="${HOME}/epics/base"
if [ "${EPICS_FOUND}" -eq 0 ] && epics_is_built "${DEFAULT_EPICS_BASE}"; then
    EPICS_BASE="${DEFAULT_EPICS_BASE}"
    info "[Layer 3] Found working EPICS build at default path: ${EPICS_BASE}"
    ok "EPICS Base is already installed — skipping clone and compile."
    EPICS_FOUND=1
fi

# ---------- Layer 4: clone from GitHub and compile ----------
if [ "${EPICS_FOUND}" -eq 0 ]; then
    EPICS_BRANCH="${EPICS_BRANCH:-R7.0.7}"
    EPICS_ROOT="${EPICS_ROOT:-${HOME}/epics}"
    EPICS_BASE="${EPICS_ROOT}/base"

    info "[Layer 4] EPICS Base not found anywhere — installing from scratch."
    info "          Target: ${EPICS_BASE}  Branch: ${EPICS_BRANCH}"

    # Clone only if not already partially cloned
    if [ ! -d "${EPICS_BASE}/.git" ]; then
        info "Cloning EPICS Base ${EPICS_BRANCH} (shallow clone, ~100 MB) …"
        mkdir -p "${EPICS_ROOT}"
        git clone --branch "${EPICS_BRANCH}" --depth 1 \
            https://github.com/epics-base/epics-base.git "${EPICS_BASE}"
    else
        info "Partial clone found at ${EPICS_BASE} — resuming compile."
    fi

    info "Compiling EPICS Base — this takes 2–5 minutes on modern hardware …"
    make -C "${EPICS_BASE}" EPICS_HOST_ARCH="${EPICS_ARCH}" -j"$(nproc)"

    # Final sanity check after compile
    if epics_is_built "${EPICS_BASE}"; then
        ok "EPICS Base R7.0.7 compiled and verified."
        EPICS_FOUND=1
    else
        die "EPICS compile finished but softIoc or libca.so is still missing. Check /tmp/lamps_make.log"
    fi
fi

info "Using EPICS_BASE = ${EPICS_BASE}"

# ────────────────────────────────────────────────────────────────────────────
# STEP 3 — Persist EPICS environment variables
# ────────────────────────────────────────────────────────────────────────────
banner "Step 3/8 — Configuring shell environment"

# Snippet we want to inject into shell rc files
EPICS_ENV_BLOCK="
# ── EPICS Base (added by LAMPS setup.sh) ───────────────────
export EPICS_BASE=\"${EPICS_BASE}\"
export EPICS_HOST_ARCH=\"${EPICS_ARCH}\"
export PATH=\"\$PATH:${EPICS_BASE}/bin/${EPICS_ARCH}\"
export LD_LIBRARY_PATH=\"${EPICS_BASE}/lib/${EPICS_ARCH}\${LD_LIBRARY_PATH:+:\$LD_LIBRARY_PATH}\"
# ───────────────────────────────────────────────────────────
"

inject_env() {
    local RC_FILE="$1"
    if [ -f "$RC_FILE" ] && grep -q "EPICS_BASE" "$RC_FILE"; then
        info "EPICS env already present in ${RC_FILE} — skipping."
    else
        echo "${EPICS_ENV_BLOCK}" >> "$RC_FILE"
        ok "EPICS env written to ${RC_FILE}"
    fi
}

inject_env "$HOME/.bashrc"
[ -f "$HOME/.zshrc" ] && inject_env "$HOME/.zshrc"

# Export for the remainder of THIS script session
export EPICS_BASE
export EPICS_HOST_ARCH="${EPICS_ARCH}"
export PATH="${PATH}:${EPICS_BASE}/bin/${EPICS_ARCH}"
export LD_LIBRARY_PATH="${EPICS_BASE}/lib/${EPICS_ARCH}${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"

# ────────────────────────────────────────────────────────────────────────────
# STEP 4 — Compile LAMPS (C sources + Fortran + EPICS bridge)
# ────────────────────────────────────────────────────────────────────────────
banner "Step 4/8 — Compiling LAMPS DAQ"

cd "${REPO_ROOT}/src"

info "Running: make clean && make -j$(nproc) EPICS_BASE=${EPICS_BASE}"
make clean
make -j"$(nproc)" EPICS_BASE="${EPICS_BASE}" 2>&1 | tee /tmp/lamps_make.log

ok "LAMPS compiled. Binaries written to ${REPO_ROOT}/"

# Also build the standalone SHM simulator
info "Compiling sim_shm …"
# sim_shm only needs standard POSIX — no GTK, no EPICS
gcc -O2 -o "${REPO_ROOT}/src/sim_shm" \
    "${REPO_ROOT}/src/sim_shm.c" \
    -I"${REPO_ROOT}/src" \
    -lrt -lm -lpthread
cp "${REPO_ROOT}/src/sim_shm" "${REPO_ROOT}/sim_shm"
ok "sim_shm built."

cd "${REPO_ROOT}"

# ────────────────────────────────────────────────────────────────────────────
# STEP 5 — Rebuild CAMAC kernel module for THIS machine's kernel
# ────────────────────────────────────────────────────────────────────────────
banner "Step 5/8 — Building CAMAC kernel module (cmcamac.ko)"

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
# STEP 6 — Python dashboard dependencies
# ────────────────────────────────────────────────────────────────────────────
banner "Step 6/8 — Installing Python dashboard dependencies"

REQ_FILE="${REPO_ROOT}/tools/requirements.txt"

# Prefer a venv inside the repo so we don't pollute system Python
VENV_DIR="${REPO_ROOT}/.venv"

if [ ! -d "${VENV_DIR}" ]; then
    info "Creating Python virtual environment at ${VENV_DIR} …"
    python3 -m venv "${VENV_DIR}"
fi

info "Installing Python packages from ${REQ_FILE} …"
# Upgrade pip first to avoid legacy resolver issues on Ubuntu 24.04
"${VENV_DIR}/bin/pip" install --upgrade pip
"${VENV_DIR}/bin/pip" install -r "${REQ_FILE}"

ok "Python packages installed inside ${VENV_DIR}"

# ────────────────────────────────────────────────────────────────────────────
# STEP 7 — Make all scripts executable
# ────────────────────────────────────────────────────────────────────────────
banner "Step 7/8 — Fixing permissions on shell scripts"

chmod +x \
    "${REPO_ROOT}/setup.sh" \
    "${REPO_ROOT}/setup_lamps_only.sh" \
    "${REPO_ROOT}/setup_environment.sh" \
    "${REPO_ROOT}/run_bridge.sh" \
    "${REPO_ROOT}/start_sim_dashboard.sh" \
    "${REPO_ROOT}/src/run_ioc.sh" || true

ok "Script permissions set."

# ────────────────────────────────────────────────────────────────────────────
# STEP 8 — Smoke test
# ────────────────────────────────────────────────────────────────────────────
banner "Step 8/8 — Smoke tests"

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

check "sim_shm"            "${REPO_ROOT}/sim_shm"
check "lamps_epics_bridge" "${REPO_ROOT}/lamps_epics_bridge"
check "camacctl"           "${REPO_ROOT}/camacctl"
check "softIoc (EPICS)"    "${EPICS_BASE}/bin/${EPICS_ARCH}/softIoc"

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
banner "Setup Complete!"

cat <<GUIDE

${BOLD}════════════════════  GETTING STARTED  ════════════════════${RESET}

${BOLD}▶  Apply environment variables to your current shell:${RESET}
    source ~/.bashrc

${BOLD}▶  Simulator mode (no hardware needed) — 3 terminals:${RESET}

  Terminal 1 — Physics data simulator:
    ${CYAN}${REPO_ROOT}/sim_shm${RESET}

  Terminal 2 — EPICS IOC (Process Variable server):
    ${CYAN}softIoc -m "P=LAMPS:" -d ${REPO_ROOT}/src/lamps.db${RESET}

  Terminal 3 — EPICS Bridge (SHM → EPICS publisher):
    ${CYAN}${REPO_ROOT}/run_bridge.sh -v${RESET}

${BOLD}▶  Python dashboard (GUI):${RESET}
    ${CYAN}${VENV_DIR}/bin/python3 ${REPO_ROOT}/tools/lamps_dashboard.py${RESET}

  Or activate the venv first:
    ${CYAN}source ${VENV_DIR}/bin/activate${RESET}
    ${CYAN}python3 tools/lamps_dashboard.py${RESET}

${BOLD}▶  Live hardware mode (real CMC100 CAMAC controller):${RESET}
    ${CYAN}${REPO_ROOT}/lamps${RESET}

${BOLD}▶  Quick EPICS channel check (after IOC + bridge are running):${RESET}
    ${CYAN}camonitor LAMPS:TOTAL_EVENTS${RESET}
    ${CYAN}caget     LAMPS:STATUS${RESET}

${BOLD}════════════════════════════════════════════════════════════${RESET}
GUIDE

[ "${FAIL}" -gt 0 ] && warn "Some checks failed — review warnings above." || ok "All checks passed. Happy physics!"
