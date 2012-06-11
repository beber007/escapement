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
/* File ZottaOS_UART.c: Contains the implementation of an API allowing facilitated message
**                      transmissions and easy access to the receiver part of a UART.
** Platform version: All MSP430 and CC430 microcontrollers.
** Version identifier: March 2012
** Authors: MIS-TIC */

#include "ZottaOS.h"
#include "ZottaOS_UART.h"

typedef struct UART_TRANSMIT_INTERRUPT_DESCRIPTOR { // Interrupt handler opaque descriptor
  void (*InterruptHandler)(struct UART_TRANSMIT_INTERRUPT_DESCRIPTOR *);
  UINT8 *HardwareTransmitBuffer;       // Output UART data transmit buffer
  UINT8 *HardwareControlRegister;      // Register controlling the transmit interrupt
  UINT8 EnableTransmitInterruptBit;    // Enable interrupt bit 
  UINT8 CurrentBufferIndex;            // Next byte to transmit from the current buffer
  void *FifoArray;                     // Descriptor to FIFO queue
  UINT16 NbTransmit;                   // Number of remaining bytes to transmit
  UINT8 *CurrentBuffer;                // Current buffer being emptied to the output port
} UART_TRANSMIT_INTERRUPT_DESCRIPTOR;

typedef struct UART_RECEIVE_INTERRUPT_DESCRIPTOR { // Interrupt handler opaque descriptor
  void (*InterruptHandler)(struct UART_RECEIVE_INTERRUPT_DESCRIPTOR *);
  UINT8 *HardwareReceiveBuffer;        // Input UART data buffer
  UINT8 *HardwareControlRegister;      // Register controlling the receive interrupt
  UINT8 EnableReceiveInterruptBit;     // Enable interrupt bit
  BOOL (*UserReceiveInterruptHandler)(UINT8 data); // User receiver ISR
} UART_RECEIVE_INTERRUPT_DESCRIPTOR;


/* Internal interrupt handler prototypes */
static void ReceiveInterruptHandler(UART_RECEIVE_INTERRUPT_DESCRIPTOR *descriptor);
static void TransmitInterruptHandler(UART_TRANSMIT_INTERRUPT_DESCRIPTOR *descriptor);


/* OSInitReceiveUART: Creates and binds a descriptor that will be used with the functions
** related to a receive UART. */
BOOL OSInitReceiveUART(BOOL (*userReceiveInterruptHandler)(UINT8), UINT8 interruptIndex)
{
  UART_RECEIVE_INTERRUPT_DESCRIPTOR *descriptor;
  if ((descriptor = (UART_RECEIVE_INTERRUPT_DESCRIPTOR *)
                            OSMalloc(sizeof(UART_RECEIVE_INTERRUPT_DESCRIPTOR))) == NULL)
     return FALSE;
  descriptor->InterruptHandler = ReceiveInterruptHandler;
  descriptor->UserReceiveInterruptHandler = userReceiveInterruptHandler;
  OSSetISRDescriptor(interruptIndex,descriptor);
  switch (interruptIndex) {  // Specific USART or USCI device configuration
     #ifdef OS_IO_USART0RX
        case OS_IO_USART0RX:
           descriptor->HardwareReceiveBuffer = (UINT8*)&U0RXBUF;
           #if defined (__MSP430F122__) || defined (__MSP430F1222__)
              descriptor->HardwareControlRegister = (UINT8*)&IE2;
           #else
              descriptor->HardwareControlRegister = (UINT8*)&IE1;
           #endif
           descriptor->EnableReceiveInterruptBit = URXIE0;
           return TRUE;
     #endif
     #ifdef OS_IO_USART1RX
        case OS_IO_USART1RX:
           descriptor->HardwareReceiveBuffer = (UINT8*)&U1RXBUF;
           descriptor->HardwareControlRegister = (UINT8*)&IE2;
           descriptor->EnableReceiveInterruptBit = URXIE1;
           return TRUE;
     #endif
     #ifdef OS_IO_USCIA0RX
        case OS_IO_USCIA0RX:
           descriptor->HardwareReceiveBuffer = (UINT8*)&UCA0RXBUF;
           descriptor->HardwareControlRegister = (UINT8*)&IE2;
           descriptor->EnableReceiveInterruptBit = UCA0RXIE;
           return TRUE;
     #endif
     #ifdef OS_IO_USCIA1RX
        case OS_IO_USCIA1RX:
           descriptor->HardwareReceiveBuffer = (UINT8*)&UCA1RXBUF;
           descriptor->HardwareControlRegister = (UINT8*)&UC1IE;
           descriptor->EnableReceiveInterruptBit = UCA1RXIE;
           return TRUE;
     #endif
     #ifdef OS_IO_USCIB0RX
        case OS_IO_USCIB0RX:
           descriptor->HardwareReceiveBuffer = (UINT8*)&UCB0RXBUF;
           descriptor->HardwareControlRegister = (UINT8*)&IE2;
           descriptor->EnableReceiveInterruptBit = UCB0RXIE;
           return TRUE;
     #endif
     #ifdef OS_IO_USCIB1RX
        case OS_IO_USCIB1RX:
           descriptor->HardwareReceiveBuffer = (UINT8*)&UCB1RXBUF;
           descriptor->HardwareControlRegister = (UINT8*)&UC1IE;
           descriptor->EnableReceiveInterruptBit = UCB1RXIE;
           return TRUE;
     #endif
     #ifdef OS_IO_USCI_A0_RX
        case OS_IO_USCI_A0_RX:
           descriptor->HardwareReceiveBuffer = (UINT8*)&UCA0RXBUF;
           descriptor->HardwareControlRegister = (UINT8*)&UCA0IE;
           descriptor->EnableReceiveInterruptBit = UCRXIE;
           return TRUE;
     #endif
     #ifdef OS_IO_USCI_A1_RX
        case OS_IO_USCI_A1_RX:
           descriptor->HardwareReceiveBuffer = (UINT8*)&UCA1RXBUF;
           descriptor->HardwareControlRegister = (UINT8*)&UCA1IE;
           descriptor->EnableReceiveInterruptBit = UCRXIE;
           return TRUE;
     #endif
     #ifdef OS_IO_USCI_A2_RX
        case OS_IO_USCI_A2_RX:
           descriptor->HardwareReceiveBuffer = (UINT8*)&UCA2RXBUF;
           descriptor->HardwareControlRegister = (UINT8*)&UCA2IE;
           descriptor->EnableReceiveInterruptBit = UCRXIE;
           return TRUE;
     #endif
     #ifdef OS_IO_USCI_A3_RX
        case OS_IO_USCI_A3_RX:
           descriptor->HardwareReceiveBuffer = (UINT8*)&UCA3RXBUF;
           descriptor->HardwareControlRegister = (UINT8*)&UCA3IE;
           descriptor->EnableReceiveInterruptBit = UCRXIE;
           return TRUE;
     #endif
     #ifdef OS_IO_USCI_B0_RX
        case OS_IO_USCI_B0_RX:
           descriptor->HardwareReceiveBuffer = (UINT8*)&UCB0RXBUF;
           descriptor->HardwareControlRegister = (UINT8*)&UCB0IE;
           descriptor->EnableReceiveInterruptBit = UCRXIE;
           return TRUE;
     #endif
     #ifdef OS_IO_USCI_B1_RX
        case OS_IO_USCI_B1_RX:
           descriptor->HardwareReceiveBuffer = (UINT8*)&UCB1RXBUF;
           descriptor->HardwareControlRegister = (UINT8*)&UCB1IE;
           descriptor->EnableReceiveInterruptBit = UCRXIE;
           return TRUE;
     #endif
     #ifdef OS_IO_USCI_B2_RX
        case OS_IO_USCI_B2_RX:
           descriptor->HardwareReceiveBuffer = (UINT8*)&UCB2RXBUF;
           descriptor->HardwareControlRegister = (UINT8*)&UCB2IE;
           descriptor->EnableReceiveInterruptBit = UCRXIE;
           return TRUE;
     #endif
     #ifdef OS_IO_USCI_B3_RX
        case OS_IO_USCI_B3_RX:
           descriptor->HardwareReceiveBuffer = (UINT8*)&UCB3RXBUF;
           descriptor->HardwareControlRegister = (UINT8*)&UCB3IE;
           descriptor->EnableReceiveInterruptBit = UCRXIE;
           return TRUE;
     #endif
     default:
        descriptor->HardwareReceiveBuffer = NULL;
        descriptor->HardwareControlRegister = NULL;
        descriptor->EnableReceiveInterruptBit = 0;
        return FALSE; 
  }
} /* end of OSInitReceiveUART */


/* OSInitTransmitUART: Creates and binds a descriptor that will later be used with the
** functions related to the transmitter part of a UART. */
BOOL OSInitTransmitUART(UINT8 maxNodes, UINT8 maxNodeSize, UINT8 interruptIndex)
{
  UART_TRANSMIT_INTERRUPT_DESCRIPTOR *descriptor;
  if ((descriptor = (UART_TRANSMIT_INTERRUPT_DESCRIPTOR *)
                           OSMalloc(sizeof(UART_TRANSMIT_INTERRUPT_DESCRIPTOR))) == NULL)
     return FALSE;
  descriptor->InterruptHandler = TransmitInterruptHandler;
  descriptor->FifoArray = OSInitFIFOQueue(maxNodes,maxNodeSize);
  descriptor->NbTransmit = 0;
  descriptor->CurrentBuffer = NULL;
  OSSetISRDescriptor(interruptIndex,descriptor);
  switch (interruptIndex) {  // Specific USART or USCI device configuration
     #ifdef OS_IO_USART0TX
        case OS_IO_USART0TX:
           descriptor->HardwareTransmitBuffer = (UINT8*)&U0TXBUF;
           #if defined (__MSP430F122__) || defined (__MSP430F1222__)
              descriptor->HardwareControlRegister = (UINT8*)&IE2;
           #else
              descriptor->HardwareControlRegister = (UINT8*)&IE1;
           #endif
           descriptor->EnableTransmitInterruptBit = UTXIE0;
           return TRUE;
     #endif
     #ifdef OS_IO_USART1TX
        case OS_IO_USART1TX:
           descriptor->HardwareTransmitBuffer = (UINT8*)&U1TXBUF;
           descriptor->HardwareControlRegister = (UINT8*)&IE2;
           descriptor->EnableTransmitInterruptBit = UTXIE1;
           return TRUE;
     #endif
     #ifdef OS_IO_USCIA0TX
        case OS_IO_USCIA0TX:
           descriptor->HardwareTransmitBuffer = (UINT8*)&UCA0TXBUF;
           descriptor->HardwareControlRegister = (UINT8*)&IE2;
           descriptor->EnableTransmitInterruptBit = UCA0TXIE;
           return TRUE;
     #endif
     #ifdef OS_IO_USCIA1TX
        case OS_IO_USCIA1TX:
           descriptor->HardwareTransmitBuffer = (UINT8*)&UCA1TXBUF;
           descriptor->HardwareControlRegister = (UINT8*)&UC1IE;
           descriptor->EnableTransmitInterruptBit = UCA1TXIE;
           return TRUE;
     #endif
     #ifdef OS_IO_USCI_A0_TX
        case OS_IO_USCI_A0_TX:
           descriptor->HardwareTransmitBuffer = (UINT8*)&UCA0TXBUF;
           descriptor->HardwareControlRegister = (UINT8*)&UCA0IE;
           descriptor->EnableTransmitInterruptBit = UCTXIE;
           return TRUE;
     #endif
     #ifdef OS_IO_USCI_A1_TX
        case OS_IO_USCI_A1_TX:
           descriptor->HardwareTransmitBuffer = (UINT8*)&UCA1TXBUF;
           descriptor->HardwareControlRegister = (UINT8*)&UCA1IE;
           descriptor->EnableTransmitInterruptBit = UCTXIE;
           return TRUE;
     #endif
     #ifdef OS_IO_USCI_A2_TX
        case OS_IO_USCI_A2_TX:
           descriptor->HardwareTransmitBuffer = (UINT8*)&UCA2TXBUF;
           descriptor->HardwareControlRegister = (UINT8*)&UCA2IE;
           descriptor->EnableTransmitInterruptBit = UCTXIE;
           return TRUE;
     #endif
     #ifdef OS_IO_USCI_A3_TX
        case OS_IO_USCI_A3_TX:
           descriptor->HardwareTransmitBuffer = (UINT8*)&UCA3TXBUF;
           descriptor->HardwareControlRegister = (UINT8*)&UCA3IE;
           descriptor->EnableTransmitInterruptBit = UCTXIE;
           return TRUE;
     #endif
     default: 
        descriptor->HardwareTransmitBuffer = NULL;
        descriptor->HardwareControlRegister = NULL;
        descriptor->EnableTransmitInterruptBit = 0;
        return FALSE;
  }
} /* end of OSInitTransmitUART */


/* OSGetFreeNodeUART: Returns a free buffer that an application can fill with useful data
** and then transfers that buffer to OSEnqueueUART. */
void *OSGetFreeNodeUART(UINT8 interruptIndex)
{
  UART_TRANSMIT_INTERRUPT_DESCRIPTOR *descriptor =
                (UART_TRANSMIT_INTERRUPT_DESCRIPTOR *)OSGetISRDescriptor(interruptIndex);
  return OSGetFreeNodeFIFO(descriptor->FifoArray);
} /* end of OSGetFreeNodeUART */


/* OSReleaseNodeUART: Releases a buffer that was obtained from OSGetFreeNodeUART and re-
** turns it to the pool of available buffers associated with an UART. */
void OSReleaseNodeUART(void *buffer, UINT8 interruptIndex)
{
  UART_TRANSMIT_INTERRUPT_DESCRIPTOR *descriptor =
                (UART_TRANSMIT_INTERRUPT_DESCRIPTOR *)OSGetISRDescriptor(interruptIndex);
  OSReleaseNodeFIFO(descriptor->FifoArray,buffer);
} /* end of OSReleaseNodeUART */


/* OSEnqueueUART: Called by an application task to output a string of bytes contained in
** a buffer that was returned by OSGetFreeNodeUART. */
void OSEnqueueUART(void *buffer, UINT8 dataSize, UINT8 interruptIndex)
{
  UART_TRANSMIT_INTERRUPT_DESCRIPTOR *descriptor =
                (UART_TRANSMIT_INTERRUPT_DESCRIPTOR *)OSGetISRDescriptor(interruptIndex);
  OSEnqueueFIFO(descriptor->FifoArray,buffer,dataSize);
  /* Enable transmit buffer empty interrupts from the UART once it finishes the previous
  ** send operation. Note that this can be immediate if the last transmission has already
  ** finished or later if the UART is busy sending a byte. */
  *(descriptor->HardwareControlRegister) |= descriptor->EnableTransmitInterruptBit;
} /* end of OSEnqueueUART */


/* OSEnableReceiveInterruptUART: Re-enables receiver interrupts of a particular UART. */
void OSEnableReceiveInterruptUART(UINT8 interruptIndex)
{
  UART_RECEIVE_INTERRUPT_DESCRIPTOR *descriptor =
                 (UART_RECEIVE_INTERRUPT_DESCRIPTOR *)OSGetISRDescriptor(interruptIndex);
  *(descriptor->HardwareControlRegister) |= descriptor->EnableReceiveInterruptBit;
} /* end of OSEnableReceiveInterruptUART */


/* ReceiveInterruptHandler: UART ISR called whenever a receive interrupt is raised for a
** particular UART. */ 
void ReceiveInterruptHandler(UART_RECEIVE_INTERRUPT_DESCRIPTOR *descriptor)
{
  // Call user handler.
  if (descriptor->UserReceiveInterruptHandler(*(descriptor->HardwareReceiveBuffer)))
     // Re-enable interrupt only if user handler return true.
     *(descriptor->HardwareControlRegister) |= descriptor->EnableReceiveInterruptBit;
} /* end of ReceiveInterruptHandler */


/* TransmitInterruptHandler: UART ISR called whenever a transmit buffer empty interrupt
** is raised for a particular UART. */
void TransmitInterruptHandler(UART_TRANSMIT_INTERRUPT_DESCRIPTOR *descriptor)
{
  UINT8 byteToSend;
  if (descriptor->CurrentBuffer == NULL) { // If last message is completely sent
     descriptor->CurrentBufferIndex = 0;
     descriptor->CurrentBuffer =
                   (UINT8 *)OSDequeueFIFO(descriptor->FifoArray,&descriptor->NbTransmit);
  }
  descriptor->NbTransmit--;                // One less byte to send.
  byteToSend = descriptor->CurrentBuffer[descriptor->CurrentBufferIndex++];
  if (descriptor->NbTransmit == 0) {       // Is this message completely sent?
     OSReleaseNodeFIFO(descriptor->FifoArray,descriptor->CurrentBuffer);
     descriptor->CurrentBufferIndex = 0;
     descriptor->CurrentBuffer =
                   (UINT8 *)OSDequeueFIFO(descriptor->FifoArray,&descriptor->NbTransmit);
  }
  *(descriptor->HardwareTransmitBuffer) = byteToSend;
  /* If there are more data to send, we need to re-enable transmit buffer empty inter-
  ** rupts so that the next byte can be sent. */
  if (descriptor->CurrentBuffer != NULL)
     *(descriptor->HardwareControlRegister) |= descriptor->EnableTransmitInterruptBit;
} /* end of TransmitInterruptHandler */
