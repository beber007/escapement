# The Hazard3 cores of the RP2350

The RP2350 of the Pico 2 has two Arm Cortex-M33 cores and two Hazard3 RISC-V cores. The
image chooses which pair runs. Escapement runs on both pairs. The clock, the memory and
the peripherals stay the same, and only the core changes. That makes a fair comparison
of the two architectures (issue #77).

This page says how the RISC-V port works, how it was checked on the board, and what it
measured. The Arm port of the same chip is in `rp2040.md`, "The Pico 2 on the board".

## What is shared, and what is new

The portable kernel did not change: `EscapementHard.c`, `EscapementSoft.c`, and their
FIFO queue and slot buffers. Both architectures build the same drivers of the RP2350:
the timer, the timer events, the UART, the clocks and the launch of core 1. They reach
the interrupt controller through macros, which each architecture defines
(`Escapement_CortexMx.h`, `Escapement_RISCV.h`). Some lines differ under
`#ifdef __riscv`:

- the memory barrier, `fence rw,rw` in place of `DMB` (`Escapement_Processor.h`);
- the table of the peripherals' vectors: on the Hazard3, `meinext` gives the number of
  the interrupt, and the RISC-V layer calls its handler (`Escapement_Interrupts.c`);
- the cycle counter;
- the launch of core 1, described below.

Three functions of ZottaOS are shared between the two architectures, in
`Escapement/Escapement_Port.c`: the soft timer interrupt, the end of the context
switch's preparation, and `OSMalloc`.

The RISC-V layer is new, in `Escapement/RISC-V/`. It is the counterpart of the Cortex-M
layer:

| Need | Cortex-M33 | Hazard3 |
|---|---|---|
| The timer's alarm | an interrupt of the NVIC | an external interrupt, same number |
| The soft timer interrupt | SysTick, lowest priority | SPARE_IRQ 46, forced pending, lowest level |
| The context switch | PendSV | the machine software interrupt (MSI) |
| Registers saved on an interrupt | eight, by the hardware | all that a call does not keep, by the handler |
| LL/SC | `LDREX`/`STREX` | `lr.w`/`sc.w` |
| LL/SC of a byte | `LDREXB`/`STREXB` | `lr.w`/`sc.w` on the word that holds it |
| Stack guard | MSPLIM | a PMP region of 1 KB, no access |
| Start from SRAM | the debugger resumes at 0x20000000 | the bootrom boots an IMAGE_DEF |

The Hazard3 has its own interrupt controller, Xh3irq (RP2350 datasheet, 3.8.6). Its
external interrupt handler follows the one of the pico-sdk. The MSI plays the part of
PendSV. That handler clears the MSI's enable while it runs, and only the outermost one
sets it back. So the MSI is taken once every external handler has returned. A prototype
showed it on the Pico 2 on 2026-11-05, with an interrupt of a higher level nested in the
soft timer's.

An interrupt between an `lr.w` and its `sc.w` makes the `sc.w` fail: the Hazard3 drops
the reservation on the way into a trap and out of it. A store of the other core drops it
too. On 2026-11-06, core 0 waited between an `lr.w` and its `sc.w` while core 1 stored
nearby. Every `sc.w` failed when core 1 stored to the same word, or 4, 8 or 12 bytes
away. None failed at 16 or 32 bytes, nor with no store. The granule of
the global monitor is 16 bytes, as on the Cortex-M33. Nothing needs to be set first,
where the Cortex-M33 needs ACTLR.EXTEXCLALL.

Core 1 starts through the bootrom's handshake on the SIO FIFO, as on Arm. The word of
the vector table is the value of `mtvec`. `sev` and `wfe` become `h3.unblock` and
`h3.block`, two hints of the Hazard3 (pico-sdk, `hardware/hazard3/instructions.h`).

## Building

```sh
brew install riscv64-elf-gcc            # macOS: GCC 16.2, as for Arm
sudo apt install gcc-riscv64-unknown-elf   # Debian / Ubuntu: GCC 14.2 on Ubuntu 26.04
cd Escapement/CORTEX-Mx/RP2350/Examples/pico2
make ARCH=riscv                         # hard kernel, EDF, into build-riscv/
make ARCH=riscv KERNEL=SOFT             # the soft kernel
make ARCH=riscv SCHEDULER=DEADLINE_MONOTONIC_SCHEDULING
make ARCH=riscv RISCV_CROSS_COMPILE=riscv64-unknown-elf-   # Ubuntu's toolchain
```

The base is RV32IMAC, with `-mstrict-align`. Every RISC-V core of 32 bits with atomics
runs it. The Hazard3 has more extensions, which the pico-sdk uses (Zba, Zbb, Zbs, Zbkb,
Zca, Zcb, Zcmp). With them, the images are 1.1 to 1.5 % smaller (2026-11-06). The port
keeps the base.

The images run from SRAM only. `FLASH`, `TRACE`, `WCET`, `LIVE` and `SLEEP_GATE` are
refused under `ARCH=riscv`. The power-aware kernel is not ported to either pair of cores
(#23).

The CI builds the four variants with Ubuntu's GCC 14.2, and checks that a rebuild gives
the same bytes (job `riscv`). Homebrew publishes no `riscv64-elf-gcc` for Linux.

## On the board

No emulator has the RP2350's Hazard3 and its interrupt controller. Renode and QEMU have
neither. The board is the only check.

A RISC-V image is loaded from the Arm side. The tools then ask the bootrom for a RAM
image boot, through the watchdog's scratch registers. The bootrom finds the IMAGE_DEF in
SRAM and switches both cores to RISC-V. The Results are read through core 0's RISC-V
debug, without stopping it. `tools/pico2_check.py`, `pico2_uart.py`, `pico2_soak.py`
and `pico2_latency.py` know a RISC-V image by its ELF header and do this on their own.
Two traps:

- after a RISC-V image, OpenOCD cannot examine the Cortex-M33, and `reset halt` fails.
  A `rescue_reset` first gives it back;
- `reset halt` leaves VC_CORERESET set in DEMCR. A reset then stops the core in the
  bootrom before it reads the scratch registers. The tools clear DEMCR before resuming.

The board CI runs the six examples and the UART on the Hazard3 cores too, on images the
CI builds with Ubuntu's GCC 14.2 (`board/pico2`, `tools/board_ci.md`). The table below
was checked by hand, on images built with GCC 16.2. Its last line also ran the CI's
images, built with GCC 14.2.

| Date | What | Result |
|---|---|---|
| 2026-11-05 | `TaskLEDPico2`, hard kernel | runs at 150 MHz, no fault |
| 2026-11-05 | the PMP guard | a store to the first and the last word of the 1 KB faults, one on each side does not |
| 2026-11-05 | `IPCPico2` | passes; a mutant that does not restore `s5` changed 2,861 registers, caught |
| 2026-11-05 | `UARTEchoPico2`, `UARTSendersPico2` | pass |
| 2026-11-05 | `TestTimerEventPico2` | 20.1 and 19.7 % (Arm: 20.2 and 20.2) |
| 2026-11-06 | the six examples of `pico2_check.py` | pass, under the hard kernel with EDF and with DM, and the soft kernel with EDF and with DM |
| 2026-11-06 | `SoakPico2`, its writer on core 1 | 2,399 s, two wraps, no error; the pulse at most 50 µs late |
| 2026-11-06 | `SoakFirmPico2`, EDF and DM | 10 min each, no error; about 40 % of the optional instances dropped; every mandatory one on time |
| 2026-11-07 | the part of `board/pico2` on the Hazard3 cores: on the CI's images (GCC 14.2), then again with core 1's stack aligned to 16 bytes (GCC 16.2) | the six examples and the UART pass, in 80 s |

The litmus tests found no access reordered between the Hazard3 cores, with or without a
`fence`. There were 2.56 million rounds of each test, on 2026-11-06. That shows nothing
was reordered then. It does not prove the cores never reorder.

`tools/check_order.py` reads the Arm disassembly only. The order of the barriers in the
RISC-V objects is checked by the board alone (#78).

## Measurements

On the Pico 2, at 150 MHz, under the hard kernel and EDF, on 2026-11-06.

**A scheduling round.** From the alarm's handler to the next alarm armed, counted in
cycles of the core: the DWT's CYCCNT on the Cortex-M33, `mcycle` on the Hazard3. It is
built with `ESCAPEMENT_MEASURE_SCHEDULING_COST` (`Escapement_Config.h`).

| Example | Cortex-M33 | Hazard3 |
|---|---:|---:|
| `TaskLEDPico2`, three tasks | 294.9 cycles (min 260, max 919) | 286.2 (min 255, max 849) |
| `LatencyPico2`, one task | 230 | 231 |

The rounds cost the same within 3 %. The timer read 1.28 µs and 2.17 µs for the same
rounds of `TaskLEDPico2`. The timer counts whole microseconds, and the round falls at a
different phase against them on each core (#76).

**From the alarm to the task.** `LatencyPico2` counts the cycles from the alarm of an
arrival to the first instruction of its task. Its idle task stores the cycle counter in
a loop of three instructions, and the task reads the counter as it starts. The count
starts when the core leaves that loop. The path of the alarm from the timer to the core
is not in it. `tools/pico2_latency.py` runs it.

| | Cortex-M33 | Hazard3 |
|---|---:|---:|
| Alarm to task | 371 cycles, 371 to 373 (2.47 µs) | 474 cycles, 473 to 476 (3.16 µs) |
| The same, with the counters of the round | 452 | 531 |

The round costs the same on both. So the difference is the entry of the interrupt and
the context switch: 103 cycles, or 79 with the counters of the round. The Hazard3 saves
in software what the Cortex-M33 stacks in hardware.

**Sizes.** `text` in bytes, with GCC 16.2. The extensions are those of the pico-sdk
listed under "Building".

| | Cortex-M33 | Hazard3, RV32IMAC | Hazard3, extensions |
|---|---:|---:|---:|
| `TaskLEDPico2` | 4,596 | 4,864 | 4,792 |
| `IPCPico2` | 6,352 | 7,352 | 7,248 |
| `SoakPico2` | 8,848 | 10,008 | 9,864 |
| `FIFOCoresPico2` | 5,232 | 5,696 | 5,632 |
| the context switch and the handlers, in assembly | 112 | 612 | 610 |

**Not measured yet.** The current, with the PPK2.
