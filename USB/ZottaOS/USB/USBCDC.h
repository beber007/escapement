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
/* File USBCDC.h: .
** Version identifier: April 2010
** Authors: MIS-TIC
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
