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
/* File TestTimerEvent.c: .
** Version identifier: March 2012
** Authors: MIS-TIC */

#include "msp430.h"
#include "ZottaOS.h"
#include "ZottaOS_TimerEvent.h"

void assert_failed(UINT8* pcFile,UINT32 ulLine ) { while (TRUE); }


typedef struct TaskParametersDef {
   UINT16 GPIO_Pin;    // Output pin for the task
   UINT32 Delay;
   void *Event;
} TaskParametersDef;

static void InitializeTimer(void);

static void SetLedTask(void *argument);
static void ClearLedTask(void *argument);

#define ToggleBit(GPIO_Pin) P1OUT ^= GPIO_Pin;


int main(void)
{
  TaskParametersDef *TaskParameters;

  WDTCTL = WDTPW + WDTHOLD;  // Disable watchdog timer

  // Initialize output I/O ports
  P1SEL = 0x00;   // Set for GPIO
  P1DIR = 0x07;   // Set to output
  P1OUT = 0x00;   // Initially start at low

  InitializeTimer();

  #if defined(ZOTTAOS_VERSION_HARD)
     TaskParameters = (TaskParametersDef *)OSMalloc(sizeof(TaskParametersDef));
     TaskParameters->GPIO_Pin = 0x1;
     TaskParameters->Delay = 1000;
     TaskParameters->Event = OSCreateEventDescriptor();
     OSCreateSynchronousTask((void (*)(void *))ClearLedTask,1000,TaskParameters->Event,TaskParameters);
     OSCreateTask((void (*)(void *))SetLedTask,0,10000,10000,TaskParameters);

     TaskParameters = (TaskParametersDef *)OSMalloc(sizeof(TaskParametersDef));
     TaskParameters->GPIO_Pin = 0x2;
     TaskParameters->Delay = 2000;
     TaskParameters->Event = OSCreateEventDescriptor();
     OSCreateSynchronousTask((void (*)(void *))ClearLedTask,2000,TaskParameters->Event,TaskParameters);
     OSCreateTask((void (*)(void *))SetLedTask,0,20000,20000,TaskParameters);
  #elif defined(ZOTTAOS_VERSION_SOFT)
     tmp = OSCreateEventDescriptor();
     OSCreateSynchronousTask((void (*)(void *))ClearLed1Task,0,1000,0,tmp,NULL);
     OSCreateTask((void (*)(void *))SetLed1Task,0,0,10000,10000,1,1,0,tmp);

     tmp = OSCreateEventDescriptor();
     OSCreateSynchronousTask((void (*)(void *))ClearLed2Task,0,2000,0,tmp,NULL);
     OSCreateTask((void (*)(void *))SetLed2Task,0,0,20000,20000,1,1,0,tmp);
  #endif

  /* Start the OS so that it starts scheduling the user tasks */
  return OSStartMultitasking();
} /* end of main */


/* InitializeTimer: . */
void InitializeTimer(void)
{
  OSInitTimerEvent(2,OS_IO_TIMER1_A1_TA,(UINT16 *)&TA1CTL,(UINT16 *)&TA1R,(UINT16 *)&TA1CCR0,TAIE);
  TA1CCR0 = 0xFFFE;
  TA1CTL |= TASSEL_1 | TAIE | MC_1;
 } /* end of InitializeTimer */


/* SetLedTask: . */
void SetLedTask(void *argument)
{
  TaskParametersDef *TaskParameters = (TaskParametersDef *)argument;
  ToggleBit(TaskParameters->GPIO_Pin)
  OSScheduleTimerEvent(TaskParameters->Event,1000,OS_IO_TIMER1_A1_TA);
  OSEndTask();
} /* end of SetLedTask */


/* ClearLedTask: . */
void ClearLedTask(void *argument)
{
  TaskParametersDef *TaskParameters = (TaskParametersDef *)argument;
  ToggleBit(TaskParameters->GPIO_Pin)
  OSSuspendSynchronousTask();
} /* end of ClearLed2Task */
