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
/* File Escapement_TimerEvent.h: Turns an alarm of TIMER0 of the RP2350 into a manager that
** wakes event-driven tasks after a delay. Same entry points as on the STM32, except that
** the delays count the microseconds of the timer, which has no prescaler.
** Platform version: RP2350 (Raspberry Pi Pico 2).
*/

#ifndef _TIMEREVENT_
#define _TIMEREVENT_

/* OSInitTimerEvent: Creates the descriptor of an alarm used as an event manager, able to
** hold a number of pending events. To be called from main, before OSStartMultitasking.
** Parameters:
**  (1) (UINT8) nbNode: maximum number of pending events;
**  (2) (UINT8) priority: priority level of the alarm interrupt, 0 to 13 (14 and 15 are
**      the kernel's own);
**  (3) (UINT16) interruptIndex: OS_IO_TIMER_2 or OS_IO_TIMER_3, the kernel using alarms
**      0 and 1. */
void OSInitTimerEvent(UINT8 nbNode, UINT8 priority, UINT16 interruptIndex);

/* OSScheduleTimerEvent: Wakes the event-driven task waiting on an event after a delay.
** Parameters:
**  (1) (void *) the event, as returned by OSCreateEventDescriptor;
**  (2) (UINT32) delay in microseconds, smaller than 2^30;
**  (3) (UINT16) index of the alarm, as given to OSInitTimerEvent.
** Returned value: (BOOL) FALSE when all the nodes are in use. */
BOOL OSScheduleTimerEvent(void *event, UINT32 delay, UINT16 interruptIndex);

/* OSUnScheduleTimerEvent: Removes the first pending occurrence of an event.
** Parameters:
**  (1) (void *) the event to remove;
**  (2) (UINT16) index of the alarm.
** Returned value: (BOOL) FALSE when the event was not pending. */
BOOL OSUnScheduleTimerEvent(void *event, UINT16 interruptIndex);

#endif /* _TIMEREVENT_ */
