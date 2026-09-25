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
/* File TestTimerEventF4.c: Shows how to use API Escapement_TimerEvent. This simple program
** periodically turns the flags on and then schedules an event to turn them off.
** Version date: March 2012
*/

#include "Escapement.h"
#include "Escapement_TimerEvent.h"
#include "BoardF4.h"


#define FLAG_PORT GPIOB
#define FLAG1_PIN PIN(13)
#define FLAG2_PIN PIN(14)

#define EVENT_TIMER_INDEX OS_IO_TIM14

static void InitializeFlags(UINT16 GPIO_Pin);
static void SetLed1Task(void *argument);
static void ClearLed1Task(void *argument);
static void SetLed2Task(void *argument);
static void ClearLed2Task(void *argument);


int main(void)
{
  void *tmp;
  #if ESCAPEMENT_TIMER == EVENT_TIMER_INDEX
     #error Event timer device must be different from the internal timer used by Escapement
  #endif
  /* Stop Escapement internal timer during debugger connection */
  BoardStopTimerInDebug(ESCAPEMENT_TIMER);
  /* Keep debugger connection during sleep mode */
  BoardKeepDebugInSleep();
  /* Stop event timer during debugger connection */
  BoardStopTimerInDebug(EVENT_TIMER_INDEX);
  /* Initialize Hardware */
  SystemInit();
  BoardInitClock();
  InitializeFlags(FLAG1_PIN | FLAG2_PIN);
  OSInitTimerEvent(2,83,1,0,EVENT_TIMER_INDEX);
  #if defined(ESCAPEMENT_VERSION_HARD)
     tmp = OSCreateEventDescriptor();
     OSCreateSynchronousTask(ClearLed1Task,1000,tmp,NULL);
     OSCreateTask(SetLed1Task,0,5000,5000,tmp);
     tmp = OSCreateEventDescriptor();
     OSCreateSynchronousTask(ClearLed2Task,1000,tmp,NULL);
     OSCreateTask(SetLed2Task,0,10000,10000,tmp);
  #elif defined(ESCAPEMENT_VERSION_SOFT)
     tmp = OSCreateEventDescriptor();
     OSCreateSynchronousTask(ClearLed1Task,0,1000,0,tmp,NULL);
     OSCreateTask(SetLed1Task,0,0,5000,5000,1,1,0,tmp);
     tmp = OSCreateEventDescriptor();
     OSCreateSynchronousTask(ClearLed2Task,0,2000,0,tmp,NULL);
     OSCreateTask(SetLed2Task,0,0,10000,10000,1,1,0,tmp);
  #endif
  /* Start the OS so that it starts scheduling the user tasks */
  return OSStartMultitasking(NULL,NULL);
} /* end of main */


/* InitializeFlags: Initialize input/output pin for flags.*/
void InitializeFlags(UINT16 GPIO_Pin)
{
  /* Configure the flag pins as push-pull outputs */
  BoardInitOutputs(GPIOB, GPIO_Pin);
} /* end of InitializeFlags */


/* SetLed1Task: Sets a LED and triggers its clear after 1000 clock ticks. */
void SetLed1Task(void *argument)
{
  FLAG_PORT->BSRR = FLAG1_PIN;
  OSScheduleTimerEvent(argument,1000,EVENT_TIMER_INDEX);
  OSEndTask();
} /* end of SetLed1Task */


/* ClearLed1Task: Clears the LED toggled by SetLed1Task(). */
void ClearLed1Task(void *argument)
{
  FLAG_PORT->BSRR = (UINT32)FLAG1_PIN << 16;
  OSSuspendSynchronousTask();
} /* end of ClearLed1Task */


/* SetLed2Task: Sets a LED and triggers its clear after 2000 clock ticks. */
void SetLed2Task(void *argument)
{
  FLAG_PORT->BSRR = FLAG2_PIN;
  OSScheduleTimerEvent(argument,2000,EVENT_TIMER_INDEX);
  OSEndTask();
} /* end of SetLed2Task */


/* ClearLed2Task: Clears the LED toggled by SetLed2Task(). */
void ClearLed2Task(void *argument)
{
  FLAG_PORT->BSRR = (UINT32)FLAG2_PIN << 16;
  OSSuspendSynchronousTask();
} /* end of ClearLed2Task */
