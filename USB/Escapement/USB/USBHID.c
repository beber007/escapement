/* Copyright (c) 2026 Bertrand Hurst. All rights reserved.
** Escapement - Lightweight Power-Aware Real-Time OS.
** Derived from prior work; see LICENSE and NOTICE at the root of this repository.
*/
/* File USBHID.c: .
** Version identifier: April 2010
*/
/* (c)2009 by Texas Instruments Incorporated, All Rights Reserved. */

#include "..\Escapement.h"
#include "..\Escapement_msp430.h"
#include "USB.h"
#include "USBInternal.h"
#include "descriptors.h"

#ifdef _HID_

   static void USBHidInputHandler(INPUT_ENDPOINT_DESCRIPTOR *descriptor);
   static void USBHidOutputHandler(HID_OUTPUT_ENDPOINT_DESCRIPTOR *descriptor);

   extern UINT8 USBRequestReturnData[];
   extern UINT8 USBRequestIncomingData[];

   extern void *(*USBTxMemcpy)(void * dest, const void * source, UINT8 count);
   extern void *(*USBRxMemcpy)(void * dest, const void * source, UINT8 count);

   /* USBHidGetHidDescriptor: Return Hid descriptor to host over control endpoint. */
   void USBHidGetHidDescriptor(void)
   {
     USBSendDataPacketOnEP0((UINT8 *)
                               &ConfigurationDescriptorGroup[START_HID_DESCRIPTOR],9);
   } /* end of USBHidGetHidDescriptor */


   /* USBHidGetReportDescriptor: Return HID report descriptor to host over control
   ** endpoint. */
   void USBHidGetReportDescriptor(void)
   {
     USBSendDataPacketOnEP0((UINT8 *)&ReportDescriptor,SIZEOF_REPORT_DESCRIPTOR);
   } /* end of USBHidGetReportDescriptor */


   /* USBHidSetReport: Receive Out-report from host. */
   void USBHidSetReport(void)
   {
   	 // receive data over EP0 from Host.
     USBReceiveDataPacketOnEP0((UINT8 *)&USBRequestIncomingData);
   } /* end of USBHidSetReport */


   /* USBHidGetReport: Return In-report or In-feature-report to host over interrupt
   ** endpoint. */
   void USBHidGetReport(void)
   {
   } /* end of USBHidGetReport */


   /* USBHidInitOutput: Output interface configuration */ 
   ENDPOINT_DESCRIPTOR *USBHidInitOutput(void (*OutputUserHandler)(UINT8 *, UINT8))
   {
   	 HID_OUTPUT_ENDPOINT_DESCRIPTOR *descriptor;
     if ((descriptor = (HID_OUTPUT_ENDPOINT_DESCRIPTOR *)
                               OSAlloca(sizeof(HID_OUTPUT_ENDPOINT_DESCRIPTOR))) == NULL)
        return NULL;
     descriptor->UserOutputHandler = OutputUserHandler;
     descriptor->InterruptHandler = USBHidOutputHandler;
     return (ENDPOINT_DESCRIPTOR *)descriptor;
   } /* end of USBHidInitOutput */

 
   /* USBHidOutputHandler: */
   void USBHidOutputHandler(HID_OUTPUT_ENDPOINT_DESCRIPTOR *descriptor)
   {
     UINT8 index;
     while (*descriptor->USBCounter[descriptor->NextReadyUSBBufferIndex] & 0x80) {
     	index = descriptor->NextReadyUSBBufferIndex;
        descriptor->UserOutputHandler(descriptor->USBBuffer[index] + 2,
                                      descriptor->USBBuffer[index][1]);
        *(descriptor->USBCounter[index]) &= 0x7F;
        descriptor->NextReadyUSBBufferIndex = !descriptor->NextReadyUSBBufferIndex;
     }
     USBOEPIE |= descriptor->EndPointInterruptBit;
   } /* end of USBHidOutputHandler */


   /* USBHidInitInput: Input interface configuration */ 
   ENDPOINT_DESCRIPTOR *USBHidInitInput(UINT8 maxNodesInputFifo,
                                        UINT8 maxNodeSizeInputFifo)
   {
     INPUT_ENDPOINT_DESCRIPTOR *descriptor;
     if ((descriptor = (INPUT_ENDPOINT_DESCRIPTOR *)
                                    OSAlloca(sizeof(INPUT_ENDPOINT_DESCRIPTOR))) == NULL)
        return NULL;
     descriptor->InterruptHandler = USBHidInputHandler;
     if ((descriptor->FifoArray = OSInitFIFOQueue(maxNodesInputFifo,
                                                  maxNodeSizeInputFifo)) == NULL)
        return NULL;
     descriptor->CurrentNode = NULL;
     descriptor->UsedNodes = 0;
     return (ENDPOINT_DESCRIPTOR*)descriptor;
   } /* end of USBHidInitInput */


   /* USBHidInputHandler: */
   void USBHidInputHandler(INPUT_ENDPOINT_DESCRIPTOR *descriptor)
   {
     UINT8 index, usedNodes;
     UINT16 sendSize;
     if (descriptor->CurrentNode == NULL) {
        descriptor->CurrentNode = (UINT8 *)OSDequeueFIFO(descriptor->FifoArray,
                                                         &(descriptor->CurrentNodeSize));
        descriptor->CurrentNodeIndex = 0;
     }
     while ((sendSize = descriptor->CurrentNodeSize - descriptor->CurrentNodeIndex) >  0 
            && (*descriptor->USBCounter[descriptor->NextReadyUSBBufferIndex] & 0x80)) {
        index = descriptor->NextReadyUSBBufferIndex;
        if (sendSize > MAX_PACKET_SIZE - 2)
     	   sendSize = MAX_PACKET_SIZE - 2;
        descriptor->USBBuffer[index][0] = 0x3f; // set HID report descriptorriptor: 0x3F
        descriptor->USBBuffer[index][1] = sendSize; // set byte count of valid data
        USBRxMemcpy(descriptor->USBBuffer[index] + 2,descriptor->CurrentNode + 
                    descriptor->CurrentNodeIndex,sendSize);
        *descriptor->USBCounter[index] = MAX_PACKET_SIZE;
  	    descriptor->CurrentNodeIndex += sendSize;
  	    descriptor->NextReadyUSBBufferIndex = !descriptor->NextReadyUSBBufferIndex;
     } 
     if (sendSize == 0) {
        OSReleaseNodeFIFO(descriptor->FifoArray,descriptor->CurrentNode);
        descriptor->CurrentNode = 
            (UINT8 *)OSDequeueFIFO(descriptor->FifoArray,&(descriptor->CurrentNodeSize));
        descriptor->CurrentNodeIndex = 0;
        do {
           usedNodes = OSUINT8_LL(&descriptor->UsedNodes);
        } while (!OSUINT8_SC(&descriptor->UsedNodes,usedNodes - 1));
     }
     /* Claude Si plus aucun noeud contenant des donées devant être envoié n'est 
     ** disponnible alors il faut sortir sans repermettre l'interruption. L'interruption
     ** sera repermise par la fonction OSEnqueueInputUSB (USB.c). */
     if (descriptor->CurrentNode != NULL)
        USBIEPIE |= descriptor->EndPointInterruptBit;
   } /* end of USBHidInputHandler */
 
#endif
