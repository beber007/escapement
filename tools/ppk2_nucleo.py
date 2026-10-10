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
the board's 3V3 and GND on its ground: pins gives, for each input given, the period
from the count of its edges and the time high, against the period expected within
2 %. TaskLEDU3 (Examples/nucleo-u385) drives D7, D8, D13
at 10, 20 and 60 ms and toggles D12 every millisecond:

    PPK2_PINS=D7:10000,D8:20000,D12:2000,D13:60000 tools/ppk2_nucleo.py pins 10

For SleepPico2 on a Pico 2 (Examples/pico2), the PPK2 powers the board instead, a source
of PPK2_SOURCE_MV millivolts on VSYS, its USB unplugged, and its phase is a number on D0
and D1, PPK2_PHASES naming each:

    PPK2_SOURCE_MV=5000 PPK2_PHASES=WFI,SLEEP,DORMANT tools/ppk2_nucleo.py phases 200

ppk2-api (IRNAS, 0.9.2), pyserial and numpy go on PYTHONPATH: the UNO Q has neither pip
nor venv, their wheels unpacked into ~/ppk2-lib serve.

The samples are decoded here, a block at a time with numpy, by ppk2-api's own formulas
and its filter of the samples that follow a change of range (Decoder). Its get_samples,
a sample at a time in Python, took 1.17 s of the UNO Q's for each second of samples on
2026-10-10, while a read that decoded nothing took in all of them: the serial port's
buffer overflowed, and some 30 to 40 % of the samples were lost, in chunks.
"""
import glob
import os
import sys
import time

import numpy as np
from ppk2_api.ppk2_api import PPK2_API

RATE = 100000            # samples a second the PPK2 takes
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


class Decoder:
    """ppk2-api's get_samples and get_adc_result over a block of bytes at once: the range
    and the ADC value of each 4-byte sample, the calibration of its range, and the logic
    inputs in its top byte. ppk2-api replaces the samples that follow a change of range by
    rolling averages (its spike filter); that filter runs here as there, sample by sample,
    over each change and the WARM samples before it, from which its averages start: they
    forget their start within some 150 samples at the slower of their two rates."""
    WARM = 300

    def __init__(self, ppk2):
        m = ppk2.modifiers
        table = lambda key: np.array([float(m[key][str(r)]) for r in range(5)])
        self.o, self.r, self.ug = table("O"), table("R"), table("UG")
        self.gs, self.gi, self.s, self.i = table("GS"), table("GI"), table("S"), table("I")
        self.mult, self.vdd = ppk2.adc_mult, ppk2.current_vdd / 1000
        self.a, self.a5 = ppk2.spike_filter_alpha, ppk2.spike_filter_alpha5
        self.spike = ppk2.spike_filter_samples
        self.rest = b""
        self.tail_adc, self.tail_rng = np.zeros(0), np.zeros(0, dtype=np.intp)

    def decode(self, data):
        """(currents in uA, logic levels) of the whole samples in data, the bytes of a
        sample cut at its end kept for the next block."""
        data = self.rest + data
        whole = len(data) // 4 * 4
        self.rest = data[whole:]
        raw = np.frombuffer(data[:whole], dtype="<u4")
        rng = np.minimum((raw >> 14) & 7, 4).astype(np.intp)
        value = ((raw & 0x3FFF) * 4).astype(np.float64)
        w = (value - self.o[rng]) * (self.mult / self.r[rng])
        adc = self.ug[rng] * (w * (self.gs[rng] * w + self.gi[rng]) +
                              (self.s[rng] * self.vdd + self.i[rng]))
        out = adc.copy()
        allr = np.concatenate((self.tail_rng, rng))
        alla = np.concatenate((self.tail_adc, adc))
        t = len(self.tail_rng)
        changes = np.flatnonzero(allr[1:] != allr[:-1]) + 1
        changes = changes[changes + self.spike > t]
        spans = []
        for c in changes:
            start, end = max(0, c - self.WARM), c + self.spike
            if spans and start <= spans[-1][1]:
                spans[-1][1] = max(spans[-1][1], end)
            else:
                spans.append([start, end])
        for start, end in spans:
            self._filter(alla, allr, start, min(end, len(alla)), t, out)
        keep = self.WARM + self.spike
        self.tail_rng, self.tail_adc = allr[-keep:], alla[-keep:]
        return out * 1e6, (raw >> 24).astype(np.uint8)

    def _filter(self, alla, allr, start, end, t, out):
        """get_adc_result's spike filter from start, its averages begun at that sample,
        the samples it replaces from t on written into out."""
        ra = ra4 = alla[start]
        prev, after, consecutive = allr[start], 0, 0
        for k in range(start, end):
            adc, rng = alla[k], allr[k]
            pra, pra4 = ra, ra4
            ra = self.a * adc + (1 - self.a) * ra
            ra4 = self.a5 * adc + (1 - self.a5) * ra4
            if rng != prev or after > 0:
                if rng != prev:
                    consecutive, after = 0, self.spike
                else:
                    consecutive += 1
                if rng == 4:
                    if consecutive < 2:
                        ra, ra4 = pra, pra4
                    adc = ra4
                else:
                    adc = ra
                after -= 1
                if k >= t:
                    out[k - t] = adc
            prev = rng


def blocks(ppk2, seconds, mask=(1 << BITS) - 1):
    """(currents in uA, the digital inputs of mask) arrays for seconds, after SETTLE_S,
    as the PPK2 sends them: the port read without a pause while it holds bytes."""
    decoder = Decoder(ppk2)
    ppk2.start_measuring()
    settled = time.monotonic() + SETTLE_S
    end = settled + seconds
    try:
        while time.monotonic() < end:
            data = ppk2.get_data()
            if data == b"":
                time.sleep(0.001)
                continue
            values, levels = decoder.decode(data)
            if time.monotonic() >= settled and len(values):
                # D0 in bit 0 of each sample, D1 in bit 1 (ppk2-api's digital_channels)
                yield values, levels & mask
    finally:
        ppk2.stop_measuring()


def samples(ppk2, seconds, mask=(1 << BITS) - 1):
    """(current in uA, the digital inputs of mask) pairs for seconds, after SETTLE_S."""
    for values, levels in blocks(ppk2, seconds, mask):
        yield from zip(values.tolist(), levels.tolist())


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
    for values, levels in blocks(ppk2, seconds):
        cuts = np.concatenate(([0], np.flatnonzero(levels[1:] != levels[:-1]) + 1,
                               [len(levels)]))
        for a, b in zip(cuts[:-1], cuts[1:]):
            level, part = int(levels[a]), values[a:b]
            if not found or found[-1][0] != level:
                found.append([level, 0, 0.0, float(part[0]), float(part[0]), {}])
            phase = found[-1]
            phase[1] += len(part)
            phase[2] += float(part.sum())
            phase[3] = min(phase[3], float(part.min()))
            phase[4] = max(phase[4], float(part.max()))
            bins, counts = np.unique(np.floor(part / BIN_UA).astype(np.int64),
                                     return_counts=True)
            for b_, c in zip(bins.tolist(), counts.tolist()):
                phase[5][b_] = phase[5].get(b_, 0) + c
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
    """For each pin of PPK2_PINS, name:period in us, on D0, D1 and on in that order: its
    period from the count of its rising edges over the samples read, the median of its
    times high, and whether the period is within 2 % of the one expected. Under
    TaskLEDU3 on 2026-10-10, every sample read (Decoder), the four came within 0.2 %;
    with ppk2-api's decoding, some 40 % of the samples lost in chunks, the median of the
    times between edges had been off by up to 30 %. Fails if a pin does not keep its
    period."""
    wanted = [(n, int(p)) for n, p in (w.split(":") for w in os.environ["PPK2_PINS"].split(","))]
    mask = (1 << len(wanted)) - 1
    ups = [[] for _ in wanted]
    downs = [[] for _ in wanted]
    last, read = 0, 0
    for _, levels in blocks(open_ppk2(), seconds, mask):
        full = np.concatenate(([last], levels)).astype(np.int64)
        for k in range(len(wanted)):
            bit = (full >> k) & 1
            ups[k].append(np.flatnonzero((bit[1:] == 1) & (bit[:-1] == 0)) + read)
            downs[k].append(np.flatnonzero((bit[1:] == 0) & (bit[:-1] == 1)) + read)
        last, read = int(levels[-1]), read + len(levels)
    rises, highs = [], []
    for k in range(len(wanted)):
        up, down = np.concatenate(ups[k]), np.concatenate(downs[k])
        rises.append(np.diff(up).tolist())
        # each fall against the last rise before it
        before = np.searchsorted(up, down) - 1
        highs.append((down[before >= 0] - up[before[before >= 0]]).tolist())
    failed = []
    for k, (name_, period) in enumerate(wanted):
        if not rises[k]:
            print(f"{name_} (D{k}): no edge")
            failed.append(name_)
            continue
        got = read * 1e6 / RATE / (len(rises[k]) + 1)
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
