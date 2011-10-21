/* Copyright (c) 2006-2010 MIS Institute of the HEIG affiliated to the University of
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
/* File USBCDC.c: .
** Version identifier: April 2010
** Authors: MIS-TIC
*/
/* (c)2009 by Texas Instruments Incorporated, All Rights Reserved. */

#include "..\ZottaOS.h"
#include "..\ZottaOS_msp430.h"
#include "descriptors.h"
#include "USB.h"
#include "USBInternal.h"
#include "USBCdc.h"

#ifdef _CDC_
   
   static void USBCdcInputHandler(INPUT_ENDPOINT_DESCRIPTOR *descriptor);
   static void USBCdcOutputHandler(CDC_OUTPUT_ENDPOINT_DESCRIPTOR *descriptor);

   extern UINT8 USBRequestReturnData[];
   extern UINT8 USBRequestIncomingData[];

   extern void *(*USBTxMemcpy)(void *dest, const void *source, UINT8 count);
   extern void *(*USBRxMemcpy)(void *dest, const void *source, UINT8 count);

   /* USBCdcGetLineCoding: Send a packet with the settings of the second uart back to the
   ** usb host.
   **  Line Coding Structure
   **  DTERate      | 4 | Data terminal rate, in bits per second
   **  CharFormat   | 1 | Stop bits, 0 = 1 Stop bit, 1 = 1,5 Stop bits, 2 = 2 Stop bits
   **  ParityType   | 1 | Parity, 0 = None, 1 = Odd, 2 = Even, 3= Mark, 4 = Space
   **  DataBits     | 1 | Data bits (5,6,7,8,16) */
   void USBCdcGetLineCoding(void) 
   {
     CDC_OUTPUT_ENDPOINT_DESCRIPTOR *descriptor = (CDC_OUTPUT_ENDPOINT_DESCRIPTOR *)
                                           OSGetIODescriptor(OS_IO_USB_OUTPUT_ENDPOINT3);
     USBRequestReturnData[6] = descriptor->DataBits; // Data bits = 8
     USBRequestReturnData[5] = descriptor->Parity;   // No Parity
     USBRequestReturnData[4] = descriptor->StopBits; // Stop bits = 1
     USBRequestReturnData[3] = descriptor->Baudrate >> 24;
     USBRequestReturnData[2] = descriptor->Baudrate >> 16;
     USBRequestReturnData[1] = descriptor->Baudrate >> 8;
     USBRequestReturnData[0] = descriptor->Baudrate;
     USBSendDataPacketOnEP0((UINT8 *)&USBRequestReturnData[0],0x07); // send data to host
   } /* end of USBCdcGetLineCoding */


   /* USBCdcSetLineCoding: Prepare EP0 to receive a packet with the settings for the
   ** second uart. */
   void USBCdcSetLineCoding(void)
   {
   	 // receive data over EP0 from Host
     USBReceiveDataPacketOnEP0((UINT8 *)&USBRequestIncomingData);
   } /* end of USBCdcSetLineCoding */


   /* USBSetControlLineState: Function set or reset RTS. */
   void USBSetControlLineState(void)
   {
     USBSendZeroLengthPacketOnIEP0(); // Send ZLP for status stage
   } /* end of USBSetControlLineState */


   /* USBCdcSetLineCodingHandler: Readout the settings (send from USB host) for the
   ** second uart. */
   void USBCdcSetLineCodingHandler(CDC_OUTPUT_ENDPOINT_DESCRIPTOR *descriptor)
   {
     /* Baudrate Settings */
     descriptor->Baudrate = (UINT32)USBRequestIncomingData[3] << 24 |
                            (UINT32)USBRequestIncomingData[2] << 16 |
                            (UINT32)USBRequestIncomingData[1] <<  8 | 
                            USBRequestIncomingData[0];
   } /* end of USBCdcSetLineCodingHandler */
   
   
   /* USBCdcInitInput: Input CDC interface configuration. */ 
   ENDPOINT_DESCRIPTOR *USBCdcInitInput(UINT8 maxNodesFifo, UINT8 maxNodeSizeFifo)
   {
     INPUT_ENDPOINT_DESCRIPTOR *descriptor;
   	 /* Input interrupt configuration */ 
     if ((descriptor = (INPUT_ENDPOINT_DESCRIPTOR *)
                                    OSAlloca(sizeof(INPUT_ENDPOINT_DESCRIPTOR))) == NULL)
        return NULL;
     descriptor->InterruptHandler = USBCdcInputHandler;
     if ((descriptor->FifoArray = OSInitFIFOQueue(maxNodesFifo,maxNodeSizeFifo)) == NULL)
        return NULL;
     descriptor->UsedNodes = 0; 
     descriptor->CurrentNode = NULL;
     return (ENDPOINT_DESCRIPTOR *)descriptor;
   } /* end of USBCdcInitInput */


   /* USBCdcInitOutput: Output CDC interface configuration. */ 
   ENDPOINT_DESCRIPTOR *USBCdcInitOutput(void (*OutputUserHandler)(UINT8 *, UINT8),
                                         UINT8 maxNodesFifo, UINT8 maxNodeSizeFifo)
   {
   	 CDC_OUTPUT_ENDPOINT_DESCRIPTOR *descriptor;
     if ((descriptor = (CDC_OUTPUT_ENDPOINT_DESCRIPTOR *)
                               OSAlloca(sizeof(CDC_OUTPUT_ENDPOINT_DESCRIPTOR))) == NULL)
        return NULL;
     descriptor->USBReceiveHandler = OutputUserHandler;
     descriptor->InterruptHandler = USBCdcOutputHandler;
     if ((descriptor->FifoArray = OSInitFIFOQueue(maxNodesFifo,maxNodeSizeFifo)) == NULL)
        return NULL;
     descriptor->FreeNodes = maxNodesFifo;
     descriptor->CurrentNode = NULL;
     descriptor->Baudrate = 0;
     descriptor->DataBits = 8;
     descriptor->StopBits = 0;
     descriptor->Parity = 0;
     return (ENDPOINT_DESCRIPTOR *)descriptor;
   } /* end of USBCdcInitOutput */
   
   
   /* USBCdcOutputHandler: Completely empties the USB buffers that have been filled by
   ** the USB manager. At the end of this call all output USB buffers become free. */
   void USBCdcOutputHandler(CDC_OUTPUT_ENDPOINT_DESCRIPTOR *descriptor)
   {
     UINT8 size, index, freeNodes;
     if (descriptor->CurrentNode == NULL) {
        descriptor->CurrentNode = (UINT8 *)OSGetFreeNodeFIFO(descriptor->FifoArray);
         do {
           freeNodes = OSUINT8_LL(&descriptor->FreeNodes);
        } while (!OSUINT8_SC(&descriptor->FreeNodes,freeNodes - 1));
     }
     while (*descriptor->USBCounter[descriptor->NextReadyUSBBufferIndex] & 0x80) {
        index = descriptor->NextReadyUSBBufferIndex;
        size = *descriptor->USBCounter[index] & 0x7F;
        USBRxMemcpy(descriptor->CurrentNode,descriptor->USBBuffer[index],size);
        *descriptor->USBCounter[index] = size; // Clear occupency bit
        descriptor->USBReceiveHandler(descriptor->CurrentNode,size);
        descriptor->NextReadyUSBBufferIndex = !descriptor->NextReadyUSBBufferIndex;
        /* Claude Si plus aucun noeud libre n'est disponnible alors il faut sortir 
        ** sans repermettre l'interruption. L'interruption sera repermise pas
        ** la fonction OSReleaseOutputNodeUSB (USB.c). */
        if (descriptor->FreeNodes == 0) {
           descriptor->CurrentNode = NULL;
           return;
        }
        descriptor->CurrentNode = (UINT8 *)OSGetFreeNodeFIFO(descriptor->FifoArray);
        do {
           freeNodes = OSUINT8_LL(&descriptor->FreeNodes);
        } while (!OSUINT8_SC(&descriptor->FreeNodes,freeNodes - 1));
     } 
     USBOEPIE |= descriptor->EndPointInterruptBit; // Enable output USB interrupt.
   } /* end of USBCdcOutputHandler */
   
   
   /* USBCdcInputHandler: */
   void USBCdcInputHandler(INPUT_ENDPOINT_DESCRIPTOR *descriptor)
   {
     UINT8 index, usedNodes;
     UINT16 sendSize;
     if (descriptor->CurrentNode == NULL) {
        descriptor->CurrentNode = (UINT8 *)OSDequeueFIFO(descriptor->FifoArray,
                                                       &(descriptor->CurrentNodeSize));
        descriptor->CurrentNodeIndex = 0;
     }
     while ((sendSize = descriptor->CurrentNodeSize - descriptor->CurrentNodeIndex) > 0
              && (*descriptor->USBCounter[descriptor->NextReadyUSBBufferIndex] & 0x80)) {
        index = descriptor->NextReadyUSBBufferIndex;
        if (sendSize > MAX_PACKET_SIZE)
     	   sendSize = MAX_PACKET_SIZE;
        USBRxMemcpy(descriptor->USBBuffer[index],descriptor->CurrentNode + 
                    descriptor->CurrentNodeIndex,sendSize);
        *descriptor->USBCounter[index] = sendSize;
  	    descriptor->CurrentNodeIndex += sendSize;
  	    descriptor->NextReadyUSBBufferIndex = !descriptor->NextReadyUSBBufferIndex;
     } 
     if (sendSize == 0) { // 
        OSReleaseNodeFIFO(descriptor->FifoArray,descriptor->CurrentNode);
        descriptor->CurrentNode = (UINT8 *)OSDequeueFIFO(descriptor->FifoArray,
                                                       &(descriptor->CurrentNodeSize));
        descriptor->CurrentNodeIndex = 0;
        do {
           usedNodes = OSUINT8_LL(&descriptor->UsedNodes);
        } while (!OSUINT8_SC(&descriptor->UsedNodes,usedNodes - 1));
     }
     if (descriptor->CurrentNode != NULL)
        USBIEPIE |= descriptor->EndPointInterruptBit; // Enable input USB interrupt.
   } /* end of USBCdcInputHandler */
   
#endif //ifdef _CDC_

