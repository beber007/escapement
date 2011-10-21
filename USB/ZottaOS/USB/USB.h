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
/* File USB.h: .
** Version identifier: April 2010
** Authors: MIS-TIC
*/
/* (c)2009 by Texas Instruments Incorporated, All Rights Reserved. */

#ifndef _USB_H_
#define _USB_H_

/* Function return values */
#define USB_SUCCEED          0x00
#define USB_GENERAL_ERROR    0x01
#define USB_NOT_ENABLE       0x02
#define USB_VBUS_NOT_PRESENT 0x03

/* return values USB_connectionInfo(), USB_connect() */
#define USB_VBUS_PRESENT    0x01
#define USB_BUS_ACTIVE      0x02    // frame sync packets are being received
#define USB_CONNECT_NO_VBUS 0x04
#define USB_SUSPENDED       0x08
#define USB_NOT_SUSPENDED   0x10
#define USB_ENUMERATED      0x20
#define USB_PURHIGH         0x40

/* USB connection states */
#define ST_USB_DISCONNECTED         0x80
#define ST_USB_CONNECTED_NO_ENUM    0x81
#define ST_ENUM_IN_PROGRESS         0x82
#define ST_ENUM_ACTIVE              0x83
#define ST_ENUM_SUSPENDED           0x84
#define ST_FAILED_ENUM              0x85
#define ST_ERROR                    0x86
#define ST_NOENUM_SUSPENDED         0x87

/* MSP430 USB Module Management functions, these functions can be used in application */

/* Init the USB HW interface. */
/* OSInitUSB: Called at task creation time in the main program to initialize the inter-
** nal data structure associated with a UART device. This function initializes the kernel
** internals in order to store the bytes that are to be sent on a specified output port.
** The function must be called before doing any I/O related with the UART and can be call-
** ed before or after creating the application tasks but never from an application task.
** Low-level protocol, e.g. RS232 or I2C, and transmission baud rate of an I/O port must
** be done by the application, typically in the main program. The kernel does not provide
** a function to accomplish this task because low-level hardware settings depend on the
** particular microcontroller at hand.
** Parameters:
**   (1) (void (*)(UINT8)) ReceiveHandler: ISR function that is called when the UART re-
**       ceives a new byte from its input port.
**   (2) (UINT8) receivePortIndex: Port number corresponding to a specific UART.
**   (3) (UINT8) maxNodes: Maximum number of outstanding messages (or message parts) that
**       can be enqueued at once. This value also corresponds to the initial number of
**       free buffers that are created and inserted into the free list associated with a
**       particular UART device.
**   (4) (UINT8) maxNodeSize: Byte size of the buffers that are in the free list. This
**       value also corresponds to the maximum number of bytes that can be inserted into
**       a message that the UART can transmit.
**   (5) (UINT8) transmitPortIndex: Port number corresponding to a specific UART.
** Returned value: (BOOL) TRUE on success and FALSE if there's insufficient memory to
**   create the descriptor or the buffers that will be used in connection with the UART.
** Note: Although the implementation is fully concurrent, the UART does not prevent mes-
** sage parts from being interleaved. Therefore maxNodeSize should be the maximal size of
** a message that is filled by an application task and transferred to the UART. However
** as the implementation guarantees a FIFO processing of the messages transferred to the
** UART, a long message can be broken into smaller parts when there is a single applica-
** tion task that transmits messages through the UART. */ 
BOOL OSInitUSB(void);

/* Force a remote wakeup of the USB host.
** This method can be generated only if device supports
** remote wake-up feature in some of its configurations.
** The method wakes-up the USB bus only if wake-up feature is enabled by the host. */
UINT8 OSForceRemoteWakeupUSB(void);

/* Returns the state of the USB connection. */
UINT8 OSConnectionStateUSB(void);


/* OSInitOutputUSB: . 
** Note: maxNodesFifo & maxNodeSizeFifo used only for CDC. */
BOOL OSInitOutputUSB(void (*userHandler)(UINT8 *, UINT8), UINT8 maxNodesFifo, 
                     UINT8 maxNodeSizeFifo, UINT8 portIndex);

/* OSReleaseOutputNodeUSB: . */
void OSReleaseOutputNodeUSB(void * node,UINT8 portIndex);

/* OSEnqueueOutputNodeUSB: . */
void OSEnqueueOutputNodeUSB(UINT8 *node, UINT8 nodeSize, UINT8 portIndex);

/* OSDequeueOutputNodeUSB: . */
void *OSDequeueOutputNodeUSB(UINT16 *nodeSize, UINT8 portIndex);


/* OSInitOutputUSB: . */
BOOL OSInitInputUSB(UINT8 maxNodesFifo, UINT8 maxNodeSizeFifo,UINT8 portIndex);

/* OSGetFreeNodeInputNodeUSB: . */
void *OSGetFreeNodeInputNodeUSB(UINT8 portIndex);

/* OSReleaseInputNodeUSB: Releases a buffer that was obtained from USBGetFreeNode and
** returns it to the pool of available buffers. */
void OSReleaseInputNodeUSB(void *node, UINT8 portIndex);
  
/* OSEnqueueInputUSB: Called by an application task to output a string of bytes
** contained in a buffer that was provided by USBGetFreeNode. */
void OSEnqueueInputUSB(void * node, UINT8 nodeSize, UINT8 portIndex);

#endif /* _USB_H */
