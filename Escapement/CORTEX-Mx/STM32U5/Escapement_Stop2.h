/* Copyright (c) 2026 Bertrand Hurst. All rights reserved.
** Escapement - Lightweight Power-Aware Real-Time OS.
** Distributed under the terms of LICENSE at the root of this repository.
*/
/* File Escapement_Stop2.h: The idle task of the STM32U585 in Stop 2 rather than Sleep
** when the next event of the kernel is far enough off, LPTIM1 on the LSE waking it and
** counting the time TIM2, the kernel's clock, stood still (Escapement_Stop2.c).
** Platform version: STM32U585 (Arduino UNO Q), any STM32U5.
*/

#ifndef ESCAPEMENT_STOP2_H
#define ESCAPEMENT_STOP2_H

/* The time a wake-up takes, the HSE's start and PLL1's lock, which LPTIM1 wakes the chip
** that much before the next event; and the shortest wait slept in Stop 2, below which it
** would save little. */
#define OS_STOP2_WAKE_US 3000u
#define OS_STOP2_MIN_US  5000u

/* OSInitStop2: Starts LPTIM1 and lets the idle task enter Stop 2. To be called from main,
** after OSInitializeSystemClocks and before OSStartMultitasking; returns FALSE, the idle
** task left in Sleep, if the LSE does not run. The idle task enters Stop 2 only when no
** UART receives or sends (Escapement_UART.c), and wakes before the next timer event
** (Escapement_TimerEvent.c) as before the next arrival; any other clock the application
** uses stops with it. */
BOOL OSInitStop2(void);

/* OSAllowStop2: FALSE keeps the idle task in Sleep, as without OSInitStop2, until TRUE
** lets it enter Stop 2 again; TRUE after OSInitStop2. To compare the two on the same
** load (SleepU5). */
void OSAllowStop2(BOOL allowed);

/* The counts of the idle task, for the examples: the times it entered Stop 2, the largest
** wake-up it took, in ticks of LPTIM1, and the times it woke past the next event, whose
** kernel time it then set just before it. */
typedef struct OS_STOP2_COUNTS {
  UINT32 Entries;
  UINT32 WakeMaxTicks;
  UINT32 Late;
} OS_STOP2_COUNTS;

void OSGetStop2Counts(OS_STOP2_COUNTS *counts);

#endif /* ESCAPEMENT_STOP2_H */
