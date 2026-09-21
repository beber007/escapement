/* Copyright (c) 2026 Bertrand Hurst. All rights reserved.
** Escapement - Lightweight Power-Aware Real-Time OS.
** Derived from prior work; see LICENSE and NOTICE at the root of this repository.
*/
/* File TestTimerEventL1b.c: Shows how to use API Escapement_TimerEvent. This program is
** based on TestTimerEventL1.c but uses a single event-driven task per LED and shows
** how to initiate 2 events.
** Version identifier: June 2012
*/

#include "Escapement.h"
#include "Escapement_TimerEvent.h"
#include "BoardL1.h"


#define FLAG_PORT GPIOB
#define FLAG1_PIN PIN(13)
#define FLAG2_PIN PIN(14)

#define EVENT_TIMER_INDEX OS_IO_TIM4


void InitializeFlags(UINT16 GPIO_Pin);
void InitApplication(void *argument);
void ToggleLed1Task(void *argument);
void ToggleLed2Task(void *argument);

int main(void)
{
  void *event[2];
  #if ESCAPEMENT_TIMER == EVENT_TIMER_INDEX
     #error Event timer device must be different from the internal timer used by Escapement
  #endif
  /* Stop timer during debugger connection */
  BoardStopTimerInDebug(ESCAPEMENT_TIMER);
  /* Keep debugger connection during sleep mode */
  BoardKeepDebugInSleep();
  /* Initialize Hardware */
  SystemInit();
  InitializeFlags(FLAG1_PIN | FLAG2_PIN);
  /* Define and start the event handlers */
  OSInitTimerEvent(2,31,0,0,EVENT_TIMER_INDEX);
  event[0] = OSCreateEventDescriptor();
  event[1] = OSCreateEventDescriptor();
  #if defined(ESCAPEMENT_VERSION_HARD)
     OSCreateSynchronousTask(ToggleLed1Task,1000,event[0],event[0]);
     OSCreateSynchronousTask(ToggleLed2Task,100,event[1],event[1]);
  #elif defined(ESCAPEMENT_VERSION_SOFT)
     OSCreateSynchronousTask(ToggleLed1Task,0,1000,0,event[0],event[0]);
     OSCreateSynchronousTask(ToggleLed2Task,0,100,0,event[1],event[1]);
  #endif
  /* Start the OS so that it starts scheduling the user tasks */
  return OSStartMultitasking(InitApplication,event);
} /* end of main */


/* InitializeFlags: Initialize input/output pin for flags.*/
void InitializeFlags(UINT16 GPIO_Pin)
{
  /* Configure the flag pins as push-pull outputs */
  BoardInitOutputs(GPIOB, GPIO_Pin);
} /* end of InitializeFlags */


/* InitApplication: Triggers event-driven tasks ToggleLed1Task and ToggleLed2Task. */
void InitApplication(void *argument)
{
  void **event = (void **)argument;
  OSScheduleSuspendedTask(event[0]);
  OSScheduleSuspendedTask(event[1]);
} /* end of InitApplication */


/* ToggleLed1Task: Sets a LED every 5000 clock ticks and then clears it after 1000 clock
** ticks. */
void ToggleLed1Task(void *argument)
{
  static UINT8 state = 0;
  switch (state) {
     case 0:
        FLAG_PORT->BSRRL = FLAG1_PIN;
        OSScheduleTimerEvent(argument,1000,EVENT_TIMER_INDEX);
        break;
     case 1:
     default:
        FLAG_PORT->BSRRH = FLAG1_PIN;
        OSScheduleTimerEvent(argument,4000,EVENT_TIMER_INDEX);
  }
  state = !state;
  OSSuspendSynchronousTask();
} /* end of ToggleLed1Task */


/* ToggleLed2Task: Sets a LED every 10000 clock ticks and triggers its clearing after a
** variable delay comprised between 1000 and 9000 clock ticks. */
void ToggleLed2Task(void *argument)
{
  static UINT16 delay = 1000;
  static UINT8 state = 0;
  switch (state) {
     case 0:
        FLAG_PORT->BSRRL = FLAG2_PIN;
        OSScheduleTimerEvent(argument,delay,EVENT_TIMER_INDEX);
        delay += 100;
        if (delay > 9000)
           delay = 1000;
        break;
     case 1:
     default:
        FLAG_PORT->BSRRH = FLAG2_PIN;
        OSScheduleTimerEvent(argument,10000-delay,EVENT_TIMER_INDEX);
  }
  state = !state;
  OSSuspendSynchronousTask();
} /* end of ToggleLed2Task */
