# LAMPS DAQ & EPICS Integration

This repository contains the legacy LAMPS Data Acquisition software for the CMC100 CAMAC Controller, newly modernized and integrated with **EPICS (Experimental Physics and Industrial Control System)**.

This integration uses a lockless **POSIX Shared Memory** architecture (a Seqlock) to allow the high-speed LAMPS DAQ to stream live physics data to an EPICS network without ever slowing down or blocking the data acquisition process.

## 1. Automated Installation (Recommended)

To get all the necessary dependencies, libraries, and EPICS base installed on a brand new Ubuntu/Debian machine, simply run the automated setup script:

```bash
chmod +x setup_environment.sh
./setup_environment.sh
```

**What the script does:**
1. Installs all required Linux packages (GCC, GTK2, Fortran, LibUSB, etc.).
2. Downloads the official `epics-base` from GitHub.
3. Compiles the EPICS core libraries (this takes a few minutes).

**After the script finishes:**
Be sure to add the EPICS tools to your environment variables as instructed by the script. (You can add the export lines to your `~/.bashrc`).

## 2. Compiling LAMPS & The EPICS Bridge

Once EPICS is installed and your environment variables are set, compile the DAQ software:

```bash
cd src
make EPICS_BASE=$HOME/epics/base
cd ..
```

*(If you do not pass `EPICS_BASE=...` to `make`, it will compile the core LAMPS DAQ but skip building the EPICS network bridge).*

## 3. How to Run the System

To run the full EPICS integration, you need three processes running simultaneously. You can run them in three separate terminals (or use `screen`/`tmux`).

### Terminal 1: Run the EPICS Database Server (`softIoc`)
This hosts the Process Variables (PVs) on the network.
```bash
softIoc -m "P=LAMPS:" -d src/lamps.db
```

### Terminal 2: Run the DAQ (LAMPS)
Start the actual data acquisition engine. It will own the hardware and stream data into Shared Memory.
```bash
./lamps
```
*(For testing without hardware, you can run `./lamps --simulator` or `./src/sim_shm`).*

### Terminal 3: Run the EPICS Bridge Publisher
This acts as the middleman, pulling data out of the Shared Memory and publishing it to the `softIoc`.
```bash
LD_LIBRARY_PATH=$HOME/epics/base/lib/linux-x86_64 ./src/lamps_epics_bridge
```
*(Add the `-v` flag if you want to see live terminal printouts of the event rates).*

## 4. Viewing the Data

With all three running, any PC on the network can view the live data using standard EPICS Channel Access tools or graphical clients like LabVIEW or CS-Studio Phoebus.

From any terminal, type:
```bash
# Watch the total events climb live
camonitor LAMPS:TOTAL_EVENTS

# Check the current status of the DAQ
caget LAMPS:STATUS

# Read the first 10 channels of the live 1D spectrum
caget -c 10 LAMPS:SPEC1D:001
```

## System Architecture Details
* **LAMPS Driver Layer:** Abstracted into `libcamac` for safe user-space hardware interactions.
* **Data Drop-Box:** `/dev/shm/lamps_daq_shm` is used as a zero-copy POSIX shared memory ring.
* **Concurrency:** A Seqlock pattern (`generation` counter) ensures the EPICS Bridge never blocks the LAMPS DAQ from acquiring data, guaranteeing no torn reads and no hardware lockups.
