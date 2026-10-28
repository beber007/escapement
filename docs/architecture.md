# Architecture and targets

Escapement has three kernels, two scheduling algorithms and a few lock-free mechanisms
to pass data. This page says how they fit together. It lists the chips they run on, how
far each port is checked, and where the sources live. How to use the kernels is in
`api.md`. How to build them is in `build.md`.

## Kernel variants

| Variant | Files | Use |
|---|---|---|
| **Hard** | `Escapement/EscapementHard.{c,h}` | Hard real time: deadlines are guaranteed |
| **Soft** | `Escapement/EscapementSoft.{c,h}` | (m,k)-firm real time: out of every k instances of a task, m are guaranteed and the others run only if they can finish in time |
| **Hard PA** | `Escapement/EscapementHardPA.{c,h}` | Hard real time plus dynamic energy management (DVFS) |

## Scheduling algorithm

Two algorithms are implemented. Under earliest deadline first (EDF), the task with the
nearest deadline runs first. Under deadline-monotonic scheduling (DM), priorities are
fixed before the start, from the declared deadlines. An application picks one in its
`Escapement_Config.h`:

```c
#define SCHEDULER_REAL_TIME_MODE EARLIEST_DEADLINE_FIRST
```

Every example here selects EDF. An application that says nothing gets DM: the kernel
headers fall back to it. Until the host test caught it, every example got that fallback
without meaning to (`method.md`).

The power-aware kernel adds EDF*, a deterministic variant of EDF. Its policies DRA,
DR_OTE and DM_SLACK impose their own algorithm, whatever the application chose. OTE, the
policy of the examples, works with either.

The names of the algorithms live in `Escapement/Escapement_Modes.h`. Every file that
tests the choice includes it first. The examples build in every combination without
editing any file, and the CI runs them all:

```sh
make                                           # hard kernel, earliest deadline first
make SCHEDULER=DEADLINE_MONOTONIC_SCHEDULING   # hard kernel, deadline-monotonic
make KERNEL=SOFT                               # soft kernel, earliest deadline first
make KERNEL=SOFT SCHEDULER=DEADLINE_MONOTONIC_SCHEDULING
```

On the Pico and the NUCLEO-U385RG-Q, `make KERNEL=PA` also builds the power-aware
kernel. On the Pico, `make KERNEL=PA UNDERVOLT=1` builds it to run below the specified
core voltage. That build is for the measurement bench only (`power-aware.md`). No other
port has the power-aware kernel.

## Synchronisation

The kernels update their queues with load-linked / store-conditional (LL/SC) pairs, not
by masking interrupts. The Cortex-M3, M4 and M33 have `LDREX`/`STREX`. The Cortex-M0+
has no exclusive access, so the port emulates it. A flag stands for the reservation.
Every context switch and every interrupt clears it on its way out
(`Escapement_Atomic.c`, `_OSIOHandler`, the end of `_OSContextSwapHandler`). The
emulated store-conditional masks interrupts for its few instructions.

The kernels mask interrupts in a few other places only:

- at start-up;
- while the idle task starts the timer;
- in the traps of `DEBUG_MODE`;
- in `OSSuspendSynchronousTask`, from the enqueue of the suspending task until it has
  left the ready queue. This one was added on 2026-09-25, after the endurance test found
  a race there (`method.md`).

Some drivers of the ports, among them the timer events and the trace, also mask
interrupts for a few instructions.

An application gets the same guarantees from two mechanisms:

- FIFO queues (`OSInitFIFOQueue`) may be shared by any number of tasks and interrupt
  handlers. Their buffers are allocated once. They build on the array-based LL/SC queue
  of Evéquoz (ICPP 2008). One operation at a time is announced. Any caller that preempts
  it completes it, so every operation ends in a bounded number of steps.
- Slot buffers (`OSInitBuffer`) carry data from one writer to one reader, and neither
  waits for the other. The 4-slot buffer follows Simpson (1990) and uses no atomic
  instruction. The 3-slot buffer follows Chen and Burns (1997), with an LL/SC pair.

Between two cores, each slot buffer orders its accesses with four calls to
`_OSMemoryBarrier()`. Both buffers share a fifth, before a full slot is marked unread.
`_OSMemoryBarrier()` is a `DMB` on the RP2040, the RP2350, the STM32U5 and the STM32U3,
and a compiler barrier alone in the host build. `tools/check_order.py` reads the
compiled code of every build of the four ports. It checks that the accesses and the
barriers keep the order of the models. On the Pico 2, it checks the queue between the
cores too, with its seven barriers (2026-09-30). It also checks the order of the stores
a task makes that the timer interrupt may find half done (`method.md`).

Each mechanism has an exhaustive model in `test/model`, run by the CI:

- The queue's model runs a few operations that preempt one another at every possible
  access. It checks each run for linearizability. A descriptor lives on its operation's
  stack. The model checks that each access to it comes while that operation still runs
  (2026-10-23).
- The slot buffers' models explore every interleaving of a writer with its reader. The
  writer is an interrupt handler or code on the other core. The models check the
  properties that Rushby model-checked for Simpson's algorithm: no read mixes two
  records, and none goes backwards. Between two cores, each core is also free to reorder
  its accesses.

The models found six defects, now fixed (`method.md`).

### Across the two cores

The kernel runs on core 0 alone. The emulated LL/SC and the announced operation of the
kernel's queue both rely on preemptions that nest, which two cores do not give. Between
the cores, the slot buffers work. On the RP2350, so does a queue made for the purpose
(below).

The 4-slot buffer crossed between the cores of the Pico on the board (`rp2040.md`). It
crossed between those of the Pico 2 too, under Renode and on the board. On the RP2350,
the 3-slot buffer works as well. Its model holds if the exclusive monitors see both
cores. `ACTLR.EXTEXCLALL` gives that, and the port sets it on each core.
`ThreeSlotCoresPico2` passes under Renode, with that monitor played (`emulation.md`). It
passed on a Pico 2 on 2026-09-28: 30 runs of 15 s, some 480,000 reads each, none torn or
backwards. The plain array beside it tore some 10,000 times a run
(`tools/pico2_check.py`).

Code on core 1 may not signal an event. `OSScheduleSuspendedTask` would pend the timer
interrupt of core 1, where no kernel runs.

`LitmusPico2` asks the chip whether the RP2350 reorders at all. It runs three tests
between the two cores: store buffering, message passing and load buffering (Alglave,
Maranget, Sarkar and Sewell, TACAS 2011). Each runs with and without a DMB between its
two accesses. At each round, the two cores start at a pseudo-random offset from each
other, to the cycle. On a Pico 2 on 2026-09-28, each of the six tests ran 69.4 million
rounds in 10 minutes. None ended in its weak outcome. The two cores did meet within a
cycle or two. Without a DMB:

- both loads of store buffering saw the other core's store in 109,901 rounds;
- the reader of message passing saw the data without the flag in 1.3 million;
- both loads of load buffering came before both stores in 1.4 million.

The DMBs stay. The architecture allows the reorderings, and a test that saw none does
not show that none can happen.

The queue between the cores of the RP2350 (`Escapement_CoreQueue.c`, `OSInitCoreQueue`)
is the array-based queue of Evéquoz's Figure 3. It is lock-free, not wait-free. It
serves any number of producers and consumers on either core. Its model
(`test/model/fifo_mp.py`) called for two changes to fit the chip:

- Figure 3 assumes an SC that fails only when another thread's SC succeeded. The paper
  itself notes that real LL/SC do not promise this. On the RP2350, an SC also fails when
  the other core wrote in its granule, or for no reason at all. The SC that advances
  Tail or Head after an operation is therefore tried again while the index has not
  moved.
- Seven `DMB` order each core's accesses where the queue needs it, each shown needed.
  The first version had fifteen. A model of weakly ordered cores cut them down to six.
  The model then let a store pass an SC ahead of it, as Armv8-M allows. It showed two
  more needed, and one of the six superfluous (2026-09-29, `method.md`).

The hardware spinlocks of the SIO were no alternative. They are unreliable on that chip
(erratum RP2350-E2 of the
[datasheet](https://datasheets.raspberrypi.com/rp2350/rp2350-datasheet.pdf)).

## Supported targets

| Chip, board | Core, clock | Kernels | Checked on its board |
|---|---|---|---|
| RP2040, Raspberry Pi Pico | Cortex-M0+, 125 MHz | hard, soft, power-aware | yes |
| RP2350, Raspberry Pi Pico 2 | Cortex-M33, 150 MHz | hard, soft | since 2026-09-28 |
| STM32U585, Arduino UNO Q | Cortex-M33, 160 MHz | hard, soft | since 2026-09-26 |
| STM32U385, NUCLEO-U385RG-Q | Cortex-M33, 96 MHz | hard, soft, power-aware | since 2026-10-10 |

The bench checks each board on `main`, and on each pull request from a branch of this
repository since 2026-10-13 (`tools/board_ci.md`). All four also run under Renode in the
CI. Each is described below.

- **Raspberry Pi RP2040** (Pico): Cortex-M0+, port under `Escapement/CORTEX-Mx/RP2040/`.
  Its 64-bit timer has four alarms. It is clocked **independently of the core clock**.
  The kernel takes two of the alarms, and the timer events one of the other two. This is
  the target of the power-aware kernel, with the project's own DVFS driver
  (`power-aware.md`). The Cortex-M0+ has no stack limit. Since 2026-10-05, a region of
  its MPU, with no access, covers 1 KB that the linker script keeps between the globals
  and the stack. An overflow ends in the HardFault handler (`StackGuardPico`).
- **Raspberry Pi RP2350** (Pico 2): Cortex-M33, port under
  `Escapement/CORTEX-Mx/RP2350/`. It was transposed from the RP2040 port on 2026-09-24:
  the clocks at 150 MHz, TIMER0 with its tick from the TICKS block, 52 interrupts, the
  pads released from their isolation, the UART, the timer events and the launch of core
  1. In the CI, the hard and the soft kernel build the examples under `pico2/`: those of
  the Pico that are not benches, plus `ThreeSlotCoresPico2`, `FIFOCoresPico2` and
  `LitmusPico2`. They run under Renode on a platform of our own (`emulation.md`). That
  covers the 2^30 wrap of the kernel clock, both slot buffers between the two cores and
  the queue between them. `IdlePico2`, `SleepPico2`, `StackGuardPico2`, `LposcPico2`,
  `DormantPico2` and the three variants of the latter (`DormantCalAsleepPico2`,
  `AwakeCalAsleepPico2`, `RoscCalAsleepPico2`) are for the board only. Renode shows that
  the examples it runs schedule, not that the clocks are programmed right. A Pico 2
  showed the clocks right on 2026-09-28, and the bench checks them. Its clocks, the
  periods on a frequency counter, the UART and the 2^30 wrap on the board are in
  `rp2040.md`, "The Pico 2 on the board". The power-aware kernel is not ported.
- **STM32U5** (the STM32U585 of the Arduino UNO Q): Cortex-M33, port under
  `Escapement/CORTEX-Mx/STM32U5/`. It was written anew on 2026-09-25, after the model of
  the RP2350 port. The clocks run at 160 MHz from the board's 16 MHz crystal. TIM2
  serves the kernel, and TIM5 the timer events. USART1 goes to the connector, and
  LPUART1 to the board's Linux. An idle task can sleep in Stop 2, woken by LPTIM1. The
  hard and the soft kernel build the images under `uno-q/`. Two of them are for the wrap
  under Renode: `SleepWrapU5`, which is `SleepU5` with its times scaled, and
  `Stop2EventWrapU5`. The images run from SRAM and leave Arduino's firmware in the
  flash. All but `StackGuardU5` run under Renode on a platform of our own. The board has
  run `TaskLEDU5` and the endurance test since 2026-09-26, the latter for hours
  (`stm32u5.md`). The same sources build `SleepU5` for a NUCLEO-U575ZI-Q, to measure the
  MCU's current. They also build the endurance test, which has run on that board since
  2026-09-28 (`tools/board_ci.md`). `SleepU5` ran there on 2026-10-02 (`stm32u5.md`).
  The power-aware kernel is not ported (`power-aware.md`).
- **STM32U3** (the STM32U385 of a NUCLEO-U385RG-Q): Cortex-M33, port under
  `Escapement/CORTEX-Mx/STM32U3/`, written on 2026-10-06 from the U5's and the reference
  manual. It has been on the board since 2026-10-09 (`stm32u3.md`). The MSIS runs at
  96 MHz, locked on the 32.768 kHz crystal. There is no PLL. TIM2 serves the kernel, and
  TIM4 the timer events. USART1 goes to the ST-LINK's virtual COM port, and LPUART1 to
  D0 and D1. An idle task can sleep in Stop 2, woken by LPTIM1. All three kernels build
  its images under `nucleo-u385/`. The power-aware one has had DVFS between 48 and
  96 MHz since 2026-10-11, its timers then counting the MSIK. The images run from SRAM,
  or from the flash with `FLASH=1`, as the long run's does. All but `ClockU3`,
  `StackGuardU3`, `BenchDVFSU3` and `DVFSU3Flash` run under Renode, on a platform of our
  own that checks the clock set-up against the manual.
- **ARM Cortex-M33** (ARMv8-M Mainline): the generic layer takes it down the
  Cortex-M3/M4 path, under `CORTEX_M33`. The registers to save are the same. The frame
  with the floating-point unit left off is the same too, as are `LDREX`/`STREX`/`CLREX`.
  It is built without the floating-point unit (`-mcpu=cortex-m33+nofp`). Since
  2026-10-04, the port sets MSPLIM to the end of the globals. A stack that grows into
  them then faults instead of overwriting them, as a Pico 2 and the UNO Q showed
  (`StackGuardPico2`, `StackGuardU5`, roadmap, "A stack that faults"). The errata of the
  core itself were read on 2026-09-26. Arm's notice (SDEN-756493, v9.0, April 2018)
  leaves only 1080541 open in r0p4, the STM32U585's core. That one concerns the MPU,
  which the port does not use. The errata of the context switch, 851802, 937163 and
  1015127 among them, are fixed by r0p4. The RP2350's core is r1p0. That version of the
  notice predates it. The current one, which would cover it, is not publicly served. The
  RP2350 datasheet lists no erratum of the core.
- **ARM Cortex-M0 / M3 / M4**: the generic layer under `Escapement/CORTEX-Mx/`. The
  Cortex-M33 takes its Cortex-M3/M4 path. The STM32 ports it served were removed one
  after the other:
  - the F0, F1 and F2 on 2026-09-20. Their examples had only ever been compiled;
  - the L1 on 2026-09-22, with its DVFS driver, once the Pico ran every test it ran;
  - the F4 on 2026-09-26. It had been kept for the Cortex-M3/M4 path, which the RP2350
    and the STM32U5 now take. The STM32U5 is checked on its board by the bench.

Escapement began life on the TI MSP430. That port was removed on 2026-09-20
(`roadmap.md`). The history keeps it, and so does the archived `beber007/zottaos`.

## Source tree

```
Escapement/
  EscapementHard.{c,h}      hard real-time kernel
  EscapementSoft.{c,h}      (m,k)-firm real-time kernel
  EscapementHardPA.{c,h}    power-aware hard real-time kernel
  Escapement_Modes.h        names of the scheduling algorithms
  CORTEX-Mx/                ARM port: generic Cortex-M layer, RP2040, RP2350, STM32U5,
                            STM32U3
  CORTEX-Mx/RP2040/Examples/ the Raspberry Pi Pico examples
  CORTEX-Mx/RP2350/Examples/ the Raspberry Pi Pico 2 examples
  CORTEX-Mx/STM32U5/Examples/ the Arduino UNO Q examples, and SleepU5, SleepU5Flash
                            and SoakU5 for the NUCLEO-U575ZI-Q
  CORTEX-Mx/STM32U3/Examples/ the NUCLEO-U385RG-Q examples
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
