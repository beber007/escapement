/* Copyright (c) 2026 Bertrand Hurst. All rights reserved.
** Escapement - Lightweight Power-Aware Real-Time OS.
** Derived from prior work; see LICENSE and NOTICE at the root of this repository.
*/
/* File TestTimerEvent2.c: Shows how to use API Escapement_TimerEvent. This program is based
** on TestTimerEvent.c but uses a single event-driven task per LED and shows how to ini-
** tiate 2 events.
** Prior to using the Escapement_TimerEvent API, you should run EscapementConf.exe to define a
** timer with two interrupt sources (one for the overflow and its corresponding capture
** compare register 1, e.g. OS_IO_TIMER1_A1_TA and OS_IO_TIMER1_A1_CC1 for Timer1 A), and
** also a port pin interrupt to act as a software interrupt (e.g. on port 1 pin 6 defined
** as OS_IO_PORT1_6).
** Version identifier: June 2012
*/

#include "Escapement.h"
#include "Escapement_TimerEvent.h"


void InitApplication(void *argument);
void ToggleLed1Task(void *argument);
void ToggleLed2Task(void *argument);

#define SetFlag(GPIO_Pin) P1OUT |= GPIO_Pin;
#define ClearFlag(GPIO_Pin) P1OUT &= ~GPIO_Pin;

int main(void)
{
  void *event[2];

  WDTCTL = WDTPW + WDTHOLD;  // Disable watchdog timer

  /* Initialize output I/O ports */
  P1SEL = 0x00;    // Set for GPIO
  P1DIR = 0x03;    // Set to output
  P1OUT = 0x00;    // Initially start at low

  /* Define and start the event handlers */
  if (!OSInitTimerEvent(2,OS_IO_PORT1_6,OS_IO_TIMER1_A1_TA))
     while (TRUE); // Initialization problem

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
        SetFlag(1);
        OSScheduleTimerEvent(argument,1000,OS_IO_PORT1_6);
        break;
     case 1:
     default:
        ClearFlag(1);
        OSScheduleTimerEvent(argument,4000,OS_IO_PORT1_6);
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
        SetFlag(2);
        OSScheduleTimerEvent(argument,delay,OS_IO_PORT1_6);
        delay += 100;
        if (delay > 9000)
           delay = 1000;
        break;
     case 1:
     default:
        ClearFlag(2);
        OSScheduleTimerEvent(argument,10000-delay,OS_IO_PORT1_6);
  }
  state = !state;
  OSSuspendSynchronousTask();
} /* end of ToggleLed2Task */
