#!/usr/bin/env python3
# Copyright (c) 2026 Bertrand Hurst. Part of Escapement, distributed under the terms of
# LICENSE at the root of this repository.
#
"""Run BenchDVFSPico on a Pico and print how long each change of speed took.

    tools/dvfs_bench.py path/to/BenchDVFSPico.elf

BenchDVFSPico (make KERNEL=PA bench) times, 10,000 times each and without the kernel,
every change of operating point of the DVFS driver and the wake-up path of a slow
idle task, then raises BenchDone. The tool loads it, waits for that, reads BenchSum
without stopping a core, and prints the means in microseconds as they come: two reads
of the 1 us counter included, whose own cost, timed too, is printed below them. The
register steps of 12 -> 125 MHz, timed one by one, are printed last.
docs/rp2040.md, "Where the idle task sleeps", has the figures of 2026-09-24.
"""

import argparse
import os
import re
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import read_trace   # noqa: E402

REPS = 10000   # BENCH_REPS
ROWS = ["12 -> 125 MHz", "125 -> 12 MHz", "50 -> 125 MHz", "125 -> 50 MHz",
        "12 -> 50 MHz", "50 -> 12 MHz",
        "two reads at 12 MHz", "two reads at 50 MHz", "two reads at 125 MHz",
        "step VREG", "step clk_sys onto ref", "step PLL post-divider", "step clk_sys onto PLL",
        "wake from 12 MHz", "wake from 50 MHz"]
ORDER = [0, 1, 2, 3, 4, 5, 13, 14, 6, 7, 8, 9, 10, 11, 12]


def words(address, count):
    out = read_trace.openocd([f"echo \"W [read_memory {address:#x} 32 {count}]\""])
    found = re.search(r"W ([0-9a-fx ]+)", out)
    if not found:
        sys.exit("could not read the board:\n" + out)
    return [int(w, 0) for w in found.group(1).split()]


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("elf")
    args = parser.parse_args()

    sym = read_trace.symbols(args.elf)
    if "BenchSum" not in sym:
        sys.exit(f"{args.elf} is not BenchDVFSPico")
    out = read_trace.openocd(["reset halt", f"load_image {args.elf}", "resume 0x20000000"])
    if "downloaded" not in out and "bytes written" not in out:
        sys.exit("could not load the image:\n" + out)
    for _ in range(30):
        time.sleep(2)
        if words(sym["BenchDone"], 1)[0] == 1:
            break
    else:
        sys.exit("BenchDone never rose")
    sums = words(sym["BenchSum"], len(ROWS))
    for k in ORDER:
        print(f"{ROWS[k]:<22s}: {sums[k] / REPS:.2f} us")


if __name__ == "__main__":
    main()
