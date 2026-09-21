/* Copyright (c) 2026 Bertrand Hurst. All rights reserved.
** Escapement - Lightweight Power-Aware Real-Time OS.
** Derived from prior work; see LICENSE and NOTICE at the root of this repository.
*/
/* File UARTSimpleEchoL1.c: Receives characters that are then forwarded back to the
** sender.
** Version identifier: February 2012
*/

#include "Escapement.h"
#include "Escapement_UART.h"

#include "BoardL1.h"

/* UART transmit FIFO buffer size definitions */
#define UART_TRANSMIT_FIFO_NB_NODE    1
#define UART_TRANSMIT_FIFO_NODE_SIZE  10

#define UART_VECTOR OS_IO_USART2

static void UARTUserReceiveInterruptHandler(UINT8 data);
static void InitializeUART2Hardware(void);


int main(void)
{
  /* Stop timer during debugger connection */
  BoardStopTimerInDebug(ESCAPEMENT_TIMER);
  /* Initialize Hardware */
  SystemInit();
  BoardInitClock();
  /* Initialize Escapement I/O UART drivers */
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
  UINT32 ppre1, pclk1;
  /* Enable USART2 and GPIOA clocks */
  BoardEnablePort(GPIOA);
  RCC->APB1ENR |= RCC_APB1ENR_USART2EN;
  /* Connect PA2 to USART2_Tx and PA3 to USART2_Rx */
  //GPIOA->AFR[0] = (GPIOA->AFR[0] & ~(0xFFu << 8)) | (0x77u << 8);
  /* PA2 and PA3 in alternate function mode, push-pull, 40 MHz, pull-up */
  GPIOA->MODER = (GPIOA->MODER & ~(0xFu << 4)) | (0xAu << 4);
  GPIOA->OTYPER &= ~(PIN(2) | PIN(3));
  GPIOA->OSPEEDR = (GPIOA->OSPEEDR & ~(0xFu << 4)) | (0xFu << 4);
  GPIOA->PUPDR = (GPIOA->PUPDR & ~(0xFu << 4)) | (0x5u << 4);
  /* USART2 configured as follow:
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
