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

#define OS_12MHZ_SPEED  0   /* crystal alone, PLL stopped */
#define OS_48MHZ_SPEED  1
#define OS_125MHZ_SPEED 2   /* nominal frequency of the RP2040 */

#define OS_MAX_SPEED    OS_125MHZ_SPEED

/* Switches the clock tree onto the 12 MHz crystal. To be called first, before the timer
** and before any peripheral whose rate depends on the clock. */
void OSInitializeSystemClocks(void);

#ifdef ESCAPEMENT_VERSION_HARD_PA
   void OSInitProcessorSpeed(void);
   void OSSetProcessorSpeed(UINT8 speed);
   void OSSetMinimalProcessorSpeed(UINT8 speed);
#endif

#endif /* ESCAPEMENT_PROCESSOR_H */
