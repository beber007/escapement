# Architecture and targets

Escapement has three kernels, two scheduling algorithms and a handful of lock-free
mechanisms for passing data. This page describes how they fit together, which chips they
run on and how far each port has been verified, and where the sources live. How to use
the kernels is in `api.md`, how to build them in `build.md`.

## Kernel variants

| Variant | Files | Use |
|---|---|---|
| **Hard** | `Escapement/EscapementHard.{c,h}` | Hard real time: deadlines are guaranteed |
| **Soft** | `Escapement/EscapementSoft.{c,h}` | (m,k)-firm real time: out of every k instances of a task, m are guaranteed and the others run only if they can finish in time |
| **Hard PA** | `Escapement/EscapementHardPA.{c,h}` | Hard real time plus dynamic energy management (DVFS) |

## Scheduling algorithm

Two algorithms are implemented. Under earliest deadline first (EDF), the task whose
deadline is nearest runs first. Under deadline-monotonic scheduling (DM), priorities are
fixed before start-up from the declared deadlines. An application picks one in its
`Escapement_Config.h`:

```c
#define SCHEDULER_REAL_TIME_MODE EARLIEST_DEADLINE_FIRST
```

Every example here selects EDF. When an application says nothing, the kernel headers
fall back to DM. Until the host test caught it, that fallback was what every example got
without meaning to (`method.md`).

The power-aware kernel adds EDF*, a deterministic variant of EDF. Its DRA, DR_OTE and
DM_SLACK policies impose their own algorithm whatever the application chose. OTE, the
policy the examples use, works with either.

The names of the algorithms live in `Escapement/Escapement_Modes.h`, which every file
that tests the choice includes first. The examples build in every combination without
editing a file, and the CI runs them all:

```sh
make                                           # hard kernel, earliest deadline first
make SCHEDULER=DEADLINE_MONOTONIC_SCHEDULING   # hard kernel, deadline-monotonic
make KERNEL=SOFT                               # soft kernel, earliest deadline first
make KERNEL=SOFT SCHEDULER=DEADLINE_MONOTONIC_SCHEDULING
```

On the Pico, `make KERNEL=PA` also builds the power-aware kernel. `make KERNEL=PA
UNDERVOLT=1` builds it to run below the specified core voltage, for the measurement bench
only (`power-aware.md`). No other port has the power-aware kernel.

## Synchronisation

The kernels update their queues with load-linked / store-conditional (LL/SC) pairs
rather than by masking interrupts. The Cortex-M3, M4 and M33 have `LDREX`/`STREX`. The
Cortex-M0+ has no exclusive access, so the port emulates it: a flag stands for the
reservation, and every context switch and every interrupt clears it on its way out
(`Escapement_Atomic.c`, `_OSIOHandler`, the end of `_OSContextSwapHandler`). The
emulated store-conditional masks interrupts for the few instructions it takes.

Interrupts are masked in a few other places only. The kernels mask them at start-up,
while the idle task starts the timer, in the traps of `DEBUG_MODE`, and in
`OSSuspendSynchronousTask` from the enqueue of the suspending task until it has left the
ready queue. The last one was added on 2026-09-25, after the endurance test found a race
there (`method.md`). Some drivers of the ports, the timer events and the trace among
them, also mask interrupts for a few instructions.

An application gets the same guarantees from two mechanisms.

- FIFO queues (`OSInitFIFOQueue`) can be shared by any number of tasks and interrupt
  handlers. Their buffers are allocated once. They build on the array-based LL/SC queue
  of Evéquoz (ICPP 2008). One operation at a time is announced, and any caller that
  preempts it completes it, so every operation ends in a bounded number of steps.
- Slot buffers (`OSInitBuffer`) carry data from one writer to one reader, and neither
  waits for the other. The 4-slot buffer follows Simpson (1990) and uses no atomic
  instruction. The 3-slot buffer follows Chen and Burns (1997) with an LL/SC pair.

For use between two cores, each slot buffer orders its accesses with four calls to
`_OSMemoryBarrier()`. That is a `DMB` on the RP2040, the RP2350 and the STM32U5, and a
compiler barrier alone in the host build. On every build of the Pico, the Pico 2 and the
STM32U5, `tools/check_order.py` reads the compiled code and checks that the accesses and
the barriers keep the order of the models, and on the Pico 2 so do the seven of the
queue between the cores (2026-09-30). It also checks the order of the stores a task
makes that the timer interrupt may find half done (`method.md`).

Each mechanism has an exhaustive model in `test/model`, run by the CI. The queue model
runs a few operations preempting one another at every possible access and checks each
run for linearizability. The slot-buffer models explore every interleaving of a writer
with its reader, the writer being an interrupt handler or code on the other core. They
check the properties Rushby model-checked for Simpson's algorithm: no read mixes two
records, and none goes backwards. Between two cores, each core is also free to reorder
its accesses. The models found four defects, now fixed (`method.md`).

### Across the two cores

The kernel runs on core 0 alone. The emulated LL/SC and the announced operation of the
kernel's queue both rely on preemptions nesting, which two cores do not provide. Between
the cores, the slot buffers work, and so does a queue of its own on the RP2350.

The 4-slot buffer crossed between the cores of the Pico on the board (`rp2040.md`) and
between those of the Pico 2, under Renode and on the board. On the RP2350 the 3-slot
buffer works too. Its model holds provided the exclusive monitors see both cores, which
`ACTLR.EXTEXCLALL` gives; the port sets it on each core. `ThreeSlotCoresPico2` passes
under Renode with that monitor played (`emulation.md`), and on a Pico 2 on 2026-09-28:
30 runs of 15 s, some 480,000 reads each, none torn nor backwards, while the plain array
beside it tore some 10,000 times a run (`tools/pico2_check.py`).
Code on core 1 may not signal an event: `OSScheduleSuspendedTask` would pend the timer
interrupt of core 1, where no kernel runs.

Whether the RP2350 reorders at all, `LitmusPico2` asks of the chip: store buffering,
message passing and load buffering between its two cores, each with and without a DMB
between its two accesses (Alglave, Maranget, Sarkar and Sewell, TACAS 2011). Each round
the two cores start at a pseudo-random offset from each other, to the cycle. On a Pico 2
on 2026-09-28, in 10 minutes, each of the six tests ran 69.4 million rounds and none
ended in its weak outcome. Each also met the other core within a cycle or two: in store
buffering without a DMB, both loads saw the other core's store in 109,901 rounds, in
message passing the reader saw the data without the flag in 1.3 million, and in load
buffering both loads came before both stores in 1.4 million. The chip showed no reordering between its cores.
The DMBs stay: the architecture allows the reorderings, and a test that saw none does
not show that none can happen.

The queue between the cores of the RP2350 (`Escapement_CoreQueue.c`, `OSInitCoreQueue`)
is the array-based queue of Evéquoz's Figure 3. It is lock-free rather than wait-free,
and serves any number of producers and consumers on either core. Its model
(`test/model/fifo_mp.py`) called for two changes to fit the chip:

- Figure 3 assumes an SC that fails only when another thread's SC succeeded. The paper
  itself notes that real LL/SC do not promise this. On the RP2350 an SC also fails when
  the other core wrote in its granule, or for no reason at all. The SC that advances
  Tail or Head after an operation is therefore tried again while the index has not
  moved.
- Seven `DMB` order each core's accesses where the queue needs it, each shown needed.
  The first version had fifteen; a model of weakly ordered cores cut them down to six,
  then showed two more needed once it let a store pass an SC ahead of it, as Armv8-M
  allows, and one of the six superfluous (2026-09-29, `method.md`).

The hardware spinlocks of the SIO were no alternative: they are unreliable on that chip
(erratum RP2350-E2 of the
[datasheet](https://datasheets.raspberrypi.com/rp2350/rp2350-datasheet.pdf)).

## Supported targets

- **Raspberry Pi RP2040** (Pico): Cortex-M0+, port under `Escapement/CORTEX-Mx/RP2040/`.
  Its 64-bit timer has four alarms and is clocked **independently of the core clock**.
  The kernel takes two of the alarms, the timer events one of the other two. This is the
  target of the power-aware kernel, with the project's own DVFS driver
  (`power-aware.md`).
- **Raspberry Pi RP2350** (Pico 2): Cortex-M33, port under
  `Escapement/CORTEX-Mx/RP2350/`. It was transposed from the RP2040 port on 2026-09-24:
  the clocks at 150 MHz, TIMER0 with its tick from the TICKS block, 52 interrupts, the
  pads released from their isolation, the UART, the timer events and the launch of
  core 1. The hard and the soft kernel build ten examples under `pico2/` in the CI. They
  are the seven of the Pico that are not benches, plus `ThreeSlotCoresPico2`,
  `FIFOCoresPico2` and `LitmusPico2`. All ten run under Renode on a platform of our own (`emulation.md`),
  including the 2^30 wrap of the kernel clock, both slot buffers between the two cores
  and the queue between them. That shows that they schedule, not that the clocks are
  programmed right. A Pico 2 did on 2026-09-28: the frequency counter gave clk_sys
  150,000 kHz on the PLL, clk_ref and clk_peri 12,000 kHz on the crystal. On it the six
  examples that count in memory pass the criteria of the Renode suite
  (`tools/pico2_check.py`): both slot buffers and the queue between the cores, `IPCPico2`,
  the endurance test and the litmus tests. The frequency counter of the Bus Pirate read
  the outputs on 2026-09-29, loaded as `tools/pico2_check.py` loads, each reading 8 s
  after the one before, and every one agreed with the next:

  | Output | Example, task | Expected | Measured |
  |---|---|---:|---:|
  | `GP4` | `TaskLEDPico2`, 1 ms probe | 500 Hz | 500.01 Hz, 6 readings |
  | `GP2` | `TaskLEDPico2`, 20 ms task | 50 Hz | 50.0014 to 50.0015 Hz, 4 |
  | `GP3` | `TaskLEDPico2`, 60 ms task | 16.66667 Hz | 16.66715 Hz, 4 |
  | `GP2` | `TestTimerEventPico2`, 5 ms event | 200 Hz | 200.007 Hz, 4 |
  | `GP3` | `TestTimerEventPico2`, 10 ms event | 100 Hz | 100.0031 Hz, 4 |

  Every output is +28 to +35 ppm off, as the Pico's were on the same instrument
  (`rp2040.md`, 16.66713 and 100.0031 Hz there): two boards the same, it is the Bus
  Pirate's reference that is some 30 ppm slow rather than either crystal.

  The UART, through the Debug Probe's own on GP0 and GP1, on 2026-09-30
  (`tools/pico2_uart.py`, the CI's images of 3c141ca): `UARTEchoPico2` sent back the
  suite's line, the 256 byte values and 1,408 bytes of lines sent back to back, each
  byte once and in order. `UARTSendersPico2` put 18,106 lines on the port in 20 s, the
  line full, none broken. The lines its tasks had queued when their counts were read,
  17,658, lay between the 17,553 on the port just before and the 17,661 just after.

  The 2^30 wrap on the board, the same day: `TaskWrapPico2` has its periods scaled for
  Renode, a thousand times too long here, so `SoakPico2` crossed it instead, as the
  Pico's endurance test did (`rp2040.md`). It ran 2,400 s across two wraps, read every
  minute over SWD without stopping a core (`tools/pico2_soak.py`): no error in any of
  its eight parts, no restart, the pulse at most 46 µs late and the timer events 40, at
  least 952 bytes of core 1's stack never used. The power-aware kernel is not ported.
- **STM32U5** (the STM32U585 of the Arduino UNO Q): Cortex-M33, port under
  `Escapement/CORTEX-Mx/STM32U5/`. It was written anew on 2026-09-25, after the model of
  the RP2350 port. The clocks run at 160 MHz from the board's 16 MHz crystal. TIM2 serves
  the kernel and TIM5 the timer events. USART1 goes to the connector and LPUART1 to the
  board's Linux. An idle task can sleep in Stop 2, woken by LPTIM1. The hard and the soft
  kernel build ten images under `uno-q/`, two of them for the wrap under Renode:
  `SleepWrapU5`, which is `SleepU5` with its times scaled, and `Stop2EventWrapU5`. They run from SRAM and leave Arduino's firmware
  in the flash. All ten run under Renode on a platform of our own. The board has run `TaskLEDU5` and the
  endurance test since 2026-09-26, the latter for hours (`stm32u5.md`). The same sources
  build `SleepU5` for a NUCLEO-U575ZI-Q, to measure the MCU's current, and the endurance
  test, which has run on that board since 2026-09-28 (`tools/board_ci.md`); `SleepU5`
  ran there on 2026-10-02 (`stm32u5.md`). The power-aware kernel is not ported (`power-aware.md`).
- **ARM Cortex-M33** (ARMv8-M Mainline): the generic layer takes it down the
  Cortex-M3/M4 path under `CORTEX_M33`. The registers to save are the same, and so is
  the frame with the floating-point unit left off, and `LDREX`/`STREX`/`CLREX`. It is
  built without the floating-point unit (`-mcpu=cortex-m33+nofp`). The errata of the
  core itself were read on 2026-09-26. Arm's notice (SDEN-756493, v9.0, April 2018)
  leaves only 1080541 open in r0p4, the STM32U585's core, and that one concerns the MPU,
  which the port does not use. The errata of the context switch, 851802, 937163 and
  1015127 among them, are fixed by r0p4. The RP2350's core is r1p0. That version of the
  notice predates it, and the current one, which would cover it, is not publicly
  served. The RP2350 datasheet lists no erratum of the core.
- **ARM Cortex-M0 / M3 / M4**: the generic layer under `Escapement/CORTEX-Mx/`. The
  Cortex-M33 takes its Cortex-M3/M4 path. The STM32 ports it served were removed one
  after the other:
  - the F0, F1 and F2 on 2026-09-20. Their examples had only ever been compiled, and the
    project demonstrates what the kernel does rather than list the parts it could run
    on;
  - the L1 on 2026-09-22, with its DVFS driver, once the Pico ran every test it ran;
  - the F4 on 2026-09-26. It had been kept for the Cortex-M3/M4 path, which the RP2350
    and the STM32U5 now take; the STM32U5 is checked on its board at each commit.

Escapement began life on the TI MSP430. That port was removed on 2026-09-20
(`roadmap.md`). The history keeps it, and so does the archived `beber007/zottaos`.

## Source tree

```
Escapement/
  EscapementHard.{c,h}      hard real-time kernel
  EscapementSoft.{c,h}      (m,k)-firm real-time kernel
  EscapementHardPA.{c,h}    power-aware hard real-time kernel
  Escapement_Modes.h        names of the scheduling algorithms
  CORTEX-Mx/                ARM port: generic Cortex-M layer, RP2040, RP2350, STM32U5
  CORTEX-Mx/RP2040/Examples/ the Raspberry Pi Pico examples
  CORTEX-Mx/RP2350/Examples/ the Raspberry Pi Pico 2 examples
  CORTEX-Mx/STM32U5/Examples/ the Arduino UNO Q examples, and SleepU5 for the
                            NUCLEO-U575ZI-Q
test/host/                  the three kernels built for the host
test/model/                 exhaustive models of the lock-free mechanisms
emulation/renode/           Renode platforms, models and Robot suites
tools/                      trace capture, loading and board checks, endurance tests,
                            the compiled-order check, static analysis, figures
ci/                         the container image the CI runs in
.github/workflows/build.yml builds the examples and runs them under Renode on
                            every kernel and algorithm; runs the models, the
                            kernels on the host, the order check and the static
                            analysis
```
