#!/usr/bin/env python3
# Copyright (c) 2026 Bertrand Hurst. Part of Escapement, distributed under the terms of
# LICENSE at the root of this repository.
"""The rate of the U5's clock against Linux's, on the UNO Q, from SoakU5's reports.

SoakU5 sends a report on LPUART1 at the start of each of its seconds, the seconds run
first. This times the arrival of each and fits a line through them: how much longer than
Linux's a second of the kernel's lasts, in ppm, to within a few in minutes where the
seconds counted in the log need hours.

The first line of the result is against CLOCK_MONOTONIC_RAW, the crystal of the board's
Qualcomm processor as it is, which nothing corrects. The second is against
CLOCK_MONOTONIC, whose rate NTP disciplines, and which is the truer reference only while
NTP is steady: over Wi-Fi answering in 1.5 or 204 ms, on 2026-09-28, NTP pulled it by
hundreds of ppm, and two runs read +278 and -686 ppm. The check (tools/unoq_check.sh)
reads the first, then, and its bound of 300 ppm dwarfs a crystal's error.

Run it on the board with the endurance test's service stopped, which holds the port
(tools/soak.py stops Arduino's Bridge, which does too).

    tools/unoq_drift.py SECONDS
"""
import os
import select
import sys
import termios
import time

fd = os.open("/dev/ttyHS1", os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
attrs = termios.tcgetattr(fd)
attrs[0] = attrs[1] = attrs[3] = 0                        # raw
attrs[2] = termios.CS8 | termios.CREAD | termios.CLOCAL   # no flow control
attrs[4] = attrs[5] = termios.B57600                      # LPUART1 (Escapement_UART.c)
termios.tcsetattr(fd, termios.TCSANOW, attrs)
termios.tcflush(fd, termios.TCIOFLUSH)

end = time.monotonic() + float(sys.argv[1])
pending, points = b"", []    # (seconds of the kernel's, arrival raw, arrival disciplined)
while time.monotonic() < end:
    if select.select([fd], [], [], 1)[0]:
        raw, now = time.clock_gettime(time.CLOCK_MONOTONIC_RAW), time.monotonic()
        pending += os.read(fd, 4096)
        while b"\n" in pending:
            line, pending = pending.split(b"\n", 1)
            words = line.split()
            if len(words) >= 2 and words[0] == b"SOAK":
                try:
                    points.append((int(words[1], 16), raw, now))
                except ValueError:
                    pass
if len(points) < 3:
    sys.exit("fewer than three reports: is SoakU5 running, and the port free?")
n = len(points)


def fit(column):
    """The slope of the arrivals on one clock against the kernel's seconds, less one, in
    ppm, with its standard error in ppm and the scatter of the arrivals in ms."""
    xs, ys = [p[0] for p in points], [p[column] for p in points]
    mx, my = sum(xs) / n, sum(ys) / n
    sxx = sum((x - mx) ** 2 for x in xs)
    slope = sum((x - mx) * (y - my) for x, y in zip(xs, ys)) / sxx
    spread = (sum((y - my - slope * (x - mx)) ** 2 for x, y in zip(xs, ys)) / (n - 2)) ** 0.5
    return (slope - 1) * 1e6, spread / sxx ** 0.5 * 1e6, spread * 1e3


raw, error, jitter = fit(1)
print(f"{n} reports over {points[-1][0] - points[0][0]} s: a second of the kernel's lasts "
      f"{raw:+.1f} ppm more than Linux's raw clock (standard error {error:.1f} ppm, "
      f"jitter {jitter:.1f} ms)")
ntp, error, jitter = fit(2)
print(f"against the clock NTP disciplines: {ntp:+.1f} ppm (standard error {error:.1f} ppm, "
      f"jitter {jitter:.1f} ms); NTP runs the raw clock {ntp - raw:+.1f} ppm off")
