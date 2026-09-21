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

All four are built on every push by the CI:

| Example | Core | MCU | Tasks | `text` / `data` / `bss` |
|---|---|---|---|---|
| `stm32f4-discovery` | Cortex-M4 | STM32F407VG | 3 | 3,920 / 484 / 36 |
| `stm32l-discovery` | Cortex-M3 | STM32L152RB | 4 | 4,005 / 12 / 216 |
| `stm32l-discovery-pa` | Cortex-M3 | STM32L152RB | 1 (*power-aware*) | 4,569 / 12 / 224 |
| `RP2040/Examples/pico` | Cortex-M0+ | RP2040 | 4 | 5,160 / 8 / 160 |

Bytes of the `TaskLED` target, that is the whole kernel plus the periodic tasks
of the example. Everything is built at `-O2`, which the kernel did not survive
until the exception-pending barriers went in — see `method.md`.

The toolchain prefix can be overridden:
`make CROSS_COMPILE=/path/to/arm-none-eabi-`.

The code is compiled freestanding and linked without a C library
(`-nostdlib`), with only `libgcc` for the routines the hardware does not
provide (integer division on Cortex-M0 and M3). No newlib is needed.

> **Only the RP2040 port has been run on hardware** (see `rp2040.md`). All the
> examples are executed under Renode in CI, the RP2040 ones included, except
> `stm32l-discovery`, which is only built — it exists because
> `stm32l-discovery-pa` borrows its linker script and its sources.

Flashing is done through OpenOCD (`openocd.cfg` is provided under
`Escapement/CORTEX-Mx/STM32/Examples/`).
