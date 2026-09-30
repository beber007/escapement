#!/usr/bin/env python3
# Copyright (c) 2026 Bertrand Hurst. Part of Escapement, distributed under the terms of
# LICENSE at the root of this repository.
"""The endurance test on a board: load its firmware, let it run, read its counts at every
interval without stopping it, for as long as asked.

    tools/soak.py pico DURATION INTERVAL [ELF]      SoakPico, through the Debug Probe $PROBE
    tools/soak.py uno-q DURATION INTERVAL [ELF]     SoakU5, on the board's own Linux
    tools/soak.py nucleo DURATION INTERVAL [ELF]    SoakU5 on a NUCLEO-U575ZI-Q, through
                                                    its ST-LINK, beside the UNO Q

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
checks of each commit on the UNO Q do not interrupt.

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
    baud = termios.B57600    # LPUART1 (Escapement_UART.c)
    silent = 10         # seconds without a report that say the board restarted
    bridge = ["arduino-router-serial.path", "arduino-router-serial", "arduino-router"]
    loader = "unoq_load.sh"

    def __init__(self, elf):
        self.elf = elf
        symbol(elf, "Results")
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
        self.pending, self.last, self.sent, self.value = b"", None, 0, None
        self.lock = threading.Lock()   # self.value, between the link and a restart

    def load(self):
        """Through the loader beside this script, tools/unoq_load.sh run on the board, or
        tools/nucleo_load.sh. Returns OpenOCD's error if the load failed, None otherwise:
        one that failed unseen, the image before left running, passed for restarts of the
        board (2026-09-27)."""
        loader = os.path.join(os.path.dirname(os.path.abspath(__file__)), self.loader)
        done = subprocess.run(["sh", loader, self.elf], capture_output=True, text=True,
                              check=False)
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


class Nucleo(UnoQ):
    """SoakU5 on a NUCLEO-U575ZI-Q: the same reports and link, on USART1 to the virtual
    COM port of its ST-LINK, and no Bridge to stop."""
    name, context = "SoakU5 (NUCLEO-U575ZI-Q)", "board/soak-nucleo"
    elf = os.path.expanduser("~/soak-nucleo/SoakU5.elf")
    tty = os.environ.get("NUCLEO_TTY") or next(
        iter(sorted(glob.glob("/dev/serial/by-id/usb-STMicroelectronics_STLINK*"))),
        "/dev/ttyACM0")
    baud = termios.B115200   # USART1 (Escapement_UART.c)
    bridge = []
    loader = "nucleo_load.sh"


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


def main():
    boards = {"pico": Pico, "uno-q": UnoQ, "nucleo": Nucleo}
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
