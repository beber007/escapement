#!/usr/bin/env python3
# Copyright (c) 2026 Bertrand Hurst. Part of Escapement, distributed under the terms of
# LICENSE at the root of this repository.
#
"""Run LatencyPico2 on the Pico 2 and print the cycles from an arrival's alarm to the
first instruction of its task, for the Cortex-M33 and the Hazard3 cores (issue #77).

    tools/pico2_latency.py ELF [ELF ...] [--seconds N]

Each ELF, an Arm or a RISC-V image of Examples/pico2, is loaded as tools/pico2_check.py
loads it, left to run, and its Results read over SWD without stopping a core. A
measurement, with no criterion to pass: LatencyPico2.c says what it covers. Needs what
tools/pico2_check.py needs, and the probe's lock (tools/board_ci.md, "The bench").
"""

import argparse
import tempfile
import time

import pico2_check as p


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("elf", nargs="+")
    parser.add_argument("--seconds", type=int, default=15)
    args = parser.parse_args()
    for elf in args.elf:
        p.openocd(["rescue_reset"])
        with tempfile.TemporaryDirectory() as directory:
            p.load(elf, None if p.is_riscv(elf) else p.build_seed(directory))
        time.sleep(args.seconds)
        out = p.openocd([f"mdw {p.symbol(elf, 'Results'):#x} 5"], p.core_of(elf))
        values = []
        for line in out.splitlines():
            if line.startswith("0x") and ":" in line:
                values += [int(w, 16) for w in line.split(":", 1)[1].split()]
        if len(values) < 5 or values[0] <= 10:
            raise SystemExit(f"Results of {elf} not read back, or no instance:\n{out}")
        count, total, least, most, _ = values[:5]
        arch = "Hazard3" if p.is_riscv(elf) else "Cortex-M33"
        print(f"{elf} ({arch}): {count} instances, {total / (count - 10):.1f} cycles "
              f"on average, {least} to {most}")


if __name__ == "__main__":
    main()
