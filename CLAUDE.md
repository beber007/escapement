# CLAUDE.md

Guidance for Claude Code (claude.ai/code) working in this repository. What the project
is and how it is verified lives in `README.md` and `docs/`; this file only holds what a
session needs to work here and would otherwise have to rediscover.

## Rules that do not change

Set by the user; no session reopens them.

- **One task worked on at a time, each carried to its end.** A task is finished when it
  is done, checked by its instrument, documented and committed. What turns up along the
  way, a defect, an idea, a measurement worth making, is written in the open list of
  `docs/roadmap.md` and left there, not followed. Where a finding blocks the task in
  hand, say so and ask, rather than start a second direction.
- **A task left with nothing but a wait may be set aside** (rule changed by the user on
  2026-10-09). When all its work is done and only a passive wait with a known end
  remains, an endurance run, the CI, the user's hands on the bench, the next task of
  `docs/roadmap.md`, in its order, may begin, provided it touches neither the board, the
  probe nor the machine of a run in progress. When a wait ends, closing that task comes
  before anything else. At most two tasks wait at once; beyond that, wait.
- **A release ships when it is coherent, not on a date.** Every claim it makes is
  checked on the code it ships, and every endurance run and measurement it cites was made
  on that code. There is no deadline to trade that against.

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
make FLASH=1                                  # the same, into the flash (build-flash/), which
                                              # OPENOCD=<Raspberry Pi's> tools/pico_flash.sh ELF writes
make SLEEP_GATE=1                             # idle task in SLEEP, clocks gated (bench only)
make TRACE=1                                  # scheduling trace in RAM (tools/read_trace.py)
make KERNEL=PA bench                          # BenchDVFSPico, BenchVregPico: board timings
tools/fourslot_cores.sh                       # FourSlotCoresPico: 4-slot buffer across cores
tools/board_ci.sh --force                     # board checks, run by a timer on the bench;
                                              # BOARD_CI_IMAGES=ci takes the CI's images
tools/board_images.sh OUT                     # the images those checks run (the CI builds them)
tools/timer_events.py <elf>                   # TestTimerEventPico (TRACE=1 + cost build) summed up
tools/dvfs_bench.py <elf>                     # BenchDVFSPico: means of each change of speed
tools/soak.py pico 14d 1m                     # endurance test: SoakPico, read without stopping
                                              # it, status board/soak (holds its probe's lock);
                                              # started again, it takes its run over
tools/bench_status.sh                         # the bench on one screen, over SSH (board_ci.md)
tools/soak_emulated.sh OUT 60 1440 hard::1 soft:KERNEL=SOFT:2   # instances under Renode
tools/soak_emulated_status.sh OUT [SHA]       # their sum, status emulation/soak

# Pico 2 (RP2350, Cortex-M33) — no KERNEL=PA yet. Its Renode suite runs on a platform of
# our own, on the Mac too (Renode 1.17 portable, robotframework 6.1 venv)
make -C Escapement/CORTEX-Mx/RP2350/Examples/pico2
renode-test emulation/renode/escapement_pico2.robot
OPENOCD=~/opt/openocd-rpi/bin/openocd PROBE=probe3 tools/pico2_check.py DIR   # on the UNO Q:
                                              # the six examples that count in memory
PROBE=probe3 tools/pico2_uart.py DIR          # the UART echo and senders, on the probe's UART
make -C Escapement/CORTEX-Mx/RP2350/Examples/pico2 FLASH=1   # the same, into the flash
OPENOCD=... PROBE=probe3 tools/pico2_check.py --flash DIR   # written there and booted
PROBE=probe3 tools/pico2_soak.py DIR          # SoakPico2 for 40 min: two 2^30 wraps
                                              # (both with OPENOCD as above, holding the lock)
ADAPTER_KHZ=1000 OPENOCD=... PROBE=probe3 tools/pico2_sleep_load.sh build/SleepPico2.elf
PROBE=probe3 tools/pico2_sleep_load.sh --wake 285     # SleepPico2: WFI, SLEEP, DORMANT for
                                              # the PPK2 (roadmap item 5), its DORMANT woken
                                              # by bytes on GP1; PPK2_SOURCE_MV=5000
                                              # PPK2_PHASES=WFI,SLEEP,DORMANT tools/ppk2_nucleo.py
make SLEEP_GATE=1 -C ...pico2                  # IdlePico2's idle task in SLEEP, PLL_SYS stopped
                                              # (roadmap item 5); loaded as SleepPico2 is

# STM32U5 (Arduino UNO Q, STM32U585, Cortex-M33) — examples run from SRAM, which
# leaves Arduino's firmware in the flash; its Renode suite runs on a platform of our own
# as for the Pico 2 (docs/stm32u5.md); no KERNEL=PA
make -C Escapement/CORTEX-Mx/STM32U5/Examples/uno-q
renode-test emulation/renode/escapement_u5.robot
tools/unoq_load.sh build/SoakU5.elf           # load and start on the board, over SSH
tools/unoq_load.sh --reset                    # back to Arduino's firmware
tools/soak.py uno-q 0 1m SoakU5.elf           # on the UNO Q: SoakU5, read on LPUART1, until
                                              # stopped (service escapement-soak-u5)
make -C Escapement/CORTEX-Mx/STM32U5/Examples/nucleo-u575 [PHASES=30] [SMPS=1] [MHZ=16]   # SleepU5,
                                              # SoakU5 for a NUCLEO-U575ZI-Q; current: JP5;
                                              # SleepU5Flash.elf runs from its flash
tools/unoq_check.sh SoakU5.elf SHA            # on the UNO Q: the board check of a commit
                                              # (status board/u5), then the long run goes on;
                                              # SleepU5.elf, then SleepNoHSEU5.elf, beside
                                              # it run first (unoq_sleep.py)
tools/soak.py nucleo 0 1m SoakU5.elf          # on the UNO Q: SoakU5 of Examples/nucleo-u575
                                              # on a NUCLEO-U575ZI-Q, over its ST-LINK
                                              # (service escapement-soak-nucleo)
make -C .../nucleo-u575 KERNEL=SOFT           # SoakFirmU5 too: SoakU5 with an (m,k)-firm
                                              # task whose optional instances get dropped;
                                              # every port's soft build has its SoakFirm

# STM32U3 (NUCLEO-U385RG-Q, Cortex-M33) — written from RM0487 before the board came, on
# the bench since 2026-10-09; hard and soft kernels, the idle task in Stop 2, no KERNEL=PA
# yet; Renode on a platform of our own whose RCC, PWR and FLASH check the manual's rules,
# the wake-up from Stop 2 included (docs/stm32u3.md)
make -C Escapement/CORTEX-Mx/STM32U3/Examples/nucleo-u385 [MHZ=96|48|24|12] [FAST=1]
                                              # PHASES=30 [WAKE=|RUN=]: SleepU3 for the PPK2
renode-test emulation/renode/escapement_u3.robot
BOARD=u3 NUCLEO_SERIAL=... NUCLEO_TTY=... OPENOCD=~/opt/openocd-upstream/bin/openocd \
    tools/unoq_check.sh SoakU3.elf SHA       # on the UNO Q: the board check (board/u3)
tools/soak.py nucleo-u3 0 1m SoakU3.elf       # its long run (service escapement-soak-u3)
renode-test --variable MHZ:48 --variable PLATFORM:escapement_u3_48mhz.repl --include stop2 \
    emulation/renode/escapement_u3.robot      # the Stop 2 tests on a MHZ=48 build (24, 12)

# The scheduler on the host, every kernel and algorithm, under AddressSanitizer; the CI
# also runs it at -O2 under the whole of UndefinedBehaviorSanitizer
make -C test/host run
make -C test/host run OPT=-O2 SANITIZE=address,undefined BUILD=build-O2
python3 tools/coverage.py            # every kernel line covered or excluded with its reason
                                     # (COVERAGE-LINE, COVERAGE-OFF/ON), branches >= the
                                     # floor in test/host/coverage-floor (run by the CI)
python3 tools/differential.py        # random task sets run by each build, every trace
                                     # checked against EDF or DM, events and (m,k)-firm
                                     # sets included, and the power-aware speeds against
                                     # tools/speed_reference.py (run by the CI, ~40 s)
python3 tools/firm_admission.py      # (m,k)-firm sets under EDF simulated whole, the
                                     # kernel's admission test against the demand test
                                     # prototype: optional instances run, deadlines missed
python3 tools/mutants.py hard --jobs 2   # one fault at a time in a kernel, the host test
                                     # and differential.py on each (some 2 h a kernel)
                                     # (on home, not this Mac; --survivors R.jsonl, --score
                                     # R.jsonl; equivalents: test/host/equivalent-mutants.jsonl)

# Exhaustive models of the lock-free mechanisms (run by the CI)
python3 test/model/fourslot.py
python3 test/model/threeslot.py      # ~1 min, up to 1.2 GB; --jobs N in parallel
python3 test/model/fifo.py           # ~35 s
python3 test/model/fifo_mp.py        # the queue between the cores, ~100 s, 2.3 GB

# The queue between the cores and the 3- and 4-slot buffers on a processor that reorders:
# an Armv8-A host, the Mac (test/litmus, docs/method.md); each with its barriers, then
# without each, ~1 min; test/host builds the kernel for it with HOST_LITMUS
sh test/litmus/run.sh

# The compiled order of the slot buffers and of the queue between the cores against the
# models, and of the task-level stores the timer interrupt relies on (run by the CI)
tools/check_order.py Escapement/CORTEX-Mx/RP2350/Examples/pico2/build/Escapement*.o
tools/check_order_mutants.sh Escapement/CORTEX-Mx/RP2350/Examples/pico2   # every one caught

sh tools/check_encoding.sh           # every tracked file must be valid UTF-8

# Static analysis (run by the CI): cppcheck over each port, GCC's -fanalyzer per Cortex-M
sh tools/cppcheck.sh
sh tools/analyze.sh
sh tools/clang_check.sh              # a third compiler: clang, -Wall, compiled not linked
```

Emulation under Renode: `docs/emulation.md` and `emulation/renode/RP2040.md`. The board:
`docs/rp2040.md` (loading into SRAM over SWD, the trace, `tools/measure_cost.sh`).

## Before a commit

- Host tests, `coverage.py`, `differential.py`, the four models, the encoding check, the
  static analysis (cppcheck, `-fanalyzer`, clang), and every Pico, Pico 2, STM32U5 and STM32U3
  variant still build.
- A change meant to leave a build alone (comments, an option off by default) must leave
  its images byte for byte identical: compare `arm-none-eabi-objcopy -O binary` outputs
  against those of `main`.
- The three kernels (`EscapementHard.c`, `EscapementSoft.c`, `EscapementHardPA.c`) share
  their FIFO queue and slot-buffer code: a fix to one goes to all three. Likewise the
  RP2040 and RP2350 ports share the logic of their timer, timer events, UART and core 1
  launch, and their examples: a fix to one goes to both. The STM32U5 port shares the
  timer events, the UART and the examples of the RP2350: a fix there goes to it too.
- A result stated in the docs is a measured one, with its date; one that did not
  reproduce is said so, not quietly replaced (`docs/method.md`).

## Before a release

Written on 2026-10-07; the user left the three open choices to the assistant, who made
them as below. `docs/roadmap.md`, "Release 0.1", applies this list to the first release.

1. **A freeze**: a tag `vX.Y.Z-rcN`. From it on, fixes only, each a new candidate.
2. **The CI and the board checks pass** on the candidate (`build`, `board/pico`,
   `board/pico2`, `board/u5`, `board/u3`).
3. **A week of endurance on the candidate** for each kernel and each port changed since
   its last week (`git log` of `Escapement/`), on a board that runs it; one unchanged
   keeps its week, cited with its commit.
4. **Each measurement the release cites was made on the code it ships**, or is taken
   out, or marked as made on an older version.
5. **Each claim of `README.md` checked** on the candidate (`docs/method.md`, "What the
   README claims"), and "Hypotheses that were wrong" up to date.
6. **The mutants**: every survivor in code changed since the last release read, then
   killed, declared equivalent with its reason, or explained in `docs/method.md`; the
   scores measured again on the candidate. For 0.1, every survivor.
7. **Coverage**: every line run or excluded with its reason, branches above the floor.
8. **The licence**: `NOTICE` lists every third-party component; each file derived from
   ZottaOS keeps its notice, each new one the project's header.
9. **The known limits written**, and whether the API is stable (not before 1.0).
10. **The version**: a number in `Escapement.h`, introduced at the first release; the
    "Version identifier" lines of 2012 in the headers stay, as ZottaOS's.
11. **The notes, the tag, a GitHub release.**

## Conventions

- Commit messages in English, imperative subject, a body that explains why.
- License headers: a file carrying code from ZottaOS keeps the original MIS/HEIG-VD
  notice, its three sentences and "Authors: MIS-TIC", followed by a line for the changes
  made since; a file written anew carries the project's own short header. Code copied
  from elsewhere keeps its notice and license text, and is listed in `NOTICE`.
- Comments explain why, in the style of the file around them. Sources — datasheets,
  papers, SDKs — are cited where a fact rests on them.

## The bench

The boards hang on the UNO Q, not on this Mac: `tools/board_ci.md`, "The bench", says
which probe drives which board and holds the rules. Run `tools/bench_status.sh` before
any work there. probe2 carries a week-long endurance run: never load anything on it.
Rebooting the UNO Q or unbinding its drivers interrupts every run: ask first.

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
  the trace, not by halting it. A cost build (`ESCAPEMENT_MEASURE_SCHEDULING_COST`) does
  the reverse: the timer runs on while the debugger holds the core, and a halt of a few
  ms stops the kernel on a `DEBUG_MODE` overload check. Read it without halting.
- At 12 MHz the core and the 1 µs timer run off the same crystal: a timing loop meets
  the counter at the same phase every time. Dither before each measurement. Two loops
  on the two cores fall into step the same way.
- OpenOCD halts both cores and resumes core 0 only. That held core 1 also paused the
  watchdog a flash firmware may have armed: an image that runs core 1 must be loaded
  with `set USE_CORE 0` and disarm the watchdog, or the chip reboots within a second.
- In SLEEP (`make SLEEP_GATE=1`) the probe reads zeros from the whole bus, without an
  error: hold core 1, not core 0, for each read, as `tools/read_trace.py` does. The
  SLEEP_EN registers outlive a debugger's reset, even the rescue DP's; the port sets
  them back in `OSInitializeSystemClocks`, and an image that does not must be suspected.
- OpenOCD's cmsis-dap driver asks every Raspberry Pi USB device for its strings, and the
  Pico's own USB, once a flash firmware has enumerated it, may not answer: 3.3 s lost
  per connection, which failed the cost check. The tools select the Debug Probe by its
  ids and serial, `tools/probe.sh` (`PROBE=probe2`, probe1 by default): the bench has
  three; pass `-c "$(tools/probe.sh)"` in a command typed by hand.
- A Pico 2 just powered runs its flash, which may keep the debugger from examining core
  0: `rescue_reset` (Raspberry Pi's target/rp2350.cfg) gets it back, as the tool does.
- On the RP2350 a debugger's reset stops core 0 before the bootrom seeds the RCP, and
  core 1's bootrom then never answers the launch: run `tools/rp2350_rcp_seed.S` first.
  That reset also leaves core 1 running the previous image, which wrote into the next
  one's heap (a 3-slot pointer moved by 3, up to 38 % of reads torn): force it off in the
  PSM before loading, as `tools/pico2_check.py` does.
- An image whose idle task sleeps in Stop 2 (`OSInitStop2`) clears DBG_STOP and
  DBG_STANDBY: the debug port is unpowered most of the time, and OpenOCD connecting then
  fails on an SWD parity error, the image left running. Connect with the reset held, as
  `tools/unoq_load.sh` does (`srst_nogate connect_assert_srst`).
- The two OpenOCDs of the UNO Q differ: Debian's (`/usr/bin/openocd`, the NUCLEO's)
  sets DBG_STOP and DBG_STANDBY at each connection, Arduino's (`/opt/openocd`) clears
  them, and a system reset leaves them. `OSInitStop2` clears both: with DBG_STANDBY
  alone set, the NUCLEO never woke from Stop 2 and only a power-off got it back. The
  NUCLEO's NRST reaches the MCU through JP2: check it is fitted, or a "reset halt"
  resets nothing and the image is started wherever the core was.
- The PPK2 on the NUCLEO (`tools/ppk2_nucleo.py`): an ampere meter in place of JP5, VIN
  from JP4's side, VOUT to the MCU. The other way round the MCU runs through a diode and
  the PPK2 reads 0. The debugger never reaches the MCU through it: program the flash with
  JP5 fitted, then unplug CN1 before measuring, or the debug domain stays powered.
- The PPK2 as a source cuts the board's power when the script that drives it ends: load
  the image while `tools/ppk2_nucleo.py` runs, then `--wake` above. Powered so, the Pico
  2's SWD read nothing at 5 MHz, and does at 1 (`ADAPTER_KHZ=1000`).
- A firmware run before leaves the RP2040's and RP2350's clock dividers as it set them,
  which a debugger's reset keeps: the Pico 2's left clk_ref divided by 2, TIMER0 at half
  speed. `OSInitializeSystemClocks` sets them back to 1.
- Emulation proves scheduling and register sequences, not energy; the board and an
  instrument decide.
