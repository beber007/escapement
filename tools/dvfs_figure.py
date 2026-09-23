#!/usr/bin/env python3
# Copyright (c) 2026 Bertrand Hurst. Part of Escapement, distributed under the terms of
# LICENSE at the root of this repository.
#
"""Draw the power-aware kernel changing the clock of a Pico, from a trace, as an SVG.

Reads the CSV that tools/read_trace.py --csv writes for TaskLEDPico built with
make KERNEL=PA TRACE=1, and writes a standalone SVG: an overview of the whole trace
and a zoom on two rounds, each with the tasks above and the clock of the core below.
No dependency, so that the figure can be regenerated from the data alone:

    tools/read_trace.py build/TaskLEDPico.elf --csv > docs/data/pico-pa-trace.csv
    tools/dvfs_figure.py docs/data/pico-pa-trace.csv docs/images/pico-dvfs.svg
"""
import csv
import sys

MARK, SPEED = 7, 6
MHZ = {0: 12, 1: 50, 2: 125}

# pin, label, period, colour — the order and colours of tools/chronogram.py, and the
# probe, which toggles its output on each instance, drawn from its mark to the next event
TASKS = [
    (4,  "GP4",  "probe · 1 ms",  "#1a7f37"),
    (25, "GP25", "LED · 10 ms",   "#1f6feb"),
    (2,  "GP2",  "task · 20 ms",  "#6f42c1"),
    (3,  "GP3",  "task · 60 ms",  "#bc4c00"),
]
PROBE = 4

W, LEFT, RIGHT = 900, 128, 24
LANE_H, LANE_GAP = 22, 6
SPEED_H = 70
INK, MUTED, GRID = "#1f2328", "#59636e", "#e7ecf0"


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


def panel(out, top, t0, t1, runs, speeds, title, ticks, unit, scale,
          px0=LEFT, px1=W - RIGHT):
    x = lambda us: px0 + (px1 - px0) * (us - t0) / (t1 - t0)
    labels = px0 == LEFT
    lanes_h = len(TASKS) * (LANE_H + LANE_GAP)
    bottom = top + lanes_h + 14 + SPEED_H
    out.append(f'<text x="{px0}" y="{top - 12}" font-size="12.5" font-weight="600" '
               f'fill="{INK}">{title}</text>')
    for tick in ticks:
        gx = x(tick)
        out.append(f'<line x1="{gx:.1f}" y1="{top - 4}" x2="{gx:.1f}" y2="{bottom}" '
                   f'stroke="{GRID}" stroke-width="1"/>')
        out.append(f'<text x="{gx:.1f}" y="{bottom + 15}" font-size="11" fill="{MUTED}" '
                   f'text-anchor="middle">{tick / scale:g}</text>')
    out.append(f'<text x="{px1}" y="{bottom + 30}" font-size="11" fill="{MUTED}" '
               f'text-anchor="end">{unit}</text>')

    for i, (pin, name, label, colour) in enumerate(TASKS):
        lt = top + i * (LANE_H + LANE_GAP)
        base, high = lt + LANE_H - 4, lt + 4
        if labels:
            out.append(f'<text x="{LEFT - 10}" y="{lt + 10}" font-size="11.5" '
                       f'font-weight="600" fill="{INK}" text-anchor="end">{name}</text>')
            out.append(f'<text x="{LEFT - 10}" y="{lt + 21}" font-size="10" fill="{MUTED}" '
                       f'text-anchor="end">{label}</text>')
        out.append(f'<line x1="{px0}" y1="{base}" x2="{px1}" y2="{base}" '
                   f'stroke="{colour}" stroke-width="1.2" opacity="0.4"/>')
        for a, b in runs[pin]:
            if b < t0 or a > t1:
                continue
            x0 = x(max(a, t0))
            x1 = max(x(min(b, t1)), x0 + 1.6)
            out.append(f'<rect x="{x0:.2f}" y="{high}" width="{x1 - x0:.2f}" '
                       f'height="{base - high}" rx="1" fill="{colour}"/>')

    st = top + lanes_h + 14
    y = lambda mhz: st + SPEED_H - SPEED_H * mhz / 125
    for mhz in (12, 50, 125):
        out.append(f'<line x1="{px0}" y1="{y(mhz):.1f}" x2="{px1}" y2="{y(mhz):.1f}" '
                   f'stroke="{GRID}" stroke-width="1"/>')
        if labels:
            out.append(f'<text x="{LEFT - 10}" y="{y(mhz) + 4:.1f}" font-size="10.5" '
                       f'fill="{MUTED}" text-anchor="end">{mhz} MHz</text>')
    out.append(f'<line x1="{px0}" y1="{st + SPEED_H}" x2="{px1}" y2="{st + SPEED_H}" '
               f'stroke="{MUTED}" stroke-width="1" opacity="0.5"/>')
    level = next((m for t, m in reversed(speeds) if t <= t0), speeds[0][1])
    d = [f"M{px0} {y(level):.1f}"]
    for t, mhz in speeds:
        if t0 < t < t1:
            d.append(f"H{x(t):.2f} V{y(mhz):.1f}")
            level = mhz
    d.append(f"H{px1}")
    out.append(f'<path d="{" ".join(d)}" fill="none" stroke="{INK}" stroke-width="1.6" '
               'stroke-linejoin="miter"/>')
    if labels:
        out.append(f'<text x="{LEFT}" y="{st - 3}" font-size="10.5" fill="{MUTED}">'
                   'clock of the core</text>')
    return bottom + 36


def render(runs, speeds, last):
    span = (last // 1000) * 1000
    out = []
    y = panel(out, 76, 0, span, runs, speeds, f"The whole trace, {span // 1000} ms",
              range(0, span + 1, 2000), "ms", 1000)
    split = LEFT + (W - RIGHT - LEFT) * 0.64
    top = y + 34
    panel(out, top, 990, 1310, runs, speeds, "Zoom: 50 MHz for the 20 ms task",
          range(1000, 1301, 50), "ms", 1000, LEFT, split - 14)
    y = panel(out, top, 1995, 2075, runs, speeds, "12 MHz for the probe",
              range(2000, 2071, 20), "ms", 1000, split + 14, W - RIGHT)
    height = y + 12
    head = [
        f'<svg xmlns="http://www.w3.org/2000/svg" width="{W}" height="{height}" '
        f'viewBox="0 0 {W} {height}" font-family="ui-sans-serif,-apple-system,'
        'Segoe UI,Helvetica,Arial,sans-serif">',
        f'<rect width="{W}" height="{height}" fill="#ffffff"/>',
        f'<text x="{LEFT}" y="26" font-size="15" font-weight="600" fill="{INK}">'
        'The power-aware kernel changing the clock of a real Raspberry Pi Pico</text>',
        f'<text x="{LEFT}" y="43" font-size="12" fill="{MUTED}">'
        'Written by the firmware into RAM, read over SWD without stopping a core, '
        'timed to the microsecond</text>',
    ]
    return "\n".join(head + out + ["</svg>"])


def main():
    src = sys.argv[1] if len(sys.argv) > 1 else "docs/data/pico-pa-trace.csv"
    dst = sys.argv[2] if len(sys.argv) > 2 else "docs/images/pico-dvfs.svg"
    runs, speeds, last = read(src)
    with open(dst, "w") as fh:
        fh.write(render(runs, speeds, last) + "\n")
    for pin, name, *_ in TASKS:
        print(f"{name}: {len(runs[pin])} instances")
    print(f"{len(speeds) - 1} changes of speed")


if __name__ == "__main__":
    main()
