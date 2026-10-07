#!/usr/bin/env python3
# Copyright (c) 2026 Bertrand Hurst. Part of Escapement, distributed under the terms of
# LICENSE at the root of this repository.
#
"""Run BenchAdmissionPico on a Pico and print what the soft kernel's test of an optional
instance costs.

    tools/admission_cost.py path/to/BenchAdmissionPico.elf [...]

BenchAdmissionPico (make KERNEL=SOFT bench-admission) times in cycles, at 125 MHz, each
test the kernel makes of an optional instance, eight (m,k)-firm tasks running under EDF,
then raises AdmissionDone; BenchAdmissionCountPico does the same on the count the test
by demand replaced. The tool loads each image in turn, waits for that, reads Admission
without stopping a core, and prints the tests made and admitted, the cycles in the mean,
the most and the fewest, in microseconds too, and how they spread by powers of two.
Needs OpenOCD and the Pico's Debug Probe (PROBE, tools/probe.sh), and its lock on the
bench (tools/board_ci.md).
"""

import os
import re
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import read_trace   # noqa: E402

MHZ = 125


def words(address, count):
    out = read_trace.openocd([f"echo \"W [read_memory {address:#x} 32 {count}]\""])
    found = re.search(r"W ([0-9a-fx ]+)", out)
    if not found:
        sys.exit("could not read the board:\n" + out)
    return [int(w, 0) for w in found.group(1).split()]


def run(elf):
    sym = read_trace.symbols(elf)
    if "Admission" not in sym:
        sys.exit(f"{elf} is not BenchAdmissionPico")
    out = read_trace.openocd(["reset halt", f"load_image {elf}", "resume 0x20000000"])
    if "downloaded" not in out and "bytes written" not in out:
        sys.exit("could not load the image:\n" + out)
    for _ in range(60):
        time.sleep(2)
        if words(sym["AdmissionDone"], 1)[0] == 1:
            break
    else:
        made = words(sym["Admission"], 1)[0]
        sys.exit(f"AdmissionDone never rose, {made} tests made")
    a = words(sym["Admission"], 22)
    tests, admitted, total, most, fewest, empty = a[:6]
    print(f"{os.path.basename(elf)}: {tests} tests, {admitted} admitted; "
          f"the timing's own {empty} cycles taken off")
    print(f"  cycles: {total / tests:.0f} in the mean, {most} at most, {fewest} at fewest; "
          f"{total / tests / MHZ:.2f}, {most / MHZ:.2f} and {fewest / MHZ:.2f} us")
    low = 0
    for k, n in enumerate(a[6:]):
        high = 64 << k
        if n:
            label = f"{low}-{high - 1}" if k < 15 else f"{low} and more"
            print(f"  {label:>14s} cycles: {n}")
        low = high


def main():
    if len(sys.argv) < 2:
        sys.exit(__doc__)
    for elf in sys.argv[1:]:
        run(elf)


if __name__ == "__main__":
    main()
