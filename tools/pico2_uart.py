#!/usr/bin/env python3
# Copyright (c) 2026 Bertrand Hurst. Part of Escapement, distributed under the terms of
# LICENSE at the root of this repository.
#
"""Run the two UART examples of the Pico 2 on the board, through the UART of its Debug
Probe, and hold them to the criteria of emulation/renode/escapement_pico2.robot.

    tools/pico2_uart.py [BUILD_DIR] [--tty TTY] [--only echo|senders ...]

BUILD_DIR holds the images of Examples/pico2 (its build/ by default). The probe's UART
must be wired to GP0 and GP1 (115200 8N1); TTY is found from the probe's serial in the
bench's table (tools/probe.sh) unless given. Each image is loaded as tools/pico2_check.py
loads it, whose functions this uses:
- UARTEchoPico2 must send back the suite's line, every byte value, and 64 lines sent one
  on the heels of the other, each byte once and in order;
- UARTSendersPico2 runs 20 s, the port read throughout: every line on it must be whole,
  and the lines its two tasks had queued when their counts were read must lie between
  those on the port just before and just after, but the 4 buffers still queued.

The Debug Probe keeps what the board sent while the port was closed, and hands it over
once it is opened, which flushing the tty does not clear: on 2026-09-30, 3,968 bytes of a
UARTSendersPico2 left running before. The port is therefore drained once opened.
"""

import argparse
import glob
import os
import subprocess
import sys
import tempfile
import termios
import threading
import time

import pico2_check as p


class Port:
    """The probe's UART, read by a thread: at 11.5 kB/s the kernel's buffer would fill
    while the tool waits on OpenOCD."""

    def __init__(self, tty):
        self.fd = os.open(tty, os.O_RDWR | os.O_NOCTTY)
        attrs = termios.tcgetattr(self.fd)
        attrs[0] = attrs[1] = attrs[3] = 0
        attrs[2] = termios.CS8 | termios.CREAD | termios.CLOCAL
        attrs[4] = attrs[5] = termios.B115200
        attrs[6][termios.VMIN], attrs[6][termios.VTIME] = 0, 1
        termios.tcsetattr(self.fd, termios.TCSANOW, attrs)
        termios.tcflush(self.fd, termios.TCIOFLUSH)
        self.data, self.lock, self.stop = bytearray(), threading.Lock(), False
        self.thread = threading.Thread(target=self.reader, daemon=True)
        self.thread.start()
        time.sleep(0.5)
        self.clear()

    def reader(self):
        while not self.stop:
            chunk = os.read(self.fd, 4096)
            if chunk:
                with self.lock:
                    self.data += chunk

    def take(self):
        with self.lock:
            return bytes(self.data)

    def clear(self):
        with self.lock:
            self.data.clear()

    def close(self):
        self.stop = True
        self.thread.join()
        os.close(self.fd)


def probe_tty():
    """The UART of the probe tools/probe.sh picks, by its serial under /dev/serial/by-id."""
    line = subprocess.run(["sh", os.path.join(p.TOOLS, "probe.sh")], stdout=subprocess.PIPE,
                          text=True, check=True).stdout
    serial = line.split("adapter serial ", 1)[1].split(";")[0] if "adapter serial" in line \
        else ""
    ttys = glob.glob(f"/dev/serial/by-id/usb-Raspberry_Pi_Debug_Probe*{serial}*-if01")
    if len(ttys) != 1:
        sys.exit(f"no single UART for the probe {serial or '(any)'}: {ttys}; give --tty")
    return ttys[0]


def load(elf, seed):
    out = p.openocd(["reset halt", f"mww {p.PSM_FRCE_OFF_SET:#x} {p.PSM_PROC1:#x}",
                     f"load_image {seed} 0x20000000 bin", "resume 0x20000000",
                     "wait_halt 500", f"load_image {elf}", "resume 0x20000000"])
    if out.count("bytes written") < 2:
        sys.exit(f"could not load {elf}:\n{out}")


def results(elf, words):
    out = p.openocd([f"mdw {p.symbol(elf, 'Results'):#x} {words}"])
    values = []
    for line in out.splitlines():
        if line.startswith("0x") and ":" in line:
            values += [int(w, 16) for w in line.split(":", 1)[1].split()]
    return values[:words]


def echo(build, port, seed):
    load(os.path.join(build, "UARTEchoPico2.elf"), seed)
    time.sleep(1)
    checks = []
    for message in [b"escapement\n", bytes(range(256)),
                    b"".join(b"line %04d of the echo\n" % i for i in range(64))]:
        port.clear()
        os.write(port.fd, message)
        deadline = time.time() + 2 + len(message) / 5000
        while time.time() < deadline and len(port.take()) < len(message):
            time.sleep(0.05)
        back = port.take()
        checks.append((back == message, f"{len(message)} bytes sent, {len(back)} back"
                       + ("" if back == message else f", differing: {back[:40]!r}")))
    return checks


def senders(build, port, seed, seconds=20):
    elf = os.path.join(build, "UARTSendersPico2.elf")
    port.clear()
    load(elf, seed)
    time.sleep(seconds)
    before = port.take().count(b"\n")
    r = results(elf, 4)
    after = port.take().count(b"\n")
    time.sleep(0.5)
    lines = port.take().split(b"\n")[:-1]
    if len(r) < 4:
        return [(False, "Results not read back")]
    bad = [line for line in lines if line not in (b"a" * 15, b"b" * 7)]
    queued = r[1] + r[2]
    return [(r[1] > 100 and r[2] > 100,
             f"{r[1]} lines of a and {r[2]} of b queued in {seconds} s, "
             f"{r[3]} instances without a buffer"),
            (not bad, f"{lines.count(b'a' * 15)} lines of a and {lines.count(b'b' * 7)} of b "
                      f"whole on the port, {len(bad)} not: {bad[:3]}"),
            (before <= queued <= after + 4,
             f"{before} lines on the port before the counts were read, {queued} queued, "
             f"{after} on the port once read")]


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("build", nargs="?", default=p.EXAMPLES)
    parser.add_argument("--tty", default=None)
    parser.add_argument("--only", nargs="*", default=None)
    args = parser.parse_args()

    failed = 0
    p.openocd(["rescue_reset"])
    port = Port(args.tty or probe_tty())
    with tempfile.TemporaryDirectory() as directory:
        seed = p.build_seed(directory)
        for name, test in [("UARTEchoPico2", echo), ("UARTSendersPico2", senders)]:
            if args.only and test.__name__ not in args.only:
                continue
            checks = test(args.build, port, seed)
            ok = all(passed for passed, _ in checks)
            failed += not ok
            print(f"{'ok  ' if ok else 'FAIL'} {name}")
            for passed, text in checks:
                print(f"     {'' if passed else '!! '}{text}")
    port.close()
    sys.exit(1 if failed else 0)


if __name__ == "__main__":
    main()
