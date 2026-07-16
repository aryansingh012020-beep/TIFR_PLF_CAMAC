# EPICS Channel Access (CA) Cheat Sheet

This document serves as a detailed reference guide for interacting with the EPICS network using the standard Channel Access command-line tools.

**Prerequisite:** 
Before using any of these commands, you must ensure that the EPICS binaries are in your system `PATH`. If you used the automated setup script, you can do this by running:
```bash
export PATH=$PATH:$HOME/epics/base/bin/linux-x86_64/
```

---

## 1. `caget` (Read a Process Variable)
`caget` asks the network for the current value of a Process Variable (PV) and immediately exits.

### Basic Usage
To get the current value of the event rate:
```bash
caget LAMPS:EVENT_RATE
```
**Example Output:** `LAMPS:EVENT_RATE   12.5`

### Advanced Flags
* **`-a` (Alarm Status):** Shows the timestamp and the current alarm severity. If the hardware is offline, it will show `INVALID` or `UDF` (Undefined).
  ```bash
  caget -a LAMPS:STATUS
  ```
  **Example Output:** `LAMPS:STATUS    2026-06-25 10:00:00.000  RUNNING   NO_ALARM`

* **`-c` (Count/Arrays):** By default, `caget` truncates large arrays (like 8,192-channel histograms). Use `-c` to specify exactly how many array elements you want to print.
  ```bash
  caget -c 10 LAMPS:SPEC1D:001
  ```
  **Example Output:** Prints the first 10 bins of the 1D spectrum.

---

## 2. `camonitor` (Watch a Live Stream)
Instead of typing `caget` repeatedly, `camonitor` subscribes to a PV. It will keep your terminal open and print a new line *only* when the value actually changes. 

*(To exit a camonitor stream, press `Ctrl+C`)*.

### Basic Usage
To watch the total events climbing in real-time during an experiment:
```bash
camonitor LAMPS:TOTAL_EVENTS
```
**Example Output:**
```text
LAMPS:TOTAL_EVENTS   2026-06-25 10:00:01   10500
LAMPS:TOTAL_EVENTS   2026-06-25 10:00:02   10650
LAMPS:TOTAL_EVENTS   2026-06-25 10:00:03   10800
```

---

## 3. `caput` (Write to a Process Variable)
`caput` allows you to send commands or change parameters on the DAQ/hardware. 
*(Note: You can only use `caput` on PVs that are configured to be writable. Attempting to write to a read-only PV like `TOTAL_EVENTS` will fail).*

### Basic Usage
To send a "Start" command to the DAQ:
```bash
caput LAMPS:CMD:START 1
```
**Example Output:**
```text
Old : LAMPS:CMD:START                0
New : LAMPS:CMD:START                1
```

To send a string to change the run name:
```bash
caput -s LAMPS:RUN_NAME "Calibration_Run_01"
```

---

## 4. `cainfo` (Inspect PV Metadata)
If you know a PV exists but need technical details about it (e.g., what data type it is, or which computer/IP address is hosting it), use `cainfo`.

### Basic Usage
To inspect the 1D spectrum array:
```bash
cainfo LAMPS:SPEC1D:001
```
**Example Output:**
```text
State:            CONNECTED
Host:             192.168.1.10:5064
Access:           read, write
Native data type: DBR_LONG
Request type:     DBR_LONG
Element count:    8192
```

---

## Summary of Useful LAMPS PVs
Here is a list of the primary PVs we created in `lamps.db` that you can test these commands on:

**Status & Stats:**
- `LAMPS:STATUS` (String: e.g., "RUNNING", "STOPPED")
- `LAMPS:RUN_NAME` (String)
- `LAMPS:TOTAL_EVENTS` (Integer)
- `LAMPS:EVENT_RATE` (Float)
- `LAMPS:BUFS_ACQUIRED` (Integer)
- `LAMPS:ELAPSED_SEC` (Integer)

**Data Arrays:**
- `LAMPS:SCALER01` through `LAMPS:SCALER64` (Integers)
- `LAMPS:SPEC1D:001` through `LAMPS:SPEC1D:064` (Waveform Arrays)

**Commands:**
- `LAMPS:CMD:START`
- `LAMPS:CMD:STOP`
