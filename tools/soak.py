#!/usr/bin/env python3
# Copyright (c) 2026 Bertrand Hurst. Part of Escapement, distributed under the terms of
# LICENSE at the root of this repository.
"""The endurance test on a board: load its firmware, let it run, read its counts at every
interval without stopping it, for as long as asked.

    tools/soak.py pico DURATION INTERVAL [ELF]      SoakPico, through the Debug Probe $PROBE
    tools/soak.py uno-q DURATION INTERVAL [ELF]     SoakU5, on the board's own Linux
    tools/soak.py nucleo DURATION INTERVAL [ELF]    SoakU5 on a NUCLEO-U575ZI-Q, through
                                                    its ST-LINK, beside the UNO Q
    tools/soak.py nucleo-u3 DURATION INTERVAL [ELF] SoakU3 on a NUCLEO-U385RG-Q, the same

DURATION and INTERVAL take a suffix s, m, h or d; a DURATION of 0 runs until stopped.

A Pico is read over SWD, through OpenOCD and the Debug Probe, which reads the RP2040's
memory as it runs. The STM32U585 of an Arduino UNO Q sends its counts once a second on
LPUART1, which the board's Linux sees as /dev/ttyHS1: the script runs there and reads
them, and sends the MCU bytes that count up by one, in bursts of random length at random
times, interrupts at moments of Linux's choosing that the firmware checks (SoakU5.c, the
link). Arduino's Bridge, which holds /dev/ttyHS1, is stopped meanwhile. A NUCLEO-U575ZI-Q
runs the same SoakU5, built in Examples/nucleo-u575, its reports and link on USART1 to the
virtual COM port of its ST-LINK ($NUCLEO_TTY, the ST-LINK's first under /dev/serial/by-id
by default), loaded by tools/nucleo_load.sh: a long run on a board of its own, which the
checks of each commit on the UNO Q do not interrupt. A NUCLEO-U385RG-Q runs SoakU3 of
Examples/nucleo-u385 the same way, with the same reports; plugged in beside the other, it
is named by $NUCLEO_TTY and $NUCLEO_SERIAL, and loaded by the OpenOCD $OPENOCD names,
newer than 0.12 (tools/board_ci.md).

Each reading appends a line to the log (BOARD_SOAK_LOG, soak-<board>-<date>.log in the
current directory by default): the seconds run by the firmware, the wraps of the kernel
clock crossed, the activity and the errors of each part, the worst lateness of the pulse
and of the timer events, the stack left unused, the work of the long task in the phase
of load drawn at random; on a Pico the reason of the last reset the watchdog block
records, and every hour the lateness by bins of 10 us; on the UNO Q the bytes of the link,
its errors and overruns, the causes of reset and the times the MSIS was locked again.

The test runs to its end whatever it finds. A reading whose marker is gone, whose
seconds went back, or, on the UNO Q, no report for 10 s, has seen the board restart,
into its firmware in flash since the image runs from SRAM: it is logged, the image is
loaded again, and the counts start over. Errors counted are added up across restarts, as
is a part that did not move between two readings, or seconds that fell behind the time
that passed. The test passes if none of these happened.

The state of a run, its start, its counts and its last reading, is kept beside the log
(BOARD_SOAK_STATE, the log's name and .state by default). The script started again with
the same board, image, commit and duration, as a service is after this machine
restarted, reads it back: if the image ran on meanwhile, its seconds and each part moving
on from the last reading, the run goes on where it was, without loading anything; if not,
the image is loaded again and the interruption is counted apart from the board's own
restarts, since the board did not cause it, though the run was not continuous. A run
that has ended is not started again. The UNO Q had rebooted twice on 2026-09-28, each
time starting the Nucleo's run over.

With a token (BOARD_CI_TOKEN, as tools/board_ci.sh), the state is posted to GitHub as the
commit status "board/soak" of BOARD_SOAK_SHA (HEAD of this checkout by default) on a Pico,
"board/soak-u5" on the UNO Q, "board/soak-nucleo" on the Nucleo: pending with the time
run, the restarts and the errors, failure as soon as either is not 0, success at the end
if both are. GitHub keeps at most 1,000 statuses for a commit and a context and refuses
the next: posted at each reading, a run's reached it in 16 hours and could post neither
its failure nor its end (2026-09-28). A status is therefore posted when the state or the
counts change, and otherwise once an hour. On a
Pico it holds the lock of its probe (BOARD_CI_LOCK, board-ci/lock-$PROBE) while it runs,
so that the board CI loads no other image there; it checks the other boards meanwhile.
"""
import atexit
import fcntl
import hashlib
import json
import glob
import os
import random
import select
import shutil
import signal
import subprocess
import sys
import tempfile
import termios
import threading
import time
import urllib.request

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
MARKER = 0x534F414B        # "SOAK", in SoakPico.c, SoakPico2.c and SoakU5.c


def seconds(text):
    """seconds("2h") -> 7200."""
    unit = {"s": 1, "m": 60, "h": 3600, "d": 86400}.get(text[-1:], None)
    return int(text[:-1]) * unit if unit else int(text)


def elapsed(d):
    return f"{d // 86400}d{d % 86400 // 3600:02d}h{d % 3600 // 60:02d}m"


def symbol(elf, name, needed=True):
    out = subprocess.run(["arm-none-eabi-nm", elf], capture_output=True, text=True,
                         check=True).stdout
    for line in out.splitlines():
        words = line.split()
        if len(words) == 3 and words[2] == name:
            return int(words[0], 16)
    if needed:
        sys.exit(f"{elf} has no {name}: not the endurance test")
    return None


class Pico:
    """SoakPico over SWD. Results, in words (SoakPico.c): 0 marker, 1 seconds, 2 wraps,
    3-10 activity, 11-18 errors, 19-20 lateness, 21-22 stack of each core, 23 the work of
    the long task in its phase,
    24-55 and 56-87 the lateness by bins of 10 us. SoakFirmPico has the structure Firm
    besides, as the five numbers SoakFirmU5 adds to its reports (UnoQ)."""
    name, context = "SoakPico", "board/soak"
    parts = ["pulse", "queue", "buffer", "events", "cores", "heartbeat", "interrupt",
             "memory", "firm"]
    elf = os.path.join(ROOT, "Escapement/CORTEX-Mx/RP2040/Examples/pico/build/SoakPico.elf")
    watchdog_reason = 0x40058008       # WATCHDOG_REASON, RP2040 datasheet
    words = 88

    def __init__(self, elf):
        self.elf = elf
        self.results = symbol(elf, "Results")
        self.firm = symbol(elf, "Firm", needed=False)
        # The Debug Probe, $PROBE of the bench's table (tools/probe.sh).
        self.probe = subprocess.run(["sh", os.path.join(ROOT, "tools/probe.sh")],
                                    stdout=subprocess.PIPE, text=True,
                                    check=True).stdout.strip()
        # The lock of that probe (tools/board_ci.sh, hold), not of the whole bench: the
        # board CI goes on checking the boards wired to the other probes.
        lock = os.environ.get("BOARD_CI_LOCK", os.path.expanduser(
            "~/escapement-rp2040/board-ci/lock-" + os.environ.get("PROBE", "probe1")))
        os.makedirs(os.path.dirname(lock), exist_ok=True)
        # Under the flock the board CI takes its locks under (tools/board_ci.sh, take):
        # two processes taking over one stale lock at once would both hold it.
        with open(os.path.join(os.path.dirname(lock), "lock-take"), "w") as serial:
            fcntl.flock(serial, fcntl.LOCK_EX)
            try:
                os.mkdir(lock)
            except FileExistsError:
                # Taken over if its process is gone, as the board CI does: a run as a
                # service is started again after the board rebooted, which left the lock
                # behind.
                try:
                    with open(os.path.join(lock, "pid")) as f:
                        os.kill(int(f.read()), 0)
                    sys.exit(f"the board is in use ({lock})")
                except (OSError, ValueError):
                    shutil.rmtree(lock, True)
                    os.mkdir(lock)
            with open(os.path.join(lock, "pid"), "w") as f:
                f.write(str(os.getpid()))
        atexit.register(shutil.rmtree, lock, True)
        # systemd stops a service with SIGTERM, which would skip the line above.
        signal.signal(signal.SIGTERM, lambda *_: sys.exit(1))

    def ocd(self, *commands):
        """OpenOCD with the Debug Probe and the RP2040, core 0 only; its output."""
        args = ["openocd", "-f", "interface/cmsis-dap.cfg", "-c", self.probe,
                "-c", "adapter speed 5000", "-c", "set USE_CORE 0",
                "-f", "target/rp2040.cfg", "-c", "init"]
        for command in commands:
            args += ["-c", command]
        # Bounded: an OpenOCD stuck on the USB held the probe's lock and left the status
        # pending for good (a review, 2026-09-30). Its output then counts as no reading.
        try:
            out = subprocess.run(args + ["-c", "exit"], capture_output=True, text=True,
                                 check=False, timeout=60)
        except subprocess.TimeoutExpired:
            return "Error: OpenOCD did not answer within 60 s"
        return out.stdout + out.stderr

    def words_at(self, address, n):
        values = []
        for line in self.ocd(f"mdw {address:#x} {n}").splitlines():
            if line.startswith("0x") and ": " in line:
                values += [int(w, 16) for w in line.split(": ", 1)[1].split()]
        return values if len(values) == n else None

    def load(self):
        self.ocd("reset halt", f"load_image {self.elf}", "resume 0x20000000")

    def start(self, reload=False):
        self.load()

    def take_over(self, seconds):
        """Whether the image runs on, without stopping it: the marker, at least seconds
        counted, the count moving on over two seconds, and its first 64 words in SRAM those
        of the ELF. An image left halted in SRAM kept the rest (2026-09-28)."""
        r = self.read()
        if r is None or r["marker"] != MARKER or r["seconds"] < seconds:
            return False
        time.sleep(2)
        later = self.read()
        if later is None or later["marker"] != MARKER or later["seconds"] <= r["seconds"]:
            return False
        with tempfile.NamedTemporaryFile() as binary:
            subprocess.run(["arm-none-eabi-objcopy", "-O", "binary", self.elf, binary.name],
                           check=True)
            image = binary.read()
        words = [int.from_bytes(image[i:i + 4], "little") for i in range(0, 256, 4)]
        return self.words_at(0x20000000, 64) == words

    def read(self):
        w = self.words_at(self.results, self.words)
        reason = self.words_at(self.watchdog_reason, 1)
        firm = self.words_at(self.firm, 5) if self.firm else None
        if w is None or (self.firm and firm is None):
            return None
        activity, errors = w[3:11], w[11:19]
        extra = f", reset {reason[0]:#010x}" if reason else ""
        if firm:
            activity, errors = activity + [firm[0]], errors + [firm[3]]
            extra += f", firm {firm[1]} optional run, {firm[2]} dropped, late max {firm[4]} us"
        return {"marker": w[0], "seconds": w[1], "wraps": w[2], "activity": activity,
                "errors": errors, "late": (w[19], w[20]), "stack": (w[21], w[22]),
                "load": w[23], "bins": (w[24:56], w[56:88]), "extra": extra, "link": None}


class UnoQ:
    """SoakU5 on LPUART1: a report a second, SOAK and 28 hexadecimal numbers (SoakU5.c):
    seconds, wraps, the activity and the errors of the eight parts, the bytes of the link,
    its errors and overruns, the lateness of the pulse and of the timer events, the stack
    never used, the work of the long task in its phase, the byte the link expects next,
    and the flags of reset the run found in RCC_CSR, with in bits 0 to 23 the times the
    MSIS was locked again on the LSE (erratum 2.2.27 of the chip), and the longest burst
    of the link: the bytes the interrupt found waiting in the FIFO, which came over at
    least one byte time each but the first, and the time the kernel's clock counted since
    the byte before them. Less than that, and the clock stopped while the UART received.
    SoakFirmU5 adds five: the (m,k)-firm task's mandatory instances run, its optional ones
    run and dropped, its errors and its worst time from an arrival to the end of its
    instance; the mandatory instances count as the activity of a ninth part, the errors
    as its errors."""
    name, context = "SoakU5", "board/soak-u5"
    parts = ["pulse", "queue", "buffer", "events", "buffer4", "heartbeat", "interrupt",
             "memory", "firm"]
    elf = os.path.expanduser("~/soak/SoakU5.elf")
    tty = "/dev/ttyHS1"
    baud = termios.B115200   # SoakU5's LPUART1 (Examples/uno-q/Makefile)
    byte_us = 10e6 / 115200  # a start bit, 8 data bits, a stop bit
    silent = 10         # seconds without a report that say the board restarted
    settle_s = 3        # seconds of reading before a report may set the link's count
    bridge = ["arduino-router-serial.path", "arduino-router-serial", "arduino-router"]
    loader = "unoq_load.sh"
    loader_env = {}

    def __init__(self, elf):
        self.elf = elf
        symbol(elf, "Results")
        # The length of this image's reports, and no other: on 2026-10-09 some 20 bytes
        # lost in the middle of a report of SoakFirmU5 left 27 words, the length of an
        # older image, and its numbers were read shifted, 67,879,873 errors for one line.
        self.words = 34 if symbol(elf, "Firm", needed=False) is not None else 29
        if self.bridge:
            subprocess.run(["sudo", "-n", "systemctl", "stop"] + self.bridge,
                           capture_output=True, check=False)
        # Not blocking: the thread that writes shares the descriptor.
        self.fd = os.open(self.tty, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
        attrs = termios.tcgetattr(self.fd)
        attrs[0] = attrs[1] = attrs[3] = 0                     # raw
        attrs[2] = termios.CS8 | termios.CREAD | termios.CLOCAL   # no flow control
        attrs[4] = attrs[5] = self.baud
        termios.tcsetattr(self.fd, termios.TCSANOW, attrs)
        termios.tcflush(self.fd, termios.TCIOFLUSH)
        self.last, self.sent, self.value = None, 0, None
        self.lock = threading.Lock()   # self.value, between the link and a restart
        self.begin()

    def begin(self):
        """The reader, a thread that drains the port as the reports come. Read only at
        each interval, the port held a minute of reports, some 18 kB against the 4 kB of
        Linux's buffer: the driver held the ST-LINK back, which dropped what it could not
        keep, and the reports a NUCLEO-U385RG-Q sent were cut short once a minute
        (2026-10-09); the link's count taken over from a report that old was behind the
        board's, an error of the link at the first byte."""
        self.arrived = threading.Condition()   # what follows, between the reader and all
        self.pending, self.latest, self.rejected, self.more = b"", None, [], 0
        self.synced, self.settle = False, time.monotonic() + self.settle_s
        threading.Thread(target=self.receive, daemon=True).start()

    def forget(self):
        """What the reader holds, once the port was emptied: a report from then on, and
        the link's count from one received once the old bytes had time to drain."""
        with self.arrived:
            self.pending, self.latest = b"", None
            self.synced, self.settle = False, time.monotonic() + self.settle_s

    def load(self):
        """Through the loader beside this script, tools/unoq_load.sh run on the board, or
        tools/nucleo_load.sh. Returns OpenOCD's error if the load failed, None otherwise:
        one that failed unseen, the image before left running, passed for restarts of the
        board (2026-09-27)."""
        loader = os.path.join(os.path.dirname(os.path.abspath(__file__)), self.loader)
        # The image loaded expects the link from 0: the count of the one before, which a
        # take-over that failed may have read, would make an error of its first byte (the
        # U5's run after the failed check of d9a32eb, 2026-10-02). The link waits for the
        # new image's first report, as after a restart, its reports and bytes still in
        # the driver dropped.
        with self.lock:
            self.value = None
            termios.tcflush(self.fd, termios.TCIOFLUSH)
        self.forget()
        done = subprocess.run(["sh", loader, self.elf], capture_output=True, text=True,
                              check=False, env=dict(os.environ, **self.loader_env))
        # The image before reported on until the loader stopped it: what came meanwhile
        # is dropped too, lest its count be taken for the new one's (a review, 2026-10-03).
        with self.lock:
            self.value = None
            termios.tcflush(self.fd, termios.TCIFLUSH)
        self.forget()
        self.last = None
        if done.returncode == 0:
            return None
        errors = [l for l in (done.stdout + done.stderr).splitlines()
                  if l.startswith("Error")]
        return errors[0] if errors else f"the loader ended with {done.returncode}"

    def start(self, reload=False):
        """The image already running is read as it is, not loaded again: a test can be
        taken over. Loaded if no report comes, or if reload."""
        if reload or self.report(self.silent) is None:
            failed = self.load()
            if failed:
                print(f"LOAD FAILED: {failed}", flush=True)
        threading.Thread(target=self.link, daemon=True).start()

    def take_over(self, seconds):
        """Whether reports come with at least seconds counted and the count moving on; the
        link then goes on from the byte the MCU expects."""
        r = self.report(self.silent)
        if r is None or r[0] < seconds:
            return False
        time.sleep(2)
        later = self.report(self.silent)
        if later is None or later[0] <= r[0]:
            return False
        threading.Thread(target=self.link, daemon=True).start()
        return True

    def link(self):
        """A count, in bursts of 1 to 64 bytes, 1 to 100 ms apart, going on from the byte
        the MCU expects: a script started anew makes no error of the link."""
        while True:
            time.sleep(random.uniform(0.001, 0.1))
            with self.lock:
                if self.value is None:
                    continue
                burst = bytes((self.value + i) & 0xFF
                              for i in range(random.randint(1, 64)))
                try:
                    n = os.write(self.fd, burst)
                except BlockingIOError:
                    n = 0      # the driver's buffer full: the count goes on from here
                self.value = (self.value + n) & 0xFF
                self.sent += n

    def report(self, wait):
        """The newest report, if it came within the last second (they come once a second),
        or the next within wait seconds; None if none."""
        fresh = time.monotonic() - 1
        with self.arrived:
            self.arrived.wait_for(lambda: self.latest and self.latest[0] >= fresh, wait)
            return self.latest[1] if self.latest and self.latest[0] >= fresh else None

    def receive(self):
        while True:
            try:
                if not select.select([self.fd], [], [], 1)[0]:
                    continue
                data = os.read(self.fd, 4096)
            except BlockingIOError:
                continue
            except (OSError, ValueError):
                return             # the port closed, as at the end of the self-test
            with self.arrived:
                self.pending += data
                while b"\n" in self.pending:
                    line, self.pending = self.pending.split(b"\n", 1)
                    # The first line after the port was opened or emptied may be the end
                    # of one: it is not taken, nor counted as rejected.
                    if not self.synced:
                        self.synced = True
                        continue
                    numbers = self.parse(line)
                    if numbers is None:
                        # Kept as received, for the log: a report that lost or changed
                        # bytes on the way, which the board does not count.
                        if not line.strip():
                            pass
                        elif len(self.rejected) < 20:
                            self.rejected.append(line)
                        else:
                            self.more += 1
                        continue
                    self.latest = (time.monotonic(), numbers)
                    self.arrived.notify_all()
                    if self.latest[0] >= self.settle:
                        with self.lock:
                            # An image taken over has counted what was sent to it before,
                            # by the board's check of 2 min among others: 78,000 bytes
                            # more received than sent on the U5 (2026-10-01).
                            if self.value is None:
                                self.value, self.sent = numbers[25], numbers[18]
                self.pending = self.pending[-4096:]   # what is no report never piles up

    def parse(self, line):
        """SOAK and 28 numbers, 33 from SoakFirmU5: this image's length and no other."""
        words = line.decode(errors="replace").split()
        if len(words) != self.words or words[0] != "SOAK":
            return None
        try:
            return [int(w, 16) for w in words[1:]]
        except ValueError:
            return None

    def read(self):
        r = self.report(self.silent)
        if r is None:
            # No report for 10 s: the board restarted, into Arduino's firmware. The link
            # waits for the first report of the image loaded next, which expects 0, the
            # old count still waiting in the driver dropped (a restart, 2026-09-26, made
            # the new image count an error of the link).
            with self.lock:
                self.value = None
                termios.tcflush(self.fd, termios.TCOFLUSH)
            with self.arrived:
                self.settle = time.monotonic() + self.settle_s
            return {"marker": 0, "seconds": 0, "rejected": self.take_rejected()}
        resets = (f", reset {self.resets(r[26])}, MSI locked again {r[26] & 0xFFFFFF}"
                  if len(r) > 26 else "")
        if len(r) > 27 and r[27] >> 24:
            n, gap = r[27] >> 24, r[27] & 0xFFFFFF
            resets += f", longest burst {n} bytes after {gap} us"
            if gap < (n - 1) * self.byte_us:
                resets += f" (the clock lost {(n - 1) * self.byte_us - gap:.0f} us at least)"
        activity, errors = r[2:10], r[10:18]
        if len(r) > 28:
            activity, errors = activity + [r[28]], errors + [r[31]]
            resets += f", firm {r[29]} optional run, {r[30]} dropped, late max {r[32]} us"
        return {"marker": MARKER, "rejected": self.take_rejected(),
                "seconds": r[0], "wraps": r[1], "activity": activity,
                "errors": errors, "late": (r[21], r[22]), "stack": (r[23],),
                "load": r[24], "bins": None,
                "extra": f", link {r[18]} bytes of {self.sent} sent, {r[19]} errors, "
                         f"{r[20]} overruns{resets}", "link": r[18:21]}

    def take_rejected(self):
        with self.arrived:
            rejected, self.rejected = self.rejected, []
            if self.more:
                rejected.append(f"and {self.more} more".encode())
                self.more = 0
        return rejected

    @staticmethod
    def resets(flags):
        """The flags of reset of RCC_CSR the run found (RM0456), by name. A load resets
        the pin; a first start after power adds BOR; the independent watchdog, IWDG."""
        names = [(25, "OBL"), (26, "pin"), (27, "BOR"), (28, "software"), (29, "IWDG"),
                 (30, "WWDG"), (31, "low-power")]
        return "+".join(n for b, n in names if flags >> b & 1) or "none"


class Nucleo(UnoQ):
    """SoakU5 on a NUCLEO-U575ZI-Q: the same reports and link, on USART1 to the virtual
    COM port of its ST-LINK, and no Bridge to stop."""
    name, context = "SoakU5 (NUCLEO-U575ZI-Q)", "board/soak-nucleo"
    elf = os.path.expanduser("~/soak-nucleo/SoakU5.elf")
    tty = os.environ.get("NUCLEO_TTY") or next(
        iter(sorted(glob.glob("/dev/serial/by-id/usb-STMicroelectronics_STLINK*"))),
        "/dev/ttyACM0")
    baud = termios.B115200   # USART1 (Escapement_UART.c)
    byte_us = 10e6 / 115200
    bridge = []
    loader = "nucleo_load.sh"


class NucleoU3(Nucleo):
    """SoakU3 on a NUCLEO-U385RG-Q: the same reports and link on USART1, its ST-LINK named
    by $NUCLEO_TTY and $NUCLEO_SERIAL beside the NUCLEO-U575ZI-Q's."""
    name, context = "SoakU3 (NUCLEO-U385RG-Q)", "board/soak-u3"
    elf = os.path.expanduser("~/soak-u3/SoakU3.elf")
    loader_env = {"NUCLEO_MCU": "u385"}


class Status:
    """The commit status on GitHub, if a token is there."""

    def __init__(self, context):
        self.context = context
        self.posted = (None, None, 0.0)   # state, counts, when
        self.repo = os.environ.get("BOARD_CI_REPO", "beber007/escapement")
        token = os.environ.get("BOARD_CI_TOKEN",
                               os.path.expanduser("~/.config/escapement-board-ci/token"))
        self.token = open(token).read().strip() if os.access(token, os.R_OK) else None
        self.sha = os.environ.get("BOARD_SOAK_SHA") or subprocess.run(
            ["git", "-C", ROOT, "rev-parse", "HEAD"], capture_output=True,
            text=True).stdout.strip()

    def post(self, state, description, counts=None):
        """Posts now if the state or the counts changed, or an hour after the last."""
        if not self.token or not self.sha:
            return
        if counts is not None and self.posted[:2] == (state, counts) and \
                time.monotonic() - self.posted[2] < 3600:
            return
        self.posted = (state, counts, time.monotonic())
        request = urllib.request.Request(
            f"https://api.github.com/repos/{self.repo}/statuses/{self.sha}",
            data=json.dumps({"state": state, "context": self.context,
                             "description": description[:140]}).encode(),
            headers={"Authorization": f"Bearer {self.token}",
                     "Accept": "application/vnd.github+json"}, method="POST")
        try:
            urllib.request.urlopen(request, timeout=30).close()
        except OSError:
            pass


def self_test():
    """Known cases of the link, with no board: a pseudo-terminal for the port, the reader
    on it, and a loader that does nothing."""
    import tempfile
    master, slave = os.openpty()
    board = object.__new__(UnoQ)
    board.fd, board.lock, board.elf = slave, threading.Lock(), os.devnull
    board.words, board.settle_s = 29, 60
    board.value, board.sent, board.last = 0x37, 123, 1
    board.begin()
    with tempfile.NamedTemporaryFile("w", suffix=".sh", delete=False) as loader:
        loader.write("exit 0\n")
    board.loader = loader.name

    def send(*lines):
        """Lines as the board sends them, and the reader past the last."""
        os.write(master, b"".join(line.encode() + b"\n" for line in lines))
        last = board.parse(lines[-1].encode())
        with board.arrived:
            assert board.arrived.wait_for(
                lambda: board.latest and board.latest[1] == last, 2), lines[-1]

    def report(n, **values):
        numbers = [f"{values.get(f'n{i}', 1):x}" for i in range(n)]
        return "SOAK " + " ".join(numbers)
    try:
        # A load forgets the count read from the image before, and what the driver held:
        # the image loaded expects the link from 0 (the failed check of d9a32eb).
        board.pending = b"SOAK"
        assert board.load() is None
        assert board.value is None and board.pending == b"" and board.last is None, \
            (board.value, board.pending)
        # The end of a line the board was sending when the port was emptied: dropped,
        # not rejected. A report before the old bytes had time to drain is read, but sets
        # no count of the link: the take-over of the NUCLEO-U385RG-Q on 2026-10-09 took
        # one a minute old, behind the board's.
        os.write(master, b" 0 0 5\n")
        send(report(28, n0=7, n18=0x30, n25=9))
        assert board.report(1) is not None and board.value is None, board.value
        # Past that, the first report sets it: byte 25 expected, 18 received.
        board.settle_s = 0
        assert board.load() is None
        os.write(master, b" 0 0 5\n")
        send(report(28, n0=8, n18=0x12, n25=5))
        assert (board.value, board.sent) == (5, 0x12), (board.value, board.sent)
        assert board.take_rejected() == []
        # A report of SoakFirmU5: the firm task a ninth part, its errors counted.
        board.words = 34
        send(report(33, n0=9, n31=2))
        r = board.read()
        assert len(r["activity"]) == 9 and r["errors"][8] == 2, r
        assert "firm 1 optional run, 1 dropped" in r["extra"], r["extra"]
        assert r["rejected"] == [], r["rejected"]
        # The report of 2026-10-09 16:55, 20 bytes lost from its middle: 27 words, once
        # the length of an older image's, read shifted. Rejected and kept as received; the
        # good report after it read.
        lost = ("SOAK 9b1b 24 25dd6a0 8163a5c 9d5a0d 792c53 12eecd0 9b1b 5433012 9b1b 0 "
                "8 0 0 38 2b bbc44 712 c8 4000000 0 307a52 2d4a6f 1b6d12 0 fb0")
        send(lost, report(33, n0=10, n31=2))
        r = board.read()
        assert r["rejected"] == [lost.encode()], r["rejected"]
        assert r["errors"][8] == 2 and sum(r["errors"][:8]) == 8, r["errors"]
        # Beyond 20 lines rejected between two readings, only their number.
        send(*["garbled"] * 25, report(33, n0=11))
        rejected = board.take_rejected()
        assert len(rejected) == 21 and rejected[-1] == b"and 5 more", rejected[-3:]
    finally:
        os.unlink(loader.name)
        os.close(master)
        os.close(slave)
    print("self-test passed")


def main():
    if sys.argv[1:] == ["--self-test"]:
        self_test()
        return
    boards = {"pico": Pico, "uno-q": UnoQ, "nucleo": Nucleo, "nucleo-u3": NucleoU3}
    if len(sys.argv) < 4 or sys.argv[1] not in boards:
        sys.exit(__doc__.split("\n\n")[1])
    kind = boards[sys.argv[1]]
    duration, interval = seconds(sys.argv[2]), seconds(sys.argv[3])
    elf = sys.argv[4] if len(sys.argv) > 4 else kind.elf
    if not os.path.isfile(elf):
        sys.exit(f"no image {elf}")
    board = kind(elf)
    log = os.environ.get("BOARD_SOAK_LOG",
                         f"soak-{sys.argv[1]}-{time.strftime('%Y%m%d-%H%M%S')}.log")
    status = Status(board.context)
    state_file = os.environ.get("BOARD_SOAK_STATE", log + ".state")
    with open(elf, "rb") as f:
        run_id = {"board": sys.argv[1], "sha": status.sha, "duration": duration,
                  "image": hashlib.sha256(f.read()).hexdigest()}

    def write(line, show=False):
        with open(log, "a") as out:
            out.write(line + "\n")
        if show:
            print(line, flush=True)

    def save(ended=False):
        with open(state_file + ".new", "w") as out:
            json.dump(dict(run_id, start=start, restarts=restarts, errors=errors,
                           wraps=wraps, interruptions=interruptions, previous=previous,
                           last_hour=last_hour, ended=ended, at=time.time()), out)
        os.replace(state_file + ".new", state_file)

    def summarise():
        return (f"{restarts} restarts, {errors} errors, {wraps} wraps" +
                (f", {interruptions} interrupted" if interruptions else ""))

    try:
        with open(state_file) as f:
            saved = json.load(f)
        if any(saved.get(k) != v for k, v in run_id.items()):
            saved = None
    except (OSError, ValueError):
        saved = None
    span = f"for {elapsed(duration)}" if duration else "until stopped"
    if saved and saved["ended"]:
        sys.exit(f"this run has ended ({state_file})")
    if saved:
        start, restarts, errors = saved["start"], saved["restarts"], saved["errors"]
        wraps, interruptions = saved["wraps"], saved["interruptions"]
        previous, last_hour = saved["previous"], saved["last_hour"]
        stamp = time.strftime("%Y-%m-%d %H:%M:%S")
        gap = int(time.time() - saved["at"])
        # The image ran on only if its seconds kept up with the time this script was
        # away, within the 5 % the readings allow.
        if board.take_over(previous[0] + int(gap * 0.95) if previous else 0):
            write(f"{stamp} TAKEN OVER after {gap} s: the image ran on", True)
        else:
            interruptions += 1
            write(f"{stamp} INTERRUPTED {interruptions}: the image did not run on "
                  f"through {gap} s without this script, loaded again", True)
            board.start(reload=True)
            previous = None
    else:
        board.start()
        start = time.time()
        write(f"{board.name} {status.sha or '(no commit)'}, {time.ctime()}, {span}, "
              f"read every {interval} s, {elf}", True)
        status.post("pending", f"running since {time.strftime('%Y-%m-%d')}")
        restarts = errors = wraps = interruptions = 0
        previous = None           # seconds, errors found, activity, time, wraps
        last_hour = start
    save()
    while True:
        time.sleep(interval)
        now = time.time()
        stamp = time.strftime("%Y-%m-%d %H:%M:%S")
        r = board.read()
        for line in (r or {}).get("rejected", []):
            write(f"{stamp} REJECTED {len(line)} bytes: {line!r}", True)
        if r is None:
            write(f"{stamp} no reading from the board", True)
            errors += 1
        elif r["marker"] != MARKER or (previous and r["seconds"] < previous[0]):
            restarts += 1
            write(f"{stamp} RESTART {restarts}{r.get('extra', '')}", True)
            failed = board.load()
            if failed:
                write(f"{stamp} LOAD FAILED: {failed}", True)
            previous = None
        else:
            line = f"{stamp} run {r['seconds']} s, {r['wraps']} wraps"
            for part, a, e in zip(board.parts, r["activity"], r["errors"]):
                line += f", {part} {a}/{e}"
            line += f", late max {r['late'][0]}/{r['late'][1]} us, stack free " + \
                "/".join(str(s) for s in r["stack"]) + \
                f", load {r['load']} us{r['extra']}"
            found = sum(r["errors"]) + (r["link"][1] if r["link"] else 0)
            write(line)
            if found:
                write(f"{stamp} ERRORS {found}: {line}", True)
            new = found
            if previous:
                prev_secs, prev_found, prev_activity, prev_now, prev_wraps = previous
                new = found - prev_found
                # The firmware's seconds, against the time this machine saw pass, within 5 %.
                if r["seconds"] - prev_secs < (now - prev_now) * 0.95:
                    write(f"{stamp} SECONDS BEHIND: {line}", True)
                    new += 1
                # Modulo 2^32: the queue's count, some 4,000 a second on the Pico, wraps
                # after 12.5 days, within a run of two weeks (2026-09-25).
                moving = r["activity"] + ([r["link"][0]] if r["link"] else [])
                if any(((a - b) & 0xFFFFFFFF) == 0 for a, b in zip(moving, prev_activity)):
                    write(f"{stamp} STOPPED: {line}", True)
                    new += 1
                wraps += r["wraps"] - prev_wraps
            else:
                wraps += r["wraps"]
            errors += new
            if r["bins"] and now - last_hour >= 3600:
                for what, bins in zip(("pulse", "events"), r["bins"]):
                    write(f"  {what} late by us: " +
                          " ".join(f"{i * 10}:{v}" for i, v in enumerate(bins) if v))
                last_hour = now
            previous = [r["seconds"], found,
                        r["activity"] + ([r["link"][0]] if r["link"] else []), now,
                        r["wraps"]]
        save()
        run = elapsed(int(now - start)) + (f" of {elapsed(duration)}" if duration else "")
        summary = summarise()
        status.post("failure" if restarts + errors else "pending", f"{run}: {summary}",
                    counts=(restarts, errors, interruptions))
        if duration and now - start >= duration:
            break
    save(ended=True)
    write(f"end after {elapsed(int(now - start))}: {summary}", True)
    if restarts + errors + interruptions == 0:
        status.post("success", f"{elapsed(int(now - start))} without error: {wraps} wraps")
        sys.exit(0)
    status.post("failure", f"{elapsed(int(now - start))}: {summary}")
    sys.exit(1)


if __name__ == "__main__":
    main()
