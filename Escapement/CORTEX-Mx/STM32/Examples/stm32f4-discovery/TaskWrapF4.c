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
/* File TaskWrapF4.c: Three periodic tasks whose only purpose is to cross the 2^30
** boundary of the kernel clock while they run.
**
** The kernel counts time modulo 2^30 and shifts every temporal variable back when its
** counter wraps. On a real board that happens once every eighteen minutes at the usual
** tick rate, which is why no test had ever reached it. Here the emulated platform
** (escapement_f4_wrap.repl) clocks the timer so that its counter ticks at 10 GHz, and the
** boundary arrives after 107 ms. The periods are given in those ticks, 1, 2 and 6 ms,
** close to those of TaskLEDF4 under escapement_f4.repl (0.82, 1.64 and 4.92 ms), so the
** kernel is called about as often while the emulated time stays short.
**
** A source of its own, built with the other examples; only escapement_f4_wrap.robot runs
** it.
** Platform version: STM32F4-Discovery under Renode.
*/

#include "Escapement.h"
#include "BoardF4.h"

#define FLAG_PORT GPIOB
#define FLAG1_PIN PIN(13)
#define FLAG2_PIN PIN(14)
#define FLAG3_PIN PIN(15)

typedef struct TaskParametersDef {
   GPIO_TypeDef* GPIOx;
   UINT16 GPIO_Pin;
   UINT32 Delay;
} TaskParametersDef;

static void InitializeFlags(UINT16 GPIO_Pin);
static void FixedDelayTask(void *argument);

/* The soft kernel takes more parameters; every task here is (1,1)-firm, i.e. hard. */
#if defined(ESCAPEMENT_VERSION_SOFT)
   #define CreateTask(period,parameters) \
              OSCreateTask(FixedDelayTask,0,0,period,period,1,1,0,parameters)
#else
   #define CreateTask(period,parameters) OSCreateTask(FixedDelayTask,0,period,period,parameters)
#endif


int main(void)
{
  TaskParametersDef *TaskParameters;
  SystemInit();
  BoardInitClock();
  InitializeFlags(FLAG1_PIN | FLAG2_PIN | FLAG3_PIN);
  /* A tick is a tenth of a nanosecond here, so these periods are 1, 2 and 6 ms, the ratios
  ** of TaskLEDF4, with a counter that wraps inside a test. */
  TaskParameters = (TaskParametersDef *)OSMalloc(sizeof(TaskParametersDef));
  TaskParameters->GPIOx = FLAG_PORT;
  TaskParameters->GPIO_Pin = FLAG1_PIN;
  TaskParameters->Delay = 20;
  CreateTask(10000000,TaskParameters);
  TaskParameters = (TaskParametersDef *)OSMalloc(sizeof(TaskParametersDef));
  TaskParameters->GPIOx = FLAG_PORT;
  TaskParameters->GPIO_Pin = FLAG2_PIN;
  TaskParameters->Delay = 40;
  CreateTask(20000000,TaskParameters);
  TaskParameters = (TaskParametersDef *)OSMalloc(sizeof(TaskParametersDef));
  TaskParameters->GPIOx = FLAG_PORT;
  TaskParameters->GPIO_Pin = FLAG3_PIN;
  TaskParameters->Delay = 120;
  CreateTask(60000000,TaskParameters);
  return OSStartMultitasking(NULL,NULL);
} /* end of main */


/* InitializeFlags: Initialize input/output pin for flags. */
void InitializeFlags(UINT16 GPIO_Pin)
{
  /* Configure the flag pins as push-pull outputs */
  BoardInitOutputs(GPIOB, GPIO_Pin);
} /* end of InitializeFlags */


/* FixedDelayTask: Raises its output, burns a fixed number of iterations, lowers it. */
void FixedDelayTask(void *argument)
{
  volatile UINT32 i;
  TaskParametersDef *TaskParameters = (TaskParametersDef *)argument;
  TaskParameters->GPIOx->BSRR = TaskParameters->GPIO_Pin;
  for (i = 0; i < TaskParameters->Delay; i += 1);
  TaskParameters->GPIOx->BSRR = (UINT32)TaskParameters->GPIO_Pin << 16;
  OSEndTask();
} /* end of FixedDelayTask */
