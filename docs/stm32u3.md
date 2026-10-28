# The STM32U3 port

`Escapement/CORTEX-Mx/STM32U3` runs the kernel on the STM32U385 of a NUCLEO-U385RG-Q.
The chip is a Cortex-M33 at up to 96 MHz with 1 MB of flash and 256 KB of SRAM. The port
was written on 2026-10-06 from the reference manual and the errata, before the board
came, and it was run under Renode. The idle task in Stop 2, LPTIM1 and LPUART1 followed
the same day, written the same way ("The idle task in Stop 2", below). The board came on
2026-10-09. What it has run and measured since is recorded in "On the board", "Measuring
DVFS" and "The power-aware kernel".

```sh
U3=Escapement/CORTEX-Mx/STM32U3/Examples/nucleo-u385
make -C $U3                                    # hard kernel, EDF, 96 MHz
make -C $U3 KERNEL=SOFT                        # soft kernel
make -C $U3 SCHEDULER=DEADLINE_MONOTONIC_SCHEDULING
make -C $U3 MHZ=48                             # 48, 24 or 12 MHz
make -C $U3 PHASES=30 [WAKE=2200|RUN=80000]    # SleepU3 in phases, for the PPK2 on JP4
make -C $U3 FAST=1                             # the MSI's PLL mode kept through Stop 2
renode-test emulation/renode/escapement_u3.robot
renode-test --variable MHZ:48 --variable PLATFORM:escapement_u3_48mhz.repl \
    --include stop2 emulation/renode/escapement_u3.robot   # built with MHZ=48; 24, 12 too
```

## Sources

- **RM0487 rev. 3** (February 2026), the reference manual of the STM32U3. Pages are
  those printed in its footers. Every address, bit and interrupt number that the port
  writes was checked there. The file that uses each of them cites the page.
- **ES0626 rev. 3** (July 2026), the errata of the STM32U375/385.
- STMicroelectronics' `cmsis-device-u3`, `stm32u385xx.h`. It was read on 2026-10-06 to
  cross-check the addresses, the register offsets, the bits and the IRQ numbers. It
  agreed with RM0487 everywhere. It was read, not copied into the repository.
- The plan of the port (version 2, 2026-10-06), kept outside the repository. The pins of
  the board come from it: UM3062 rev. 2 and ST's BSP. They were not read again, and
  revision 4 of the user manual was not read until 2026-10-10 ("On the board").

## What is written

The port is the STM32U5's, changed where the chip differs.

| Part | File | What it does, and where RM0487 says so |
|---|---|---|
| Clocks | `Escapement_Processor.c` | the MSIS from MSIRC0 at 96 MHz (48, 24 or 12 with `make MHZ=`), locked on the 32.768 kHz crystal in the MSI's PLL mode, by the sequence below; the RCC's interrupt (IRQ 9) puts the PLL mode back if it unlocks |
| Kernel timer | `Escapement_Timer.c` | TIM2, 32 bits (table 401, p. 1609), counting microseconds of HCLK ("Timer clock", p. 408), of the MSIK under the power-aware kernel ("The power-aware kernel", below), and wrapping at 2^30; URS set (TIMx_CR1, p. 1688) |
| Timer events | `Escapement_TimerEvent.c` | TIM4: the U3 has no TIM5, and its interrupt, 48, is reserved (table 134, p. 629) |
| UART | `Escapement_UART.c` | USART1 on PA9 and PA10, alternate function 7, to the ST-LINK's virtual COM port, 115,200 baud from PCLK2 (RCC_CCIPR1, p. 468), from HSI16 under the power-aware kernel, with its FIFO; LPUART1 on PA2 and PA3, alternate function 8, D1 and D0, from HSI16 (RCC_CCIPR3, p. 471), which receives through Stop 2 |
| Low-power timer | `Escapement_LPTimer.c` | LPTIM1 on the LSE (LPTIM1SEL, p. 471), 16 bits, reset through the RCC rather than by its ENABLE bit (ES0626, 2.11.1) |
| Stop 2 | `Escapement_Stop2.c` | the idle task in Stop 2 between the kernel's events, LPTIM1 waking it, as on the U5; on waking, the lock of the MSI waited for and the raise of the clock replayed from what the registers read (below) |
| Interrupts | `Escapement_Interrupts.c` | the 125 entries of table 134 (p. 627-632), all routed to the kernel's dispatcher; 13 of them are reserved on the U375/385 |
| From SRAM | `Escapement_RamEntry.S`, `STM32U3_SRAM.ld` | SRAM1 and SRAM2, 256 KB from 0x20000000 (table 5, p. 116); the vector table aligned on 1024 bytes |

The clock set-up follows the order that RM0487 gives:

1. PWR's clock is enabled (RCC_AHB1ENR2.PWREN, p. 444) and read back for the 2 bus
   cycles that p. 410 asks for.
2. The MSIS and the MSIK go onto MSIRC0 divided by 8. That is 12 MHz, as at reset, which
   range 2 runs (table 104, p. 406). The PLL mode of an RC needs an output on it,
   enabled and ready (p. 402, 419). A single RC for both outputs is what ST recommends
   (p. 401).
3. The LSE comes next. Its drive is set to medium-high before LSEON (p. 404, 473). The
   code then waits for LSERDY, a bounded wait, and sets LSESYSEN and waits for
   LSESYSRDY. Without LSESYSEN, the LSE reaches only the RTC (p. 405). Next comes
   MSIPLL0EN, then MSIPLL0RDY, also a bounded wait. Locked, MSIRC0 runs at 96.010 MHz,
   2930 periods of the LSE, 107 ppm fast (table 102, p. 403). If the LSE does not start,
   MSIRC0 runs free.
4. Above 24 MHz the booster is needed, and above 48 MHz range 1 is needed too. The order
   is that of 9.3.3 (p. 333-334): BOOSTSEL = MSIS (RCC_CFGR4, p. 429), R1EN once R2RDY
   is set (PWR_VOSR, p. 368-369), R1RDY, BOOSTEN, BOOSTRDY.
5. The flash's wait states come from table 43 (p. 227). They are read back, and they are
   set before the frequency rises (7.3.3, p. 228). The prefetch stays off.
6. MSISDIV is written while the MSIS is ready (RCC_ICSCR1, p. 421), then MSISRDY is
   awaited.

Steps 4 to 6 are `_OSRaiseSystemClock`, which the idle task runs again after each
Stop 2. Each step looks first at what the registers read, and the step is skipped if it
is already done.

The examples are those of the U5:

- `TaskLEDU3`, `UARTEchoU3`, `TestTimerEventU3`, `TaskWrapU3`, `IPCU3`, `StackGuardU3`
  and `SoakU3`;
- with Stop 2, `TestLPTimerU3`, `SleepU3`, `SleepWrapU3` and `Stop2EventWrapU3`.

Two more serve the Renode suite. `SleepFastU3` is `SleepU3` built with `FAST=1`, and
`SoakStop2U3` is `SoakU3` with the idle task of Stop 2 installed. `ClockU3` times the
clock set-up on the board. The backup domain is reset first, so that the LSE starts as
after a power-on. When built with `OS_CLOCK_TIMES`, the port reads the DWT's cycle
counter at each step. Only the image of `ClockU3` is built so, and every other image is
left byte for byte as it was. `DVFSU3Flash`, with no kernel, steps through the operating
points for the PPK2 ("Measuring DVFS"). `BenchDVFSU3`, with no kernel either, times the
power-aware kernel's driver ("The power-aware kernel"). The outputs are D7 (PA8), D8
(PC7), D12 (PA6) and D13 (PA5, also LD2). `SoakU3` reports on USART1. In word 93 of its
results, beside the count of relocks, it adds whether the MSI ever locked (bit 24).
Every example builds with `-Wall` and no warning:

- with the hard and the soft kernel under both algorithms;
- at all four frequencies;
- with the power-aware kernel under each of its policies.

Left out for now:

- The HSE as the reference of the PLL mode. The HSE would have to be 16 MHz (RCC_ICSCR1,
  p. 423), and the board comes without that crystal.

## The idle task in Stop 2

`Escapement_Stop2.c` is the U5's idle task (`docs/stm32u5.md`, "The idle task in Stop
2") at the addresses of this chip. The idle task arms LPTIM1 3 ms short of the next
event of the kernel when three conditions hold:

- the event is 5 ms off or more;
- no UART sends;
- USART1 does not receive.

The idle task then stops TIM2 and TIM4 on an edge of LPTIM1, disables USART1 and enters
Stop 2. On waking, it raises the clock and starts both timers again, moved on by the
ticks that LPTIM1 counted. LPUART1 receives through Stop 2. The window after a byte
keeps the idle task in Sleep for 20 ms, as on the U5. The margins (`OS_STOP2_WAKE_US`,
`OS_STOP2_MIN_US`, `OS_STOP2_RAISE_US`) and the 30 cycles given back at each restart are
the U5's. The board has measured how much of the margins a wake-up takes ("On the
board"), but the margins have not been lowered.

Each difference from the U5 comes from RM0487:

- **The wake-up.** The chip wakes on the MSIS in range 2 (9.3.5, p. 351). The MSIS keeps
  its frequency up to 48 MHz. Above 48 MHz, the hardware sets MSISDIV to 48 MHz (10.3,
  p. 415; RCC_ICSCR1, p. 421). An image at 96 MHz thus wakes at 48 in range 2, and an
  image at 48, 24 or 12 MHz wakes as it was. The port enters Stop 2 from 96 MHz and
  leaves this to the hardware. It does not step down to 48 MHz itself before each Stop
  2: the manual says what the hardware does, and stepping down would cost a wait for
  R2RDY at every entry.
- **The booster.** A wake-up at 48 MHz needs the booster on before Stop 2, fed by the
  MSIS (10.2.3, p. 405). At 96 and 48 MHz the port keeps the booster on throughout, with
  BOOSTSEL set to the MSIS.
- **Range 1 on waking.** RM0487 does not say whether R1EN reads 1 or 0 after a Stop 2
  entered in range 1. `_OSRaiseSystemClock` reads PWR_VOSR. If R1EN reads 0, it waits
  for R2RDY, writes R1EN and waits for R1RDY. If R1EN reads 1, it only waits for R1RDY,
  since writing R1EN again would change nothing. Then it waits for BOOSTRDY, sets the
  wait states, and writes MSISDIV once MSISRDY and MSIKRDY are set (p. 421-422).
- **The MSI's lock.** The PLL mode loses its lock when the MSI stops in Stop 2
  (MSIPLL0RDY, p. 418). With MSIPLL0FAST at 0, the mode must lock again after each
  wake-up (p. 415). The idle task first waits for MSIPLL0RDY, at most 64 ticks of
  LPTIM1, 1.95 ms (`OS_STOP2_LOCK_TICKS`), and it counts a miss. Then it sleeps the rest
  of the margin in Sleep, at the clock it woke on, and keeps 500 µs for the raise. This
  is the U5's order, where the HSE was waited for first: a lock that misses its bound
  eats into the sleep, not into the time kept for the raise. `make FAST=1` sets
  MSIPLL0FAST once the mode has locked. That keeps the lock through Stop 2, at the cost
  of the MSI's current there (p. 403, 415, 419).
- **STOPWUCK and STOPKERWUCK.** `OSInitStop2` clears both. The chip then wakes on the
  MSIS, and the MSIK is switched on with it (RCC_CR, p. 419-420; RCC_CFGR1, p. 426). The
  raise waits for MSIKRDY before it writes the dividers, so an MSIK left off would hang
  the raise.
- **What does not run in Stop 2 is disabled first.** RM0487 asks this of every
  peripheral that does not run there (caution, p. 351), and of USART1 in particular
  (table 563, note 1, p. 2406). TIM2 and TIM4 are stopped by their counter enable, as on
  the U5. USART1 is new: `_OSUARTStop2` clears TE, then UE, once the last byte has gone
  (TC), as the note of UE says (p. 2412). It sets both again after the raise. With its
  transmitter off, PA9 returns to its port's configuration (51.5.2, p. 2362), so PA9
  gets a pull-up to stay at idle rather than float. The U5 port never disabled USART1,
  and it ran.
- **Wake-up flags.** Stop 2 is entered only if every flag that may wake the chip is
  clear (table 91, p. 351). The compare flag of LPTIM1 is cleared before the WFI.
  PWR_SR.STOPF, cleared by CSSF (p. 375), tells a Stop 2 from a WFI that fell through.
  HSION is cleared on entry to Stop 2 (p. 418), and it is set again for LPUART1 after
  Stop 2. HSION is read before the WFI.
- **LPTIM1** runs on the LSE, which reaches a peripheral other than the RTC only with
  LSESYSEN (p. 404-405): `OSInitLPTimer` checks LSESYSRDY as well as LSERDY. Its clock
  is enabled in Run, Sleep and Stop (LPTIM1EN, LPTIM1SLPEN, LPTIM1STPEN). All three are
  needed for its interrupt to wake the chip (table 91, p. 352). The same goes for
  LPUART1.
- **Neither ICACHE nor the SRAM is touched.** RM0487 asks nothing of the cache before
  Stop (8.5, p. 319), and ES0626 has nothing like the U5's erratum 2.2.11. No SRAM is
  powered down, which keeps the chip clear of errata 2.2.7 to 2.2.12 and 2.2.17.
- **DBG_STOP and DBG_STANDBY** are cleared as on the U5 (DBGMCU_CR, p. 2849).

`SleepU3` is `SleepU5` on this board. A task every 100 ms raises D7, and a timer event
of TIM4 wakes a second task 40 ms later. The reports go out on USART1 to the virtual COM
port. USART1 only sends, so the idle task may enter Stop 2. LPUART1, on D0, receives the
framed count of the U5's link, but nothing on the bench feeds it yet. With `PHASES=n`,
the idle task alternates n seconds in Stop 2 and n seconds in Sleep. D8 marks the phase:
D13, the U5's choice, also drives LD2, whose current JP4 would count. `WAKE=us` and
`RUN=us` give the second phase a later wake-up or a busy core, as on the U5. Word 14 of
the results of `SleepU3` counts the locks that missed their bound, where the U5 counted
HSE starts.

## Under Renode

Renode models no STM32U3. `emulation/renode/escapement_u3.repl` adapts the U5's
platform. It has the Cortex-M33, TIM2 to TIM4, the watchdog, USART1, and GPIO ports A
and C. Its RCC, PWR and FLASH are Python peripherals that check the set-up rather than
just acknowledge it. Each ready flag comes some reads after its enable, so code that
does not wait for a flag reads it clear. Each rule of RM0487 that the set-up must keep
sets a bit when the set-up breaks it. The rules are listed in the platform's header:

- the PLL mode only after LSERDY and LSESYSRDY, and only on an RC that runs;
- range 1 before the MSIS goes above 48 MHz, and the booster before it goes above 24;
- the wait states before the frequency, and BOOSTSEL before BOOSTEN;
- DBP before RCC_BDCR is written, and PWREN before PWR is;
- R1EN and R2EN changed only while the range is ready, and never to equal values.

The first 12 tests of `escapement_u3.robot` all passed on 2026-10-06 under the hard and
the soft kernel, EDF and deadline-monotonic:

- **The clock set-up follows RM0487 to 96 MHz.** The end state of every register that
  the set-up writes is checked, and no rule is broken.
- **Without the LSE the MSIS runs free and the kernel still runs.**
- **An unlock of the MSI's PLL mode is set right by the RCC's interrupt.** The handler
  clears the flag and sets the mode again, and `SoakU3` counts the unlock.
- **The platform catches a set-up that breaks the manual's rules.** The robot writes
  wrong sequences itself, and each one sets its bit.
- The checks of `escapement_u5.robot` that the examples allow:
  - the probe task every millisecond;
  - the three periodic tasks;
  - the UART echo;
  - the timer events on TIM4;
  - the 2^30 wrap, with TIM2 1000 times faster (`escapement_u3_wrap.repl`);
  - tasks preempting one another in the FIFO queue and a slot buffer;
  - every part of `SoakU3` without error;
  - its report and link on USART1.

On 2026-10-06 the set-up was also made wrong by hand in seven ways:

- the wait for R1RDY dropped;
- the wait for BOOSTRDY dropped;
- the wait states written after the frequency;
- no BOOSTSEL;
- no LSESYSEN;
- the PLL mode before the MSIS was on MSIRC0;
- no PWREN, and, separately, no DBP.

The first test failed each time. Without DBP the set-up hangs in its wait.

### Stop 2 under Renode

The platform enters Stop 2 when the port writes CSSF in PWR_SR with LPMS = 010, the
port's last step before the WFI. At once, the platform lays out what RM0487 gives the
wake-up:

- range 2, with its ready flag 4 reads of PWR_VOSR later;
- the MSIS's divider at 48 MHz if it was at 96;
- the MSIS and the MSIK ready 2 reads of RCC_CR later;
- MSIPLL0RDY cleared and set again 8 reads later, or at once with MSIPLL0FAST once the
  mode has locked;
- HSION cleared;
- SW from STOPWUCK;
- the booster kept on and ready, its setup being part of the wake-up (table 91, p. 352).

R1EN is cleared, or kept, with its R1RDY coming back 4 reads later, when the robot asks
(PWR 0x3F4): RM0487 does not say which. LPTIM1 is Renode's model of the STM32L0's LPTIM,
as on the U5, and LPUART1 is Renode's USART. Five rules join those of the set-up:

- MSISSEL or MSISDIV changed while the MSIS is not ready, or the same of the MSIK (p.
  421-422);
- the MSIS raised after a wake-up before its PLL mode has locked again (p. 415);
- Stop 2 entered from an MSIS above 24 MHz without the booster on and fed by the MSIS
  (p. 405);
- a Stop mode entered other than Stop 2;
- Stop 2 entered with TIM2, TIM4 or USART1 still enabled (p. 351, 2406).

Thirteen tests were added on 2026-10-06, 25 in all. All passed that day under the hard
and the soft kernel, EDF and deadline-monotonic. The eight tagged `stop2` also passed on
builds at 48, 24 and 12 MHz, on platforms whose timers count at that frequency
(`escapement_u3_48mhz.repl` and the like):

- **LPTIM1 counts the crystal of 32.768 kHz** (`TestLPTimerU3`).
- **The idle task sleeps in Stop 2 and the wake-up replays the raise of the clock**
  (`SleepU3`, 3 s): every instance on its period within 1 µs, none late, every timer
  event within 5 µs, two entries into Stop 2 a period, every lock within its bound, the
  clock back as at start, and no rule broken.
- **The wake-up keeps to the manual when R1EN reads 1 after Stop 2**: the same results,
  with R1EN kept.
- **A lock that does not come is waited for no longer than its bound**: every wake-up
  counted as a miss, 69 ticks at most, none late. The one rule broken is the raise
  before the lock, at 96 MHz only, since below 96 MHz the clock is not raised.
- **With MSIPLL0FAST the chip wakes locked** (`SleepFastU3`), on a platform where the
  lock otherwise never comes back.
- **SleepU3 reports on USART1 across Stop 2**, two reports a second apart.
- The three tests of `escapement_u5.robot` for LPUART1's window and wake-up byte.
- **The idle task sleeps in Stop 2 across the 2^30 wrap**, and **up to the wrap when an
  arrival lies beyond it** (`SleepWrapU3`, `Stop2EventWrapU3`, on
  `escapement_u3_wrap.repl`, whose TIM2, TIM4 and LPTIM1 run 1000 times faster): every
  start within 0.62 µs of its period, every event within 11.6 µs, and TIM4 less TIM2
  spread over 3.7 µs at most across the four builds, against 5 µs allowed.
- **The endurance test runs with the idle task of Stop 2 installed** (`SoakStop2U3`): no
  Stop 2, and every part without error.
- **The platform catches a wake-up from Stop 2 that breaks the manual's rules**: the
  robot itself enters Stop 2 with the booster off, TIM2 counting and USART1 enabled. It
  raises the MSIS to 96 MHz at once, then enters another Stop mode. Each of these sets
  its bit.

On 2026-10-06 the port was also made wrong by hand in thirteen ways. Each wrong port was
then run against the 15 tests that concern Stop 2 and the set-up:

- the wait for the lock dropped;
- the wait for R2RDY before R1EN dropped, which hangs the wake-up;
- the wait for R1RDY dropped;
- R1EN written again when it reads 1, which hangs the wake-up when R1EN is kept;
- the wait for BOOSTRDY dropped;
- USART1 left enabled;
- STOPWUCK set, the chip waking on HSI16;
- LPMS left at Stop 0;
- the booster cleared before Stop 2;
- TIM2 left counting;
- HSION read after CSSF, as the U5 port reads it, which loses it for LPUART1;
- the raise left out;
- the wait for the MSIS and the MSIK ready before their dividers dropped.

Each failed at least one test, except the last. The platform cannot show the last one:
the MSIS is the clock that the core runs on after the wake-up, and the wait for the lock
comes first and reads RCC_CR enough times for both flags to be set.

The platform does not model MSPLIM, so `StackGuardU3` runs on the board only. The timers
count at 96 MHz exactly, not at the 96.010 MHz of the lock.

**This proves an order of writes, not a chip that runs.** The platform knows nothing of:

- how long the LSE, the PLL mode, the booster and range 1 take, nor the wake-up from
  Stop 2 and the relock after it;
- whether the crystal starts at the chosen drive;
- the frequency the MSI actually reaches;
- the current the chip draws, in Stop 2 or out of it.

## Where the plan was corrected by the manual

The plan agreed with RM0487 on every address, bit and number that the port uses. Reading
the manual again changed three things:

- **The interrupts.** The plan listed the lines of table 134 without saying which are
  reserved on the U375/385. The CMSIS header of the U385 leaves out 13 of them: TIM12,
  USART2, HSP1, TIM8 (4 lines), LCD, FDCAN2 (2), I2C4 (2) and SPI4. Table 134's note 1
  says that such lines are reserved, and `Escapement_Interrupts.h` marks them.
- **TIM3 is 32 bits on the U3** (table 401, p. 1609), where the U5's is 16. The platform
  models it so.
- **Two misprints in RM0487.** RCC_BDCR (p. 471) says DBP is "in PWR_BDCR", but it is in
  PWR_DBPR (p. 373, and the CMSIS header). Table 134 prints LPTIM4's offset as 0x1EC.
  The plan already noted that one.

Stop 2, LPTIM1 and LPUART1 were checked against the manual on the same day. The plan's
addresses, bits and interrupts held there too:

- LPTIM1 at 0x40044400 on interrupt 67;
- LPUART1 at 0x40042400 on 66;
- APB3SLPENR at 0x0D0 and APB3STPENR at 0x0F8;
- LPTIM1SEL and LPUART1SEL in RCC_CCIPR3;
- STOPWUCK and STOPKERWUCK in RCC_CFGR1;
- LPMS, STOPF and CSSF in PWR.

The manual added five things that the plan had left out:

- **LPTIM1 needs LSESYSEN too.** The plan asked for it for the MSI's PLL mode only.
  RM0487 asks for it for any user of the LSE but the RTC and TAMP (p. 404-405).
  `OSInitLPTimer` checks LSESYSRDY with LSERDY.
- **The MSIK wakes only through STOPKERWUCK.** The plan said that HSION and MSISON are
  forced on at the wake-up through STOPWUCK. MSIKON depends on STOPKERWUCK (p. 419-420),
  and the raise waits for MSIKRDY: both bits must be 0, as RCC_CFGR1 asks them to be
  equal (p. 426).
- **A disabled USART lets its TX pin go.** The plan saw no cost in disabling USART1
  before Stop 2. With its transmitter off, the TX pin returns to its port's
  configuration (p. 2362). The pin would then float on the line to the ST-LINK, hence
  the pull-up on PA9.
- **APB3SLPENR and APB3STPENR are all ones at reset** (p. 458, 465). The port sets the
  bits of LPTIM1 and LPUART1 all the same, in case a firmware before it cleared them.
- **ARR, not CCR1, says the compare must stay below it** (LPTIM_ARR, p. 1940), as the
  U5's driver had it from RM0456. The code is unchanged.

## On the board

The NUCLEO-U385RG-Q came on 2026-10-09. It was plugged into the UNO Q beside the
NUCLEO-U575ZI-Q, and each ST-LINK is then named by its serial (`NUCLEO_SERIAL`,
`NUCLEO_TTY`). Before anything was loaded, the board read:

- DBGMCU_IDCODE 0x10016454: revision Z (REV_ID 0x1001), DEV_ID 0x454, and bits 15 to 12
  at 6 where the plan expected 0;
- CPUID 0x410FD214, r0p4;
- FLASH_OPTR 0x1FEFF0AA, as ST ships it.

An OpenOCD built from its sources on 2026-10-07 (`~/opt/openocd-upstream`,
0.12.0+dev-02732) loads the SRAM with `stm32u3x.cfg`:

```sh
NUCLEO_MCU=u385 OPENOCD=~/opt/openocd-upstream/bin/openocd NUCLEO_SERIAL=... \
    tools/nucleo_load.sh build/SoakU3.elf
NUCLEO_MCU=u385 OPENOCD=... NUCLEO_SERIAL=... NUCLEO_TTY=... tools/soak.py nucleo 5m 1m SoakU3.elf
```

At 96 MHz, at d9bad06, `SoakU3` ran 329 s with every part active and none in error. The
pulse was at most 75 µs late, and the timer events at most 82 µs. The link carried
189,569 bytes at 115,200 baud with no overrun and no error. `SoakFirmU3` ran 182 s:
14,481 mandatory instances, 11,099 optional ones run and 10,621 dropped, and none in
error. Its latest end came 3,474 µs into the 5 ms, and the link carried 114,197 bytes
with no overrun. Neither image relocked the MSI. That was the first run of the port.

The same day, at 2e7f0b6, the check of each commit was run by hand (`board_ci.md`, "The
NUCLEO-U385RG-Q"). It answered these points of the list below:

- **The PLL mode.** Locked, the kernel's second lasts 121.0 ppm less than the one NTP
  disciplines, over 300 reports (standard error 1.9 ppm): MSIRC0 runs 121 ppm fast,
  where 2930 periods of the LSE give 107. The 14 ppm between are within what a crystal
  of 32.768 kHz may be off. A 16 MHz crystal as X2 is not needed for the kernel's time.
- **The relock, and the margin of the wake-up.** `SleepU3` ran 120 s, with 1,200
  instances and 2,281 entries into Stop 2. Its events were at most 7 µs off, and no
  relock missed: the longest wake-up took 24 ticks of LPTIM1, 732 µs, of the 3 ms
  allowed. With `FAST=1`, MSIPLL0FAST keeps the MSI's PLL mode running in Stop 2, and
  the longest wake-up took 1 tick, 30 µs: the relock is nearly the whole of the wake-up.
  The current on JP4 is to say what MSIPLL0FAST costs in Stop 2.
- **PWR_VOSR after Stop 2 entered in range 1.** The raise back to 96 MHz, range 1 and
  the booster, ran after each of those 2,281 wake-ups. Which way R1EN read was not
  recorded.
- **USART1 disabled across Stop 2.** Each of `SleepU3`'s reports came whole.
- **The 30 cycles given back at each restart of TIM2.** TIM2 ran 8.1 ppm ahead of LPTIM1
  over 120 s of `SleepU3`, and 8.6 over 60, as the U5's ran some 7.5 behind: the
  correction is about right, and its sign is to be read over a longer run.

**A day of the soft kernel**: `SoakFirmU3` of 2e7f0b6 was loaded on 2026-10-09 at 19:51
UTC (roadmap, task 3) and stopped on 2026-10-10 at 19:53. It ran 86,505 s, over 80 wraps
of the kernel's clock, with every count of the board at 0:

- 6,920,321 mandatory instances, 6,525,547 optional ones run and 3,854,934 dropped, none
  in error;
- the latest end 4,029 µs into the 5 ms;
- the pulse at most 92 µs late;
- the link 55,322,791 bytes, with no error and no overrun;
- no line rejected by `tools/soak.py`.

The MSI left its PLL mode and was locked again 3 times in the day, and B1 was never
pressed. A day does not say whether the cause was erratum 2.2.1 or the LSE disturbed by
something else. The kernel's time came through each relock.

**The clock set-up**: `ClockU3` ran the same evening, with the backup domain reset so
that the LSE starts cold. In cycles of the 12 MHz of reset, the steps took:

- the LSE's start, 1,512,515, 126.0 ms;
- LSESYSRDY, 730, 61 µs, the two cycles of the LSE that the manual gives;
- the MSI's lock, 7,528, 627 µs, against tSTAB's 0.8 ms;
- R1RDY, 233, 19.4 µs;
- BOOSTRDY, 38, 3.2 µs;
- the MSIS at 96 MHz, 50 cycles.

The MSI was locked, with no relock.

**The pins**: the same evening, the PPK2's logic inputs were on CN10, pins 23, 21, 13
and 11 (UM3062 rev. 4, table 18). Under `TaskLEDU3`, `tools/ppk2_nucleo.py pins` found
D7 (PA8), D8 (PC7), D12 (PA6) and D13 (PA5) each on its period. UM3062's tables 15 to 18
give the same pins as `BoardU3.h`. UM3062 gives JP4's pin 2 as the MCU's side (7.4.6 and
"VDD power supply input"). On the bench the PPK2 itself told the sides apart, below
("Measuring DVFS").

Still open:

- LPUART1 on D0 and D1, which an FTDI TTL-232R-3V3 cable on the UNO Q will reach
  (`LINK_TTY` of `tools/unoq_sleep.py`);
- the current of Stop 2 and Sleep on JP4 (`SleepU3` with `PHASES=30`): the PPK2 had then
  measured the operating points there, not the sleeps ("Measuring DVFS"). The sleeps
  were measured on 2026-10-12 ("The power-aware kernel").

## The endurance test from the flash

`make FLASH=1` links the examples into the flash (`STM32U3_FLASH.ld`), as on the U5.
`tools/soak.py` writes such an image through `tools/nucleo_flash.sh`, which reads the
flash first and writes only what differs. On 2026-10-10, after the day of task 3, the
images ran from the flash:

- `SoakFirmU3` ran 10 min, with 4,800 mandatory instances a minute and none in error.
  The latest end came 4,018 µs into the 5 ms, and the link had no error.
- `SoakU3` then ran 10 min, 601 s, with every count at 0. The pulse was at most 69 µs
  late, and the link carried 383,062 bytes without an error.

Each start found the image before still running from the flash, with its reports in the
port. `tools/soak.py` rejected them, 20 lines and 3 more, since their length was not the
new image's. It then wrote and started its own image. The long run, the service
`escapement-soak-u3`, has run `SoakU3` from the flash since 20:44 UTC that day. The
image is the one of the CI's build of c7c8afa, which the flash already held
(`ESCAPEMENT-SAME`). `BOARD_CI_U3=1` has been set on the bench since then
(`board_ci.md`, "The NUCLEO-U385RG-Q").

## Measuring DVFS

Before a DVFS driver was written for this chip (plan §5; release 0.1, task 11, as the
user decided on 2026-10-10), the PPK2 was to say whether DVFS would gain anything. On
the STM32U5 the PPK2 said that it would not (`power-aware.md`). The datasheet (DS14830
rev. 2, CoreMark on the SMPS at 3.3 V, typical figures, as the plan read them) gives
16.1 µA/MHz at 96 MHz in range 1 and 12.9 at 48 MHz in range 2. That is 20 % less a
cycle: range 2 is 0.75 V typical, against range 1's 0.9 (RM0487, 9.3.3, p. 333). On the
U5 the datasheet's figures favoured the slower clock too, and the board did not.

`DVFSU3.c` (`DVFSU3Flash.elf`, written and run on the board on 2026-10-10) computes
without a pause. It computes a CRC-32, checked against the first, for 20 s of LPTIM1 at
each of eight points:

- 96 MHz in range 1;
- 48 MHz in range 1 and in range 2, with the voltage alone between them;
- 24 MHz in range 1, in range 2, and in range 2 with the booster on, which shows what
  the booster draws by itself;
- 12 MHz in range 1 and in range 2.

The eight points run on the SMPS, then on the LDO, and round again, 320 s a round. There
is no kernel. The port's clock set-up runs at 12 MHz, then each point is reached by the
sequences of 9.3.3. Each change of range, booster, divider and regulator is timed in
cycles of the DWT. D7, D8 and D12 give the point to the PPK2's D0 to D2. A line on
USART1 at the end of each phase gives the regulator, the point, the CRCs computed and
those wrong, and the times. `escapement_u3.robot` runs it at 1 ms a point
(`DVFSU3Short`): 16 points, every CRC right, no rule of the platform broken. A run at 96
MHz without the booster, tried on purpose, broke one.

The order is this, on the bench of "The current on JP4" below:

1. The pins come first, then untested on this board. The PPK2's D0 to D3 go on D7, D8,
   D12 and D13, its VCC on the board's 3V3, and GND on its ground. JP4 is fitted and
   `TaskLEDU3` loaded. Then the bench runs
   `PPK2_PINS=D7:10000,D8:20000,D12:2000,D13:60000 tools/ppk2_nucleo.py pins 10`.
2. `DVFSU3Flash.elf` is written with JP4 fitted (`NUCLEO_MCU=u385 tools/nucleo_flash.sh
   build/DVFSU3Flash.elf`, one write of the flash, counted).
3. CN1 is unplugged, and the PPK2 goes in place of JP4, with its VIN on the pin from the
   board's 3V3 and D3 off D13. CN1 is plugged back, and the image starts from the flash
   with no debugger since.

   What the bench did on 2026-10-10 instead: the PPK2 went in place of JP4 at once, and
   the image was written through it, with its switch closed (`tools/nucleo_flash.sh`).
   Then that switch powered the MCU down for 6 s, which the debug domain does not
   outlive, before the measure: CN1 stays in. Three things read nothing on the way, and
   each was taken for another first. The PPK2's ground wire was on a pin that was not
   sure to be a ground, and it was moved to a GND of the Arduino connector. Until then
   the MCU never powered, in either mode. VIN and VOUT were the wrong way round: in
   ampere-meter mode the MCU ran through the PPK2's closed switch while the PPK2 read
   some -0.7 µA, as on the U5's NUCLEO, and the MCU stopped once the switch opened. In
   source mode the PPK2 fed the board's 3V3, which its regulator already held, and the
   MCU did not start. The pin of JP4 that VIN takes is the one from which the MCU runs
   in ampere-meter mode, and the PPK2 then reads its current. That holds whichever
   number UM3062 or the silkscreen gives the pin.
4. Two rounds run, with the lines of USART1 kept beside:
   `PPK2_PHASES=96R1,48R1,48R2,24R1,24R2,24R2B,12R1,12R2 tools/ppk2_nucleo.py phases 700`
   and `cat` of the ST-LINK's virtual COM port meanwhile. The phases come in order: the
   first eight of each round on the SMPS, the next eight on the LDO. The means by level
   at the end mix both, but the rows of each phase do not.

From each phase come:

- its median current;
- the charge a cycle (the current over the frequency);
- the charge a CRC (the current times 20 s over the CRCs counted), which counts the wait
  states and anything else the clock does not.

DVFS gains if 48 MHz in range 2 costs less a CRC than 96 MHz in range 1 by more than the
Stop 2 it gives up: a given work done at 96 MHz and followed by Stop 2, against the same
work spread at 48. If word 9 reads 1, REGS never followed REGSEL: the package has no
SMPS, and every phase ran on the LDO.

**Measured on 2026-10-10**: two rounds, with the PPK2 in ampere-meter mode in place of
JP4 (`docs/data/dvfs-u3-2026-10-10-*.txt`, `tools/dvfs_u3.py`), and every CRC right:

| Point | SMPS | a cycle | a CRC against 96 MHz | LDO | a cycle |
|---|---:|---:|---:|---:|---:|
| 96 MHz, range 1 | 2.02 mA | 21.1 pC | | 6.00 mA | 62.5 pC |
| 48 MHz, range 1 | 1.19 mA | 24.8 pC | +17.7 % | 3.19 mA | 66.4 pC |
| 48 MHz, range 2 | 0.87 mA | 18.1 pC | **−14.2 %** | 2.69 mA | 56.1 pC |
| 24 MHz, range 1 | 0.78 mA | 32.3 pC | +53.3 % | 1.80 mA | 75.0 pC |
| 24 MHz, range 2 | 0.48 mA | 19.8 pC | −6.2 % | 1.50 mA | 62.4 pC |
| 24 MHz, range 2, booster on | 0.47 mA | 19.6 pC | −6.8 % | 1.51 mA | 62.9 pC |
| 12 MHz, range 1 | 0.49 mA | 40.5 pC | +92.0 % | 1.14 mA | 95.3 pC |
| 12 MHz, range 2 | 0.30 mA | 24.9 pC | +18.1 % | 0.92 mA | 76.9 pC |

On the SMPS, 48 MHz in range 2 does the same work for 14.2 % less charge than 96 MHz in
range 1 (the datasheet's CoreMark said 20 %). Range 2 alone takes 27 % off at 48 MHz.
The booster costs nothing that the PPK2 sees. Racing at 96 MHz would sleep in Stop 2 for
the time saved, 144 µs a CRC. Even at 10 µA, that Stop 2 is some 1.4 nC, against the
41.6 nC that a CRC saved: **DVFS pays on the STM32U3**, unlike the STM32U5. The changes
took, in cycles of the clock in effect:

- range 1 ready in 295 at 12 MHz (24.6 µs);
- range 2 in 86 at 48 MHz (1.8 µs);
- the booster in 35 at 12 MHz (2.9 µs);
- REGS following REGSEL in 178 to 206 at 12 MHz (15 to 17 µs).

The package has its SMPS.

## The power-aware kernel

Since 2026-10-11 (release 0.1, task 11), `make KERNEL=PA` builds the power-aware kernel
on this port. Its DVFS driver is in `Escapement_Processor.c`, and it works between two
operating points:

| Point | MSIS | Range | Booster | Flash |
|---|---|---|---|---|
| `OS_48MHZ_SPEED` | MSIRC0 / 2 | 2 | on | 2 wait states |
| `OS_96MHZ_SPEED`, the maximum | MSIRC0 / 1 | 1 | on | 2 wait states |

There are two points only, from "Measuring DVFS" above: at 24 and 12 MHz a cycle costs
more than at 48 MHz in range 2, 19.8 and 24.9 pC against 18.1 on the SMPS, at either
range. The kernel lowers the speed as far as the deadlines allow, and it would spend
more there than at 48 MHz with Stop 2 after. Both points keep the booster and 2 wait
states (table 43, p. 227), so a change touches the range and the divider alone. A change
follows the order of 9.3.3 (p. 333-334):

- up: R1EN once R2RDY is set, R1RDY, then MSISDIV;
- down: MSISDIV, then R2EN once R1RDY is set, and R2RDY, so that the next raise finds
  range 2 ready.

Interrupts stay masked meanwhile.

TIM2 counts HCLK, which the changes halve and double. The plan (§5.3) had TIM2's
prescaler rewritten at each change, but loading a prescaler takes an update event, which
restarts its count: the fraction of a microsecond under way would be lost at every
change, and the kernel changes speed thousands of times a second. Under the power-aware
kernel, TIM2, TIM4 and `SoakU3`'s TIM3 count the MSIK instead, in external clock mode 2.
On all three, tim_etr3 is the MSIK (table 409, p. 1613; ETRSEL, TIMx_AF1, p. 1715; ECE,
TIMx_SMCR, p. 1690). The MSIK stays at 12 MHz from MSIRC0, locked on the LSE as the MSIS
is, whatever the MSIS's divider. It is divided by 4 before the timer (ETPS). Its 3 MHz
are then at most a quarter of the timer's clock from 12 MHz up, as RM0487 requires (p.
1631). The 3 MHz are then divided by 3 in the timer (`OS_TIMER_PRESCALER`,
`Escapement_Processor.h`). USART1 runs on HSI16 (USART1SEL, RCC_CCIPR1, p. 468), as
LPUART1 does. The other kernels keep TIM2 on HCLK, and their images are byte for byte as
before.

The idle task sleeps at the speed of the task before it, which the kernel leaves as it
is. Entered at 48 MHz, Stop 2 wakes at 48 MHz in range 2, with nothing to raise. When
Stop 2 is entered at 96, the chip wakes at 48 in range 2 (9.3.5, p. 351; RCC_ICSCR1, p.
421), and `_OSRaiseSystemClock` takes the chip back to 96 by the hard kernel's path.
Either way the MSIK is awaited before TIM2 starts again: the hardware sets MSIKON on the
way out, since STOPKERWUCK is 0 (RCC_CR, p. 419).

**On the board, 2026-10-11**: `BenchDVFSU3` (no kernel, interrupts masked, from SRAM)
ran two rounds, each the same. With no change of speed, TIM2 counted 60,006,431 µs over
1,966,081 ticks of LPTIM1. Those ticks are 60,000,031 µs of the LSE, so TIM2 ran 107 ppm
fast, the lock of the MSI (table 102, p. 403). While the driver changed speed 5,830,972
times, then 5,830,435, TIM2 counted the same 60,006,431 µs over the same ticks: not a
microsecond was lost to the changes. A change up to 96 MHz took 15.2 µs on average and
16 at most, most of it the wait for R1RDY. A change down to 48 MHz took 4.3, and 5 at
most. `_OSRaiseSpeedOnWake` from 48 MHz took 15.2. Each figure includes two reads of
TIM2, 0.21 µs at 48 MHz and 0.10 at 96. The longest change holds the interrupts for 16
µs, against the 87 µs of a byte at 115,200 baud and the 8 bytes of USART1's FIFO.

The board check of `tools/board_ci.sh` on the power-aware images (`soak_u3_pa`) was run
by hand the same day:

- `SleepU3` ran a minute: 600 instances, 1,141 entries into Stop 2, none late, every
  start within 1 µs of its period, every timer event within 9 µs, and TIM2 7.6 ppm off
  LPTIM1 across the sleeps;
- `SoakU3` ran two minutes, with no error and no restart;
- the reports of `SoakU3` were then timed for five minutes against Linux's clock. The
  kernel's second was 68 ppm short of NTP's. With the 107 ppm of the lock above, that
  puts the LSE some 39 ppm slow.

**What it saves, measured on 2026-10-12** (release 0.1, task 14b), on the candidate's
own images: `SleepU3` was built from `v0.1.0-rc1` with `FLASH=1 PHASES=30`. Under the
hard kernel, it runs at 96 MHz and races to its sleep. Under the power-aware kernel,
with each policy, it runs its task at 48 MHz in range 2. The task is some 100 µs of work
at 96 MHz, declared as 200. One periodic task runs every 100 ms, a timer event comes 40
ms on, and a report goes out on USART1 each second. The idle task spends 30 s in Stop 2,
D8 high, then 30 s in Sleep, D8 low. The PPK2 is in ampere-meter mode in place of JP4,
with its D0 on D8 (CN10, pin 21). Each image was written through the PPK2
(`tools/nucleo_flash.sh`). The PPK2's switch then powered the MCU down for 6 s, so that
the debug domain went down with it. Then 300 s of samples were taken once the MCU ran
again from its flash, with CN1 left in (`tools/pa_u3.py`). Each mean is over the four or
five phases of each kind that the window held whole. There were two runs of the five
images, from 15:24 and from 15:56 UTC. The gain is against the hard kernel of the same
run:

| `SleepU3`, kernel and policy | Idle in Stop 2, runs 1 and 2 | Idle in Sleep, runs 1 and 2 |
|---|---|---|
| hard, racing at 96 MHz | 485.39, 484.68 µA | 3.6460, 3.6420 mA |
| power-aware, OTE | 466.70, 466.45 µA (−3.9, −3.8 %) | 3.3989, 3.3977 mA (−6.8, −6.7 %) |
| power-aware, DRA | 468.62, 468.56 µA (−3.5, −3.3 %) | 3.3985, 3.3989 mA (−6.8, −6.7 %) |
| power-aware, DR_OTE | 466.29, 466.30 µA (−3.9, −3.8 %) | 3.3975, 3.3985 mA (−6.8, −6.7 %) |
| power-aware, DM_SLACK | 465.93, 466.20 µA (−4.0, −3.8 %) | 3.3977, 3.3998 mA (−6.8, −6.6 %) |

Within a run, the phases of an image agreed within 0.12 %, and from one run to the next
within 0.15 %. Each image ran its 3,070 instances with no wake-up late. The LSE kept
running, and the MSI locked again at each of its 2,984 wake-ups. Every start came within
3 µs of its period, and every timer event within 11 µs. OTE, DR_OTE and DM_SLACK give
the same figures within 0.1 %. DRA draws some 2 µA more in Stop 2 in both runs, its
phases apart from theirs, for a reason not looked into. **The power-aware kernel saves
some 4 % of this task set's charge with the idle task in Stop 2, some 7 % in Sleep.**
The work itself, 0.1 % of the time, is too small a part of the charge to account for
that. `stm32u5.md` found that the time awake makes up a Stop 2 phase's mean: the Sleep
of the wake-up margin before each event, and the report each second. That time is spent
at 48 MHz in range 2 under the power-aware kernel, and at 96 in range 1 under the hard
one. The Sleep phase shows what that difference is worth. This is a reading, not a
measure of each part. A task set that computes more of its time should gain more, up to
the 14 % of a cycle at 48 MHz. A task set that computes less should gain less. Neither
was measured.

Under Renode the platform's timers take the MSIK's 12 MHz in external clock mode 2
(`Escapement_STM32_Timer.cs`, `etrFrequency`). The RCC counts the changes of the MSIS's
frequency at 0x3E8. `escapement_u3.robot`, run with `--variable KERNEL:PA` on a `make
KERNEL=PA` build, passes whole, and each image that the build leaves out is skipped.
`BenchDVFSU3Short` changes speed some 2,000 times in 2 ms of LPTIM1 by the manual's
sequences. `SoakU3` changes speed some 2,400 times in 3.5 s, with every part without
error.

## What only the board can decide

This list was written before the board came. "On the board" and "Measuring DVFS" have
answered these points since:

- the revision, Z, though DBGMCU_IDCODE read 0x10016454, with its bits 15 to 12 at 6
  where the list below expects 0;
- FLASH_OPTR, as ST ships it;
- loading, by an OpenOCD built from its sources;
- the LSE, started in 126 ms;
- the lock, 627 µs, with the microsecond 121 ppm fast against NTP on 2026-10-09;
- range 1 and the booster;
- the pins;
- the relock, 24 ticks of LPTIM1 at most, and 1 with `FAST=1`;
- USART1 across Stop 2, with every report whole;
- the 30 cycles of a restart, with TIM2 some 8 ppm ahead of LPTIM1.

Still open are what PWR_VOSR reads after Stop 2 entered in range 1, LPUART1 on D0 and
D1, the cause of the 3 relocks of a day (erratum 2.2.1 or not), the margin of the
wake-up lowered, FSTEN, and, on JP4, what MSIPLL0FAST costs and the core running
(`RUN=`). The current of the sleeps on JP4 was measured on 2026-10-12 ("The power-aware
kernel"). The list as written before the board came:

- **Which revision the chip is.** DBGMCU_IDCODE should read 0x10010454 (Z) or 0x10070454
  (X), and the errata differ between the two (ES0626, table 2). CPUID should read r0p4.
- **FLASH_OPTR.** ST ships it at 0x1FEFF0AA (p. 280), with SRAM_RST and SRAM2_RST at 1.
  That is what lets an image in SRAM survive a debugger's "reset halt". A tool may have
  changed FLASH_OPTR.
- **Loading.** Debian's OpenOCD 0.12.0 has no `stm32u3x.cfg`. It is not known whether
  its `stm32u5x.cfg` can load into this SRAM. A loader should freeze TIM2 to TIM4 under
  the debugger (DBGMCU_APB1LFZR = 0x7, p. 2849-2850).
- **The LSE.** Does it start at the medium-high drive, and how fast? The drive was taken
  from the U5, whose errata needed it. ES0626 has no such erratum.
- **The PLL mode.** How long the lock takes is unknown: about 0.8 ms in the datasheet,
  and bounded in the code. The microsecond should be 107 ppm fast, to be measured
  against a reference. Whether to fit a 16 MHz crystal as X2 for an exact 96 MHz follows
  from that.
- **Range 1 and the booster.** The board must show the times that R1RDY and BOOSTRDY
  take, and whether the sequence runs at all. Only its order has been checked. Answered
  on 2026-10-10: 19.4 and 3.2 µs (`ClockU3`, "On the board").
- **The pins.** Are PA9 and PA10 on alternate function 7 to the virtual COM port, and
  D7, D8, D12 and D13 on PA8, PC7, PA6 and PA5 (UM3062 rev. 2, not read again)? Are JP4
  and JP5 where the plan says? Answered on 2026-10-10 ("On the board"): the reports came
  on the virtual COM port, and D7 to D13 were where UM3062 rev. 4 and the PPK2 put them.
  JP4's sides are as the PPK2 found them ("Measuring DVFS").
- **Erratum 2.2.1.** PC13 toggling disturbs the LSE, and B1 is on PC13. The relock
  counter of `SoakU3` will tell whether it unlocks the PLL mode. In a day of
  `SoakFirmU3`, with B1 never pressed, the MSI was locked again 3 times ("On the
  board").
- **Erratum 2.2.13** (revision Z only). VDDA must equal VDD for any current measurement.
- **PWR_VOSR after Stop 2 entered in range 1.** RM0487 does not say whether R1EN reads 1
  or 0. The port takes either, and Renode runs both (above). If the chip did something
  else, say R1EN at 1 with R1RDY never set again, the raise would wait forever.
- **The relock.** How long does MSIPLL0RDY take after each wake-up, against the 64 ticks
  that the port allows? What does MSIPLL0FAST cost in Stop 2, against what it saves in
  the wait (`make FAST=1`)? Word 14 of `SleepU3` counts the misses.
- **The margin of the wake-up.** The 3 ms of `OS_STOP2_WAKE_US` and the 500 µs kept for
  the raise are the U5's. The longest wake-up that `SleepU3` counts (word 6) will say
  how far to lower them, with and without FSTEN. FSTEN is the regulators' fast start
  (PWR_CR3, p. 367), which the port leaves off.
- **USART1 disabled across Stop 2.** Does the line to the ST-LINK stay quiet with PA9
  pulled up? Does the first report after each wake-up come out whole?
- **LPUART1 on D0 and D1.** PA2 and PA3 on alternate function 8 come from the plan's
  reading of UM3062 rev. 2 and ST's BSP, not from RM0487. Does 115,200 baud survive
  HSI16's start on this chip without a wake-up byte? The U385's datasheet has not been
  read for that time.
- **The current on JP4.** `SleepU3` with `PHASES=30` gives Stop 2, Sleep and, with
  `RUN=`, the core running, for a PPK2 in place of JP4 (UM3062 rev. 2, as the plan read
  it). `SleepU3Flash`, the same image linked into the flash (`STM32U3_FLASH.ld`, Renode
  runs it), survives the power cycle that the measurement needs. The steps are as on the
  U5's NUCLEO: programmed with JP4 fitted, CN1 unplugged, the PPK2 in place of JP4, D8
  to its D0, `tools/ppk2_nucleo.py`, then CN1 plugged back. As on the U5's NUCLEO, PA13
  may leak once the ST-LINK is unplugged (ST's README sets it analog), VDDA must equal
  VDD (erratum 2.2.13 above), and B1 must not be pressed.
- **The 30 cycles given back at each restart of TIM2** (RESTART_CYCLES) were measured on
  the U5 at 160 MHz. The kernel's second against LPTIM1 over a long run will tell.
