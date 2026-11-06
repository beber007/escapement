#!/usr/bin/env python3
# Copyright (c) 2026 Bertrand Hurst. Part of Escapement, distributed under the terms of
# LICENSE at the root of this repository.
#
"""Run the examples of the Pico 2 that count in memory on the board, and hold each to the
criteria of emulation/renode/escapement_pico2.robot.

    tools/pico2_check.py [BUILD_DIR] [--only NAME ...]
    tools/pico2_check.py --flash [BUILD_DIR] [--only NAME ...]

BUILD_DIR holds the images of Examples/pico2 (its build/ by default). Each one is loaded
into SRAM, left to run, and its Results read over SWD without stopping a core. With
--flash, BUILD_DIR holds those of make FLASH=1 (build-flash/): each is written into the
flash and booted by the bootrom (tools/pico2_flash.sh), and held to the same criteria. Six of
the suite's tests count in memory: the 4-slot and the 3-slot buffers, the queue of
Evéquoz across the two cores, IPCPico2, SoakPico2 and the litmus tests of LitmusPico2,
which the board holds to more than Renode can: the outcome that needs the two cores
within a cycle of each other must show. The criteria that count what only
the emulator sees (SCs failed, reservations cleared, helpers entered on another stack)
are left out; the tests of the outputs need a witness on the pins, and those of the UART
are tools/pico2_uart.py's.

An image built for the Hazard3 cores (make ARCH=riscv, issue #77), known by its ELF
header, is loaded from the Arm side and booted by the bootrom: a RAM image boot, asked
for through the watchdog's scratch registers (RP2350 datasheet, the bootrom's watchdog
boot), finds its
IMAGE_DEF in SRAM and switches both cores to RISC-V; core 1 waits in the bootrom for
OSLaunchCore1, as on the Cortex-M33. Its Results are read through core 0's RISC-V debug,
without stopping it.

Needs the GNU Arm binutils and an OpenOCD that knows the RP2350 (Raspberry Pi's fork, in
$OPENOCD), and a CMSIS-DAP probe: PROBE=name picks one of the bench's (tools/probe.sh).

Three things of the board that the emulator does not have, found on 2026-09-28:
- After the board is powered, the firmware in its flash runs, and the one on the bench
  left the debugger unable to examine core 0 (its access port answered WAIT). The tool
  first has the RP-AP restart the chip into its bootrom, as it does for rescue
  (rescue_reset in Raspberry Pi's target/rp2350.cfg), which the flash cannot prevent.
- A debugger's reset stops core 0 at the entry of the bootrom, before it seeds the RCP
  of both cores, and core 1 never answers the launch (tools/rp2350_rcp_seed.S, which is
  run first for that).
- That reset (SYSRESETREQ) leaves core 1 running the previous image. The tool forces it
  off through the PSM before loading. Left running, it wrote into the new image's heap
  until OSLaunchCore1 reset it: in 4 runs of ThreeSlotCoresPico2 out of 38 loaded so,
  22 to 38 % of the reads tore, and the one dumped had a slot's data pointer moved by 3;
  none in 30 with core 1 forced off.
"""

import argparse
import os
import subprocess
import sys
import tempfile
import time

TOOLS = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(TOOLS)
EXAMPLES = os.path.join(ROOT, "Escapement/CORTEX-Mx/RP2350/Examples/pico2/build")
PSM_FRCE_OFF_SET = 0x4001A004   # PSM 0x40018000, FRCE_OFF 0x04, set alias +0x2000
PSM_FRCE_OFF_CLR = 0x4001B004   # its clear alias, +0x3000
PSM_PROC1 = 1 << 24
WATCHDOG_SCRATCH2 = 0x400D8014
# A RAM image boot of the SRAM's first 512 KB (pico-sdk, boot/bootrom_constants.h,
# BOOT_TYPE_RAM_IMAGE): scratch 2 and 3 the region, 4 the magic, 5 its complement, 6 the
# type, 7 the magic again. Checked on the Pico 2 on
# 2026-11-05: the bootrom found the IMAGE_DEF and ran the image on both Hazard3.
RAM_IMAGE_BOOT = [0x20000000, 0x80000, 0xB007C0D3, 0xFFFFFFFE, 3, 0xB007C0D3]
DEMCR = 0xE000EDFC   # the debugger's reset leaves VC_CORERESET set: it holds the core


def is_riscv(elf):
    """An ELF built for RISC-V, by e_machine (243)."""
    with open(elf, "rb") as f:
        header = f.read(20)
    return int.from_bytes(header[18:20], "little") == 243


def core_of(elf):
    """The core OpenOCD debugs for elf's Results."""
    return "rv0" if is_riscv(elf) else "0"


def adapter():
    """The OpenOCD command that picks the Debug Probe, $PROBE of the bench's table."""
    return subprocess.run(["sh", os.path.join(TOOLS, "probe.sh")], stdout=subprocess.PIPE,
                          text=True, check=True).stdout.strip()


def openocd(commands, core="0"):
    """OpenOCD on core 0 only: left to itself it would halt core 1 and keep it so. core
    is "0" for the Cortex-M33, "rv0" for the Hazard3."""
    args = [os.environ.get("OPENOCD", "openocd"), "-f", "interface/cmsis-dap.cfg",
            "-c", adapter(), "-c", "adapter speed 5000", "-c", f"set USE_CORE {core}",
            "-f", "target/rp2350.cfg", "-c", "init"]
    for c in commands:
        args += ["-c", c]
    args += ["-c", "exit"]
    run = subprocess.run(args, capture_output=True, text=True, timeout=60)
    return run.stdout + run.stderr


def symbol(elf, name):
    out = subprocess.run(["arm-none-eabi-nm", elf], capture_output=True, text=True,
                         check=True).stdout
    for line in out.splitlines():
        parts = line.split()
        if len(parts) == 3 and parts[2] == name:
            return int(parts[0], 16)
    sys.exit(f"no {name} in {elf}")


def build_seed(directory):
    """The stub, assembled: it only loads relative to the PC, so needs no linker, and
    binutils alone do (the bench has no arm-none-eabi-gcc)."""
    source = os.path.join(TOOLS, "rp2350_rcp_seed.S")
    obj, binary = os.path.join(directory, "seed.o"), os.path.join(directory, "seed.bin")
    subprocess.run(["arm-none-eabi-as", "-o", obj, source], check=True)
    subprocess.run(["arm-none-eabi-objcopy", "-O", "binary", obj, binary], check=True)
    return binary


def load(elf, seed):
    """Load elf after the seed; with no seed, write it into the flash and boot it."""
    if is_riscv(elf):
        # The image before, on the Hazard3, leaves the Cortex-M33 unexamined, and "reset
        # halt" fails on it: the rescue reset gives it back first (2026-11-05).
        openocd(["rescue_reset"])
        boot = [f"mww {WATCHDOG_SCRATCH2 + 4 * i:#x} {w:#x}"
                for i, w in enumerate(RAM_IMAGE_BOOT)]
        out = openocd(["reset halt", f"mww {PSM_FRCE_OFF_SET:#x} {PSM_PROC1:#x}",
                       f"load_image {elf}"] + boot +
                      [f"mww {PSM_FRCE_OFF_CLR:#x} {PSM_PROC1:#x}", f"mww {DEMCR:#x} 0",
                       "resume"])
        if "bytes written" not in out:
            sys.exit(f"could not load {elf}:\n{out}")
        return
    if seed is None:
        out = subprocess.run(["sh", os.path.join(TOOLS, "pico2_flash.sh"), elf],
                             capture_output=True, text=True, timeout=120).stdout
        if "Verified OK" not in out:
            sys.exit(f"could not write {elf} into the flash:\n{out}")
        return
    out = openocd(["reset halt", f"mww {PSM_FRCE_OFF_SET:#x} {PSM_PROC1:#x}",
                   f"load_image {seed} 0x20000000 bin", "resume 0x20000000",
                   "wait_halt 500", f"load_image {elf}", "resume 0x20000000"])
    if out.count("bytes written") < 2:
        sys.exit(f"could not load {elf}:\n{out}")


def run(elf, seed, seconds, words):
    """Load elf (load), let it run, and return words words of its Results."""
    load(elf, seed)
    time.sleep(seconds)
    out = openocd([f"mdw {symbol(elf, 'Results'):#x} {words}"], core_of(elf))
    values = []
    for line in out.splitlines():
        if line.startswith("0x") and ":" in line:
            values += [int(w, 16) for w in line.split(":", 1)[1].split()]
    if len(values) < words:
        sys.exit(f"Results of {elf} not read back:\n{out}")
    return values[:words]


def slot_buffer(r, name):
    """FourSlotCoresPico2, ThreeSlotCoresPico2: Written, then Reads, Torn, Backwards,
    Last of the buffer and of the plain array."""
    return [(r[0] > 1000, f"{r[0]} records written"),
            (r[1] > 5000, f"{r[1]} reads of the {name} buffer"),
            (r[2] == 0 and r[3] == 0, f"{r[2]} torn, {r[3]} backwards"),
            (r[6] > 0, f"plain array torn {r[6]} times (must tear)")]


def fifo_cores(r):
    """FIFOCoresPico2: Written per producer, then per consumer Taken, Sum, Squares per
    producer, Torn, OutOfOrder."""
    checks = []
    for p in range(2):
        taken = [r[2 + 8 * c + p] for c in range(2)]
        total = sum(r[2 + 8 * c + 2 + p] for c in range(2))
        squares = sum(r[2 + 8 * c + 4 + p] for c in range(2))
        checks.append((r[p] == 500 and sum(taken) == 500 and min(taken) > 0,
                       f"producer {p}: {r[p]} written, taken {taken[0]}+{taken[1]}"))
        checks.append((total == 125250 and squares == 41791750,
                       f"producer {p}: sum {total}, squares {squares}"))
    for c in range(2):
        torn, late = r[2 + 8 * c + 6], r[2 + 8 * c + 7]
        checks.append((torn == 0 and late == 0,
                       f"consumer {c}: {torn} torn, {late} out of order"))
    return checks


def ipc(r):
    """IPCPico2: Put and Taken of each producer, OutOfOrder, then the slot buffer's
    Reads, Repeats, Torn, and the registers found changed."""
    return [(r[2] > 200 and r[3] > 10, f"taken {r[2]}+{r[3]}"),
            (r[0] - r[2] <= 8 and r[1] - r[3] <= 8, f"put {r[0]}+{r[1]}"),
            (r[4] == 0, f"{r[4]} out of order"),
            (r[6] > 10 and r[7] == 0 and r[8] == 0,
             f"slot buffer: {r[6]} reads, {r[7]} repeated, {r[8]} torn"),
            (r[9] == 0, f"{r[9]} registers changed")]


def soak(r):
    """SoakPico2: Marker, Seconds, then the activity and the errors of its eight parts."""
    checks = [(r[0] == 0x534F414B, f"marker {r[0]:#x}"),
              (r[1] >= 2 and r[3] >= (r[1] - 1) * 1000,
               f"{r[1]} s, {r[3]} pulses")]
    for part in range(8):
        activity, errors = r[3 + part], r[11 + part]
        checks.append((activity > 0 and errors == 0,
                       f"part {part}: {activity} done, {errors} errors"))
    return checks


def litmus(r, overlap_needed=True):
    """LitmusPico2: Marker, then for SB, SB+DMB, MP, MP+DMB, LB, LB+DMB the rounds and the
    four outcomes, indexed r0 * 2 + r1. None may end in its weak outcome, and each must
    have met the other core within a cycle or two: the outcome that needs both halves to
    overlap (SB 11, MP 01, LB 00) must show, or the rounds proved nothing. From the flash
    that witness is not required, only printed: LB's never showed there, the two cores
    fetching through the one XIP cache, and the user kept it to the images in SRAM
    (docs/rp2040.md, "From the flash", 2026-10-09)."""
    checks = [(r[0] == 0x4C544D53, f"marker {r[0]:#x}")]
    for i, (name, weak, overlap) in enumerate([("SB", 0, 3), ("SB+DMB", 0, 3),
                                               ("MP", 2, 1), ("MP+DMB", 2, 1),
                                               ("LB", 3, 0), ("LB+DMB", 3, 0)]):
        rounds, outcome = r[1 + 5 * i], r[2 + 5 * i:6 + 5 * i]
        checks.append((rounds > 100000 and outcome[weak] == 0
                       and (outcome[overlap] > 0 or not overlap_needed),
                       f"{name}: {rounds} rounds, weak {outcome[weak]}, "
                       f"overlapping {outcome[overlap]}"))
    return checks


TESTS = [("FourSlotCoresPico2", 10, 10, lambda r: slot_buffer(r, "4-slot")),
         ("ThreeSlotCoresPico2", 10, 10, lambda r: slot_buffer(r, "3-slot")),
         ("FIFOCoresPico2", 3, 18, fifo_cores),
         ("IPCPico2", 5, 10, ipc),
         ("SoakPico2", 5, 19, soak),
         ("LitmusPico2", 20, 31, litmus)]


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("build", nargs="?", default=None)
    parser.add_argument("--flash", action="store_true")
    parser.add_argument("--only", nargs="*", default=None)
    args = parser.parse_args()

    failed = 0
    build = args.build or (EXAMPLES + "-flash" if args.flash else EXAMPLES)
    tests = [t for t in TESTS if not args.only or t[0] in args.only]
    missing = [t[0] for t in tests
               if not os.path.exists(os.path.join(build, t[0] + ".elf"))]
    if missing:
        sys.exit(f"not in {build}: {', '.join(missing)}")
    openocd(["rescue_reset"])
    with tempfile.TemporaryDirectory() as directory:
        seed = None if args.flash else build_seed(directory)
        for name, seconds, words, judge in tests:
            elf = os.path.join(build, name + ".elf")
            values = run(elf, seed, seconds, words)
            checks = litmus(values, False) if args.flash and judge is litmus \
                else judge(values)
            ok = all(passed for passed, _ in checks)
            failed += not ok
            print(f"{'ok  ' if ok else 'FAIL'} {name}")
            for passed, text in checks:
                print(f"     {'' if passed else '!! '}{text}")
    sys.exit(1 if failed else 0)


if __name__ == "__main__":
    main()
