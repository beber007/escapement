# The power-aware variant and DVFS

`Escapement/CORTEX-Mx/STM32/Escapement_Processor.c` is a DVFS driver for the
STM32L1: three steps at 4, 16 and 32 MHz, with the core voltage set through
`PWR_CR`. The `stm32l-discovery-pa` example enables it — same application and
same 90 % load as `stm32l-discovery`, except that each task declares its
worst-case execution time, which the kernel uses to lower the frequency.

```sh
cd Escapement/CORTEX-Mx/STM32/Examples/stm32l-discovery-pa && make bin && cd -
renode emulation/renode/escapement_l1_pa.resc
```

**What works**: the variant builds (7,018 bytes against 6,116 for the Hard
version, about 900 bytes of DVFS logic), it starts, the driver writes
`PWR_CR = 0x800` — the 1.8 V range — and each of the three tasks runs one
instance before the kernel reaches its PA-specific idle loop, which raises the
frequency back before every `WFI`.

The three tasks are scheduled at their periods, verified by a Robot test in CI.
Over one emulated second:

| Output | Period | Toggles | Measured ratio | Theoretical ratio |
|---|---:|---:|---:|---:|
| PB12 | 500 | 625 | 5.95 | 6.00 |
| PB13 | 1000 | 313 | 2.98 | 3.00 |
| PB14 | 3000 | 105 | 1.00 | 1.00 |

The DVFS driver does get exercised: **18 PLL reprogrammings** over half a
second. The core voltage, however, stays at range 1 — the idle loop of the PA
variant goes back to full speed before every `WFI`, and a 90 % load leaves
little room to step down.

## A third platform defect

On an STM32L152RB, `OS_IO_TIM5` does not exist, so the kernel falls back to its
**16-bit timer mode**, where the upper half of the clock is rebuilt from the
overflow interrupt. But the Renode L151 platform declares TIM2 with
`initialLimit: 0xFFFFFFFF`, making it a **32-bit** counter — whereas TIM2 on an
STM32L1 is a 16-bit timer.

As a result the counter never wrapped at 65,536, the overflow interrupt never
came, and the upper half of the time stayed frozen. Measured directly:
`CNT = 93,741` after 0.3 s, well beyond what a 16-bit counter can reach. The
derived platform sets `initialLimit` to `0xFFFF`.

## Is DVFS worth anything on an STM32?

An open question, and one worth asking honestly before investing in this
variant.

**The physics.** For a fixed amount of work of *W* cycles, the dynamic energy
is `α·C·V²·f · W/f = α·C·V²·W`: it **does not depend on the frequency**.
Lowering *f* alone gains nothing, and even lengthens the active time and
therefore the leakage energy. The only lever is **V²**, and V can only be
lowered by lowering f.

**What makes the STM32L1 interesting.** Three regulator ranges — 1.8 V up to
32 MHz, 1.5 V up to 16 MHz, 1.2 V up to 4.2 MHz. That is (1.8/1.2)² =
**2.25×** in theory on dynamic energy. And the range is selected by software:
most firmwares set range 1 at start-up and never touch it again, so there is
unexploited headroom.

**What tempers this considerably.** The figure that matters is the µA/MHz of
the datasheet, and it does not follow a V² law: the order of magnitude quoted
for the STM32L1 is around 230 µA/MHz in range 1 against 185 to 200 in range 3 —
**figures to be checked against the ST datasheet**, they do not come from a
measurement made here. That is on the order of a 20 % gain per cycle, far from
the theoretical 2.25×. The V² law applies to switching only; the regulator, the
flash, the always-on analogue blocks and the leakage are incompressible.

**The real competitor is not “stay at maximum”, it is *race-to-sleep*.** Go up
to 32 MHz, finish as fast as possible, drop into Stop mode at a few µA. Since
dynamic energy is independent of f, running fast costs nothing extra and
shortens the window during which the fixed costs are paid. That is almost
always at least as good, and far simpler.

**DVFS only wins when sleeping is not an option:** idle gaps shorter than the
cost of waking up (leaving Stop plus relocking the PLL, a few tens of µs — to
be compared with the 820 µs and 1.64 ms periods of our examples), a latency
constraint forbidding deep sleep, or a peripheral requiring the core clock
domain. Note that the 90 % load of the example leaves almost no idle time to
exploit anyway.

**What remains solid in this project**, regardless of the energy gain: the
contribution is not “lowering the frequency” but **knowing when it can be
lowered without missing a deadline**, which is what DRA, OTE and DM_SLACK
compute from the declared WCETs. That is a scheduling contribution, and it
stands even if the measured gain proves modest.

## The measurement bench already exists

`Escapement/CORTEX-Mx/STM32/Examples/stm32l-discovery/IccMeasure.c` steps
through the three ranges and reads the current through the Icc measurement
built into the STM32L-Discovery board:

```c
OSSetProcessorSpeed(OS_32MHZ_SPEED);   ...
OSSetProcessorSpeed(OS_16MHZ_SPEED);   ...
OSSetProcessorSpeed(OS_4MHZ_SPEED);    ...
```

The original authors had set up exactly the experiment that is needed. It did
not build when the project was taken over; it now builds with the power-aware
kernel, as a target of `stm32l-discovery-pa`, and the CI keeps it that way. It
has not been run: **until it is, everything above remains reasoning, not
measurement.** A Discovery board settles it for good, and the result belongs in
this documentation whatever it turns out to be — including if it is
disappointing.

## Measuring the RP2040 instead

The L1 board is not the only way to settle the question, and it may not be the best one:
this documentation already argues that the RP2040 is where DVFS has a niche, because it
sleeps poorly. Measuring it needs a bench, which is described here so that it can be built
when the time comes.

**Where to insert the measurement.** On a Pico, 5 V from `VSYS` goes through a buck-boost
converter before reaching the 3V3 rail that feeds the RP2040. Measuring upstream of it
would mostly measure the efficiency of that converter, which varies with the load. The
right point is the 3V3 rail, and it is reachable without touching the board: ground
`3V3_EN` (pin 37) to disable the on-board regulator and feed `3V3(OUT)` (pin 36) from
outside, with the shunt on that supply.

One design choice of this port helps here. The firmware runs **from SRAM**, so the QSPI
flash stays idle and contributes almost nothing to what is measured.

**Use a Pico, not a Pico W.** The CYW43439 wireless chip sits on the same rail and draws
current even when idle, which would put a varying floor under every reading.

**Two benches.** An **INA226** module — shunt plus a 16-bit converter, read over I²C —
gives an average current, which is enough to compare steady-state operating points. Prefer
it to the INA219, whose 12 bits are marginal for telling voltage ranges apart. It can be
read straight from a Bus Pirate acting as I²C master, with no second microcontroller to
program. A **Nordic Power Profiler Kit II** costs an order of magnitude more and earns it
if the measurement becomes an axis of the project: it powers the target, spans about a
hundred nanoamps to an amp, and **integrates energy over a window** — which is the quantity
that actually settles DVFS against race-to-sleep, a comparison about energy per unit of
work rather than about average current.

**One caveat before spending anything.** The core regulator of the RP2040 is a **linear
regulator**, not a switching one, so part of the theoretical benefit is dissipated in the
regulator rather than saved. Lowering the voltage does lower the switching current, so a
gain remains measurable, but it will fall short of what a V² law suggests — one more
reason to measure rather than reason. Worth confirming against the datasheet first.

## The DVFS driver of the RP2040

`Escapement/CORTEX-Mx/RP2040/Escapement_Processor.c` implements `OSSetProcessorSpeed` on
three operating points, built with `make KERNEL=PA` in the Pico example:

| Point | clk_sys | Core voltage | With `UNDERVOLT=1` |
|---|---|---:|---:|
| `OS_125MHZ_SPEED` | PLL, 1500 MHz / 6 / 2 | 1.10 V | 1.10 V |
| `OS_50MHZ_SPEED` | PLL, 1500 MHz / 6 / 5 | 1.05 V | 0.95 V |
| `OS_12MHZ_SPEED` | crystal, through `clk_ref` | 1.05 V | 0.90 V |

**The PLL stays locked.** Every change parks `clk_sys` on `clk_ref`, changes a post divider
if need be and takes the PLL back, glitchlessly both ways: none waits for a lock, so the
whole change runs with interrupts masked for a few cycles. The price is a VCO that keeps
running at 12 MHz; stopping it there would save its current and cost a relock on the way
back up, a trade the bench can settle. The 50 MHz point replaces the 48 MHz one first
declared, which the same VCO cannot produce. The 1 µs tick of the timer comes from the
crystal and does not move.

**The voltage moves little within the specification.** The datasheet guarantees the core
between **1.05 and 1.16 V** only (table 634), whatever the regulator accepts — 0.80 to
1.30 V by 50 mV steps. Within the specification the voltage therefore only goes from 1.10
to 1.05 V, about 9 % less energy per cycle; since 1.05 V is valid up to 133 MHz, raising
the frequency never has to wait for the regulator. What is left is mostly frequency
scaling, which on its own saves little against race-to-sleep: the bench will say how much.

**Undervolting, for the bench only.** `make KERNEL=PA UNDERVOLT=1` pairs 0.95 V with
50 MHz and 0.90 V with 12 MHz, and lowers the brown-out detector, which would otherwise
reset the chip around 0.86 V (0.83 to 0.89 V), to 0.817 V. The figures come from reports
rather than from any guarantee: on the Raspberry Pi forums a Pico ran at 10 MHz under
0.90 V and did not under 0.85 V, and an RP2350 at 150 MHz failed below 0.90 V. What it
risks:

- **no damage** — lowering the voltage does not wear the chip, as raising it may;
- **timing errors**: below the voltage a frequency needs, some paths of the logic settle
  late, and the result is a fault or, worse, a **wrong result with no symptom** — the bench
  has to run a computation whose result it checks, not only measure current;
- **spread between chips and temperatures**, with the regulator itself within 3 %: a
  setting of 0.90 V may deliver 0.873 V;
- **the order of a change**: the voltage has to rise before the frequency and fall after.
  The driver does so, and when raising waits `RP2040_VREG_SETTLING_US` — 100 µs, a guess the
  bench has to replace, the datasheet giving no settling time and `ROK` only reporting 90 %
  of the target — with interrupts masked, which the kernel's latency pays for.

**What emulation proves.** The RP2040 models have no voltage regulator, so
`escapement_pico.repl` adds a model of `VREG`, and `escapement_pico.robot` hooks the writes
to it, to `CLK_SYS_CTRL` and to the post dividers (`rp2040_dvfs_check.py`): over 100 ms of
`TaskLEDPico`, clk_sys never runs faster than the voltage allows, the voltage does change
and reaches its lowest setting. As on the L1, the emulated core does not slow down with its
clock: the registers are driven in the right order, and nothing is said about energy.

## Which MCU to port to next?

The selection criterion **inverts the usual ranking**: DVFS pays off where
sleep is poor. The best low-power MCUs are precisely the ones where it helps
least, since race-to-sleep dominates everything there.

What to look for: a wide voltage range that is **finely controllable**, fixed
costs that are small next to switching — hence a **high frequency** — a **poor
or expensive-to-wake deep sleep**, and a **continuous** load that rules out
sleeping.

| Target | Why | Caveat |
|---|---|---|
| **RP2040** | Core voltage adjustable by 50 mV steps — though the datasheet only guarantees 1.05 to 1.16 V, see above — clock programmable from a few kHz to 133 MHz, and above all **poor sleep**: no 1 µA Stop mode, and dormant mode loses the clocks. Race-to-sleep is weak there, so DVFS regains a real niche. Cortex-M0+, already covered by the generic layer. | External QSPI flash running XIP, which does not follow the core voltage: a new fixed cost that will eat into the gain. |
| **RP2350** | The sequel to the RP2040 on a board as cheap, the Pico 2, and the way to take what is learnt on it further: same PLL and core regulator scheme, so the DVFS driver should largely carry over, and 150 MHz. Its core regulator is a **switching** one, which removes the caveat of the linear regulator of the RP2040 below. Its Cortex-M33 is ARMv8-M, the same kernel port as the U5 — made here on a better DVFS target, it would leave the U5 a mere board port. | Sleep is better than on the RP2040 — a power manager with switchable domains and SRAM retention — so the argument of poor sleep weakens; how much, only a measurement will say. XIP from QSPI flash as on the RP2040. Whether a Renode model exists has not been checked. |
| **Cortex-M7 (STM32H7…)** | The largest gain in absolute watts: at 400–550 MHz switching finally dominates the budget, so V² applies to the majority share. Typical workloads — audio, SDR, motor control — are **continuous**, hence impossible to put to sleep. | Core not covered by the generic layer, which stops at M4. |
| **STM32L4** | The cheapest STM32 port: a Cortex-M4, which the generic layer already covers, with wider voltage scaling than the L1. | Renode ships no L4 platform, so the emulation level of verification would have to be built along with the port. |
| **STM32U5** | Four voltage ranges, a 40 nm process, less leakage than anything else here. | Two traps. It is a **Cortex-M33**, so ARMv8-M: the generic layer stops at ARMv7-M and this is a kernel port, not a board port. And its Stop 2 mode reaches about a µA, so race-to-sleep dominates even more than on the L1 — the gain is probably **smaller**, not larger. |
| ESP32, Ambiq Apollo | — | To rule out: the ESP32 already has vendor DFS (`esp_pm`) with automatic idling; on Ambiq parts the voltage is managed internally, with no lever for the user. |

Cost of a port, measured on the current base:

```
Generic Cortex-M layer (M0/M3/M4)      921 lines   reusable as is
Vendor-specific layer                ~1800 lines   to be rewritten
  of which the DVFS driver             157 lines   the easy part
```

The context switch, the atomics and the scheduler do not move. The bulk of the
work is the comparator timer, the vector table and the UART — not energy
management.

**But the order matters.** Measuring comes before porting: until there is a
figure from hardware, choosing the next target is done blind. The bench described
above uses a board that is already at hand, which is why it now comes before
buying an L1 Discovery.

One practical note on any STM32 beyond the two families kept here: Renode
provides platforms for the F0, F1, F4, F7, G0, H7, L0, L1, L5 and W families, but
**none for the L4 or the U5**. Porting to either means writing its platform as
well, or giving up the emulation level of verification for that target.

## What emulation will never tell

The L151 platform models neither RCC nor PWR: the writes of the driver are
visible but have no effect on the real speed of the core. **No emulator will
validate DVFS** — that would require modelling the effect of a voltage change
on execution speed. What emulation proves here is that the kernel schedules
correctly *and* drives the right registers; not that it saves energy. That
measurement will need hardware.
