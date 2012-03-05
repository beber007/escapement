/* Copyright (c) 2010 MIS Institute of the HEIG affiliated to the University of Applied
** Sciences of Western Switzerland. All rights reserved.
** IN NO EVENT SHALL THE MIS INSTITUTE NOR THE HEIG NOR THE UNIVERSITY OF APPLIED
** SCIENCES OF WESTERN SWITZERLAND BE LIABLE TO ANY PARTY FOR DIRECT, INDIRECT, SPECIAL,
** INCIDENTAL, OR CONSEQUENTIAL DAMAGES ARISING OUT OF THE USE OF THIS SOFTWARE AND ITS
** DOCUMENTATION, EVEN IF THE MIS INSTITUTE OR THE HEIG OR THE UNIVERSITY OF APPLIED
** SCIENCES OF WESTERN SWITZERLAND HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
** THE MIS INSTITUTE, THE HEIG AND THE UNIVERSITY OF APPLIED SCIENCES OF WESTERN SWIT-
** ZERLAND SPECIFICALLY DISCLAIMS ANY WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
** IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE. THE SOFT-
** WARE PROVIDED HEREUNDER IS ON AN "AS IS" BASIS, AND THE MIS INSTITUTE NOR THE HEIG
** AND NOR THE UNIVERSITY OF APPLIED SCIENCES OF WESTERN SWITZERLAND HAVE NO OBLIGATION
** TO PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR MODIFICATIONS.
*/
/* File TaskLED.c: Illustrates 3 simple periodic tasks that toggle an output GPIO port.
** Two of these tasks do a constant number of iterations in a loop, while the third does
** a variable number of iterations, which increases at each invocation until this number
** reaches a maximum value, at which time it restarts with a iteration of 1.
** This application requires no special setting other than setting the system clocks.
** To build this application, run ZottaOS Configurator Tool for your MSP430xxx to get the
** assembler and header files corresponding to your MSP430 and to get assembler function
** OSInitializeSystemClocks:
**
**              MSP430
**         -----------------
**        |              XIN|-
**        |                 | 32kHz
**        |             XOUT|-
**        |                 |    _____________
**        |             P1.0|---|
**        |             P1.1|---| Oscilloscope
**        |             P1.2|---|
**        |                 |   |
**        |                 |
** 
** You can then attach an oscilloscope to pins P1.0 through P1.2 to see the changes of
** the GPIO output states.
** Although trivial, this simple application uses a single code template to instanciate
** 2 different tasks, each having its particular set of parameters.
**
**
** Version identifier: July 2010
** Authors: MIS-TIC */

#include "../ZottaOS.h"
#include "msp430.h"


typedef struct TaskParametersDef {
   UINT16 GPIO_Pin;    // Output pin for the task
   UINT32 Delay;       // Number of iterations done in the task's loop
} TaskParametersDef;

static void FixedDelayTask(void *argument);
static void VariableDelayTask(void *argument);

#define ToggleBit(GPIO_Pin) P1OUT ^= GPIO_Pin; 

int main(void)
{
  TaskParametersDef *TaskParameters;

  WDTCTL = WDTPW + WDTHOLD;  // Disable watchdog timer

  // Initialize output I/O ports
  P1SEL = 0x00;   // Set for GPIO
  P1DIR = 0x07;   // Set to output
  P1OUT = 0x00;   // Initially start at low

  /* Set the system clock characteristics */
  OSInitializeSystemClocks();

  /* Create the 3 tasks. Notice that each particular task receives a private set of para-
  ** meters that it inherits from the main program and that it is the only task that can
  ** later access. */

  #if defined(ZOTTAOS_VERSION_HARD)
     TaskParameters = (TaskParametersDef *)OSMalloc(sizeof(TaskParametersDef));
     TaskParameters->GPIO_Pin = 0x1;
     TaskParameters->Delay = 100;
     OSCreateTask(FixedDelayTask,0,100000,100000,TaskParameters);

     TaskParameters = (TaskParametersDef *)OSMalloc(sizeof(TaskParametersDef));
     TaskParameters->GPIO_Pin = 0x2;
     TaskParameters->Delay = 50;
     OSCreateTask(FixedDelayTask,0,200,200,TaskParameters);

     TaskParameters = (TaskParametersDef *)OSMalloc(sizeof(TaskParametersDef));
     TaskParameters->GPIO_Pin = 0x4;
     TaskParameters->Delay = 2500;
     OSCreateTask(VariableDelayTask,0,500,350,TaskParameters);
  #elif defined(ZOTTAOS_VERSION_SOFT)
     TaskParameters = (TaskParametersDef *)OSMalloc(sizeof(TaskParametersDef));
     TaskParameters->GPIO_Pin = 0x1;
     TaskParameters->Delay = 220;
     OSCreateTask(FixedDelayTask,17,0,65,65,1,3,0,TaskParameters);

     TaskParameters = (TaskParametersDef *)OSMalloc(sizeof(TaskParametersDef));
     TaskParameters->GPIO_Pin = 0x2;
     TaskParameters->Delay = 220;
     OSCreateTask(FixedDelayTask,17,0,65,39,1,3,0,TaskParameters);

     TaskParameters = (TaskParametersDef *)OSMalloc(sizeof(TaskParametersDef));
     TaskParameters->GPIO_Pin = 0x4;
     TaskParameters->Delay = 1900;
     OSCreateTask(VariableDelayTask,131,0,195,195,1,1,0,TaskParameters);
  #else
     #error Wrong kernel version
  #endif  

  /* Start the OS so that it starts scheduling the user tasks */
  return OSStartMultitasking();
} /* end of main */


/* FixedDelayTask: Changes the state of the task's attributed output I/O port before and
** after executing a fixed number of loop iterations The number of iterations is a param-
** eter transfered by main. */
void FixedDelayTask(void *argument)
{
  UINT32 i;
  TaskParametersDef *TaskParameters = (TaskParametersDef *)argument;
  ToggleBit(TaskParameters->GPIO_Pin);
  for (i = 0; i < TaskParameters->Delay; i += 1);
  ToggleBit(TaskParameters->GPIO_Pin);
  OSEndTask();
} /* end of FixedDelayTask */


/* VariableDelayTask: Same as FixedDelayTask but performs an additional iteration every
** time the task is invoked and until it reaches a limit fixed by main. Once number of
** iterations attains the limit, the process restarts from 1. */
void VariableDelayTask(void *argument)
{ // Caution: Unfortunately, TI Code Composer does not directly comply with ANSI C that
  // dictates that static variables are initialized to 0 when no other initializer is
  // specified.
  static UINT32 k = 0;
  UINT32 i;
  TaskParametersDef *TaskParameters = (TaskParametersDef *)argument;
  ToggleBit(TaskParameters->GPIO_Pin);
  if (k == TaskParameters->Delay)
     k = 1;
  else
     k++;
  for (i = 0; i < k; i++);
  ToggleBit(TaskParameters->GPIO_Pin);
  OSEndTask();
} /* end of VariableDelayTask */
