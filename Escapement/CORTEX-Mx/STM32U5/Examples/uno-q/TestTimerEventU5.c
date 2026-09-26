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
/* File TestTimerEventU5.c: Shows how to use Escapement_TimerEvent. Transposition of
** TestTimerEventPico2.c: two periodic tasks each raise an output and schedule an event on
** TIM5, which wakes an event-driven task that lowers it.
**
** LED3 (PH11) lights every 5 ms for 1 ms, LED4 (PH15) every 10 ms for 2 ms. The period of
** each output is the kernel's timer at work, its high time the event manager's, and both
** show that each event-driven task ran when it was woken.
** Platform version: STM32U585 (Arduino UNO Q).
*/

#include "Escapement.h"
#include "BoardU5.h"
#include "Escapement_TimerEvent.h"


#define EVENT_TIMER_INDEX OS_IO_TIM5

static void SetLed1Task(void *argument);
static void ClearLed1Task(void *argument);
static void SetLed2Task(void *argument);
static void ClearLed2Task(void *argument);



int main(void)
{
  void *event;
  OSInitializeSystemClocks();
  InitializeFlag(FLAG1_PIN);
  InitializeFlag(FLAG2_PIN);
  /* Two pending events at most, TIM5 at the priority of the application. */
  OSInitTimerEvent(2,1,EVENT_TIMER_INDEX);
  /* Each periodic task passes the event it schedules as its argument. */
  #if defined(ESCAPEMENT_VERSION_SOFT)
     event = OSCreateEventDescriptor();
     OSCreateSynchronousTask(ClearLed1Task,0,1000,0,event,NULL);
     OSCreateTask(SetLed1Task,0,0,5000,5000,1,1,0,event);
     event = OSCreateEventDescriptor();
     OSCreateSynchronousTask(ClearLed2Task,0,2000,0,event,NULL);
     OSCreateTask(SetLed2Task,0,0,10000,10000,1,1,0,event);
  #else
     event = OSCreateEventDescriptor();
     OSCreateSynchronousTask(ClearLed1Task,1000,event,NULL);
     OSCreateTask(SetLed1Task,0,5000,5000,event);
     event = OSCreateEventDescriptor();
     OSCreateSynchronousTask(ClearLed2Task,2000,event,NULL);
     OSCreateTask(SetLed2Task,0,10000,10000,event);
  #endif
  return OSStartMultitasking(NULL,NULL);
} /* end of main */



/* SetLed1Task: Lights LED3 and has it put out 1000 us later. */
static void SetLed1Task(void *argument)
{
  SetPin(FLAG1_PIN);
  OSTrace(OS_TRACE_MARK,PIN_NUMBER(FLAG1_PIN),0);
  OSScheduleTimerEvent(argument,1000,EVENT_TIMER_INDEX);
  OSEndTask();
} /* end of SetLed1Task */


/* ClearLed1Task: Lowers the output SetLed1Task raised. */
static void ClearLed1Task(void *argument)
{
  ClearPin(FLAG1_PIN);
  OSTrace(OS_TRACE_MARK,PIN_NUMBER(FLAG1_PIN),1);
  OSSuspendSynchronousTask();
} /* end of ClearLed1Task */


/* SetLed2Task: Lights LED4 and has it put out 2000 us later. */
static void SetLed2Task(void *argument)
{
  SetPin(FLAG2_PIN);
  OSTrace(OS_TRACE_MARK,PIN_NUMBER(FLAG2_PIN),0);
  OSScheduleTimerEvent(argument,2000,EVENT_TIMER_INDEX);
  OSEndTask();
} /* end of SetLed2Task */


/* ClearLed2Task: Lowers the output SetLed2Task raised. */
static void ClearLed2Task(void *argument)
{
  ClearPin(FLAG2_PIN);
  OSTrace(OS_TRACE_MARK,PIN_NUMBER(FLAG2_PIN),1);
  OSSuspendSynchronousTask();
} /* end of ClearLed2Task */
