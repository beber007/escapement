/* Copyright (c) 2026 Bertrand Hurst. All rights reserved.
** Escapement - Lightweight Power-Aware Real-Time OS.
** Derived from prior work; see LICENSE and NOTICE at the root of this repository.
*/
/* File TaskWrapF4.c: Three periodic tasks whose only purpose is to cross the 2^30
** boundary of the kernel clock while they run.
**
** The kernel counts time modulo 2^30 and shifts every temporal variable back when its
** counter wraps. On a real board that happens once every eighteen minutes at the usual
** tick rate, which is why no test had ever reached it. Here the timer is clocked at 1 GHz
** by the emulated platform and runs with no prescaler, so the boundary arrives after
** 1.07 seconds; the task periods are scaled by the same factor, leaving the kernel with
** exactly the load it would have on hardware.
**
** Built from the same sources as TaskLEDF4, with ESCAPEMENT_WRAP_TEST defined.
** Platform version: STM32F4-Discovery under Renode.
*/

#include "Escapement.h"
#include "stm32f4xx.h"

#define FLAG_PORT GPIOB
#define FLAG1_PIN GPIO_Pin_13
#define FLAG2_PIN GPIO_Pin_14
#define FLAG3_PIN GPIO_Pin_15

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
  /* One tick is a nanosecond here, so these periods are 1, 2 and 6 ms — the same ratios
  ** as TaskLEDF4 and the same real load, reached with a counter that runs fast enough to
  ** wrap inside a test. */
  TaskParameters = (TaskParametersDef *)OSMalloc(sizeof(TaskParametersDef));
  TaskParameters->GPIOx = FLAG_PORT;
  TaskParameters->GPIO_Pin = FLAG1_PIN;
  TaskParameters->Delay = 20;
  OSCreateTask(FixedDelayTask,0,1000000,1000000,TaskParameters);
  TaskParameters = (TaskParametersDef *)OSMalloc(sizeof(TaskParametersDef));
  TaskParameters->GPIOx = FLAG_PORT;
  TaskParameters->GPIO_Pin = FLAG2_PIN;
  TaskParameters->Delay = 40;
  OSCreateTask(FixedDelayTask,0,2000000,2000000,TaskParameters);
  TaskParameters = (TaskParametersDef *)OSMalloc(sizeof(TaskParametersDef));
  TaskParameters->GPIOx = FLAG_PORT;
  TaskParameters->GPIO_Pin = FLAG3_PIN;
  TaskParameters->Delay = 120;
  OSCreateTask(FixedDelayTask,0,6000000,6000000,TaskParameters);
  return OSStartMultitasking(NULL,NULL);
} /* end of main */


/* InitializeFlags: Initialize input/output pin for flags. */
void InitializeFlags(UINT16 GPIO_Pin)
{
  GPIO_InitTypeDef GPIO_InitStructure;
  RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOB, ENABLE);
  GPIO_InitStructure.GPIO_Pin = GPIO_Pin;
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
  GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
  GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
  GPIO_Init(GPIOB, &GPIO_InitStructure);
} /* end of InitializeFlags */


/* FixedDelayTask: Raises its output, burns a fixed number of iterations, lowers it. */
void FixedDelayTask(void *argument)
{
  volatile UINT32 i;
  TaskParametersDef *TaskParameters = (TaskParametersDef *)argument;
  GPIO_SetBits(TaskParameters->GPIOx,TaskParameters->GPIO_Pin);
  for (i = 0; i < TaskParameters->Delay; i += 1);
  GPIO_ResetBits(TaskParameters->GPIOx,TaskParameters->GPIO_Pin);
  OSEndTask();
} /* end of FixedDelayTask */
