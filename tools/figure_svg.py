# Copyright (c) 2026 Bertrand Hurst. Part of Escapement, distributed under the terms of
# LICENSE at the root of this repository.
"""What the SVG figures of the documentation share, so that their style stays one: the
font, the palettes of a light and a dark page, the page itself, a time axis and its grid,
a lane's label and its bars. tools/host_figures.py, tools/schedule_figure.py and
tools/dvfs_figure.py draw with it, each at its own sizes. No dependency.
"""

FONT = "ui-sans-serif,-apple-system,Segoe UI,Helvetica,Arial,sans-serif"
LIGHT = {"bg": "#ffffff", "ink": "#1f2328", "muted": "#59636e", "grid": "#e7ecf0"}
DARK = {"bg": "#0d1117", "ink": "#e6edf3", "muted": "#9198a1", "grid": "#21262d"}


def scale(t0, t1, px0, px1):
    """The x of a time, t0 at px0 and t1 at px1."""
    return lambda t: px0 + (px1 - px0) * (t - t0) / (t1 - t0)


def text(x, y, content, size, fill, weight=None, anchor=None):
    """A line of text; x and y as they are to be written."""
    weight = f' font-weight="{weight}"' if weight else ""
    anchor = f' text-anchor="{anchor}"' if anchor else ""
    return (f'<text x="{x}" y="{y}" font-size="{size}"{weight} fill="{fill}"{anchor}>'
            f'{content}</text>')


def hline(x0, x1, y, stroke, width=1, opacity=None):
    """A horizontal line; its coordinates as they are to be written."""
    opacity = f' opacity="{opacity}"' if opacity is not None else ""
    return (f'<line x1="{x0}" y1="{y}" x2="{x1}" y2="{y}" stroke="{stroke}" '
            f'stroke-width="{width}"{opacity}/>')


def grid(out, th, x, ticks, top, bottom, label_y, size, tick_label=str):
    """A time axis: at each tick, a line from top to bottom and its label below."""
    for tick in ticks:
        gx = x(tick)
        out.append(f'<line x1="{gx:.1f}" y1="{top}" x2="{gx:.1f}" y2="{bottom}" '
                   f'stroke="{th["grid"]}" stroke-width="1"/>')
        out.append(text(f"{gx:.1f}", label_y, tick_label(tick), size, th["muted"],
                        anchor="middle"))


def label(out, th, x, y, name, caption, size, small, leading):
    """A lane's name, right-aligned at x, and its caption leading below it."""
    out.append(text(x, y, name, size, th["ink"], weight="600", anchor="end"))
    out.append(text(x, y + leading, caption, small, th["muted"], anchor="end"))


def bars(out, x, spans, t0, t1, high, base, colour):
    """A rectangle from high to base for each (start, end) of spans, cut to the window
    from t0 to t1 and never thinner than 1.6 px, so that a microsecond still shows."""
    for a, b in spans:
        if b < t0 or a > t1:
            continue
        x0 = x(max(a, t0))
        x1 = max(x(min(b, t1)), x0 + 1.6)
        out.append(f'<rect x="{x0:.2f}" y="{high}" width="{x1 - x0:.2f}" '
                   f'height="{base - high}" rx="1" fill="{colour}"/>')


def document(th, width, height, body, rounded=None):
    """The whole SVG: the page, its corners rounded by rounded px if given, then body."""
    rx = f' rx="{rounded}"' if rounded else ""
    return "\n".join([f'<svg xmlns="http://www.w3.org/2000/svg" width="{width}" '
                      f'height="{height}" viewBox="0 0 {width} {height}" '
                      f'font-family="{FONT}">',
                      f'<rect width="{width}" height="{height}"{rx} fill="{th["bg"]}"/>']
                     + body + ["</svg>"])
