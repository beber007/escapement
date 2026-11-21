#!/usr/bin/env python3
# Copyright (c) 2026 Bertrand Hurst. Part of Escapement, distributed under the terms of
# LICENSE at the root of this repository.
#
"""Run SuspendPico on a Pico and print the cycles each suspension takes.

    tools/pico_suspend.py ELF [ELF ...] [--seconds N]

SuspendPico (Examples/pico) counts, in cycles of SysTick on the processor's clock, from
the call of OSSuspendSynchronousTask to the task it preempted running again (#102), as
SuspendPico2 does on the Pico 2 (tools/pico2_latency.py). Each ELF is loaded into SRAM,
left to run, and its Results read over SWD without stopping the core. A measurement,
with no criterion to pass: SuspendPico.c says what it covers.
Needs OpenOCD and the Pico's Debug Probe (PROBE, tools/probe.sh), and its lock on the
bench (tools/board_ci.md).
"""

import argparse
import os
import re
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import read_trace   # noqa: E402

SKIPPED = 10   # SuspendPico.c leaves the first instances out of the sum


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("elf", nargs="+")
    parser.add_argument("--seconds", type=int, default=15)
    args = parser.parse_args()
    for elf in args.elf:
        sym = read_trace.symbols(elf)
        if "Results" not in sym or "SuspendTask" not in sym:
            sys.exit(f"{elf} is not SuspendPico")
        out = read_trace.load(elf)
        if not any(w in out for w in ("downloaded", "bytes written", "Verified OK")):
            sys.exit("could not load the image:\n" + out)
        time.sleep(args.seconds)
        out = read_trace.openocd([f"echo \"W [read_memory {sym['Results']:#x} 32 5]\""])
        found = re.search(r"W ([0-9a-fx ]+)", out)
        if not found:
            sys.exit("could not read the board:\n" + out)
        count, total, least, most, _ = [int(w, 0) for w in found.group(1).split()]
        if count <= SKIPPED:
            sys.exit(f"{elf}: {count} instances, too few")
        if most * (count - SKIPPED) >= 1 << 32:
            sys.exit(f"{elf}: the sum of 32 bits may have wrapped; "
                     "give fewer --seconds")
        mean = total / (count - SKIPPED)
        print(f"{elf}: {count} instances, {mean:.1f} cycles on average, "
              f"{least} to {most}")


if __name__ == "__main__":
    main()
