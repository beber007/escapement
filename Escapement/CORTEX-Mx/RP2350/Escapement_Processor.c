/* Copyright (c) 2026 Bertrand Hurst. All rights reserved.
** Escapement - Lightweight Power-Aware Real-Time OS.
** Distributed under the terms of LICENSE at the root of this repository.
*/
/* File Escapement_Processor.c: Clock tree setup for the RP2350, transposed from the RP2040
** port. The clocks, the crystal oscillator and the PLLs are the same blocks at other
** addresses, with the same registers at the same offsets (RP2350 datasheet; pico-sdk,
** hardware/regs/clocks.h, xosc.h, pll.h, resets.h): the sequence is the RP2040's.
**
** The bootrom leaves the chip on its ring oscillator. The crystal is started and the
** whole tree switched onto it: the 1 us tick of the timer and the UART both need a
** precise reference. The system clock then goes to 150 MHz, the nominal
** frequency of the RP2350, which its core voltage after reset, 1.10 V, supports. The
** reference and peripheral clocks stay on the crystal.
**
** Platform version: RP2350 (Raspberry Pi Pico 2).
*/

#include "Escapement.h"

#define XOSC_BASE            0x40048000
#define XOSC_CTRL            *((volatile UINT32 *)(XOSC_BASE + 0x00))
#define XOSC_STATUS          *((volatile UINT32 *)(XOSC_BASE + 0x04))
#define XOSC_STARTUP         *((volatile UINT32 *)(XOSC_BASE + 0x0C))
#define XOSC_FREQ_RANGE_1_15 0xAA0
#define XOSC_ENABLE          (0xFAB << 12)
#define XOSC_STABLE          (1u << 31)

#define CLOCKS_BASE          0x40010000
#define CLK_REF_CTRL         *((volatile UINT32 *)(CLOCKS_BASE + 0x30))
#define CLK_REF_SELECTED     *((volatile UINT32 *)(CLOCKS_BASE + 0x38))
#define CLK_SYS_CTRL         *((volatile UINT32 *)(CLOCKS_BASE + 0x3C))
#define CLK_SYS_SELECTED     *((volatile UINT32 *)(CLOCKS_BASE + 0x44))
#define CLK_REF_DIV          *((volatile UINT32 *)(CLOCKS_BASE + 0x34))
#define CLK_SYS_DIV          *((volatile UINT32 *)(CLOCKS_BASE + 0x40))
#define CLK_PERI_DIV         *((volatile UINT32 *)(CLOCKS_BASE + 0x4C))
#define CLK_DIV_1            0x00010000u   /* INT = 1, FRAC = 0, as after reset */
#define CLK_PERI_CTRL        *((volatile UINT32 *)(CLOCKS_BASE + 0x48))
#define CLOCKS_SLEEP_EN0     *((volatile UINT32 *)(CLOCKS_BASE + 0xB4))
#define CLOCKS_SLEEP_EN1     *((volatile UINT32 *)(CLOCKS_BASE + 0xB8))
#define CLK_PERI_ENABLE      (1u << 11)

#define CLK_REF_SRC_XOSC     2
#define CLK_SYS_SRC_REF      0
#define CLK_SYS_SRC_AUX      1
#define CLK_SYS_AUXSRC_PLL   (0u << 5)
#define CLK_PERI_AUXSRC_XOSC (4u << 5)

#define RESETS_CLR           *((volatile UINT32 *)(0x40020000 + 0x3000))
#define RESETS_DONE          *((volatile UINT32 *)(0x40020000 + 0x08))
#define RESETS_PLL_SYS_BIT   (1u << 14)

#define PLL_SYS_BASE         0x40050000
#define PLL_CS               *((volatile UINT32 *)(PLL_SYS_BASE + 0x00))
#define PLL_PWR              *((volatile UINT32 *)(PLL_SYS_BASE + 0x04))
#define PLL_FBDIV_INT        *((volatile UINT32 *)(PLL_SYS_BASE + 0x08))
#define PLL_PRIM             *((volatile UINT32 *)(PLL_SYS_BASE + 0x0C))
#define PLL_CS_LOCK          (1u << 31)
#define PLL_PWR_PD           (1u << 0)
#define PLL_PWR_POSTDIVPD    (1u << 3)
#define PLL_PWR_VCOPD        (1u << 5)
#define PLL_PRIM_150MHZ      ((5u << 16) | (2u << 12))

/* Start-up delay of the crystal, in batches of 256 cycles: the millisecond of a 12 MHz
** crystal, 47, times the 6 the pico-sdk applies by default for slow-starting oscillators
** (PICO_XOSC_STARTUP_DELAY_MULTIPLIER, hardware/xosc.h), where the RP2040 port took 1. */
#define XOSC_STARTUP_DELAY   (47 * 6)


/* OSInitializeSystemClocks: Switches the reference, system and peripheral clocks onto the
** 12 MHz crystal, then the system clock onto the PLL at 150 MHz. Must be called before
** anything that depends on time, which includes _OSInitializeTimer and the UART. */
void OSInitializeSystemClocks(void)
{
  volatile UINT32 i;
  /* Every clock on through SLEEP, as after a power-on, as on the RP2040: a debugger's
  ** reset leaves the CLOCKS block alone, and an image loaded after one that gated them
  ** (Escapement_SleepGate.c) slept with the bus gated, the debugger reading zeros from
  ** all of it (2026-10-04). */
  CLOCKS_SLEEP_EN0 = 0xFFFFFFFFu;
  CLOCKS_SLEEP_EN1 = 0x7FFFFFFFu;
  /* Start the crystal and wait for it to settle. */
  XOSC_STARTUP = XOSC_STARTUP_DELAY;
  XOSC_CTRL = XOSC_FREQ_RANGE_1_15 | XOSC_ENABLE;
  while ((XOSC_STATUS & XOSC_STABLE) == 0);
  /* Reference clock onto the crystal, then system clock onto the reference. Each switch
  ** is acknowledged by a bit of the SELECTED register. */
  CLK_REF_CTRL = CLK_REF_SRC_XOSC;
  while ((CLK_REF_SELECTED & (1u << CLK_REF_SRC_XOSC)) == 0);
  /* SRC alone: the RP2350 comes out of reset with clk_sys on the auxiliary mux, itself on
  ** the ring oscillator (RP2350 datasheet, CLK_SYS_CTRL, SRC 1 and AUXSRC 2 at reset),
  ** and the auxiliary source may change only once the glitchless mux has left it
  ** (8.1.3.2). Writing 0 to the whole register, as on the RP2040 where it was the reset
  ** value, turned the auxiliary mux to the PLL, still in reset, while clk_sys ran
  ** through it (a review, 2026-09-30). */
  CLK_SYS_CTRL &= ~(UINT32)CLK_SYS_SRC_AUX;
  while ((CLK_SYS_SELECTED & (1u << CLK_SYS_SRC_REF)) == 0);
  /* Both dividers back to 1: a firmware run before may have changed them, and a reset of
  ** the cores by the debugger leaves the clocks as they were. The pico-sdk's left clk_ref
  ** divided by 2 on the Pico 2 of the bench, and TIMER0, which counts microseconds of
  ** it, ran at half speed under SleepPico2 (2026-10-03). */
  CLK_REF_DIV = CLK_DIV_1;
  CLK_SYS_DIV = CLK_DIV_1;
  /* Peripheral clock straight onto the crystal, so the UART keeps dividing a known 12 MHz
  ** whatever the system clock does afterwards. */
  /* Its aux mux changes only with the generator stopped, some cycles of the clock it ran
  ** on given for that (datasheet, "Clock generators"): an image started without a reset
  ** of the chip finds clk_peri on and on clk_sys, as the pico-sdk leaves it. */
  CLK_PERI_CTRL = 0;
  for (i = 0; i < 4; i += 1);
  CLK_PERI_DIV = CLK_DIV_1;              // the RP2040's clk_peri has no divider
  CLK_PERI_CTRL = CLK_PERI_AUXSRC_XOSC;
  CLK_PERI_CTRL = CLK_PERI_ENABLE | CLK_PERI_AUXSRC_XOSC;
  /* System clock to 150 MHz: the 12 MHz crystal multiplied by 125 gives a 1500 MHz VCO,
  ** divided by 5 then by 2, as the pico-sdk does for the RP2350. */
  RESETS_CLR = RESETS_PLL_SYS_BIT;
  while ((RESETS_DONE & RESETS_PLL_SYS_BIT) == 0);
  /* Off first, in case a firmware before left it running with other dividers, as the
  ** pico-sdk does (pll_init): its lock would otherwise be read from the old ones. */
  PLL_PWR = 0xFFFFFFFFu;
  PLL_FBDIV_INT = 0;
  PLL_CS = 1;                      /* REFDIV = 1 */
  PLL_FBDIV_INT = 125;
  PLL_PWR &= ~(PLL_PWR_PD | PLL_PWR_VCOPD);
  while ((PLL_CS & PLL_CS_LOCK) == 0);
  PLL_PRIM = PLL_PRIM_150MHZ;
  PLL_PWR &= ~PLL_PWR_POSTDIVPD;
  /* Switch glitchlessly: park on the reference, select the PLL as auxiliary source, then
  ** take the auxiliary. */
  CLK_SYS_CTRL = CLK_SYS_SRC_REF;
  while ((CLK_SYS_SELECTED & (1u << CLK_SYS_SRC_REF)) == 0);
  CLK_SYS_CTRL = CLK_SYS_AUXSRC_PLL | CLK_SYS_SRC_REF;
  CLK_SYS_CTRL = CLK_SYS_AUXSRC_PLL | CLK_SYS_SRC_AUX;
  while ((CLK_SYS_SELECTED & (1u << CLK_SYS_SRC_AUX)) == 0);
} /* end of OSInitializeSystemClocks */
