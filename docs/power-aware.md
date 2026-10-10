# The power-aware variant and DVFS

The power-aware kernel, `EscapementHardPA`, lowers the frequency and the core voltage
when the execution times the tasks declare leave room before the next deadline. Its one
driver is the RP2040's, described below. The kernel also runs in the host test
(`test/host`). This page sets out what the datasheets lead one to expect, what the
bench has measured, and what emulation can and cannot show.

**Measured so far.** On the STM32U5, 2026-10-03: a cycle costs least at 160 MHz, and
racing to Stop 2 beats every slower speed at every load (below). On the RP2350, the
same day: DVFS saves some 10 % at best within the specification, with the idle task in
SLEEP and PLL_SYS running, and next to nothing once SLEEP stops PLL_SYS (roadmap items 3
and 5). On the RP2040, the one chip with a driver: not measured yet (roadmap item 1).

## Is DVFS worth anything?

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
PLL kept running, the always-on blocks and the leakage do not scale with it.

**The alternative is race-to-sleep.** Run at full speed, finish, then sleep as deeply as
the chip allows. On the STM32U585 of the UNO Q, Stop 2 with all SRAM kept draws 20.5 µA
by its datasheet, and 8.2 µA on the STM32U575 with its SMPS (`stm32u5.md`). The RP2040
has no such mode (see the table below). Since dynamic energy does not depend on f,
running fast costs nothing extra and shortens the time the fixed costs are paid for.
Where the chip sleeps well, race-to-sleep is expected to match DVFS or beat it, and it is
simpler. On the U5 and the RP2350 it does (Measured so far, above).

DVFS can win only where sleeping is not an option:

- idle gaps shorter than the cost of waking up. On the U5, waking from Stop 2 took up to
  885 µs on the board on 2026-09-27, the clock rebuilt from the crystal (`stm32u5.md`),
  against the 1 ms period of the probe in `TaskLEDPico`;
- a latency constraint that forbids deep sleep;
- a peripheral that needs the core clock domain.

Whatever the energy gain, the kernel's part is the scheduling. Its four policies, OTE
(the default), DRA, DR_OTE and DM_SLACK, compute from the declared WCETs when the speed
can be lowered without a task missing its deadline (`EscapementHardPA.h`). OTE, the
one-time extension, gives the slack to the last task instance in the ready queue, after
Shin and Choi [1]. DRA, the dynamic reclaiming algorithm, and the EDF\* it runs on are
those of Aydin, Melhem, Mossé and Mejía-Alvarez [2]; DR_OTE combines the two. DM_SLACK,
for deadline-monotonic scheduling, was designed for ZottaOS: the slack a task leaves goes
to lower-priority tasks only, a scheme close to Saewong and Rajkumar's but with fewer
computations [3] (ZottaOS User Manual, May 2012, pp. 92-100).

ZottaOS measured a gain. Its manual shows the energy saved by EDF with dynamic
reclaiming and OTE, against EDF that only sleeps (LPM1) when idle, as a function of the
load, for three synthetic tasks on a prerelease MSP430F5438A (the X430F55438): about
10 % at a fixed frequency assignment for an 80 % worst-case load, and some 20 % more
when the actual load drops to 20 % (fig. 6.11 and table 6.5, pp. 98-99). The MSP430
port is gone and that result has not been reproduced on any target of Escapement.

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

**The instrument.** A Nordic Power Profiler Kit II powers the target, spans about a
hundred nanoamps to an amp, and integrates energy over a window: energy per unit of
work, not the average current an INA226 would give, is what settles DVFS against
race-to-sleep. Chosen on 2026-09-24, it has measured the STM32U5 and the Pico 2 since
2026-10-03, and the Pico on 2026-10-10 (below). The Pico 2 was measured on VSYS, its regulator included,
so its figures are not those of the 3V3 rail recommended above.

Because the core regulator is linear (RP2040 datasheet, section 2.10), part of what V²
promises is dissipated in it rather than saved. The bench would measure a gain smaller
than V².

**Measured on 2026-10-10** (roadmap, task 9), on probe2's plain Pico, the PPK2 powering
the 3V3 rail at 3.3 V, `3V3_EN` grounded, the USB unplugged. `SleepPico` (`rp2040.md`)
computing without a pause, the current at each operating point of the driver:

| Point | Within the specification | Undervolted (`UNDERVOLT=1`) |
|---|---|---|
| 125 MHz, 1.10 V | 21.89 mA, 175 pC a cycle | |
| 50 MHz | 10.18 mA at 1.05 V, 204 pC | 9.39 mA at 0.95 V, 188 pC |
| 12 MHz | 4.97 mA at 1.05 V, 414 pC | 4.48 mA at 0.90 V, 374 pC |

No result of the CRC computed throughout came out wrong, undervolted included. As on the
STM32U5 and the RP2350, a cycle costs least at full speed: what decides is the sleep the
time saved goes to. Then the same load in each sleep, some 100 µs of work at 125 MHz
every 100 ms, two cycles of 30 s phases agreeing within 0.5 %:

| The idle task's sleep | Mean |
|---|---|
| WFI at 125 MHz, as it is | 18.87 mA |
| WFI at 12 MHz, PLL_SYS locked (`SLEEP_SPEED=0`) | 5.09 mA |
| SLEEP at 125 MHz, PLL_SYS running (`SLEEP_GATE=1`) | 5.07 mA |
| SLEEP, PLL_SYS stopped, locked again in 53 µs on waking | 1.22 mA |

Racing at 125 MHz beats running at 50 MHz when the sleep draws under 2.4 mA, at 12 MHz
under 3.2 mA; undervolted, under 1.05 and 2.6 mA. The kernel itself, `TaskLEDPico`'s task
set built under each kernel and policy, its LED moved off the board, 60 s each:

| Kernel and policy | Idle in WFI | Idle in SLEEP (`SLEEP_GATE=1`) | The same, PLL_SYS stopped |
|---|---|---|---|
| hard, racing | 20.41 mA | 5.49 mA | 2.18 mA |
| power-aware, OTE | 19.75 mA (−3.2 %) | 5.53 mA (+0.7 %) | 2.22 mA (+1.8 %) |
| power-aware, DRA | 20.38 mA (−0.2 %) | | |
| power-aware, DR_OTE | 19.42 mA (−4.9 %) | 5.61 mA (+2.1 %) | 2.41 mA (+10.6 %) |
| power-aware, DM_SLACK | 19.55 mA (−4.2 %) | | |
| power-aware, OTE, idle at 12 MHz | 6.60 mA (−68 %) | | |

**The verdict on the RP2040**, within its specification: DVFS saves up to some 5 % while
the idle task waits in WFI at full speed, and nothing once it sleeps in SLEEP, where
racing wins; DR_OTE saves most of the four policies, DRA least. The sleep is the lever,
some 73 % of the task set's energy, and a SLEEP with PLL_SYS stopped, 1.22 mA against
5.07, takes more: the idle task does it since the same day (roadmap, task 10; the last
column, measured afterwards, `rp2040.md`, "SLEEP with PLL_SYS stopped"), 60 % off the
task set's current in SLEEP, and DVFS there costs more still. Below the
specified voltage 50 MHz edges racing to that sleep by a hair.

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
there would save its current and cost a relock on the way back up, 53 µs, which in SLEEP
takes the idle task from 5.07 to 1.22 mA (measured on 2026-10-10, above); the idle task
built with `SLEEP_GATE=1` does so, the operating points themselves unchanged. The 50 MHz point replaces the 48 MHz one first declared, which the same VCO
cannot produce. The timer's 1 µs tick comes from the crystal and does not move.

The idle task sleeps at 125 MHz by default. `make KERNEL=PA SLEEP_SPEED=0` lets it sleep
at 12 MHz, the interrupt that wakes it raising the speed again (`rp2040.md`, "Where the
idle task sleeps").

**The voltage moves little within the specification.** The datasheet guarantees the core
between 1.05 and 1.16 V only (table 634), although the regulator accepts 0.80 to 1.30 V
in 50 mV steps. Within the specification the voltage therefore only goes from 1.10 to
1.05 V (The physics, above). Since 1.05 V is valid up to 133 MHz, raising the
frequency never has to wait for the regulator. What is left is mostly frequency scaling,
which on its own saves little against race-to-sleep: on the RP2350, some 10 % at best
(Measured so far, above).

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
does change and reaches its lowest setting. The test shows that the registers are driven
in the right order, and says nothing about energy: none of the platforms used here ties
the speed of the emulated core to its clock, and none models a supply voltage.

## Targets weighed

DVFS pays off where sleep is poor. The best low-power MCUs are therefore those where it
should help least, race-to-sleep serving them better.

A good candidate has a wide, finely controlled voltage range and fixed costs small next
to switching, hence a high frequency. Its deep sleep is poor or slow to wake from, or its
load is continuous and rules out sleeping.

| Target | Why | Caveat |
|---|---|---|
| **RP2040** | Core voltage adjustable in 50 mV steps, though the datasheet only guarantees 1.05 to 1.16 V (see above). Clock programmable from a few kHz to 133 MHz. Above all, **poor sleep**: no 1 µA Stop mode, and dormant mode loses the clocks. SLEEP, which gates the clocks while the kernel's timer runs on, is given at 0.39 mA typical in the datasheet's example, its PLLs stopped (roadmap, item 1). Race-to-sleep is weak there, so DVFS has a niche. Cortex-M0+, covered by the generic layer. | QSPI flash that does not follow the core voltage. The images run from SRAM, which leaves it idle, but it stays on the rail. |
| **RP2350** | The RP2040's successor, on a board as cheap, the Pico 2. The same PLL scheme, so the clock half of the DVFS driver should largely carry over, and 150 MHz. Its core regulator is a **switching** one, to be driven anew, which removes the caveat of the RP2040's linear regulator. Its Cortex-M33 is ARMv8-M, which the generic layer covers since 2026-09-24. The port exists, without a DVFS driver (`architecture.md`). | Sleep is better than on the RP2040, with a power manager, switchable domains and SRAM retention, so the argument of poor sleep weakens: measured on 2026-10-03, SLEEP 4.2 mA, DORMANT 0.36 mA, DVFS some 10 % at best (roadmap items 3 and 5). QSPI flash as on the RP2040. Renode has no model of it: a platform of our own (`emulation.md`). |
| **Cortex-M7 (STM32H7…)** | Presumably the largest gain in absolute watts. At 400–550 MHz switching should make up most of the budget, so V² would apply to most of it; not checked against a datasheet here. Typical workloads (audio, SDR, motor control) are **continuous** and cannot sleep. | Core not covered by the generic layer, which has the M0, M3, M4 and M33. |
| **STM32L4** | Once the cheapest STM32 port: a Cortex-M4, which the generic layer covers, with wider voltage scaling than the L1. Set aside (`roadmap.md`). | Renode ships no L4 platform, so the emulation would have to be built along with the port. |
| **STM32U5** | Four voltage ranges, a 40 nm process, less leakage than anything else here. Ported, without DVFS. | Stop 2 draws tens of µA or less, so race-to-sleep dominates. Measured on 2026-10-03: DVFS gains nothing (below). |
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
(`stm32u5.md`).

**Measured on 2026-10-03**, on the STM32U575 of a NUCLEO-U575ZI-Q, its SMPS on, at 3.3 V,
with a PPK2 (`stm32u5.md`, "Slower clocks, measured"): the core computing drew 10.68 mA
at 160 MHz, 5.66 at 80, 3.64 at 40 and 1.82 at 16, some 67, 71, 91 and 113 pC a cycle.
The lower voltage ranges do not pay for the longer time: a cycle costs least at the
highest speed, and racing to Stop 2, 5 to 7 µA, beats each slower speed at every load.
On the LDO, the U585's only regulator, the same images drew 20.44, 10.50, 6.20 and 2.89
mA, 128, 131, 155 and 180 pC a cycle: twice the SMPS's, in the same order, where the
figures above, 84 and 73 µA/MHz, put a cycle 13 % cheaper slower. DVFS has nothing to
gain on the U5, on either regulator.

The bulk of a port is the comparator timer, the vector table and the UART, not energy
management: the context switch, the atomics and the scheduler do not move.

The RP2350 port was begun for its two cores rather than its regulator, and the U5 port
for the Arduino UNO Q's STM32U585 (`roadmap.md`).

## The STM32L1 driver, removed

The first DVFS driver of the project was written for the STM32L1, three steps at 4, 16
and 32 MHz, and removed on 2026-09-22 with the L1 examples (4dede39), once the Pico ran
every test they ran. Its bench, `IccMeasure.c`, was never run, for want of a board; under
Renode the voltage stayed in its highest range, the idle loop going back to full speed
before every `WFI`. That work found Renode's L151 platform declaring TIM2 a 32-bit
counter where the L1 has a 16-bit one, which a derived platform fixed. The history keeps
both.

## Sources

1. Y. Shin and K. Choi, *Power-Conscious Fixed Priority Scheduling for Hard Real-Time
   Systems*, Proc. 36th Design Automation Conference (DAC '99), pp. 134-139, June 1999.
2. H. Aydin, R. Melhem, D. Mossé and P. Mejía-Alvarez, *Power-Aware Scheduling for
   Periodic Real-Time Tasks*, IEEE Transactions on Computers, 53(5), pp. 584-600,
   May 2004.
3. S. Saewong and R. Rajkumar, *Practical Voltage-Scaling for Fixed-Priority Real-Time
   Systems*, Proc. IEEE Real-Time and Embedded Technology and Applications Symposium
   (RTAS), pp. 106-115, May 2003.
4. MIS-TIC, HEIG-VD, *ZottaOS User Manual*, May 2012, written by C. Evéquoz according to
   its metadata, in the archived repository
   [beber007/zottaos](https://github.com/beber007/zottaos).

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
