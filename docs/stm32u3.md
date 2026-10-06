# The STM32U3 port

`Escapement/CORTEX-Mx/STM32U3` runs the kernel on the STM32U385 of a NUCLEO-U385RG-Q, a
Cortex-M33 at up to 96 MHz with 1 MB of flash and 256 KB of SRAM. The board was ordered
on 2026-10-06 and has not arrived. The port was written the same day from the reference
manual and the errata, before any chip could run it: it has run under Renode only, and
nothing on this page is measured.

```sh
U3=Escapement/CORTEX-Mx/STM32U3/Examples/nucleo-u385
make -C $U3                                    # hard kernel, EDF, 96 MHz
make -C $U3 KERNEL=SOFT                        # soft kernel
make -C $U3 SCHEDULER=DEADLINE_MONOTONIC_SCHEDULING
make -C $U3 MHZ=48                             # 48, 24 or 12 MHz
renode-test emulation/renode/escapement_u3.robot
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
| UART | `Escapement_UART.c` | USART1 on PA9 and PA10, alternate function 7, to the ST-LINK's virtual COM port, 115,200 baud from PCLK2 (RCC_CCIPR1, p. 468), with its FIFO |
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

The examples are those of the U5 that need no Stop 2: `TaskLEDU3`, `UARTEchoU3`,
`TestTimerEventU3`, `TaskWrapU3`, `IPCU3`, `StackGuardU3` and `SoakU3`. The outputs are
D7 (PA8), D8 (PC7), D12 (PA6) and D13 (PA5, also LD2). `SoakU3` reports on USART1. It
adds to word 93 of its results whether the MSI ever locked (bit 24), beside the count of
relocks. Every example builds with `-Wall` and no warning, with the hard and the soft
kernel under both algorithms, at all four frequencies.

Left out for now, and refused at build time where it applies:

- The power-aware kernel. TIM2 counts HCLK on this chip, so a DVFS driver would have to
  rescale TIM2 and TIM4 at each change of speed (plan §5.3).
- Stop 2, LPTIM1 and LPUART1, which go together in the plan's step 4.
- A flash linker script.
- The HSE as the reference of the PLL mode. It would have to be 16 MHz (RCC_ICSCR1,
  p. 423), and the board comes without that crystal.
- A loader script and support in `tools/soak.py`.

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

`escapement_u3.robot` has 12 tests. All passed on 2026-10-06 under the hard and the soft
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

The platform does not model MSPLIM, so `StackGuardU3` runs on the board only. The timers
count at 96 MHz exactly, not at the 96.010 MHz of the lock.

**This proves an order of writes, not a chip that runs.** The platform knows nothing of:

- how long the LSE, the PLL mode, the booster and range 1 take;
- whether the crystal starts at the chosen drive;
- the frequency the MSI actually reaches;
- the current the chip draws.

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
  sequence runs at all; only its order has been checked.
- **The pins.** PA9 and PA10 on alternate function 7 to the virtual COM port, D7, D8,
  D12 and D13 on PA8, PC7, PA6 and PA5 (UM3062 rev. 2, not read again). Also whether
  JP4 and JP5 are where the plan says.
- **Erratum 2.2.1.** PC13 toggling disturbs the LSE, and B1 is on PC13. The relock
  counter of `SoakU3` will tell whether it unlocks the PLL mode.
- **Erratum 2.2.13** (revision Z only). VDDA must equal VDD for any current measurement.
- **The next steps of the plan.** Stop 2 brings its own questions. RM0487 does not say
  what PWR_VOSR reads on waking from Stop 2 entered in range 1. RM0487 also requires
  peripherals that cannot run in Stop 2 to be disabled before it, which the U5 port does
  not do. Then the relock after a wake-up with and without MSIPLL0FAST, and the margin of
  the wake-up.
