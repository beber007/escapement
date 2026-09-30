#!/usr/bin/env python3
# Copyright (c) 2026 Bertrand Hurst. Part of Escapement, distributed under the terms of
# LICENSE at the root of this repository.
#
"""Run SoakPico2 on the Pico 2 for a number of minutes, reading its Results over SWD
every minute without stopping a core, long enough to cross the 2^30 wrap of the kernel
clock: 17 min 54 s at the chip's 1 us tick.

    tools/pico2_soak.py [BUILD_DIR] [--minutes N]      40 by default: two wraps

The image is loaded as tools/pico2_check.py loads it, whose functions this uses, and
every reading is printed as tools/soak.py logs the Pico's. It fails on a restart (the
marker gone or the seconds going back), on any error of the eight parts, and on a part
or the seconds that did not move between two readings, which is how a hung kernel shows
should the watchdog not restart the chip (SoakPico2 sets what it resets since
2026-09-30; before, it reset nothing). Unlike tools/soak.py it posts no status and keeps
no state: a run by hand, holding the probe's lock (tools/board_ci.md, "The bench").
"""

import argparse
import os
import sys
import tempfile
import time

import pico2_check as p

INTERVAL, WORDS = 60, 24
PARTS = ["pulse", "queue", "buffer", "events", "cores", "heartbeat", "interrupt", "memory"]


def read(address):
    out = p.openocd([f"mdw {address:#x} {WORDS}"])
    values = []
    for line in out.splitlines():
        if line.startswith("0x") and ":" in line:
            values += [int(w, 16) for w in line.split(":", 1)[1].split()]
    return values[:WORDS] if len(values) >= WORDS else None


def show(r):
    parts = ", ".join(f"{name} {r[3 + i]}/{r[11 + i]}" for i, name in enumerate(PARTS))
    print(f"{time.strftime('%Y-%m-%d %H:%M:%S', time.gmtime())} run {r[1]} s, {r[2]} wraps, "
          f"{parts}, late max {r[19]}/{r[20]} us, stack free {r[21]}/{r[22]}, "
          f"load {r[23]} us", flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("build", nargs="?", default=p.EXAMPLES)
    parser.add_argument("--minutes", type=int, default=40)
    args = parser.parse_args()
    elf = os.path.join(args.build, "SoakPico2.elf")

    print(f"SoakPico2 {elf}, {args.minutes} readings every {INTERVAL} s", flush=True)
    p.openocd(["rescue_reset"])
    with tempfile.TemporaryDirectory() as directory:
        started = time.time()
        r = p.run(elf, p.build_seed(directory), INTERVAL, WORDS)
    address = p.symbol(elf, "Results")
    last = errors = missed = wraps = stalls = 0
    before = None
    for n in range(1, args.minutes + 1):
        if n > 1:
            time.sleep(max(0, started + n * INTERVAL - time.time()))
            r = read(address)
        if r is None:
            missed += 1
            print(f"{time.strftime('%Y-%m-%d %H:%M:%S', time.gmtime())} no reading",
                  flush=True)
            continue
        if r[0] != 0x534F414B or r[1] < last:
            print(f"RESTART: marker {r[0]:#x}, {r[1]} s after {last} s", flush=True)
            sys.exit(1)
        if before is not None and (r[1] <= before[1] or
                                   any(r[3 + i] == before[3 + i] for i in range(8))):
            stalls += 1
            print(f"STALLED: {r[1]} s after {before[1]} s, parts "
                  f"{[r[3 + i] - before[3 + i] for i in range(8)]}", flush=True)
        before = r
        last, wraps, errors = r[1], r[2], sum(r[11:19])
        show(r)
    print(f"end: {last} s, {wraps} wraps, {errors} errors, {stalls} stalls, "
          f"{missed} readings missed", flush=True)
    sys.exit(1 if errors or stalls else 0)


if __name__ == "__main__":
    main()
