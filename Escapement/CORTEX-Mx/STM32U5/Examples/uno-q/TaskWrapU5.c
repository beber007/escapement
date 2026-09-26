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
/* File TaskWrapU5.c: TaskWrapPico2.c on the Arduino UNO Q, the same tasks and periods:
** periodic tasks whose only purpose is to cross the 2^30 boundary of the kernel clock
** while they run.
**
** The kernel counts time modulo 2^30 and shifts every temporal variable back when its
** counter wraps: TIM2 wraps there, 17 min 54 s after the kernel starts at the 1 us tick.
** escapement_u5.robot clocks the emulated timer 1000 times faster, so a tick is a
** nanosecond and the boundary comes after 1.07 s; the periods below are scaled by the
** same factor, leaving the kernel with the load it would have on the board. On a board
** this image runs a thousand times slower than intended.
**
** The outputs are those of TaskLEDU5: three tasks of 100, 200 and 600 ms, and a probe
** toggled every 50 ms, whose period is timed after the boundary.
** Platform version: STM32U585 (Arduino UNO Q).
*/

#include "Escapement.h"
#include "BoardU5.h"


typedef struct TaskParametersDef {
   UINT8 Pin;
   UINT32 Delay;
} TaskParametersDef;

static void FixedDelayTask(void *argument);
static void ProbeTask(void *argument);

/* Periods and execution times are in ticks of a nanosecond. Every task is hard: (1,1)-firm
** under the soft kernel. The execution times are far above what the tasks take at
** 160 MHz. */
#if defined(ESCAPEMENT_VERSION_SOFT)
   #define CreateTask(task,wcet,period,parameters) \
              OSCreateTask(task,wcet,0,period,period,1,1,0,parameters)
#else
   #define CreateTask(task,wcet,period,parameters) \
              OSCreateTask(task,0,period,period,parameters)
#endif



int main(void)
{
  TaskParametersDef *TaskParameters;
  OSInitializeSystemClocks();
  InitializeFlag(FLAG1_PIN);
  InitializeFlag(FLAG2_PIN);
  InitializeFlag(FLAG3_PIN);
  InitializeFlag(PROBE_PIN);
  TaskParameters = (TaskParametersDef *)OSMalloc(sizeof(TaskParametersDef));
  TaskParameters->Pin = FLAG1_PIN;
  TaskParameters->Delay = 500;
  CreateTask(FixedDelayTask,100000,100000000,TaskParameters);
  TaskParameters = (TaskParametersDef *)OSMalloc(sizeof(TaskParametersDef));
  TaskParameters->Pin = FLAG2_PIN;
  TaskParameters->Delay = 1000;
  CreateTask(FixedDelayTask,200000,200000000,TaskParameters);
  TaskParameters = (TaskParametersDef *)OSMalloc(sizeof(TaskParametersDef));
  TaskParameters->Pin = FLAG3_PIN;
  TaskParameters->Delay = 2000;
  CreateTask(FixedDelayTask,400000,600000000,TaskParameters);
  CreateTask(ProbeTask,20000,50000000,NULL);
  return OSStartMultitasking(NULL,NULL);
} /* end of main */



/* FixedDelayTask: Raises its output, burns a fixed number of iterations, lowers it. */
static void FixedDelayTask(void *argument)
{
  volatile UINT32 i;
  TaskParametersDef *TaskParameters = (TaskParametersDef *)argument;
  SetPin(TaskParameters->Pin);
  for (i = 0; i < TaskParameters->Delay; i += 1);
  ClearPin(TaskParameters->Pin);
  OSEndTask();
} /* end of FixedDelayTask */


/* ProbeTask: Toggles its output, so that the output's period is twice the task's. */
static void ProbeTask(void *argument)
{
  static UINT32 level = 0;
  level ^= 1;
  if (level) SetPin(PROBE_PIN); else ClearPin(PROBE_PIN);
  OSEndTask();
} /* end of ProbeTask */
