#!/usr/bin/env python3
"""Load BenchSlotsPico images in turn on a Pico, let each run, read its counts without
stopping a core, and print one line of JSON each.

    PROBE=probe1 tools/forth/bench_slots.py SECONDS A.elf B.elf ...
"""

import json
import os
import re
import sys
import time

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
import read_trace   # noqa: E402


def words(address, count):
    out = read_trace.openocd([f"echo \"W [read_memory {address:#x} 32 {count}]\""])
    found = re.search(r"W ([0-9a-fx ]+)", out)
    if not found:
        sys.exit("could not read the board:\n" + out)
    return [int(w, 0) for w in found.group(1).split()]


def main():
    seconds = float(sys.argv[1])
    for elf in sys.argv[2:]:
        sym = read_trace.symbols(elf)
        out = read_trace.openocd(["reset halt", f"load_image {elf}", "resume 0x20000000"])
        if "downloaded" not in out and "bytes written" not in out:
            sys.exit("could not load the image:\n" + out)
        time.sleep(seconds)
        slots = words(sym["Slots"], 6)
        forth = words(sym["Forth"], 6)
        print(json.dumps({"elf": os.path.basename(elf), "slots": slots, "forth": forth}),
              flush=True)


main()
