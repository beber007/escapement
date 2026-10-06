#!/usr/bin/env python3
# Copyright (c) 2026 Bertrand Hurst. Part of Escapement, distributed under the terms of
# LICENSE at the root of this repository.
#
"""Stamp the lines DormantPico2 sends on probe3's UART with the UNO Q's clocks, and give
how far the always-on timer, on LPOSC, kept the time asleep.

    python3 tools/pico2_dormant_stamp.py SECONDS LOG [SLEEP_MS]

Run on the UNO Q before the image is loaded, for longer than its calibration and its
sleeps (900 s for the defaults). Each line of LOG is CLOCK_MONOTONIC, CLOCK_MONOTONIC_RAW
and the line received: the first is slewed by NTP, the second runs on the UNO Q's crystal
alone, and their difference bounds what the reference itself is worth. The error is
taken between the lines of cycle 1 and of the last cycle, each sent after a wake-up of
the same delay; cycle 0's follows none.
"""

import glob
import os
import select
import sys
import termios
import time

PROBE3 = "/dev/serial/by-id/*Debug_Probe*E665B838877F432E-if01"


def main():
    seconds, log = float(sys.argv[1]), sys.argv[2]
    sleep_ms = int(sys.argv[3]) if len(sys.argv) > 3 else 10000
    fd = os.open(glob.glob(PROBE3)[0], os.O_RDONLY | os.O_NOCTTY | os.O_NONBLOCK)
    a = termios.tcgetattr(fd)
    a[0] = a[1] = a[3] = 0
    a[2] = termios.CS8 | termios.CREAD | termios.CLOCAL
    a[4] = a[5] = termios.B115200
    termios.tcsetattr(fd, termios.TCSANOW, a)
    termios.tcflush(fd, termios.TCIFLUSH)
    end, buf, wakes = time.monotonic() + seconds, b"", {}
    with open(log, "w") as out:
        while time.monotonic() < end:
            # A timeout, not a blocking read: the image goes quiet after its last cycle.
            if not select.select([fd], [], [], 1.0)[0]:
                continue
            chunk = os.read(fd, 64)
            now = time.clock_gettime(time.CLOCK_MONOTONIC)
            raw = time.clock_gettime(time.CLOCK_MONOTONIC_RAW)
            for c in chunk:
                if c != 10:
                    buf += bytes([c])
                    continue
                text = buf.decode(errors="replace").strip()
                buf = b""
                out.write("%.6f %.6f %s\n" % (now, raw, text))
                out.flush()
                fields = text.split()
                if len(fields) == 3 and fields[0] == "WAKE":
                    wakes[int(fields[1], 16)] = (now, raw)
    os.close(fd)
    cycles = sorted(c for c in wakes if c >= 1)
    if len(cycles) < 2:
        print("%d wake-ups seen: too few for an error" % len(cycles))
        sys.exit(1)
    first, last = cycles[0], cycles[-1]
    expected = (last - first) * sleep_ms / 1000.0
    for name, k in (("MONOTONIC", 0), ("MONOTONIC_RAW", 1)):
        elapsed = wakes[last][k] - wakes[first][k]
        print("%s: cycles %d to %d, %.6f s for %.3f, %+.1f ppm"
              % (name, first, last, elapsed, expected, (elapsed / expected - 1) * 1e6))


if __name__ == "__main__":
    main()
