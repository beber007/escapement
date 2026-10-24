#!/usr/bin/env python3
# Copyright (c) 2026 Bertrand Hurst. Part of Escapement, distributed under the terms of
# LICENSE at the root of this repository.
#
"""Read the execution times a Pico or a Pico 2 built with make WCET=1 measures of its tasks.

The kernel keeps, for each task, the largest number of ticks one of its instances held
the processor (EscapementHard.h and EscapementSoft.h, "EXECUTION TIMES"), in a list the
debugger walks from _OSTaskTimes. Each figure is one word, which the probe reads whole:
the tool reads them without stopping a core, which would make the tasks miss their
deadlines.

    tools/wcet.py path/to/SoakPico.elf [--margin 20]
    OPENOCD=~/opt/openocd-rpi/bin/openocd PROBE=probe3 tools/wcet.py --target rp2350 \
        path/to/SoakPico2.elf

For each task, by the name of its code and its argument: the instances ended, the largest
time seen and, with --margin, that time plus the margin in percent, a figure to declare.
It is the worst case seen, for the load the run gave it, not a bound (docs/api.md,
"Measuring execution times").

Needs OpenOCD and a CMSIS-DAP probe (PROBE=name picks one of the bench's,
tools/probe.sh), Raspberry Pi's OpenOCD for the RP2350, and the arm-none-eabi binutils
for the addresses of the symbols. The STM32 boards are reached by other debuggers: there
a debugger walks _OSTaskTimes by hand.
"""

import argparse
import math
import os
import re
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from read_trace import adapter, symbols  # noqa: E402

FIELDS = 6   # OSTASKTIME: Next, TaskCodePtr, Argument, Held, MaxHeld, Instances


def openocd(target, commands):
    args = [os.environ.get("OPENOCD", "openocd"), "-f", "interface/cmsis-dap.cfg",
            "-c", adapter(), "-c", "adapter speed 5000", "-f", f"target/{target}.cfg",
            "-c", "init"]
    for c in commands:
        args += ["-c", c]
    args += ["-c", "exit"]
    run = subprocess.run(args, capture_output=True, text=True, timeout=60)
    return run.stdout + run.stderr


def read(elf, target="rp2040"):
    """The figures of every task, the first created first, as dictionaries."""
    sym = symbols(elf)
    if "_OSTaskTimes" not in sym:
        sys.exit("_OSTaskTimes not found: build with make WCET=1")
    # The list is fixed once the kernel runs: one session walks it, record by record.
    walk = (f"set p [lindex [read_memory {sym['_OSTaskTimes']:#x} 32 1] 0]; "
            f"set n 0; "
            f"while {{$p != 0 && $n < 256}} {{ "
            f"echo \"TASK [read_memory $p 32 {FIELDS}]\"; "
            f"set p [lindex [read_memory $p 32 1] 0]; incr n }}")
    # In SLEEP (make SLEEP_GATE=1) the probe reads zeros from the whole bus, without an
    # error, unless core 1 is held, as tools/read_trace.py does; but a core held stops
    # the timer (TIMER_DBGPAUSE), and the instance running on core 0 meanwhile would be
    # charged less than it took. Such an image gives no figure rather than a low one.
    if "OSInitSleepGate" in sym:
        sys.exit("a SLEEP_GATE image cannot be read without stopping the timer")
    out = openocd(target, [walk, 'echo "END"'])
    if "END" not in out:
        sys.exit("could not read the figures:\n" + out)
    names = {address & ~1: name for name, address in sym.items()}
    tasks = []
    for line in re.findall(r"TASK ([0-9a-fx ]+)", out):
        _, code, argument, held, most, instances = (int(w, 0) for w in line.split())
        tasks.append({"code": names.get(code & ~1, f"{code:#x}"), "argument": argument,
                      "held": held, "max": most, "instances": instances})
    return tasks[::-1]


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("elf")
    parser.add_argument("--target", choices=["rp2040", "rp2350"], default="rp2040",
                        help="the chip, as OpenOCD names its configuration")
    parser.add_argument("--margin", type=float, default=None,
                        help="propose the largest time plus this margin, in percent")
    args = parser.parse_args()

    tasks = read(args.elf, args.target)
    if not tasks:
        sys.exit("no task measured")
    print(f"{'task':24s} {'argument':>10s} {'instances':>10s} {'largest':>10s}"
          + (f" {'proposed':>10s}" if args.margin is not None else ""))
    for t in tasks:
        line = (f"{t['code']:24s} {t['argument']:#10x} {t['instances']:10d} "
                f"{t['max']:10d}")
        if args.margin is not None:
            line += f" {math.ceil(t['max'] *(1 + args.margin / 100)):10d}"
        print(line)
    print("times in ticks of the kernel's clock, microseconds")


if __name__ == "__main__":
    main()
