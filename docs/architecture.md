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

The Pico examples are written for the hard kernel only, and the power-aware one takes
`SCHEDULER` but not `KERNEL`.

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
- **Raspberry Pi RP2040** — Cortex-M0+, port under
  `Escapement/CORTEX-Mx/RP2040/`. A 64-bit timer with four alarms, clocked
  **independently of the core clock**.
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
  CORTEX-Mx/                ARM port (STM32, CMSIS)
  CORTEX-Mx/STM32/Examples/ five examples with a Makefile
  CORTEX-Mx/RP2040/Examples/ Raspberry Pi Pico example
tools/                      trace capture and figure generation
.github/workflows/build.yml builds the four examples, runs three under Renode
                            and the scheduler on the host
```
