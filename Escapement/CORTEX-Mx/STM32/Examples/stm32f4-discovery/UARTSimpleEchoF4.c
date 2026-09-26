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
** Authors: MIS-TIC
**
** Escapement - Lightweight Power-Aware Real-Time OS, derived from ZottaOS.
** Modifications Copyright (c) 2026 Bertrand Hurst, distributed under the same terms;
** see LICENSE and NOTICE at the root of this repository.
*/
/* File UARTSimpleEchoF4.c: Receives characters that are then forwarded back to the
** sender.
** Version identifier: February 2012
*/

#include "Escapement.h"
#include "Escapement_UART.h"

#include "BoardF4.h"

/* Transmit queue: the echo sends one byte at a time, but a few buffers absorb a short
** burst of input. */
#define UART_TRANSMIT_FIFO_NB_NODE    8
#define UART_TRANSMIT_FIFO_NODE_SIZE  1

#define UART_VECTOR OS_IO_USART2

static void UARTUserReceiveInterruptHandler(UINT8 data);
static void InitializeUART2Hardware(void);


int main(void)
{
  BoardStopTimerInDebug(ESCAPEMENT_TIMER);
  /* Keep debugger connection during sleep mode */
  BoardKeepDebugInSleep();
  /* Initialize Hardware */
  SystemInit();
  BoardInitClock();
  /* Initialize Escapement I/O UART drivers */
  OSInitUART(UART_TRANSMIT_FIFO_NB_NODE,UART_TRANSMIT_FIFO_NODE_SIZE,
             UARTUserReceiveInterruptHandler,UART_VECTOR);
  /* Initialize USART2 hardware */
  InitializeUART2Hardware();
  /* Start the OS so that it runs the idle task, which puts the processor to sleep when
  ** there are no interrupts. */
  return OSStartMultitasking(NULL,NULL);
} /* end of main */


/* InitializeUART2Hardware: Initializes USART2 hardware. Tx is connected to PA2 and Rx
**  is connected to PA3 */
void InitializeUART2Hardware(void)
{
  UINT32 ppre1, pclk1;
  /* Enable USART2 and GPIOA clocks */
  BoardEnablePort(GPIOA);
  RCC->APB1ENR |= RCC_APB1ENR_USART2EN;
  (void)RCC->APB1ENR;     // the USART clocked before it is written (ES0182 rev 19, 2.2.13)
  /* Connect PA2 to USART2_Tx and PA3 to USART2_Rx */
  GPIOA->AFR[0] = (GPIOA->AFR[0] & ~(0xFFu << 8)) | (0x77u << 8);
  /* PA2 and PA3 in alternate function mode, push-pull, 50 MHz, pull-up */
  GPIOA->MODER = (GPIOA->MODER & ~(0xFu << 4)) | (0xAu << 4);
  GPIOA->OTYPER &= ~(PIN(2) | PIN(3));
  GPIOA->OSPEEDR = (GPIOA->OSPEEDR & ~(0xFu << 4)) | (0xAu << 4);
  GPIOA->PUPDR = (GPIOA->PUPDR & ~(0xFu << 4)) | (0x5u << 4);
  /* USART2 configured as follows:
     - BaudRate = 115200 baud, from the APB1 clock with oversampling by 16
     - Word Length = 8 Bits
     - One Stop Bit
     - No parity
     - Hardware flow control disabled (RTS and CTS signals)
     - Receive and transmit enabled, with the receive interrupt */
  SystemCoreClockUpdate();
  ppre1 = (RCC->CFGR & RCC_CFGR_PPRE1) / RCC_CFGR_PPRE1_0;
  pclk1 = (ppre1 & 4) ? SystemCoreClock >> ((ppre1 & 3) + 1) : SystemCoreClock;
  USART2->BRR = (pclk1 + 115200 / 2) / 115200;
  USART2->CR2 &= ~USART_CR2_STOP;
  USART2->CR3 &= ~(USART_CR3_RTSE | USART_CR3_CTSE);
  USART2->CR1 = USART_CR1_RE | USART_CR1_TE | USART_CR1_RXNEIE;
  /* Enable the USART2 */
  USART2->CR1 |= USART_CR1_UE;
  /* Enable the USART2 Interrupt, one priority group below the kernel timer */
  NVIC_SetPriority(USART2_IRQn, (TIMER_PRIORITY + 1) << (PRIGROUP - 3));
  NVIC_EnableIRQ(USART2_IRQn);
} /* end of InitializeUART2Hardware */


/* UARTUserReceiveInterruptHandler: This function is called every time a new byte is
** received, and it simply copies the byte into the output queue of the UART. */
void UARTUserReceiveInterruptHandler(UINT8 data)
{
  UINT8 *tmp = (UINT8 *)OSGetFreeNodeUART(UART_VECTOR);
  if (tmp != NULL) {   // all buffers still being sent: the byte is dropped
     *tmp = data;
     OSEnqueueUART(tmp,1,UART_VECTOR);
  }
} /* end of UARTUserReceiveInterruptHandler */
