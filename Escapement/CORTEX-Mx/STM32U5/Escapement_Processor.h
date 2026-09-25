/* Copyright (c) 2026 Bertrand Hurst. All rights reserved.
** Escapement - Lightweight Power-Aware Real-Time OS.
** Distributed under the terms of LICENSE at the root of this repository.
*/
/* File Escapement_Processor.h: Clock set-up of the STM32U575. The kernel's timer counts
** microseconds through a prescaler set for the system clock this file chooses, so the
** clock is set once, first, and not changed afterwards. The power-aware kernel is not
** ported yet: its DVFS driver would need the voltage ranges and the SMPS of this chip.
** Platform version: STM32U575 (NUCLEO-U575ZI-Q).
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

#endif /* ESCAPEMENT_PROCESSOR_H */
