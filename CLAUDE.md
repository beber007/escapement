# CLAUDE.md

Guidance for Claude Code (claude.ai/code) working in this repository. What the project
is and how it is verified lives in `README.md` and `docs/`; this file only holds what a
session needs to work here and would otherwise have to rediscover.

## Commands

```sh
# Pico (RP2040) examples — the main target
cd Escapement/CORTEX-Mx/RP2040/Examples/pico
make                                          # hard kernel, EDF
make SCHEDULER=DEADLINE_MONOTONIC_SCHEDULING  # hard kernel, DM
make KERNEL=SOFT                              # (m,k)-firm kernel
make KERNEL=PA                                # power-aware kernel, DVFS
make KERNEL=PA UNDERVOLT=1                    # below the specified voltage, bench only
make KERNEL=PA SLEEP_SPEED=0                  # idle task sleeps at 12 MHz (default 125)
make TRACE=1                                  # scheduling trace in RAM (tools/read_trace.py)
make KERNEL=PA bench                          # BenchDVFSPico, BenchVregPico: board timings
tools/fourslot_cores.sh                       # FourSlotCoresPico: 4-slot buffer across cores
tools/board_ci.sh --force                     # board checks, run by a timer on the bench
tools/timer_events.py <elf>                   # TestTimerEventPico (TRACE=1 + cost build) summed up
tools/dvfs_bench.py <elf>                     # BenchDVFSPico: means of each change of speed

# STM32F4 examples — the Cortex-M3/M4 assembler path on Renode's own platform (the
# RP2350 takes that path too, on ours)
make -C Escapement/CORTEX-Mx/STM32/Examples/stm32f4-discovery

# Pico 2 (RP2350, Cortex-M33) — six examples; no KERNEL=PA yet. Its Renode suite runs
# on a platform of our own, on the Mac too (Renode 1.17 portable, robotframework 6.1 venv)
make -C Escapement/CORTEX-Mx/RP2350/Examples/pico2
renode-test emulation/renode/escapement_pico2.robot

# The scheduler on the host, every kernel and algorithm, under AddressSanitizer
make -C test/host run

# Exhaustive models of the lock-free mechanisms (run by the CI)
python3 test/model/fourslot.py
python3 test/model/threeslot.py      # ~1 min, up to 1.2 GB
python3 test/model/fifo.py           # ~35 s

sh tools/check_encoding.sh           # every tracked file must be valid UTF-8
```

Emulation under Renode: `docs/emulation.md` and `emulation/renode/RP2040.md`. The board:
`docs/rp2040.md` (loading into SRAM over SWD, the trace, `tools/measure_cost.sh`).

## Before a commit

- Host tests, the three models, the encoding check, and every Pico and Pico 2 variant
  plus the F4 still build.
- A change meant to leave a build alone (comments, an option off by default) must leave
  its images byte for byte identical: compare `arm-none-eabi-objcopy -O binary` outputs
  against those of `main`.
- The three kernels (`EscapementHard.c`, `EscapementSoft.c`, `EscapementHardPA.c`) share
  their FIFO queue and slot-buffer code: a fix to one goes to all three. Likewise the
  RP2040 and RP2350 ports share the logic of their timer, timer events, UART and core 1
  launch, and their examples: a fix to one goes to both.
- A result stated in the docs is a measured one, with its date; one that did not
  reproduce is said so, not quietly replaced (`docs/method.md`).

## Conventions

- Commit messages in English, imperative subject, a body that explains why.
- License headers: a file carrying code from ZottaOS keeps the original MIS/HEIG-VD
  notice, its three sentences and "Authors: MIS-TIC", followed by a line for the changes
  made since; a file written anew carries the project's own short header. Code copied
  from elsewhere keeps its notice and license text, and is listed in `NOTICE`.
- Comments explain why, in the style of the file around them. Sources — datasheets,
  papers, SDKs — are cited where a fact rests on them.

## Pitfalls met here

- `#if` on a macro not yet defined reads it as 0. `Escapement_CortexMx.h` is read before
  the port defines its operating points: compare such macros in C, not in `#if`.
  Scheduling names come from `Escapement_Modes.h`, included first for the same reason.
- A store-conditional can fail although the value did not change — any interrupt between
  it and its load-linked, on the emulated pair of the Cortex-M0+ as with LDREX/STREX.
  Retry it; never take a single SC for a compare-and-swap.
- In a model of lock-free code, make the SC a step of its own: merged with the reads
  before it, the model hides the very window the reservation protects. Give each model
  faulty variants it must catch.
- On the RP2040, stopping a core with the debugger pauses the timer, and loading an
  image leaves the previous one's timer interrupts behind. Observe a running board with
  the trace, not by halting it.
- At 12 MHz the core and the 1 µs timer run off the same crystal: a timing loop meets
  the counter at the same phase every time. Dither before each measurement. Two loops
  on the two cores fall into step the same way.
- OpenOCD halts both cores and resumes core 0 only. That held core 1 also paused the
  watchdog a flash firmware may have armed: an image that runs core 1 must be loaded
  with `set USE_CORE 0` and disarm the watchdog, or the chip reboots within a second.
- Emulation proves scheduling and register sequences, not energy; the board and an
  instrument decide.
