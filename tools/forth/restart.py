#!/usr/bin/env python3
"""Load RestartPico on a Pico, wait for its restarts of core 0, and print what core 1
counted, without stopping a core.

    PROBE=probe1 tools/forth/restart.py path/to/RestartPico.elf [SECONDS]

Core 1 runs the watcher: the image is loaded with OpenOCD handling core 0 alone
(set USE_CORE 0), as tools/fourslot_cores.sh does. The chip is first reset whole through
the rescue debug port, as tools/pico_flash.sh does, which needs Raspberry Pi's OpenOCD
(OPENOCD=~/opt/openocd-rpi/bin/openocd): an image before may have left both cores off.
"""

import os
import re
import subprocess
import sys
import time

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
import read_trace   # noqa: E402

NAMES = ["asked", "soft ok", "hard made", "hard ok", "soft lost max", "soft lost sum",
         "hard lost max", "hard lost sum", "gave up", "done", "beats", "windows short",
         "starts"]


def openocd(commands):
    args = [os.environ.get("OPENOCD", "openocd"), "-f", "interface/cmsis-dap.cfg",
            "-c", read_trace.adapter(), "-c", "adapter speed 5000", "-c", "set USE_CORE 0",
            "-f", "target/rp2040.cfg", "-c", "init"]
    for c in commands:
        args += ["-c", c]
    args += ["-c", "exit"]
    run = subprocess.run(args, capture_output=True, text=True, timeout=60)
    return run.stdout + run.stderr


def counts(sym):
    out = openocd([f"echo \"W [read_memory {sym['Restart']:#x} 32 {len(NAMES)}]\""])
    found = re.search(r"W ([0-9a-fx ]+)", out)
    if not found:
        sys.exit("could not read the board:\n" + out)
    return dict(zip(NAMES, (int(w, 0) for w in found.group(1).split())))


def main():
    elf = sys.argv[1]
    seconds = float(sys.argv[2]) if len(sys.argv) > 2 else 60
    sym = read_trace.symbols(elf)
    try:   # Debian's OpenOCD ignores RESCUE and stays up as a server
        subprocess.run([os.environ.get("OPENOCD", "openocd"), "-f", "interface/cmsis-dap.cfg",
                        "-c", read_trace.adapter(), "-c", "set RESCUE 1",
                        "-f", "target/rp2040.cfg"], capture_output=True, timeout=20)
    except subprocess.TimeoutExpired:
        pass
    time.sleep(1)
    out = openocd(["reset halt", f"load_image {elf}", "resume 0x20000000"])
    if "downloaded" not in out and "bytes written" not in out:
        sys.exit("could not load the image:\n" + out)
    start = time.time()
    while time.time() - start < seconds:
        time.sleep(5)
        c = counts(sym)
        if c["done"] or c["gave up"]:
            break
    c = counts(sym)
    print(c)
    for kind in ("soft", "hard"):
        ok = c[f"{kind} ok"]
        if ok:
            print(f"{kind}: {ok} restarts, real time lost {c[f'{kind} lost sum'] / ok:.0f} us "
                  f"in the mean, {c[f'{kind} lost max']} at most")


main()
