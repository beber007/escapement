#!/usr/bin/env python3
# Copyright (c) 2026 Bertrand Hurst. Part of Escapement, distributed under the terms of
# LICENSE at the root of this repository.
"""SleepU5 on the UNO Q, read from its reports on LPUART1: the idle task in Stop 2.

SleepU5 (Examples/uno-q) sends a line every ten instances of its task, one a second:
the counts of Results. This reads them for SECONDS, the image already loaded, and ends
with an error unless, at the last report:
  - the instances are as many as the time read allows, less three of the start;
  - each started one period after the one before, within MAX_JITTER_US;
  - none woke past its event, and Stop 2 was set up (the LSE runs);
  - Stop 2 was entered by at least MIN_ENTRIES of the instances (those that send a
    report stay in Sleep);
  - the longest wake-up took less than the OS_STOP2_WAKE_US allowed, 3 ms;
  - each instance's timer event came, within MAX_EVENT_OFF_US of when it was due;
  - TIM2 and LPTIM1 agree within MAX_PPM over the run;
and unless a line through the arrival of each report on CLOCK_MONOTONIC gives a second of
the kernel's within LIMIT_PPM of Linux's, the bound of tools/unoq_drift.py.
Run on the board, the endurance test's service stopped (tools/unoq_check.sh).

    tools/unoq_sleep.py SECONDS
"""
import os
import select
import subprocess
import sys
import termios
import time

MAX_JITTER_US = 2
MAX_EVENT_OFF_US = 20
MIN_ENTRIES = 0.8
WAKE_TICKS = 3000 * 32768 // 1000000
MAX_PPM = 20
LIMIT_PPM = 300
BRIDGE = ["arduino-router-serial.path", "arduino-router-serial", "arduino-router"]

subprocess.run(["sudo", "-n", "systemctl", "stop"] + BRIDGE, capture_output=True,
               check=False)
fd = os.open("/dev/ttyHS1", os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
attrs = termios.tcgetattr(fd)
attrs[0] = attrs[1] = attrs[3] = 0                        # raw
attrs[2] = termios.CS8 | termios.CREAD | termios.CLOCAL   # no flow control
attrs[4] = attrs[5] = termios.B115200
termios.tcsetattr(fd, termios.TCSANOW, attrs)
termios.tcflush(fd, termios.TCIOFLUSH)

seconds = float(sys.argv[1])
end = time.monotonic() + seconds
pending, last, points = b"", None, []        # points: (kernel's us, arrival)
while time.monotonic() < end:
    if select.select([fd], [], [], 1)[0]:
        now = time.monotonic()
        pending += os.read(fd, 4096)
        while b"\n" in pending:
            line, pending = pending.split(b"\n", 1)
            words = line.split()
            if len(words) == 11 and words[0] == b"SLEEP":
                try:
                    last = [int(w, 16) for w in words[1:]]
                except ValueError:
                    continue
                points.append((last[2], now))

if last is None or len(points) < 3:
    sys.exit("SleepU5: no report")
instances, ticks, micros, jitter, entries, wake, late, nolse, events, event_off = last
n = len(points)
mx = sum(p[0] for p in points) / n
my = sum(p[1] for p in points) / n
slope = sum((p[0] - mx) * (p[1] - my) for p in points) / \
        sum((p[0] - mx) ** 2 for p in points)
rate = (slope * 1e6 - 1) * 1e6                  # ppm a second of the kernel's lasts longer
ppm = (micros - ticks * 1e6 / 32768) / micros * 1e6
print(f"SleepU5: {instances} instances, {entries} into Stop 2, gap off by {jitter} us "
      f"at most, longest wake-up {wake} ticks, {late} late, {events} events off by "
      f"{event_off} us at most, TIM2 {ppm:+.1f} ppm against "
      f"LPTIM1, a second lasts {rate:+.1f} ppm against Linux's over {n} reports")
failures = []
if instances < (seconds - 3) * 10:
    failures.append("too few instances")
if jitter > MAX_JITTER_US:
    failures.append("a start off its period")
if late or nolse:
    failures.append("a wake-up past its event" if late else "no LSE")
if entries < MIN_ENTRIES * instances:
    failures.append("too few entries into Stop 2")
if wake >= WAKE_TICKS:
    failures.append("a wake-up longer than allowed")
if events < instances or event_off > MAX_EVENT_OFF_US:
    failures.append("a timer event missing or off its time")
if abs(ppm) > MAX_PPM:
    failures.append("TIM2 and LPTIM1 apart")
if abs(rate) > LIMIT_PPM:
    failures.append("the clock against Linux's")
if failures:
    sys.exit("SleepU5: " + ", ".join(failures))
