/* Copyright (c) 2012 MIS Institute of the HEIG affiliated to the University of
** Applied Sciences of Western Switzerland. All rights reserved.
** IN NO EVENT SHALL THE MIS INSTITUTE NOR THE HEIG NOR THE UNIVERSITY OF APPLIED
** SCIENCES OF WESTERN SWITZERLAND BE LIABLE TO ANY PARTY FOR DIRECT, INDIRECT, SPECIAL,
** INCIDENTAL, OR CONSEQUENTIAL DAMAGES ARISING OUT OF THE USE OF THIS SOFTWARE AND ITS
** DOCUMENTATION, EVEN IF THE MIS INSTITUTE OR THE HEIG OR THE UNIVERSITY OF APPLIED
** SCIENCES OF WESTERN SWITZERLAND HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
** THE MIS INSTITUTE, THE HEIG AND THE UNIVERSITY OF APPLIED SCIENCES OF WESTERN SWITZER-
** LAND SPECIFICALLY DISCLAIMS ANY WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
** WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE. THE SOFTWARE PRO-
** VIDED HEREUNDER IS ON AN "AS IS" BASIS, AND THE MIS INSTITUTE NOR THE HEIG AND NOR THE
** UNIVERSITY OF APPLIED SCIENCES OF WESTERN SWITZERLAND HAVE NO OBLIGATION TO PROVIDE
** MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR MODIFICATIONS.
*/
/* File UARTSimpleEcho.c: Receives characters that are then forwarded back to the sender.
** Version identifier: February 2012
** Authors: MIS-TIC */

#include "ZottaOS.h"
#include "ZottaOS_UART.h"
#include "stm32f10x.h"

/* UART transmit FIFO buffer size definitions */
#define UART_TRANSMIT_FIFO_NB_NODE    1
#define UART_TRANSMIT_FIFO_NODE_SIZE  10

#define UART_VECTOR OS_IO_USART1

static void UARTUserReceiveInterruptHandler(UINT8 data);
static void InitializeUARTHardware(void);

void assert_failed(UINT8* pcFile,UINT32 ulLine )
{
  while (TRUE);
}

int main(void)
{
  /* Stop timer during debugger connection */
  DBGMCU_Config(DBGMCU_TIM4_STOP,ENABLE);
  /* Keep debugger connection during sleep mode */
  DBGMCU_Config(DBGMCU_SLEEP,ENABLE);

  /* Initialize ZottaOS I/O UART drivers */
  OSInitUART(UART_TRANSMIT_FIFO_NB_NODE,UART_TRANSMIT_FIFO_NODE_SIZE,
             UARTUserReceiveInterruptHandler, UART_VECTOR);

  /* Initialize USART or USCI 0 hardware */
  InitializeUARTHardware();

  /* Start the OS so that it runs the idle task, which puts the processor to sleep when
  ** there are no interrupts. */
  return OSStartMultitasking();
} /* end of main */


/* InitializeUARTHardware: Initializes USART or USCI 0 hardware. */
void InitializeUARTHardware(void)
{
  USART_InitTypeDef USART_InitStructure;
  GPIO_InitTypeDef GPIO_InitStructure;
  NVIC_InitTypeDef NVIC_InitStructure;
  /* Enable USART1, GPIOA, GPIOx and AFIO clocks */
  RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1 | RCC_APB2Periph_GPIOA | RCC_APB2Periph_AFIO, ENABLE);
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
  NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
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
