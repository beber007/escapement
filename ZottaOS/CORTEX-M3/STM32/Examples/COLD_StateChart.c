/* Copyright (c) 2012 MIS Institute of the HEIG affiliated to the University of Applied
** Sciences of Western Switzerland. All rights reserved.
** IN NO EVENT SHALL THE MIS INSTITUTE NOR THE HEIG NOR THE UNIVERSITY OF APPLIED
** SCIENCES OF WESTERN SWITZERLAND BE LIABLE TO ANY PARTY FOR DIRECT, INDIRECT, SPECIAL,
** INCIDENTAL, OR CONSEQUENTIAL DAMAGES ARISING OUT OF THE USE OF THIS SOFTWARE AND ITS
** DOCUMENTATION, EVEN IF THE MIS INSTITUTE OR THE HEIG OR THE UNIVERSITY OF APPLIED
** SCIENCES OF WESTERN SWITZERLAND HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
** THE MIS INSTITUTE, THE HEIG AND THE UNIVERSITY OF APPLIED SCIENCES OF WESTERN SWIT-
** ZERLAND SPECIFICALLY DISCLAIMS ANY WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
** IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE. THE SOFT-
** WARE PROVIDED HEREUNDER IS ON AN "AS IS" BASIS, AND THE MIS INSTITUTE NOR THE HEIG
** AND NOR THE UNIVERSITY OF APPLIED SCIENCES OF WESTERN SWITZERLAND HAVE NO OBLIGATION
** TO PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR MODIFICATIONS.
*/
/* File COLD_LED4.c: .
** Version identifier: February 2012
** Authors: MIS-TIC */

#include "ZottaOS.h"
#include "stm32f10x.h"
#include "COLD_StateChart.h"

void assert_failed(UINT8* pcFile,UINT32 ulLine ) {while (TRUE);}

typedef struct EventNodeDef {
  void *event;
  UINT16 CurrentState;
  UINT16 NextState;
  void (*Function)(void *handle);
} EventNodeDef;

static void StateChartTask(void *argument);
static void InitializeTimer(void);

int main(void)
{
  /* Stop timer during debugger connection */
  DBGMCU_Config(DBGMCU_TIM4_STOP,ENABLE);
  DBGMCU_Config(DBGMCU_TIM3_STOP,ENABLE);
  /* Keep debugger connection during sleep mode */
  DBGMCU_Config(DBGMCU_SLEEP,ENABLE);

  OSInitTimerEvent(5,OS_IO_TIM3);

  InitStateCharts();

  InitializeTimer();

  /* Start the OS so that it starts scheduling the user tasks */
  return OSStartMultitasking();
} /* end of main */


void InitializeTimer(void)
{
  GPIO_InitTypeDef GPIO_InitStructure;
  TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;
  NVIC_InitTypeDef NVIC_InitStructure;
  /* Enable GPIO_LED clock */
  RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
 /* Configure GPIO_LED Pin as Output push-pull */
  GPIO_InitStructure.GPIO_Pin = GPIO_Pin_14 | GPIO_Pin_13;
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
  GPIO_Init(GPIOB, &GPIO_InitStructure);

  /* Enable clock for timer */
  RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM3,ENABLE);
  /* Configure timer TIM2 */
  TIM_TimeBaseStructure.TIM_Period = 65535;
  TIM_TimeBaseStructure.TIM_Prescaler = 71;
  TIM_TimeBaseStructure.TIM_ClockDivision = 0;
  TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;
  TIM_TimeBaseInit(TIM3,&TIM_TimeBaseStructure);
  /* Interrupts configuration */
  TIM_ClearITPendingBit(TIM3,TIM_IT_Update);
  TIM_ClearITPendingBit(TIM3,TIM_IT_CC1);
  TIM_ITConfig(TIM3,TIM_IT_CC1,ENABLE);
  TIM_ITConfig(TIM3,TIM_IT_Update,ENABLE);

  /* Enable timer Interrupts */
  NVIC_InitStructure.NVIC_IRQChannel = TIM3_IRQn;
  NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;
  NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
  NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
  NVIC_Init(&NVIC_InitStructure);

  TIM_Cmd(TIM3,ENABLE);   // TIM enable counter
}


/* InitStateChart: .*/
#if defined(ZOTTAOS_VERSION_HARD)
void InitStateChart(void (*function)(void *), UINT8 initialState, INT32 workLoad)
#elif defined(ZOTTAOS_VERSION_SOFT)
void InitStateChart(void (*function)(void *),UINT8 initialState,INT32 wcet,
                    INT32 workLoad, UINT8 aperiodicUtilization)
#endif
{
  EventNodeDef *eventNode;
  eventNode = (EventNodeDef *)OSMalloc(sizeof(EventNodeDef));
  eventNode->event = OSCreateEventDescriptor();
#if defined(ZOTTAOS_VERSION_HARD)
   OSCreateSynchronousTask(StateChartTask,workLoad,eventNode->event,(void *)eventNode);
#elif defined(ZOTTAOS_VERSION_SOFT)
   OSCreateSynchronousTask(StateChartTask,wcet,workLoad,aperiodicUtilization,
                           eventNode->event,(void *)eventNode);
#endif
  eventNode->Function = function;
  eventNode->NextState = initialState;
  OSScheduleTimerEvent(eventNode->event,100,OS_IO_TIM3);
} /* end of InitStateChart */


/* ScheduleNextState: .*/
void ScheduleNextState(void *handle, UINT16 nextState, INT32 time)
{
  EventNodeDef *eventNode = (EventNodeDef *)handle;
  eventNode->NextState = nextState;
  OSScheduleTimerEvent(eventNode->event,time,OS_IO_TIM3);
} /* end of ScheduleNextState */


/* UnScheduleNextState: .*/
void UnScheduleNextState(void *handle)
{
  EventNodeDef *eventNode = (EventNodeDef *)handle;
  OSUnScheduleTimerEvent(eventNode->event,OS_IO_TIM3);
} /* end of UnScheduleNextState */


/* GotoNextState: .*/
void GotoNextState(void *handle, UINT16 nextState)
{
  EventNodeDef *eventNode = (EventNodeDef *)handle;
  eventNode->CurrentState = nextState;
} /* end of GotoNextState */


/* GetCurrentState: .*/
UINT16 GetCurrentState(void *handle)
{
  EventNodeDef *eventNode = (EventNodeDef *)handle;
  return eventNode->CurrentState;
} /* end of GetCurrentState */


/* StateChartTask: .*/
void StateChartTask(void *argument)
{
  UINT16 prevState;
  EventNodeDef *stateChart = (EventNodeDef *)argument;
  stateChart->CurrentState = stateChart->NextState;
  do {
    prevState = stateChart->CurrentState;
    stateChart->Function(stateChart);
  } while (prevState != stateChart->CurrentState);
  OSSuspendSynchronousTask();
} /* end of StateChartTask */
