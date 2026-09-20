/* Copyright (c) 2026 Bertrand Hurst. All rights reserved.
** Escapement - Lightweight Power-Aware Real-Time OS.
** Derived from prior work; see LICENSE and NOTICE at the root of this repository.
*/
/* File Escapement_Processor.c: Clock tree setup for the RP2040.
**
** The bootrom leaves the chip running on its ring oscillator, whose frequency is neither
** precise nor known — around 6 MHz. Both the 1 us tick of the timer and the UART baud
** rate need a reference worthy of the name, so the crystal is started and the whole tree
** is switched onto it.
**
** The system clock is then taken to the nominal 125 MHz of the RP2040 through the PLL,
** because the per-activation cost of the kernel sets a floor on the achievable period: at
** 12 MHz a task of 1 ms already trips its overload guard.
**
** The reference and peripheral clocks stay on the crystal on purpose. The former keeps the
** microsecond tick exact whatever the core does — which is precisely what makes this chip
** a good target for the power-aware variant — and the latter keeps the UART dividing a
** frequency the driver knows.
**
** Platform version: RP2040 (Raspberry Pi Pico).
*/

#include "Escapement.h"

#define XOSC_BASE            0x40024000
#define XOSC_CTRL            *((volatile UINT32 *)(XOSC_BASE + 0x00))
#define XOSC_STATUS          *((volatile UINT32 *)(XOSC_BASE + 0x04))
#define XOSC_STARTUP         *((volatile UINT32 *)(XOSC_BASE + 0x0C))
#define XOSC_FREQ_RANGE_1_15 0xAA0
#define XOSC_ENABLE          (0xFAB << 12)
#define XOSC_STABLE          (1u << 31)

#define CLOCKS_BASE          0x40008000
#define CLK_REF_CTRL         *((volatile UINT32 *)(CLOCKS_BASE + 0x30))
#define CLK_REF_SELECTED     *((volatile UINT32 *)(CLOCKS_BASE + 0x38))
#define CLK_SYS_CTRL         *((volatile UINT32 *)(CLOCKS_BASE + 0x3C))
#define CLK_SYS_SELECTED     *((volatile UINT32 *)(CLOCKS_BASE + 0x44))
#define CLK_PERI_CTRL        *((volatile UINT32 *)(CLOCKS_BASE + 0x48))
#define CLK_PERI_ENABLE      (1u << 11)

#define CLK_REF_SRC_XOSC     2
#define CLK_SYS_SRC_REF      0
#define CLK_SYS_SRC_AUX      1
#define CLK_SYS_AUXSRC_PLL   (0u << 5)
#define CLK_PERI_AUXSRC_XOSC (4u << 5)

#define RESETS_CLR           *((volatile UINT32 *)(0x4000C000 + 0x3000))
#define RESETS_DONE          *((volatile UINT32 *)(0x4000C000 + 0x08))
#define RESETS_PLL_SYS_BIT   (1u << 12)

#define PLL_SYS_BASE         0x40028000
#define PLL_CS               *((volatile UINT32 *)(PLL_SYS_BASE + 0x00))
#define PLL_PWR              *((volatile UINT32 *)(PLL_SYS_BASE + 0x04))
#define PLL_FBDIV_INT        *((volatile UINT32 *)(PLL_SYS_BASE + 0x08))
#define PLL_PRIM             *((volatile UINT32 *)(PLL_SYS_BASE + 0x0C))
#define PLL_CS_LOCK          (1u << 31)
#define PLL_PWR_PD           (1u << 0)
#define PLL_PWR_POSTDIVPD    (1u << 3)
#define PLL_PWR_VCOPD        (1u << 5)


/* OSInitializeSystemClocks: Switches the reference, system and peripheral clocks onto the
** 12 MHz crystal. Must be called before anything that depends on time, which includes
** _OSInitializeTimer and the UART. */
void OSInitializeSystemClocks(void)
{
  /* Start the crystal and wait for it to settle. The startup delay is counted in batches
  ** of 256 cycles; 47 gives the millisecond recommended for a 12 MHz crystal. */
  XOSC_STARTUP = 47;
  XOSC_CTRL = XOSC_FREQ_RANGE_1_15 | XOSC_ENABLE;
  while ((XOSC_STATUS & XOSC_STABLE) == 0);
  /* Reference clock onto the crystal, then system clock onto the reference. Each switch
  ** is acknowledged by a bit of the SELECTED register. */
  CLK_REF_CTRL = CLK_REF_SRC_XOSC;
  while ((CLK_REF_SELECTED & (1u << CLK_REF_SRC_XOSC)) == 0);
  CLK_SYS_CTRL = CLK_SYS_SRC_REF;
  while ((CLK_SYS_SELECTED & (1u << CLK_SYS_SRC_REF)) == 0);
  /* Peripheral clock straight onto the crystal, so the UART keeps dividing a known 12 MHz
  ** whatever the system clock does afterwards. */
  CLK_PERI_CTRL = CLK_PERI_ENABLE | CLK_PERI_AUXSRC_XOSC;
  /* System clock to 125 MHz: the 12 MHz crystal multiplied by 125 gives a 1500 MHz VCO,
  ** divided by 6 then by 2. */
  RESETS_CLR = RESETS_PLL_SYS_BIT;
  while ((RESETS_DONE & RESETS_PLL_SYS_BIT) == 0);
  PLL_CS = 1;                      /* REFDIV = 1 */
  PLL_FBDIV_INT = 125;
  PLL_PWR &= ~(PLL_PWR_PD | PLL_PWR_VCOPD);
  while ((PLL_CS & PLL_CS_LOCK) == 0);
  PLL_PRIM = (6u << 16) | (2u << 12);
  PLL_PWR &= ~PLL_PWR_POSTDIVPD;
  /* Switch glitchlessly: park on the reference, select the PLL as auxiliary source, then
  ** take the auxiliary. */
  CLK_SYS_CTRL = CLK_SYS_SRC_REF;
  while ((CLK_SYS_SELECTED & (1u << CLK_SYS_SRC_REF)) == 0);
  CLK_SYS_CTRL = CLK_SYS_AUXSRC_PLL | CLK_SYS_SRC_REF;
  CLK_SYS_CTRL = CLK_SYS_AUXSRC_PLL | CLK_SYS_SRC_AUX;
  while ((CLK_SYS_SELECTED & (1u << CLK_SYS_SRC_AUX)) == 0);
} /* end of OSInitializeSystemClocks */
