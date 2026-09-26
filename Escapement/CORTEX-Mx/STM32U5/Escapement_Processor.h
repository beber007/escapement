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
/* File Escapement_Processor.h: Clock set-up of the STM32U575. The kernel's timer counts
** microseconds through a prescaler set for the system clock this file chooses, so the
** clock is set once, first, and not changed afterwards. The power-aware kernel is not
** ported yet: its DVFS driver would need the voltage ranges and the SMPS of this chip.
** Platform version: STM32U585 (Arduino UNO Q), any STM32U5.
*/

#ifndef ESCAPEMENT_PROCESSOR_H
#define ESCAPEMENT_PROCESSOR_H

#include "Escapement_Config.h"
#include "Escapement_CortexMx.h"

/* _OSMemoryBarrier: Orders the accesses of the slot buffers where their models say it
** must, as on the RP2350: the STM32U575 has one core, but a DMB costs little and keeps the
** buffers correct against a bus master that reads them (Armv8-M Architecture Reference
** Manual, B7.2.11); the memory clobber keeps the compiler from moving them across. */
#define _OSMemoryBarrier() __asm volatile ("DMB" ::: "memory")

#ifdef ESCAPEMENT_VERSION_HARD_PA
   #error "the power-aware kernel is not ported to the STM32U5 yet (docs/roadmap.md)"
#endif

/* The system clock OSInitializeSystemClocks sets, which the timer and the UART derive
** their rates from. */
#define OS_SYSTEM_CLOCK_HZ 160000000u

/* Takes the system clock from the 4 MHz MSIS left by reset to 160 MHz through PLL1. To be
** called first, before the timer and before any peripheral whose rate depends on the
** clock. */
void OSInitializeSystemClocks(void);

/* OSGetMSIRelocks: Returns the times the MSIS, having left its lock on the LSE, was locked
** again (erratum 2.2.27 of the chip, Escapement_Processor.c). */
UINT32 OSGetMSIRelocks(void);

#endif /* ESCAPEMENT_PROCESSOR_H */
