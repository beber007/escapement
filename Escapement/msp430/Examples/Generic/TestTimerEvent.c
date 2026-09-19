/* Copyright (c) 2026 Bertrand Hurst. All rights reserved.
** Escapement - Lightweight Power-Aware Real-Time OS.
** Derived from prior work; see LICENSE and NOTICE at the root of this repository.
*/
/* File TestTimerEvent.c: Shows how to use API Escapement_TimerEvent. This simple program
** periodically turns LEDS on and then schedules an event to turn them off.
** Prior to using this API, you should run EscapementConf.exe to define a timer with two
** interrupt sources (one for the overflow and its corresponding capture/compare regis-
** ter 1, e.g. OS_IO_TIMER1_A1_TA and OS_IO_TIMER1_A1_CC1 for Timer1 A), and also a port
** pin interrupt to act as a software interrupt (e.g. port 1 pin 6 OS_IO_PORT1_6).
** Version identifier: May 2012

#include "Escapement.h"
#include "Escapement_TimerEvent.h"

static void SetLed1Task(void *argument);
static void ClearLed1Task(void *argument);
static void SetLed2Task(void *argument);
static void ClearLed2Task(void *argument);

#define SetFlag(GPIO_Pin) P1OUT |= GPIO_Pin;
#define ClearFlag(GPIO_Pin) P1OUT &= ~GPIO_Pin;

int main(void)
{
  void *event;

  WDTCTL = WDTPW + WDTHOLD;  // Disable watchdog timer

  /* Initialize output I/O ports */
  P1SEL = 0x00;    // Set for GPIO
  P1DIR = 0x03;    // Set to output
  P1OUT = 0x00;    // Initially start at low

  /* Define and start the event handlers */
  if (!OSInitTimerEvent(2,OS_IO_PORT1_6,OS_IO_TIMER1_A1_TA))
     while (TRUE); // Initialization problem

  event = OSCreateEventDescriptor();
  #if defined(ESCAPEMENT_VERSION_HARD)
     OSCreateSynchronousTask(ClearLed1Task,100,event,NULL);
     OSCreateTask(SetLed1Task,0,500,500,event);
     event = OSCreateEventDescriptor();
     OSCreateSynchronousTask(ClearLed2Task,100,event,NULL);
     OSCreateTask(SetLed2Task,0,1000,1000,event);
  #elif defined(ESCAPEMENT_VERSION_SOFT)
     OSCreateSynchronousTask(ClearLed1Task,0,100,0,event,NULL);
     OSCreateTask(SetLed1Task,0,0,500,500,1,1,0,event);
     event = OSCreateEventDescriptor();
     OSCreateSynchronousTask(ClearLed2Task,0,100,0,event,NULL);
     OSCreateTask(SetLed2Task,0,0,1000,1000,1,1,0,event);
  #endif

  /* Start the OS so that it starts scheduling the user tasks */
  return OSStartMultitasking(NULL,NULL);
} /* end of main */


/* SetLed1Task: Sets a LED and triggers its clear after 100 clock ticks. */
void SetLed1Task(void *argument)
{
  SetFlag(1);
  OSScheduleTimerEvent(argument,100,OS_IO_PORT1_6);
  OSEndTask();
} /* end of SetLed1Task */


/* ClearLed1Task: Clears the LED toggled by SetLed1Task(). */
void ClearLed1Task(void *argument)
{
  ClearFlag(1);
  OSSuspendSynchronousTask();
} /* end of ClearLed1Task */


/* SetLed2Task: Sets a LED and triggers its clear after a variable delay comprised between
** 100 and 800 clock ticks. */
void SetLed2Task(void *argument)
{
  static UINT16 delay = 100;
  SetFlag(2);
  OSScheduleTimerEvent(argument,delay,OS_IO_PORT1_6);
  delay += 1;
  if (delay > 800)
     delay = 100;
  OSEndTask();
} /* end of SetLed2Task */


/* ClearLed2Task: Clears the LED toggled by SetLed2Task(). */
void ClearLed2Task(void *argument)
{
  ClearFlag(2);
  OSSuspendSynchronousTask();
} /* end of ClearLed2Task */

