# Building

Bare-metal ARM toolchain:

```sh
brew install arm-none-eabi-gcc         # macOS
sudo apt install gcc-arm-none-eabi     # Debian / Ubuntu
```

Each example is built from its own directory:

```sh
cd Escapement/CORTEX-Mx/STM32/Examples/stm32f4-discovery
make
```

The CI builds all three on every push:

| Example | Core | MCU | Tasks | `text` / `data` / `bss` |
|---|---|---|---|---|
| `stm32f4-discovery` | Cortex-M4 | STM32F407VG | 3 | 3,924 / 484 / 116 |
| `RP2040/Examples/pico` | Cortex-M0+ | RP2040 | 4 | 5,164 / 8 / 240 |
| `RP2350/Examples/pico2` | Cortex-M33 | RP2350 | 4 | 4,288 / 8 / 344 |

Bytes of the `TaskLED` target, hard kernel, that is the whole kernel plus the
periodic tasks of the example, measured on 2026-09-25; the Pico's was 5,156 bytes of
`text` on 2026-09-22. Everything is built at `-O2`, which the kernel did not survive
until the exception-pending barriers went in — see `method.md`.

The toolchain prefix can be overridden:
`make CROSS_COMPILE=/path/to/arm-none-eabi-`.

The code is compiled freestanding and linked without a C library
(`-nostdlib`), with only `libgcc` for the routines the hardware does not
provide (integer division on the Cortex-M0+, which has no divide instruction). No
newlib is needed.

> **Only the RP2040 port has been run on hardware** (see `rp2040.md`): all three
> kernels, the DVFS driver and the timer events, and the 4-slot buffer between the
> cores; the wrap of the kernel clock has been seen under emulation alone, and the
> core below its specified voltage only by the regulator bench. The Pico 2 port has
> run under Renode only. Every example runs under Renode in CI but three:
> `FourSlotCoresPico`, which only the board runs, and the two benches of the Pico.

On the Pico, `make KERNEL=SOFT`, `make KERNEL=PA` and `make SCHEDULER=...` build
the other kernels and algorithms; see `architecture.md`.

Flashing is done through OpenOCD (`openocd.cfg` is provided under
`Escapement/CORTEX-Mx/STM32/Examples/`).
