/* Copyright (c) 20012 MIS Institute of the HEIG affiliated to the University of
** Applied Sciences of Western Switzerland. All rights reserved.
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
/* File UART.c: Contains the implementation of an API allowing facilitated message
** transmissions and easy access to the receiver part of a UART.
** Version identifier: February 2012
** Compiler and linker:
** Authors: MIS-TIC
*/
#include "ZottaOS_CortexM3.h"
#include "ZottaOS.h"
#include "ZottaOS_UART.h"


/* STM-32 hardware registers */
#define USART1_BASE 0x40013800
#define USART2_BASE 0x40004400
#define USART3_BASE 0x40004800
#define UART4_BASE  0x40004C00
#define UART5_BASE  0x40005000


typedef struct UART_INTERRUPT_DESCRIPTOR { // Interrupt handler opaque descriptor
  void (*InterruptHandler)(struct UART_INTERRUPT_DESCRIPTOR *);
  UINT16 *Status;
  UINT16 *ControlReg2;
  UINT16 *HardwareBuffer;
  UINT8 CurrentBufferIndex;            // Next byte to transmit from the current buffer
  void *FifoArray;                     // Descriptor to FIFO queue
  UINT16 NbTransmit;                   // Number of remaining bytes to transmit
  UINT8 *CurrentBuffer;                // Current buffer being emptied to the output port
  void (*UserReceiveInterruptHandler)(UINT8 data); // User receiver ISR
} UART_INTERRUPT_DESCRIPTOR;


/* Internal interrupt handler prototypes */
static void InterruptHandler(UART_INTERRUPT_DESCRIPTOR *descriptor);


/* OSInitUART: Creates and binds a descriptor that will be used with the functions rela-
** ted to a UART. */
BOOL OSInitUART(UINT8 maxNodes, UINT8 maxNodeSize, void (*ReceiveHandler)(UINT8),
                UINT8 interruptIndex)
{
  UART_INTERRUPT_DESCRIPTOR *descriptor;
  if ((descriptor =
        (UART_INTERRUPT_DESCRIPTOR *)OSMalloc(sizeof(UART_INTERRUPT_DESCRIPTOR))) == NULL)
     return FALSE;
  descriptor->InterruptHandler = InterruptHandler;
  descriptor->UserReceiveInterruptHandler = ReceiveHandler;
  descriptor->FifoArray = OSInitFIFOQueue(maxNodes,maxNodeSize);
  descriptor->NbTransmit = 0;
  descriptor->CurrentBuffer = NULL;
  switch (interruptIndex) {  /* Specific UART device configuration */
        case OS_IO_USART1:
           descriptor->Status = (UINT16 *)USART1_BASE;
           descriptor->HardwareBuffer = (UINT16 *)(USART1_BASE + 0x04);
           descriptor->ControlReg2 = (UINT16 *)(USART1_BASE + 0x0C);
           break;
        case OS_IO_USART2:
            descriptor->Status = (UINT16 *)USART2_BASE;
            descriptor->HardwareBuffer = (UINT16 *)(USART2_BASE + 0x04);
            descriptor->ControlReg2 = (UINT16 *)(USART2_BASE + 0x0C);
           break;
        case OS_IO_USART3:
            descriptor->Status = (UINT16 *)USART3_BASE;
            descriptor->HardwareBuffer = (UINT16 *)(USART3_BASE + 0x04);
            descriptor->ControlReg2 = (UINT16 *)(USART3_BASE + 0x0C);
           break;
        case OS_IO_UART4:
            descriptor->Status = (UINT16 *)UART4_BASE;
            descriptor->HardwareBuffer = (UINT16 *)(UART4_BASE + 0x04);
            descriptor->ControlReg2 = (UINT16 *)(UART4_BASE + 0x0C);
           break;
        case OS_IO_UART5:
            descriptor->Status = (UINT16 *)UART5_BASE;
            descriptor->HardwareBuffer = (UINT16 *)(UART5_BASE + 0x04);
            descriptor->ControlReg2 = (UINT16 *)(UART5_BASE + 0x0C);
           break;
     default: break;
  }
  OSSetISRDescriptor(interruptIndex,descriptor);
  return TRUE;
} /* end of OSInitUART */


/* OSGetFreeNodeUART: Returns a free buffer that an application can fill with useful data
** and then transfers that buffer to OSEnqueueUART. */
void *OSGetFreeNodeUART(UINT8 interruptIndex)
{
  UART_INTERRUPT_DESCRIPTOR *descriptor =
                          (UART_INTERRUPT_DESCRIPTOR *)OSGetISRDescriptor(interruptIndex);
  return OSGetFreeNodeFIFO(descriptor->FifoArray);
} /* end of OSGetFreeNodeUART */


/* OSReleaseNodeUART: Releases a buffer that was obtained from OSGetFreeNodeUART and re-
** turns it to the pool of available buffers associated with an UART. */
void OSReleaseNodeUART(void *buffer, UINT8 interruptIndex)
{
  UART_INTERRUPT_DESCRIPTOR *descriptor =
                          (UART_INTERRUPT_DESCRIPTOR *)OSGetISRDescriptor(interruptIndex);
  OSReleaseNodeFIFO(descriptor->FifoArray,buffer);
} /* end of OSReleaseNodeUART */


/* OSEnqueueUART: Called by an application task to output a string of bytes contained in
** a buffer that was returned by OSGetFreeNodeUART. */
void OSEnqueueUART(void *buffer, UINT8 dataSize, UINT8 interruptIndex)
{
  UART_INTERRUPT_DESCRIPTOR *descriptor =
                          (UART_INTERRUPT_DESCRIPTOR *)OSGetISRDescriptor(interruptIndex);
  OSEnqueueFIFO(descriptor->FifoArray,buffer,dataSize);
  /* Enable transmit buffer empty interrupts from the UART once it finishes the previous
  ** send operation. Note that this can be immediate if the last transmission has already
  ** finished or later if the UART is busy sending a byte. */
  *descriptor->ControlReg2 |= 0x80;
} /* end of OSEnqueueUART */


/* InterruptHandler: Global UART ISR called whenever an interrupt is raised for a particular
** UART. If the interrupt source refers to an input interrupt, the function provided to
** OSInitUART is invoked and it is up to the application to process the interrupt.
** If the interrupt refers to an output interrupt, a new byte is transferred from the
** current buffer to the output port and if that buffer becomes empty, a new buffer is
** retrieved from the FIFO queue associated with the UART. If there remains something to
** transmit, the output interrupt source is re-enabled. Otherwise it is up to OSEnqueue-
** UART to raise an interrupt so that this ISR can be called and retrieve the newly in-
** serted buffer.
** Parameters:
**   (1) (UART_INTERRUPT_DESCRIPTOR *) UART descriptor holding the necessary information
**       to process the interrupt. This descriptor is provided to the ISR essentially to
**       access the FIFO queue holding the bytes to output from the UART. */
void InterruptHandler(UART_INTERRUPT_DESCRIPTOR *descriptor)
{
  UINT8 tmp;
  /* Data reception interrupt */
  if ((*descriptor->Status & 0x20)) {
    if (descriptor->UserReceiveInterruptHandler != NULL)
       descriptor->UserReceiveInterruptHandler(*descriptor->HardwareBuffer);
  }
  /* Transmit buffer empty: insert a new byte to send. */
  if ((*descriptor->Status & 0x80) && (*descriptor->ControlReg2  & 0x80)) {
    if (descriptor->CurrentBuffer == NULL) { // If last message is completely sent
       descriptor->CurrentBufferIndex = 0;
       descriptor->CurrentBuffer = (UINT8 *)OSDequeueFIFO(descriptor->FifoArray,&descriptor->NbTransmit);
    }
    descriptor->NbTransmit--;                // One less byte to send.
    tmp = descriptor->CurrentBuffer[descriptor->CurrentBufferIndex++];
    if (descriptor->NbTransmit == 0) {       // Is this message is completely sent?
       OSReleaseNodeFIFO(descriptor->FifoArray,descriptor->CurrentBuffer);
       descriptor->CurrentBufferIndex = 0;
       descriptor->CurrentBuffer = (UINT8 *)OSDequeueFIFO(descriptor->FifoArray,&descriptor->NbTransmit);
    }
    *descriptor->HardwareBuffer = tmp;
    /* If there are more data to send, we need to disable transmit buffer empty interrupts. */
    if (descriptor->CurrentBuffer == NULL)
       *descriptor->ControlReg2 &= ~0x80;
  }
} /* end of InterruptHandler */
