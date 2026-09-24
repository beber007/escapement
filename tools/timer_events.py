#!/usr/bin/env python3
# Copyright (c) 2026 Bertrand Hurst. Part of Escapement, distributed under the terms of
# LICENSE at the root of this repository.
#
"""Run TestTimerEventPico on a Pico and sum up what its trace shows.

    tools/timer_events.py path/to/TestTimerEventPico.elf [--reads N]

The example raises GPIO 2 every 5 ms and has an event-driven task, woken by an alarm
of the timer, lower it 1 ms later; GPIO 3 the same every 10 ms for 2 ms. Built with
make TRACE=1 and ESCAPEMENT_MEASURE_SCHEDULING_COST (which lets the kernel's clock run
while the debugger holds a core), it leaves a mark in the trace at each edge, and the
event manager one at each delivery, with how late the alarm fired. The tool loads the
image, then reads the trace N times, a second apart, without stopping a core, and
prints for each output the periods and the high times seen, and how late the events
were. docs/rp2040.md, "Timer events on the board", has the figures of 2026-09-23.

The image is loaded with both cores held and core 1 left so: the example does not use
it, and a firmware in flash may have armed a watchdog that only pauses while a core is
held.
"""

import argparse
import os
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import read_trace   # noqa: E402


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("elf")
    parser.add_argument("--reads", type=int, default=5)
    args = parser.parse_args()

    out = read_trace.openocd(["reset halt", f"load_image {args.elf}", "resume 0x20000000"])
    if "downloaded" not in out and "bytes written" not in out:
        sys.exit("could not load the image:\n" + out)
    periods, highs, late, events = {}, {}, [], 0
    for _ in range(args.reads):
        time.sleep(1)
        _, _, entries = read_trace.read(args.elf)
        events += len(entries)
        p, h = read_trace.pin_stats(entries)
        for pin, values in p.items():
            periods.setdefault(pin, []).extend(values)
        for pin, values in h.items():
            highs.setdefault(pin, []).extend(values)
        late += [extra for _, event, _, extra in entries if event == 8]

    print(f"reads             : {args.reads}, {events} events")
    for pin in sorted(set(periods) | set(highs)):
        line = f"gpio {pin:<13d}:"
        for name, values in (("period", periods.get(pin)), ("high", highs.get(pin))):
            if values:
                line += f" {name} {min(values)} {max(values)} us ({len(values)})"
        print(line)
    if late:
        print(f"events delivered  : {len(late)}, {min(late)} to {max(late)} us late")
    else:
        print("events delivered  : 0")


if __name__ == "__main__":
    main()
