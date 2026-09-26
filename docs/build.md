# Building

Bare-metal ARM toolchain:

```sh
brew install arm-none-eabi-gcc         # macOS
sudo apt install gcc-arm-none-eabi     # Debian / Ubuntu
```

Each example is built from its own directory:

```sh
cd Escapement/CORTEX-Mx/RP2040/Examples/pico
make
```

The CI builds all three on every push:

| Example | Core | MCU | Tasks | `text` / `data` / `bss` |
|---|---|---|---|---|
| `RP2040/Examples/pico` | Cortex-M0+ | RP2040 | 4 | 5,164 / 8 / 240 |
| `RP2350/Examples/pico2` | Cortex-M33 | RP2350 | 4 | 4,288 / 8 / 344 |
| `STM32U5/Examples/uno-q` | Cortex-M33 | STM32U585 | 4 | 5,596 / 8 / 640 |

Bytes of the `TaskLED` target, hard kernel, that is the whole kernel plus the
periodic tasks of the example, measured on 2026-09-25, the STM32U5's on 2026-09-26; the
Pico's was 5,156 bytes of
`text` on 2026-09-22. Everything is built at `-O2`, which the kernel did not survive
until the exception-pending barriers went in — see `method.md`.

The toolchain prefix can be overridden:
`make CROSS_COMPILE=/path/to/arm-none-eabi-`.

The code is compiled freestanding and linked without a C library
(`-nostdlib`), with only `libgcc` for the routines the hardware does not
provide (integer division on the Cortex-M0+, which has no divide instruction). No
newlib is needed.

> **The RP2040 and STM32U5 ports have run on hardware.** The RP2040's (see `rp2040.md`):
> all three kernels, the DVFS driver and the timer events, and the 4-slot buffer between
> the cores; the wrap of the kernel clock has been seen under emulation alone, and the
> core below its specified voltage only by the regulator bench. The STM32U5's, on the
> Arduino UNO Q since 2026-09-26 (see `stm32u5.md`): the clock set-up, `TaskLEDU5` and the
> endurance test, run for hours. The Pico 2 port has run under Renode only. Every example runs under Renode in CI but three:
> `FourSlotCoresPico`, which only the board runs, and the two benches of the Pico.

On the Pico, `make KERNEL=SOFT`, `make KERNEL=PA` and `make SCHEDULER=...` build
the other kernels and algorithms; see `architecture.md`.

Flashing is done through OpenOCD. The Pico's images are loaded into SRAM over SWD
(`rp2040.md`); the STM32U5's too, by `tools/unoq_load.sh` and the OpenOCD of the UNO Q,
which drives the MCU's SWD from its Linux processor (`stm32u5.md`).
