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
** Staying at the 12 MHz of the crystal, without engaging the PLL, is deliberate: it keeps
** this layer short, makes the microsecond tick exact, and leaves plenty of margin for the
** task sets used by the examples.
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
  /* Peripheral clock onto the system clock, so the UART divides a known 12 MHz. */
  CLK_PERI_CTRL = CLK_PERI_ENABLE;
} /* end of OSInitializeSystemClocks */
