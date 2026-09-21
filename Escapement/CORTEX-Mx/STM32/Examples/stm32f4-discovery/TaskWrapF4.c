/* Copyright (c) 2026 Bertrand Hurst. All rights reserved.
** Escapement - Lightweight Power-Aware Real-Time OS.
** Derived from prior work; see LICENSE and NOTICE at the root of this repository.
*/
/* File TaskWrapF4.c: Three periodic tasks whose only purpose is to cross the 2^30
** boundary of the kernel clock while they run.
**
** The kernel counts time modulo 2^30 and shifts every temporal variable back when its
** counter wraps. On a real board that happens once every eighteen minutes at the usual
** tick rate, which is why no test had ever reached it. Here the emulated platform clocks
** the timer so that its counter ticks at 10 GHz, and the boundary arrives after 107 ms;
** the task periods are scaled by the same factor, leaving the kernel with exactly the
** load it would have on hardware while there is ten times less processor time to
** emulate.
**
** Built from the same sources as TaskLEDF4, with ESCAPEMENT_WRAP_TEST defined.
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


int main(void)
{
  TaskParametersDef *TaskParameters;
  SystemInit();
  InitializeFlags(FLAG1_PIN | FLAG2_PIN | FLAG3_PIN);
  /* A tick is a tenth of a nanosecond here, so these periods are 1, 2 and 6 ms — the same
  ** ratios as TaskLEDF4 and the same real load, with a counter that wraps inside a test. */
  TaskParameters = (TaskParametersDef *)OSMalloc(sizeof(TaskParametersDef));
  TaskParameters->GPIOx = FLAG_PORT;
  TaskParameters->GPIO_Pin = FLAG1_PIN;
  TaskParameters->Delay = 20;
  OSCreateTask(FixedDelayTask,0,10000000,10000000,TaskParameters);
  TaskParameters = (TaskParametersDef *)OSMalloc(sizeof(TaskParametersDef));
  TaskParameters->GPIOx = FLAG_PORT;
  TaskParameters->GPIO_Pin = FLAG2_PIN;
  TaskParameters->Delay = 40;
  OSCreateTask(FixedDelayTask,0,20000000,20000000,TaskParameters);
  TaskParameters = (TaskParametersDef *)OSMalloc(sizeof(TaskParametersDef));
  TaskParameters->GPIOx = FLAG_PORT;
  TaskParameters->GPIO_Pin = FLAG3_PIN;
  TaskParameters->Delay = 120;
  OSCreateTask(FixedDelayTask,0,60000000,60000000,TaskParameters);
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
