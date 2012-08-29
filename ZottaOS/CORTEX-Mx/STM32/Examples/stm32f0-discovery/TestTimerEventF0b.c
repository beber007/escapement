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
/* File TestTimerEventF0b.c: Shows how to use API ZottaOS_TimerEvent. This program is
** based on TestTimerEventF0.c but uses a single event-driven task per LED and shows
** how to initiate 2 events.
** Version identifier: June 2012
** Authors: MIS-TIC */

#include "ZottaOS.h"
#include "ZottaOS_TimerEvent.h"
#include "stm32f0xx.h"


#define FLAG_PORT GPIOB
#define FLAG1_PIN GPIO_Pin_1
#define FLAG2_PIN GPIO_Pin_2

#define EVENT_TIMER_INDEX OS_IO_TIM2


void InitializeFlags(UINT16 GPIO_Pin);
void InitApplication(void *argument);
void ToggleLed1Task(void *argument);
void ToggleLed2Task(void *argument);

int main(void)
{
  void *event[2];
  #if ZOTTAOS_TIMER == EVENT_TIMER_INDEX
     #error Event timer device must be different from the internal timer used by ZottaOS
  #endif
  // Enable debug module clock
  RCC_APB2PeriphClockCmd(RCC_APB2Periph_DBGMCU,ENABLE);
  /* Stop timer during debugger connection */
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
  /* Define and start the event handlers */
  OSInitTimerEvent(2,47,0,EVENT_TIMER_INDEX);
  event[0] = OSCreateEventDescriptor();
  event[1] = OSCreateEventDescriptor();
  #if defined(ZOTTAOS_VERSION_HARD)
     OSCreateSynchronousTask(ToggleLed1Task,1000,event[0],event[0]);
     OSCreateSynchronousTask(ToggleLed2Task,100,event[1],event[1]);
  #elif defined(ZOTTAOS_VERSION_SOFT)
     OSCreateSynchronousTask(ToggleLed1Task,0,1000,0,event[0],event[0]);
     OSCreateSynchronousTask(ToggleLed2Task,0,100,0,event[1],event[1]);
  #endif
  /* Start the OS so that it starts scheduling the user tasks */
  return OSStartMultitasking(InitApplication,event);
} /* end of main */


/* InitializeFlags: Initialize input/output pin for flags.*/
void InitializeFlags(UINT16 GPIO_Pin)
{
  GPIO_InitTypeDef GPIO_InitStructure;
  /* Enable GPIO_LED clock */
  RCC_AHBPeriphClockCmd(RCC_AHBPeriph_GPIOB, ENABLE);
  /* Configure GPIO_LED Pin as Output push-pull */
  GPIO_InitStructure.GPIO_Pin = GPIO_Pin;
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
  GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_Level_3;
  GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
  GPIO_Init(GPIOB, &GPIO_InitStructure);
} /* end of InitializeFlags */


/* InitApplication: Triggers event-driven tasks ToggleLed1Task and ToggleLed2Task. */
void InitApplication(void *argument)
{
  void **event = (void **)argument;
  OSScheduleSuspendedTask(event[0]);
  OSScheduleSuspendedTask(event[1]);
} /* end of InitApplication */


/* ToggleLed1Task: Sets a LED every 5000 clock ticks and then clears it after 1000 clock
** ticks. */
void ToggleLed1Task(void *argument)
{
  static UINT8 state = 0;
  switch (state) {
     case 0:
        GPIO_SetBits(FLAG_PORT,FLAG1_PIN);
        OSScheduleTimerEvent(argument,1000,EVENT_TIMER_INDEX);
        break;
     case 1:
     default:
        GPIO_ResetBits(FLAG_PORT,FLAG1_PIN);
        OSScheduleTimerEvent(argument,4000,EVENT_TIMER_INDEX);
  }
  state = !state;
  OSSuspendSynchronousTask();
} /* end of ToggleLed1Task */


/* ToggleLed2Task: Sets a LED every 10000 clock ticks and triggers its clearing after a
** variable delay comprised between 1000 and 9000 clock ticks. */
void ToggleLed2Task(void *argument)
{
  static UINT16 delay = 1000;
  static UINT8 state = 0;
  switch (state) {
     case 0:
        GPIO_SetBits(FLAG_PORT,FLAG2_PIN);
        OSScheduleTimerEvent(argument,delay,EVENT_TIMER_INDEX);
        delay += 100;
        if (delay > 9000)
           delay = 1000;
        break;
     case 1:
     default:
        GPIO_ResetBits(FLAG_PORT,FLAG2_PIN);
        OSScheduleTimerEvent(argument,10000-delay,EVENT_TIMER_INDEX);
  }
  state = !state;
  OSSuspendSynchronousTask();
} /* end of ToggleLed2Task */
