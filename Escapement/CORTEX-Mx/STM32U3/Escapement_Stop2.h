/* Copyright (c) 2026 Bertrand Hurst. All rights reserved.
** Escapement - Lightweight Power-Aware Real-Time OS.
** Distributed under the terms of LICENSE at the root of this repository.
*/
/* File Escapement_Stop2.h: The idle task of the STM32U385 in Stop 2 rather than Sleep
** when the next event of the kernel is far enough off, LPTIM1 on the LSE waking it and
** counting the time TIM2, the kernel's clock, stood still (Escapement_Stop2.c). The
** interface of the STM32U5 port's.
** Platform version: STM32U385 (NUCLEO-U385RG-Q), any STM32U375/385.
*/

#ifndef ESCAPEMENT_STOP2_H
#define ESCAPEMENT_STOP2_H

/* The time a wake-up takes, the regulator, the MSI and its lock on the LSE, the voltage
** range and the booster, which LPTIM1 wakes the chip that much before the next event; and
** the shortest wait slept in Stop 2, below which it would save little. Both in ticks of
** TIM2, which SleepWrapU3 scales (Makefile). The margin is the U5's, kept until the board
** has measured this chip's wake-up: the wait for the lock alone may take 1.95 ms
** (OS_STOP2_LOCK_TICKS). */
#ifndef OS_STOP2_WAKE_US
   #define OS_STOP2_WAKE_US 3000u
#endif
#ifndef OS_STOP2_MIN_US
   #define OS_STOP2_MIN_US  5000u
#endif

/* The part of that margin kept for raising the clock, the voltage range and the booster:
** the rest, once the MSI is locked, is slept in Sleep on the clock the chip wakes on,
** rather than on the system clock (Escapement_Stop2.c, SlowSleep). In ticks of TIM2 too. */
#ifndef OS_STOP2_RAISE_US
   #define OS_STOP2_RAISE_US 500u
#endif

/* The most the wake-up waits for the MSI's PLL mode to lock again, in ticks of LPTIM1:
** 64, 1.95 ms, against a tSTAB of some 0.8 ms in the datasheet (DS14830, not read again).
** Past it the clock is raised on the MSI running free, counted in LockMissed below. */
#ifndef OS_STOP2_LOCK_TICKS
   #define OS_STOP2_LOCK_TICKS 64u
#endif

/* The window after a byte received on LPUART1, or a wake-up it caused, during which the
** idle task stays in Sleep, so that a client may wake the chip with one byte and send its
** message at 115,200 baud once the clock is up (docs/roadmap.md, item 6). It covers the
** wake-up, the jitter of the sender and the gaps in a message. In microseconds, counted
** on LPTIM1: SleepWrapU3 leaves it unscaled, which LPTIM1's 16 bits would not hold. */
#ifndef OS_STOP2_LINK_WINDOW_US
   #define OS_STOP2_LINK_WINDOW_US 20000u
#endif

/* OSInitStop2: Starts LPTIM1 and lets the idle task enter Stop 2. To be called from main,
** after OSInitializeSystemClocks and before OSStartMultitasking; returns FALSE, the idle
** task left in Sleep, if the LSE does not run. The idle task enters Stop 2 only when no
** UART sends and USART1 does not receive (Escapement_UART.c), LPUART1 receiving through
** it, the window above passed, and wakes before the next timer event
** (Escapement_TimerEvent.c) as before the next arrival; any other clock the application
** uses stops with it. */
BOOL OSInitStop2(void);

/* OSAllowStop2: FALSE keeps the idle task in Sleep, as without OSInitStop2, until TRUE
** lets it enter Stop 2 again; TRUE after OSInitStop2. To compare the two on the same
** load (SleepU3). */
void OSAllowStop2(BOOL allowed);

/* OSSetStop2Wake: How long before the next event LPTIM1 wakes the chip, in ticks of TIM2,
** OS_STOP2_WAKE_US until then. A wake-up that takes longer comes late (the counts below).
** For a board whose wake-up is known, or to price the margin against the time spent in
** Sleep after it (SleepU3 built with WAKE=). Under OS_STOP2_MIN_US, which stays fixed. */
void OSSetStop2Wake(UINT32 micros);

/* The counts of the idle task, for the examples: the times it entered Stop 2, the largest
** wake-up it took, in ticks of LPTIM1, the lock and the clock's raise, the sleep between
** them left out, the times it woke past the next event, whose kernel time it then set
** just before it, the wake-ups whose lock did not come within OS_STOP2_LOCK_TICKS, the
** MSIS then running free until it did, the sleeps held in Sleep by the window of LPUART1
** when Stop 2 was otherwise due, and the wake-ups that slept part of their margin. The
** U5's, with the lock in place of the HSE. */
typedef struct OS_STOP2_COUNTS {
  UINT32 Entries;
  UINT32 WakeMaxTicks;
  UINT32 Late;
  UINT32 LockMissed;
  UINT32 LinkHeld;
  UINT32 Slow;
} OS_STOP2_COUNTS;

void OSGetStop2Counts(OS_STOP2_COUNTS *counts);

#endif /* ESCAPEMENT_STOP2_H */
