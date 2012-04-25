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
*/
/* File ZottaOSHard_Timer.h: Defines the interface between the hardware timer and
**                           ZottaOS-Hard.
** Platform version: All STM32 microcontrollers.
** Version date: March 2012
** Authors: MIS-TIC
*/

#ifndef ZOTTAOS_TIMER_H
#define ZOTTAOS_TIMER_H

/* Keeping track of time and performing all its related events is a tricky business. In
** ZottaOS we divide these 2 actions in two. The first only keeps track of the time (the
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

/* OSGetActualTime: Retrieve the current time.
** Returned value: (INT32) current time.
** This function is defined as INT32 OSGetActualTime(void) and is defined in the user API
** for kernel version. */

/* _OSSetTimer: Sets the timer to interrupt at a specified value. This function is called
** by the software timer ISR to set the next time event.
** Parameter: (INT32) nextArrival: . */
void _OSSetTimer(INT32 nextArrival);

/* _OSTimerShift: Shifts the global Time variable.
** Parameter: (INT32) shiftTimeLimit: The value will be subtract to the wall clock This is
** a value greater than 2^16. */
void _OSTimerShift(INT32 shiftTimeLimit);

#define TIMER_ISR_TAB_SIZE 2

typedef struct TIMERSELECT {
   void (*TimerSelector)(struct TIMERSELECT *);
   void *TimerISRTab[TIMER_ISR_TAB_SIZE];
   UINT32 BaseRegisterTab[TIMER_ISR_TAB_SIZE];
   UINT16 InterruptBit;
} TIMERSELECT;

void _OSTimerSelector(struct TIMERSELECT *timerSelect);

#endif /* ZOTTAOS_TIMER_H */
