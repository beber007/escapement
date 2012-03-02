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
/* File COLD_LED2.c:
** Version identifier: February 2012
** Authors: MIS-TIC */

#include "ZottaOS.h"
#include "stm32f10x.h"

void assert_failed(UINT8* pcFile,UINT32 ulLine ) { while (TRUE); }

static void SetLedTask(void *argument);
static void ClearLedTask(void *argument);


int main(void)
{
  void *event;

  /* Stop timer during debugger connection */
  DBGMCU_Config(DBGMCU_TIM4_STOP,ENABLE);
  /* Keep debugger connection during sleep mode */
  DBGMCU_Config(DBGMCU_SLEEP,ENABLE);

  GPIO_InitTypeDef GPIO_InitStructure;
  /* Enable GPIO_LED clock */
  RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
  /* Configure GPIO_LED Pin as Output push-pull */
  GPIO_InitStructure.GPIO_Pin = GPIO_Pin_14;
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
  GPIO_Init(GPIOB, &GPIO_InitStructure);

  // Create an event
  event = OSCreateEventDescriptor();
  // Create an aperiodic task and a periodic task
  #if defined(ZOTTAOS_VERSION_HARD)
     OSCreateTask(SetLedTask,0,10000,10000,event);
     OSCreateSynchronousTask(ClearLedTask,1000,event,event);
  #elif defined(ZOTTAOS_VERSION_SOFT)
     OSCreateTask(SetLedTask,0,0,10000,10000,1,1,0,event);
     OSCreateSynchronousTask(ClearLedTask,0,1000,0,event,event);
  #endif

  /* Start the OS so that it starts scheduling the user tasks */
  return OSStartMultitasking();
} /* end of main */


/* SetLED: . */
void SetLedTask(void *argument)
{
  GPIOB->BSRR = GPIO_Pin_14; // Set LED
  OSScheduleSuspendedTask(argument);
  OSEndTask();
} /* end of SetLED */


/* ClearLedTask: . */
void ClearLedTask(void *argument)
{
  static BOOL delay = TRUE;
  if (delay) {
    delay = FALSE;
    OSScheduleSuspendedTask(argument);
  }
  else {
    delay = TRUE;
    GPIOB->BRR = GPIO_Pin_14; // Clear LED
  }
  OSSuspendSynchronousTask();
} /* end of ClearLedTask */
