/* Copyright (c) 2026 Bertrand Hurst. All rights reserved.
** Escapement - Lightweight Power-Aware Real-Time OS.
** Derived from prior work; see LICENSE and NOTICE at the root of this repository.
*/
/* File TestTimerEventF4.c: Shows how to use API Escapement_TimerEvent. This simple program
** periodically turns LEDS on and then schedules an event to turn them off.
** Version date: March 2012
*/

#include "Escapement.h"
#include "Escapement_TimerEvent.h"
#include "stm32f4xx.h"


#define FLAG_PORT GPIOB
#define FLAG1_PIN GPIO_Pin_13
#define FLAG2_PIN GPIO_Pin_14

#define EVENT_TIMER_INDEX OS_IO_TIM14

static void InitializeFlags(UINT16 GPIO_Pin);
static void SetLed1Task(void *argument);
static void ClearLed1Task(void *argument);
static void SetLed2Task(void *argument);
static void ClearLed2Task(void *argument);


int main(void)
{
  void *tmp;
  #if ESCAPEMENT_TIMER == EVENT_TIMER_INDEX
     #error Event timer device must be different from the internal timer used by Escapement
  #endif
  /* Stop Escapement internal timer during debugger connection */
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
  /* Stop event timer during debugger connection */
  #if EVENT_TIMER_INDEX == OS_IO_TIM14
     DBGMCU_APB1PeriphConfig(DBGMCU_TIM14_STOP,ENABLE);
  #elif EVENT_TIMER_INDEX == OS_IO_TIM13
     DBGMCU_APB1PeriphConfig(DBGMCU_TIM13_STOP,ENABLE);
  #elif EVENT_TIMER_INDEX == OS_IO_TIM12
     DBGMCU_APB1PeriphConfig(DBGMCU_TIM12_STOP,ENABLE);
  #elif EVENT_TIMER_INDEX == OS_IO_TIM11
     DBGMCU_APB2PeriphConfig(DBGMCU_TIM11_STOP,ENABLE);
  #elif EVENT_TIMER_INDEX == OS_IO_TIM10
     DBGMCU_APB2PeriphConfig(DBGMCU_TIM10_STOP,ENABLE);
  #elif EVENT_TIMER_INDEX == OS_IO_TIM9
     DBGMCU_APB2PeriphConfig(DBGMCU_TIM9_STOP,ENABLE);
  #elif EVENT_TIMER_INDEX == OS_IO_TIM8
     DBGMCU_APB2PeriphConfig(DBGMCU_TIM8_STOP,ENABLE);
  #elif EVENT_TIMER_INDEX == OS_IO_TIM5
     DBGMCU_APB1PeriphConfig(DBGMCU_TIM5_STOP,ENABLE);
  #elif EVENT_TIMER_INDEX == OS_IO_TIM4
     DBGMCU_APB1PeriphConfig(DBGMCU_TIM4_STOP,ENABLE);
  #elif EVENT_TIMER_INDEX == OS_IO_TIM3
     DBGMCU_APB1PeriphConfig(DBGMCU_TIM3_STOP,ENABLE);
  #elif EVENT_TIMER_INDEX == OS_IO_TIM2
     DBGMCU_APB1PeriphConfig(DBGMCU_TIM2_STOP,ENABLE);
  #elif EVENT_TIMER_INDEX == OS_IO_TIM1
     DBGMCU_APB2PeriphConfig(DBGMCU_TIM1_STOP,ENABLE);
  #endif
  /* Initialize Hardware */
  SystemInit();
  InitializeFlags(FLAG1_PIN | FLAG2_PIN);
  OSInitTimerEvent(2,83,1,0,EVENT_TIMER_INDEX);
  #if defined(ESCAPEMENT_VERSION_HARD)
     tmp = OSCreateEventDescriptor();
     OSCreateSynchronousTask(ClearLed1Task,1000,tmp,NULL);
     OSCreateTask(SetLed1Task,0,5000,5000,tmp);
     tmp = OSCreateEventDescriptor();
     OSCreateSynchronousTask(ClearLed2Task,1000,tmp,NULL);
     OSCreateTask(SetLed2Task,0,10000,10000,tmp);
  #elif defined(ESCAPEMENT_VERSION_SOFT)
     tmp = OSCreateEventDescriptor();
     OSCreateSynchronousTask(ClearLed1Task,0,1000,0,tmp,NULL);
     OSCreateTask(SetLed1Task,0,0,10000,10000,1,1,0,tmp);
     tmp = OSCreateEventDescriptor();
     OSCreateSynchronousTask(ClearLed2Task,0,2000,0,tmp,NULL);
     OSCreateTask(SetLed2Task,0,0,20000,20000,1,1,0,tmp);
  #endif
  /* Start the OS so that it starts scheduling the user tasks */
  return OSStartMultitasking(NULL,NULL);
} /* end of main */


/* InitializeFlags: Initialize input/output pin for flags.*/
void InitializeFlags(UINT16 GPIO_Pin)
{
  GPIO_InitTypeDef GPIO_InitStructure;
  /* Enable GPIO_LED clock */
  RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOB,ENABLE);
  /* Configure GPIO_LED Pin as Output push-pull */
  GPIO_InitStructure.GPIO_Pin = GPIO_Pin;
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
  GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
  GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
  GPIO_Init(GPIOB, &GPIO_InitStructure);
} /* end of InitializeFlags */


/* SetLed1Task: Sets a LED and triggers its clear after 1000 clock ticks. */
void SetLed1Task(void *argument)
{
  GPIO_SetBits(FLAG_PORT,FLAG1_PIN);
  OSScheduleTimerEvent(argument,1000,EVENT_TIMER_INDEX);
  OSEndTask();
} /* end of SetLed1Task */


/* ClearLed1Task: Clears the LED toggled by SetLed1Task(). */
void ClearLed1Task(void *argument)
{
  GPIO_ResetBits(FLAG_PORT,FLAG1_PIN);
  OSSuspendSynchronousTask();
} /* end of ClearLed1Task */


/* SetLed2Task: Sets a LED and triggers its clear after 2000 clock ticks. */
void SetLed2Task(void *argument)
{
  GPIO_SetBits(FLAG_PORT,FLAG2_PIN);
  OSScheduleTimerEvent(argument,2000,EVENT_TIMER_INDEX);
  OSEndTask();
} /* end of SetLed2Task */


/* ClearLed1Task: Clears the LED toggled by SetLed2Task(). */
void ClearLed2Task(void *argument)
{
  GPIO_ResetBits(FLAG_PORT,FLAG2_PIN);
  OSSuspendSynchronousTask();
} /* end of ClearLed2Task */
