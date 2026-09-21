/* Copyright (c) 2026 Bertrand Hurst. All rights reserved.
** Escapement - Lightweight Power-Aware Real-Time OS.
** Derived from prior work; see LICENSE and NOTICE at the root of this repository.
*/
/* File Escapement_Timer.h: Simulated timer of the host test build. The clock is a plain
** variable that the test advances; nothing runs in the background.
*/

#ifndef ESCAPEMENT_TIMER_H
#define ESCAPEMENT_TIMER_H

#define NonMaskableSoftwareTimer

void _OSInitializeTimer(void);
void _OSStartTimer(void);
INT32 _OSGetActualTime(void);
BOOL _OSTimerIsOverflow(INT32 shiftTimeLimit);
BOOL _OSSetTimer(INT32 nextArrivalTime);

/* Driven by the test: advances the clock to the armed deadline and returns the time it
** reached, or -1 when no deadline is armed. */
INT32 HostTicksToNextEvent(void);
INT32 HostClockNow(void);

#endif /* ESCAPEMENT_TIMER_H */
