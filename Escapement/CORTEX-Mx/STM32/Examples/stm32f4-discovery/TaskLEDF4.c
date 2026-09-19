/* Copyright (c) 2026 Bertrand Hurst. All rights reserved.
** Escapement - Lightweight Power-Aware Real-Time OS.
** Derived from prior work; see LICENSE and NOTICE at the root of this repository.
*/
/* File TaskLEDF4.c: Illustrates 3 simple periodic tasks that toggle an output GPIO port.
** Two of these tasks do a constant number of iterations in a loop, while the third does
** a variable number of iterations, which increases at each invocation until this number
** reaches a maximum value, at which time it restarts with a iteration of 1.
** Version date: March 2012
*/

#include "Escapement.h"
#include "stm32f4xx.h"

#define FLAG_PORT GPIOB
#define FLAG1_PIN GPIO_Pin_13
#define FLAG2_PIN GPIO_Pin_14
#define FLAG3_PIN GPIO_Pin_15

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
  #if ESCAPEMENT_TIMER == OS_IO_TIM14
     DBGMCU_APB1PeriphConfig(DBGMCU_TIM14_STOP,ENABLE);
  #elif ESCAPEMENT_TIMER == OS_IO_TIM13
     DBGMCU_APB1PeriphConfig(DBGMCU_TIM13_STOP,ENABLE);
  #elif ESCAPEMENT_TIMER == OS_IO_TIM12
     DBGMCU_APB1PeriphConfig(DBGMCU_TIM12_STOP,ENABLE);
  #elif ESCAPEMENT_TIMER == OS_IO_TIM11
     DBGMCU_APB2PeriphConfig(DBGMCU_TIM11_STOP,ENABLE);
  #elif ESCAPEMENT_TIMER == OS_IO_TIM10
     DBGMCU_APB2PeriphConfig(DBGMCU_TIM10_STOP,ENABLE);
  #elif ESCAPEMENT_TIMER == OS_IO_TIM9
     DBGMCU_APB2PeriphConfig(DBGMCU_TIM9_STOP,ENABLE);
  #elif ESCAPEMENT_TIMER == OS_IO_TIM8
     DBGMCU_APB2PeriphConfig(DBGMCU_TIM8_STOP,ENABLE);
  #elif ESCAPEMENT_TIMER == OS_IO_TIM5
     DBGMCU_APB1PeriphConfig(DBGMCU_TIM5_STOP,ENABLE);
  #elif ESCAPEMENT_TIMER == OS_IO_TIM4
     DBGMCU_APB1PeriphConfig(DBGMCU_TIM4_STOP,ENABLE);
  #elif ESCAPEMENT_TIMER == OS_IO_TIM3
     DBGMCU_APB1PeriphConfig(DBGMCU_TIM3_STOP,ENABLE);
  #elif ESCAPEMENT_TIMER == OS_IO_TIM2
     DBGMCU_APB1PeriphConfig(DBGMCU_TIM2_STOP,ENABLE);
  #elif ESCAPEMENT_TIMER == OS_IO_TIM1
     DBGMCU_APB2PeriphConfig(DBGMCU_TIM1_STOP,ENABLE);
  #endif
  /* Keep debugger connection during sleep mode */
  DBGMCU_Config(DBGMCU_SLEEP,ENABLE);
  /* Initialize Hardware */
  SystemInit();
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
  GPIO_InitTypeDef GPIO_InitStructure;
  /* Enable GPIO_LED clock */
  RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOB, ENABLE);
  /* Configure GPIO_LED Pin as Output push-pull */
  GPIO_InitStructure.GPIO_Pin = GPIO_Pin;
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
  GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
  GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
  GPIO_Init(GPIOB, &GPIO_InitStructure);
} /* end of InitializeFlags */


/* FixedDelayTask: Changes the state of the task's attributed output I/O port before and
** after executing a fixed number of loop iterations The number of iterations is a param-
** eter transfered by main. */
void FixedDelayTask(void *argument)
{
  volatile UINT32 i;
  TaskParametersDef *TaskParameters = (TaskParametersDef *)argument;
  GPIO_SetBits(TaskParameters->GPIOx,TaskParameters->GPIO_Pin);
  for (i = 0; i < TaskParameters->Delay; i += 1);
  GPIO_ResetBits(TaskParameters->GPIOx,TaskParameters->GPIO_Pin);
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
  GPIO_SetBits(TaskParameters->GPIOx,TaskParameters->GPIO_Pin);
  for (i = 0; i < k; i++);
  GPIO_ResetBits(TaskParameters->GPIOx,TaskParameters->GPIO_Pin);
  OSEndTask();
} /* end of VariableDelayTask */
