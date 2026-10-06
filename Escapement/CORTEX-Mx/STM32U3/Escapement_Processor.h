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
/* File Escapement_Processor.h: Clock set-up of the STM32U385. The kernel's timer counts
** microseconds through a prescaler set for the system clock this file chooses, so the
** clock is set once, first, and not changed afterwards, but for its restart on waking
** from Stop 2 (Escapement_Stop2.c), TIM2 stopped meanwhile. The power-aware kernel is not
** ported yet: its DVFS driver would have to rescale TIM2 at each change of speed (TIM2
** follows HCLK on this chip, RM0487 rev. 3, 10.2.3, "Timer clock", p. 408).
** Platform version: STM32U385 (NUCLEO-U385RG-Q), any STM32U375/385.
*/

#ifndef ESCAPEMENT_PROCESSOR_H
#define ESCAPEMENT_PROCESSOR_H

#include "Escapement_Config.h"
#include "Escapement_CortexMx.h"

/* _OSMemoryBarrier: Orders the accesses of the slot buffers where their models say it
** must, as on the RP2350 and the STM32U5: one core here, but a DMB costs little and keeps
** the buffers correct against a bus master that reads them (Armv8-M Architecture
** Reference Manual, B7.2.11); the memory clobber keeps the compiler from moving them. */
#define _OSMemoryBarrier() __asm volatile ("DMB" ::: "memory")

#ifdef ESCAPEMENT_VERSION_HARD_PA
   #error "the power-aware kernel is not ported to the STM32U3 yet (docs/stm32u3.md)"
#endif

/* The system clock OSInitializeSystemClocks sets, which the timers and USART1 derive their
** rates from: 96 MHz, the maximum of the chip, or 48, 24 or 12 (make MHZ=), each in the
** lowest voltage range that runs it (Escapement_Processor.c). Locked on the LSE, the MSIS
** runs 107 ppm above each (RM0487, table 102, p. 403). */
#ifndef OS_SYSTEM_CLOCK_HZ
   #define OS_SYSTEM_CLOCK_HZ 96000000u
#endif

/* Takes the system clock to OS_SYSTEM_CLOCK_HZ on the MSIS, from MSIRC0 locked on the
** 32.768 kHz crystal, or running free should the crystal not start. To be called first,
** before the timer and before any peripheral whose rate depends on the clock. */
void OSInitializeSystemClocks(void);

/* _OSRaiseSystemClock: The second half of OSInitializeSystemClocks, from the MSIS in range
** 2 to OS_SYSTEM_CLOCK_HZ: the booster, range 1, the flash's wait states and the MSIS's
** divider, each as the registers read it. Escapement_Stop2.c calls it again on waking from
** Stop 2, after _OSWaitMSILock, which waits for the MSI's PLL mode to lock again, at most
** ticks of the caller's clock, and returns TRUE if it did not. */
void _OSRaiseSystemClock(void);
BOOL _OSWaitMSILock(UINT16 (*clock)(void), UINT16 ticks);

/* The idle task sleeps through _OSIdleHook once OSInitStop2 has set it, so that an image
** without Stop 2 links neither it nor LPTIM1; the hook returns with interrupts enabled. */
extern void (*_OSIdleHook)(void);
#undef _OSSleep
#define _OSSleep() while (TRUE) { \
                      if (_OSIdleHook != NULL) \
                         _OSIdleHook(); \
                      else \
                         __asm volatile ("WFI" ::: "memory"); \
                   };

/* OSGetMSIRelocks: Returns the times the MSI, having left its PLL mode on the LSE, was put
** back in it (Escapement_Processor.c). */
UINT32 OSGetMSIRelocks(void);

/* OSMSILocked: TRUE once the MSI is in its PLL mode on the LSE; FALSE if the LSE did not
** start, the MSIS then running free, within about 1 % of its frequency. */
BOOL OSMSILocked(void);

#endif /* ESCAPEMENT_PROCESSOR_H */
