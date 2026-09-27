#!/usr/bin/env python3
# Copyright (c) 2026 Bertrand Hurst. Part of Escapement, distributed under the terms of
# LICENSE at the root of this repository.
"""Draw TaskLEDPico's schedule as computed beside the one a trace shows, as an SVG.

Reads the CSV of tools/read_trace.py --csv --absolute (or the same compressed with gzip),
for TaskLEDPico built with make TRACE=1, and picks the release of the 60 ms task whose
instance ran longest in the trace, an instant when the four tasks are released together.
Above, the schedule computed for that instant: the kernel's entry, then the tasks in
priority order, each for its execution time, the costs being the medians the trace
itself shows (tools/response_times.py for the bounds). Below, the trace: the kernel from
the alarm to the first task's mark, each task from its start mark to its end mark, the
probe, which marks only its start, up to the next task's start when one follows at once,
for the median of those times otherwise. No dependency:

    tools/schedule_figure.py docs/data/pico-response-dm.csv.gz docs/images/pico-schedule.svg
"""
import gzip
import statistics
import sys

ALARM, MARK = 1, 7
PROBE = 4
# pin, name, caption, colour, period in us, execution time in us for the computed row
# (the 60 ms task's is that of the traced instance, so that the rows can be compared)
TASKS = [
    (PROBE, "GP4", "probe · 1 ms", "#1a7f37", 1000),
    (25, "GP25", "10 ms", "#1f6feb", 10000),
    (2, "GP2", "20 ms", "#6f42c1", 20000),
    (3, "GP3", "60 ms", "#bc4c00", 60000),
]
KERNEL = "#8c959f"

W, LEFT, RIGHT = 900, 128, 24
LANE_H, LANE_GAP = 22, 6
INK, MUTED, GRID = "#1f2328", "#59636e", "#e7ecf0"


def read(path):
    opener = gzip.open if path.endswith(".gz") else open
    with opener(path, "rt") as fh:
        lines = fh.read().splitlines()
    origin = next(int(l.split()[2]) for l in lines if l.startswith("# origin"))
    rows = [tuple(int(v) for v in l.split(",")) for l in lines if l[:1].isdigit()]
    return origin, rows


def runs_in(rows):
    """Per pin, the (start, end) of each instance; the kernel's (alarm, first mark)."""
    runs = {pin: [] for pin, *_ in TASKS}
    runs["kernel"] = []
    opened, alarm = {}, None
    for i, (t, event, pin, extra) in enumerate(rows):
        if event == ALARM:
            alarm = t
        elif event == MARK and pin in runs:
            if alarm is not None and t - alarm < 100:
                runs["kernel"].append((alarm, t))
            alarm = None
            if pin == PROBE:
                # its end is the next task's start mark when one follows at once
                nxt = next(((u, e) for u, e, *_ in rows[i + 1:] if e in (ALARM, MARK)),
                           None)
                runs[pin].append((t, nxt[0] if nxt and nxt[1] == MARK and
                                  nxt[0] - t < 20 else None))
            elif extra == 0:
                opened[pin] = t
            elif pin in opened:
                start = opened.pop(pin)
                if t - start < 60000:
                    runs[pin].append((start, t))
    return runs


def pick(origin, runs):
    """The release of the 60 ms task whose instance the trace shows longest."""
    best = max(runs[3], key=lambda r: r[1] - r[0])
    since = (best[0] - origin) & 0xFFFFFFFF
    return best[0] - since % 60000, best[1] - best[0]


def medians(origin, runs):
    """The costs the computed row uses, each the median the trace shows: the kernel's
    entry at the releases of the 60 ms task, when all four tasks are released and the
    handler moves them all; the probe, from its mark to the next task's start; the switch
    from one task's end mark to the next one's start mark."""
    probe = statistics.median(b - a for a, b in runs[PROBE] if b is not None)
    runs[PROBE] = [(a, b if b is not None else a + probe) for a, b in runs[PROBE]]
    joint = [b - a for a, b in runs["kernel"] if ((a - origin) & 0xFFFFFFFF) % 60000 < 5]
    entry = statistics.median(joint)
    ends = sorted(b for pin in (25, 2, 3) for a, b in runs[pin])
    starts = sorted(a for pin in (25, 2, 3) for a, b in runs[pin])
    gaps = []
    for e in ends:
        after = next((s for s in starts if s > e), None)
        if after is not None and after - e < 20:
            gaps.append(after - e)
    switch = statistics.median(gaps)
    return entry, probe, switch


def computed(release, entry, probe, switch, spans):
    """The four tasks released at once and run in priority order, none released again in
    the window: the kernel's entry, the probe, then each task for its time, a switch
    between one and the next."""
    rows, t = {"kernel": [(release, release + entry)]}, release + entry
    for pin, *_ in TASKS:
        length = probe if pin == PROBE else spans[pin]
        rows[pin] = [(t, t + length)]
        t += length + (0 if pin == PROBE else switch)
    return rows


def panel(out, top, t0, t1, runs, title, ticks):
    x = lambda us: LEFT + (W - RIGHT - LEFT) * (us - t0) / (t1 - t0)
    lanes = [("kernel", "kernel", "entry", KERNEL)] + [(p, n, c, col)
                                                       for p, n, c, col, _ in TASKS]
    bottom = top + len(lanes) * (LANE_H + LANE_GAP)
    out.append(f'<text x="{LEFT}" y="{top - 12}" font-size="12.5" font-weight="600" '
               f'fill="{INK}">{title}</text>')
    for tick in ticks:
        gx = x(t0 + tick)
        out.append(f'<line x1="{gx:.1f}" y1="{top - 4}" x2="{gx:.1f}" y2="{bottom}" '
                   f'stroke="{GRID}" stroke-width="1"/>')
        out.append(f'<text x="{gx:.1f}" y="{bottom + 15}" font-size="11" fill="{MUTED}" '
                   f'text-anchor="middle">{tick}</text>')
    out.append(f'<text x="{W - RIGHT}" y="{bottom + 30}" font-size="11" fill="{MUTED}" '
               'text-anchor="end">µs after the release</text>')
    for i, (key, name, label, colour) in enumerate(lanes):
        lt = top + i * (LANE_H + LANE_GAP)
        base, high = lt + LANE_H - 4, lt + 4
        out.append(f'<text x="{LEFT - 10}" y="{lt + 10}" font-size="11.5" '
                   f'font-weight="600" fill="{INK}" text-anchor="end">{name}</text>')
        out.append(f'<text x="{LEFT - 10}" y="{lt + 21}" font-size="10" fill="{MUTED}" '
                   f'text-anchor="end">{label}</text>')
        out.append(f'<line x1="{LEFT}" y1="{base}" x2="{W - RIGHT}" y2="{base}" '
                   f'stroke="{colour}" stroke-width="1.2" opacity="0.4"/>')
        for a, b in runs.get(key, []):
            if b < t0 or a > t1:
                continue
            x0 = x(max(a, t0))
            x1 = max(x(min(b, t1)), x0 + 1.6)
            out.append(f'<rect x="{x0:.2f}" y="{high}" width="{x1 - x0:.2f}" '
                       f'height="{base - high}" rx="1" fill="{colour}"/>')
    return bottom + 40


def render(release, window, top_runs, bottom_runs, title, subtitle):
    out = []
    ticks = range(0, window + 1, 50)
    y = panel(out, 76, release, release + window, top_runs,
              "Computed from the task set and the medians of the trace", ticks)
    y = panel(out, y + 24, release, release + window, bottom_runs,
              "Traced on the board", ticks)
    height = y + 8
    head = [
        f'<svg xmlns="http://www.w3.org/2000/svg" width="{W}" height="{height}" '
        f'viewBox="0 0 {W} {height}" font-family="ui-sans-serif,-apple-system,'
        'Segoe UI,Helvetica,Arial,sans-serif">',
        f'<rect width="{W}" height="{height}" fill="#ffffff"/>',
        f'<text x="{LEFT}" y="26" font-size="15" font-weight="600" fill="{INK}">'
        f'{title}</text>',
        f'<text x="{LEFT}" y="43" font-size="12" fill="{MUTED}">{subtitle}</text>',
    ]
    return "\n".join(head + out + ["</svg>"])


def main():
    src = sys.argv[1] if len(sys.argv) > 1 else "docs/data/pico-response-dm.csv.gz"
    dst = sys.argv[2] if len(sys.argv) > 2 else "docs/images/pico-schedule.svg"
    origin, rows = read(src)
    runs = runs_in(rows)
    release, span3 = pick(origin, runs)
    entry, probe, switch = medians(origin, runs)
    spans = {pin: min(b - a for a, b in runs[pin]) for pin in (25, 2)}
    spans[3] = span3
    top = computed(release, entry, probe, switch, spans)
    end = top[3][0][1] - release
    window = max(100, ((int(end) + 40) // 50 + 1) * 50)
    svg = render(release, window, top, runs,
                 "TaskLEDPico on a Raspberry Pi Pico: the four tasks released at once",
                 f"Medians of the trace: kernel's entry {entry:g} µs when all four are "
                 f"released, probe {probe:g} µs, switch {switch:g} µs; the 60 ms task's "
                 f"instance ran {span3} µs")
    with open(dst, "w") as fh:
        fh.write(svg + "\n")
    print(f"release at {release}, window {window} µs, entry {entry} µs, probe {probe} µs, "
          f"switch {switch} µs, tasks {spans}")


if __name__ == "__main__":
    main()
