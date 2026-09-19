/* Copyright (c) 2026 Bertrand Hurst. All rights reserved.
** Escapement - Lightweight Power-Aware Real-Time OS.
** Derived from prior work; see LICENSE and NOTICE at the root of this repository.
*/
/* File USBCDC.h: .
** Version identifier: April 2010
*/
/* (c)2009 by Texas Instruments Incorporated, All Rights Reserved. */

#ifndef _USBCDC_H_
#define _USBCDC_H_

/* These functions is to be used ONLY by USB stack, and not by application */

/* Send a packet with the settings of the second uart back to the USB host */
void USBCdcGetLineCoding(void); 

/* Prepare EP0 to receive a packet with the settings for the second uart */
void USBCdcSetLineCoding(void);

/* Function set or reset RTS */
void USBSetControlLineState(void);

/* Readout the settings (send from USB host) for the second uart */
void USBCdcSetLineCodingHandler(CDC_OUTPUT_ENDPOINT_DESCRIPTOR *descriptor);

/* Input interface configuration */ 
ENDPOINT_DESCRIPTOR *USBCdcInitInput(UINT8 maxNodesFifo, UINT8 maxNodeSizeFifo);

/* Output interface configuration */ 
ENDPOINT_DESCRIPTOR *USBCdcInitOutput(void (*OutputUserHandler)(UINT8 *, UINT8), UINT8 maxNodesFifo, UINT8 maxNodeSizeFifo);

#endif //_USBCDC_H_
