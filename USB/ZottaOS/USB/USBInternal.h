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
/* File USBInternal.h: .
** Version identifier: April 2010
** Authors: MIS-TIC
*/
/* (c)2009 by Texas Instruments Incorporated, All Rights Reserved. */

#ifndef USB_INTERNAL_H_
#define USB_INTERNAL_H_

#define MAX_ENDPOINT_NUMBER     0x07
#define EP0_MAX_PACKET_SIZE     0x08
#define EP0_PACKET_SIZE         0x08

//  Bit definitions for CONFIG_DESCRIPTOR.bmAttributes
#define CFG_DESC_ATTR_SELF_POWERED  0x40    // Bit 6: If set, device is self powered
#define CFG_DESC_ATTR_BUS_POWERED   0x80    // Bit 7: If set, device is bus powered
#define CFG_DESC_ATTR_REMOTE_WAKE   0x20    // Bit 5: If set, device supports remote wakeup

//  Descriptor Type Values
#define DESC_TYPE_DEVICE                1       // Device Descriptor (Type 1)
#define DESC_TYPE_CONFIG                2       // Configuration Descriptor (Type 2)
#define DESC_TYPE_STRING                3       // String Descriptor (Type 3)
#define DESC_TYPE_INTERFACE             4       // Interface Descriptor (Type 4)
#define DESC_TYPE_ENDPOINT              5       // Endpoint Descriptor (Type 5)
#define DESC_TYPE_DEVICE_QUALIFIER      6       // Endpoint Descriptor (Type 6)
#define DESC_TYPE_HUB                   0x29    // Hub Descriptor (Type 6)
#define DESC_TYPE_HID			        0x21    // HID Descriptor
#define DESC_TYPE_REPORT		        0x22    // Report Descriptor
#define DESC_TYPE_PHYSICAL		        0x23	// Physical Descriptor

//  INTERFACE_DESCRIPTOR structure
#define SIZEOF_INTERFACE_DESCRIPTOR 0x09

//  ENDPOINT_DESCRIPTOR structure
#define SIZEOF_ENDPOINT_DESCRIPTOR 0x07

//  CONFIG_DESCRIPTOR structure
#define SIZEOF_CONFIG_DESCRIPTOR 0x09

//  HID DESCRIPTOR structure
#define SIZEOF_HID_DESCRIPTOR 0x09

//  Bit definitions for EndpointDescriptor.EndpointAddr
#define EP_DESC_ADDR_EP_NUM     0x0F    // Bit 3-0: Endpoint number
#define EP_DESC_ADDR_DIR_IN     0x80    // Bit 7: Direction of endpoint, 1/0 = In/Out

//  Bit definitions for EndpointDescriptor.EndpointFlags
#define EP_DESC_ATTR_TYPE_MASK  0x03    // Mask value for bits 1-0
#define EP_DESC_ATTR_TYPE_CONT  0x00    // Bit 1-0: 00 = Endpoint does control transfers
#define EP_DESC_ATTR_TYPE_ISOC  0x01    // Bit 1-0: 01 = Endpoint does isochronous transfers
#define EP_DESC_ATTR_TYPE_BULK  0x02    // Bit 1-0: 10 = Endpoint does bulk transfers
#define EP_DESC_ATTR_TYPE_INT   0x03    // Bit 1-0: 11 = Endpoint does interrupt transfers

typedef struct EDB {
  UINT8 EPCNF;             // Endpoint Configuration
  UINT8 EPBBAX;            // Endpoint X Buffer Base Address
  UINT8 EPBCTX;            // Endpoint X Buffer UINT8 Count
  UINT8 SPARE0;            // no used
  UINT8 SPARE1;            // no used
  UINT8 EPBBAY;            // Endpoint Y Buffer Base Address
  UINT8 EPBCTY;            // Endpoint Y Buffer UINT8 Count
  UINT8 EPSIZXY;           // Endpoint XY Buffer Size
} EDB;

typedef struct EP0_DESCRIPTOR {
  void (*InterruptHandler)(struct EP0_DESCRIPTOR  *);
  UINT8 *NextByte;
  UINT16 BytesRemaining; // A value of 0 means that a 0-length data packet
                         // A value of 0xFFFF means that transfer is complete.
} EP0_DESCRIPTOR;

typedef struct ENDPOINT_DESCRIPTOR {
  void (*InterruptHandler)(struct ENDPOINT_DESCRIPTOR  *);
  UINT8 *USBCounter[2];
  UINT8 *USBBuffer[2];
  UINT8 NextReadyUSBBufferIndex;
  UINT8 EndPointInterruptBit;
} ENDPOINT_DESCRIPTOR;

typedef struct CDC_OUTPUT_ENDPOINT_DESCRIPTOR {
  void (*InterruptHandler)(struct CDC_OUTPUT_ENDPOINT_DESCRIPTOR  *);
  UINT8 *USBCounter[2];
  UINT8 *USBBuffer[2];
  UINT8 NextReadyUSBBufferIndex;
  UINT8 EndPointInterruptBit;
  UINT8 FreeNodes;
  void (*USBReceiveHandler)(UINT8 *, UINT8);
  void *FifoArray; // Descriptor to FIFO
  UINT8 *CurrentNode;
  UINT32 Baudrate;
  UINT8 DataBits;
  UINT8 StopBits;
  UINT8 Parity;
} CDC_OUTPUT_ENDPOINT_DESCRIPTOR;

typedef struct HID_OUTPUT_ENDPOINT_DESCRIPTOR {
  void (*InterruptHandler)(struct HID_OUTPUT_ENDPOINT_DESCRIPTOR  *);
  UINT8 *USBCounter[2];
  UINT8 *USBBuffer[2];
  UINT8 NextReadyUSBBufferIndex;
  UINT8 EndPointInterruptBit;
  UINT8 FreeNodes;
  void (*UserOutputHandler)(UINT8 *, UINT8);
} HID_OUTPUT_ENDPOINT_DESCRIPTOR;

typedef struct INPUT_ENDPOINT_DESCRIPTOR {
  void (*InterruptHandler)(struct INPUT_ENDPOINT_DESCRIPTOR *);
  UINT8 *USBCounter[2];
  UINT8 *USBBuffer[2];
  UINT8 NextReadyUSBBufferIndex;
  UINT8 EndPointInterruptBit;
  UINT8 UsedNodes;
  void *FifoArray; // Descriptor to FIFO
  UINT8 *CurrentNode;
  UINT16 CurrentNodeIndex;
  UINT16 CurrentNodeSize; 
} INPUT_ENDPOINT_DESCRIPTOR;

// Base addresses of transmit and receive buffers
#define START_OF_USB_BUFFER     0x1C00
#define OEP1_X_BUFFER_ADDRESS   0x1C00  // Input  Endpoint 1 X Buffer Base-address
#define OEP1_Y_BUFFER_ADDRESS   0x1C40  // Input  Endpoint 1 Y Buffer Base-address
#define IEP1_X_BUFFER_ADDRESS   0x1C80  // Output Endpoint 1 X Buffer Base-address
#define IEP1_Y_BUFFER_ADDRESS   0x1CC0  // Output Endpoint 1 Y Buffer Base-address
#define OEP2_X_BUFFER_ADDRESS   0x1D00  // Input  Endpoint 2 X Buffer Base-address
#define OEP2_Y_BUFFER_ADDRESS   0x1D40  // Input  Endpoint 2 Y Buffer Base-address
#define IEP2_X_BUFFER_ADDRESS   0x1D80  // Output Endpoint 2 X Buffer Base-address
#define IEP2_Y_BUFFER_ADDRESS   0x1DC0  // Output Endpoint 2 Y Buffer Base-address
#define OEP3_X_BUFFER_ADDRESS   0x1E00  // Input  Endpoint 2 X Buffer Base-address
#define OEP3_Y_BUFFER_ADDRESS   0x1E40  // Input  Endpoint 2 Y Buffer Base-address
#define IEP3_X_BUFFER_ADDRESS   0x1E80  // Output Endpoint 2 X Buffer Base-address
#define IEP3_Y_BUFFER_ADDRESS   0x1EC0  // Output Endpoint 2 Y Buffer Base-address
#define OEP4_X_BUFFER_ADDRESS   0x1F00  // Input  Endpoint 2 X Buffer Base-address
#define OEP4_Y_BUFFER_ADDRESS   0x1F40  // Input  Endpoint 2 Y Buffer Base-address
#define IEP4_X_BUFFER_ADDRESS   0x1F80  // Output Endpoint 2 X Buffer Base-address
#define IEP4_Y_BUFFER_ADDRESS   0x1FC0  // Output Endpoint 2 Y Buffer Base-address
#define OEP5_X_BUFFER_ADDRESS   0x2000  // Input  Endpoint 2 X Buffer Base-address
#define OEP5_Y_BUFFER_ADDRESS   0x2040  // Input  Endpoint 2 Y Buffer Base-address
#define IEP5_X_BUFFER_ADDRESS   0x2080  // Output Endpoint 2 X Buffer Base-address
#define IEP5_Y_BUFFER_ADDRESS   0x20C0  // Output Endpoint 2 Y Buffer Base-address
#define OEP6_X_BUFFER_ADDRESS   0x2100  // Input  Endpoint 2 X Buffer Base-address
#define OEP6_Y_BUFFER_ADDRESS   0x2140  // Input  Endpoint 2 Y Buffer Base-address
#define IEP6_X_BUFFER_ADDRESS   0x2180  // Output Endpoint 2 X Buffer Base-address
#define IEP6_Y_BUFFER_ADDRESS   0x21C0  // Output Endpoint 2 Y Buffer Base-address
#define OEP7_X_BUFFER_ADDRESS   0x2200  // Input  Endpoint 2 X Buffer Base-address
#define OEP7_Y_BUFFER_ADDRESS   0x2240  // Input  Endpoint 2 Y Buffer Base-address
#define IEP7_X_BUFFER_ADDRESS   0x2280  // Output Endpoint 2 X Buffer Base-address
#define IEP7_Y_BUFFER_ADDRESS   0x22C0  // Output Endpoint 2 Y Buffer Base-address


/* Init and start the USB PLL. */
UINT8 USBEnable();

/* Disables the USB module and PLL. */
UINT8 USBDisable(void);

/* Instruct USB module to make itself available to the PC for connection, by pulling PUR high. */
UINT8 USBConnect();

/* Force a disconnect from the PC by pulling PUR low. */
UINT8 USBDisconnect();

/* Reset USB-SIE and global variables. */
UINT8 USBReset();

/* Suspend USB. */
UINT8 USBSuspend(void);

/* Resume USB. */
UINT8 USBResume(void);

/* Returns the status of the USB connection. */
UINT8 USBConnectionInfo(void);

/* These functions is to be used ONLY by USB stack, and not by application */

/* Send stall handshake for in- and out-endpoint0 (control pipe) */
void USBStallEndpoint0(void);

/* Send stall handshake for out-endpoint0 (control pipe) */
void USBStallOEP0(void);

/* Send further data over control pipe if needed. Function is called from control-in IRQ.
** Do not call from user application. */
void USBSendNextPacketOnIEP0(void);

/* Send data over control pipe to host.
** Number of UINT8s to transmit should be set with global varible "wUINT8sRemainingOnIEP0"
** before function is called. */
void USBSendDataPacketOnEP0(UINT8 *pbBuffer, UINT16 size);

/* Receive further data from control pipe if needed.
** Function is called from control-out IRQ. Do not call from user application */
void USBReceiveNextPacketOnOEP0(void);

/* Receive data from control pipe.
** Number of UINT8s to receive should be set with global varible "wUINT8sRemainingOnOEP0" 
** before function is called. */
void USBReceiveDataPacketOnEP0(UINT8 *pbBuffer);

/* Send zero length packet on control pipe. */
void USBSendZeroLengthPacketOnIEP0(void);

/* Decode incoming usb setup packet and call corresponding function
** usbDecodeAndProcessUsbRequest is called from IRQ. Do not call from user application
** returns TRUE to keep CPU awake */
UINT8 USBDecodeAndProcessUsbRequest(void);

/* USBGetFreeNode: This function returns a free buffer that an application can fill
** with useful data and then transfer it to OSEnqueueUSB. The free buffer should come
** from a pool of buffers that is associated with the USB queue for which the ap-
** plication will later invoke an OSEnqueueUSB operation.
** Returned value: If all the pre-allocated buffers created when calling OSInitUART are
**   in use, OSGetFreeNodeUSB returns NULL. Otherwise the function returns the starting
**   address of the buffer that can be filled. This same address can then be transferred
**   to OSEnqueueUART, or it can be released to the free pool of buffers by calling func-
**   tion USBHidReleaseNode. */
void *USBGetFreeNode(UINT8 portIndex);

/* USBHidReleaseNode: This function releases a buffer that was obtained from USBHidGet-
** FreeNode and returns it to the pool of available buffers associated with USB device.
** This function can be handy when the logic of the program first obtains a buffer from
** USBHidGetFreeNode to later realize that the buffer cannot be used.
** Once released, the buffer contents must neither be accessed nor modified.
** Parameters:
**   (1) (void *) buffer: Pointer to the buffer to free;
** Returned value: None. */
void USBReleaseNode(void *buffer,UINT8 portIndex);

/* USBHidOSEnqueue: This function is called by an application task to output a string of
** bytes contained in a buffer that was provided by USBHidGetFreeNode. The buffer that is
** transmitted with the call must not be freed with USBHidReleaseNode as it is given to
** the UART which will later free it once its contents has been transmitted.
** Parameters:
**   (1) (void *) buffer: Byte contents to output from the UART;
**   (2) (void *) dataSize: Number of bytes to consider from 1st parameter;
** Returned value: None. The function always succeeds as memory availability is checked
**   when obtaining the buffer from OSGetFreeNodeUSB.
** Notes: It is important that the buffer supplied with this call originally comes from
** USBHidGetFreeNode. Failure to comply with this restriction will lead into an erratic
** behavior. */
void USBEnqueue(UINT8 *buffer, UINT8 dataSize,UINT8 portIndex);

#endif /*USB_INTERNAL_H_*/
