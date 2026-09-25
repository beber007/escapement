# Architecture and targets

## Kernel variants

| Variant | Files | Use |
|---|---|---|
| **Hard** | `Escapement/EscapementHard.{c,h}` | Hard real time: deadlines are guaranteed |
| **Soft** | `Escapement/EscapementSoft.{c,h}` | (m,k)-firm real time: out of every k instances of a task, m are guaranteed and the others run only if they can finish in time |
| **Hard PA** | `Escapement/EscapementHardPA.{c,h}` | Hard real time plus dynamic energy management (DVFS) |

## Scheduling algorithm

Two are implemented: earliest deadline first, where the task whose deadline is nearest
runs first, and deadline-monotonic, where priorities are fixed before start-up from the
declared deadlines. An application picks one in its `Escapement_Config.h`:

```c
#define SCHEDULER_REAL_TIME_MODE EARLIEST_DEADLINE_FIRST
```

Every example here selects earliest deadline first. The kernel headers fall back to
deadline-monotonic when an application says nothing — which is what all of them used to
do without meaning to, see `method.md`. The power-aware kernel adds EDF*, a deterministic
EDF, and its DRA, DR_OTE and DM_SLACK algorithms impose their scheduling whatever the
application chose; OTE, the one the example uses, works with any.

The names of the algorithms live in `Escapement/Escapement_Modes.h`, which every file that
tests the choice includes first. The examples build in every combination without editing
a file, and the CI runs them all:

```sh
make                                           # hard kernel, earliest deadline first
make SCHEDULER=DEADLINE_MONOTONIC_SCHEDULING   # hard kernel, deadline-monotonic
make KERNEL=SOFT                               # soft kernel, earliest deadline first
make KERNEL=SOFT SCHEDULER=DEADLINE_MONOTONIC_SCHEDULING
```

On the Pico, `make KERNEL=PA` builds the power-aware kernel as well, and
`make KERNEL=PA UNDERVOLT=1` the same below the specified core voltage, for the
measurement bench only (`power-aware.md`). The STM32 port no longer provides the
power-aware kernel.

## Synchronisation

The kernels mask interrupts only at start-up and in the traps of `DEBUG_MODE`: their
queues are updated with load-linked / store-conditional pairs, `LDREX`/`STREX` on the
Cortex-M3, M4 and M33 and an emulation on the Cortex-M0+, where a flag stands for the
reservation and every context switch and every interrupt clears it on its way out
(`Escapement_Atomic.c`, `_OSIOHandler`, the end of `_OSContextSwapHandler`). The
application gets the same guarantees from two mechanisms:

- **FIFO queues** (`OSInitFIFOQueue`), shared by any number of tasks and interrupt
  handlers, with buffers allocated once. They build on the array-based LL/SC queue of
  Evéquoz (ICPP 2008), with one operation announced at a time and completed by any
  caller that preempts it, so that every operation ends in a bounded number of steps.
- **Slot buffers** (`OSInitBuffer`), from one writer to one reader, neither waiting for
  the other: four slots after Simpson (1990), without atomic instructions, or three
  after Chen and Burns (1997), with an LL/SC pair. Between two cores, each buffer
  orders its accesses with four calls to `_OSMemoryBarrier()`: a `DMB` on the RP2040
  and the RP2350, a compiler barrier alone in the single-core STM32 port and the host
  build.

Each mechanism has an exhaustive model in `test/model`, run by the CI: every run of a
few queue operations preempting one another at any access, checked for linearizability;
every interleaving of a slot buffer's writer with its reader, the writer being an
interrupt handler or code on the other core, checked against the properties Rushby
model-checked for Simpson's algorithm — no read mixes two records, none goes backwards —
and, between two cores, with each core free to reorder its accesses. The models found
four defects, now fixed (`method.md`).

**Across the two cores.** The kernel runs on core 0 alone. The emulated LL/SC and the
announced operation of the queue both rely on preemptions nesting, which two cores do
not give, so between the cores only the slot buffers work. The 4-slot buffer works on
both chips, and was measured on the Pico (`rp2040.md`). On the RP2350 the 3-slot buffer
should work too: its model holds provided the exclusive monitors see both cores —
`ACTLR.EXTEXCLALL`, which the port sets on each — and `ThreeSlotCoresPico2` waits for a
board to show it. Code on
core 1 may not signal an event: `OSScheduleSuspendedTask` would pend the timer interrupt
of core 1, where no kernel runs. The queue across the cores is in the roadmap.

## Supported targets

- **ARM Cortex-M0 / M3 / M4** — port under `Escapement/CORTEX-Mx/`. The ST libraries are
  bundled for the STM32F4 only, the one family the examples use. Support for the F0, F1
  and F2 families was removed on 2026-09-20: those examples were only ever compiled, and
  this is a demonstration of what the kernel does, not a catalogue of the parts it could
  run on. The L1 followed on 2026-09-22, with its DVFS driver: the Pico had come to run
  every test it ran, and the F4 is kept for the Cortex-M3/M4 path of the context switch,
  which it alone executed until the RP2350 port took that path too. The port still
  carries branches for the families removed; none is built.
- **ARM Cortex-M33** (ARMv8-M Mainline), toward the RP2350 of the Pico 2: the generic
  layer takes it down the Cortex-M3/M4 path — the same registers to save, the same
  frame with the floating-point unit left off, `LDREX`/`STREX`/`CLREX` — under
  `CORTEX_M33`. Built without the floating-point unit (`-mcpu=cortex-m33+nofp`).
- **Raspberry Pi RP2350** (Pico 2) — Cortex-M33, port under
  `Escapement/CORTEX-Mx/RP2350/`, transposed from the RP2040 port on 2026-09-24: the
  clocks at 150 MHz, TIMER0 with its tick from the TICKS block, 52 interrupts, the pads
  released from their isolation, the UART, the timer events and the launch of core 1.
  The hard and the soft kernel build six examples under `pico2/` in the CI — the five of
  the Pico and `ThreeSlotCoresPico2` — and five of them run under Renode on a platform
  of our own (`emulation.md`), the 2^30 wrap of the kernel clock and the 4-slot buffer
  between the two cores included, which shows that they schedule, not that the clocks
  are programmed right. The power-aware kernel is not ported, and no board has run the
  port yet.
- **Raspberry Pi RP2040** — Cortex-M0+, port under
  `Escapement/CORTEX-Mx/RP2040/`. A 64-bit timer with four alarms, clocked
  **independently of the core clock**: the kernel takes two of them, the timer
  events one of the other two. The target of the power-aware kernel, with the
  DVFS driver of the project (`power-aware.md`).

Escapement began life on the TI MSP430, and that port was removed on
2026-09-20: see `roadmap.md`. The history keeps it, and so does the archived
`beber007/zottaos`.

## Source tree

```
Escapement/
  EscapementHard.{c,h}      hard real-time kernel
  EscapementSoft.{c,h}      (m,k)-firm real-time kernel
  Escapement_Modes.h        names of the scheduling algorithms
  EscapementHardPA.{c,h}    power-aware hard real-time kernel
  CORTEX-Mx/                ARM port: generic Cortex-M layer, STM32, RP2040, RP2350
  CORTEX-Mx/STM32/Examples/ the STM32F4-Discovery example
  CORTEX-Mx/RP2040/Examples/ the Raspberry Pi Pico examples
  CORTEX-Mx/RP2350/Examples/ the Raspberry Pi Pico 2 examples
test/host/                  the three kernels built for the host
test/model/                 exhaustive models of the lock-free mechanisms
emulation/renode/           Renode platforms, models and Robot suites
tools/                      trace capture, board checks and figure generation
.github/workflows/build.yml builds the examples, runs them under Renode on
                            every kernel and algorithm, the models, and the
                            kernels on the host
```
