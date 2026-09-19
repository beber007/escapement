/* Copyright (c) 2026 Bertrand Hurst. All rights reserved.
** Escapement - Lightweight Power-Aware Real-Time OS.
** Derived from prior work; see LICENSE and NOTICE at the root of this repository.
*/
/* File Escapement_Timer.h: Defines the interface between the hardware timer and the Escapement
**                       family of real-time kernels.
** Version identifier: June 2011
*/

#ifndef ESCAPEMENT_TIMER_H
#define ESCAPEMENT_TIMER_H

/* Keeping track of time and performing all its related events is a tricky business. In
** Escapement we divide these 2 actions in two. The first only keeps track of the time (the
** wall clock) with a dedicated hardware timer and is implemented by an ISR. The second
** part, which is triggered by the hardware timer ISR, takes care of all the actions that
** need to be done when the wall clock progresses. This includes processing the instance
** arrivals that occur at the current time, preventing temporal variable overflow and
** programming the next hardware timer interrupt. This second part is implemented as a
** software ISR and it may be triggered any time and as often as there is an event to
** process.
** This scheme provides 2 main advantages:
** (1) The latency associated with the time events can be kept to a minimal (18 machine
**     cycles for MSP430) as only the keeping of time executes in a critical section.
** (2) The scheme can be ported to any microcontroller independently of whether it has
**     prioritized interrupts or not. */

/* _OSInitializeTimer: Sets up the hardware timer to count in up mode without starting
** it. After the call, the timer is ready to produce an interrupt as soon as it is order-
** ed to start counting and at the first interrupt, the current wall clock is set to 0. */
void _OSInitializeTimer(void);

/* _OSStartTimer: Orders the hardware timer to begin counting. */
void _OSStartTimer(void);

/* _OSGenerateSoftTimerInterrupt: Provokes a software timer interrupt. This function is
** called when an event occurs and should be scheduled. */
void _OSGenerateSoftTimerInterrupt(void);

/* _OSSetTimer: Sets the timer to interrupt at a specified value. This function is called
** by the software timer ISR to set the next time event.
** Parameter: (INT32) nextTimeInterval: time duration until the next hardware timer in-
**                    terrupt, i.e. the time of the next event minus the current time. */
void _OSSetTimer(INT32 nextTimeInterval);

#if ESCAPEMENT_VERSION == ESCAPEMENT_VERSION_HARD_PA && POWER_MANAGEMENT != NONE
   /* _OSGetTimerCounter: Return the current value of the timer counter. */
   UINT16 _OSGetTimerCounter(void);
#endif

#endif /* ESCAPEMENT_TIMER_H */
