# The power-aware variant and DVFS

The power-aware kernel, `EscapementHardPA`, lowers the frequency and the core voltage
when the execution times the tasks declare leave room before the next deadline. Its one
driver is the RP2040's, described below. The kernel also runs in the host test
(`test/host`). Whether it saves energy has not been measured: this page sets out what the
datasheets lead one to expect, what the bench will have to settle, and what emulation
can and cannot show.

## The STM32L1 driver, removed

The first DVFS driver of the project was written for the STM32L1. It had three steps, at
4, 16 and 32 MHz, set the core voltage through `PWR_CR`, and ran in the
`stm32l-discovery-pa` example. It was removed on 2026-09-22 with the L1 examples
(4dede39), once the Pico ran every test they ran. The history keeps it, along with
`IccMeasure.c`, the original authors' bench that stepped through the three ranges and
read the current of the STM32L-Discovery. That bench was never run, for want of a board.

Under Renode, the variant scheduled its three tasks at their periods and reprogrammed
the PLL eighteen times in half a second. The voltage stayed in its highest range the
whole time: the idle loop went back to full speed before every `WFI`, and a load of 90 %
left little room. The same work found a defect in the Renode L151 platform, which
declared TIM2 as a 32-bit counter where the L1 has a 16-bit one. The port's 16-bit timer
mode therefore never saw its overflow; a derived platform fixed it.

## Is DVFS worth anything?

This is an open question, to be settled before more goes into this variant.

**The physics.** For a fixed amount of work of *W* cycles, the dynamic energy is
`α·C·V²·f · W/f = α·C·V²·W`, which does not depend on the frequency. Lowering *f* alone
gains nothing. It even lengthens the active time, and with it the leakage energy. The
only lever is V², and V can only be lowered by lowering f.

**What the RP2040 offers.** Within its specification the core runs at 1.10 V at 125 MHz
and at 1.05 V below (see the driver further down). (1.05/1.10)² ≈ 0.91, so some 9 % less
dynamic energy per cycle in the core. Undervolted, as the bench may do outside the
specification, 0.90 V at 12 MHz gives (0.90/1.10)² ≈ 0.67, a third less.

**What tempers this.** Those figures are for the core alone, and computed, not measured.
The RP2040's core regulator is linear, so the board draws the core's current at the
supply voltage. That current per cycle falls with V, not V², and the 9 % in the core
becomes some 5 % at the supply. The V² law also covers switching only. The crystal, the
PLL kept running, the always-on blocks and the leakage do not scale with it. The
STM32L1, the first target, promised more on paper: three regulator ranges from 1.8 to
1.2 V, 2.25× in theory. The µA/MHz its datasheet was said to quote pointed to some 20 %
per cycle, a figure never checked here.

**The alternative is race-to-sleep.** Run at full speed, finish, then sleep as deeply as
the chip allows. On the STM32U585 of the UNO Q, Stop 2 with all SRAM kept draws 20.5 µA
by its datasheet, and 8.2 µA on the STM32U575 with its SMPS (`stm32u5.md`). The RP2040
has no such mode (see the table below). Since dynamic energy does not depend on f,
running fast costs nothing extra and shortens the time the fixed costs are paid for.
Where the chip sleeps well, race-to-sleep is expected to match DVFS or beat it, and it is
simpler. No measurement here says by how much.

DVFS can win only where sleeping is not an option:

- idle gaps shorter than the cost of waking up. On the U5, waking from Stop 2 took up to
  885 µs on the board on 2026-09-27, the clock rebuilt from the crystal (`stm32u5.md`),
  against the 1 ms period of the probe in `TaskLEDPico`;
- a latency constraint that forbids deep sleep;
- a peripheral that needs the core clock domain.

Whatever the energy gain, the kernel's part is the scheduling. Its four policies, OTE
(the default), DRA, DR_OTE and DM_SLACK, compute from the declared WCETs when the speed
can be lowered without a task missing its deadline (`EscapementHardPA.h`).

## Measuring the RP2040

The argument above gives DVFS a niche on the RP2040, because it sleeps poorly, and the
board is at hand. Measuring it needs a bench, described here so that it can be built.

**Where to measure.** On a Pico, 5 V from `VSYS` goes through a buck-boost converter
before it reaches the 3V3 rail that feeds the RP2040. Measuring upstream of it would
mostly measure the converter's efficiency, which varies with the load. The right point
is the 3V3 rail, and it can be reached without touching the board: ground `3V3_EN`
(pin 37) to disable the on-board regulator, and feed `3V3(OUT)` (pin 36) from outside,
the shunt on that supply.

The port helps here. The firmware runs from SRAM, so the QSPI flash stays idle and adds
almost nothing to the reading.

**Use a Pico, not a Pico W.** The CYW43439 wireless chip sits on the same rail and draws
current even when idle, which would put a varying floor under every reading.

**Two benches were weighed.** An INA226 module, a shunt and a 16-bit converter read over
I²C, gives an average current. That is enough to compare steady operating points, and it
is better than the INA219, whose 12 bits are marginal for telling voltage ranges apart. A
Bus Pirate acting as I²C master can read it, with no second microcontroller to program.
A Nordic Power Profiler Kit II costs an order of magnitude more. It powers the target,
spans about a hundred nanoamps to an amp, and integrates energy over a window. Energy per
unit of work, not average current, is what settles DVFS against race-to-sleep. The PPK2
was chosen on 2026-09-24; as of 2026-09-27 the bench has not been built.

Because the core regulator is linear (RP2040 datasheet, section 2.10), part of what V²
promises is dissipated in it rather than saved. The bench would measure a gain smaller
than V².

## The DVFS driver of the RP2040

`Escapement/CORTEX-Mx/RP2040/Escapement_Processor.c` implements `OSSetProcessorSpeed` on
three operating points. `make KERNEL=PA` builds it in the Pico example.

| Point | clk_sys | Core voltage | With `UNDERVOLT=1` |
|---|---|---:|---:|
| `OS_125MHZ_SPEED` | PLL, 1500 MHz / 6 / 2 | 1.10 V | 1.10 V |
| `OS_50MHZ_SPEED` | PLL, 1500 MHz / 6 / 5 | 1.05 V | 0.95 V |
| `OS_12MHZ_SPEED` | crystal, through `clk_ref` | 1.05 V | 0.90 V |

**The PLL stays locked.** Every change parks `clk_sys` on `clk_ref`, changes a post
divider if need be, and takes the PLL back, glitchlessly both ways. None of this waits
for a lock. The whole change runs with interrupts masked, 3.9 to 8.7 µs by the bench of
2026-09-24 (`rp2040.md`). The price is a VCO that keeps running at 12 MHz. Stopping it
there would save its current and cost a relock on the way back up, a trade the bench can
settle. The 50 MHz point replaces the 48 MHz one first declared, which the same VCO
cannot produce. The timer's 1 µs tick comes from the crystal and does not move.

The idle task sleeps at 125 MHz by default. `make KERNEL=PA SLEEP_SPEED=0` lets it sleep
at 12 MHz, the interrupt that wakes it raising the speed again (`rp2040.md`, "Where the
idle task sleeps").

**The voltage moves little within the specification.** The datasheet guarantees the core
between 1.05 and 1.16 V only (table 634), although the regulator accepts 0.80 to 1.30 V
in 50 mV steps. Within the specification the voltage therefore only goes from 1.10 to
1.05 V, about 9 % less energy per cycle. Since 1.05 V is valid up to 133 MHz, raising the
frequency never has to wait for the regulator. What is left is mostly frequency scaling,
which on its own saves little against race-to-sleep. The bench will say how much.

**Undervolting, for the bench only.** `make KERNEL=PA UNDERVOLT=1` pairs 0.95 V with
50 MHz and 0.90 V with 12 MHz. It also lowers the brown-out detector to 0.817 V; left
alone, it would reset the chip around 0.86 V (0.83 to 0.89 V). These settings rest on
reports, not on any guarantee. On the Raspberry Pi forums a Pico ran at 10 MHz under
0.90 V and did not under 0.85 V, and an RP2350 at 150 MHz failed below 0.90 V. The risks:

- no damage: lowering the voltage does not wear the chip, as raising it may;
- timing errors. Below the voltage a frequency needs, some logic paths settle late. The
  result is a fault or, worse, a wrong result with no symptom, so the bench has to run a
  computation whose result it checks, not only measure current;
- spread between chips and temperatures, with the regulator itself within 3 %. A
  setting of 0.90 V may deliver 0.873 V;
- the order of a change. The voltage has to rise before the frequency and fall after,
  which the driver does. When raising, it waits `RP2040_VREG_SETTLING_US`, 100 µs, with
  interrupts masked, and the kernel's latency pays for it. That is twice the 49 µs at
  most that `ROK` took to come back from 0.90 to 1.10 V on the board (`rp2040.md`). `ROK`
  only reports some 90 % of the target, and the datasheet gives no settling time.

**What emulation proves.** The RP2040 models have no voltage regulator, so
`escapement_pico.repl` adds a model of `VREG`. `escapement_pico.robot` hooks the writes
to it, to `CLK_SYS_CTRL` and to the post dividers (`rp2040_dvfs_check.py`). Over 100 ms
of `TaskLEDPico`, clk_sys never runs faster than the voltage allows, and the voltage
does change and reaches its lowest setting. The emulated core does not slow down with
its clock. The test shows that the registers are driven in the right order, and says
nothing about energy.

## Which MCU to port to next?

DVFS pays off where sleep is poor. The best low-power MCUs are therefore those where it
should help least, race-to-sleep serving them better.

A good candidate has a wide, finely controlled voltage range and fixed costs small next
to switching, hence a high frequency. Its deep sleep is poor or slow to wake from, or its
load is continuous and rules out sleeping.

| Target | Why | Caveat |
|---|---|---|
| **RP2040** | Core voltage adjustable in 50 mV steps, though the datasheet only guarantees 1.05 to 1.16 V (see above). Clock programmable from a few kHz to 133 MHz. Above all, **poor sleep**: no 1 µA Stop mode, and dormant mode loses the clocks. SLEEP, which gates the clocks while the kernel's timer runs on, is given at 0.39 mA typical in the datasheet's example, its PLLs stopped (roadmap, item 1). Race-to-sleep is weak there, so DVFS has a niche. Cortex-M0+, covered by the generic layer. | QSPI flash that does not follow the core voltage. The images run from SRAM, which leaves it idle, but it stays on the rail. |
| **RP2350** | The RP2040's successor, on a board as cheap, the Pico 2. The same PLL scheme, so the clock half of the DVFS driver should largely carry over, and 150 MHz. Its core regulator is a **switching** one, to be driven anew, which removes the caveat of the RP2040's linear regulator. Its Cortex-M33 is ARMv8-M, which the generic layer covers since 2026-09-24. The port exists, without a DVFS driver (`architecture.md`). | Sleep is better than on the RP2040, with a power manager, switchable domains and SRAM retention, so the argument of poor sleep weakens; only a measurement will say how much. QSPI flash as on the RP2040. **No Renode model exists** (checked 2026-09-22): Renode itself models neither the RP2040 nor the RP2350, and the RP2350 branch of the RP2040 models stopped at its first commits, in 2024. Renode does emulate the Cortex-M33, and a minimal platform was written for it (`emulation.md`). |
| **Cortex-M7 (STM32H7…)** | Presumably the largest gain in absolute watts. At 400–550 MHz switching should make up most of the budget, so V² would apply to most of it; not checked against a datasheet here. Typical workloads (audio, SDR, motor control) are **continuous** and cannot sleep. | Core not covered by the generic layer, which has the M0, M3, M4 and M33. |
| **STM32L4** | Once the cheapest STM32 port: a Cortex-M4, which the generic layer covers, with wider voltage scaling than the L1. Set aside (`roadmap.md`). | Renode ships no L4 platform, so the emulation would have to be built along with the port. |
| **STM32U5** | Four voltage ranges, a 40 nm process, less leakage than anything else here. Ported, without DVFS. | Stop 2 draws tens of µA or less, so race-to-sleep dominates even more than on the L1, and the gain is probably **smaller**, not larger. On the UNO Q's STM32U585 it is smaller still: see below. |
| ESP32, Ambiq Apollo | — | To rule out. The ESP32 already has vendor DFS (`esp_pm`) with automatic idling; on Ambiq parts the voltage is managed internally, with no lever for the user. |

**The STM32U585 of the UNO Q, read on 2026-09-26** (RM0456 rev. 7, datasheet DS13086
rev. 10; typical figures at 25 °C and 3 V, none measured). Its package is a BGA without
the SMPS (PKG = 00111, read on the board), so the core runs on the LDO, which dissipates
what the core voltage saves. It draws 84 µA/MHz in run at 160 MHz in range 1 and 73 at
24 MHz in range 4 (table 38), some 13 % apart. PLL1 runs at 160 MHz in range 1 only, at
55 MHz at most in range 3, and not at all in range 4 (table 87). Its output divider is
written with the PLL stopped, so a lower speed means either the 16 MHz crystal alone or
PLL1 locked again, 25 to 50 µs. Raising the range waits 12 to 21 µs a step for VOSRDY on
the LDO (table 76), and up to 50 µs for the EPOD booster above 55 MHz. The kernel's
timer, TIM2, counts on the APB clock, which a change of speed changes.

Idle, the difference is far larger. The idle task's WFI left the chip in Sleep, 4.35 mA
at 160 MHz (table 45). Stop 2 keeping all of SRAM, which the images run from, takes
20.5 µA (table 56). With a tenth of the processor busy at 160 MHz, that is some 5.3 mA
against 1.4 mA. Stop 2 stops TIM2, but LPTIM1 on the 32.768 kHz crystal counts through it
(RCC_CCIPR3), in steps of 30.5 µs. The order for the U5 was therefore an LPTIM time base
and an idle task in Stop 2 first, some four times less current by the datasheet. DVFS
would come after, if at all, for some 10 % more on this board, and only once the PPK2 has
measured the first. The idle task has slept in Stop 2 on the board since 2026-09-27
(`stm32u5.md`); its current is still to be measured.

Cost of a port, measured on 2026-09-20, before the Cortex-M33 joined the generic layer:

```
Generic Cortex-M layer (M0/M3/M4)      921 lines   reusable as is
Vendor-specific layer                ~1800 lines   to be rewritten
  of which the DVFS driver             157 lines   the easy part
```

The context switch, the atomics and the scheduler do not move. The bulk of a port is the
comparator timer, the vector table and the UART, not energy management.

For energy, measuring comes before porting. Until there is a figure from hardware, the
next target is chosen blind, and the bench described above uses a board already at hand.
The RP2350 port was begun anyway, for its two cores rather than its regulator, and the
U5 port for the Arduino UNO Q's STM32U585 (`roadmap.md`).

Renode provides platforms for the STM32 F0, F1, F4, F7, G0, H7, L0, L1, L5 and W
families, but none for the L4 or the U5. The U5 port wrote a platform of its own
(`emulation.md`), and an L4 port would have to as well.

## What emulation will never tell

None of the platforms used here, from the RP2040 models and the L151 platform before
them to the platforms of the RP2350 and the U5, ties the speed of the emulated core to its
clock, and none models a supply voltage. No emulator here can say that DVFS saves energy.
What emulation shows is said above ("What emulation proves"): the kernel schedules, and
drives the registers in the right order.

## Sources

- Raspberry Pi, [*RP2040 Datasheet*](https://datasheets.raspberrypi.com/rp2040/rp2040-datasheet.pdf):
  the core regulator, its output voltages and the range the chip is specified for,
  the brown-out detector, the system PLL and the clock dividers.
- Raspberry Pi, [*RP2350 Datasheet*](https://datasheets.raspberrypi.com/rp2350/rp2350-datasheet.pdf):
  the switching core regulator and the power domains of the Pico 2.
- Raspberry Pi, pico-sdk: after raising the core voltage at start-up it waits
  `SYS_CLK_VREG_VOLTAGE_AUTO_ADJUST_DELAY_US`, 1000 µs by default
  ([`hardware/clocks.h`](https://github.com/raspberrypi/pico-sdk/blob/master/src/rp2_common/hardware_clocks/include/hardware/clocks.h),
  [`runtime_init_clocks.c`](https://github.com/raspberrypi/pico-sdk/blob/master/src/rp2_common/pico_runtime_init/runtime_init_clocks.c)).
- STMicroelectronics, RM0456 rev. 7 (STM32U5 reference manual) and DS13086 rev. 10
  (STM32U585 datasheet): the U585's figures above; DS13737 rev. 4 for the U575's Stop 2
  with the SMPS.
- Texas Instruments, [*INA226*](https://www.ti.com/product/INA226), and Nordic
  Semiconductor, [*Power Profiler Kit II*](https://www.nordicsemi.com/Products/Development-hardware/Power-Profiler-Kit-2):
  the two current benches weighed above.
