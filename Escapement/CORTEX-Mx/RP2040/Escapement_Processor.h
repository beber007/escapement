/* Copyright (c) 2006-2012 MIS Institute of the HEIG-VD affiliated to the University of
** Applied Sciences of Western Switzerland. All rights reserved.
** Permission to use, copy, modify, and distribute this software and its documentation
** for any purpose, without fee, and without written agreement is hereby granted, pro-
** vided that the above copyright notice, the following three sentences and the authors
** appear in all copies of this software and in the software where it is used.
** IN NO EVENT SHALL THE MIS INSTITUTE NOR THE HEIG-VD NOR THE UNIVERSITY OF APPLIED
** SCIENCES OF WESTERN SWITZERLAND BE LIABLE TO ANY PARTY FOR DIRECT, INDIRECT, SPECIAL,
** INCIDENTAL, OR CONSEQUENTIAL DAMAGES ARISING OUT OF THE USE OF THIS SOFTWARE AND ITS
** DOCUMENTATION, EVEN IF THE MIS INSTITUTE OR THE HEIG-VD OR THE UNIVERSITY OF APPLIED
** SCIENCES OF WESTERN SWITZERLAND HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
** THE MIS INSTITUTE, THE HEIG-VD AND THE UNIVERSITY OF APPLIED SCIENCES OF WESTERN SWIT-
** ZERLAND SPECIFICALLY DISCLAIMS ANY WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
** IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE. THE SOFT-
** WARE PROVIDED HEREUNDER IS ON AN "AS IS" BASIS, AND THE MIS INSTITUTE NOR THE HEIG-VD
** AND NOR THE UNIVERSITY OF APPLIED SCIENCES OF WESTERN SWITZERLAND HAVE NO OBLIGATION
** TO PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR MODIFICATIONS.
** Authors: MIS-TIC
**
** Escapement - Lightweight Power-Aware Real-Time OS, derived from ZottaOS.
** Modifications Copyright (c) 2026 Bertrand Hurst, distributed under the same terms;
** see LICENSE and NOTICE at the root of this repository.
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
