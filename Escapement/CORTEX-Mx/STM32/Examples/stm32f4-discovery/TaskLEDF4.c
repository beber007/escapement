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
/* File TaskLEDF4.c: Illustrates 3 simple periodic tasks that toggle an output GPIO port.
** Two of these tasks do a constant number of iterations in a loop, while the third does
** a variable number of iterations, which increases at each invocation until this number
** reaches a maximum value, at which time it restarts with one iteration.
** Version date: March 2012
*/

#include "Escapement.h"
#include "BoardF4.h"

#define FLAG_PORT GPIOB
#define FLAG1_PIN PIN(13)
#define FLAG2_PIN PIN(14)
#define FLAG3_PIN PIN(15)

typedef struct TaskParametersDef {
   GPIO_TypeDef* GPIOx; // Output port for the task
   UINT16 GPIO_Pin;     // Output pin for the task
   UINT32 Delay;        // Number of iterations done in the task's loop
} TaskParametersDef;

static void InitializeFlags(UINT16 GPIO_Pin);
static void FixedDelayTask(void *argument);
static void VariableDelayTask(void *argument);


int main(void)
{
  TaskParametersDef *TaskParameters;
  /* Stop timer during debugger connection */
  BoardStopTimerInDebug(ESCAPEMENT_TIMER);
  /* Keep debugger connection during sleep mode */
  BoardKeepDebugInSleep();
  /* Initialize Hardware */
  SystemInit();
  BoardInitClock();
  InitializeFlags(FLAG1_PIN | FLAG2_PIN | FLAG3_PIN);
  /* Create the 3 tasks. Notice that each particular task receives a private set of para-
  ** meters that it inherits from the main program and that it is the only task that can
  ** later access. */
  #if defined(ESCAPEMENT_VERSION_HARD)
     TaskParameters = (TaskParametersDef *)OSMalloc(sizeof(TaskParametersDef));
     TaskParameters->GPIOx = FLAG_PORT;
     TaskParameters->GPIO_Pin = FLAG1_PIN;
     TaskParameters->Delay = 280;
     OSCreateTask(FixedDelayTask,0,100,100,TaskParameters);
     TaskParameters = (TaskParametersDef *)OSMalloc(sizeof(TaskParametersDef));
     TaskParameters->GPIOx = FLAG_PORT;
     TaskParameters->GPIO_Pin = FLAG2_PIN;
     TaskParameters->Delay = 560;
     OSCreateTask(FixedDelayTask,0,200,200,TaskParameters);
     TaskParameters = (TaskParametersDef *)OSMalloc(sizeof(TaskParametersDef));
     TaskParameters->GPIOx = FLAG_PORT;
     TaskParameters->GPIO_Pin = FLAG3_PIN;
     TaskParameters->Delay = 2350;
     OSCreateTask(VariableDelayTask,0,600,600,TaskParameters);
  #elif defined(ESCAPEMENT_VERSION_SOFT)
     TaskParameters = (TaskParametersDef *)OSMalloc(sizeof(TaskParametersDef));
     TaskParameters->GPIOx = FLAG_PORT;
     TaskParameters->GPIO_Pin = FLAG1_PIN;
     TaskParameters->Delay = 280;
     OSCreateTask(FixedDelayTask,25,0,100,100,1,3,0,TaskParameters);
     TaskParameters = (TaskParametersDef *)OSMalloc(sizeof(TaskParametersDef));
     TaskParameters->GPIOx = FLAG_PORT;
     TaskParameters->GPIO_Pin = FLAG2_PIN;
     TaskParameters->Delay = 560;
     OSCreateTask(FixedDelayTask,50,0,200,200,1,3,0,TaskParameters);
     TaskParameters = (TaskParametersDef *)OSMalloc(sizeof(TaskParametersDef));
     TaskParameters->GPIOx = FLAG_PORT;
     TaskParameters->GPIO_Pin = FLAG3_PIN;
     TaskParameters->Delay = 4700;
     OSCreateTask(VariableDelayTask,480,0,600,600,1,1,0,TaskParameters);
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


/* FixedDelayTask: Changes the state of the task's attributed output I/O port before and
** after executing a fixed number of loop iterations. The number of iterations is a param-
** eter passed by main. */
void FixedDelayTask(void *argument)
{
  volatile UINT32 i;
  TaskParametersDef *TaskParameters = (TaskParametersDef *)argument;
  TaskParameters->GPIOx->BSRR = TaskParameters->GPIO_Pin;
  for (i = 0; i < TaskParameters->Delay; i += 1);
  TaskParameters->GPIOx->BSRR = (UINT32)TaskParameters->GPIO_Pin << 16;
  OSEndTask();
} /* end of FixedDelayTask */


/* VariableDelayTask: Same as FixedDelayTask but performs an additional iteration every
** time the task is invoked and until it reaches a limit fixed by main. Once number of
** iterations attains the limit, the process restarts from 1. */
void VariableDelayTask(void *argument)
{
  volatile static UINT32 k;
  volatile UINT32 i;
  TaskParametersDef *TaskParameters = (TaskParametersDef *)argument;
  if (k == TaskParameters->Delay)
     k = 1;
  else
     k++;
  TaskParameters->GPIOx->BSRR = TaskParameters->GPIO_Pin;
  for (i = 0; i < k; i++);
  TaskParameters->GPIOx->BSRR = (UINT32)TaskParameters->GPIO_Pin << 16;
  OSEndTask();
} /* end of VariableDelayTask */
