/* Copyright (c) 2026 Bertrand Hurst. All rights reserved.
** Escapement - Lightweight Power-Aware Real-Time OS.
** Derived from prior work; see LICENSE and NOTICE at the root of this repository.
*/
/* File TestTimerEventL1.c: Shows how to use API Escapement_TimerEvent. This simple program
** periodically turns LEDS on and then schedules an event to turn them off.
** Version identifier: May 2012
*/

#include "Escapement.h"
#include "Escapement_TimerEvent.h"
#include "BoardL1.h"


#define FLAG_PORT GPIOB
#define FLAG1_PIN PIN(13)
#define FLAG2_PIN PIN(14)

#define EVENT_TIMER_INDEX OS_IO_TIM4

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
  /* Initialize Hardware */
  SystemInit();
  BoardInitClock();
  InitializeFlags(FLAG1_PIN | FLAG2_PIN);
  OSInitTimerEvent(2,83,0,0,EVENT_TIMER_INDEX);
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
     OSCreateTask(SetLed1Task,0,0,10000,10000,1,1,0,tmp);
     tmp = OSCreateEventDescriptor();
     OSCreateSynchronousTask(ClearLed2Task,0,2000,0,tmp,NULL);
     OSCreateTask(SetLed2Task,0,0,20000,20000,1,1,0,tmp);
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
