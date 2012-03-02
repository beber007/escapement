/* Copyright (c) 2012 MIS Institute of the HEIG affiliated to the University of
** Applied Sciences of Western Switzerland. All rights reserved.
** Permission to use, copy, modify, and distribute this software and its documentation
** for any purpose, without fee, and without written agreement is hereby granted, pro-
** vided that the above copyright notice, the following three sentences and the authors
** appear in all copies of this software and in the software where it is used.
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
/* File COLD_StatChart.c: .
** Version identifier: February 2012
** Authors: MIS-TIC
*/

#include "ZottaOS.h"
#include "stm32f10x.h"
#include "COLD_StateChart.h"

#define ENABLE_IO_CLK()  RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE)
#define DISABLE_IO_CLK() RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, DISABLE)

/* InitializeFlag: .*/
void InitializeFlag(UINT16 GPIO_Pin)
{
  GPIO_InitTypeDef GPIO_InitStructure;
  /* Enable GPIO_LED clock */
  ENABLE_IO_CLK();
  /* Configure GPIO_LED Pin as Output push-pull */
  GPIO_InitStructure.GPIO_Pin = GPIO_Pin;
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
  GPIO_Init(GPIOB, &GPIO_InitStructure);
} /* end of InitializeFlag */


/* SetLED: . */
void SetLED(UINT16 GPIO_Pin)
{
  ENABLE_IO_CLK();
  GPIOB->BSRR = GPIO_Pin; // Set LED
  DISABLE_IO_CLK();
} /* end of SetLED */


/* ClearLED: . */
void ClearLED(UINT16 GPIO_Pin)
{
  ENABLE_IO_CLK();
  GPIOB->BRR = GPIO_Pin; // Clear LED
  DISABLE_IO_CLK();
} /* end of ClearLED */


/* StateChart1: . */
void StateChart1(void *handle)
{
  switch (GetCurrentState(handle)) {
  case 0:
     InitializeFlag(GPIO_Pin_14);
     GotoNextState(handle,1);
     break;
  case 1:
     SetLED(GPIO_Pin_14);
     ScheduleNextState(handle,2,1000);
     ScheduleNextState(handle,2,2000);
     break;
  case 2:
     ClearLED(GPIO_Pin_14);
     UnScheduleNextState(handle);
     ScheduleNextState(handle,1,9000);
     break;
  default:
  break;
  }
} /* end of StateChart1 */


/* StateChart2: . */
void StateChart2(void *handle)
{
  switch (GetCurrentState(handle)) {
  case 0:
     InitializeFlag(GPIO_Pin_13);
     GotoNextState(handle,1);
     break;
  case 1:
     ScheduleNextState(handle,2,2000);
     SetLED(GPIO_Pin_13);
     break;
  case 2:
     ScheduleNextState(handle,1,18000);
     ClearLED(GPIO_Pin_13);
     break;
  default:
  break;
  }
} /* end of StateChart2 */


/* StateChart3: . */
void StateChart3(void *handle)
{
  switch (GetCurrentState(handle)) {
  case 0:
     InitializeFlag(GPIO_Pin_12);
     GotoNextState(handle,1);
     break;
  case 1:
     ScheduleNextState(handle,2,4000);
     SetLED(GPIO_Pin_12);
     break;
  case 2:
     ScheduleNextState(handle,1,36000);
     ClearLED(GPIO_Pin_12);
     break;
  default:
  break;
  }
} /* end of StateChart3 */


/* StateChart4: . */
void StateChart4(void *handle)
{
  switch (GetCurrentState(handle)) {
  case 0:
     InitializeFlag(GPIO_Pin_15);
     GotoNextState(handle,1);
     break;
  case 1:
     ScheduleNextState(handle,2,3000);
     SetLED(GPIO_Pin_15);
     break;
  case 2:
     ScheduleNextState(handle,1,27000);
     ClearLED(GPIO_Pin_15);
     break;
  default:
  break;
  }
} /* end of StateChart4 */


/* InitStateCharts: . */
void InitStateCharts(void)
{
  #if defined(ZOTTAOS_VERSION_HARD)
     InitStateChart(StateChart1,0,500);
     InitStateChart(StateChart2,0,2000);
     InitStateChart(StateChart3,0,4000);
     InitStateChart(StateChart4,0,3000);
#elif defined(ZOTTAOS_VERSION_SOFT)
     InitStateChart(StateChart1,0,0,500,0);
     InitStateChart(StateChart2,0,0,2000,0);
     InitStateChart(StateChart3,0,0,4000,0);
     InitStateChart(StateChart4,0,0,3000,0);
#endif
} /* end of InitStateCharts */
