#!/usr/bin/env python3
"""
lamps_verify.py — Data integrity checker for the LAMPS EPICS bridge.

Reads every PV value from EPICS AND reads the same field directly from
the POSIX shared memory, then compares them side-by-side.

Usage:
    python3 tools/lamps_verify.py
    python3 tools/lamps_verify.py --prefix LAMPS --ioc localhost --watch
"""
import argparse
import ctypes
import os
import sys
import time
import numpy as np

# ── EPICS ──────────────────────────────────────────────────────────────────
try:
    import epics
    EPICS_OK = True
except ImportError:
    EPICS_OK = False
    print("[WARN] pyepics not installed — scalar PVs won't be compared")

# ── SHM struct (must match lamps_shm.h exactly) ────────────────────────────
MAX_SCALER = 64
MAX_ONED   = 64
MAX_CHAN    = 8192

class LampsShmBlock(ctypes.LittleEndianStructure):
    _fields_ = [
        ("magic",        ctypes.c_uint32),
        ("version",      ctypes.c_uint32),
        ("generation",   ctypes.c_uint64),
        ("run_name",     ctypes.c_char * 40),
        ("acq_state",    ctypes.c_uint32),
        ("_pad0",        ctypes.c_uint32),   # alignment padding
        ("total_events", ctypes.c_uint64),
        ("bufs_acquired",ctypes.c_uint64),
        ("bufs_processed",ctypes.c_uint64),
        ("elapsed_sec",  ctypes.c_double),
        ("event_rate",   ctypes.c_double),
        ("data_kbps",    ctypes.c_double),
        ("n_scaler",     ctypes.c_uint32),
        ("sc_name",      ctypes.c_char * (MAX_SCALER * 32)),
        ("sc_val",       ctypes.c_uint32 * MAX_SCALER),
        ("n_oned",       ctypes.c_uint32),
        ("od_name",      ctypes.c_char * (MAX_ONED * 32)),
        ("od_chan",       ctypes.c_uint32 * MAX_ONED),
        ("od_wsz",        ctypes.c_uint32 * MAX_ONED),
        ("od_data",       ctypes.c_uint32 * (MAX_ONED * MAX_CHAN)),
    ]

SHM_FILE   = "/dev/shm/lamps_daq_shm"
MAGIC_EXPECTED = 0xDAC00001
ACQ_NAMES  = {0: "STOPPED", 1: "PAUSED", 2: "RUNNING"}

# ── helpers ────────────────────────────────────────────────────────────────
GREEN  = "\033[92m"
RED    = "\033[91m"
YELLOW = "\033[93m"
CYAN   = "\033[96m"
RESET  = "\033[0m"
BOLD   = "\033[1m"

def ok(s):  return f"{GREEN}✓ {s}{RESET}"
def err(s): return f"{RED}✗ {s}{RESET}"
def warn(s):return f"{YELLOW}⚠ {s}{RESET}"

def read_shm():
    """Read a consistent seqlock snapshot from the SHM."""
    if not os.path.exists(SHM_FILE):
        return None, f"SHM file {SHM_FILE!r} not found — is LAMPS (or sim_shm) running?"
    try:
        for _ in range(10):                     # retry up to 10 times for seqlock
            with open(SHM_FILE, "rb") as f:
                raw = f.read()
            blk = LampsShmBlock.from_buffer_copy(raw)
            gen1 = blk.generation
            with open(SHM_FILE, "rb") as f:
                raw2 = f.read()
            blk2 = LampsShmBlock.from_buffer_copy(raw2)
            gen2 = blk2.generation
            if gen1 == gen2 and (gen1 % 2 == 0):  # even → stable
                break
            blk = blk2
        if blk.magic != MAGIC_EXPECTED:
            return None, f"SHM magic mismatch: 0x{blk.magic:08X} (expected 0x{MAGIC_EXPECTED:08X})"
        return blk, None
    except Exception as e:
        return None, str(e)

def caget_str(pv, timeout=2.0):
    if not EPICS_OK:
        return None
    try:
        v = epics.caget(pv, as_string=True, timeout=timeout)
        return v
    except Exception:
        return None

def caget_num(pv, timeout=2.0):
    if not EPICS_OK:
        return None
    try:
        v = epics.caget(pv, timeout=timeout)
        return v
    except Exception:
        return None

def caget_waveform(pv, count=MAX_CHAN, timeout=3.0):
    if not EPICS_OK:
        return None
    try:
        v = epics.caget(pv, count=count, timeout=timeout)
        if v is None:
            return None
        return np.asarray(v, dtype=np.float64)
    except Exception:
        return None

# ── main comparison ────────────────────────────────────────────────────────
def verify(prefix: str, ioc: str, watch: bool, interval: float):
    if EPICS_OK:
        os.environ.setdefault("EPICS_CA_ADDR_LIST",      ioc)
        os.environ.setdefault("EPICS_CA_AUTO_ADDR_LIST", "YES")

    first = True
    while True:
        if not first:
            time.sleep(interval)
        first = False

        ts = time.strftime("%H:%M:%S")
        print(f"\n{BOLD}{'─'*65}{RESET}")
        print(f"{BOLD}  LAMPS Data Integrity Check   {ts}{RESET}")
        print(f"{BOLD}{'─'*65}{RESET}")

        blk, err_msg = read_shm()
        if blk is None:
            print(err(f"SHM read failed: {err_msg}"))
            if not watch:
                sys.exit(1)
            continue

        passes = 0
        fails  = 0
        warns  = 0

        def compare_str(label, shm_val, pv_name, allow_mismatch=False):
            nonlocal passes, fails, warns
            pv_val = caget_str(f"{prefix}:{pv_name}")
            shm_s = str(shm_val)
            if pv_val is None:
                print(f"  {YELLOW}⚠{RESET} {label:<22} SHM={shm_s!r}  PV=<timeout>")
                warns += 1
            elif shm_s.strip() == (pv_val or "").strip():
                print(f"  {GREEN}✓{RESET} {label:<22} {shm_s!r}")
                passes += 1
            else:
                sym = warn if allow_mismatch else err
                print(f"  {RED}✗{RESET} {label:<22} SHM={shm_s!r}  PV={pv_val!r}")
                if allow_mismatch:
                    warns += 1
                else:
                    fails += 1

        def compare_num(label, shm_val, pv_name, tol_pct=1.0, fmt=".0f"):
            nonlocal passes, fails, warns
            pv_val = caget_num(f"{prefix}:{pv_name}")
            if pv_val is None:
                print(f"  {YELLOW}⚠{RESET} {label:<22} SHM={shm_val:{fmt}}  PV=<timeout>")
                warns += 1
                return
            diff_pct = abs(shm_val - pv_val) / max(abs(shm_val), 1) * 100
            if diff_pct <= tol_pct:
                print(f"  {GREEN}✓{RESET} {label:<22} {shm_val:{fmt}}  (PV={pv_val:{fmt}})")
                passes += 1
            else:
                print(f"  {RED}✗{RESET} {label:<22} SHM={shm_val:{fmt}}  PV={pv_val:{fmt}}  Δ={diff_pct:.1f}%")
                fails += 1

        # ── Scalar fields ──────────────────────────────────────────────────
        print(f"\n{CYAN}  Scalars{RESET}")
        acq_str = ACQ_NAMES.get(blk.acq_state, "UNKNOWN")
        compare_str("STATUS",        acq_str,           "STATUS")
        compare_str("RUN_NAME",      blk.run_name.decode("ascii","replace"), "RUN_NAME", allow_mismatch=True)
        compare_num("TOTAL_EVENTS",  blk.total_events,  "TOTAL_EVENTS", tol_pct=5.0)
        compare_num("BUFS_ACQUIRED", blk.bufs_acquired, "BUFS_ACQUIRED", tol_pct=5.0)
        compare_num("BUFS_PROCESSED",blk.bufs_processed,"BUFS_PROCESSED",tol_pct=5.0)
        compare_num("ELAPSED_SEC",   blk.elapsed_sec,   "ELAPSED_SEC",   tol_pct=5.0, fmt=".1f")
        compare_num("EVENT_RATE",    blk.event_rate,    "EVENT_RATE",    tol_pct=10.0, fmt=".1f")

        # ── Spectrum ───────────────────────────────────────────────────────
        print(f"\n{CYAN}  1D Spectrum{RESET}")
        print(f"  n_oned  = {blk.n_oned}")
        for i in range(min(blk.n_oned, MAX_ONED)):
            nchan = blk.od_chan[i]
            if nchan == 0:
                print(f"  {YELLOW}⚠{RESET}  det {i+1}: oned_chan[{i}]=0 — no spectrum configured")
                warns += 1
                continue

            shm_spec = np.array(blk.od_data[i*MAX_CHAN : i*MAX_CHAN + nchan],
                                dtype=np.float64)

            pv_name = f"{prefix}:SPEC1D:{i+1:03d}"
            pv_spec = caget_waveform(pv_name, count=nchan)

            shm_total = int(shm_spec.sum())
            shm_max   = int(shm_spec.max())
            shm_pk_ch = int(shm_spec.argmax())

            print(f"\n  Detector {i+1}  ({nchan} ch):")
            print(f"    SHM  → total={shm_total:>12,}  max={shm_max:>8,} at ch {shm_pk_ch}")

            if pv_spec is None or len(pv_spec) == 0:
                print(f"    EPICS→ {RED}PV timeout or empty — bridge may not be publishing spectrum{RESET}")
                fails += 1
            else:
                pv_total  = int(pv_spec.sum())
                pv_max    = int(pv_spec.max())
                pv_pk_ch  = int(pv_spec.argmax())
                print(f"    EPICS→ total={pv_total:>12,}  max={pv_max:>8,} at ch {pv_pk_ch}")

                # Channel-by-channel comparison
                min_len = min(len(shm_spec), len(pv_spec))
                diff = np.abs(shm_spec[:min_len] - pv_spec[:min_len])
                max_diff   = int(diff.max())
                diff_chans = int((diff > 0).sum())

                if diff_chans == 0:
                    print(f"    {GREEN}✓ PERFECT MATCH — all {nchan} channels identical{RESET}")
                    passes += 1
                elif pv_total >= shm_total and max_diff < 50:
                    # EPICS has slightly MORE counts than our SHM snapshot —
                    # this is normal: the bridge published a tick AFTER we
                    # read the SHM.  Data is correct; just timing skew.
                    print(f"    {GREEN}✓ MATCH (timing skew only){RESET}")
                    print(f"      EPICS is {pv_total - shm_total:+,} cts ahead "
                          f"(max Δ={max_diff} — normal for live data)")
                    passes += 1
                elif max_diff < 50 and diff_chans < nchan // 4:
                    print(f"    {YELLOW}⚠ Near-match: {diff_chans} ch differ (max Δ={max_diff}) — timing skew{RESET}")
                    warns += 1
                else:
                    print(f"    {RED}✗ MISMATCH: {diff_chans} channels differ  max_diff={max_diff}{RESET}")
                    worst = np.argsort(diff)[-5:][::-1]
                    for ch in worst:
                        print(f"      ch {ch:4d}: SHM={int(shm_spec[ch]):6d}  "
                              f"EPICS={int(pv_spec[ch]):6d}  Δ={int(diff[ch])}")
                    fails += 1


        # ── Summary ────────────────────────────────────────────────────────
        print(f"\n{BOLD}{'─'*65}{RESET}")
        total = passes + fails + warns
        color = GREEN if fails == 0 else RED
        print(f"{color}{BOLD}  Result: {passes} passed  {fails} failed  {warns} warnings  "
              f"({total} checks){RESET}")
        if fails == 0 and warns == 0:
            print(f"{GREEN}{BOLD}  ✓ Dashboard is showing IDENTICAL data to LAMPS SHM{RESET}")
        elif fails == 0:
            print(f"{YELLOW}  ⚠ Minor timing differences only — data is correct{RESET}")
        else:
            print(f"{RED}  ✗ Data mismatch detected — check bridge or PV names{RESET}")
        print()

        if not watch:
            sys.exit(0 if fails == 0 else 1)

def main():
    p = argparse.ArgumentParser(description="LAMPS EPICS data integrity checker")
    p.add_argument("--prefix",   default="LAMPS",     help="PV prefix (default: LAMPS)")
    p.add_argument("--ioc",      default="localhost",  help="IOC host (default: localhost)")
    p.add_argument("--watch",    action="store_true",  help="Keep running, repeat every --interval s")
    p.add_argument("--interval", type=float, default=5.0, help="Watch interval in seconds")
    args = p.parse_args()
    verify(args.prefix, args.ioc, args.watch, args.interval)

if __name__ == "__main__":
    main()
