/* Copyright (c) 2026 Bertrand Hurst. All rights reserved.
** Escapement - Lightweight Power-Aware Real-Time OS.
** Derived from prior work; see LICENSE and NOTICE at the root of this repository.
*/
/* File Escapement_Timer.h: Defines the interface between the hardware timer and Escapement.
** Platform version: All STM32 microcontrollers.
** Version date: March 2012
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
** (1) The latency associated with the time events can be kept to a minimal as only the
**     keeping of time executes in a critical section.
** (2) The scheme can be ported to any microcontroller independently of whether it has
**     prioritized interrupts or not. */

/* Must be defined if the platform does not offer the option to disable the software
** interrupt timer. Used only prior to starting task scheduling. */
#define NonMaskableSoftwareTimer

/* _OSInitializeTimer: Sets up the hardware timer to count in up mode without starting
** it. After the call, the timer is ready to produce an interrupt as soon as it is order-
** ed to start counting and at the first interrupt, the current wall clock is set to 0. */
void _OSInitializeTimer(void);

/* _OSStartTimer: Orders the hardware timer to begin counting. */
void _OSStartTimer(void);

/* _OSGetActualTime: Returns the current value of the wall clock.
** Returned value: (INT32) current time. */
INT32 _OSGetActualTime(void);

/* _OSTimerIsOverflow: return true if a timer overflow occurs. */
BOOL _OSTimerIsOverflow(INT32 shiftTimeLimit);

/* _OSSetTimer: Sets the timer to interrupt at a specified value. This function is called
** by the software timer ISR to set the next time event.
** Parameter: (INT32) nextArrivalTime: next compare register time interrupt. This is the
**                    earliest time at which the timer ISR should again acquire the proc-
**                    essor so that it can schedule the next task arrival.
** Returned value: (BOOL) true if the compare register has been correctly assigned, and
** false if the nextArrivalTime has been passed and missed. In this last case, the ISR
** should consider that the event has occurred and process it. */
BOOL _OSSetTimer(INT32 nextArrivalTime);

#endif /* ESCAPEMENT_TIMER_H */
