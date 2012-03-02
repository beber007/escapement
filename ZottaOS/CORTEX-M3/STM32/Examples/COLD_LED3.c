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
/* File COLD_LED3.c:
** Version identifier: February 2012
** Authors: MIS-TIC */

#include "ZottaOS.h"
#include "stm32f10x.h"

#define BASE 0x40000400

#define STATUS           *((UINT16 *)(BASE + 0x10))
#define EVENT_GENERATION *((UINT16 *)(BASE + 0x14))
#define COUNTER          *((UINT16 *)(BASE + 0x24))
#define COMPARATOR       *((UINT16 *)(BASE + 0x34))

void assert_failed(UINT8* pcFile,UINT32 ulLine ) { while (TRUE); }

static void SetLedTask(void *argument);

typedef struct DescriptorDef {
  void (*InterruptHandler)(struct DescriptorDef *);
} DescriptorDef;

static void InitializeTimer(void);
static void TimerInterruptHandler(DescriptorDef *descriptor);


int main(void)
{
  DescriptorDef *descriptor;

  /* Stop timer during debugger connection */
  DBGMCU_Config(DBGMCU_TIM4_STOP,ENABLE);
  DBGMCU_Config(DBGMCU_TIM3_STOP,ENABLE);
  /* Keep debugger connection during sleep mode */
  DBGMCU_Config(DBGMCU_SLEEP,ENABLE);

  descriptor = (DescriptorDef *)OSMalloc(sizeof(DescriptorDef));
  descriptor->InterruptHandler = TimerInterruptHandler;
  OSSetISRDescriptor(OS_IO_TIM3,descriptor);

  // Create an aperiodic task
  #if defined(ZOTTAOS_VERSION_HARD)
     OSCreateTask(SetLedTask,0,10000,10000,NULL);
  #elif defined(ZOTTAOS_VERSION_SOFT)
     OSCreateTask(SetLedTask,0,0,10000,10000,1,1,0,NULL);
  #endif


  InitializeTimer();

  /* Start the OS so that it starts scheduling the user tasks */
  return OSStartMultitasking();
} /* end of main */


/* InitializeTimer: . */
void InitializeTimer(void)
{
  TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;
  TIM_OCInitTypeDef TIM_OCInitStructure;
  NVIC_InitTypeDef NVIC_InitStructure;

  GPIO_InitTypeDef GPIO_InitStructure;
  /* Enable GPIO_LED clock */
  RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
  /* Configure GPIO_LED Pin as Output push-pull */
  GPIO_InitStructure.GPIO_Pin = GPIO_Pin_14;
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
  GPIO_Init(GPIOB, &GPIO_InitStructure);

  /* Enable clock for timer TIM2 */
  RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM3,ENABLE);
  /* Configure timer TIM2 */
  TIM_TimeBaseStructure.TIM_Period = 1000;
  TIM_TimeBaseStructure.TIM_Prescaler = 71;
  TIM_TimeBaseStructure.TIM_ClockDivision = 0;
  TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;
  TIM_TimeBaseInit(TIM3,&TIM_TimeBaseStructure);
  /* Mode configuration: Channel1 */
  TIM_OCInitStructure.TIM_OCMode = TIM_OCMode_Inactive;
  TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Disable;
  TIM_OCInitStructure.TIM_Pulse = 1;
  TIM_OC1Init(TIM3, &TIM_OCInitStructure);
  /* Timer 3 Interrupts configuration */
  TIM_ClearITPendingBit(TIM3,TIM_IT_Update);
  TIM_ITConfig(TIM3,TIM_IT_Update,ENABLE);
  /* Enable TIM2 Interrupts */
  NVIC_InitStructure.NVIC_IRQChannel = TIM3_IRQn;
  NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;
  NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
  NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
  NVIC_Init(&NVIC_InitStructure);
} /* end of InitializeTimer */


/* TimerInterruptHandler: . */
void TimerInterruptHandler(DescriptorDef *descriptor)
{
  TIM_Cmd(TIM3,DISABLE);                     // TIM enable counter
  TIM_ClearITPendingBit(TIM3,TIM_IT_Update); // Clear interrupt flag
  GPIOB->BRR = GPIO_Pin_14; // Clear LED
} /* end of TimerInterruptHandler */


/* SetLED: . */
void SetLedTask(void *argument)
{
  GPIOB->BSRR = GPIO_Pin_14; // Set LED
  COUNTER = 0;
  TIM_Cmd(TIM3,ENABLE);
  OSEndTask();
} /* end of SetLED */
