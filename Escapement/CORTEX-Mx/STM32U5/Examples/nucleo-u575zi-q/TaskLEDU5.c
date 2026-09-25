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
/* File TaskLEDU5.c: Three periodic tasks that each toggle an output while they run,
** and a 1 ms probe. TaskLEDPico2.c on the NUCLEO-U575ZI-Q: the same tasks and periods,
** the outputs of BoardU5.h, the first two tasks on the green and blue LEDs of the board.
** Platform version: STM32U575 (NUCLEO-U575ZI-Q).
*/

#include "Escapement.h"
#include "BoardU5.h"


/* Parameters handed to each task instance */
typedef struct TaskParametersDef {
   UINT8 Pin;       /* output driven by the task */
   UINT32 Delay;    /* number of loop iterations before releasing the output */
} TaskParametersDef;

static void FixedDelayTask(void *argument);
static void VariableDelayTask(void *argument);
static void ProbeTask(void *argument);



int main(void)
{
  TaskParametersDef *TaskParameters;
  /* 160 MHz: the microsecond tick of the timer, and therefore the task periods, depend on
  ** the clock this sets. */
  OSInitializeSystemClocks();
  InitializeFlag(FLAG1_PIN);
  InitializeFlag(FLAG2_PIN);
  InitializeFlag(FLAG3_PIN);
  InitializeFlag(PROBE_PIN);
  /* Create the 3 tasks. Periods are expressed in microseconds, the resolution of the
  ** timer: 10, 20 and 60 ms, plus the 1 ms probe below. The delays are counts of volatile
  ** loop iterations, whose length depends on the core frequency, 160 MHz here, and on the
  ** optimisation level: low enough on the RP2040 at 125 MHz that the kernel's overload
  ** guard never fired. */
  TaskParameters = (TaskParametersDef *)OSMalloc(sizeof(TaskParametersDef));
  TaskParameters->Pin = FLAG1_PIN;
  TaskParameters->Delay = 500;
  #if defined(ESCAPEMENT_VERSION_SOFT)
     /* Under the soft kernel the first two tasks are (1,3)-firm, as in TaskLEDF4: one
     ** instance in three is mandatory, the others run if the declared execution times,
     ** generous here, leave room for them. The probe stays (1,1), i.e. hard. */
     OSCreateTask(FixedDelayTask,100,0,10000,10000,1,3,0,TaskParameters);
  #else
     OSCreateTask(FixedDelayTask,0,10000,10000,TaskParameters);
  #endif
  TaskParameters = (TaskParametersDef *)OSMalloc(sizeof(TaskParametersDef));
  TaskParameters->Pin = FLAG2_PIN;
  TaskParameters->Delay = 1000;
  #if defined(ESCAPEMENT_VERSION_SOFT)
     OSCreateTask(FixedDelayTask,200,0,20000,20000,1,3,0,TaskParameters);
  #else
     OSCreateTask(FixedDelayTask,0,20000,20000,TaskParameters);
  #endif
  TaskParameters = (TaskParametersDef *)OSMalloc(sizeof(TaskParametersDef));
  TaskParameters->Pin = FLAG3_PIN;
  TaskParameters->Delay = 4000;
  #if defined(ESCAPEMENT_VERSION_SOFT)
     OSCreateTask(VariableDelayTask,1000,0,60000,60000,1,1,0,TaskParameters);
  #else
     OSCreateTask(VariableDelayTask,0,60000,60000,TaskParameters);
  #endif
  /* Measurement probe, 1 ms period, no payload. */
  #if defined(ESCAPEMENT_VERSION_SOFT)
     OSCreateTask(ProbeTask,20,0,1000,1000,1,1,0,NULL);
  #else
     OSCreateTask(ProbeTask,0,1000,1000,NULL);
  #endif
  /* Start the OS so that it starts scheduling the user tasks */
  return OSStartMultitasking(NULL,NULL);
} /* end of main */



/* FixedDelayTask: Raises its output, burns a fixed number of iterations, lowers it. */
static void FixedDelayTask(void *argument)
{
  volatile UINT32 i;
  TaskParametersDef *TaskParameters = (TaskParametersDef *)argument;
  OSTrace(OS_TRACE_MARK,TaskParameters->Pin,0);
  SetPin(TaskParameters->Pin);
  for (i = 0; i < TaskParameters->Delay; i += 1);
  ClearPin(TaskParameters->Pin);
  OSTrace(OS_TRACE_MARK,TaskParameters->Pin,1);
  OSEndTask();
} /* end of FixedDelayTask */


static void ProbeTask(void *argument)
{
  static UINT32 level = 0;
  OSTrace(OS_TRACE_MARK,PROBE_PIN,0);
  level ^= 1;
  if (level) SetPin(PROBE_PIN); else ClearPin(PROBE_PIN);
  OSEndTask();
}


/* VariableDelayTask: Same, but the number of iterations grows on each instance until it
** reaches the limit, then restarts from 1. */
static void VariableDelayTask(void *argument)
{
  volatile static UINT32 k;
  volatile UINT32 i;
  TaskParametersDef *TaskParameters = (TaskParametersDef *)argument;
  if (k >= TaskParameters->Delay)
     k = 1;
  else
     k += 1;
  OSTrace(OS_TRACE_MARK,TaskParameters->Pin,0);
  SetPin(TaskParameters->Pin);
  for (i = 0; i < k; i += 1);
  ClearPin(TaskParameters->Pin);
  OSTrace(OS_TRACE_MARK,TaskParameters->Pin,1);
  OSEndTask();
} /* end of VariableDelayTask */
