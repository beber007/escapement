#!/usr/bin/env python3
# Copyright (c) 2026 Bertrand Hurst. Part of Escapement, distributed under the terms of
# LICENSE at the root of this repository.
"""The rate of the U5's clock against Linux's, on the UNO Q, from SoakU5's reports.

SoakU5 sends a report on LPUART1 at the start of each of its seconds, the seconds run
first. This times the arrival of each on CLOCK_MONOTONIC, whose rate NTP disciplines,
and fits a line through them: how much longer than Linux's a second of the kernel's
lasts, in ppm, to within a few in minutes where the seconds counted in the log need
hours. Run it on the board with the endurance test's service stopped, which holds the
port (tools/soak.py stops Arduino's Bridge, which does too).

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
attrs[4] = attrs[5] = termios.B115200
termios.tcsetattr(fd, termios.TCSANOW, attrs)
termios.tcflush(fd, termios.TCIOFLUSH)

end = time.monotonic() + float(sys.argv[1])
pending, points = b"", []                    # (seconds of the kernel's, arrival)
while time.monotonic() < end:
    if select.select([fd], [], [], 1)[0]:
        now = time.monotonic()
        pending += os.read(fd, 4096)
        while b"\n" in pending:
            line, pending = pending.split(b"\n", 1)
            words = line.split()
            if len(words) >= 2 and words[0] == b"SOAK":
                try:
                    points.append((int(words[1], 16), now))
                except ValueError:
                    pass
if len(points) < 3:
    sys.exit("fewer than three reports: is SoakU5 running, and the port free?")
n = len(points)
mx = sum(x for x, _ in points) / n
my = sum(y for _, y in points) / n
sxx = sum((x - mx) ** 2 for x, _ in points)
slope = sum((x - mx) * (y - my) for x, y in points) / sxx
spread = (sum((y - my - slope * (x - mx)) ** 2 for x, y in points) / (n - 2)) ** 0.5
print(f"{n} reports over {points[-1][0] - points[0][0]} s: a second of the kernel's lasts "
      f"{(slope - 1) * 1e6:+.1f} ppm more than Linux's (standard error "
      f"{spread / sxx ** 0.5 * 1e6:.1f} ppm, jitter {spread * 1e3:.1f} ms)")
