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

ppk2-api (IRNAS, 0.9.2) and pyserial go on PYTHONPATH: the UNO Q has neither pip nor
venv, their wheels unpacked into ~/ppk2-lib serve.
"""
import glob
import sys
import time

from ppk2_api.ppk2_api import PPK2_API

RATE = 100000            # samples a second the PPK2 takes; the UNO Q reads some 70 to 80 %
                         # of them, chunks lost whole, which leaves the means right
SETTLE_S = 3             # the first second the PPK2 sends holds placeholder samples


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
    ppk2.use_ampere_meter()
    ppk2.set_source_voltage(3300)      # asked for by the library, unused as a meter
    ppk2.toggle_DUT_power("ON")
    time.sleep(0.5)
    return ppk2


def samples(ppk2, seconds):
    """(current in uA, D0) pairs for seconds, after SETTLE_S."""
    ppk2.start_measuring()
    settled = time.monotonic() + SETTLE_S
    end = settled + seconds
    try:
        while time.monotonic() < end:
            data = ppk2.get_data()
            if data != b"":
                values, raw = ppk2.get_samples(data)
                if time.monotonic() >= settled:
                    yield from zip(values, ppk2.digital_channels(raw)[0])
            time.sleep(0.001)
    finally:
        ppk2.stop_measuring()


def phases(seconds):
    """Each run of D0 at one level is a phase, its sums kept as samples come; the first
    and the last, cut by the window, are left out of the means by level."""
    found = []                         # [level, n, sum, min, max]
    ppk2 = open_ppk2()
    for value, level in samples(ppk2, seconds):
        if not found or found[-1][0] != level:
            found.append([level, 0, 0.0, value, value])
        phase = found[-1]
        phase[1] += 1
        phase[2] += value
        phase[3] = min(phase[3], value)
        phase[4] = max(phase[4], value)
    rate = sum(p[1] for p in found) / seconds          # the samples read a second
    print(f"{rate / 1000:.0f} kS/s read of the PPK2's {RATE // 1000}")
    for i, (level, n, total, low, high) in enumerate(found):
        cut = " (cut by the window)" if i in (0, len(found) - 1) else ""
        print(f"phase {i}: {'Stop 2' if level else 'Sleep '} {n / rate:5.1f} s, mean "
              f"{total / n:9.1f} uA, min {low:8.1f}, max {high:8.1f}{cut}")
    for level, name in ((1, "Stop 2"), (0, "Sleep")):
        whole = [p for i, p in enumerate(found) if p[0] == level and 0 < i < len(found) - 1]
        if whole:
            n = sum(p[1] for p in whole)
            print(f"{name}: {len(whole)} whole phases, mean {sum(p[2] for p in whole) / n:.1f} uA")


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


if __name__ == "__main__":
    if sys.argv[1:2] == ["phases"] and len(sys.argv) == 3:
        phases(float(sys.argv[2]))
    elif sys.argv[1:2] == ["trace"] and len(sys.argv) <= 3:
        trace(int(sys.argv[2]) if len(sys.argv) == 3 else 200)
    else:
        sys.exit(__doc__.split("\n\n")[-2])
