# Architecture and targets

## Kernel variants

| Variant | Files | Use |
|---|---|---|
| **Hard** | `Escapement/EscapementHard.{c,h}` | Hard real time: deadlines are guaranteed |
| **Soft** | `Escapement/EscapementSoft.{c,h}` | Soft real time: aperiodic tasks tolerating overruns |
| **Hard PA** | `Escapement/EscapementHardPA.{c,h}` | Hard real time plus dynamic energy management (DVFS) |

## Supported targets

- **ARM Cortex-M0 / M3 / M4** — port under `Escapement/CORTEX-Mx/`, the
  *development target*. The ST libraries are bundled for the STM32F0, F1, F2,
  F4 and L1 families; five STM32 examples are built in CI (see `build.md`),
  the F2 has none.
- **Raspberry Pi RP2040** — Cortex-M0+, port under
  `Escapement/CORTEX-Mx/RP2040/`. A 64-bit timer with four alarms, clocked
  **independently of the core clock**.
- **TI MSP430** — MSP430x1xx through x5xx, MSP430FR57xx, CC430. Port under
  `Escapement/msp430/`. *Frozen*, see `roadmap.md`. **There is no build for
  this target**: the original projects were IAR / Code Composer projects, and
  they are not in the repository.

## Source tree

```
Escapement/
  EscapementHard.{c,h}      hard real-time kernel
  EscapementSoft.{c,h}      soft real-time kernel
  EscapementHardPA.{c,h}    power-aware hard real-time kernel
  CORTEX-Mx/                ARM port (STM32, CMSIS, StdPeriph)
  msp430/                   MSP430 port
  CORTEX-Mx/STM32/Examples/ five examples with a Makefile
  CORTEX-Mx/RP2040/Examples/ Raspberry Pi Pico example
PA/                         power-aware example (MSP430F5419A)
USB/                        example with a USB stack (MSP430x552x)
Balls/                      graphical demo (MSP-EXP430F5438)
.github/workflows/build.yml builds the six examples and runs them under Renode
```
