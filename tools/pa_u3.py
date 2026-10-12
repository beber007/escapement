#!/usr/bin/env python3
# Copyright (c) 2026 Bertrand Hurst. Part of Escapement, distributed under the terms of
# LICENSE at the root of this repository.
"""What the STM32U3's power-aware kernel saves (docs/roadmap.md, release 0.1, task 14b):
SleepU3 built with PHASES=30 under the hard kernel, racing to its sleep, and under the
power-aware kernel with each policy, each image measured in turn by the PPK2 in place of
JP4 on the NUCLEO-U385RG-Q, on the UNO Q.

    tools/pa_u3.py SECONDS NAME=ELF...

Each ELF, linked into the flash (make FLASH=1 PHASES=30 [KERNEL=PA POWER=...]), is written
through the PPK2, its switch closed (tools/nucleo_flash.sh); the switch then opens for 6 s,
which the debug domain the debugger left powered does not outlive (docs/stm32u3.md,
"Measuring DVFS"), and the image starts again from the flash. After START_S, SECONDS of
samples: the mean of the Stop 2 phases, D8 high on the PPK2's D0, and of the Sleep phases,
only those the window did not cut (tools/ppk2_nucleo.py). Meanwhile the lines of USART1
are kept in NAME.lines beside the ELF, and the last read: an image whose wake-ups came past
the next event (word 7), whose LSE failed (word 8) or whose MSI did not lock again
(word 14) fails, its figures printed all the same.

$NUCLEO_SERIAL, $NUCLEO_TTY and $OPENOCD as in tools/soak.py; NUCLEO_MCU is u385 here.
"""
import os
import subprocess
import sys
import termios
import threading
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import ppk2_nucleo   # noqa: E402  (beside this file, with its PYTHONPATH)

HERE = os.path.dirname(os.path.abspath(__file__))
OFF_S = 6        # the switch open, the MCU and its debug domain unpowered
START_S = 5      # the clocks and the first line of USART1 before the samples
WORDS = 16       # SLEEP and words 1 to 16 (SleepU3.c)
FAILS = {7: "wake-ups past the next event", 8: "no LSE", 14: "MSI not locked again"}


def reader(tty, path, stop, last, keep):
    """The lines of USART1 at 115,200 baud into path, the last SLEEP line in last[0], those
    come before keep is set dropped: the ST-LINK holds what its port received while no one
    read it, and gives it all at once when the port opens."""
    fd = os.open(tty, os.O_RDONLY | os.O_NOCTTY | os.O_NONBLOCK)
    attrs = termios.tcgetattr(fd)
    attrs[0] = attrs[1] = attrs[3] = 0
    attrs[2] = termios.CS8 | termios.CREAD | termios.CLOCAL
    attrs[4] = attrs[5] = termios.B115200
    termios.tcsetattr(fd, termios.TCSANOW, attrs)
    termios.tcflush(fd, termios.TCIFLUSH)
    line = b""
    with open(path, "w") as out:
        while not stop.is_set():
            try:
                data = os.read(fd, 4096)
            except BlockingIOError:
                time.sleep(0.05)
                continue
            for c in data:
                if c in b"\r\n":
                    text = line.decode("ascii", "replace").strip()
                    if text and keep.is_set():
                        out.write(f"{time.strftime('%H:%M:%S')} {text}\n")
                        words = text.split()
                        if len(words) == WORDS + 1 and words[0] == "SLEEP":
                            last[0] = [int(w, 16) for w in words[1:]]
                    line = b""
                else:
                    line += bytes([c])
    os.close(fd)


def measure(ppk2, name, elf, seconds, tty):
    env = dict(os.environ, NUCLEO_MCU="u385")
    # SleepU3 reaches Stop 2 at once from the reset, where the debug domain drops: OpenOCD's
    # polling after "reset run" then fails, the image written and read back all the same.
    flash = subprocess.run([os.path.join(HERE, "nucleo_flash.sh"), elf], env=env,
                           capture_output=True, text=True)
    print(flash.stdout, end="", flush=True)
    if flash.returncode and not any(mark in flash.stdout
                                    for mark in ("ESCAPEMENT-SAME", "Verified OK")):
        sys.exit(f"{name}: {elf} not in the flash")
    stop, keep, last = threading.Event(), threading.Event(), [None]
    lines = os.path.splitext(elf)[0] + ".lines"
    thread = threading.Thread(target=reader, args=(tty, lines, stop, last, keep))
    thread.start()
    ppk2.toggle_DUT_power("OFF")
    time.sleep(OFF_S)
    keep.set()
    ppk2.toggle_DUT_power("ON")
    time.sleep(START_S)
    try:
        found, rate = ppk2_nucleo.record(ppk2, seconds)
    finally:
        stop.set()
        thread.join()
    means = {}
    for level, phase in ((1, "Stop 2"), (0, "Sleep")):
        kept = ppk2_nucleo.whole(found, level)
        n = sum(p[1] for p in kept)
        each = [p[2] / p[1] for p in kept] or [float("nan")]
        means[phase] = (len(kept), sum(p[2] for p in kept) / n if n else float("nan"),
                        min(each), max(each))
    words = last[0]
    faults = ["no line of USART1"] if words is None else \
        [f"{what}: {words[w - 1]}" for w, what in FAILS.items() if words[w - 1]]
    print(f"{name}: {rate / 1000:.0f} kS/s, "
          + ", ".join(f"{phase} {k} phases {m:.2f} uA ({low:.2f} to {high:.2f})"
                      for phase, (k, m, low, high) in means.items())
          + (f", instances {words[0]}, gap {words[3]} us, event {words[9]} us"
             if words else "")
          + ("" if not faults else ", FAILED: " + "; ".join(faults)), flush=True)
    return not faults


def main():
    if len(sys.argv) < 3 or any("=" not in a for a in sys.argv[2:]):
        sys.exit(__doc__.split("\n\n")[1])
    seconds = float(sys.argv[1])
    tty = os.environ["NUCLEO_TTY"]
    ppk2 = ppk2_nucleo.open_ppk2()
    failed = []
    for arg in sys.argv[2:]:
        name, elf = arg.split("=", 1)
        if not measure(ppk2, name, elf, seconds, tty):
            failed.append(name)
    if failed:
        sys.exit("failed: " + ", ".join(failed))


if __name__ == "__main__":
    main()
