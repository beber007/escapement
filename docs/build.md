# Building

Every example builds with a bare-metal ARM toolchain and `make`, from its own directory.
This page lists the build options, the memory footprint and how an image reaches a
board.

## Toolchain

```sh
brew install arm-none-eabi-gcc         # macOS
sudo apt install gcc-arm-none-eabi     # Debian / Ubuntu
```

The toolchain prefix can be overridden:
`make CROSS_COMPILE=/path/to/arm-none-eabi-`.

The code is compiled freestanding and linked without a C library (`-nostdlib`). Only
`libgcc` is linked. It supplies the routines that the hardware lacks, such as integer
division on the Cortex-M0+, which has no divide instruction. No newlib is needed.
Everything is built at `-O2`. The kernel did not survive that level until the
exception-pending barriers were added (`method.md`).

## Examples and footprint

```sh
cd Escapement/CORTEX-Mx/RP2040/Examples/pico
make
```

The CI builds the four example directories (with `STM32U3/Examples/nucleo-u385`) on each
push to `main` and each pull request. `STM32U5/Examples/nucleo-u575` is built by its job
for the board's images (`tools/board_images.sh`). The sizes below are with GCC 16.2 at
commit 2ade62a, on 2026-10-26:

| Example | Core | MCU | Tasks | `text` / `data` / `bss` |
|---|---|---|---|---|
| `RP2040/Examples/pico` | Cortex-M0+ | RP2040 | 4 | 5,248 / 8 / 1,912 |
| `RP2350/Examples/pico2` | Cortex-M33 | RP2350 | 4 | 4,824 / 8 / 380 |
| `STM32U5/Examples/uno-q` | Cortex-M33 | STM32U585 | 4 | 6,304 / 8 / 668 |
| `STM32U3/Examples/nucleo-u385` | Cortex-M33 | STM32U385 | 4 | 5,916 / 8 / 660 |

The figures are the bytes of the `TaskLED` target with the hard kernel: the whole kernel
plus the periodic tasks of the example. On 2026-09-27, the earlier figures were rebuilt
with GCC 16.2, each at the commit that published it. They match byte for byte:

| Published | Commit | Pico | Pico 2 | STM32U5 |
|---|---|---|---|---|
| 2026-09-22 | 4dede39 | 5,156 / 8 / 240 | | |
| 2026-09-25 | 6f90fa2 | 5,164 / 8 / 240 | 4,288 / 8 / 344 | |
| 2026-09-26 | ff4fd7b | | | 5,596 / 8 / 640 |
| 2026-09-27 | bf447b8 | 4,716 / 8 / 240 | 4,324 / 8 / 344 | 5,656 / 8 / 648 |

Rebuilt on 2026-10-26, bf447b8 gives the same figures on the three boards. Since
bf447b8, the Pico's `bss` has grown from 240 to 1,912 bytes. Of that growth, 1,628 bytes
are the `.stack_guard` section, added on 2026-10-05. That section is the 1 KB that the
MPU guards below the stack (`api.md`, "Memory"), aligned on its size (`RP2040_SRAM.ld`).

## Build options

The hard kernel under EDF is the default everywhere. The other kernels and algorithms
are chosen on the command line (`architecture.md`):

| Option | Directories | Effect |
|---|---|---|
| `SCHEDULER=DEADLINE_MONOTONIC_SCHEDULING` | all four | deadline-monotonic scheduling |
| `KERNEL=SOFT` | all four | the (m,k)-firm kernel |
| `KERNEL=PA` | `pico`, `nucleo-u385` | the power-aware kernel |
| `POWER=DRA`, `DR_OTE`, `DM_SLACK` | `pico`, `nucleo-u385`, with `KERNEL=PA` | another power-management policy than OTE |
| `UNDERVOLT=1` | `pico`, with `KERNEL=PA` | below the specified core voltage, bench only |
| `SLEEP_SPEED=0` | `pico`, `nucleo-u385`, with `KERNEL=PA` | the idle task's WFI at the slowest point, 12 MHz on the Pico, 48 on the U3 |
| `MHZ=` | `nucleo-u575`, `nucleo-u385` | another system clock (80, 40 or 16 MHz on the U575 rather than 160, 48, 24 or 12 on the U385 rather than 96), not with `KERNEL=PA` |
| `FLASH=1` | all four | the images linked into the flash, in `build-flash/` |
| `TRACE=1` | all four | a scheduling trace in RAM, not built by the CI |
| `PHASES=30` | `uno-q`, `nucleo-u575`, `nucleo-u385` | `SleepU5` or `SleepU3` alternates 30 s in Stop 2 and 30 s in Sleep |
| `FAST=1` | `nucleo-u385` | the MSI's PLL mode kept through Stop 2 (MSIPLL0FAST) |
| `SMPS=1` | `nucleo-u575` | the STM32U575's SMPS rather than its LDO |

Each Makefile comments other options, for the bench alone (`SLEEP_GATE`, `NOHSE`, `MHZ`
and more).

`make KERNEL=PA bench` builds the two benches of the Pico, `BenchDVFSPico` and
`BenchVregPico`. `make bin` adds raw binaries of the examples.

`STM32U5/Examples/nucleo-u575` builds `SleepU5` and `SoakU5` for a NUCLEO-U575ZI-Q, from
the sources of `uno-q`. `SleepU5` is for measuring the MCU's current (`stm32u5.md`).
`SoakU5` is for a long endurance run, and it reports on the ST-LINK's virtual COM port
(`tools/board_ci.md`). With `KERNEL=SOFT`, the directory also builds `SoakFirmU5`.

Built for the soft kernel, each port adds `SoakFirm` to its endurance test
(`SoakFirmPico`, `SoakFirmPico2`, `SoakFirmU5`, `SoakFirmU3`). `SoakFirm` is the same
test with an (m,k)-firm task, under a load that makes the kernel drop some of its
optional instances. None of its mandatory instances may be missed, and no instance may
end late (`SoakU5.c`).

## On hardware and under emulation

> **All four ports have run on hardware**, and a bench checks each port at every commit
> (`tools/board_ci.md`). Only the regulator bench has seen the core of the RP2040 below
> its specified voltage (`rp2040.md`). Every example runs under Renode in the CI, except
> those made for a board:
>
> - `FourSlotCoresPico`, `SleepPico` and the four benches of the Pico;
> - `IdlePico2`, `SleepPico2`, `DormantPico2`, `LposcPico2` and `StackGuardPico2`;
> - `StackGuardU5`;
> - `ClockU3`, `StackGuardU3`, `BenchDVFSU3` and `DVFSU3Flash` (`DVFSU3Short` and
>   `BenchDVFSU3Short` run in their place).

The images run from SRAM, loaded over SWD with OpenOCD. A Debug Probe loads the images
of the Pico and the Pico 2 (`rp2040.md`). `tools/unoq_load.sh` and the OpenOCD of the
UNO Q load the images of the STM32U5. That OpenOCD drives the MCU's SWD from the board's
Linux processor (`stm32u5.md`). `make FLASH=1` links the images into the flash on every
port. The NUCLEO-U385RG-Q has run its long run from there since 2026-10-10. The
NUCLEO-U575ZI-Q runs its endurance tests from there. `SleepU5Flash`, `SleepU3Flash` and
`DVFSU3Flash` run from there to measure the current without a debugger attached.

## Host tests and models

The kernels also build for the host, and the lock-free mechanisms have exhaustive models
in Python. Neither needs the ARM toolchain:

```sh
make -C test/host run                # every kernel and algorithm, see test/host/README.md
python3 tools/coverage.py            # the lines and branches it runs (clang)
python3 tools/differential.py        # random task sets, each trace checked
python3 test/model/fourslot.py       # and threeslot.py, fifo.py, fifo_mp.py
```
