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
| `stm32f4-discovery` | Cortex-M4 | STM32F407VG | 3 | 5,940 / 480 / 32 |
| `stm32l-discovery` | Cortex-M3 | STM32L152RB | 4 | 6,116 / 8 / 212 |
| `stm32l-discovery-pa` | Cortex-M3 | STM32L152RB | 1 (*power-aware*) | 7,018 / 10 / 224 |
| `RP2040/Examples/pico` | Cortex-M0+ | RP2040 | 4 | 6,908 / 8 / 180 |
| `stm32vl-discovery` | Cortex-M3 | STM32F103RC | 3 | 7,796 / 8 / 272 |
| `stm32f0-discovery` | Cortex-M0 | STM32F051R8 | 4 | 8,888 / 8 / 160 |

Bytes of the `TaskLED` target, that is the whole kernel plus the periodic tasks
of the example.

> **Everything is built without optimisation.** No `Makefile` carries a `-O`
> flag: the sizes above, and the costs measured elsewhere in this
> documentation, are `-O0` figures. The port builds cleanly at `-O2`, where the
> Pico example drops to 5,248 bytes — 24 % less. Moving to `-O2` requires
> redoing the hardware measurements before publishing the resulting figures,
> and has therefore not been done yet.

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
