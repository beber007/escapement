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
/* File COLD_LED1.c:
** Version identifier: February 2012
** Authors: MIS-TIC */

#include "ZottaOS.h"
#include "stm32f10x.h"

static void SetLedTask(void *argument);
static void ClearLedTask(void *argument);

void assert_failed(UINT8* pcFile,UINT32 ulLine ) { while (TRUE); }


int main(void)
{
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

  // Create an aperiodic task
  #if defined(ZOTTAOS_VERSION_HARD)
     OSCreateTask(SetLedTask,0,10000,10000,NULL);
     OSCreateTask(ClearLedTask,0,1000,1000,NULL);
  #elif defined(ZOTTAOS_VERSION_SOFT)
     OSCreateTask(SetLedTask,0,0,10000,10000,1,1,0,NULL);
     OSCreateTask(ClearLedTask,0,0,1000,1000,1,1,0,NULL);
  #endif

  /* Start the OS so that it starts scheduling the user tasks */
  return OSStartMultitasking();
} /* end of main */


/* SetLED: . */
void SetLedTask(void *argument)
{
  GPIOB->BSRR = GPIO_Pin_14; // Set LED
  OSEndTask();
} /* end of SetLED */


/* ClearLedTask: . */
void ClearLedTask(void *argument)
{
  static UINT8 iteration = 1;
  if (iteration == 0) {
     GPIOB->BRR = GPIO_Pin_14; // Clear LED
     iteration = 9;
  }
  else
     iteration--;
  OSEndTask();
} /* end of ClearLedTask */
