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
/* File TaskLED.c: Illustrates 3 simple periodic tasks that toggle an output GPIO port.
** Two of these tasks do a constant number of iterations in a loop, while the third does
** a variable number of iterations, which increases at each invocation until this number
** reaches a maximum value, at which time it restarts with a iteration of 1.
** Version date: March 2012
** Authors: MIS-TIC */

#include "ZottaOS.h"
#include "stm32f10x.h"

typedef struct TaskParametersDef {
   UINT16 GPIO_Pin;    // Output pin for the task
   UINT32 Delay;       // Number of iterations done in the task's loop
} TaskParametersDef;

static void FixedDelayTask(void *argument);
static void VariableDelayTask(void *argument);

void assert_failed(UINT8* pcFile,UINT32 ulLine )
{
  while (TRUE);
}

void InitializeFlag(u16 GPIO_Pin);
void ToggleLED(UINT16 GPIO_Pin);
void SetLED(UINT16 GPIO_Pin);
void ClearLED(UINT16 GPIO_Pin);


int main(void)
{
  TaskParametersDef *TaskParameters;
  /* Stop timer during debugger connection */
  #if ZOTTAOS_TIMER == OS_IO_TIM17
     DBGMCU_Config(DBGMCU_TIM17_STOP,ENABLE);
  #elif ZOTTAOS_TIMER == OS_IO_TIM16
     DBGMCU_Config(DBGMCU_TIM16_STOP,ENABLE);
   #elif ZOTTAOS_TIMER == OS_IO_TIM15
     DBGMCU_Config(DBGMCU_TIM15_STOP,ENABLE);
   #elif ZOTTAOS_TIMER == OS_IO_TIM14
     DBGMCU_Config(DBGMCU_TIM14_STOP,ENABLE);
   #elif ZOTTAOS_TIMER == OS_IO_TIM13
     DBGMCU_Config(DBGMCU_TIM13_STOP,ENABLE);
   #elif ZOTTAOS_TIMER == OS_IO_TIM12
     DBGMCU_Config(DBGMCU_TIM12_STOP,ENABLE);
   #elif ZOTTAOS_TIMER == OS_IO_TIM11
     DBGMCU_Config(DBGMCU_TIM11_STOP,ENABLE);
   #elif ZOTTAOS_TIMER == OS_IO_TIM10
     DBGMCU_Config(DBGMCU_TIM10_STOP,ENABLE);
   #elif ZOTTAOS_TIMER == OS_IO_TIM9
     DBGMCU_Config(DBGMCU_TIM9_STOP,ENABLE);
   #elif ZOTTAOS_TIMER == OS_IO_TIM8
     DBGMCU_Config(DBGMCU_TIM8_STOP,ENABLE);
   #elif ZOTTAOS_TIMER == OS_IO_TIM7
     DBGMCU_Config(DBGMCU_TIM7_STOP,ENABLE);
   #elif ZOTTAOS_TIMER == OS_IO_TIM6
     DBGMCU_Config(DBGMCU_TIM6_STOP,ENABLE);
   #elif ZOTTAOS_TIMER == OS_IO_TIM5
     DBGMCU_Config(DBGMCU_TIM5_STOP,ENABLE);
   #elif ZOTTAOS_TIMER == OS_IO_TIM34
     DBGMCU_Config(DBGMCU_TIM4_STOP,ENABLE);
   #elif ZOTTAOS_TIMER == OS_IO_TIM3
     DBGMCU_Config(DBGMCU_TIM3_STOP,ENABLE);
   #elif ZOTTAOS_TIMER == OS_IO_TIM2
     DBGMCU_Config(DBGMCU_TIM2_STOP,ENABLE);
   #elif ZOTTAOS_TIMER == OS_IO_TIM1
     DBGMCU_Config(DBGMCU_TIM1_STOP,ENABLE);
   #endif
  /* Keep debugger connection during sleep mode */
  DBGMCU_Config(DBGMCU_SLEEP,ENABLE);

  OSInitializeSystemClocks();

  /* Create the 3 tasks. Notice that each particular task receives a private set of para-
  ** meters that it inherits from the main program and that it is the only task that can
  ** later access. */

  InitializeFlag(GPIO_Pin_12 | GPIO_Pin_13 | GPIO_Pin_14);

  #if defined(ZOTTAOS_VERSION_HARD)
     TaskParameters = (TaskParametersDef *)OSMalloc(sizeof(TaskParametersDef));
     TaskParameters->GPIO_Pin = GPIO_Pin_12;
     TaskParameters->Delay = 150;
     OSCreateTask(FixedDelayTask,0,700,699,TaskParameters);

     TaskParameters = (TaskParametersDef *)OSMalloc(sizeof(TaskParametersDef));
     TaskParameters->GPIO_Pin = GPIO_Pin_13;
     TaskParameters->Delay = 150;
     OSCreateTask(FixedDelayTask,0,700,700,TaskParameters);

     TaskParameters = (TaskParametersDef *)OSMalloc(sizeof(TaskParametersDef));
     TaskParameters->GPIO_Pin = GPIO_Pin_14;
     TaskParameters->Delay = 7000;
     OSCreateTask(VariableDelayTask,0,2100,2100,TaskParameters);
  #elif defined(ZOTTAOS_VERSION_SOFT)
     TaskParameters = (TaskParametersDef *)OSMalloc(sizeof(TaskParametersDef));
     TaskParameters->GPIO_Pin = GPIO_Pin_12;
     TaskParameters->Delay = 600;
     OSCreateTask(FixedDelayTask,200,0,700,699,1,3,0,TaskParameters);

     TaskParameters = (TaskParametersDef *)OSMalloc(sizeof(TaskParametersDef));
     TaskParameters->GPIO_Pin = GPIO_Pin_13;
     TaskParameters->Delay = 600;
     OSCreateTask(FixedDelayTask,200,0,700,700,1,3,0,TaskParameters);

     TaskParameters = (TaskParametersDef *)OSMalloc(sizeof(TaskParametersDef));
     TaskParameters->GPIO_Pin = GPIO_Pin_14;
     TaskParameters->Delay = 7000;
     OSCreateTask(VariableDelayTask,1600,0,2100,2100,1,1,0,TaskParameters);
  #endif

  /* Start the OS so that it starts scheduling the user tasks */
  return OSStartMultitasking();
} /* end of main */


/* FixedDelayTask: Changes the state of the task's attributed output I/O port before and
** after executing a fixed number of loop iterations The number of iterations is a param-
** eter transfered by main. */
void FixedDelayTask(void *argument)
{
	volatile UINT32 i;
  TaskParametersDef *TaskParameters = (TaskParametersDef *)argument;
  SetLED(TaskParameters->GPIO_Pin);
  for (i = 0; i < TaskParameters->Delay; i += 1);
  ClearLED(TaskParameters->GPIO_Pin);
  OSEndTask();
} /* end of FixedDelayTask */


/* VariableDelayTask: Same as FixedDelayTask but performs an additional iteration every
** time the task is invoked and until it reaches a limit fixed by main. Once number of
** iterations attains the limit, the process restarts from 1. */
void VariableDelayTask(void *argument)
{ // Caution: Unfortunately, TI Code Composer does not directly comply with ANSI C that
  // dictates that static variables are initialized to 0 when no other initializer is
  // specified.
  volatile  static UINT32 k = 0;
  UINT32 i;
  TaskParametersDef *TaskParameters = (TaskParametersDef *)argument;
  if (k == TaskParameters->Delay)
     k = 1;
  else
     k++;
  SetLED(TaskParameters->GPIO_Pin);
  for (i = 0; i < k; i++);
  ClearLED(TaskParameters->GPIO_Pin);
  OSEndTask();
} /* end of VariableDelayTask */


/* InitializeFlag: .*/
void InitializeFlag(u16 GPIO_Pin)
{
  GPIO_InitTypeDef GPIO_InitStructure;
  /* Enable GPIO_LED clock */
  RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
  /* Configure GPIO_LED Pin as Output push-pull */
  GPIO_InitStructure.GPIO_Pin = GPIO_Pin;
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
  GPIO_Init(GPIOB, &GPIO_InitStructure);
} /* end of InitializeFlag */


/* ToggleLED: . */
void ToggleLED(UINT16 GPIO_Pin)
{
  if ((GPIOB->ODR & GPIO_Pin) != GPIO_Pin)
     GPIOB->BSRR = GPIO_Pin;
  else
     GPIOB->BRR = GPIO_Pin;
} /* end of ToggleLED */


/* SetLED: . */
void SetLED(UINT16 GPIO_Pin)
{
  GPIOB->BSRR = GPIO_Pin;
} /* end of SetLED */


/* ClearLED: . */
void ClearLED(UINT16 GPIO_Pin)
{
  GPIOB->BRR = GPIO_Pin;
} /* end of ClearLED */
