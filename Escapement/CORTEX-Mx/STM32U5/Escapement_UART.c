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
** USART1 goes out on PB6 (TX) and PB7 (RX), alternate function 7, D1 and D0 of the
** connector of the Arduino UNO Q (datasheet ABX00162/ABX00173, 9.6 JDIGITAL). LPUART1,
** whose registers sit at the same offsets, goes to the board's Linux processor on PG7
** (TX) and PG8 (RX), alternate function 8, where Linux sees /dev/ttyHS1 (Zephyr's
** description of the board, arduino_uno_q-common.dtsi). Its flow control is not used,
** but its RTS line, PG6, is also the CTS of the Linux side's UART (qup2 on gpio6, pulled
** neither way there): left in analog mode, as after a reset, it floated, and read high,
** "do not send", on 2026-09-28. PG6 is driven low, "ready", instead, which leaves nothing
** to chance. It was not what stopped Linux's transmitter that day: driven low over SWD,
** it did not start it again.
** Either is chosen by its interrupt, OS_IO_USART1 or OS_IO_LPUART1. The transmit
** interrupt reflects a state, room in the transmit register or FIFO, and fires as soon as
** it is enabled while there is room, so that enabling it is all a new buffer needs, where
** the PL011 of the RP2350 had to be primed.
**
** LPUART1 receives through Stop 2 (Escapement_Stop2.c), which USART1 cannot (RM0456,
** table 686): its kernel clock is HSI16, which the LPUART wakes itself as a byte comes
** (UESM, autonomous mode, RM0456 67.4.15 and 11.4.24), and its receive FIFO holds the
** 8 bytes that come while the clock of the chip is raised again, interrupts masked, up to
** some 900 us. The first byte is sampled while HSI16 starts, up to 3.6 us (DS13086,
** table 82): at 115,200 baud that is 3.8 % of the frame, past the 3.41 % the receiver
** tolerates (RM0456, 67.4.15), hence 57,600 baud on LPUART1 by default; USART1 stays at
** 115,200, with its FIFO too. An image whose client wakes the chip with a byte of its own
** before each message, the rest coming in the window that byte opens (Escapement_Stop2.h),
** may take 115,200 on LPUART1 too, with OS_LPUART1_BAUD_RATE (SleepU5).
** Platform version: STM32U585 (Arduino UNO Q).
*/

#include "Escapement.h"
#include "Escapement_UART.h"

#define USART1_BASE          0x40013800
#define LPUART1_BASE         0x46002400
#define USART_CR1            0x00
#define USART_BRR            0x0C
#define USART_ISR            0x1C
#define USART_ICR            0x20
#define USART_RDR            0x24
#define USART_TDR            0x28

#define CR1_UE               (1u << 0)
#define CR1_UESM             (1u << 1)
#define CR1_RE               (1u << 2)
#define CR1_TE               (1u << 3)
#define CR1_RXNEIE           (1u << 5)
#define CR1_TXEIE            (1u << 7)   /* TXFNFIE with the FIFO */
#define CR1_FIFOEN           (1u << 29)
#define ISR_ORE              (1u << 3)
#define ISR_RXNE             (1u << 5)   /* RXFNE with the FIFO */
#define ISR_TC               (1u << 6)
#define ISR_TXE              (1u << 7)
#define ICR_ORECF            (1u << 3)

#define GPIOA_BASE           0x42020000
#define GPIOA_MODER          *((volatile UINT32 *)(GPIOA_BASE + 0x00))
#define GPIOA_AFRH           *((volatile UINT32 *)(GPIOA_BASE + 0x24))
#define NUCLEO_TX_PIN        9
#define NUCLEO_RX_PIN        10
#define RCC_AHB2ENR1_GPIOAEN (1u << 0)
#define GPIOB_BASE           0x42020400
#define GPIOB_MODER          *((volatile UINT32 *)(GPIOB_BASE + 0x00))
#define GPIOB_AFRL           *((volatile UINT32 *)(GPIOB_BASE + 0x20))
#define TX_PIN               6
#define RX_PIN               7
#define MODE_AF              2u
#define AF_USART1            7u
#define GPIOG_BASE           0x42021800
#define GPIOG_MODER          *((volatile UINT32 *)(GPIOG_BASE + 0x00))
#define GPIOG_AFRL           *((volatile UINT32 *)(GPIOG_BASE + 0x20))
#define GPIOG_AFRH           *((volatile UINT32 *)(GPIOG_BASE + 0x24))
#define GPIOG_BSRR           *((volatile UINT32 *)(GPIOG_BASE + 0x18))
#define LP_RTS_PIN           6
#define LP_TX_PIN            7
#define LP_RX_PIN            8
#define AF_LPUART1           8u
#define MODE_OUTPUT          1u

#define RCC_AHB2ENR1         *((volatile UINT32 *)(0x46020C00 + 0x8C))
#define RCC_APB2ENR          *((volatile UINT32 *)(0x46020C00 + 0xA4))
#define RCC_APB3ENR          *((volatile UINT32 *)(0x46020C00 + 0xA8))
#define RCC_APB3SMENR        *((volatile UINT32 *)(0x46020C00 + 0xD0))
#define RCC_SRDAMR           *((volatile UINT32 *)(0x46020C00 + 0xD8))
#define RCC_CCIPR3           *((volatile UINT32 *)(0x46020C00 + 0xE8))
#define RCC_CR               *((volatile UINT32 *)(0x46020C00 + 0x00))
#define RCC_CR_HSION         (1u << 8)
#define RCC_CR_HSIRDY        (1u << 10)
#define LPUART1SEL_MASK      7u
#define LPUART1SEL_HSI16     2u
#define RCC_AHB2ENR1_GPIOBEN (1u << 1)
#define RCC_AHB2ENR1_GPIOGEN (1u << 6)
#define RCC_APB2ENR_USART1EN (1u << 14)
#define RCC_APB3ENR_LPUART1EN (1u << 6)   /* and LPUART1SMEN, LPUART1AMEN */
/* Port G from PG2 is supplied by VDDIO2, which must be declared valid before the port
** is used (RM0456, PWR_SVMCR.IO2SV); the clock of PWR is on since the clock set-up. */
#define PWR_SVMCR            *((volatile UINT32 *)(0x46020800 + 0x10))
#define PWR_SVMCR_IO2SV      (1u << 29)

/* One bit per interrupt, 32 to a word. */
#define NVIC_ISER(irq)       ((volatile UINT32 *)0xE000E100)[(irq) >> 5]
#define NVIC_ISPR(irq)       ((volatile UINT32 *)0xE000E200)[(irq) >> 5]
#define NVIC_BIT(irq)        (1u << ((irq) & 0x1F))

/* USART1 is clocked by PCLK2, the system clock with the APB prescaler at 1 (USART1SEL left
** at its reset value); LPUART1 by HSI16. */
#define BAUD_RATE            115200u
#ifndef OS_LPUART1_BAUD_RATE
   #define OS_LPUART1_BAUD_RATE 57600u
#endif
#define LP_BAUD_RATE         OS_LPUART1_BAUD_RATE
#define HSI16_HZ             16000000u


typedef struct UART_INTERRUPT_DESCRIPTOR { // Interrupt handler opaque descriptor
  void (*InterruptHandler)(struct UART_INTERRUPT_DESCRIPTOR *);
  UINT32 Base;                     // First register of the USART
  UINT8 CurrentBufferIndex;        // Next byte to transmit from the current buffer
  void *FifoArray;                 // Descriptor of the transmit queue
  UINT16 NbTransmit;               // Number of bytes left to transmit
  UINT8 *CurrentBuffer;            // Buffer being emptied onto the port
  void (*UserReceiveInterruptHandler)(UINT8 data); // Application receive handler
  UINT32 Overruns;                 // Bytes lost, one not read before the next came
  volatile BOOL Prime;             // A buffer queued, for the interrupt to send
} UART_INTERRUPT_DESCRIPTOR;

#define REG(des,off) *((volatile UINT32 *)((des)->Base + (off)))

/* A byte came on LPUART1 since _OSUARTReceived last asked: set by the interrupt, read and
** cleared by the idle task, interrupts masked. */
static volatile BOOL LinkReceived;

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
     if (interruptIndex != OS_IO_USART1 && interruptIndex != OS_IO_LPUART1)
        while (TRUE);                  // only USART1 and LPUART1 are driven on this port
  #endif
  if ((descriptor =
        (UART_INTERRUPT_DESCRIPTOR *)OSMalloc(sizeof(UART_INTERRUPT_DESCRIPTOR))) == NULL)
     return FALSE;
  descriptor->InterruptHandler = InterruptHandler;
  descriptor->UserReceiveInterruptHandler = ReceiveHandler;
  if ((descriptor->FifoArray = OSInitFIFOQueue(maxNodes,maxNodeSize)) == NULL)
     return FALSE;                     // TRUE promised the queue too (2026-09-29)
  descriptor->NbTransmit = 0;
  descriptor->CurrentBuffer = NULL;
  descriptor->CurrentBufferIndex = 0;
  descriptor->Overruns = 0;
  descriptor->Prime = FALSE;
  if (interruptIndex == OS_IO_LPUART1) {
     descriptor->Base = LPUART1_BASE;
     PWR_SVMCR |= PWR_SVMCR_IO2SV;
     RCC_AHB2ENR1 |= RCC_AHB2ENR1_GPIOGEN;
     RCC_CR |= RCC_CR_HSION;
     while ((RCC_CR & RCC_CR_HSIRDY) == 0);
     RCC_CCIPR3 = (RCC_CCIPR3 & ~LPUART1SEL_MASK) | LPUART1SEL_HSI16;
     RCC_APB3ENR |= RCC_APB3ENR_LPUART1EN;
     RCC_APB3SMENR |= RCC_APB3ENR_LPUART1EN;
     RCC_SRDAMR |= RCC_APB3ENR_LPUART1EN;
     (void)RCC_APB3ENR;                // the clocks run before the blocks are written
     /* PG7 and PG8 to alternate function 8. */
     GPIOG_AFRL = (GPIOG_AFRL & ~(0xFu << 4 * LP_TX_PIN)) | AF_LPUART1 << 4 * LP_TX_PIN;
     GPIOG_AFRH = (GPIOG_AFRH & ~(0xFu << 4 * (LP_RX_PIN - 8))) |
                  AF_LPUART1 << 4 * (LP_RX_PIN - 8);
     GPIOG_MODER = (GPIOG_MODER & ~(3u << 2 * LP_TX_PIN | 3u << 2 * LP_RX_PIN)) |
                   MODE_AF << 2 * LP_TX_PIN | MODE_AF << 2 * LP_RX_PIN;
     #ifndef BOARD_NUCLEO_U575
        /* PG6 low, then an output: the Linux side's CTS held at "ready" (see above). */
        GPIOG_BSRR = 1u << (16 + LP_RTS_PIN);
        GPIOG_MODER = (GPIOG_MODER & ~(3u << 2 * LP_RTS_PIN)) | MODE_OUTPUT << 2 * LP_RTS_PIN;
     #endif
     /* 8 bits, no parity, one stop bit: the divisor of a low-power UART is 256 times the
     ** clock over the baud rate, rounded (RM0456, LPUART_BRR). */
     REG(descriptor,USART_CR1) = 0;
     REG(descriptor,USART_BRR) = (UINT32)((256ull * HSI16_HZ + LP_BAUD_RATE / 2) /
                                          LP_BAUD_RATE);
     /* FIFOEN and UESM while the LPUART is disabled. */
     REG(descriptor,USART_CR1) = CR1_FIFOEN | CR1_UESM;
  }
  else {
     descriptor->Base = USART1_BASE;
     #ifdef BOARD_NUCLEO_U575
        RCC_AHB2ENR1 |= RCC_AHB2ENR1_GPIOAEN;
     #else
        RCC_AHB2ENR1 |= RCC_AHB2ENR1_GPIOBEN;
     #endif
     RCC_APB2ENR |= RCC_APB2ENR_USART1EN;
     (void)RCC_APB2ENR;                // the clocks run before the blocks are written
     #ifdef BOARD_NUCLEO_U575
        /* The NUCLEO-U575ZI-Q's virtual COM port of its ST-LINK: PA9 and PA10, alternate
        ** function 7 (UM2861, 6.9; ST's BSP, stm32u5xx_nucleo.h). */
        GPIOA_AFRH = (GPIOA_AFRH & ~(0xFu << 4 * (NUCLEO_TX_PIN - 8) |
                                     0xFu << 4 * (NUCLEO_RX_PIN - 8))) |
                     AF_USART1 << 4 * (NUCLEO_TX_PIN - 8) |
                     AF_USART1 << 4 * (NUCLEO_RX_PIN - 8);
        GPIOA_MODER = (GPIOA_MODER & ~(3u << 2 * NUCLEO_TX_PIN | 3u << 2 * NUCLEO_RX_PIN)) |
                      MODE_AF << 2 * NUCLEO_TX_PIN | MODE_AF << 2 * NUCLEO_RX_PIN;
     #else
        /* PB6 and PB7 to alternate function 7. */
        GPIOB_AFRL = (GPIOB_AFRL & ~(0xFu << 4 * TX_PIN | 0xFu << 4 * RX_PIN)) |
                     AF_USART1 << 4 * TX_PIN | AF_USART1 << 4 * RX_PIN;
        GPIOB_MODER = (GPIOB_MODER & ~(3u << 2 * TX_PIN | 3u << 2 * RX_PIN)) |
                      MODE_AF << 2 * TX_PIN | MODE_AF << 2 * RX_PIN;
     #endif
     /* 8 bits, no parity, one stop bit, oversampling by 16: the divisor is the clock over
     ** the baud rate, rounded. */
     REG(descriptor,USART_CR1) = 0;
     REG(descriptor,USART_BRR) = (OS_SYSTEM_CLOCK_HZ + BAUD_RATE / 2) / BAUD_RATE;
     /* FIFOEN while the USART is disabled. Without it, a byte had to be read within one
     ** frame, 87 us at 115,200 baud: SoakU5 on a NUCLEO-U575ZI-Q lost one after 17 h, on
     ** 2026-09-29, every count of the kernel at 0. The 8 bytes of the FIFO give 700 us. */
     REG(descriptor,USART_CR1) = CR1_FIFOEN;
  }
  /* Reception interrupt only; transmission is enabled by OSEnqueueUART when there is
  ** something to send. */
  REG(descriptor,USART_CR1) |= CR1_UE | CR1_RE | CR1_TE | CR1_RXNEIE;
  OSSetISRDescriptor(interruptIndex,descriptor);
  NVIC_ISER(interruptIndex) = NVIC_BIT(interruptIndex);
  return TRUE;
} /* end of OSInitUART */


/* OSGetUARTOverruns: The bytes lost so far, each for one not read before the next came. */
UINT32 OSGetUARTOverruns(UINT8 interruptIndex)
{
  return ((UART_INTERRUPT_DESCRIPTOR *)OSGetISRDescriptor(interruptIndex))->Overruns;
} /* end of OSGetUARTOverruns */


/* _OSUARTIdle: TRUE when no UART has a byte to send and USART1 does not receive, which
** Stop 2, where their clocks stop, would lose (Escapement_Stop2.c); LPUART1 receives
** through it. A UART that hands its bytes to a handler may receive one at any time.
** Called with interrupts masked. */
BOOL _OSUARTIdle(void)
{
  static const UINT8 index[] = { OS_IO_USART1, OS_IO_LPUART1 };
  UART_INTERRUPT_DESCRIPTOR *des;
  UINT32 i;
  for (i = 0; i < sizeof(index); i += 1)
     if ((des = (UART_INTERRUPT_DESCRIPTOR *)OSGetISRDescriptor(index[i])) != NULL &&
         ((des->UserReceiveInterruptHandler != NULL && index[i] == OS_IO_USART1) ||
          des->Prime || (REG(des,USART_CR1) & CR1_TXEIE) ||
          (REG(des,USART_ISR) & ISR_TC) == 0))
        return FALSE;
  return TRUE;
} /* end of _OSUARTIdle */


/* _OSUARTReceived: TRUE once LPUART1 has received a byte since the last call, for the
** idle task to stay out of Stop 2 a while after it (Escapement_Stop2.c). Called with
** interrupts masked. */
BOOL _OSUARTReceived(void)
{
  BOOL received = LinkReceived;
  LinkReceived = FALSE;
  return received;
} /* end of _OSUARTReceived */


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


/* OSEnqueueUART: Queues a buffer and sets the interrupt pending, which enables its
** transmit part and drains the queue: CR1, which the interrupt reads, modifies and writes,
** is only ever written by it. The interrupt of this USART was masked here around a write
** of CR1 instead, and a task preempted inside it left the receiver unread for as long as
** the tasks that preempted it ran: the FIFO of 8 bytes overflowed once a day or so, from
** 2026-09-29 to 2026-10-09, each loss of the endurance tests' links on USART1 and LPUART1,
** the pulse never late. */
void OSEnqueueUART(void *buffer, UINT8 dataSize, UINT8 interruptIndex)
{
  UART_INTERRUPT_DESCRIPTOR *descriptor =
                          (UART_INTERRUPT_DESCRIPTOR *)OSGetISRDescriptor(interruptIndex);
  OSEnqueueFIFO(descriptor->FifoArray,buffer,dataSize);
  descriptor->Prime = TRUE;
  NVIC_ISPR(interruptIndex) = NVIC_BIT(interruptIndex);
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
  while (status & ISR_RXNE) {
     UINT8 data = (UINT8)REG(des,USART_RDR);
     if (des->Base == LPUART1_BASE)
        LinkReceived = TRUE;
     if (des->UserReceiveInterruptHandler != NULL)
        des->UserReceiveInterruptHandler(data);
     if ((REG(des,USART_CR1) & CR1_FIFOEN) == 0)
        break;                          // one byte an interrupt, as before the FIFO
     status = REG(des,USART_ISR);       // every byte of the FIFO
  }
  if (status & ISR_ORE) {
     REG(des,USART_ICR) = ICR_ORECF;
     des->Overruns += 1;
  }
  if (des->Prime) {
     des->Prime = FALSE;
     REG(des,USART_CR1) |= CR1_TXEIE;
  }
  if ((status & ISR_TXE) && (REG(des,USART_CR1) & CR1_TXEIE))
     Transmit(des);
} /* end of InterruptHandler */
