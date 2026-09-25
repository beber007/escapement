#!/usr/bin/env python3
# Copyright (c) 2026 Bertrand Hurst. Part of Escapement, distributed under the terms of
# LICENSE at the root of this repository.
#
"""Draw the scheduling chronogram of the stm32f4 example as an SVG.

Reads the CSV produced by trace_gpio.sh and writes a standalone SVG. No
dependency, so that the figure can be regenerated from the data alone:

    tools/trace_gpio.sh > docs/data/f4-gpio-trace.csv
    tools/chronogram.py docs/data/f4-gpio-trace.csv docs/images/f4-schedule.svg
"""
import csv
import sys

# The value trace_gpio.sh reports is the pin mask, whichever half of BSRR it was in.
PINS = [
    (8192,  "PB13", "task 1", 820,  "#1f6feb"),
    (16384, "PB14", "task 2", 1640, "#6f42c1"),
    (32768, "PB15", "task 3", 4920, "#bc4c00"),
]

WINDOW_US = 10000      # what the figure shows
W, LEFT, RIGHT = 900, 104, 24
TRACK_H, TRACK_GAP, TOP = 46, 20, 54
MIN_PULSE_PX = 2.0     # below this, a pulse would be invisible
LONGEST_US = 0         # filled in by render(), for the caption


def read_runs(path):
    """Return {mask: [(start_us, end_us), ...]} from the transition list."""
    runs = {mask: [] for mask, *_ in PINS}
    opened = {}
    with open(path, newline="") as fh:
        for row in csv.DictReader(fh):
            t, edge = int(row["time_us"]), row["edge"]
            mask = int(row["value"])
            if mask not in runs:
                continue
            if edge == "rise":
                opened[mask] = t
            elif mask in opened:
                runs[mask].append((opened.pop(mask), t))
    return runs


def esc(s):
    return s.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;")


def render(runs):
    global LONGEST_US
    LONGEST_US = max(b - a for runs_ in runs.values() for a, b in runs_ if a < WINDOW_US)
    plot_w = W - LEFT - RIGHT
    height = TOP + len(PINS) * (TRACK_H + TRACK_GAP) + 62
    x = lambda us: LEFT + plot_w * us / WINDOW_US

    out = [
        f'<svg xmlns="http://www.w3.org/2000/svg" width="{W}" height="{height}" '
        f'viewBox="0 0 {W} {height}" font-family="ui-sans-serif,-apple-system,'
        'Segoe UI,Helvetica,Arial,sans-serif">',
        f'<rect width="{W}" height="{height}" fill="#ffffff"/>',
        f'<text x="{LEFT}" y="26" font-size="15" font-weight="600" fill="#1f2328">'
        'Three periodic tasks under EDF, on an emulated STM32F407VG</text>',
        f'<text x="{LEFT}" y="43" font-size="12" fill="#59636e">'
        'GPIO transitions captured under Renode with virtual timestamps</text>',
    ]

    # vertical grid, one line per millisecond
    for ms in range(0, WINDOW_US // 1000 + 1):
        gx = x(ms * 1000)
        out.append(f'<line x1="{gx:.1f}" y1="{TOP - 8}" x2="{gx:.1f}" '
                   f'y2="{TOP + len(PINS) * (TRACK_H + TRACK_GAP) - TRACK_GAP + 6}" '
                   'stroke="#e7ecf0" stroke-width="1"/>')
        out.append(f'<text x="{gx:.1f}" y="{height - 34}" font-size="11" '
                   f'fill="#59636e" text-anchor="middle">{ms}</text>')

    for i, (mask, pin, label, period, colour) in enumerate(PINS):
        top = TOP + i * (TRACK_H + TRACK_GAP)
        base, high = top + TRACK_H - 10, top + 8
        out.append(f'<text x="{LEFT - 12}" y="{high + 6}" font-size="12.5" '
                   f'font-weight="600" fill="#1f2328" text-anchor="end">{esc(pin)}</text>')
        out.append(f'<text x="{LEFT - 12}" y="{base + 2}" font-size="11" '
                   f'fill="#59636e" text-anchor="end">{esc(label)} · {period} us</text>')
        out.append(f'<line x1="{LEFT}" y1="{base}" x2="{W - RIGHT}" y2="{base}" '
                   f'stroke="{colour}" stroke-width="1.4" opacity="0.45"/>')

        for start, end in runs[mask]:
            if start >= WINDOW_US:
                break
            x0 = x(start)
            x1 = max(x(min(end, WINDOW_US)), x0 + MIN_PULSE_PX)
            out.append(f'<path d="M{x0:.2f} {base} L{x0:.2f} {high} L{x1:.2f} {high} '
                       f'L{x1:.2f} {base}" fill="none" stroke="{colour}" '
                       'stroke-width="1.8" stroke-linejoin="miter"/>')

    shortest = min(b - a for runs_ in runs.values() for a, b in runs_ if a < WINDOW_US)
    out.append(f'<text x="{LEFT}" y="{height - 6}" font-size="11" fill="#59636e">'
               f'Time in milliseconds. Execution times run from {shortest} to '
               f'{LONGEST_US} us, far below one pixel here: every pulse is drawn at '
               f'least {MIN_PULSE_PX:.0f} px wide, while its leading edge stays exact.'
               '</text>')
    out.append('</svg>')
    return "\n".join(out)


def main():
    src = sys.argv[1] if len(sys.argv) > 1 else "docs/data/f4-gpio-trace.csv"
    dst = sys.argv[2] if len(sys.argv) > 2 else "docs/images/f4-schedule.svg"
    runs = read_runs(src)
    with open(dst, "w") as fh:
        fh.write(render(runs) + "\n")
    for mask, pin, label, period, _ in PINS:
        shown = [r for r in runs[mask] if r[0] < WINDOW_US]
        widths = [b - a for a, b in shown]
        print(f"{pin}: {len(shown)} instances shown, execution {min(widths)}-{max(widths)} us")


if __name__ == "__main__":
    main()
