/* Copyright (c) 2026 Bertrand Hurst. All rights reserved.
** Escapement - Lightweight Power-Aware Real-Time OS.
** Derived from prior work; see LICENSE and NOTICE at the root of this repository.
*/
/* File Escapement_Timer.h: Hardware abstract timer layer for the RP2040.
** Platform version: RP2040 (Raspberry Pi Pico).
*/

#ifndef ESCAPEMENT_TIMER_H
#define ESCAPEMENT_TIMER_H

/* The timer interrupt is not maskable by the kernel's critical sections; it only marks
** its cause and defers the work to a lower priority software interrupt. */
#define NonMaskableSoftwareTimer

void _OSInitializeTimer(void);
void _OSStartTimer(void);
INT32 _OSGetActualTime(void);
BOOL _OSTimerIsOverflow(INT32 shiftTimeLimit);
BOOL _OSSetTimer(INT32 nextArrivalTime);

#endif /* ESCAPEMENT_TIMER_H */
