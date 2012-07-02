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
/* File TestTimerEvent2.c: Shows how to use API ZottaOS_TimerEvent. This program is based
** on TestTimerEvent.c but uses a single event-driven task per LED and shows how to ini-
** tiate 2 events.
** Prior to using the ZottaOS_TimerEvent API, you should run ZottaOSconf.exe to define a
** timer with two interrupt sources (one for the overflow and its corresponding capture
** compare register 1, e.g. OS_IO_TIMER1_A1_TA and OS_IO_TIMER1_A1_CC1 for Timer1 A), and
** also a port pin interrupt to act as a software interrupt (e.g. on port 1 pin 6 defined
** as OS_IO_PORT1_6).
** Version identifier: June 2012
** Authors: MIS-TIC */

#include "ZottaOS.h"
#include "ZottaOS_TimerEvent.h"


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
  #if defined(ZOTTAOS_VERSION_HARD)
     OSCreateSynchronousTask(ToggleLed1Task,100,event[0],event[0]);
     OSCreateSynchronousTask(ToggleLed2Task,10,event[1],event[1]);
  #elif defined(ZOTTAOS_VERSION_SOFT)
     OSCreateSynchronousTask(ToggleLed1Task,0,100,0,event[0],event[0]);
     OSCreateSynchronousTask(ToggleLed2Task,0,10,0,event[1],event[1]);
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


/* ToggleLed1Task: Sets a LED every 500 clock ticks and then clears it after 100 clock
** ticks. */
void ToggleLed1Task(void *argument)
{
  static UINT8 state = 0;
  switch (state) {
     case 0:
        SetFlag(1);
        OSScheduleTimerEvent(argument,100,OS_IO_PORT1_6);
        break;
     case 1:
     default:
        ClearFlag(1);
        OSScheduleTimerEvent(argument,400,OS_IO_PORT1_6);
  }
  state = !state;
  OSSuspendSynchronousTask();
} /* end of ToggleLed1Task */


/* ToggleLed2Task: Sets a LED every 1000 clock ticks and triggers its clearing after a
** variable delay comprised between 10 and 900 clock ticks. */
void ToggleLed2Task(void *argument)
{
  static UINT16 delay = 10;
  static UINT8 state = 0;
  switch (state) {
     case 0:
        SetFlag(2);
        OSScheduleTimerEvent(argument,delay,OS_IO_PORT1_6);
        delay += 1;
        if (delay > 900)
           delay = 10;
        break;
     case 1:
     default:
        ClearFlag(1);
        OSScheduleTimerEvent(argument,1000-delay,OS_IO_PORT1_6);
  }
  state = !state;
  OSSuspendSynchronousTask();
} /* end of ToggleLed2Task */
