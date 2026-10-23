#!/usr/bin/env python3
# Copyright (c) 2026 Bertrand Hurst. Part of Escapement, distributed under the terms of
# LICENSE at the root of this repository.
#
"""Draw the power-aware kernel changing the clock of a Pico, from a trace, as an SVG.

Reads the CSV that tools/read_trace.py --csv writes for TaskLEDPico built with
make KERNEL=PA TRACE=1, and writes a standalone SVG: an overview of the whole trace
and a zoom on two rounds, each with the tasks above and the clock of the core below.
No dependency but tools/figure_svg.py, so that the figure can be regenerated from the
data alone:

    tools/read_trace.py build/TaskLEDPico.elf --csv > docs/data/pico-pa-trace.csv
    tools/dvfs_figure.py docs/data/pico-pa-trace.csv docs/images/pico-dvfs

which writes docs/images/pico-dvfs-light.svg and pico-dvfs-dark.svg, for a light and a
dark page.
"""
import csv
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import figure_svg as svg   # noqa: E402

MARK, SPEED = 7, 6
MHZ = {0: 12, 1: 50, 2: 125}

# pin, name, caption, hue of tools/figure_svg.py — the order and colours of the F4's
# chronogram, removed with it on 2026-09-26, and the probe, which toggles its output on
# each instance, drawn from its mark to the next event
TASKS = [
    (4,  "GP4",  "probe · 1 ms",  "green"),
    (25, "GP25", "LED · 10 ms",   "blue"),
    (2,  "GP2",  "task · 20 ms",  "purple"),
    (3,  "GP3",  "task · 60 ms",  "orange"),
]
PROBE = 4

W, LEFT, RIGHT = 900, 128, 24
LANE_H, LANE_GAP = 22, 6
SPEED_H = 70


def read(path):
    with open(path, newline="") as fh:
        rows = [(int(r["time_us"]), int(r["event"]), int(r["arg"]), int(r["extra"]))
                for r in csv.DictReader(fh)]
    runs = {pin: [] for pin, *_ in TASKS}
    opened, speeds = {}, []
    for i, (t, event, arg, extra) in enumerate(rows):
        if event == SPEED:
            if not speeds:
                speeds.append((0, MHZ[extra]))
            speeds.append((t, MHZ[arg]))
        elif event == MARK and arg in runs:
            if arg == PROBE:
                end = next((u for u, e, *_ in rows[i + 1:] if e in (MARK, SPEED)), t)
                runs[arg].append((t, end))
            elif extra == 0:
                opened[arg] = t
            elif arg in opened:
                runs[arg].append((opened.pop(arg), t))
    return runs, speeds, rows[-1][0]


def panel(out, th, top, t0, t1, runs, speeds, title, ticks, unit, scale,
          px0=LEFT, px1=W - RIGHT):
    x = svg.scale(t0, t1, px0, px1)
    labels = px0 == LEFT
    lanes_h = len(TASKS) * (LANE_H + LANE_GAP)
    bottom = top + lanes_h + 14 + SPEED_H
    out.append(svg.text(px0, top - 12, title, 12.5, th["ink"], weight="600"))
    svg.grid(out, th, x, ticks, top - 4, bottom, bottom + 15, 11,
             lambda tick: f"{tick / scale:g}")
    out.append(svg.text(px1, bottom + 30, unit, 11, th["muted"], anchor="end"))

    for i, (pin, name, caption, colour) in enumerate(TASKS):
        lt = top + i * (LANE_H + LANE_GAP)
        base, high = lt + LANE_H - 4, lt + 4
        if labels:
            svg.label(out, th, LEFT - 10, lt + 10, name, caption, 11.5, 10, 11)
        out.append(svg.hline(px0, px1, base, th[colour], 1.2, 0.4))
        svg.bars(out, x, runs[pin], t0, t1, high, base, th[colour])

    st = top + lanes_h + 14
    y = lambda mhz: st + SPEED_H - SPEED_H * mhz / 125
    for mhz in (12, 50, 125):
        out.append(svg.hline(px0, px1, f"{y(mhz):.1f}", th["grid"]))
        if labels:
            out.append(svg.text(LEFT - 10, f"{y(mhz) + 4:.1f}", f"{mhz} MHz", 10.5,
                                th["muted"], anchor="end"))
    out.append(svg.hline(px0, px1, st + SPEED_H, th["muted"], 1, 0.5))
    level = next((m for t, m in reversed(speeds) if t <= t0), speeds[0][1])
    d = [f"M{px0} {y(level):.1f}"]
    for t, mhz in speeds:
        if t0 < t < t1:
            d.append(f"H{x(t):.2f} V{y(mhz):.1f}")
            level = mhz
    d.append(f"H{px1}")
    out.append(f'<path d="{" ".join(d)}" fill="none" stroke="{th["ink"]}" '
               'stroke-width="1.6" stroke-linejoin="miter"/>')
    if labels:
        out.append(svg.text(LEFT, st - 3, "clock of the core", 10.5, th["muted"]))
    return bottom + 36


def render(th, runs, speeds, last):
    span = (last // 1000) * 1000
    out = [svg.text(LEFT, 26, "The power-aware kernel changing the clock of a real "
                    "Raspberry Pi Pico", 15, th["ink"], weight="600"),
           svg.text(LEFT, 43, "Written by the firmware into RAM, read over SWD without "
                    "stopping a core, timed to the microsecond", 12, th["muted"])]
    y = panel(out, th, 76, 0, span, runs, speeds, f"The whole trace, {span // 1000} ms",
              range(0, span + 1, 2000), "ms", 1000)
    split = LEFT + (W - RIGHT - LEFT) * 0.64
    top = y + 34
    panel(out, th, top, 990, 1310, runs, speeds, "Zoom: 50 MHz for the 20 ms task",
          range(1000, 1301, 50), "ms", 1000, LEFT, split - 14)
    y = panel(out, th, top, 1995, 2075, runs, speeds, "12 MHz for the probe",
              range(2000, 2071, 20), "ms", 1000, split + 14, W - RIGHT)
    return svg.document(th, W, y + 12, out)


def main():
    src = sys.argv[1] if len(sys.argv) > 1 else "docs/data/pico-pa-trace.csv"
    dst = sys.argv[2] if len(sys.argv) > 2 else "docs/images/pico-dvfs"
    runs, speeds, last = read(src)
    svg.write(dst, lambda th: render(th, runs, speeds, last))
    for pin, name, *_ in TASKS:
        print(f"{name}: {len(runs[pin])} instances")
    print(f"{len(speeds) - 1} changes of speed")


if __name__ == "__main__":
    main()
