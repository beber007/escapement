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
its accesses. The models found six defects, now fixed (`method.md`).

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
ended in its weak outcome. The two cores did meet within a cycle or two: without a
DMB, both loads of store buffering saw the other core's store in 109,901 rounds, the
reader of message passing saw the data without the flag in 1.3 million, and both loads
of load buffering came before both stores in 1.4 million. The DMBs stay: the
architecture allows the reorderings, and a test that saw none does not show that none
can happen.

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
  (`power-aware.md`). The Cortex-M0+ has no stack limit: since 2026-10-05 a region of
  its MPU, no access, covers 1 KB the linker script keeps between the globals and the
  stack, and an overflow ends in the HardFault handler (`StackGuardPico`).
- **Raspberry Pi RP2350** (Pico 2): Cortex-M33, port under
  `Escapement/CORTEX-Mx/RP2350/`. It was transposed from the RP2040 port on 2026-09-24:
  the clocks at 150 MHz, TIMER0 with its tick from the TICKS block, 52 interrupts, the
  pads released from their isolation, the UART, the timer events and the launch of core
  1. The hard and the soft kernel build the examples under `pico2/` in the CI: those of
  the Pico that are not benches, plus `ThreeSlotCoresPico2`, `FIFOCoresPico2` and
  `LitmusPico2`. They run under Renode on a platform of our own (`emulation.md`),
  including the 2^30 wrap of the kernel clock, both slot buffers between the two cores
  and the queue between them; `IdlePico2`, `SleepPico2` and `StackGuardPico2` are for
  the board only. That shows that they schedule, not that the clocks are programmed
  right. A Pico 2 did on 2026-09-28, and the bench checks it at each commit: its clocks,
  the periods on a frequency counter, the UART and the 2^30 wrap on the board are in
  `rp2040.md`, "The Pico 2 on the board". The power-aware kernel is not ported.
- **STM32U5** (the STM32U585 of the Arduino UNO Q): Cortex-M33, port under
  `Escapement/CORTEX-Mx/STM32U5/`. It was written anew on 2026-09-25, after the model of
  the RP2350 port. The clocks run at 160 MHz from the board's 16 MHz crystal. TIM2 serves
  the kernel and TIM5 the timer events. USART1 goes to the connector and LPUART1 to the
  board's Linux. An idle task can sleep in Stop 2, woken by LPTIM1. The hard and the soft
  kernel build the images under `uno-q/`, two of them for the wrap under Renode:
  `SleepWrapU5`, which is `SleepU5` with its times scaled, and `Stop2EventWrapU5`. They
  run from SRAM and leave Arduino's firmware in the flash. All but `StackGuardU5` run
  under Renode on a platform of our own. The board has run `TaskLEDU5` and the
  endurance test since 2026-09-26, the latter for hours (`stm32u5.md`). The same sources
  build `SleepU5` for a NUCLEO-U575ZI-Q, to measure the MCU's current, and the endurance
  test, which has run on that board since 2026-09-28 (`tools/board_ci.md`); `SleepU5`
  ran there on 2026-10-02 (`stm32u5.md`). The power-aware kernel is not ported (`power-aware.md`).
- **ARM Cortex-M33** (ARMv8-M Mainline): the generic layer takes it down the
  Cortex-M3/M4 path under `CORTEX_M33`. The registers to save are the same, and so is
  the frame with the floating-point unit left off, and `LDREX`/`STREX`/`CLREX`. It is
  built without the floating-point unit (`-mcpu=cortex-m33+nofp`). Since 2026-10-04 the
  port sets MSPLIM to the end of the globals: a stack that grows into them faults
  instead of overwriting them, as a Pico 2 and the UNO Q showed (`StackGuardPico2`,
  `StackGuardU5`, roadmap, "A stack that faults"). The errata of the core itself were
  read on 2026-09-26. Arm's notice (SDEN-756493, v9.0, April 2018)
  leaves only 1080541 open in r0p4, the STM32U585's core, and that one concerns the MPU,
  which the port does not use. The errata of the context switch, 851802, 937163 and
  1015127 among them, are fixed by r0p4. The RP2350's core is r1p0. That version of the
  notice predates it, and the current one, which would cover it, is not publicly
  served. The RP2350 datasheet lists no erratum of the core.
- **ARM Cortex-M0 / M3 / M4**: the generic layer under `Escapement/CORTEX-Mx/`. The
  Cortex-M33 takes its Cortex-M3/M4 path. The STM32 ports it served were removed one
  after the other:
  - the F0, F1 and F2 on 2026-09-20. Their examples had only ever been compiled;
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
  CORTEX-Mx/STM32U5/Examples/ the Arduino UNO Q examples, and SleepU5, SleepU5Flash
                            and SoakU5 for the NUCLEO-U575ZI-Q
test/host/                  the three kernels built for the host
test/model/                 exhaustive models of the lock-free mechanisms
test/litmus/                the lock-free mechanisms on an Armv8-A host that reorders
emulation/renode/           Renode platforms, models and Robot suites
tools/                      trace capture, loading and board checks, endurance tests,
                            the compiled-order check, static analysis, figures
ci/                         the container image the CI runs in
.github/workflows/build.yml builds the examples and runs them under Renode on
                            every kernel and algorithm; runs the models, the
                            kernels on the host with their coverage and the
                            differential test, the order check and the static
                            analysis, clang included
```
