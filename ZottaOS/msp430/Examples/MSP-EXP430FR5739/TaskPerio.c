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
/* File TaskPerio.c: Illustrates 3 simple periodic tasks that toggle an output GPIO port.
** Two of these tasks do a constant number of iterations in a loop, while the third does
** a variable number of iterations, which increases at each invocation until this number
** reaches a maximum value, at which time it restarts with a iteration of 1.
** This application requires no special setting other than setting the system clocks.
** To build this application, run "ZottaOS Configurator tool" and load the configuration
** file EXP430FR5739.zot to generate the assembly and header files, and to get assembler
** function OSInitializeSystemClocks that configures the clocks as shown below:
**    - MCLK sourced from the DCO (DCOCLK) divided by 1;
**    - ACLK sourced from the DCO (DCOCLK) divided by 8;
**    - SMCLK sourced from the DCO (DCOCLK) divided by 8;
**    - DCO set for factory calibrated value of 8.00 MHz(DCORSEL = 0, DCOFSEL = 3).
** You can then attach an oscilloscope to pins P1.0 through P1.2 to see the changes of
** the GPIO output states.
**              MSP430
**         -----------------
**        |                 |    _____________
**        |             P1.0|---|
**        |             P1.1|---| Oscilloscope
**        |             P1.2|---|
**        |                 |   |
**        |                 |
** Although trivial, this simple application uses a single code template to instantiate
** 2 different tasks, each having its particular set of parameters.
** Tested on "MSP-EXP430FR5739 - Experimenter Board".
** Version identifier: June 2012
** Authors: MIS-TIC */

#include "ZottaOS.h"


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
  P1OUT = 0x00;   // Initially start at low
  P1DIR = 0x07;   // Set to output

  /* Set the system clock characteristics */
  OSInitializeSystemClocks();

  /* Create the 3 tasks. Notice that each particular task receives a private set of para-
  ** meters that it inherits from the main program and that it is the only task that can
  ** later access. */
  #if defined(ZOTTAOS_VERSION_HARD)
     /* Calculation of the total load:
     ** 1250  / 5000  -> 25%
     ** 2500  / 10000 -> 25%
     ** 12000 / 30000 -> 40%
     ** Total:           90% */
     TaskParameters = (TaskParametersDef *)OSMalloc(sizeof(TaskParametersDef));
     TaskParameters->GPIO_Pin = 0x1;
     TaskParameters->Delay = 600;   // wcet 1250
     OSCreateTask(FixedDelayTask,0,5000,5000,TaskParameters);
     TaskParameters = (TaskParametersDef *)OSMalloc(sizeof(TaskParametersDef));
     TaskParameters->GPIO_Pin = 0x2;
     TaskParameters->Delay = 1200;  // wcet 2500
     OSCreateTask(FixedDelayTask,0,10000,10000,TaskParameters);
     TaskParameters = (TaskParametersDef *)OSMalloc(sizeof(TaskParametersDef));
     TaskParameters->GPIO_Pin = 0x4;
     TaskParameters->Delay = 7000;  // wcet 12000
     OSCreateTask(VariableDelayTask,0,30000,30000,TaskParameters);
  #elif defined(ZOTTAOS_VERSION_SOFT)
     /* Calculation of the total load:
     ** Without ZottaOS-Soft capabilities:
     **    1250  / 5000  ->  25%
     **    2500  / 10000 ->  25%
     **    24000 / 30000 ->  80%
     **    Total:           130% (Task set is NOT schedulable)
     ** With ZottaOS-Soft capabilities:
     **    1250  / 5000  / 3 ->   8%
     **    2500  / 10000 / 3 ->   8%
     **    24000 / 30000 / 1 ->  80%
     **    Total:                96% (Task set is schedulable) */
     TaskParameters = (TaskParametersDef *)OSMalloc(sizeof(TaskParametersDef));
     TaskParameters->GPIO_Pin = 0x1;
     TaskParameters->Delay = 600;   // wcet 1250
     OSCreateTask(FixedDelayTask,1250,0,5000,5000,1,3,2,TaskParameters);
     TaskParameters = (TaskParametersDef *)OSMalloc(sizeof(TaskParametersDef));
     TaskParameters->GPIO_Pin = 0x2;
     TaskParameters->Delay = 1200;  // wcet 2500
     OSCreateTask(FixedDelayTask,2500,0,10000,10000,1,3,0,TaskParameters);
     TaskParameters = (TaskParametersDef *)OSMalloc(sizeof(TaskParametersDef));
     TaskParameters->GPIO_Pin = 0x4;
     TaskParameters->Delay = 13500; // wcet 2400
     OSCreateTask(VariableDelayTask,2400,0,30000,30000,1,1,0,TaskParameters);
  #else
     #error Wrong kernel version
  #endif

  /* Start the OS so that it starts scheduling the user tasks */
  return OSStartMultitasking(NULL,NULL);
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
  if (k >= TaskParameters->Delay)
     k = 1;
  else
     k += 100;
  for (i = 0; i < k; i++);
  ToggleBit(TaskParameters->GPIO_Pin);
  OSEndTask();
} /* end of VariableDelayTask */
