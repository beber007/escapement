#!/usr/bin/env python3
# Copyright (c) 2026 Bertrand Hurst. Part of Escapement, distributed under the terms of
# LICENSE at the root of this repository.
#
"""Read back the scheduling trace of a Pico built with make TRACE=1.

The trace is a ring buffer in RAM (Escapement/CORTEX-Mx/Escapement_Trace.h). This
tool sets the flag that suspends the recording, reads the buffer in one transfer,
lets the recording go on, and prints the events in order with their times — without
ever stopping a core, which would make the tasks miss their deadlines.

    tools/read_trace.py path/to/TaskLEDPico.elf [--last N] [--keep-frozen] [--pins|--csv]

With --pins, it summarises instead the marks the tasks leave on each output: the period
from one start to the next, and the time from a start to its end. With --csv, it
prints the raw events, times counted from the first one kept (tools/dvfs_figure.py
draws them).

Needs OpenOCD and a CMSIS-DAP probe (the Raspberry Pi Debug Probe), and the
arm-none-eabi binutils for the addresses of the symbols.
"""

import argparse
import re
import subprocess
import sys

SIZE = 256   # OS_TRACE_SIZE
EVENTS = {1: "alarm0", 2: "alarm1", 3: "soft>", 4: "soft<", 5: "set_timer",
          6: "speed", 7: "mark", 8: "event"}
SPEEDS = {0: "12 MHz", 1: "50 MHz", 2: "125 MHz"}
TIMERAWL = 0x40054028


def symbols(elf):
    out = subprocess.run(["arm-none-eabi-nm", elf], capture_output=True, text=True,
                         check=True).stdout
    table = {}
    for line in out.splitlines():
        parts = line.split()
        if len(parts) == 3:
            table[parts[2]] = int(parts[0], 16)
    return table


def openocd(commands):
    args = ["openocd", "-f", "interface/cmsis-dap.cfg", "-c", "adapter speed 5000",
            "-f", "target/rp2040.cfg", "-c", "init"]
    for c in commands:
        args += ["-c", c]
    args += ["-c", "exit"]
    run = subprocess.run(args, capture_output=True, text=True, timeout=60)
    return run.stdout + run.stderr


def describe(event, arg, extra):
    name = EVENTS.get(event, f"event{event}")
    if event == 5:
        return f"{name} {'ahead ' + str(extra) + ' us' if arg else 'passed'}"
    if event == 6:
        return f"{name} {SPEEDS.get(extra, extra)} -> {SPEEDS.get(arg, arg)}"
    if event == 7:
        return f"{name} gpio {arg} {'end' if extra else 'start'}"
    if event == 8:
        return f"{name} alarm bit {arg:#x}, {extra} us late"
    return name


def pin_stats(entries):
    """The periods and high times of the marks on each output, in microseconds."""
    starts, periods, highs = {}, {}, {}
    for time, event, arg, extra in entries:
        if event != 7:
            continue
        if extra == 0:
            if arg in starts:
                periods.setdefault(arg, []).append((time - starts[arg]) & 0xFFFFFFFF)
            starts[arg] = time
        elif arg in starts:
            highs.setdefault(arg, []).append((time - starts[arg]) & 0xFFFFFFFF)
    return periods, highs


def pin_summary(entries):
    periods, highs = pin_stats(entries)
    for pin in sorted(set(periods) | set(highs)):
        line = f"gpio {pin:2d}"
        for name, values in (("period", periods.get(pin)), ("high", highs.get(pin))):
            if values:
                line += f"  {name} {min(values)}..{max(values)} us ({len(values)})"
        print(line)


def read(elf, keep_frozen=False):
    """Reads the trace without stopping a core: the count of events recorded, the time
    of the read, and the events kept, oldest first, as (time, event, arg, extra)."""
    sym = symbols(elf)
    for name in ("_OSTrace", "_OSTraceCount", "_OSTraceFrozen"):
        if name not in sym:
            sys.exit(f"{name} not found: build with make TRACE=1")
    commands = [f"write_memory {sym['_OSTraceFrozen']:#x} 32 {{1}}",
                f"echo \"COUNT [read_memory {sym['_OSTraceCount']:#x} 32 1]\"",
                f"echo \"NOW [read_memory {TIMERAWL:#x} 32 1]\"",
                f"echo \"DATA [read_memory {sym['_OSTrace']:#x} 32 {2 * SIZE}]\""]
    if not keep_frozen:
        commands.append(f"write_memory {sym['_OSTraceFrozen']:#x} 32 {{0}}")
    out = openocd(commands)
    count = re.search(r"COUNT (\S+)", out)
    now = re.search(r"NOW (\S+)", out)
    data = re.search(r"DATA ([0-9a-fx ]+)", out)
    if not (count and now and data):
        sys.exit("could not read the trace:\n" + out)
    count, now = int(count.group(1), 0), int(now.group(1), 0)
    words = [int(w, 0) for w in data.group(1).split()]

    n = min(count, SIZE)
    start = count % SIZE if count > SIZE else 0
    entries = []
    for i in range(n):
        k = (start + i) % SIZE
        time, packed = words[2 * k], words[2 * k + 1]
        entries.append((time, packed & 0xFF, (packed >> 8) & 0xFF, packed >> 16))
    return count, now, entries


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("elf")
    parser.add_argument("--last", type=int, default=SIZE, help="events to print")
    parser.add_argument("--keep-frozen", action="store_true",
                        help="leave the recording suspended after reading")
    parser.add_argument("--pins", action="store_true",
                        help="summarise the periods and high times of the marks")
    parser.add_argument("--csv", action="store_true",
                        help="print the raw events as CSV")
    args = parser.parse_args()

    count, now, entries = read(args.elf, args.keep_frozen)
    n = len(entries)
    print(f"{count} events recorded, the last {n} kept",
          file=sys.stderr if args.csv else sys.stdout)
    if not entries:
        return
    if args.pins:
        pin_summary(entries)
        return
    if args.csv:
        print("time_us,event,arg,extra")
        for time, event, arg, extra in entries:
            print(f"{(time - entries[0][0]) & 0xFFFFFFFF},{event},{arg},{extra}")
        return
    first = entries[0][0]
    previous = first
    for time, event, arg, extra in entries[-args.last:]:
        print(f"{(time - first) & 0xFFFFFFFF:10d} us  +{(time - previous) & 0xFFFFFFFF:7d}  "
              f"{describe(event, arg, extra)}")
        previous = time
    print(f"last event {(now - entries[-1][0]) & 0xFFFFFFFF} us before the read")


if __name__ == "__main__":
    main()
