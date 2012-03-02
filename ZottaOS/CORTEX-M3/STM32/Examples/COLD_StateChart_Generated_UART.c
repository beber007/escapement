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
#include "ZottaOS_UART.h"
#include "stm32f10x.h"
#include "COLD_StateChart.h"

/* UART transmit FIFO buffer size definitions */
#define UART_TRANSMIT_FIFO_NB_NODE    2
#define UART_TRANSMIT_FIFO_NODE_SIZE  10

#define ENABLE_UART_CLK()  RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1 | RCC_APB2Periph_GPIOA | RCC_APB2Periph_AFIO, ENABLE)
#define DISABLE_UART_CLK() RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1 | RCC_APB2Periph_GPIOA | RCC_APB2Periph_AFIO, DISABLE)

static void InitializeUARTHardware(void);
static void UARTUserReceiveInterruptHandler(UINT8 data);

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
  GPIOB->BSRR = GPIO_Pin;
  DISABLE_IO_CLK();
} /* end of SetLED */


/* ClearLED: . */
void ClearLED(UINT16 GPIO_Pin)
{
  ENABLE_IO_CLK();
  GPIOB->BRR = GPIO_Pin;
  DISABLE_IO_CLK();
} /* end of ClearLED */


/* StateChart1: . */
void StateChart1(void *handle)
{
  UINT8 *tmp;
  switch (GetCurrentState(handle)) {
  case 0:
     /* Initialize ZottaOS I/O UART drivers */
     InitializeUARTHardware();
     InitializeFlag(GPIO_Pin_14);
     GotoNextState(handle,1);
     break;
  case 1:
     ENABLE_UART_CLK();
     SetLED(GPIO_Pin_14); // Enable LED
     ScheduleNextState(handle,2,500);
     tmp = (UINT8 *)OSGetFreeNodeUART(OS_IO_USART1);
     if (tmp != NULL) {
         tmp[0] = '1';
        OSEnqueueUART(tmp,1,OS_IO_USART1);
     }
     break;
  case 2:
     ClearLED(GPIO_Pin_14); // Clear LED
     DISABLE_UART_CLK();
     ScheduleNextState(handle,1,1000);
     break;
  default:
  break;
  }
} /* end of StateChart1 */


/* InitStateCharts: . */
void InitStateCharts(void)
{
  OSInitUART(UART_TRANSMIT_FIFO_NB_NODE,UART_TRANSMIT_FIFO_NODE_SIZE,
             UARTUserReceiveInterruptHandler, OS_IO_USART1);
  #if defined(ZOTTAOS_VERSION_HARD)
     InitStateChart(StateChart1,0,100);
  #elif defined(ZOTTAOS_VERSION_SOFT)
     InitStateChart(StateChart1,0,0,100,0);
  #endif
} /* end of InitStateCharts */


/* UARTUserReceiveInterruptHandler: This function is called every time a new byte is
** received, and it simply copies the byte into the output queue of the UART. */
void UARTUserReceiveInterruptHandler(UINT8 data)
{
  UINT8 *tmp;
  tmp = (UINT8 *)OSGetFreeNodeUART(OS_IO_USART1);
  if (tmp != NULL) {
     *tmp = data;
     OSEnqueueUART(tmp,1,OS_IO_USART1);
  }
} /* end of UARTUserReceiveInterruptHandler */


/* InitializeUARTHardware: Initializes USART hardware. */
void InitializeUARTHardware(void)
{
  USART_InitTypeDef USART_InitStructure;
  GPIO_InitTypeDef GPIO_InitStructure;
  NVIC_InitTypeDef NVIC_InitStructure;
  /* Enable USART1, GPIOA, GPIOx and AFIO clocks */
  ENABLE_UART_CLK();
  /* Configure USART1 Rx (PA.10) as input floating */
  GPIO_InitStructure.GPIO_Pin = GPIO_Pin_10;
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
  GPIO_Init(GPIOA, &GPIO_InitStructure);
  /* Configure USART1 Tx (PA.09) as alternate function push-pull */
  GPIO_InitStructure.GPIO_Pin = GPIO_Pin_9;
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
  GPIO_Init(GPIOA, &GPIO_InitStructure);
  /* USART1 configuration */
  /* USART configured as follow:
     - BaudRate = 115200 baud
     - Word Length = 8 Bits
     - One Stop Bit
     - No parity
     - Hardware flow control disabled (RTS and CTS signals)
     - Receive and transmit enabled */
  USART_InitStructure.USART_BaudRate = 115200;
  USART_InitStructure.USART_WordLength = USART_WordLength_8b;
  USART_InitStructure.USART_StopBits = USART_StopBits_1;
  USART_InitStructure.USART_Parity = USART_Parity_No;
  USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
  USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
  /* Configure USART1 */
  USART_Init(USART1, &USART_InitStructure);
  /* Enable USART1 Receive and Transmit interrupts */
  USART_ITConfig(USART1,USART_IT_RXNE,ENABLE);
  //USART_ITConfig(USART1,USART_IT_TXE,ENABLE);
  /* Enable the USART1 */
  USART_Cmd(USART1, ENABLE);
  /* Enable the USART1 Interrupt */
  NVIC_InitStructure.NVIC_IRQChannel = USART1_IRQn;
  NVIC_InitStructure.NVIC_IRQChannelSubPriority = 5;
  NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
  NVIC_Init(&NVIC_InitStructure);
} /* end of InitializeUARTHardware */
