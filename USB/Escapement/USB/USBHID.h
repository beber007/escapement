/* Copyright (c) 2026 Bertrand Hurst. All rights reserved.
** Escapement - Lightweight Power-Aware Real-Time OS.
** Derived from prior work; see LICENSE and NOTICE at the root of this repository.
*/
/* File USBHID.h: .
** Version identifier: April 2010
*/
/* (c)2009 by Texas Instruments Incorporated, All Rights Reserved. */

#ifndef _USBHID_H_
#define _USBHID_H_

/* Return Hid descriptor to host over control endpoint */
void USBHidGetHidDescriptor(void);

/* Return HID report descriptor to host over control endpoint */
void USBHidGetReportDescriptor(void);

/* Receive Out-report from host */
void USBHidSetReport(void);

/* Return In-report or In-feature-report to host over interrupt endpoint */
void USBHidGetReport(void);

/* USBHidInitInput: Input interface configuration */ 
ENDPOINT_DESCRIPTOR *USBHidInitInput(UINT8 maxNodesInputFifo, UINT8 maxNodeSizeInputFifo);

/* USBHidInitOutput: Output interface configuration */ 
ENDPOINT_DESCRIPTOR *USBHidInitOutput(void (*OutputUserHandler)(UINT8 *, UINT8));

#endif //_USBHID_H_
