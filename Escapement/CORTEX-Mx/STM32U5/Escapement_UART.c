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
/* File Escapement_UART.c: UART driver, transposed from the RP2350 one onto USART1 of the
** STM32U575 (RM0456: USART, GPIO and RCC). Reception hands each byte to a handler
** supplied by the application, from interrupt context. Transmission goes through a queue
** of buffers served by the interrupt, so that a task never waits on the port.
**
** USART1 goes out on PA9 (TX) and PA10 (RX), alternate function 7, which the NUCLEO-
** U575ZI-Q routes to the virtual serial port of its ST-LINK. Its FIFO is left off: the
** transmit interrupt then reflects a state, the transmit register empty, and fires as
** soon as it is enabled while there is room, so that enabling it is all a new buffer
** needs, where the PL011 of the RP2350 had to be primed.
** Platform version: STM32U575 (NUCLEO-U575ZI-Q).
*/

#include "Escapement.h"
#include "Escapement_UART.h"

#define USART1_BASE          0x40013800
#define USART_CR1            0x00
#define USART_BRR            0x0C
#define USART_ISR            0x1C
#define USART_ICR            0x20
#define USART_RDR            0x24
#define USART_TDR            0x28

#define CR1_UE               (1u << 0)
#define CR1_RE               (1u << 2)
#define CR1_TE               (1u << 3)
#define CR1_RXNEIE           (1u << 5)
#define CR1_TXEIE            (1u << 7)
#define ISR_ORE              (1u << 3)
#define ISR_RXNE             (1u << 5)
#define ISR_TXE              (1u << 7)
#define ICR_ORECF            (1u << 3)

#define GPIOA_BASE           0x42020000
#define GPIOA_MODER          *((volatile UINT32 *)(GPIOA_BASE + 0x00))
#define GPIOA_AFRH           *((volatile UINT32 *)(GPIOA_BASE + 0x24))
#define TX_PIN               9
#define RX_PIN               10
#define MODE_AF              2u
#define AF_USART1            7u

#define RCC_AHB2ENR1         *((volatile UINT32 *)(0x46020C00 + 0x8C))
#define RCC_APB2ENR          *((volatile UINT32 *)(0x46020C00 + 0xA4))
#define RCC_AHB2ENR1_GPIOAEN (1u << 0)
#define RCC_APB2ENR_USART1EN (1u << 14)

/* One bit per interrupt, 32 to a word. */
#define NVIC_ISER(irq)       ((volatile UINT32 *)0xE000E100)[(irq) >> 5]
#define NVIC_ICER(irq)       ((volatile UINT32 *)0xE000E180)[(irq) >> 5]
#define NVIC_BIT(irq)        (1u << ((irq) & 0x1F))

/* USART1 is clocked by PCLK2, the system clock with the APB prescaler at 1 (USART1SEL
** left at its reset value). */
#define BAUD_RATE            115200u


typedef struct UART_INTERRUPT_DESCRIPTOR { // Interrupt handler opaque descriptor
  void (*InterruptHandler)(struct UART_INTERRUPT_DESCRIPTOR *);
  UINT32 Base;                     // First register of the USART
  UINT8 CurrentBufferIndex;        // Next byte to transmit from the current buffer
  void *FifoArray;                 // Descriptor of the transmit queue
  UINT16 NbTransmit;               // Number of bytes left to transmit
  UINT8 *CurrentBuffer;            // Buffer being emptied onto the port
  void (*UserReceiveInterruptHandler)(UINT8 data); // Application receive handler
} UART_INTERRUPT_DESCRIPTOR;

#define REG(des,off) *((volatile UINT32 *)((des)->Base + (off)))

static void InterruptHandler(UART_INTERRUPT_DESCRIPTOR *descriptor);
static void Transmit(UART_INTERRUPT_DESCRIPTOR *descriptor);
static void TakeNextBuffer(UART_INTERRUPT_DESCRIPTOR *descriptor);


/* OSInitUART: Creates and binds the descriptor used by the other functions, and brings the
** hardware up. */
BOOL OSInitUART(UINT8 maxNodes, UINT8 maxNodeSize, void (*ReceiveHandler)(UINT8),
                UINT8 interruptIndex)
{
  UART_INTERRUPT_DESCRIPTOR *descriptor;
  #ifdef DEBUG_MODE
     if (interruptIndex != OS_IO_USART1)
        while (TRUE);                  // only USART1 is driven on this port
  #endif
  if ((descriptor =
        (UART_INTERRUPT_DESCRIPTOR *)OSMalloc(sizeof(UART_INTERRUPT_DESCRIPTOR))) == NULL)
     return FALSE;
  descriptor->InterruptHandler = InterruptHandler;
  descriptor->UserReceiveInterruptHandler = ReceiveHandler;
  descriptor->FifoArray = OSInitFIFOQueue(maxNodes,maxNodeSize);
  descriptor->NbTransmit = 0;
  descriptor->CurrentBuffer = NULL;
  descriptor->CurrentBufferIndex = 0;
  descriptor->Base = USART1_BASE;
  RCC_AHB2ENR1 |= RCC_AHB2ENR1_GPIOAEN;
  RCC_APB2ENR |= RCC_APB2ENR_USART1EN;
  (void)RCC_APB2ENR;                   // the clocks run before the blocks are written
  /* PA9 and PA10 to alternate function 7. */
  GPIOA_AFRH = (GPIOA_AFRH & ~(0xFu << 4 * (TX_PIN - 8) | 0xFu << 4 * (RX_PIN - 8))) |
               AF_USART1 << 4 * (TX_PIN - 8) | AF_USART1 << 4 * (RX_PIN - 8);
  GPIOA_MODER = (GPIOA_MODER & ~(3u << 2 * TX_PIN | 3u << 2 * RX_PIN)) |
                MODE_AF << 2 * TX_PIN | MODE_AF << 2 * RX_PIN;
  /* 8 bits, no parity, one stop bit, oversampling by 16: the divisor is the clock over
  ** the baud rate, rounded. */
  REG(descriptor,USART_CR1) = 0;
  REG(descriptor,USART_BRR) = (OS_SYSTEM_CLOCK_HZ + BAUD_RATE / 2) / BAUD_RATE;
  /* Reception interrupt only; transmission is enabled by OSEnqueueUART when there is
  ** something to send. */
  REG(descriptor,USART_CR1) = CR1_UE | CR1_RE | CR1_TE | CR1_RXNEIE;
  OSSetISRDescriptor(interruptIndex,descriptor);
  NVIC_ISER(interruptIndex) = NVIC_BIT(interruptIndex);
  return TRUE;
} /* end of OSInitUART */


/* OSGetFreeNodeUART: Returns a free buffer to be filled by the application. */
void *OSGetFreeNodeUART(UINT8 interruptIndex)
{
  UART_INTERRUPT_DESCRIPTOR *descriptor =
                          (UART_INTERRUPT_DESCRIPTOR *)OSGetISRDescriptor(interruptIndex);
  return OSGetFreeNodeFIFO(descriptor->FifoArray);
} /* end of OSGetFreeNodeUART */


/* OSReleaseNodeUART: Gives a buffer back without sending it. */
void OSReleaseNodeUART(void *buffer, UINT8 interruptIndex)
{
  UART_INTERRUPT_DESCRIPTOR *descriptor =
                          (UART_INTERRUPT_DESCRIPTOR *)OSGetISRDescriptor(interruptIndex);
  OSReleaseNodeFIFO(descriptor->FifoArray,buffer);
} /* end of OSReleaseNodeUART */


/* OSEnqueueUART: Queues a buffer and enables the transmit interrupt, which will drain it.
** CR1 is read, modified and written by the interrupt too: the interrupt of this USART
** alone is masked around the write, rather than every interrupt, so that the timer of the
** kernel keeps its latency. */
void OSEnqueueUART(void *buffer, UINT8 dataSize, UINT8 interruptIndex)
{
  UART_INTERRUPT_DESCRIPTOR *descriptor =
                          (UART_INTERRUPT_DESCRIPTOR *)OSGetISRDescriptor(interruptIndex);
  OSEnqueueFIFO(descriptor->FifoArray,buffer,dataSize);
  NVIC_ICER(interruptIndex) = NVIC_BIT(interruptIndex);
  asm volatile ("dsb" ::: "memory");   // the mask must hold before the next instruction
  asm volatile ("isb" ::: "memory");
  REG(descriptor,USART_CR1) |= CR1_TXEIE;
  asm volatile ("" ::: "memory");
  NVIC_ISER(interruptIndex) = NVIC_BIT(interruptIndex);
} /* end of OSEnqueueUART */


/* Transmit: Writes one byte when the transmit register is empty, taking the next buffer
** from the queue whenever the current one runs out, and disables the transmit interrupt
** once everything has been sent. Called by the interrupt. */
static void Transmit(UART_INTERRUPT_DESCRIPTOR *des)
{
  if (des->CurrentBuffer == NULL)
     TakeNextBuffer(des);
  if (des->CurrentBuffer == NULL) {
     REG(des,USART_CR1) &= ~CR1_TXEIE;
     return;
  }
  REG(des,USART_TDR) = des->CurrentBuffer[des->CurrentBufferIndex++];
  des->NbTransmit -= 1;
  if (des->NbTransmit == 0) {
     OSReleaseNodeFIFO(des->FifoArray,des->CurrentBuffer);
     des->CurrentBuffer = NULL;
  }
} /* end of Transmit */


/* TakeNextBuffer: Takes the next buffer to send from the queue, skipping any buffer that
** holds nothing. A buffer enqueued with a size of zero has to be dropped here: the count
** of remaining bytes is unsigned, so decrementing it from zero would wrap around and empty
** 64 Kbytes of memory onto the port. */
static void TakeNextBuffer(UART_INTERRUPT_DESCRIPTOR *des)
{
  des->CurrentBufferIndex = 0;
  while ((des->CurrentBuffer = (UINT8 *)OSDequeueFIFO(des->FifoArray,&des->NbTransmit))
          != NULL && des->NbTransmit == 0)
     OSReleaseNodeFIFO(des->FifoArray,des->CurrentBuffer);
} /* end of TakeNextBuffer */


/* InterruptHandler: Single ISR of the USART. On reception it hands the byte to the
** application; an overrun, a byte lost because the previous one was not read in time, is
** cleared so that reception goes on. On transmission it writes the next byte. */
static void InterruptHandler(UART_INTERRUPT_DESCRIPTOR *des)
{
  UINT32 status = REG(des,USART_ISR);
  if (status & ISR_RXNE) {
     UINT8 data = (UINT8)REG(des,USART_RDR);
     if (des->UserReceiveInterruptHandler != NULL)
        des->UserReceiveInterruptHandler(data);
  }
  if (status & ISR_ORE)
     REG(des,USART_ICR) = ICR_ORECF;
  if ((status & ISR_TXE) && (REG(des,USART_CR1) & CR1_TXEIE))
     Transmit(des);
} /* end of InterruptHandler */
