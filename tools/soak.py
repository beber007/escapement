#!/usr/bin/env python3
# Copyright (c) 2026 Bertrand Hurst. Part of Escapement, distributed under the terms of
# LICENSE at the root of this repository.
"""The endurance test on a board: load its firmware, let it run, read its counts at every
interval without stopping it, for as long as asked.

    tools/soak.py pico DURATION INTERVAL [ELF]      SoakPico, through the Debug Probe $PROBE
    tools/soak.py uno-q DURATION INTERVAL [ELF]     SoakU5, on the board's own Linux

DURATION and INTERVAL take a suffix s, m, h or d; a DURATION of 0 runs until stopped.

A Pico is read over SWD, through OpenOCD and the Debug Probe, which reads the RP2040's
memory as it runs. The STM32U585 of an Arduino UNO Q sends its counts once a second on
LPUART1, which the board's Linux sees as /dev/ttyHS1: the script runs there and reads
them, and sends the MCU bytes that count up by one, in bursts of random length at random
times, interrupts at moments of Linux's choosing that the firmware checks (SoakU5.c, the
link). Arduino's Bridge, which holds /dev/ttyHS1, is stopped meanwhile.

Each reading appends a line to the log (BOARD_SOAK_LOG, soak-<board>-<date>.log in the
current directory by default): the seconds run by the firmware, the wraps of the kernel
clock crossed, the activity and the errors of each part, the worst lateness of the pulse
and of the timer events, the stack left unused, the work of the long task in the phase
of load drawn at random; on a Pico the reason of
the last reset the watchdog block records, and every hour the lateness by bins of 10 us;
on the UNO Q the bytes of the link, its errors and overruns.

The test runs to its end whatever it finds. A reading whose marker is gone, whose
seconds went back, or, on the UNO Q, no report for 10 s, has seen the board restart,
into its firmware in flash since the image runs from SRAM: it is logged, the image is
loaded again, and the counts start over. Errors counted are added up across restarts, as
is a part that did not move between two readings, or seconds that fell behind the time
that passed. The test passes if none of these happened.

With a token (BOARD_CI_TOKEN, as tools/board_ci.sh), the state is posted to GitHub as the
commit status "board/soak" of BOARD_SOAK_SHA (HEAD of this checkout by default) on a Pico,
"board/soak-u5" on the UNO Q: pending with the time run, the restarts and the errors at
each reading, failure as soon as either is not 0, success at the end if both are. On a
Pico it holds the lock of tools/board_ci.sh (BOARD_CI_LOCK) while it runs, so that the
board CI loads no other image.
"""
import atexit
import json
import os
import random
import select
import shutil
import subprocess
import sys
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


def symbol(elf, name):
    out = subprocess.run(["arm-none-eabi-nm", elf], capture_output=True, text=True,
                         check=True).stdout
    for line in out.splitlines():
        words = line.split()
        if len(words) == 3 and words[2] == name:
            return int(words[0], 16)
    sys.exit(f"{elf} has no {name}: not the endurance test")


class Pico:
    """SoakPico over SWD. Results, in words (SoakPico.c): 0 marker, 1 seconds, 2 wraps,
    3-10 activity, 11-18 errors, 19-20 lateness, 21-22 stack of each core, 23 the work of
    the long task in its phase,
    24-55 and 56-87 the lateness by bins of 10 us."""
    name, context = "SoakPico", "board/soak"
    parts = ["pulse", "queue", "buffer", "events", "cores", "heartbeat", "interrupt",
             "memory"]
    elf = os.path.join(ROOT, "Escapement/CORTEX-Mx/RP2040/Examples/pico/build/SoakPico.elf")
    watchdog_reason = 0x40058008       # WATCHDOG_REASON, RP2040 datasheet
    words = 88

    def __init__(self, elf):
        self.elf = elf
        self.results = symbol(elf, "Results")
        # The Debug Probe, $PROBE of the bench's table (tools/probe.sh).
        self.probe = subprocess.run(["sh", os.path.join(ROOT, "tools/probe.sh")],
                                    stdout=subprocess.PIPE, text=True,
                                    check=True).stdout.strip()
        lock = os.environ.get("BOARD_CI_LOCK",
                              os.path.expanduser("~/escapement-rp2040/board-ci/lock"))
        os.makedirs(os.path.dirname(lock), exist_ok=True)
        try:
            os.mkdir(lock)
        except FileExistsError:
            sys.exit(f"the board is in use ({lock})")
        with open(os.path.join(lock, "pid"), "w") as f:
            f.write(str(os.getpid()))
        atexit.register(shutil.rmtree, lock, True)

    def ocd(self, *commands):
        """OpenOCD with the Debug Probe and the RP2040, core 0 only; its output."""
        args = ["openocd", "-f", "interface/cmsis-dap.cfg", "-c", self.probe,
                "-c", "adapter speed 5000", "-c", "set USE_CORE 0",
                "-f", "target/rp2040.cfg", "-c", "init"]
        for command in commands:
            args += ["-c", command]
        out = subprocess.run(args + ["-c", "exit"], capture_output=True, text=True,
                             check=False)
        return out.stdout + out.stderr

    def words_at(self, address, n):
        values = []
        for line in self.ocd(f"mdw {address:#x} {n}").splitlines():
            if line.startswith("0x") and ": " in line:
                values += [int(w, 16) for w in line.split(": ", 1)[1].split()]
        return values if len(values) == n else None

    def load(self):
        self.ocd("reset halt", f"load_image {self.elf}", "resume 0x20000000")

    def start(self):
        self.load()

    def read(self):
        w = self.words_at(self.results, self.words)
        reason = self.words_at(self.watchdog_reason, 1)
        if w is None:
            return None
        return {"marker": w[0], "seconds": w[1], "wraps": w[2], "activity": w[3:11],
                "errors": w[11:19], "late": (w[19], w[20]), "stack": (w[21], w[22]),
                "load": w[23], "bins": (w[24:56], w[56:88]),
                "extra": f", reset {reason[0]:#010x}" if reason else "", "link": None}


class UnoQ:
    """SoakU5 on LPUART1: a report a second, SOAK and 27 hexadecimal numbers (SoakU5.c):
    seconds, wraps, the activity and the errors of the eight parts, the bytes of the link,
    its errors and overruns, the lateness of the pulse and of the timer events, the stack
    never used, the work of the long task in its phase, the byte the link expects next,
    and the flags of reset the run found in RCC_CSR, with in bits 0 to 23 the times the
    MSIS was locked again on the LSE (erratum 2.2.27 of the chip)."""
    name, context = "SoakU5", "board/soak-u5"
    parts = ["pulse", "queue", "buffer", "events", "buffer4", "heartbeat", "interrupt",
             "memory"]
    elf = os.path.expanduser("~/soak/SoakU5.elf")
    tty = "/dev/ttyHS1"
    silent = 10         # seconds without a report that say the board restarted
    bridge = ["arduino-router-serial.path", "arduino-router-serial", "arduino-router"]

    def __init__(self, elf):
        self.elf = elf
        symbol(elf, "Results")
        subprocess.run(["sudo", "-n", "systemctl", "stop"] + self.bridge,
                       capture_output=True, check=False)
        # Not blocking: the thread that writes shares the descriptor.
        self.fd = os.open(self.tty, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
        attrs = termios.tcgetattr(self.fd)
        attrs[0] = attrs[1] = attrs[3] = 0                     # raw
        attrs[2] = termios.CS8 | termios.CREAD | termios.CLOCAL   # no flow control
        attrs[4] = attrs[5] = termios.B115200
        termios.tcsetattr(self.fd, termios.TCSANOW, attrs)
        termios.tcflush(self.fd, termios.TCIOFLUSH)
        self.pending, self.last, self.sent, self.value = b"", None, 0, None
        self.lock = threading.Lock()   # self.value, between the link and a restart

    def load(self):
        """Through tools/unoq_load.sh, beside this script, run on the board."""
        loader = os.path.join(os.path.dirname(os.path.abspath(__file__)), "unoq_load.sh")
        subprocess.run(["sh", loader, self.elf], capture_output=True, check=False)
        self.last = None

    def start(self):
        """The image already running is read as it is, not loaded again: a test can be
        taken over. Loaded if no report comes."""
        if self.report(self.silent) is None:
            self.load()
        threading.Thread(target=self.link, daemon=True).start()

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
        """The newest report within wait seconds, or None."""
        deadline = time.monotonic() + wait
        newest = None
        while True:
            left = deadline - time.monotonic()
            if newest is not None and b"\n" not in self.pending and \
                    not select.select([self.fd], [], [], 0)[0]:
                return newest
            if left <= 0:
                return newest
            if select.select([self.fd], [], [], left)[0]:
                try:
                    self.pending += os.read(self.fd, 4096)
                except BlockingIOError:
                    pass
            while b"\n" in self.pending:
                line, self.pending = self.pending.split(b"\n", 1)
                words = line.decode(errors="replace").split()
                # SOAK and 27 numbers; 26 before the causes of reset (cf6d83e and older)
                if len(words) in (27, 28) and words[0] == "SOAK":
                    try:
                        newest = [int(w, 16) for w in words[1:]]
                    except ValueError:
                        pass
            self.pending = self.pending[-4096:]   # what is no report never piles up
            if newest is not None and self.value is None:
                with self.lock:
                    self.value = newest[25]

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
            return {"marker": 0, "seconds": 0}
        resets = (f", reset {self.resets(r[26])}, MSI locked again {r[26] & 0xFFFFFF}"
                  if len(r) > 26 else "")
        return {"marker": MARKER, "seconds": r[0], "wraps": r[1], "activity": r[2:10],
                "errors": r[10:18], "late": (r[21], r[22]), "stack": (r[23],),
                "load": r[24], "bins": None,
                "extra": f", link {r[18]} bytes of {self.sent} sent, {r[19]} errors, "
                         f"{r[20]} overruns{resets}", "link": r[18:21]}

    @staticmethod
    def resets(flags):
        """The flags of reset of RCC_CSR the run found (RM0456), by name. A load resets
        the pin; a first start after power adds BOR; the independent watchdog, IWDG."""
        names = [(25, "OBL"), (26, "pin"), (27, "BOR"), (28, "software"), (29, "IWDG"),
                 (30, "WWDG"), (31, "low-power")]
        return "+".join(n for b, n in names if flags >> b & 1) or "none"


class Status:
    """The commit status on GitHub, if a token is there."""

    def __init__(self, context):
        self.context = context
        self.repo = os.environ.get("BOARD_CI_REPO", "beber007/escapement")
        token = os.environ.get("BOARD_CI_TOKEN",
                               os.path.expanduser("~/.config/escapement-board-ci/token"))
        self.token = open(token).read().strip() if os.access(token, os.R_OK) else None
        self.sha = os.environ.get("BOARD_SOAK_SHA") or subprocess.run(
            ["git", "-C", ROOT, "rev-parse", "HEAD"], capture_output=True,
            text=True).stdout.strip()

    def post(self, state, description):
        if not self.token or not self.sha:
            return
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


def main():
    if len(sys.argv) < 4 or sys.argv[1] not in ("pico", "uno-q"):
        sys.exit(__doc__.split("\n\n")[1])
    kind = Pico if sys.argv[1] == "pico" else UnoQ
    duration, interval = seconds(sys.argv[2]), seconds(sys.argv[3])
    elf = sys.argv[4] if len(sys.argv) > 4 else kind.elf
    if not os.path.isfile(elf):
        sys.exit(f"no image {elf}")
    board = kind(elf)
    log = os.environ.get("BOARD_SOAK_LOG",
                         f"soak-{sys.argv[1]}-{time.strftime('%Y%m%d-%H%M%S')}.log")
    status = Status(board.context)

    def write(line, show=False):
        with open(log, "a") as out:
            out.write(line + "\n")
        if show:
            print(line, flush=True)

    board.start()
    start = time.time()
    span = f"for {elapsed(duration)}" if duration else "until stopped"
    write(f"{board.name} {status.sha or '(no commit)'}, {time.ctime()}, {span}, read every {interval} s, "
          f"{elf}", True)
    status.post("pending", f"running since {time.strftime('%Y-%m-%d')}")
    restarts = errors = wraps = 0
    previous = None               # seconds, errors found, activity, time
    last_hour = start
    while True:
        time.sleep(interval)
        now = time.time()
        stamp = time.strftime("%Y-%m-%d %H:%M:%S")
        r = board.read()
        if r is None:
            write(f"{stamp} no reading from the board", True)
            errors += 1
        elif r["marker"] != MARKER or (previous and r["seconds"] < previous[0]):
            restarts += 1
            write(f"{stamp} RESTART {restarts}{r.get('extra', '')}", True)
            board.load()
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
            previous = (r["seconds"], found,
                        r["activity"] + ([r["link"][0]] if r["link"] else []), now,
                        r["wraps"])
        run = elapsed(int(now - start)) + (f" of {elapsed(duration)}" if duration else "")
        summary = f"{restarts} restarts, {errors} errors, {wraps} wraps"
        status.post("failure" if restarts + errors else "pending", f"{run}: {summary}")
        if duration and now - start >= duration:
            break
    write(f"end after {elapsed(int(now - start))}: {summary}", True)
    if restarts + errors == 0:
        status.post("success", f"{elapsed(int(now - start))} without error: {wraps} wraps")
        sys.exit(0)
    sys.exit(1)


if __name__ == "__main__":
    main()
