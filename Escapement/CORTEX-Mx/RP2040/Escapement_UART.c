/* Copyright (c) 2026 Bertrand Hurst. All rights reserved.
** Escapement - Lightweight Power-Aware Real-Time OS.
** Derived from prior work; see LICENSE and NOTICE at the root of this repository.
*/
/* File Escapement_UART.c: UART driver, transposed from the STM32 one onto the PL011 of
** the RP2040. Reception hands each byte to a handler supplied by the application, from
** interrupt context. Transmission goes through a queue of buffers served by the interrupt,
** so that a task never waits on the port.
**
** Platform version: RP2040 (Raspberry Pi Pico).
*/

#include "Escapement.h"
#include "Escapement_UART.h"

#define UART0_BASE           0x40034000
#define UART1_BASE           0x40038000

/* PL011 register offsets */
#define UART_DR              0x00   /* data */
#define UART_FR              0x18   /* flags */
#define UART_IBRD            0x24   /* integer part of the baud divisor */
#define UART_FBRD            0x28   /* fractional part */
#define UART_LCR_H           0x2C   /* line control */
#define UART_CR              0x30   /* control */
#define UART_IMSC            0x38   /* interrupt mask */
#define UART_MIS             0x40   /* masked interrupt status */
#define UART_ICR             0x44   /* interrupt clear */

#define FR_TXFF              (1u << 5)   /* transmit FIFO full */
#define FR_RXFE              (1u << 4)   /* receive FIFO empty */
#define LCR_H_WLEN_8         (3u << 5)
#define LCR_H_FEN            (1u << 4)
#define CR_UARTEN            (1u << 0)
#define CR_TXE               (1u << 8)
#define CR_RXE               (1u << 9)
#define INT_RX               (1u << 4)   /* receive */
#define INT_TX               (1u << 5)   /* transmit */
#define INT_RT               (1u << 6)   /* receive timeout */

#define RESETS_RESET         *((volatile UINT32 *)0x4000C000)
#define RESETS_RESET_DONE    *((volatile UINT32 *)0x4000C008)
#define RESETS_UART0_BIT     (1u << 22)
#define RESETS_UART1_BIT     (1u << 23)
#define RESETS_IO_BANK0_BIT  (1u << 5)

#define IO_BANK0_CTRL(p)     *((volatile UINT32 *)(0x40014000 + 0x04 + 8 * (p)))
#define FUNCSEL_UART         2

#define NVIC_ISER            *((volatile UINT32 *)0xE000E100)
#define NVIC_ICER            *((volatile UINT32 *)0xE000E180)

/* Clock feeding the UART, fixed by OSInitializeSystemClocks. */
#define CLK_PERI_HZ          12000000u
#define BAUD_RATE            115200u


typedef struct UART_INTERRUPT_DESCRIPTOR { // Interrupt handler opaque descriptor
  void (*InterruptHandler)(struct UART_INTERRUPT_DESCRIPTOR *);
  UINT32 Base;                     // First register of the UART
  UINT8 CurrentBufferIndex;        // Next byte to transmit from the current buffer
  void *FifoArray;                 // Descriptor of the transmit queue
  UINT16 NbTransmit;               // Number of bytes left to transmit
  UINT8 *CurrentBuffer;            // Buffer being emptied onto the port
  void (*UserReceiveInterruptHandler)(UINT8 data); // Application receive handler
} UART_INTERRUPT_DESCRIPTOR;

#define REG(des,off) *((volatile UINT32 *)((des)->Base + (off)))

static void InterruptHandler(UART_INTERRUPT_DESCRIPTOR *descriptor);
static void Transmit(UART_INTERRUPT_DESCRIPTOR *descriptor);


/* OSInitUART: Creates and binds the descriptor used by the other functions, and brings the
** hardware up. */
BOOL OSInitUART(UINT8 maxNodes, UINT8 maxNodeSize, void (*ReceiveHandler)(UINT8),
                UINT8 interruptIndex)
{
  UART_INTERRUPT_DESCRIPTOR *descriptor;
  UINT32 divisor, resetBit;
  UINT8 txPin, rxPin;
  if ((descriptor =
        (UART_INTERRUPT_DESCRIPTOR *)OSMalloc(sizeof(UART_INTERRUPT_DESCRIPTOR))) == NULL)
     return FALSE;
  descriptor->InterruptHandler = InterruptHandler;
  descriptor->UserReceiveInterruptHandler = ReceiveHandler;
  descriptor->FifoArray = OSInitFIFOQueue(maxNodes,maxNodeSize);
  descriptor->NbTransmit = 0;
  descriptor->CurrentBuffer = NULL;
  descriptor->CurrentBufferIndex = 0;
  if (interruptIndex == OS_IO_UART0) {
     descriptor->Base = UART0_BASE;
     resetBit = RESETS_UART0_BIT;
     txPin = 0; rxPin = 1;        /* GP0 and GP1, the default pins of the Pico */
  }
  else {
     descriptor->Base = UART1_BASE;
     resetBit = RESETS_UART1_BIT;
     txPin = 4; rxPin = 5;
  }
  /* Release the UART and the pin block from reset. */
  RESETS_RESET &= ~(resetBit | RESETS_IO_BANK0_BIT);
  while ((RESETS_RESET_DONE & resetBit) == 0);
  /* Route the two pins to the UART. */
  IO_BANK0_CTRL(txPin) = FUNCSEL_UART;
  IO_BANK0_CTRL(rxPin) = FUNCSEL_UART;
  /* Baud rate: the PL011 divides the peripheral clock by 16 times a divisor held as an
  ** integer part and a sixth-fourth fraction. */
  divisor = (8 * CLK_PERI_HZ / BAUD_RATE + 1) / 2;   /* divisor in 64ths, rounded */
  REG(descriptor,UART_IBRD) = divisor >> 6;
  REG(descriptor,UART_FBRD) = divisor & 0x3F;
  /* 8 bits, no parity, one stop bit, FIFOs enabled. */
  REG(descriptor,UART_LCR_H) = LCR_H_WLEN_8 | LCR_H_FEN;
  REG(descriptor,UART_CR) = CR_UARTEN | CR_TXE | CR_RXE;
  /* Reception interrupts only; transmission is enabled by OSEnqueueUART when there is
  ** something to send. */
  REG(descriptor,UART_ICR) = 0x7FF;
  REG(descriptor,UART_IMSC) = INT_RX | INT_RT;
  OSSetISRDescriptor(interruptIndex,descriptor);
  NVIC_ISER = 1u << interruptIndex;
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


/* OSEnqueueUART: Queues a buffer and enables the transmit interrupt, which will drain it. */
void OSEnqueueUART(void *buffer, UINT8 dataSize, UINT8 interruptIndex)
{
  UART_INTERRUPT_DESCRIPTOR *descriptor =
                          (UART_INTERRUPT_DESCRIPTOR *)OSGetISRDescriptor(interruptIndex);
  OSEnqueueFIFO(descriptor->FifoArray,buffer,dataSize);
  /* Contrary to the USART of the STM32, where the transmit interrupt reflects a state and
  ** fires as soon as it is enabled, the one of the PL011 fires on a FIFO threshold being
  ** crossed. Enabling it on an already empty FIFO produces nothing: transmission has to be
  ** primed by writing the first bytes.
  ** Priming is the one moment where the fields holding the buffer being emptied are
  ** touched outside the interrupt. The interrupt of this UART alone is therefore masked
  ** around it, rather than every interrupt, so that the timer of the kernel keeps its
  ** latency. A transmit interrupt raised meanwhile stays pending and is taken as soon as
  ** it is unmasked. */
  NVIC_ICER = 1u << interruptIndex;
  asm volatile ("dsb" ::: "memory");   // the mask must hold before the next instruction
  asm volatile ("isb" ::: "memory");
  Transmit(descriptor);
  NVIC_ISER = 1u << interruptIndex;
} /* end of OSEnqueueUART */


/* Transmit: Pushes as many bytes as the transmit FIFO accepts, taking the next buffer from
** the queue whenever the current one runs out. Leaves the transmit interrupt enabled only
** while something remains to send. Called by OSEnqueueUART to prime, and by the interrupt
** to carry on. */
static void Transmit(UART_INTERRUPT_DESCRIPTOR *des)
{
  if (des->CurrentBuffer == NULL) {
     des->CurrentBufferIndex = 0;
     des->CurrentBuffer = (UINT8 *)OSDequeueFIFO(des->FifoArray,&des->NbTransmit);
  }
  while (des->CurrentBuffer != NULL && (REG(des,UART_FR) & FR_TXFF) == 0) {
     REG(des,UART_DR) = des->CurrentBuffer[des->CurrentBufferIndex++];
     des->NbTransmit -= 1;
     if (des->NbTransmit == 0) {
        OSReleaseNodeFIFO(des->FifoArray,des->CurrentBuffer);
        des->CurrentBufferIndex = 0;
        des->CurrentBuffer = (UINT8 *)OSDequeueFIFO(des->FifoArray,&des->NbTransmit);
     }
  }
  if (des->CurrentBuffer == NULL)
     REG(des,UART_IMSC) &= ~INT_TX;
  else
     REG(des,UART_IMSC) |= INT_TX;
} /* end of Transmit */


/* InterruptHandler: Single ISR of a UART. On reception it hands each byte to the
** application. On transmission it pushes bytes from the current buffer, takes the next one
** from the queue when it runs out, and disables the transmit interrupt once everything has
** been sent — OSEnqueueUART will raise it again. */
static void InterruptHandler(UART_INTERRUPT_DESCRIPTOR *des)
{
  UINT32 status = REG(des,UART_MIS);
  if (status & (INT_RX | INT_RT)) {
     while ((REG(des,UART_FR) & FR_RXFE) == 0) {
        UINT8 data = (UINT8)REG(des,UART_DR);
        if (des->UserReceiveInterruptHandler != NULL)
           des->UserReceiveInterruptHandler(data);
     }
     REG(des,UART_ICR) = INT_RX | INT_RT;
  }
  if (status & INT_TX) {
     REG(des,UART_ICR) = INT_TX;
     Transmit(des);
  }
} /* end of InterruptHandler */
