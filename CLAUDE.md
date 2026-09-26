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
tools/board_ci.sh --force                     # board checks, run by a timer on the bench;
                                              # BOARD_CI_IMAGES=ci takes the CI's images
tools/board_images.sh OUT                     # the images those checks run (the CI builds them)
tools/timer_events.py <elf>                   # TestTimerEventPico (TRACE=1 + cost build) summed up
tools/dvfs_bench.py <elf>                     # BenchDVFSPico: means of each change of speed
tools/soak.sh 14d 1m                          # endurance test: SoakPico, read without stopping
                                              # it, status board/soak (holds the board-ci lock)
tools/soak_emulated.sh OUT 60 1440 hard::1 soft:KERNEL=SOFT:2   # instances under Renode
tools/soak_emulated_status.sh OUT [SHA]       # their sum, status emulation/soak

# STM32F4 examples — the Cortex-M3/M4 assembler path on Renode's own platform (the
# RP2350 takes that path too, on ours)
make -C Escapement/CORTEX-Mx/STM32/Examples/stm32f4-discovery

# Pico 2 (RP2350, Cortex-M33) — eight examples; no KERNEL=PA yet. Its Renode suite runs
# on a platform of our own, on the Mac too (Renode 1.17 portable, robotframework 6.1 venv)
make -C Escapement/CORTEX-Mx/RP2350/Examples/pico2
renode-test emulation/renode/escapement_pico2.robot

# STM32U5 (NUCLEO-U575ZI-Q, Cortex-M33) — six examples, Renode only so far, on a platform
# of our own as for the Pico 2 (docs/stm32u5.md); no KERNEL=PA
make -C Escapement/CORTEX-Mx/STM32U5/Examples/nucleo-u575zi-q
renode-test emulation/renode/escapement_u5.robot

# The scheduler on the host, every kernel and algorithm, under AddressSanitizer; the CI
# also runs it at -O2 under the whole of UndefinedBehaviorSanitizer
make -C test/host run
make -C test/host run OPT=-O2 SANITIZE=address,undefined BUILD=build-O2

# Exhaustive models of the lock-free mechanisms (run by the CI)
python3 test/model/fourslot.py
python3 test/model/threeslot.py      # ~1 min, up to 1.2 GB
python3 test/model/fifo.py           # ~35 s
python3 test/model/fifo_mp.py        # the queue between the cores, ~1 s

# The compiled order of the slot buffers against the models, and of the task-level
# stores the timer interrupt relies on (run by the CI; --tasks alone for the F4)
tools/check_order.py Escapement/CORTEX-Mx/RP2350/Examples/pico2/build/Escapement*.o
tools/check_order.py --tasks Escapement/CORTEX-Mx/STM32/Examples/stm32f4-discovery/build/Escapement*.o
tools/check_order_mutants.sh Escapement/CORTEX-Mx/RP2350/Examples/pico2   # every one caught

sh tools/check_encoding.sh           # every tracked file must be valid UTF-8

# Static analysis (run by the CI): cppcheck over each port, GCC's -fanalyzer per Cortex-M
sh tools/cppcheck.sh
sh tools/analyze.sh
```

Emulation under Renode: `docs/emulation.md` and `emulation/renode/RP2040.md`. The board:
`docs/rp2040.md` (loading into SRAM over SWD, the trace, `tools/measure_cost.sh`).

## Before a commit

- Host tests, the three models, the encoding check, the static analysis, and every Pico,
  Pico 2 and STM32U5 variant plus the F4 still build.
- A change meant to leave a build alone (comments, an option off by default) must leave
  its images byte for byte identical: compare `arm-none-eabi-objcopy -O binary` outputs
  against those of `main`.
- The three kernels (`EscapementHard.c`, `EscapementSoft.c`, `EscapementHardPA.c`) share
  their FIFO queue and slot-buffer code: a fix to one goes to all three. Likewise the
  RP2040 and RP2350 ports share the logic of their timer, timer events, UART and core 1
  launch, and their examples: a fix to one goes to both. The STM32U5 port shares the
  timer events, the UART and the examples of the RP2350, and the kernel timer of the
  STM32 port: a fix there goes to it too.
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
- Stores a task makes that an interrupt may find half done need a `CompilerBarrier()`
  between each and the next: at -O2 GCC reordered two of them in `OSEndTask` and dropped
  a store it saw overwritten in `ScheduleNextTask`. Add a new sequence to
  `tools/check_order.py` with a mutant in `tools/check_order_mutants.sh`.
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
- OpenOCD's cmsis-dap driver asks every Raspberry Pi USB device for its strings, and the
  Pico's own USB, once a flash firmware has enumerated it, may not answer: 3.3 s lost
  per connection, which failed the cost check. The tools select the Debug Probe by its
  ids, `cmsis_dap_vid_pid 0x2e8a 0x000c`; do the same in a command typed by hand.
- Emulation proves scheduling and register sequences, not energy; the board and an
  instrument decide.
