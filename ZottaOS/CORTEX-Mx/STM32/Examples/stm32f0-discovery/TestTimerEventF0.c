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
/* File TestTimerEventF0.c: Shows how to use API ZottaOS_TimerEvent. This simple program
** periodically turns LEDS on and then schedules an event to turn them off.
** Version identifier: June 2012
** Authors: MIS-TIC */

#include "ZottaOS.h"
#include "ZottaOS_TimerEvent.h"
#include "stm32f0xx.h"


#define FLAG_PORT GPIOB
#define FLAG1_PIN GPIO_Pin_1
#define FLAG2_PIN GPIO_Pin_2

#define EVENT_TIMER_INDEX OS_IO_TIM2

static void InitializeFlags(UINT16 GPIO_Pin);
static void SetLed1Task(void *argument);
static void ClearLed1Task(void *argument);
static void SetLed2Task(void *argument);
static void ClearLed2Task(void *argument);


int main(void)
{
  void *tmp;
  #if ZOTTAOS_TIMER == EVENT_TIMER_INDEX
     #error Event timer device must be different from the internal timer used by ZottaOS
  #endif
  // Enable debug module clock
  RCC_APB2PeriphClockCmd(RCC_APB2Periph_DBGMCU,ENABLE);
  /* Stop ZottaOS internal timer during debugger connection */
  #if ZOTTAOS_TIMER == OS_IO_TIM17
     DBGMCU_APB2PeriphConfig(DBGMCU_TIM17_STOP,ENABLE);
  #elif ZOTTAOS_TIMER == OS_IO_TIM16
     DBGMCU_APB2PeriphConfig(DBGMCU_TIM16_STOP,ENABLE);
  #elif ZOTTAOS_TIMER == OS_IO_TIM15
     DBGMCU_APB2PeriphConfig(DBGMCU_TIM15_STOP,ENABLE);
  #elif ZOTTAOS_TIMER == OS_IO_TIM14
     DBGMCU_APB1PeriphConfig(DBGMCU_TIM14_STOP,ENABLE);
  #elif ZOTTAOS_TIMER == OS_IO_TIM3
     DBGMCU_APB1PeriphConfig(DBGMCU_TIM3_STOP,ENABLE);
  #elif ZOTTAOS_TIMER == OS_IO_TIM2
     DBGMCU_APB1PeriphConfig(DBGMCU_TIM2_STOP,ENABLE);
  #elif ZOTTAOS_TIMER == OS_IO_TIM1
     DBGMCU_APB2PeriphConfig(DBGMCU_TIM1_STOP,ENABLE);
  #endif
/* Initialize Hardware */
  SystemInit();
  InitializeFlags(FLAG1_PIN | FLAG2_PIN);
  OSInitTimerEvent(2,47,0,EVENT_TIMER_INDEX);
  #if defined(ZOTTAOS_VERSION_HARD)
     tmp = OSCreateEventDescriptor();
     OSCreateSynchronousTask(ClearLed1Task,1000,tmp,NULL);
     OSCreateTask(SetLed1Task,0,5000,5000,tmp);
     tmp = OSCreateEventDescriptor();
     OSCreateSynchronousTask(ClearLed2Task,1000,tmp,NULL);
     OSCreateTask(SetLed2Task,0,10000,10000,tmp);
  #elif defined(ZOTTAOS_VERSION_SOFT)
     tmp = OSCreateEventDescriptor();
     OSCreateSynchronousTask(ClearLed1Task,0,1000,0,tmp,NULL);
     OSCreateTask(SetLed1Task,0,0,5000,5000,1,1,0,tmp);
     tmp = OSCreateEventDescriptor();
     OSCreateSynchronousTask(ClearLed2Task,0,1000,0,tmp,NULL);
     OSCreateTask(SetLed2Task,0,0,10000,10000,1,1,0,tmp);
  #endif

  /* Start the OS so that it starts scheduling the user tasks */
  return OSStartMultitasking(NULL,NULL);
} /* end of main */


/* InitializeFlags: Initialize input/output pin for flags.*/
void InitializeFlags(UINT16 GPIO_Pin)
{
  GPIO_InitTypeDef GPIO_InitStructure;
  /* Enable GPIO_LED clock */
  RCC_AHBPeriphClockCmd(RCC_AHBPeriph_GPIOB,ENABLE);
  /* Configure GPIO_LED Pin as Output push-pull */
  GPIO_InitStructure.GPIO_Pin = GPIO_Pin;
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
  GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_Level_3;
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


/* ClearLed2Task: Clears the LED toggled by SetLed2Task(). */
void ClearLed2Task(void *argument)
{
  GPIO_ResetBits(FLAG_PORT,FLAG2_PIN);
  OSSuspendSynchronousTask();
} /* end of ClearLed2Task */
