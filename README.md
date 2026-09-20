# Escapement

[![build](https://github.com/beber007/escapement/actions/workflows/build.yml/badge.svg)](https://github.com/beber007/escapement/actions/workflows/build.yml)

**A preemptive deadline-driven real-time kernel for microcontrollers with a few
kilobytes of RAM.**

Escapement schedules by earliest deadline first (EDF) and has **no periodic
tick**: it arms a comparator on the next deadline and wakes the processor for
that instant alone. Its *power-aware* variant lowers the core voltage and
frequency according to the actual load, using the worst-case execution times
declared by the tasks — without ever missing a deadline.

> The name comes from the watchmaking escapement: the part that releases the
> energy of the mainspring in regular increments — precisely what a frugal
> real-time scheduler does.

## In three figures

| | |
|---|---|
| **5,196 bytes** | the whole kernel and four periodic tasks, on a Cortex-M0+ |
| **3.2 µs** | cost of one scheduling round, measured on the board — 0.43 % of the processor at 1,340 activations per second |
| **+28 ppm** | deviation of the periods read by an external frequency counter: the tolerance of the crystal on the board, not that of the scheduler |

## Verified at four levels

Each level is independent of the one before it, and the last one trusts no
software from this repository.

| Level | Means | What it establishes |
|---|---|---|
| Compilation | GitHub Actions | six examples, four MCU families, on every push |
| Replayable execution | Renode and `renode-test` | the three tasks scheduled at their periods, as a regression test |
| Internal state on hardware | OpenOCD and SWD on a Pico | deadlines armed ahead of the counter, cost counters read back from SRAM |
| Independent instrument | frequency counter of a Bus Pirate v4 | periods measured outside the kernel, outside the emulator and outside the debugger |

That last level reads 500.02 Hz, 50.0014 Hz and 16.66713 Hz for declared periods
of 1, 20 and 60 ms. Details in [`docs/rp2040.md`](docs/rp2040.md).

![Chronogram of three periodic tasks scheduled by Escapement](docs/images/f4-schedule.svg)

Every edge above was captured under emulation, with the virtual timestamps of the
emulator, and the figure is regenerated from that data by a script in `tools/`
— see [`docs/emulation.md`](docs/emulation.md).

## Watch it run

Without any hardware, using [Renode](https://renode.io):

```sh
cd Escapement/CORTEX-Mx/STM32/Examples/stm32f4-discovery && make bin && cd -
renode emulation/renode/escapement_f4.resc
```

On a Raspberry Pi Pico, loaded into SRAM over SWD — no BOOTSEL button involved:

```sh
cd Escapement/CORTEX-Mx/RP2040/Examples/pico && make
openocd -f interface/cmsis-dap.cfg -c 'adapter speed 5000' -f target/rp2040.cfg \
        -c 'init; reset halt; load_image build/TaskLEDPico.elf; resume 0x20000000; exit'
```

## What sets this kernel apart

**EDF without a tick.** Mainstream real-time kernels schedule at fixed
priorities, paced by a periodic tick. Here tasks declare a period and a deadline,
the scheduler elects the one whose deadline is nearest, and the hardware
interrupts the processor at that moment only.

**Energy management driven by the scheduler.** Tasks declare their worst-case
execution time; the kernel uses it to know *by how much* it may slow the core
down without endangering a deadline — which is what the DRA, OTE and DM_SLACK
algorithms compute. Whether that lever actually saves energy on an STM32 is
examined, without indulgence, in [`docs/power-aware.md`](docs/power-aware.md).

**A time base independent of the core, on the RP2040.** Its counter is fed by a
one-microsecond tick derived from the reference clock: changing the processor
frequency does not move the time base of the kernel, which makes this chip a
good target for the power-aware variant.

## How this project is built

This project is developed with the help of an AI, under one standing rule:
**the AI proposes, the instrument decides.** The four levels of verification
above exist for that reason.

The documentation therefore keeps a record of the hypotheses that turned out to
be wrong, rather than showing only the outcome: an emulator model accused
without evidence before a two-register reproducer settled the matter, a hardware
cause wrongly dismissed because the binary under test was stale, an instrument
wrongly suspected when the defect was in the firmware. The full account is in
[`docs/method.md`](docs/method.md).

## Documentation

| | |
|---|---|
| [`docs/architecture.md`](docs/architecture.md) | kernel variants, targets, source tree |
| [`docs/build.md`](docs/build.md) | toolchain, examples, memory footprint |
| [`docs/api.md`](docs/api.md) | writing an application in five steps |
| [`docs/emulation.md`](docs/emulation.md) | Renode, tests replayed in CI, fixes to the timer model |
| [`docs/power-aware.md`](docs/power-aware.md) | DVFS, energy analysis, choosing a target |
| [`docs/rp2040.md`](docs/rp2040.md) | Raspberry Pi Pico port and hardware measurements |
| [`docs/method.md`](docs/method.md) | verifying AI-assisted development |
| [`docs/roadmap.md`](docs/roadmap.md) | current state and open work |

## Origin and licence

Escapement continues **ZottaOS**, a real-time kernel developed at HEIG-VD whose
development stopped in 2016. The code has been rebranded and taken over as a
personal project; `LICENSE` and `NOTICE` keep the original copyright and set out
the lineage as well as the third-party components.
