/* Copyright (c) 2026 Bertrand Hurst. All rights reserved.
** Escapement - Lightweight Power-Aware Real-Time OS.
** Derived from prior work; see LICENSE and NOTICE at the root of this repository.
*/
/* File Escapement_Processor.h: Core frequency levels used by the power-aware variant.
** Contrary to the STM32, the RP2040 timer is fed by a tick independent of the core clock:
** changing the frequency does not move the kernel's time base.
** Platform version: RP2040 (Raspberry Pi Pico).
*/

#ifndef ESCAPEMENT_PROCESSOR_H
#define ESCAPEMENT_PROCESSOR_H

#include "Escapement_Config.h"
#include "Escapement_CortexMx.h"

/* Operating points of the power-aware variant, slowest first. The system PLL stays locked
** at 1500 MHz whichever is selected: moving between them only changes a post divider or
** the source of clk_sys, so none waits for the PLL to lock again. */
#define OS_12MHZ_SPEED  0   /* clk_sys on the crystal, through clk_ref */
#define OS_50MHZ_SPEED  1   /* 1500 MHz / 6 / 5 */
#define OS_125MHZ_SPEED 2   /* 1500 MHz / 6 / 2, nominal frequency of the RP2040 */

#define OS_MAX_SPEED    OS_125MHZ_SPEED

/* Switches the clock tree onto the 12 MHz crystal. To be called first, before the timer
** and before any peripheral whose rate depends on the clock. */
void OSInitializeSystemClocks(void);

#ifdef ESCAPEMENT_VERSION_HARD_PA
   /* OSInitProcessorSpeed: Sets the core voltage of the fastest operating point, which
   ** OSInitializeSystemClocks has selected, and with ESCAPEMENT_RP2040_UNDERVOLT lowers the
   ** brown-out detection threshold. To be called after OSInitializeSystemClocks. */
   void OSInitProcessorSpeed(void);
   /* OSGetProcessorSpeed: Returns the operating point in effect, one of OS_xxMHZ_SPEED. */
   UINT8 OSGetProcessorSpeed(void);
   void OSSetProcessorSpeed(UINT8 speed);
   void OSSetMinimalProcessorSpeed(UINT8 speed);
#endif

#endif /* ESCAPEMENT_PROCESSOR_H */
