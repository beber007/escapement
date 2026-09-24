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
Cortex-M3 and M4 and an emulation on the Cortex-M0+, where a flag stands for the
reservation and every context switch and every interrupt clears it on its way out
(`Escapement_Atomic.c`, `_OSIOHandler`, the end of `_OSContextSwapHandler`). The
application gets the same guarantees from two mechanisms:

- **FIFO queues** (`OSInitFIFOQueue`), shared by any number of tasks and interrupt
  handlers, with buffers allocated once. They build on the array-based LL/SC queue of
  Evéquoz (ICPP 2008), with one operation announced at a time and completed by any
  caller that preempts it, so that every operation ends in a bounded number of steps.
  `test/model/fifo.py` explores every run of a few operations, each of which may preempt
  the one running at any access, from full queues and indices about to wrap, and checks
  that every run is linearizable — that some order of its operations gives them their
  results from a plain queue. It found that a dequeue signalling an event left its
  descriptor unmarked once the signal was in place: under two nested preemptions, a
  late helper put a second signal back after an enqueue had taken the first, and one
  event woke two tasks. The dequeue now marks itself done there.
- **Slot buffers** (`OSInitBuffer`), from one writer to one reader, neither waiting for
  the other: four slots after Simpson (1990), without atomic instructions, or three
  after Chen and Burns (1997), with an LL/SC pair. `test/model/fourslot.py` and
  `threeslot.py` explore every interleaving of the writer, an interrupt handler, with
  its reader — the first also with the writer on a core of its own — the LL/SC pair emulated as on the Cortex-M0+, and check that no read mixes
  two records and none goes backwards — the properties Rushby model-checked for
  Simpson's algorithm; the CI runs both, and the FIFO model. The second found that a store-conditional,
  unlike the compare-and-swap of Chen and Burns, fails when an interrupt merely came
  between it and its load-linked, which left the reader on a slot that does not exist;
  the reader now tries again.

All of this assumes **one processor**. The emulated LL/SC and the announced operation of
the queue both rely on preemptions nesting, which two cores do not give; the kernel runs
on core 0 of the RP2040 alone. Of the three mechanisms, only Simpson's works between two
cores as it stands: `fourslot.py` checks it on two cores as well, and on the Pico it
carried 320,000 reads from core 1 to a task on core 0 without a torn one, where a plain
array tore one in twenty (`rp2040.md`, `FourSlotCoresPico`). The references are listed
in the README, and the roadmap keeps the other two for the Pico 2.

## Supported targets

- **ARM Cortex-M0 / M3 / M4** — port under `Escapement/CORTEX-Mx/`. The ST
  libraries are bundled for the STM32F4 only, the one family the examples use.
  Support for the F0, F1 and F2 families was removed on 2026-09-20: those
  examples were only ever compiled, and this is a demonstration of what the
  kernel does, not a catalogue of the parts it could run on. The L1 followed on
  2026-09-22, with its DVFS driver: the Pico had come to run every test it ran,
  and the F4 is kept for the Cortex-M3/M4 path of the context switch, which only
  it executes. The port still carries branches for the families removed; none is
  built.
- **ARM Cortex-M33** (ARMv8-M Mainline), toward the RP2350 of the Pico 2: the generic
  layer takes it down the Cortex-M3/M4 path — the same registers to save, the same
  frame with the floating-point unit left off, `LDREX`/`STREX`/`CLREX` — under
  `CORTEX_M33`. Built without the floating-point unit (`-mcpu=cortex-m33+nofp`).
- **Raspberry Pi RP2350** (Pico 2) — Cortex-M33, port under
  `Escapement/CORTEX-Mx/RP2350/`, transposed from the RP2040 port on 2026-09-24: the
  clocks at 150 MHz, TIMER0 with its tick from the TICKS block, 52 interrupts, the pads
  released from their isolation, the UART, the timer events and the launch of core 1.
  The hard and the soft kernel build the four examples of the Pico under `pico2/` in the
  CI; the power-aware kernel is not ported, and nothing has run the port yet — no board
  and no emulator of the RP2350 at hand (`roadmap.md`).
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
  CORTEX-Mx/                ARM port: generic Cortex-M layer, STM32, RP2040
  CORTEX-Mx/STM32/Examples/ the STM32F4-Discovery example
  CORTEX-Mx/RP2040/Examples/ the Raspberry Pi Pico example
test/host/                  the three kernels built for the host
emulation/renode/           Renode platforms, models and Robot suites
tools/                      trace capture and figure generation
.github/workflows/build.yml builds both examples, runs them under Renode on
                            every kernel and algorithm, and the kernels on
                            the host
```
