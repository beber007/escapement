#!/usr/bin/env python3
# Copyright (c) 2026 Bertrand Hurst. Part of Escapement, distributed under the terms of
# LICENSE at the root of this repository.
"""The Bus Pirate v4 of the bench as a plain serial port: its UART at 115,200 baud, 8N1,
idle high, its output driven to 3.3 V, then its transparent bridge, which hands every
byte of its USB port to its UART and back. With it, a host sends to a UART of a board
that has no port of its own on the UNO Q: LPUART1 of the NUCLEO-U385RG-Q on D0 and D1
(docs/stm32u3.md), the Bus Pirate's MOSI to D0, its MISO to D1, the grounds joined.

The menus are those of the community firmware v7.0, read on 2026-10-09. The bridge holds
until the Bus Pirate is reset by its power, its USB unplugged and plugged back: '#' is a
byte like any other once bridging. A Bus Pirate that answers no prompt is taken for one
already bridging, and left so.

    tools/buspirate_bridge.py           # prints the port, under /dev/serial/by-id
"""
import glob
import os
import sys
import termios
import time

# The menus in order: the mode, UART; 115,200 baud; 8 bits, no parity; 1 stop bit; idle 1;
# normal output, H at 3.3 V; then the macro of the transparent bridge, confirmed.
STEPS = [(b"m\n", b"(1)>"), (b"3\n", b"(1)>"), (b"9\n", b"(1)>"), (b"1\n", b"(1)>"),
         (b"1\n", b"(1)>"), (b"1\n", b"(1)>"), (b"2\n", b"UART>"), (b"(1)\n", b"y/n"),
         (b"y\n", None)]


def talk(fd, data, until, wait=2.0):
    """Writes data, then reads until the text until comes, or wait seconds."""
    os.write(fd, data)
    out, end = b"", time.monotonic() + wait
    while time.monotonic() < end and (until is None or until not in out):
        try:
            out += os.read(fd, 4096)
        except BlockingIOError:
            time.sleep(0.05)
    return out


ports = sorted(glob.glob("/dev/serial/by-id/usb-Dangerous_Prototypes_Bus_Pirate*"))
if not ports:
    sys.exit("no Bus Pirate under /dev/serial/by-id")
fd = os.open(ports[0], os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
attrs = termios.tcgetattr(fd)
attrs[0] = attrs[1] = attrs[3] = 0                        # raw
attrs[2] = termios.CS8 | termios.CREAD | termios.CLOCAL   # no flow control
attrs[4] = attrs[5] = termios.B115200
termios.tcsetattr(fd, termios.TCSANOW, attrs)
termios.tcflush(fd, termios.TCIOFLUSH)
if b">" not in talk(fd, b"\n", b">", 1.0):
    print(ports[0])
    print("no prompt: taken for a bridge already", file=sys.stderr)
    sys.exit(0)
for data, until in STEPS:
    out = talk(fd, data, until)
    if until is not None and until not in out:
        sys.exit(f"the Bus Pirate did not answer {data!r} with {until!r}: {out!r}")
print(ports[0])
