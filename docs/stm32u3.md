# The STM32U3 port

`Escapement/CORTEX-Mx/STM32U3` runs the kernel on the STM32U385 of a NUCLEO-U385RG-Q, a
Cortex-M33 at up to 96 MHz with 1 MB of flash and 256 KB of SRAM. The board was ordered
on 2026-10-06 and has not arrived. The port was written the same day from the reference
manual and the errata, before any chip could run it: it has run under Renode only, and
nothing on this page is measured. The idle task in Stop 2, LPTIM1 and LPUART1 followed
the same day, written the same way ("The idle task in Stop 2", below).

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
  those printed in its footers. Every address, bit and interrupt number the port writes
  was checked there, and the page is cited in the file that uses it.
- **ES0626 rev. 3** (July 2026), the errata of the STM32U375/385.
- STMicroelectronics' `cmsis-device-u3`, `stm32u385xx.h`, read on 2026-10-06 to
  cross-check the addresses, the register offsets, the bits and the IRQ numbers. It
  agreed with RM0487 everywhere. It was read, not copied into the repository.
- The plan of the port (version 2, 2026-10-06), kept outside the repository. The pins of
  the board come from it: UM3062 rev. 2 and ST's BSP. They were not read again, and
  revision 4 of the user manual has not been read at all.

## What is written

The port is the STM32U5's, changed where the chip differs.

| Part | File | What it does, and where RM0487 says so |
|---|---|---|
| Clocks | `Escapement_Processor.c` | the MSIS from MSIRC0 at 96 MHz (48, 24 or 12 with `make MHZ=`), locked on the 32.768 kHz crystal in the MSI's PLL mode, by the sequence below; the RCC's interrupt (IRQ 9) puts the PLL mode back if it unlocks |
| Kernel timer | `Escapement_Timer.c` | TIM2, 32 bits (table 401, p. 1609), counting microseconds of HCLK ("Timer clock", p. 408) and wrapping at 2^30; URS set (TIMx_CR1, p. 1688) |
| Timer events | `Escapement_TimerEvent.c` | TIM4: the U3 has no TIM5, and its interrupt, 48, is reserved (table 134, p. 629) |
| UART | `Escapement_UART.c` | USART1 on PA9 and PA10, alternate function 7, to the ST-LINK's virtual COM port, 115,200 baud from PCLK2 (RCC_CCIPR1, p. 468), with its FIFO; LPUART1 on PA2 and PA3, alternate function 8, D1 and D0, from HSI16 (RCC_CCIPR3, p. 471), which receives through Stop 2 |
| Low-power timer | `Escapement_LPTimer.c` | LPTIM1 on the LSE (LPTIM1SEL, p. 471), 16 bits, reset through the RCC rather than by its ENABLE bit (ES0626, 2.11.1) |
| Stop 2 | `Escapement_Stop2.c` | the idle task in Stop 2 between the kernel's events, LPTIM1 waking it, as on the U5; on waking, the lock of the MSI waited for and the raise of the clock replayed from what the registers read (below) |
| Interrupts | `Escapement_Interrupts.c` | the 125 entries of table 134 (p. 627-632), all routed to the kernel's dispatcher; 13 of them are reserved on the U375/385 |
| From SRAM | `Escapement_RamEntry.S`, `STM32U3_SRAM.ld` | SRAM1 and SRAM2, 256 KB from 0x20000000 (table 5, p. 116); the vector table aligned on 1024 bytes |

The clock set-up follows the order RM0487 gives:

1. PWR's clock (RCC_AHB1ENR2.PWREN, p. 444), read back for the 2 bus cycles that p. 410
   asks for.
2. The MSIS and the MSIK onto MSIRC0 divided by 8. That is 12 MHz, as at reset, which
   range 2 runs (table 104, p. 406). The PLL mode of an RC needs an output on it, enabled
   and ready (p. 402, 419), and a single RC for both outputs is what ST recommends
   (p. 401).
3. The LSE. Its drive is set to medium-high before LSEON (p. 404, 473), then the code
   waits for LSERDY, a bounded wait, and sets LSESYSEN and waits for LSESYSRDY. Without
   LSESYSEN the LSE reaches only the RTC (p. 405). Next comes MSIPLL0EN, then
   MSIPLL0RDY, also a bounded wait. Locked, MSIRC0 runs at 96.010 MHz, 2930 periods of
   the LSE, 107 ppm fast (table 102, p. 403). If the LSE does not start, MSIRC0 runs
   free.
4. Above 24 MHz the booster is needed, and above 48 MHz range 1 too. The order is that of
   9.3.3 (p. 333-334): BOOSTSEL = MSIS (RCC_CFGR4, p. 429), R1EN once R2RDY is set
   (PWR_VOSR, p. 368-369), R1RDY, BOOSTEN, BOOSTRDY.
5. The flash's wait states from table 43 (p. 227), read back, and before the frequency
   rises (7.3.3, p. 228). The prefetch stays off.
6. MSISDIV, written while the MSIS is ready (RCC_ICSCR1, p. 421), then MSISRDY.

Steps 4 to 6 are `_OSRaiseSystemClock`, which the idle task runs again after each Stop 2.
Each step looks first at what the registers read and is skipped if it is already done.

The examples are those of the U5: `TaskLEDU3`, `UARTEchoU3`, `TestTimerEventU3`,
`TaskWrapU3`, `IPCU3`, `StackGuardU3`, `SoakU3`, and with Stop 2 `TestLPTimerU3`,
`SleepU3`, `SleepWrapU3` and `Stop2EventWrapU3`. Two more serve the Renode suite:
`SleepFastU3` is `SleepU3` built with `FAST=1`, and `SoakStop2U3` is `SoakU3` with the
idle task of Stop 2 installed. `ClockU3` times the clock set-up on the board, the backup
domain reset first so that the LSE starts as after a power-on: the port reads the DWT's
cycle counter at each step when built with `OS_CLOCK_TIMES`, which only its image is,
every other left byte for byte as it was. `DVFSU3Flash`, with no kernel, steps through
the operating points for the PPK2 ("Measuring DVFS"). The outputs are D7 (PA8), D8 (PC7), D12 (PA6)
and D13 (PA5, also LD2). `SoakU3` reports on USART1. It adds to word 93 of its results
whether the MSI ever locked (bit 24), beside the count of relocks. Every example builds
with `-Wall` and no warning, with the hard and the soft kernel under both algorithms, at
all four frequencies.

Left out for now, and refused at build time where it applies:

- The power-aware kernel. TIM2 counts HCLK on this chip, so a DVFS driver would have to
  rescale TIM2 and TIM4 at each change of speed (plan §5.3).
- The HSE as the reference of the PLL mode. It would have to be 16 MHz (RCC_ICSCR1,
  p. 423), and the board comes without that crystal.
- A name of its own in `tools/soak.py`, which reads it as `nucleo` (below).

## The idle task in Stop 2

`Escapement_Stop2.c` is the U5's idle task (`docs/stm32u5.md`, "The idle task in Stop 2")
at the addresses of this chip. When the next event of the kernel is 5 ms off or more, no
UART sends and USART1 does not receive, the idle task arms LPTIM1 3 ms short of the
event. It stops TIM2 and TIM4 on an edge of LPTIM1, disables USART1 and enters Stop 2. On
waking it raises the clock and starts both timers again, moved on by the ticks LPTIM1
counted. LPUART1 receives through Stop 2, and the window after a byte keeps the idle task
in Sleep for 20 ms, as on the U5. The margins (`OS_STOP2_WAKE_US`, `OS_STOP2_MIN_US`,
`OS_STOP2_RAISE_US`) and the 30 cycles given back at each restart are the U5's. Nothing
has measured this chip's.

What differs from the U5, each from RM0487:

- **The wake-up.** The chip wakes on the MSIS in range 2 (9.3.5, p. 351). The MSIS keeps
  its frequency up to 48 MHz; above, the hardware sets MSISDIV to 48 MHz (10.3, p. 415;
  RCC_ICSCR1, p. 421). An image at 96 MHz thus wakes at 48 in range 2, and one at 48, 24
  or 12 MHz wakes as it was. The port enters Stop 2 from 96 MHz and leaves this to the
  hardware, rather than stepping down to 48 MHz itself before each Stop 2: the manual says
  what the hardware does, and stepping down would cost a wait for R2RDY at every entry.
- **The booster.** A wake-up at 48 MHz needs the booster on before Stop 2, fed by the
  MSIS (10.2.3, p. 405). At 96 and 48 MHz the port keeps it on throughout, BOOSTSEL set to
  the MSIS.
- **Range 1 on waking.** RM0487 does not say whether R1EN reads 1 or 0 after a Stop 2
  entered in range 1. `_OSRaiseSystemClock` reads PWR_VOSR. If R1EN reads 0, it waits for
  R2RDY, writes R1EN and waits for R1RDY. If R1EN reads 1, it only waits for R1RDY, since
  writing R1EN again would change nothing. Then it waits for BOOSTRDY, sets the wait
  states, and writes MSISDIV once MSISRDY and MSIKRDY are set (p. 421-422).
- **The MSI's lock.** The PLL mode loses its lock when the MSI stops in Stop 2
  (MSIPLL0RDY, p. 418). With MSIPLL0FAST at 0 it must lock again after each wake-up
  (p. 415). The idle task waits for MSIPLL0RDY first, at most 64 ticks of LPTIM1, 1.95 ms
  (`OS_STOP2_LOCK_TICKS`), and counts a miss. Then it sleeps the rest of the margin in
  Sleep at the clock it woke on, keeping 500 µs for the raise. This is the U5's order,
  where the HSE was waited for first: a lock that misses its bound eats into the sleep,
  not into the time kept for the raise. `make FAST=1` sets MSIPLL0FAST once the mode has
  locked, which keeps the lock through Stop 2 at the cost of the MSI's current there
  (p. 403, 415, 419).
- **STOPWUCK and STOPKERWUCK.** `OSInitStop2` clears both. The chip then wakes on the
  MSIS, and the MSIK is switched on with it (RCC_CR, p. 419-420; RCC_CFGR1, p. 426).
  The raise waits for MSIKRDY before writing the dividers, so an MSIK left off would hang
  it.
- **What does not run in Stop 2 is disabled first.** RM0487 asks it of every peripheral
  that does not run there (caution, p. 351), and of USART1 in particular (table 563, note
  1, p. 2406). TIM2 and TIM4 are stopped by their counter enable, as on the U5. USART1 is
  new: `_OSUARTStop2` clears TE, then UE, once the last byte has gone (TC), as the note
  of UE says (p. 2412), and sets both again after the raise. With its transmitter off, PA9
  returns to its port's configuration (51.5.2, p. 2362), so it gets a pull-up to stay at
  idle rather than float. The U5 port never disabled USART1, and ran.
- **Wake-up flags.** Stop 2 is entered only if every flag that may wake the chip is
  clear (table 91, p. 351). The compare flag of LPTIM1 is cleared before the WFI, and
  PWR_SR.STOPF, cleared by CSSF (p. 375), tells a Stop 2 from a WFI that fell through.
  HSION is cleared entering Stop 2 (p. 418) and set again for LPUART1 after it; it is read
  before the WFI.
- **LPTIM1** runs on the LSE, which reaches a peripheral other than the RTC only with
  LSESYSEN (p. 404-405): `OSInitLPTimer` checks LSESYSRDY as well as LSERDY. Its clock is
  enabled in Run, Sleep and Stop (LPTIM1EN, LPTIM1SLPEN, LPTIM1STPEN), all three needed
  for its interrupt to wake the chip (table 91, p. 352). The same goes for LPUART1.
- **Neither ICACHE nor the SRAM is touched.** RM0487 asks nothing of the cache before
  Stop (8.5, p. 319), and ES0626 has nothing like the U5's erratum 2.2.11. No SRAM is
  powered down, which keeps the chip clear of errata 2.2.7 to 2.2.12 and 2.2.17.
- **DBG_STOP and DBG_STANDBY** are cleared as on the U5 (DBGMCU_CR, p. 2849).

`SleepU3` is `SleepU5` on this board. A task every 100 ms raises D7, and a timer event of
TIM4 wakes a second task 40 ms later. The reports go out on USART1 to the virtual COM
port; USART1 only sends, so the idle task may enter Stop 2. LPUART1, on D0, receives the
framed count of the U5's link, but nothing on the bench feeds it yet. With `PHASES=n`
the idle task alternates n seconds in Stop 2 and n seconds in Sleep. D8 marks the phase:
D13, the U5's choice, also drives LD2, whose current JP4 would count. `WAKE=us` and
`RUN=us` give the second phase a later wake-up or a busy core, as on the U5. Word 14 of
its results counts the locks that missed their bound, where the U5 counted HSE starts.

## Under Renode

Renode models no STM32U3. `emulation/renode/escapement_u3.repl` adapts the U5's
platform: the Cortex-M33, TIM2 to TIM4, the watchdog, USART1, and GPIO ports A and C.
Its RCC, PWR and FLASH are Python peripherals that check the set-up rather than just
acknowledge it. Each ready flag comes some reads after its enable, so code that does not
wait for one reads it clear. Each rule of RM0487 the set-up must keep sets a bit when it
is broken. The rules are listed in the platform's header:

- the PLL mode only after LSERDY and LSESYSRDY, and only on an RC that runs;
- range 1 before the MSIS goes above 48 MHz, and the booster before it goes above 24;
- the wait states before the frequency, and BOOSTSEL before BOOSTEN;
- DBP before RCC_BDCR is written, and PWREN before PWR is;
- R1EN and R2EN changed only while the range is ready, and never to equal values.

The first 12 tests of `escapement_u3.robot` all passed on 2026-10-06 under the hard and the soft
kernel, EDF and deadline-monotonic:

- **The clock set-up follows RM0487 to 96 MHz.** The end state of every register the
  set-up writes is checked, and no rule is broken.
- **Without the LSE the MSIS runs free and the kernel still runs.**
- **An unlock of the MSI's PLL mode is set right by the RCC's interrupt.** The handler
  clears the flag and sets the mode again, and `SoakU3` counts it.
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

The platform enters Stop 2 when the port writes CSSF in PWR_SR with LPMS = 010, its last
step before the WFI, and at once lays out what RM0487 gives the wake-up: range 2, its
ready flag 4 reads of PWR_VOSR later; the MSIS's divider at 48 MHz if it was at 96; the
MSIS and the MSIK ready 2 reads of RCC_CR later; MSIPLL0RDY cleared and set again 8 reads
later, at once with MSIPLL0FAST once the mode has locked; HSION cleared; SW from
STOPWUCK; the booster kept on and ready, its setup being part of the wake-up (table 91,
p. 352). R1EN is cleared, or kept, its R1RDY coming back 4 reads later, when the robot
asks (PWR 0x3F4): RM0487 does not say which. LPTIM1 is Renode's model of the STM32L0's,
as on the U5, and LPUART1 its USART. Five rules join those of the set-up:

- MSISSEL or MSISDIV changed while the MSIS is not ready, or the same of the MSIK
  (p. 421-422);
- the MSIS raised after a wake-up before its PLL mode has locked again (p. 415);
- Stop 2 entered from an MSIS above 24 MHz without the booster on and fed by the MSIS
  (p. 405);
- a Stop mode entered other than Stop 2;
- Stop 2 entered with TIM2, TIM4 or USART1 still enabled (p. 351, 2406).

Thirteen tests were added on 2026-10-06, 25 in all. All passed that day under the hard
and the soft kernel, EDF and deadline-monotonic, and the eight tagged `stop2` also on
builds at 48, 24 and 12 MHz, on platforms whose timers count that frequency
(`escapement_u3_48mhz.repl` and the like):

- **LPTIM1 counts the crystal of 32.768 kHz** (`TestLPTimerU3`).
- **The idle task sleeps in Stop 2 and the wake-up replays the raise of the clock**
  (`SleepU3`, 3 s): every instance on its period within 1 µs, none late, every timer
  event within 5 µs, two entries into Stop 2 a period, every lock within its bound, the
  clock back as at start, and no rule broken.
- **The wake-up keeps to the manual when R1EN reads 1 after Stop 2**: the same, with
  R1EN kept.
- **A lock that does not come is waited for no longer than its bound**: every wake-up
  counted as a miss, 69 ticks at most, none late, and the raise before the lock the one
  rule broken, at 96 MHz only, since below it the clock is not raised.
- **With MSIPLL0FAST the chip wakes locked** (`SleepFastU3`), the lock never coming back
  otherwise.
- **SleepU3 reports on USART1 across Stop 2**, two reports a second apart.
- The three tests of LPUART1's window and wake-up byte of `escapement_u5.robot`.
- **The idle task sleeps in Stop 2 across the 2^30 wrap**, and **up to the wrap when an
  arrival lies beyond it** (`SleepWrapU3`, `Stop2EventWrapU3`, on
  `escapement_u3_wrap.repl`, whose TIM2, TIM4 and LPTIM1 run 1000 times faster): every
  start within 0.62 µs of its period, every event within 11.6 µs, TIM4 less TIM2 spread
  over 3.7 µs at most across the four builds, against 5 µs allowed.
- **The endurance test runs with the idle task of Stop 2 installed** (`SoakStop2U3`):
  no Stop 2, every part without error.
- **The platform catches a wake-up from Stop 2 that breaks the manual's rules**: the
  robot enters Stop 2 itself with the booster off, TIM2 counting and USART1 enabled,
  raises the MSIS to 96 MHz at once, then enters another Stop mode; each sets its bit.

On 2026-10-06 the port was also made wrong by hand in thirteen ways, each then run
against the 15 tests that concern Stop 2 and the set-up:

- the wait for the lock dropped;
- the wait for R2RDY before R1EN dropped, which hangs the wake-up;
- the wait for R1RDY dropped;
- R1EN written again when it reads 1, which hangs it when R1EN is kept;
- the wait for BOOSTRDY dropped;
- USART1 left enabled;
- STOPWUCK set, the chip waking on HSI16;
- LPMS left at Stop 0;
- the booster cleared before Stop 2;
- TIM2 left counting;
- HSION read after CSSF, as the U5 port reads it, which loses it for LPUART1;
- the raise left out;
- the wait for the MSIS and the MSIK ready before their dividers dropped.

Each failed at least one test but the last, which the platform cannot show: the MSIS is
the clock the core runs on after the wake-up, and the wait for the lock comes first and
reads RCC_CR enough times for both flags to be set.

The platform does not model MSPLIM, so `StackGuardU3` runs on the board only. The timers
count at 96 MHz exactly, not at the 96.010 MHz of the lock.

**This proves an order of writes, not a chip that runs.** The platform knows nothing of:

- how long the LSE, the PLL mode, the booster and range 1 take, nor the wake-up from
  Stop 2 and the relock after it;
- whether the crystal starts at the chosen drive;
- the frequency the MSI actually reaches;
- the current the chip draws, in Stop 2 or out of it.

## Where the plan was corrected by the manual

The plan agreed with RM0487 on every address, bit and number the port uses. Reading the
manual again changed three things:

- **The interrupts.** The plan listed the lines of table 134 without saying which are
  reserved on the U375/385. The CMSIS header of the U385 leaves out 13 of them: TIM12,
  USART2, HSP1, TIM8 (4 lines), LCD, FDCAN2 (2), I2C4 (2) and SPI4. Table 134's note 1
  says such lines are reserved, and `Escapement_Interrupts.h` marks them.
- **TIM3 is 32 bits on the U3** (table 401, p. 1609), where the U5's is 16. The platform
  models it so.
- **Two misprints in RM0487.** RCC_BDCR (p. 471) says DBP is "in PWR_BDCR"; it is in
  PWR_DBPR (p. 373, and the CMSIS header). Table 134 prints LPTIM4's offset as 0x1EC;
  the plan already noted that one.

Stop 2, LPTIM1 and LPUART1 were checked against the manual on the same day. The plan's
addresses, bits and interrupts held there too: LPTIM1 at 0x40044400 on interrupt 67,
LPUART1 at 0x40042400 on 66, APB3SLPENR at 0x0D0, APB3STPENR at 0x0F8, LPTIM1SEL and
LPUART1SEL in RCC_CCIPR3, STOPWUCK and STOPKERWUCK in RCC_CFGR1, LPMS, STOPF and CSSF in
PWR. The manual added five things the plan had left out:

- **LPTIM1 needs LSESYSEN too.** The plan asked for it for the MSI's PLL mode only.
  RM0487 asks for it for any user of the LSE but the RTC and TAMP (p. 404-405).
  `OSInitLPTimer` checks LSESYSRDY with LSERDY.
- **The MSIK wakes only through STOPKERWUCK.** The plan said HSION and MSISON are forced
  on at the wake-up through STOPWUCK. MSIKON depends on STOPKERWUCK (p. 419-420), and the
  raise waits for MSIKRDY: both bits must be 0, as RCC_CFGR1 asks them to be equal
  (p. 426).
- **A disabled USART lets its TX pin go.** The plan saw no cost in disabling USART1 before
  Stop 2. With its transmitter off, the TX pin returns to its port's configuration
  (p. 2362) and would float on the line to the ST-LINK, hence the pull-up on PA9.
- **APB3SLPENR and APB3STPENR are all ones at reset** (p. 458, 465). The port sets the
  bits of LPTIM1 and LPUART1 all the same, in case a firmware before it cleared them.
- **ARR, not CCR1, says the compare must stay below it** (LPTIM_ARR, p. 1940), as the
  U5's driver had it from RM0456; the code is unchanged.

## On the board

The NUCLEO-U385RG-Q came on 2026-10-09, plugged into the UNO Q beside the
NUCLEO-U575ZI-Q, each ST-LINK then named by its serial (`NUCLEO_SERIAL`, `NUCLEO_TTY`).
Read before anything was loaded: DBGMCU_IDCODE 0x10016454, revision Z (REV_ID 0x1001),
DEV_ID 0x454, bits 15 to 12 at 6 where the plan expected 0; CPUID 0x410FD214, r0p4;
FLASH_OPTR 0x1FEFF0AA, as ST ships it. An OpenOCD built from its sources on 2026-10-07
(`~/opt/openocd-upstream`, 0.12.0+dev-02732) loads the SRAM with `stm32u3x.cfg`:

```sh
NUCLEO_MCU=u385 OPENOCD=~/opt/openocd-upstream/bin/openocd NUCLEO_SERIAL=... \
    tools/nucleo_load.sh build/SoakU3.elf
NUCLEO_MCU=u385 OPENOCD=... NUCLEO_SERIAL=... NUCLEO_TTY=... tools/soak.py nucleo 5m 1m SoakU3.elf
```

At 96 MHz, at d9bad06, `SoakU3` ran 329 s with every part active and none in error, the
pulse at most 75 µs late, the timer events 82 µs, the link 189,569 bytes at 115,200 baud
with no overrun and no error; `SoakFirmU3` 182 s, 14,481 mandatory instances, 11,099
optional ones run and 10,621 dropped, none in error, the latest end 3,474 µs into the
5 ms, the link 114,197 bytes, no overrun. Neither relocked the MSI. That is the first
run of the port.

The same day, at 2e7f0b6, the check of each commit was run by hand (`board_ci.md`,
"The NUCLEO-U385RG-Q") and answered these points of the list below:

- **The PLL mode.** Locked, the kernel's second lasts 121.0 ppm less than the one NTP
  disciplines, over 300 reports (standard error 1.9 ppm): MSIRC0 runs 121 ppm fast,
  where 2930 periods of the LSE give 107. The 14 ppm between are within what a crystal
  of 32.768 kHz may be off. A 16 MHz crystal as X2 is not needed for the kernel's time.
- **The relock, and the margin of the wake-up.** `SleepU3` ran 120 s, 1,200 instances
  and 2,281 entries into Stop 2, its events at most 7 µs off, no relock missed: the
  longest wake-up took 24 ticks of LPTIM1, 732 µs, of the 3 ms allowed. With `FAST=1`,
  MSIPLL0FAST keeping the MSI's PLL mode running in Stop 2, it took 1 tick, 30 µs: the
  relock is nearly the whole of the wake-up. What MSIPLL0FAST costs in Stop 2 is for
  the current on JP4 to say.
- **PWR_VOSR after Stop 2 entered in range 1.** The raise back to 96 MHz, range 1 and
  the booster, ran after each of those 2,281 wake-ups; which way R1EN read was not
  recorded.
- **USART1 disabled across Stop 2.** Each of `SleepU3`'s reports came whole.
- **The 30 cycles given back at each restart of TIM2.** TIM2 ran 8.1 ppm ahead of
  LPTIM1 over 120 s of `SleepU3`, 8.6 over 60, as the U5's ran some 7.5 behind: the
  correction is about right, its sign to be read over a longer run.

**A day of the soft kernel**, `SoakFirmU3` of 2e7f0b6 loaded on 2026-10-09 at 19:51
UTC (roadmap, task 3), stopped on 2026-10-10 at 19:53: 86,505 s, 80 wraps of the
kernel's clock, every count of the board at 0; 6,920,321 mandatory instances, 6,525,547
optional ones run and 3,854,934 dropped, none in error, the latest end 4,029 µs into
the 5 ms; the pulse at most 92 µs late; the link 55,322,791 bytes, no error, no overrun;
no line rejected by `tools/soak.py`. The MSI left its PLL mode and was locked again 3
times in the day, B1 never pressed: whether erratum 2.2.1, or the LSE disturbed by
something else, a day does not say; the kernel's time came through each.

**The clock set-up**, `ClockU3` the same evening, the backup domain reset so that the
LSE starts cold, in cycles of the 12 MHz of reset: the LSE's start 1,512,515, 126.0 ms;
LSESYSRDY 730, 61 µs, the two cycles of the LSE the manual gives; the MSI's lock 7,528,
627 µs, against tSTAB's 0.8 ms; R1RDY 233, 19.4 µs; BOOSTRDY 38, 3.2 µs; the MSIS at
96 MHz 50 cycles. Locked, no relock.

**The pins**, the same evening: the PPK2's logic inputs on CN10, pins 23, 21, 13 and
11 (UM3062 rev. 4, table 18), under `TaskLEDU3`, `tools/ppk2_nucleo.py pins` found D7
(PA8), D8 (PC7), D12 (PA6) and D13 (PA5) each on its period. UM3062's tables 15 to 18
give the same pins as `BoardU3.h`; JP4's pin 2 is the MCU's side (7.4.6 and "VDD power
supply input"), which
the PPK2's VOUT takes.

Still open: LPUART1 on D0 and D1, which an FTDI TTL-232R-3V3 cable on the UNO Q will
reach (`LINK_TTY` of `tools/unoq_sleep.py`); the current on JP4, which needs the PPK2
there (roadmap, task 11).

## Measuring DVFS

Before a DVFS driver is written for this chip (plan §5; release 0.1, task 11, as the
user decided on 2026-10-10), the PPK2 is to say whether it would gain anything, as it said on the STM32U5 that it would not (`power-aware.md`). The
datasheet (DS14830 rev. 2, CoreMark on the SMPS at 3.3 V, typical figures, as the plan
read them) gives 16.1 µA/MHz at 96 MHz in range 1 and 12.9 at 48 MHz in range 2, 20 %
less a cycle: range 2 is 0.75 V typical against range 1's 0.9 (RM0487, 9.3.3, p. 333).
On the U5 the datasheet's figures favoured the slower clock too, and the board did not.

`DVFSU3.c` (`DVFSU3Flash.elf`, written on 2026-10-10, not yet run on the board) computes
without a pause, a CRC-32 checked against the first, 20 s of LPTIM1 at each of eight
points: 96 MHz in range 1; 48 MHz in range 1 and in range 2, the voltage alone between
them; 24 MHz in range 1, in range 2, and in range 2 with the booster on, what the booster
draws by itself; 12 MHz in range 1 and in range 2. The eight on the SMPS, then on the
LDO, and round again, 320 s a round. No kernel: the port's clock set-up at 12 MHz, then
each point by the sequences of 9.3.3, each change of range, booster, divider and
regulator timed in cycles of the DWT. D7, D8 and D12 give the point to the PPK2's D0 to
D2; a line on USART1 at the end of each phase gives the regulator, the point, the CRCs
computed and those wrong, and the times. `escapement_u3.robot` runs it at 1 ms a point
(`DVFSU3Short`): 16 points, every CRC right, no rule of the platform broken; 96 MHz
without the booster, tried on purpose, broke one.

The order, on the bench of "The current on JP4" below:

1. The pins first, untested on this board: the PPK2's D0 to D3 on D7, D8, D12 and D13,
   its VCC on the board's 3V3, GND on its ground, JP4 fitted, `TaskLEDU3` loaded, then
   `PPK2_PINS=D7:10000,D8:20000,D12:2000,D13:60000 tools/ppk2_nucleo.py pins 10`.
2. `DVFSU3Flash.elf` written with JP4 fitted (`NUCLEO_MCU=u385 tools/nucleo_flash.sh
   build/DVFSU3Flash.elf`, one writing of the flash, counted).
3. CN1 unplugged, the PPK2 in place of JP4, its VIN on the pin from the board's 3V3, D3
   off D13; CN1 plugged back, the image starting from the flash with no debugger since.
4. Two rounds, the lines of USART1 kept beside:
   `PPK2_PHASES=96R1,48R1,48R2,24R1,24R2,24R2B,12R1,12R2 tools/ppk2_nucleo.py phases 700`
   and `cat` of the ST-LINK's virtual COM port meanwhile. The phases come in order, the
   first eight of each round on the SMPS, the next eight on the LDO; the means by level
   at the end mix both, the rows of each phase do not.

From each phase: its median current, the charge a cycle (the current over the
frequency), and the charge a CRC (the current times 20 s over the CRCs counted), which
counts the wait states and anything else the clock does not. DVFS gains if 48 MHz in
range 2 costs less a CRC than 96 MHz in range 1 by more than the Stop 2 it gives up: a
given work done at 96 MHz and followed by Stop 2 against the same work spread at 48.
If word 9 reads 1, REGS never followed REGSEL: the package has no SMPS, and every phase
ran on the LDO.

## What only the board can decide

- **Which revision the chip is.** DBGMCU_IDCODE should read 0x10010454 (Z) or
  0x10070454 (X), and the errata differ between the two (ES0626, table 2). CPUID should
  read r0p4.
- **FLASH_OPTR.** ST ships it at 0x1FEFF0AA (p. 280), with SRAM_RST and SRAM2_RST at 1,
  which is what lets an image in SRAM survive a debugger's "reset halt". A tool may have
  changed it.
- **Loading.** Debian's OpenOCD 0.12.0 has no `stm32u3x.cfg`, and it is not known
  whether its `stm32u5x.cfg` can load into this SRAM. A loader should freeze TIM2 to TIM4
  under the debugger (DBGMCU_APB1LFZR = 0x7, p. 2849-2850).
- **The LSE.** Does it start at the medium-high drive, and how fast? The drive was taken
  from the U5, whose errata needed it; ES0626 has no such erratum.
- **The PLL mode.** How long the lock takes is unknown: about 0.8 ms in the datasheet,
  bounded in the code. The microsecond should be 107 ppm fast, to be measured against a
  reference. Whether to fit a 16 MHz crystal as X2 for an exact 96 MHz follows from that.
- **Range 1 and the booster.** The times R1RDY and BOOSTRDY take, and whether the
  sequence runs at all; only its order has been checked. Answered on 2026-10-10: 19.4
  and 3.2 µs (`ClockU3`, "On the board").
- **The pins.** PA9 and PA10 on alternate function 7 to the virtual COM port, D7, D8,
  D12 and D13 on PA8, PC7, PA6 and PA5 (UM3062 rev. 2, not read again). Also whether
  JP4 and JP5 are where the plan says. Answered on 2026-10-10 ("On the board"): the
  reports came on the virtual COM port, D7 to D13 where UM3062 rev. 4 and the PPK2 put
  them, JP4's pin 2 the MCU's.
- **Erratum 2.2.1.** PC13 toggling disturbs the LSE, and B1 is on PC13. The relock
  counter of `SoakU3` will tell whether it unlocks the PLL mode. In a day of
  `SoakFirmU3`, B1 never pressed, the MSI was locked again 3 times ("On the board").
- **Erratum 2.2.13** (revision Z only). VDDA must equal VDD for any current measurement.
- **PWR_VOSR after Stop 2 entered in range 1.** RM0487 does not say whether R1EN reads 1
  or 0. The port takes either, and Renode runs both (above). If the chip did something
  else, say R1EN at 1 with R1RDY never set again, the raise would wait forever.
- **The relock.** How long MSIPLL0RDY takes after each wake-up, against the 64 ticks the
  port allows, and what MSIPLL0FAST costs in Stop 2 against what it saves in the wait
  (`make FAST=1`). Word 14 of `SleepU3` counts the misses.
- **The margin of the wake-up.** The 3 ms of `OS_STOP2_WAKE_US` and the 500 µs kept for
  the raise are the U5's. The longest wake-up `SleepU3` counts (word 6) will say how far
  to lower them, with and without FSTEN, the regulators' fast start (PWR_CR3, p. 367),
  which the port leaves off.
- **USART1 disabled across Stop 2.** Whether the line to the ST-LINK stays quiet with PA9
  pulled up, and whether the first report after each wake-up comes out whole.
- **LPUART1 on D0 and D1.** PA2 and PA3 on alternate function 8 come from the plan's
  reading of UM3062 rev. 2 and ST's BSP, not from RM0487. Whether 115,200 baud survives
  HSI16's start on this chip without a wake-up byte, the U385's datasheet not having been
  read for that time.
- **The current on JP4.** `SleepU3` with `PHASES=30` gives Stop 2, Sleep and, with `RUN=`,
  the core running, for a PPK2 in place of JP4 (UM3062 rev. 2, as the plan read it).
  `SleepU3Flash`, the same linked into the flash (`STM32U3_FLASH.ld`, Renode runs it),
  survives the power cycle the measurement needs: programmed with JP4 fitted, CN1
  unplugged, the PPK2 in place of JP4, D8 to its D0, `tools/ppk2_nucleo.py`, then CN1
  plugged back, as on the U5's NUCLEO. As
  on the U5's NUCLEO, PA13 may leak once the ST-LINK is unplugged (ST's README sets it
  analog), VDDA must equal VDD (erratum 2.2.13 above), and B1 must not be pressed.
- **The 30 cycles given back at each restart of TIM2** (RESTART_CYCLES) were measured on
  the U5 at 160 MHz; the kernel's second against LPTIM1 over a long run will tell.
