# LAMPS DAQ & EPICS Integration

[![Ubuntu 20.04](https://img.shields.io/badge/Ubuntu-20.04-green?logo=ubuntu)](.)
[![Ubuntu 22.04](https://img.shields.io/badge/Ubuntu-22.04-green?logo=ubuntu)](.)
[![Ubuntu 24.04](https://img.shields.io/badge/Ubuntu-24.04-green?logo=ubuntu)](.)

This repository contains the legacy LAMPS Data Acquisition software for the CMC100 CAMAC Controller, modernized and integrated with **EPICS (Experimental Physics and Industrial Control System)**.

The integration uses a lockless **POSIX Shared Memory** architecture (a Seqlock) to allow the high-speed LAMPS DAQ to stream live physics data to an EPICS network without ever blocking or slowing down data acquisition.

---

## ⚡ One-Command Install (Recommended)

Clone the repo and run the automated setup script — it handles **everything**: system packages, EPICS compilation, Python virtualenv, and building all binaries.

```bash
git clone https://github.com/aryansingh012020-beep/TIFR_PLF_CAMAC.git
cd TIFR_PLF_CAMAC
chmod +x setup.sh
./setup.sh
```

Then apply the new environment variables:

```bash
source ~/.bashrc
```

> **Compatible with:** Ubuntu 20.04, 22.04, 24.04 LTS, Debian 11/12.  
> **Re-running is safe** — every step is idempotent (skip-if-already-done).

### What `setup.sh` does, step-by-step

| Step | Action |
|------|--------|
| 1 | Updates `apt` and installs GCC, GTK2, Fortran, libUSB, Python 3, pip, etc. |
| 2 | Clones EPICS Base R7.0.7 from GitHub and compiles it (≈3–5 min) |
| 3 | Writes `EPICS_BASE`, `EPICS_HOST_ARCH`, and `PATH` exports to `~/.bashrc` |
| 4 | Compiles the full LAMPS suite (`lamps`, `camacctl`, `sim_shm`, EPICS bridge) |
| 5 | Creates a Python virtualenv (`.venv/`) and installs the dashboard packages |
| 6 | Fixes executable permissions on all shell scripts |
| 7 | Runs smoke tests and prints a getting-started summary |

---

## 🚀 Running the System

You need **three processes** running simultaneously. Use three terminal tabs or `tmux`/`screen`.

### Terminal 1 — Physics Data Simulator (no hardware needed)
```bash
./sim_shm
```
*(To use real hardware instead, run `./lamps`)*

### Terminal 2 — EPICS IOC (Process Variable server)
```bash
softIoc -m "P=LAMPS:" -d src/lamps.db
```

### Terminal 3 — EPICS Bridge (Shared Memory → EPICS publisher)
```bash
./run_bridge.sh -v
```

### GUI Dashboard
```bash
source .venv/bin/activate
python3 tools/lamps_dashboard.py
```

---

## 📡 Viewing Live Data

With all three processes running, any PC on the network can monitor data:

```bash
# Watch total event count climb live
camonitor LAMPS:TOTAL_EVENTS

# Check DAQ status
caget LAMPS:STATUS

# Read the first 10 channels of the live 1D spectrum
caget -c 10 LAMPS:SPEC1D:001
```

---

## 🏗️ Manual Build (Advanced)

If you prefer to build manually after installing EPICS yourself:

```bash
cd src
make -j$(nproc) EPICS_BASE=$HOME/epics/base
```

Omitting `EPICS_BASE=...` builds everything except the EPICS bridge.

---

## System Architecture

```
  ┌──────────────┐  seqlock   ┌──────────────────────┐  Channel Access  ┌──────────┐
  │  LAMPS / DAQ │ ─────────► │  lamps_epics_bridge  │ ───────────────► │ softIoc  │
  │  (hardware)  │  /dev/shm  │  (subscriber process)│                  │  (EPICS) │
  └──────────────┘            └──────────────────────┘                  └──────────┘
        │                                                                      │
        └── sim_shm (simulator, no hardware needed) ───────────────────────────┘
```

* **LAMPS Driver Layer:** Abstracted into `libcamac` for safe user-space hardware interactions.
* **Data Drop-Box:** `/dev/shm/lamps_daq_shm` — zero-copy POSIX shared memory ring buffer.
* **Concurrency:** Seqlock pattern (`generation` counter) guarantees no torn reads and no hardware lockups.
* **Python Dashboard:** `tools/lamps_dashboard.py` — live PyQtGraph GUI for spectrum visualization.
