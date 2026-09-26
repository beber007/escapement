#!/usr/bin/env python3
# Copyright (c) 2026 Bertrand Hurst. Part of Escapement, distributed under the terms of
# LICENSE at the root of this repository.
"""The endurance test on the STM32U585 of an Arduino UNO Q, run by the board's own Linux.

SoakU5 sends its counts once a second on LPUART1, which Linux sees as /dev/ttyHS1, and
this script reads them there: no debugger, no halt of the core. The other way, it sends
the MCU bytes that count up by one, in bursts of random length at random times, so that
Linux adds interrupts at moments of its own to those of the test (SoakU5.c, the link).

At every interval it logs a line: the seconds run by the firmware, the wraps of the
kernel clock crossed, the errors each part counted, those of the link and the bytes it
lost, the parts that did not move since the line before, the worst lateness of the pulse
and of the timer events, the load phase, and the restarts. No report for 10 s, or
seconds that went back, is a restart of the board, into Arduino's firmware since the
image runs from SRAM: it is logged, the image loaded again through the board's OpenOCD,
and the counts start over, the errors added up across restarts.

Arduino's Bridge, which holds /dev/ttyHS1 to talk to Arduino's firmware on the MCU, is
stopped while the test runs: that firmware is not running.

    tools/soak_unoq.py ELF [INTERVAL]      INTERVAL in seconds, 60 by default
    SOAK_UNOQ_LOG                          the log, ~/soak-u5-<date>.log by default
"""
import os
import random
import select
import subprocess
import sys
import termios
import threading
import time

TTY = "/dev/ttyHS1"
PARTS = ["pulse", "queue", "buffer", "events", "buffer4", "heartbeat", "interrupt",
         "memory"]
SILENT = 10          # seconds without a report that say the board restarted
BRIDGE = ["arduino-router-serial.path", "arduino-router-serial", "arduino-router"]


def log(line):
    with open(LOG, "a") as out:
        out.write(line + "\n")


def open_tty():
    # Not blocking: a blocking descriptor left the reads waiting in select while reports
    # came in, the thread that writes on it sharing it.
    fd = os.open(TTY, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
    attrs = termios.tcgetattr(fd)
    attrs[0] = 0                                                # no input processing
    attrs[1] = 0                                                # nor output processing
    attrs[2] = termios.CS8 | termios.CREAD | termios.CLOCAL     # no flow control
    attrs[3] = 0                                                # raw
    attrs[4] = attrs[5] = termios.B115200
    attrs[6][termios.VMIN] = 1
    attrs[6][termios.VTIME] = 0
    termios.tcsetattr(fd, termios.TCSANOW, attrs)
    termios.tcflush(fd, termios.TCIOFLUSH)
    return fd


def load(elf):
    """As tools/unoq_load.sh: the timers frozen while the debugger halts the core."""
    subprocess.run(["./bin/openocd", "-s", "/opt/openocd", "-f", "openocd_gpiod.cfg", "-c",
                    "reset_config srst_only srst_push_pull; init; reset halt; "
                    f"mww 0xE0044008 0xb; load_image {elf}; resume 0x20000000; shutdown"],
                   cwd="/opt/openocd", capture_output=True, check=False)


class Link(threading.Thread):
    """The bytes sent to the MCU: a count, in bursts of 1 to 64 bytes, 1 to 100 ms apart."""

    def __init__(self, fd):
        super().__init__(daemon=True)
        self.fd, self.sent, self.value = fd, 0, 0

    def run(self):
        while True:
            time.sleep(random.uniform(0.001, 0.1))
            burst = bytes((self.value + i) & 0xFF for i in range(random.randint(1, 64)))
            try:
                n = os.write(self.fd, burst)
            except BlockingIOError:
                n = 0          # the driver's buffer full: the count goes on from here
            self.value = (self.value + n) & 0xFF
            self.sent += n


def reports(fd):
    """Each report of SoakU5 as a list of numbers, or None after SILENT s without one: the
    time runs from the last valid report, whatever else comes in, so that Arduino's
    firmware, running after a restart, cannot keep the test from being loaded again."""
    pending = b""
    deadline = time.monotonic() + SILENT
    while True:
        while b"\n" not in pending:
            left = deadline - time.monotonic()
            if left <= 0:
                yield None
                deadline = time.monotonic() + SILENT
                continue
            if select.select([fd], [], [], left)[0]:
                try:
                    pending += os.read(fd, 512)
                except BlockingIOError:
                    pass
            if len(pending) > 4096:
                pending = pending[-512:]      # what is no report never piles up
        line, pending = pending.split(b"\n", 1)
        words = line.decode(errors="replace").split()
        if len(words) == 27 and words[0] == "SOAK":   # the word SOAK and 26 numbers
            try:
                report = [int(w, 16) for w in words[1:]]
            except ValueError:
                continue
            deadline = time.monotonic() + SILENT
            yield report


def main():
    elf = sys.argv[1]
    interval = int(sys.argv[2]) if len(sys.argv) > 2 else 60
    subprocess.run(["sudo", "-n", "systemctl", "stop"] + BRIDGE, capture_output=True)
    fd = open_tty()
    link = Link(fd)
    log(f"SoakU5 on the UNO Q, {time.ctime()}, logged every {interval} s, {elf}")
    restarts = errors_before = stalls = 0
    run_errors = 0
    previous = None          # the activity at the last line logged
    last = None              # the last report
    next_log = time.monotonic() + interval
    for report in reports(fd):
        now = time.strftime("%Y-%m-%d %H:%M:%S")
        if report is None or (last is not None and report[0] < last[0]):
            restarts += 1
            errors_before += run_errors
            run_errors, previous, last = 0, None, None
            why = "no report" if report is None else "seconds went back"
            log(f"{now} RESTART {restarts}: {why}; loaded again")
            load(elf)
            continue
        last = report
        if not link.is_alive():
            # The count goes on from the byte the MCU expects: a script started anew
            # makes no error of the link.
            link.value = report[25]
            link.start()
        if time.monotonic() < next_log:
            continue
        next_log += interval
        secs, wraps = report[0], report[1]
        activity, errors = report[2:10], report[10:18]
        link_bytes, link_errors, overruns = report[18:21]
        pulse_late, event_late, stack, high = report[21:25]
        run_errors = sum(errors) + link_errors
        stalled = []
        if previous is not None:
            stalled = [p for p, a, b in zip(PARTS + ["link"], activity + [link_bytes],
                                             previous) if a == b]
        stalls += bool(stalled)
        previous = activity + [link_bytes]
        total = errors_before + run_errors + stalls
        line = (f"{now} run {secs} s, {wraps} wraps, part errors {sum(errors)}, link "
                f"{link_bytes} bytes of {link.sent} sent, {link_errors} errors, "
                f"{overruns} overruns, restarts {restarts}, late max {pulse_late} / "
                f"{event_late} us, stack free {stack}, load {'high' if high else 'low'}")
        if stalled:
            line += ", not moving: " + " ".join(stalled)
        log(f"ERRORS {total}: {line}" if total or restarts else line)


LOG = os.environ.get("SOAK_UNOQ_LOG",
                     os.path.expanduser(f"~/soak-u5-{time.strftime('%Y%m%d-%H%M')}.log"))

if __name__ == "__main__":
    main()
