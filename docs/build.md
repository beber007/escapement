# Building

Every example builds with a bare-metal ARM toolchain and `make`, from its own directory.
This page lists the build options, the memory footprint and how an image reaches a board.

## Toolchain

```sh
brew install arm-none-eabi-gcc         # macOS
sudo apt install gcc-arm-none-eabi     # Debian / Ubuntu
```

The toolchain prefix can be overridden:
`make CROSS_COMPILE=/path/to/arm-none-eabi-`.

The code is compiled freestanding and linked without a C library (`-nostdlib`). Only
`libgcc` is linked, for the routines the hardware does not provide, such as integer
division on the Cortex-M0+, which has no divide instruction. No newlib is needed.
Everything is built at `-O2`. The kernel did not survive that level until the
exception-pending barriers went in (`method.md`).

## Examples and footprint

```sh
cd Escapement/CORTEX-Mx/RP2040/Examples/pico
make
```

The CI builds these three example directories on each push to `main` and each pull
request; `STM32U5/Examples/nucleo-u575` is built by its job for the board's images
(`tools/board_images.sh`). Sizes with GCC 16.2 at commit bf447b8, on 2026-09-27:

| Example | Core | MCU | Tasks | `text` / `data` / `bss` |
|---|---|---|---|---|
| `RP2040/Examples/pico` | Cortex-M0+ | RP2040 | 4 | 4,716 / 8 / 240 |
| `RP2350/Examples/pico2` | Cortex-M33 | RP2350 | 4 | 4,324 / 8 / 344 |
| `STM32U5/Examples/uno-q` | Cortex-M33 | STM32U585 | 4 | 5,656 / 8 / 648 |

The figures are the bytes of the `TaskLED` target with the hard kernel, that is the
whole kernel plus the periodic tasks of the example. Rebuilt on 2026-09-27 with GCC 16.2,
each at the commit that published it, the earlier figures come out byte for byte:

| Published | Commit | Pico | Pico 2 | STM32U5 |
|---|---|---|---|---|
| 2026-09-22 | 4dede39 | 5,156 / 8 / 240 | | |
| 2026-09-25 | 6f90fa2 | 5,164 / 8 / 240 | 4,288 / 8 / 344 | |
| 2026-09-26 | ff4fd7b | | | 5,596 / 8 / 640 |
| 2026-09-27 | bf447b8 | 4,716 / 8 / 240 | 4,324 / 8 / 344 | 5,656 / 8 / 648 |

## Build options

The hard kernel under EDF is the default everywhere. The other kernels and algorithms
are chosen on the command line (`architecture.md`):

| Option | Directories | Effect |
|---|---|---|
| `SCHEDULER=DEADLINE_MONOTONIC_SCHEDULING` | all three | deadline-monotonic scheduling |
| `KERNEL=SOFT` | all three | the (m,k)-firm kernel |
| `KERNEL=PA` | `pico` | the power-aware kernel |
| `POWER=DRA`, `DR_OTE`, `DM_SLACK` | `pico`, with `KERNEL=PA` | another power-management policy than OTE |
| `UNDERVOLT=1` | `pico`, with `KERNEL=PA` | below the specified core voltage, bench only |
| `SLEEP_SPEED=0` | `pico`, with `KERNEL=PA` | the idle task sleeps at 12 MHz rather than 125 |
| `TRACE=1` | all three | a scheduling trace in RAM, not built by the CI |
| `PHASES=30` | `uno-q`, `nucleo-u575` | `SleepU5` alternates 30 s in Stop 2 and 30 s in Sleep |
| `SMPS=1` | `nucleo-u575` | the STM32U575's SMPS rather than its LDO |

Other options, for the bench alone (`SLEEP_GATE`, `NOHSE`, `MHZ` and more), are
commented in each Makefile.

`make KERNEL=PA bench` builds the two benches of the Pico, `BenchDVFSPico` and
`BenchVregPico`. `make bin` adds raw binaries of the examples.

`STM32U5/Examples/nucleo-u575` builds `SleepU5` and `SoakU5`, from the sources of
`uno-q`, for a NUCLEO-U575ZI-Q: the first for measuring the MCU's current
(`stm32u5.md`), the second for a long endurance run, its reports on the ST-LINK's
virtual COM port (`tools/board_ci.md`).

## On hardware and under emulation

> **All three ports have run on hardware**, and a bench checks each at every commit
> (`tools/board_ci.md`). The core of the RP2040 below its specified voltage has been
> seen only by the regulator bench (`rp2040.md`). Every example runs under Renode in
> the CI except those made for a board: `FourSlotCoresPico` and the two benches of the
> Pico, `IdlePico2`, `SleepPico2` and `StackGuardPico2`, and `StackGuardU5`
> (`architecture.md`).

The images run from SRAM, loaded over SWD with OpenOCD: those of the Pico and the
Pico 2 by a Debug Probe (`rp2040.md`), those of the STM32U5 by `tools/unoq_load.sh` and
the OpenOCD of the UNO Q, which drives the MCU's SWD from the board's Linux processor
(`stm32u5.md`). Only `SleepU5Flash` is written to a flash, the NUCLEO's, to measure the
current without a debugger attached.

## Host tests and models

The kernels also build for the host, and the lock-free mechanisms have exhaustive models
in Python. Neither needs the ARM toolchain:

```sh
make -C test/host run                # every kernel and algorithm, see test/host/README.md
python3 tools/coverage.py            # the lines and branches it runs (clang)
python3 tools/differential.py        # random task sets, each trace checked
python3 test/model/fourslot.py       # and threeslot.py, fifo.py, fifo_mp.py
```
