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
/* File USBHID.h: .
** Version identifier: April 2010
** Authors: MIS-TIC
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
