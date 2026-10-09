#!/usr/bin/env python3
# Copyright (c) 2026 Bertrand Hurst. Part of Escapement, distributed under the terms of
# LICENSE at the root of this repository.
"""The current of the NUCLEO-U575ZI-Q's MCU, read with a Nordic PPK2 on the UNO Q.

The PPK2 is an ampere meter in place of JP5, the IDD jumper (UM2861, 6.4.6): its VIN on
the pin of JP5 that comes from JP4, the board's 3V3, its VOUT on the pin that goes to
the MCU. Wired the other way the MCU still runs, through the body diode of the PPK2's
switch, and the PPK2 reads nothing (2026-10-03). Its logic input D0 is on D13, PA5, which
SleepU5 built with PHASES=30 drives high in its Stop 2 phase, its VCC on the board's 3V3.

The image is SleepU5Flash.elf, in the flash: the debugger cannot reach the MCU through
the PPK2, and once connected it leaves the debug domain powered until the board is
powered off. Program it with JP5 fitted, then unplug CN1, put the PPK2 in place of JP5,
run this script, which closes the PPK2's switch, and plug CN1 back (docs/stm32u5.md).

    tools/ppk2_nucleo.py phases SECONDS   # the mean of each phase, by D0
    tools/ppk2_nucleo.py trace [MS]       # means of 100 samples over MS ms of samples

On the NUCLEO-U385RG-Q the PPK2 takes the place of JP4, its IDD jumper (UM3062), the same
way round, and D0 goes to D8, PC7, which SleepU3 built with PHASES=30 drives high in its
Stop 2 phase (D13 drives LD2 there); the image is SleepU3Flash.elf, programmed with JP4
fitted through the OpenOCD that knows the STM32U3 (tools/board_ci.md):

    tools/ppk2_nucleo.py phases 600       # SleepU3Flash, make PHASES=30 [FAST=1]

Its eight logic inputs read the pins of a board too, with no current to measure, VCC on
the board's 3V3 and GND on its ground: pins gives, for each input given, the period and
the time high from the median of its edges, which the chunks the UNO Q loses leave
right, against the period expected. TaskLEDU3 (Examples/nucleo-u385) drives D7, D8, D13
at 10, 20 and 60 ms and toggles D12 every millisecond:

    PPK2_PINS=D7:10000,D8:20000,D12:2000,D13:60000 tools/ppk2_nucleo.py pins 10

For SleepPico2 on a Pico 2 (Examples/pico2), the PPK2 powers the board instead, a source
of PPK2_SOURCE_MV millivolts on VSYS, its USB unplugged, and its phase is a number on D0
and D1, PPK2_PHASES naming each:

    PPK2_SOURCE_MV=5000 PPK2_PHASES=WFI,SLEEP,DORMANT tools/ppk2_nucleo.py phases 200

ppk2-api (IRNAS, 0.9.2) and pyserial go on PYTHONPATH: the UNO Q has neither pip nor
venv, their wheels unpacked into ~/ppk2-lib serve.
"""
import glob
import os
import sys
import time

from ppk2_api.ppk2_api import PPK2_API

RATE = 100000            # samples a second the PPK2 takes; the UNO Q reads some 70 to 80 %
                         # of them, chunks lost whole, which leaves the means right
BIN_UA = 10              # the bins the median of a phase is read from
SETTLE_S = 3             # the first second the PPK2 sends holds placeholder samples
SOURCE_MV = int(os.environ.get("PPK2_SOURCE_MV", "0"))     # 0: an ampere meter
NAMES = os.environ.get("PPK2_PHASES", "Sleep,Stop 2").split(",")
BITS = max(1, (len(NAMES) - 1).bit_length())             # D0, then D1 too for 3 or 4


def open_ppk2():
    """The PPK2 as an ampere meter, its switch closed. A run before may have left it
    streaming: it is stopped and what it sent dropped, or the metadata read meets
    samples."""
    port = sorted(glob.glob("/dev/serial/by-id/usb-Nordic_Semiconductor_PPK2_*-if01"))
    if not port:
        sys.exit("no PPK2 under /dev/serial/by-id")
    ppk2 = PPK2_API(port[0], timeout=1, write_timeout=1, exclusive=True)
    ppk2.stop_measuring()
    quiet, deadline = time.monotonic(), time.monotonic() + 5
    while time.monotonic() < deadline and time.monotonic() - quiet < 0.5:
        if ppk2.ser.in_waiting:
            ppk2.ser.read(ppk2.ser.in_waiting)
            quiet = time.monotonic()
        time.sleep(0.05)
    ppk2.get_modifiers()
    if SOURCE_MV:
        ppk2.use_source_meter()
        ppk2.set_source_voltage(SOURCE_MV)
    else:
        ppk2.use_ampere_meter()
        ppk2.set_source_voltage(3300)  # asked for by the library, unused as a meter
    ppk2.toggle_DUT_power("ON")
    time.sleep(0.5)
    return ppk2


def samples(ppk2, seconds, mask=(1 << BITS) - 1):
    """(current in uA, the digital inputs of mask) pairs for seconds, after SETTLE_S."""
    ppk2.start_measuring()
    settled = time.monotonic() + SETTLE_S
    end = settled + seconds
    try:
        while time.monotonic() < end:
            data = ppk2.get_data()
            if data != b"":
                values, raw = ppk2.get_samples(data)
                if time.monotonic() >= settled:
                    # D0 in bit 0 of each raw sample, D1 in bit 1 (digital_channels)
                    yield from zip(values, (r & mask for r in raw))
            time.sleep(0.001)
    finally:
        ppk2.stop_measuring()


def name(level):
    """The phase a level of the digital inputs stands for."""
    return NAMES[level] if level < len(NAMES) else f"level {level}"


def median(bins, n):
    """The median of a phase from its counts by bin of BIN_UA."""
    seen = 0
    for b in sorted(bins):
        seen += bins[b]
        if seen * 2 >= n:
            return (b + 0.5) * BIN_UA
    return 0.0


def phases(seconds):
    """Each run of D0 at one level is a phase, its sums kept as samples come; the first
    and the last, cut by the window, are left out of the means by level. The median of
    each, from counts by bin of BIN_UA, is the level the phase spends most of its time
    at: the core's running, for a phase that computes most of each period."""
    found = []                         # [level, n, sum, min, max, bins]
    ppk2 = open_ppk2()
    for value, level in samples(ppk2, seconds):
        if not found or found[-1][0] != level:
            found.append([level, 0, 0.0, value, value, {}])
        phase = found[-1]
        phase[1] += 1
        phase[2] += value
        phase[3] = min(phase[3], value)
        phase[4] = max(phase[4], value)
        b = int(value // BIN_UA)
        phase[5][b] = phase[5].get(b, 0) + 1
    rate = sum(p[1] for p in found) / seconds          # the samples read a second
    print(f"{rate / 1000:.0f} kS/s read of the PPK2's {RATE // 1000}")
    for i, (level, n, total, low, high, bins) in enumerate(found):
        cut = " (cut by the window)" if i in (0, len(found) - 1) else ""
        print(f"phase {i}: {name(level):7} {n / rate:5.1f} s, mean "
              f"{total / n:9.1f} uA, median {median(bins, n):9.1f}, min {low:8.1f}, "
              f"max {high:8.1f}{cut}")
    for level in sorted({p[0] for p in found}, reverse=True):
        whole = [p for i, p in enumerate(found) if p[0] == level and 0 < i < len(found) - 1]
        if whole:
            n = sum(p[1] for p in whole)
            print(f"{name(level)}: {len(whole)} whole phases, mean "
                  f"{sum(p[2] for p in whole) / n:.1f} uA")


def trace(ms):
    """The current in means of 100 samples, 1 ms at the PPK2's rate, 1.3 to 1.6 ms of those
    the UNO Q reads, from the first sample of a Stop 2 phase on. Each sleep
    starts low and climbs to its level: the PPK2 takes a large shunt for a small current,
    and the MCU's decoupling capacitors charge through it for some tens of ms; the mean
    is right, the level is read at the end of a long sleep."""
    kept, started = [], False
    for value, level in samples(open_ppk2(), 40):
        started = started or level
        if started:
            kept.append(value)
            if len(kept) >= ms * RATE // 1000:
                break
    step = RATE // 1000
    means = [sum(kept[i:i + step]) / step for i in range(0, len(kept) - step + 1, step)]
    for i in range(0, len(means), 10):
        print(f"{i:5d} ms: " + " ".join(f"{m:8.1f}" for m in means[i:i + 10]))


def pins(seconds):
    """For each pin of PPK2_PINS, name:period in us, on D0, D1 and on in that order: the
    median of the times between its rising edges and of the times high, in samples of
    10 us, and whether the period is within 2 % of the one expected. The UNO Q loses
    chunks of samples whole: a time that spans one is too long, which the median leaves
    out. Fails if a pin does not keep its period."""
    wanted = [(n, int(p)) for n, p in (w.split(":") for w in os.environ["PPK2_PINS"].split(","))]
    mask = (1 << len(wanted)) - 1
    rises = [[] for _ in wanted]
    highs = [[] for _ in wanted]
    last, rose = 0, [None] * len(wanted)
    for i, (_, level) in enumerate(samples(open_ppk2(), seconds, mask)):
        for k in range(len(wanted)):
            bit, was = level >> k & 1, last >> k & 1
            if bit and not was:
                if rose[k] is not None:
                    rises[k].append(i - rose[k])
                rose[k] = i
            elif was and not bit and rose[k] is not None:
                highs[k].append(i - rose[k])
        last = level
    failed = []
    for k, (name_, period) in enumerate(wanted):
        if not rises[k]:
            print(f"{name_} (D{k}): no edge")
            failed.append(name_)
            continue
        got = sorted(rises[k])[len(rises[k]) // 2] * 1e6 / RATE
        high = sorted(highs[k])[len(highs[k]) // 2] * 1e6 / RATE if highs[k] else 0
        ok = abs(got - period) <= period * 0.02
        print(f"{name_} (D{k}): {len(rises[k])} periods, {got:.0f} us, high {high:.0f} us, "
              f"expected {period} us: {'ok' if ok else 'OFF'}")
        if not ok:
            failed.append(name_)
    if failed:
        sys.exit("pins off: " + ", ".join(failed))


if __name__ == "__main__":
    if sys.argv[1:2] == ["phases"] and len(sys.argv) == 3:
        phases(float(sys.argv[2]))
    elif sys.argv[1:2] == ["pins"] and len(sys.argv) == 3:
        pins(float(sys.argv[2]))
    elif sys.argv[1:2] == ["trace"] and len(sys.argv) <= 3:
        trace(int(sys.argv[2]) if len(sys.argv) == 3 else 200)
    else:
        sys.exit(__doc__.split("\n\n")[-2])
