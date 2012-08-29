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
/* File UARTSimpleEchoL1.c: Receives characters that are then forwarded back to the
** sender.
** Version identifier: February 2012
** Authors: MIS-TIC */

#include "ZottaOS.h"
#include "ZottaOS_UART.h"

#include "stm32l1xx.h"

/* UART transmit FIFO buffer size definitions */
#define UART_TRANSMIT_FIFO_NB_NODE    1
#define UART_TRANSMIT_FIFO_NODE_SIZE  10

#define UART_VECTOR OS_IO_USART2

static void UARTUserReceiveInterruptHandler(UINT8 data);
static void InitializeUART2Hardware(void);


int main(void)
{
  /* Stop timer during debugger connection */
  #if ZOTTAOS_TIMER == OS_IO_TIM11
     DBGMCU_APB2PeriphConfig(DBGMCU_TIM11_STOP,ENABLE);
  #elif ZOTTAOS_TIMER == OS_IO_TIM10
     DBGMCU_APB2PeriphConfig(DBGMCU_TIM10_STOP,ENABLE);
  #elif ZOTTAOS_TIMER == OS_IO_TIM9
     DBGMCU_APB2PeriphConfig(DBGMCU_TIM9_STOP,ENABLE);
  #elif ZOTTAOS_TIMER == OS_IO_TIM5
     DBGMCU_APB1PeriphConfig(DBGMCU_TIM5_STOP,ENABLE);
  #elif ZOTTAOS_TIMER == OS_IO_TIM4
     DBGMCU_APB1PeriphConfig(DBGMCU_TIM4_STOP,ENABLE);
  #elif ZOTTAOS_TIMER == OS_IO_TIM3
     DBGMCU_APB1PeriphConfig(DBGMCU_TIM3_STOP,ENABLE);
  #elif ZOTTAOS_TIMER == OS_IO_TIM2
     DBGMCU_APB1PeriphConfig(DBGMCU_TIM2_STOP,ENABLE);
  #endif
  /* Initialize Hardware */
  SystemInit();
  /* Initialize ZottaOS I/O UART drivers */
  OSInitUART(UART_TRANSMIT_FIFO_NB_NODE,UART_TRANSMIT_FIFO_NODE_SIZE,
             UARTUserReceiveInterruptHandler, UART_VECTOR);

  /* Initialize USART2 hardware */
  InitializeUART2Hardware();

  /* Start the OS so that it runs the idle task, which puts the processor to sleep when
  ** there are no interrupts. */
  return OSStartMultitasking(NULL,NULL);
} /* end of main */


/* InitializeUART2Hardware: Initializes USART2 hardware. Tx is connected to PA2 and Rx
**  is connected to PA 3*/
void InitializeUART2Hardware(void)
{
  USART_InitTypeDef USART_InitStructure;
  GPIO_InitTypeDef GPIO_InitStructure;
  NVIC_InitTypeDef NVIC_InitStructure;
  /* Enable USART2 and GPIOA clocks */
  RCC_AHBPeriphClockCmd(RCC_AHBPeriph_GPIOA,ENABLE);
  RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART2,ENABLE);
  /* Connect PA2 to USART2_Tx*/
  //GPIO_PinAFConfig(GPIOA,GPIO_PinSource2,GPIO_AF_USART2);
  /* Connect PA3 to USART2_Rx*/
  //GPIO_PinAFConfig(GPIOA,GPIO_PinSource3,GPIO_AF_USART2);
  /* Configure USART2 Tx (PA2) as alternate function push-pull */
  GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
  GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_40MHz;
  GPIO_InitStructure.GPIO_Pin = GPIO_Pin_2;
  GPIO_Init(GPIOA, &GPIO_InitStructure);
  /* Configure USART2 Rx (PA3) as alternate function */
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
  GPIO_InitStructure.GPIO_Pin = GPIO_Pin_3;
  GPIO_Init(GPIOA, &GPIO_InitStructure);
  /* USART2 configuration */
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
  /* Configure USART2 */
  USART_Init(USART2,&USART_InitStructure);
  /* Enable USART2 Receive and Transmit interrupts */
  USART_ITConfig(USART2,USART_IT_RXNE,ENABLE);
  //USART_ITConfig(USART2,USART_IT_TXE,ENABLE);
  /* Enable the USART2 */
  USART_Cmd(USART2, ENABLE);
  /* Enable the USART2 Interrupt */
  NVIC_InitStructure.NVIC_IRQChannel = USART2_IRQn;
//  NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
  NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
  NVIC_Init(&NVIC_InitStructure);
} /* end of InitializeUARTHardware */


/* UARTUserReceiveInterruptHandler: This function is called every time a new byte is
** received, and it simply copies the byte into the output queue of the UART. */
void UARTUserReceiveInterruptHandler(UINT8 data)
{
  UINT8 *tmp;
  tmp = (UINT8 *)OSGetFreeNodeUART(UART_VECTOR);
  *tmp = data;
  OSEnqueueUART(tmp,1,UART_VECTOR);
} /* end of UARTUserReceiveInterruptHandler */
