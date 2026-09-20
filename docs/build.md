# Building

Bare-metal ARM toolchain:

```sh
brew install arm-none-eabi-gcc         # macOS
sudo apt install gcc-arm-none-eabi     # Debian / Ubuntu
```

Each example is built from its own directory:

```sh
cd Escapement/CORTEX-Mx/STM32/Examples/stm32f0-discovery
make
```

All six are built on every push by the CI:

| Example | Core | MCU | Tasks | `text` / `data` / `bss` |
|---|---|---|---|---|
| `stm32f4-discovery` | Cortex-M4 | STM32F407VG | 3 | 4,012 / 477 / 32 |
| `stm32l-discovery` | Cortex-M3 | STM32L152RB | 4 | 4,124 / 5 / 212 |
| `stm32l-discovery-pa` | Cortex-M3 | STM32L152RB | 1 (*power-aware*) | 4,638 / 7 / 224 |
| `RP2040/Examples/pico` | Cortex-M0+ | RP2040 | 4 | 5,148 / 8 / 156 |
| `stm32vl-discovery` | Cortex-M3 | STM32F103RC | 3 | 5,184 / 5 / 272 |
| `stm32f0-discovery` | Cortex-M0 | STM32F051R8 | 4 | 6,172 / 5 / 160 |

Bytes of the `TaskLED` target, that is the whole kernel plus the periodic tasks
of the example. Everything is built at `-O2`.

The toolchain prefix can be overridden:
`make CROSS_COMPILE=/path/to/arm-none-eabi-`.

The code is compiled freestanding and linked without a C library
(`-nostdlib`), with only `libgcc` for the routines the hardware does not
provide (integer division on Cortex-M0 and M3). No newlib is needed.

> **Only the RP2040 port has been run on hardware** (see `rp2040.md`). For the
> five STM32 examples, the CI proves that they build and that each
> configuration agrees with its linker script; their execution is verified
> under Renode only. The MCU of `stm32vl-discovery` is in fact a deduction: its
> `Escapement_Config.h` has named an STM32L152 since 2012, while the only
> linker script shipped targets an STM32F103RC.

Flashing is done through OpenOCD (`openocd.cfg` is provided under
`Escapement/CORTEX-Mx/STM32/Examples/`).
