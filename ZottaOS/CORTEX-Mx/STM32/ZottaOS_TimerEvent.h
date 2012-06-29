/* Copyright (c) 2012 MIS Institute of the HEIG-VD affiliated to the University of
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
/* File TimerEvent.h: Provides an API that transforms a timer device into a manager that
** schedules event-driven tasks.
** Platform version: All STM32 microcontrollers.
** Version date: April 2012
** Authors: MIS-TIC
*/

#ifndef _TIMEREVENT_
#define _TIMEREVENT_

/* OSInitTimerEvent: Creates an ISR descriptor block holding the specifics of a timer
** device that is used as an event handler and which can schedule a list of event at
** their occurrence time.
** Parameters:
**  (1) (UINT8) maximum number of pending events;
**  (2) (UINT8) index denoting the timer device;
**  (3) (UINT16) ;
**  (4) (UINT8) ;
**  (5) (UINT8) . */
#if defined(CORTEX_M3) || defined(CORTEX_M4)
void OSInitTimerEvent(UINT8 nbNode, UINT16 prescaler, UINT8 priority, UINT8 subpriority,
                      UINT8 interruptIndex);
#elif defined(CORTEX_M0)
void OSInitTimerEvent(UINT8 nbNode, UINT16 prescaler, UINT8 priority, UINT8 interruptIndex);
#endif

/* OSScheduleTimerEvent: Entry point to insert an event into the event list associated
** with a timer.
** Parameters:
**  (1) (void *) the event to insert;
**  (2) (UINT32) relative time of occurrence of the event, this value should be smaller
**              than 2^30.
**  (3) (UINT8) index denoting the timer device.
** Returned value: (BOOL) successfulness of the operation. */
BOOL OSScheduleTimerEvent(void *event, UINT32 delay, UINT8 interruptIndex);

/* OSUnScheduleTimerEvent: Entry point to remove the first occurrence of a specified
** event from the list associated with a timer.
** Parameters:
**  (1) (void *) the event to remove;
**  (2) (UINT8) index denoting the timer device.
** Returned value: (BOOL) successfulness of the operation. */
BOOL OSUnScheduleTimerEvent(void *event, UINT8 interruptIndex);

#endif /* _TIMEREVENT_ */
