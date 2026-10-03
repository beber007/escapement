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
  - every byte of a count this sends, in frames of 1 to 32 bytes 0.1 to 0.5 s apart,
    came through Stop 2, none out of the count or lost to an overrun;
  - TIM2 and LPTIM1 agree within MAX_PPM over the run. So does SleepNoHSEU5's, whose
    PLL1 on the MSIS runs 106.7 ppm fast: TIM2 is set from LPTIM1 at every wake-up from
    Stop 2, where the image spends most of its time, and runs on PLL1 only awake; it
    read -4.1 ppm on 2026-10-02, where +106.7 had been expected.
The wake-ups the HSE missed, PLL1 then on the MSIS until the next, are shown but not
checked: a fallback, not an error (Escapement_Processor.c), though none was ever seen.
LPUART1 runs at 115,200 baud for SleepU5, the byte that wakes the chip sampled while its
clock starts: each frame goes after a wake-up byte, 0x00, and WAKE_WAIT_S, in the window
that byte opens (Escapement_Stop2.h); 0x00, the count and its CRC-16 encoded with COBS,
0x00 (SleepU5.c). The frames SleepU5 dropped, at most one a wake-up byte that came out
wrong, are shown; a frame of the count dropped would show as bytes missing.
A line through the arrival of each report on CLOCK_MONOTONIC gives the rate of a second
of the kernel's against Linux's, shown but not checked: over a minute it read from -37 to
+543 ppm on 2026-09-27, a report some 30 ms late being enough; tools/unoq_drift.py
checks the clock over five minutes right after.
Run on the board, the endurance test's service stopped (tools/unoq_check.sh).

With nucleo, SleepU5 of Examples/nucleo-u575 on a NUCLEO-U575ZI-Q plugged into the UNO Q,
its endurance test's service stopped: the reports come over USART1 to the virtual COM
port of its ST-LINK, found as tools/soak.py finds it, and nothing is sent, SleepU5
receiving nothing there (SleepU5.c); the checks of the link then hold on no byte.

    tools/unoq_sleep.py SECONDS [nucleo]
"""
import binascii
import glob
import os
import random
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
WAKE_WAIT_S = 0.005    # the wake-up's 3 ms at most (OS_STOP2_WAKE_US), well inside the window
BRIDGE = ["arduino-router-serial.path", "arduino-router-serial", "arduino-router"]

NUCLEO = sys.argv[2:] == ["nucleo"]
if NUCLEO:
    tty = os.environ.get("NUCLEO_TTY") or next(
        iter(sorted(glob.glob("/dev/serial/by-id/usb-STMicroelectronics_STLINK*"))),
        "/dev/ttyACM0")
else:
    tty = "/dev/ttyHS1"
    subprocess.run(["sudo", "-n", "systemctl", "stop"] + BRIDGE, capture_output=True,
                   check=False)
fd = os.open(tty, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
attrs = termios.tcgetattr(fd)
attrs[0] = attrs[1] = attrs[3] = 0                        # raw
attrs[2] = termios.CS8 | termios.CREAD | termios.CLOCAL   # no flow control
attrs[4] = attrs[5] = termios.B115200                     # LPUART1 (Makefile), USART1
termios.tcsetattr(fd, termios.TCSANOW, attrs)
termios.tcflush(fd, termios.TCIOFLUSH)


def cobs(data):
    """data with its 0x00 taken out: each code byte gives the distance to the next."""
    out, block = bytearray(), bytearray()
    for b in data:
        if b == 0:
            out += bytes([len(block) + 1]) + block
            block = bytearray()
        else:
            block.append(b)
            if len(block) == 254:
                out += b"\xff" + block
                block = bytearray()
    return bytes(out + bytes([len(block) + 1]) + block)


def frame(data):
    """The count's bytes, their CRC-16 big-endian, COBS, between two 0x00."""
    crc = binascii.crc_hqx(data, 0xFFFF).to_bytes(2, "big")
    return b"\x00" + cobs(data + crc) + b"\x00"


seconds = float(sys.argv[1])
end = time.monotonic() + seconds
quiet = end - 3                              # the last reports count every byte sent
pending, last, points = b"", None, []        # points: (kernel's us, arrival)
sent, burst = 0, time.monotonic() + 1
while time.monotonic() < end:
    if not NUCLEO and burst <= time.monotonic() < quiet:
        data = bytes((sent + i) & 0xFF for i in range(random.randint(1, 32)))
        try:
            os.write(fd, b"\x00")
            time.sleep(WAKE_WAIT_S)
            framed = frame(data)
            if os.write(fd, framed) == len(framed):   # else dropped whole by SleepU5
                sent += len(data)
        except BlockingIOError:
            pass
        burst = time.monotonic() + random.uniform(0.1, 0.5)
    if select.select([fd], [], [], 0.05)[0]:
        now = time.monotonic()
        pending += os.read(fd, 4096)
        while b"\n" in pending:
            line, pending = pending.split(b"\n", 1)
            words = line.split()
            # SLEEP and 16 numbers
            if len(words) == 17 and words[0] == b"SLEEP":
                try:
                    last = [int(w, 16) for w in words[1:]]
                except ValueError:
                    continue
                points.append((last[2], now))

if last is None or len(points) < 3:
    sys.exit("SleepU5: no report")
(instances, ticks, micros, jitter, entries, wake, late, nolse, events, event_off,
 received, link_errors, overruns) = last[:13]
hse_missed, held, dropped = last[13:16]
n = len(points)
mx = sum(p[0] for p in points) / n
my = sum(p[1] for p in points) / n
slope = sum((p[0] - mx) * (p[1] - my) for p in points) / \
        sum((p[0] - mx) ** 2 for p in points)
rate = (slope * 1e6 - 1) * 1e6                  # ppm a second of the kernel's lasts longer
ppm = (micros - ticks * 1e6 / 32768) / micros * 1e6
print(f"SleepU5: {instances} instances, {entries} into Stop 2, gap off by {jitter} us "
      f"at most, longest wake-up {wake} ticks, the HSE missed {hse_missed}, {held} sleeps "
      f"held by the link's window, {late} late, {events} events off by "
      f"{event_off} us at most, {received} bytes of {sent} received, {link_errors} out "
      f"of the count, {overruns} overruns, {dropped} frames dropped, TIM2 {ppm:+.1f} ppm against "
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
if received != sent or link_errors or overruns:
    failures.append("bytes from Linux lost")
if abs(ppm) > MAX_PPM:
    failures.append("TIM2 and LPTIM1 apart")
if failures:
    sys.exit("SleepU5: " + ", ".join(failures))
