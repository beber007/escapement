# Architecture and targets

## Kernel variants

| Variant | Files | Use |
|---|---|---|
| **Hard** | `Escapement/EscapementHard.{c,h}` | Hard real time: deadlines are guaranteed |
| **Soft** | `Escapement/EscapementSoft.{c,h}` | Soft real time: aperiodic tasks tolerating overruns |
| **Hard PA** | `Escapement/EscapementHardPA.{c,h}` | Hard real time plus dynamic energy management (DVFS) |

## Scheduling algorithm

Two are implemented: earliest deadline first, where the task whose deadline is nearest
runs first, and deadline-monotonic, where priorities are fixed before start-up from the
declared deadlines. An application picks one in its `Escapement_Config.h`:

```c
#define SCHEDULER_REAL_TIME_MODE EARLIEST_DEADLINE_FIRST
```

Every example here selects earliest deadline first. `EscapementHard.h` falls back to
deadline-monotonic when an application says nothing — which is what all of them used to
do without meaning to, see `method.md`.

## Supported targets

- **ARM Cortex-M0 / M3 / M4** — port under `Escapement/CORTEX-Mx/`. The ST
  libraries are bundled for the STM32F4 and L1 families, the two the examples
  use. Support for the F0, F1 and F2 families was removed on 2026-09-20: those
  examples were only ever compiled, and this is a demonstration of what the
  kernel does, not a catalogue of the parts it could run on.
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
  EscapementSoft.{c,h}      soft real-time kernel
  EscapementHardPA.{c,h}    power-aware hard real-time kernel
  CORTEX-Mx/                ARM port (STM32, CMSIS, StdPeriph)
  CORTEX-Mx/STM32/Examples/ five examples with a Makefile
  CORTEX-Mx/RP2040/Examples/ Raspberry Pi Pico example
tools/                      trace capture and figure generation
.github/workflows/build.yml builds the six examples and runs them under Renode
```
