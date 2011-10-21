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
/* File USB.c: .
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
#include "USBHid.h"
#include "..\DMA\DMA.h"
#include "..\VCORE\VCORE.h"


#ifdef USB_DRIVER

/* Internal Constant Definition */

#define NO_MORE_DATA         0xFFFF
#define ENUMERATION_COMPLETE 0x01
#define DISABLE              0x0
#define ENABLE               0x1

#define USB_RETURN_DATA_LENGTH  8

/* EndPoint Desciptor Block Bits */
#define EPCNF_USBIE     0x04      // USB Interrupt on Transaction Completion. Set By MCU
                                  // 0:No Interrupt, 1:Interrupt on completion
#define EPCNF_STALL     0x08      // USB Stall Condition Indication. Set by UBM
                                  // 0: No Stall, 1:USB Install Condition
#define EPCNF_DBUF      0x10      // Double Buffer Enable. Set by MCU
                                  // 0: Primary Buffer Only(x-buffer only), 1:Toggle Bit
                                  // Selects Buffer
#define EPCNF_TOGGLE    0x20      // USB Toggle bit. This bit reflects the toggle sequence
                                  // bit of DATA0 and DATA1.
#define EPCNF_UBME      0x80      // UBM Enable or Disable bit. Set or Clear by MCU.
                                  // 0:UBM can't use this endpoint
                                  // 1:UBM can use this endpoint
#define EPBCNT_UINT8CNT_MASK 0x7F // MASK for Buffer UINT8 Count
#define EPBCNT_NAK           0x80 // NAK, 0:No Valid in buffer, 1:Valid packet in buffer

/*  Bit definitions for DEVICE_REQUEST.RequestType */
/*  Bit 7:   Data direction */
#define USB_REQ_TYPE_OUTPUT     0x00    // 0 = Host sending data to device
#define USB_REQ_TYPE_INPUT      0x80    // 1 = Device sending data to host
/*  Bit 6-5: Type */
#define USB_REQ_TYPE_MASK       0x60    // Mask value for bits 6-5
#define USB_REQ_TYPE_STANDARD   0x00    // 00 = Standard USB request
#define USB_REQ_TYPE_CLASS      0x20    // 01 = Class specific
#define USB_REQ_TYPE_VENDOR     0x40    // 10 = Vendor specific
/*  Bit 4-0: Recipient */
#define USB_REQ_TYPE_RECIP_MASK 0x1F    // Mask value for bits 4-0
#define USB_REQ_TYPE_DEVICE     0x00    // 00000 = Device
#define USB_REQ_TYPE_INTERFACE  0x01    // 00001 = Interface
#define USB_REQ_TYPE_ENDPOINT   0x02    // 00010 = Endpoint
#define USB_REQ_TYPE_OTHER      0x03    // 00011 = Other

/* Values for DEVICE_REQUEST.Request */
/* Standard Device Requests */
#define USB_REQ_GET_STATUS              0
#define USB_REQ_CLEAR_FEATURE           1
#define USB_REQ_SET_FEATURE             3
#define USB_REQ_SET_ADDRESS             5
#define USB_REQ_GET_DESCRIPTOR          6
#define USB_REQ_SET_DESCRIPTOR          7
#define USB_REQ_GET_CONFIGURATION       8
#define USB_REQ_SET_CONFIGURATION       9
#define USB_REQ_GET_INTERFACE           10
#define USB_REQ_SET_INTERFACE           11
#define USB_REQ_SYNCH_FRAME             12
/* CDC CLASS Requests */
#define USB_CDC_GET_LINE_CODING         0x21
#define USB_CDC_SET_LINE_CODING         0x20
#define USB_CDC_SET_CONTROL_LINE_STATE  0x22
/* HID CLASS Requests */
#define USB_HID_REQ                     0x81
#define USB_REQ_GET_REPORT		        0x01
#define USB_REQ_GET_IDLE		        0x02
#define USB_REQ_SET_REPORT		        0x09
#define USB_REQ_SET_IDLE		        0x0A
#define USB_REQ_SET_PROTOCOL            0x0B
#define USB_REQ_GET_PROTOCOL            0x03
/*  Feature Selector Values */
#define FEATURE_REMOTE_WAKEUP           1  // Remote wakeup (Type 1)
#define FEATURE_ENDPOINT_STALL          0  // Endpoint stall (Type 0)
/* Device Status Values */
#define DEVICE_STATUS_REMOTE_WAKEUP     0x02
#define DEVICE_STATUS_SELF_POWER        0x01
/* DEVICE_DESCRIPTOR structure */
#define SIZEOF_DEVICE_DESCRIPTOR        0x12
#define OFFSET_DEVICE_DESCRIPTOR_VID_L  0x08
#define OFFSET_DEVICE_DESCRIPTOR_VID_H  0x09
#define OFFSET_DEVICE_DESCRIPTOR_PID_L  0x0A
#define OFFSET_DEVICE_DESCRIPTOR_PID_H  0x0B
#define OFFSET_CONFIG_DESCRIPTOR_POWER  0x07
#define OFFSET_CONFIG_DESCRIPTOR_CURT   0x08


/* DEVICE_REQUEST Structure */
typedef struct DEVICE_REQUEST {
  UINT8 RequestType;  // See bit definitions below
  UINT8 Request;      // See value definitions below
  UINT16 Value;       // Meaning varies with request type
  UINT16 Index;       // Meaning varies with request type
  UINT16 Length;      // Number of bytes of data to transfer
} DEVICE_REQUEST;

typedef struct DEVICE_REQUEST_COMPARE {
  UINT8 RequestType;  // See bit definitions below
  UINT8 Request;      // See value definitions below
  UINT8 ValueL;       // Meaning varies with request type
  UINT8 ValueH;       // Meaning varies with request type
  UINT8 IndexL;       // Meaning varies with request type
  UINT8 IndexH;       // Meaning varies with request type
  UINT8 LengthL;      // Number of bytes of data to transfer (LSByte)
  UINT8 LengthH;      // Number of bytes of data to transfer (MSByte)
  UINT8 CompareMask;  // MSB is bRequest, if set 1, bRequest should be matched
  void (*USBFunction)(void); // function pointer
} DEVICE_REQUEST_COMPARE;

typedef enum {
  STATUS_ACTION_NOTHING,
  STATUS_ACTION_DATA_IN,
  STATUS_ACTION_DATA_OUT
} STATUS_ACTION_LIST;

typedef struct DEFAULT_DESCRIPTOR {
  void (*InterruptHandler)(void *);
} DEFAULT_DESCRIPTOR;

/* Hardware Related Structure Definition */
typedef struct EDB0 {
  UINT8 IEPCNFG; // Input Endpoint 0 Configuration Register
  UINT8 IEPBCNT; // Input Endpoint 0 Buffer UINT8 Count
  UINT8 OEPCNFG; // Output Endpoint 0 Configuration Register
  UINT8 OEPBCNT; // Output Endpoint 0 Buffer UINT8 Count
} EDB0;


/* Internal Variables */
static UINT8 ConfigurationNumber; // Set to 1 when USB device has been
                                  // configured, set to 0 when unconfigured
static UINT8 InterfaceNumber;     // interface number
static UINT8 HostAskMoreDataThanAvailable = 0;
UINT8 USBRequestReturnData[USB_RETURN_DATA_LENGTH];
UINT8 USBRequestIncomingData[USB_RETURN_DATA_LENGTH];
UINT8 SerialStringDescriptor[34];
UINT8 StatusAction;
UINT8 FunctionSuspended = FALSE;  // TRUE if function is suspended
UINT8 EnumerationStatus = 0;      // is 0 if not enumerated
static UINT8 RemoteWakeup;


/* This variables is defined in USB.cmd file */
extern DEVICE_REQUEST SetupPacket;
extern EDB0 EndPoint0DescriptorBlock;
extern EDB InputEndPointDescriptorBlock[MAX_ENDPOINT_NUMBER];
extern EDB OutputEndPointDescriptorBlock[MAX_ENDPOINT_NUMBER];
extern UINT8 IEP0Buffer[EP0_MAX_PACKET_SIZE];
extern UINT8 OEP0Buffer[EP0_MAX_PACKET_SIZE];

/* Internal functions definition */
static void *USBMemcpyV(void * dest, const void * source, UINT8 count);
static void USBInitMemcpy(void);
static void USBInitSerialStringDescriptor(void);
static void USBSetupPacketInterruptHandler(void *descriptor);
static void USBPWRVBUSonHandler(void *descriptor);
static void USBPWRVBUSoffHandler(void *descriptor);
static void USBIEP0InterruptHandler(void *descriptor);
static void USBOEP0InterruptHandler(void *descriptor);
static void USBDoNothingHandler(void *descriptor);

/* This function is used in ZottaOS_msp430xxx.asm */
void USBKernelSleep(void);

static UINT8 OldVCORE; // used to store VCORE setting when USB is unpluged 

void *(*USBTxMemcpy)(void * dest, const void * source, UINT8 count);
void *(*USBRxMemcpy)(void * dest, const void * source, UINT8 count);

/* USBMemcpyV: . 
** Note: this functin works only with data in the area <64k (small memory model) */
void * USBMemcpyV(void * dest, const void * source, UINT8 count)
{
  UINT8 i;
  for (i = 0; i < count; i++)
     ((UINT8 *)dest)[i] = ((UINT8 *)source)[i];
  return dest;
} /* end of USBMemcpyV */


/* USBInitMemcpy: .*/
void USBInitMemcpy(void)
{
  USBTxMemcpy = USBMemcpyV;
  USBRxMemcpy = USBMemcpyV;
  switch (USB_DMA_TX) {
     case 0:
        DMAInit(USB_DMA_TX);
        USBTxMemcpy = DMAMemcpy0;
        break;
    case 1:
        DMAInit(USB_DMA_TX);
        USBTxMemcpy = DMAMemcpy1;
        break;
    case 2:
        DMAInit(USB_DMA_TX);
       USBTxMemcpy = DMAMemcpy2;
       break;
  }
  switch (USB_DMA_RX) {
     case 0:
        DMAInit(USB_DMA_TX);
        USBRxMemcpy = DMAMemcpy0;
        break;
    case 1:
        DMAInit(USB_DMA_TX);
        USBRxMemcpy = DMAMemcpy1;
        break;
    case 2:
        DMAInit(USB_DMA_TX);
       USBRxMemcpy = DMAMemcpy2;
       break;
  }
} /* end of USBInitMemcpy */


/* OSInitUSB: . */
BOOL OSInitUSB(void)
{
  volatile unsigned int i;
  DEFAULT_DESCRIPTOR *descriptor;
  OldVCORE = GetVCore();
  
  if ((descriptor = (DEFAULT_DESCRIPTOR *)OSAlloca(sizeof(DEFAULT_DESCRIPTOR))) == NULL)
     return FALSE;
  descriptor->InterruptHandler = USBPWRVBUSonHandler;
  OSSetIODescriptor(OS_IO_USB_PWR_VBUSOn,descriptor);
  
  if ((descriptor = (DEFAULT_DESCRIPTOR *)OSAlloca(sizeof(DEFAULT_DESCRIPTOR))) == NULL)
     return FALSE;
  descriptor->InterruptHandler = USBPWRVBUSoffHandler;
  OSSetIODescriptor(OS_IO_USB_PWR_VBUSOff,descriptor);
  
  if ((descriptor = (DEFAULT_DESCRIPTOR *)OSAlloca(sizeof(EP0_DESCRIPTOR))) == NULL)
     return FALSE;
  descriptor->InterruptHandler = USBIEP0InterruptHandler;
  OSSetIODescriptor(OS_IO_USB_INPUT_ENDPOINT0,descriptor);
  
  if ((descriptor = (DEFAULT_DESCRIPTOR *)OSAlloca(sizeof(EP0_DESCRIPTOR))) == NULL)
     return FALSE;
  descriptor->InterruptHandler = USBOEP0InterruptHandler;
  OSSetIODescriptor(OS_IO_USB_OUTPUT_ENDPOINT0,descriptor);
  
  if ((descriptor = (DEFAULT_DESCRIPTOR *)OSAlloca(sizeof(DEFAULT_DESCRIPTOR))) == NULL)
     return FALSE;
  descriptor->InterruptHandler = USBSetupPacketInterruptHandler;
  OSSetIODescriptor(OS_IO_USB_SETUP_PACKET_RECEIVED,descriptor);
  
  if ((descriptor = (DEFAULT_DESCRIPTOR *)OSAlloca(sizeof(DEFAULT_DESCRIPTOR))) == NULL)
     return FALSE;
  descriptor->InterruptHandler = (void (*)(void *))USBReset;
  OSSetIODescriptor(OS_IO_USB_RSTR,descriptor);
  
  if ((descriptor = (DEFAULT_DESCRIPTOR *)OSAlloca(sizeof(DEFAULT_DESCRIPTOR))) == NULL)
     return FALSE;
  descriptor->InterruptHandler = (void (*)(void *))USBSuspend;
  OSSetIODescriptor(OS_IO_USB_SUSR,descriptor);
  
  if ((descriptor = (DEFAULT_DESCRIPTOR *)OSAlloca(sizeof(DEFAULT_DESCRIPTOR))) == NULL)
     return FALSE;
  descriptor->InterruptHandler = (void (*)(void *))USBResume;
  OSSetIODescriptor(OS_IO_USB_RESR,descriptor);
  
  if ((descriptor = (DEFAULT_DESCRIPTOR *)OSAlloca(sizeof(DEFAULT_DESCRIPTOR))) == NULL)
     return FALSE;
  descriptor->InterruptHandler = USBDoNothingHandler;
  OSSetIODescriptor(OS_IO_USB_NO_INTERRUPT,descriptor);
  __disable_interrupt(); // Disable global interrupts
  // configuration of USB module
  USBKEYPID = 0x9628; // set KEY and PID to 0x9628 -> access to configuration registers enabled
  USBPHYCTL = PUSEL;  // use DP and DM as USB terminals
  USBPWRCTL = VUSBEN + SLDOEN + SLDOAON; // enable primary and secondary LDO (3.3 and 1.8 V)
  for (i = 0; i < 1000; i++); // wait some time for LDOs
  USBPWRCTL = VUSBEN + SLDOEN + SLDOAON + VBONIE; // enable interrupt VBUSon
  USBKEYPID = 0x9600; // access to configuration registers disabled
  //init Serial Number
  #if (USB_STR_INDEX_SERNUM != 0)
     USBInitSerialStringDescriptor();
  #endif
  // init memcpy() function: DMA or non-DMA
  USBInitMemcpy();
  __enable_interrupt();                // enable global interrupts
  // See if we're already attached physically to USB, and if so, connect to it
  // Normally applications don't invoke the event handlers, but this is an exception.  
  if (USBConnectionInfo() & USB_VBUS_PRESENT && USBEnable() == USB_SUCCEED) {
     USBReset();
     USBConnect();  // generate rising edge on DP -> the host enumerates our device as full speed device
  }
  return TRUE;
} /* end of OSInitUSB */


/* OSInitOutputUSB: . */
BOOL OSInitOutputUSB(void (*userHandler)(UINT8 *, UINT8), UINT8 maxNodesFifo, UINT8 maxNodeSizeFifo, UINT8 portIndex)
{
  ENDPOINT_DESCRIPTOR *descriptor;
  #ifdef _HID_
     if ((descriptor = USBHidInitOutput(userHandler)) == NULL)
        return FALSE;
  #endif  // _HID_
  #ifdef _CDC_
     if ((descriptor = USBCdcInitOutput(userHandler,maxNodesFifo,maxNodeSizeFifo)) == NULL)
        return FALSE;
  #endif  // _HID_
  switch (portIndex) {
     case OS_IO_USB_OUTPUT_ENDPOINT1:
        descriptor->USBCounter[0] = &OutputEndPointDescriptorBlock[0].EPBCTX;
        descriptor->USBCounter[1] = &OutputEndPointDescriptorBlock[0].EPBCTY;
        descriptor->USBBuffer[0] = (UINT8 *)OEP1_X_BUFFER_ADDRESS;
        descriptor->USBBuffer[1] = (UINT8 *)OEP1_Y_BUFFER_ADDRESS;
        descriptor->EndPointInterruptBit = BIT1;
        break;
     case OS_IO_USB_OUTPUT_ENDPOINT2:
        descriptor->USBCounter[0] = &OutputEndPointDescriptorBlock[1].EPBCTX;
        descriptor->USBCounter[1] = &OutputEndPointDescriptorBlock[1].EPBCTY;
        descriptor->USBBuffer[0] = (UINT8 *)OEP2_X_BUFFER_ADDRESS;
        descriptor->USBBuffer[1] = (UINT8 *)OEP2_Y_BUFFER_ADDRESS;
        descriptor->EndPointInterruptBit = BIT2;
        break;
     case OS_IO_USB_OUTPUT_ENDPOINT3:
        descriptor->USBCounter[0] = &OutputEndPointDescriptorBlock[2].EPBCTX;
        descriptor->USBCounter[1] = &OutputEndPointDescriptorBlock[2].EPBCTY;
        descriptor->USBBuffer[0] = (UINT8 *)OEP3_X_BUFFER_ADDRESS;
        descriptor->USBBuffer[1] = (UINT8 *)OEP3_Y_BUFFER_ADDRESS;
        descriptor->EndPointInterruptBit = BIT3;
        break;
     case OS_IO_USB_OUTPUT_ENDPOINT4:
        descriptor->USBCounter[0] = &OutputEndPointDescriptorBlock[3].EPBCTX;
        descriptor->USBCounter[1] = &OutputEndPointDescriptorBlock[3].EPBCTY;
        descriptor->USBBuffer[0] = (UINT8 *)OEP4_X_BUFFER_ADDRESS;
        descriptor->USBBuffer[1] = (UINT8 *)OEP4_Y_BUFFER_ADDRESS;
        descriptor->EndPointInterruptBit = BIT4;
        break;
     case OS_IO_USB_OUTPUT_ENDPOINT5:
        descriptor->USBCounter[0] = &OutputEndPointDescriptorBlock[4].EPBCTX;
        descriptor->USBCounter[1] = &OutputEndPointDescriptorBlock[4].EPBCTY;
        descriptor->USBBuffer[0] = (UINT8 *)OEP5_X_BUFFER_ADDRESS;
        descriptor->USBBuffer[1] = (UINT8 *)OEP5_Y_BUFFER_ADDRESS;
        descriptor->EndPointInterruptBit = BIT5;
        break;
     case OS_IO_USB_OUTPUT_ENDPOINT6:
        descriptor->USBCounter[0] = &OutputEndPointDescriptorBlock[5].EPBCTX;
        descriptor->USBCounter[1] = &OutputEndPointDescriptorBlock[5].EPBCTY;
        descriptor->USBBuffer[0] = (UINT8 *)OEP6_X_BUFFER_ADDRESS;
        descriptor->USBBuffer[1] = (UINT8 *)OEP6_Y_BUFFER_ADDRESS;
        descriptor->EndPointInterruptBit = BIT6;
        break;
     case OS_IO_USB_OUTPUT_ENDPOINT7:
        descriptor->USBCounter[0] = &OutputEndPointDescriptorBlock[6].EPBCTX;
        descriptor->USBCounter[1] = &OutputEndPointDescriptorBlock[6].EPBCTY;
        descriptor->USBBuffer[0] = (UINT8 *)OEP7_X_BUFFER_ADDRESS;
        descriptor->USBBuffer[1] = (UINT8 *)OEP7_Y_BUFFER_ADDRESS;
        descriptor->EndPointInterruptBit = BIT7;
     default:
        break;
  }
  descriptor->NextReadyUSBBufferIndex = 0;
  OSSetIODescriptor(portIndex,descriptor);
  return TRUE;
} /* end of OSInitOutputUSB */


/* OSReleaseOutputNodeUSB: . */
void OSReleaseOutputNodeUSB(void * node, UINT8 portIndex)
{
  UINT8 freeNodes;
  CDC_OUTPUT_ENDPOINT_DESCRIPTOR *descriptor = (CDC_OUTPUT_ENDPOINT_DESCRIPTOR *)OSGetIODescriptor(portIndex);
  OSReleaseNodeFIFO(descriptor->FifoArray,node);
  do {
     freeNodes = OSUINT8_LL(&descriptor->FreeNodes);
  } while (!OSUINT8_SC(&descriptor->FreeNodes,freeNodes + 1));
  if (freeNodes == 0)
     USBOEPIE |= descriptor->EndPointInterruptBit;
} /* end of OSReleaseOutputNodeUSB */


/* OSEnqueueOutputNodeUSB: . */
void OSEnqueueOutputNodeUSB(UINT8 *node, UINT8 nodeSize, UINT8 portIndex)
{
  CDC_OUTPUT_ENDPOINT_DESCRIPTOR *descriptor = (CDC_OUTPUT_ENDPOINT_DESCRIPTOR *)OSGetIODescriptor(portIndex);
  OSEnqueueFIFO(descriptor->FifoArray,node,nodeSize);
} /* end of OSEnqueueOutputNodeUSB */


/* OSDequeueOutputNodeUSB: . */
void *OSDequeueOutputNodeUSB(UINT16 *nodeSize, UINT8 portIndex)
{
  CDC_OUTPUT_ENDPOINT_DESCRIPTOR *descriptor = (CDC_OUTPUT_ENDPOINT_DESCRIPTOR *)OSGetIODescriptor(portIndex);
  return OSDequeueFIFO(descriptor->FifoArray,nodeSize);
} /* end of OSEnqueueUART */


/* OSInitOutputUSB: . */
BOOL OSInitInputUSB(UINT8 maxNodesFifo, UINT8 maxNodeSizeFifo,UINT8 portIndex)
{
  ENDPOINT_DESCRIPTOR * descriptor;
  #ifdef _HID_
     if ((descriptor = USBHidInitInput(maxNodesFifo,maxNodeSizeFifo)) == NULL)
        return FALSE;
  #endif  // _HID_
  #ifdef _CDC_
     if ((descriptor = USBCdcInitInput(maxNodesFifo,maxNodeSizeFifo)) == NULL)
        return FALSE;
  #endif  // _HID_
  switch (portIndex) {
     case OS_IO_USB_INPUT_ENDPOINT1:
        descriptor->USBCounter[0] = &InputEndPointDescriptorBlock[0].EPBCTX;
        descriptor->USBCounter[1] = &InputEndPointDescriptorBlock[0].EPBCTY;
        descriptor->USBBuffer[0] = (UINT8 *)IEP1_X_BUFFER_ADDRESS;
        descriptor->USBBuffer[1] = (UINT8 *)IEP1_Y_BUFFER_ADDRESS;
        descriptor->EndPointInterruptBit = BIT1;
        break;
     case OS_IO_USB_INPUT_ENDPOINT2:
        descriptor->USBCounter[0] = &InputEndPointDescriptorBlock[1].EPBCTX;
        descriptor->USBCounter[1] = &InputEndPointDescriptorBlock[1].EPBCTY;
        descriptor->USBBuffer[0] = (UINT8 *)IEP2_X_BUFFER_ADDRESS;
        descriptor->USBBuffer[1] = (UINT8 *)IEP2_Y_BUFFER_ADDRESS;
        descriptor->EndPointInterruptBit = BIT2;
        break;
     case OS_IO_USB_INPUT_ENDPOINT3:
        descriptor->USBCounter[0] = &InputEndPointDescriptorBlock[2].EPBCTX;
        descriptor->USBCounter[1] = &InputEndPointDescriptorBlock[2].EPBCTY;
        descriptor->USBBuffer[0] = (UINT8 *)IEP3_X_BUFFER_ADDRESS;
        descriptor->USBBuffer[1] = (UINT8 *)IEP3_Y_BUFFER_ADDRESS;
        descriptor->EndPointInterruptBit = BIT3;
        break;
     case OS_IO_USB_INPUT_ENDPOINT4:
        descriptor->USBCounter[0] = &InputEndPointDescriptorBlock[3].EPBCTX;
        descriptor->USBCounter[1] = &InputEndPointDescriptorBlock[3].EPBCTY;
        descriptor->USBBuffer[0] = (UINT8 *)IEP4_X_BUFFER_ADDRESS;
        descriptor->USBBuffer[1] = (UINT8 *)IEP4_Y_BUFFER_ADDRESS;
        descriptor->EndPointInterruptBit = BIT4;
        break;
     case OS_IO_USB_INPUT_ENDPOINT5:
        descriptor->USBCounter[0] = &InputEndPointDescriptorBlock[4].EPBCTX;
        descriptor->USBCounter[1] = &InputEndPointDescriptorBlock[4].EPBCTY;
        descriptor->USBBuffer[0] = (UINT8 *)IEP5_X_BUFFER_ADDRESS;
        descriptor->USBBuffer[1] = (UINT8 *)IEP5_Y_BUFFER_ADDRESS;
        descriptor->EndPointInterruptBit = BIT5;
        break;
     case OS_IO_USB_INPUT_ENDPOINT6:
        descriptor->USBCounter[0] = &InputEndPointDescriptorBlock[5].EPBCTX;
        descriptor->USBCounter[1] = &InputEndPointDescriptorBlock[5].EPBCTY;
        descriptor->USBBuffer[0] = (UINT8 *)IEP6_X_BUFFER_ADDRESS;
        descriptor->USBBuffer[1] = (UINT8 *)IEP6_Y_BUFFER_ADDRESS;
        descriptor->EndPointInterruptBit = BIT6;
        break;
     case OS_IO_USB_INPUT_ENDPOINT7:
        descriptor->USBCounter[0] = &InputEndPointDescriptorBlock[6].EPBCTX;
        descriptor->USBCounter[1] = &InputEndPointDescriptorBlock[6].EPBCTY;
        descriptor->USBBuffer[0] = (UINT8 *)IEP7_X_BUFFER_ADDRESS;
        descriptor->USBBuffer[1] = (UINT8 *)IEP7_Y_BUFFER_ADDRESS;
        descriptor->EndPointInterruptBit = BIT7;
     default:
        break;
  }
  descriptor->NextReadyUSBBufferIndex = 0;
  OSSetIODescriptor(portIndex,descriptor);
  return TRUE;
  
} /* end of OSInitInputUSB */

/* OSGetFreeNodeInputNodeUSB: . */
void *OSGetFreeNodeInputNodeUSB(UINT8 portIndex)
{
  INPUT_ENDPOINT_DESCRIPTOR *descriptor = (INPUT_ENDPOINT_DESCRIPTOR *)OSGetIODescriptor(portIndex);
  return OSGetFreeNodeFIFO(descriptor->FifoArray);
} /* end of OSGetFreeNodeInputNodeUSB */

/* OSReleaseInputNodeUSB: Releases a buffer that was obtained from USBGetFreeNode and returns
** it to the pool of available buffers. */
void OSReleaseInputNodeUSB(void *node, UINT8 portIndex)
{
  INPUT_ENDPOINT_DESCRIPTOR *descriptor = (INPUT_ENDPOINT_DESCRIPTOR *)OSGetIODescriptor(portIndex);
  OSReleaseNodeFIFO(descriptor->FifoArray,node);
} /* end of OSReleaseInputNodeUSB */

  
/* OSEnqueueInputUSB: Called by an application task to output a string of bytes contained in
** a buffer that was provided by USBGetFreeNode. */
void OSEnqueueInputUSB(void * node, UINT8 nodeSize, UINT8 portIndex)
{
  UINT8 usedNodes;
  INPUT_ENDPOINT_DESCRIPTOR *descriptor = (INPUT_ENDPOINT_DESCRIPTOR *)OSGetIODescriptor(portIndex);
  OSEnqueueFIFO(descriptor->FifoArray,node,nodeSize);
  /* Enable transmit buffer empty interrupts from the USB manager once it finishes the previous
  ** send operation. Note that this can be immediate if the last transmission has already
  ** finished or later if the USB manager is busy sending a message. */
  do {
     usedNodes = OSUINT8_LL(&descriptor->UsedNodes);
  } while (!OSUINT8_SC(&descriptor->UsedNodes,usedNodes + 1));
  if (usedNodes == 0)
     USBIEPIE |= descriptor->EndPointInterruptBit;
} /* end of OSEnqueueInputUSB */


/* USBKernelSleep: . */
void USBKernelSleep(void)
{
  while (TRUE)
     switch(OSConnectionStateUSB()) {
        case ST_ENUM_ACTIVE:
           /* set bits CPUOFF into the SR register to enter in LPM0 sleep mode */
           __asm("\tbis.w #0x10,sr");  
           break;
        case ST_ENUM_SUSPENDED:
        case ST_USB_DISCONNECTED:
        case ST_NOENUM_SUSPENDED:
           /* set bits OSCOFF, SCG0 and SCG1 into the SR register to enter in LPM3 sleep mode */
           __asm("\tbis.w #0xD0,sr");  
           break;
        case ST_ENUM_IN_PROGRESS:
        case ST_USB_CONNECTED_NO_ENUM:
        case ST_ERROR:
        default:
           break;
     }
} /* end of USBKernelSleep */


// This function will be compiled only if
#if (USB_STR_INDEX_SERNUM != 0)
   #define TLV_DIERECORD_SIZE 10
   void USBInitSerialStringDescriptor(void)
   {
     extern UINT8 TLVDieRecord[TLV_DIERECORD_SIZE];
     UINT8 i, j, hexValue;
     j = 1; // we start with second UINT8, first UINT8 (lenght) will be filled later
     SerialStringDescriptor[j++] = DESC_TYPE_STRING;
     for (i = 0; i < TLV_DIERECORD_SIZE && i < 8; i++) {
        hexValue = (TLVDieRecord[i] & 0xF0)>> 4;
        if (hexValue < 10 ) 
           SerialStringDescriptor[j++] = (hexValue + '0');
        else 
           SerialStringDescriptor[j++] = (hexValue + 55);
        SerialStringDescriptor[j++] = 0x00;  // needed for UNI-Code
        hexValue = (TLVDieRecord[i] & 0x0F);
        if (hexValue < 10 )
           SerialStringDescriptor[j++] = (hexValue + '0');
        else 
           SerialStringDescriptor[j++] = (hexValue + 55);
        SerialStringDescriptor[j++] = 0x00;  // needed for UNI-Code
     }
     SerialStringDescriptor[0] = i*4 +2;        // calculate the length
   }
#endif


UINT8 USBEnable()
{
  volatile unsigned int i;
  volatile unsigned int j = 0;
  if (!(USBPWRCTL & USBBGVBV)) // check USB Bandgap and VBUS valid
     return USB_GENERAL_ERROR;
  if ((USBCNF & USB_EN) && (USBPLLCTL & UPLLEN))
     return USB_SUCCEED;       // exit if PLL is already enalbed
  USBKEYPID = 0x9628;          // set KEY and PID to 0x9628 -> access to configuration registers enabled
  /* Start XT2 */
  P5DIR &= ~0x0C;              // Set output direction for XT1 Pins
  P5SEL |= 0x0C;               // Set output direction for XT1 Pins
  UCSCTL6 &= ~XT2OFF;          // enalbe XT2 even if not used
  while (SFRIFG1 & OFIFG) {    // check OFIFG fault flag
     UCSCTL7 &= ~(DCOFFG+XT1LFOFFG+XT2OFFG); // Clear OSC flaut Flags fault flags
     SFRIFG1 &= ~OFIFG;        // Clear OFIFG fault flag
  }
  USBPLLDIVB = USB_XT_FREQ;    // Settings desired frequency
  if (USB_PLL_XT == 2)
     USBPLLCTL = UPCS0 + UPFDEN + UPLLEN;// Select XT2 as Ref / Select PLL for USB / Discrim. on, enable PLL
  else
     USBPLLCTL = UPFDEN + UPLLEN; // Select XT1 as Ref / Select PLL for USB / Discrim. on, enable PLL
  /* Wait some time till PLL is settled */
  do {
     USBPLLIR    =     0x0000; // make sure no interrupts can occur on PLL-module
     /* wait 1/2 ms till enable USB */
     for (i = 0;i < USB_MCLK_FREQ / 1000 * 1 / 2 / 10;i++);
     if (j++ > 10) {
        USBKEYPID   =    0x9600; // access to configuration registers disabled
        return USB_GENERAL_ERROR;
     }
  } while (USBPLLIR != 0);
  USBCNF     |=    USB_EN; // enable USB module
  USBKEYPID   =    0x9600; // access to configuration registers disabled
  return USB_SUCCEED;
}


/* Disables the USB module and PLL. */
UINT8 USBDisable(void)
{
  USBKEYPID = 0x9628;        // set KEY and PID to 0x9628 -> access to configuration registers enabled
  USBCNF    = 0;             // disable USB module
  USBPLLCTL &= ~UPLLEN;      // disable PLL
  USBKEYPID = 0x9600;        // access to configuration registers disabled
  EnumerationStatus = 0x00;  // device is not enumerated
  FunctionSuspended = FALSE; // device is not suspended
  return USB_SUCCEED;
}


/* Reset USB-SIE and global variables. */
UINT8 USBReset(void)
{
  EP0_DESCRIPTOR *descriptor;
  USBKEYPID = 0x9628;            // set KEY and PID to 0x9628 -> access to configuration registers enabled
  //reset should be on the bus after this!
  EnumerationStatus = 0x00;      // Device not enumerated yet
  FunctionSuspended = FALSE;     // Device is not in suspend mode
  RemoteWakeup = DISABLE;
  ConfigurationNumber = 0x00;    // device unconfigured
  InterfaceNumber = 0x00;
  // FRSTE Workaround:
  // Clear FRSTE in the RESRIFG interrupt service routine before re-configuring USB control registers.
  // Set FRSTE at the beginning of SUSRIFG, SETUP, IEPIFG.EP0 and OEPIFG.EP0 interrupt service routines. 
  USBCTL = 0;                           // Function Reset Connection disable (FRSTE)
  descriptor = (EP0_DESCRIPTOR *)OSGetIODescriptor(OS_IO_USB_INPUT_ENDPOINT0);
  descriptor->BytesRemaining = NO_MORE_DATA;
  descriptor = (EP0_DESCRIPTOR *)OSGetIODescriptor(OS_IO_USB_OUTPUT_ENDPOINT0);
  descriptor->BytesRemaining = NO_MORE_DATA;
  StatusAction = STATUS_ACTION_NOTHING;
  //The address reset normally will be done automatically during bus function reset
  USBFUNADR = 0x00;               // reset address of USB device (unconfigured)
  /* Set settings for EP0 */
  // NAK both 0 endpoints and enable endpoint 0 interrupt
  EndPoint0DescriptorBlock.IEPBCNT = EPBCNT_NAK;
  EndPoint0DescriptorBlock.OEPBCNT = EPBCNT_NAK;
  EndPoint0DescriptorBlock.IEPCNFG = EPCNF_USBIE | EPCNF_UBME | EPCNF_STALL;    // 8 UINT8 data packet
  EndPoint0DescriptorBlock.OEPCNFG = EPCNF_USBIE | EPCNF_UBME | EPCNF_STALL;    // 8 UINT8 data packet
  USBOEPIE = BIT0 | BIT1 | BIT3;                // enable EP0, and EP3 output IRQ
  //USBIEPIE = BIT0 | BIT1 | BIT2 | BIT3;         // enable EP0, EP1, EP2 and EP3 input IRQ
  USBIEPIFG = BIT1 | BIT3;
  USBIEPIE = BIT0 | BIT2;
  #ifdef _HID_
     /* Set settings for IEP1 */
     // enable endpoint 1 interrupt, input
     InputEndPointDescriptorBlock[EDB(HID_INEP_ADDR)].EPCNF   = EPCNF_USBIE | EPCNF_UBME | EPCNF_DBUF; //double buffering
     InputEndPointDescriptorBlock[EDB(HID_INEP_ADDR)].EPBBAX  = (UINT8)(((IEP1_X_BUFFER_ADDRESS - START_OF_USB_BUFFER) >> 3) & 0x00ff);
     InputEndPointDescriptorBlock[EDB(HID_INEP_ADDR)].EPBBAY  = (UINT8)(((IEP1_Y_BUFFER_ADDRESS - START_OF_USB_BUFFER) >> 3) & 0x00ff);
     InputEndPointDescriptorBlock[EDB(HID_INEP_ADDR)].EPBCTX  = EPBCNT_NAK;
     InputEndPointDescriptorBlock[EDB(HID_INEP_ADDR)].EPBCTY  = EPBCNT_NAK;
     InputEndPointDescriptorBlock[EDB(HID_INEP_ADDR)].EPSIZXY = MAX_PACKET_SIZE;
     /* Set settings for OEP1 */
     // enable endpoint 1 interrupt, output
     OutputEndPointDescriptorBlock[EDB(HID_OUTEP_ADDR)].EPCNF   = EPCNF_USBIE | EPCNF_UBME | EPCNF_DBUF ; //double buffering
     OutputEndPointDescriptorBlock[EDB(HID_OUTEP_ADDR)].EPBBAX  = (UINT8)(((OEP1_X_BUFFER_ADDRESS - START_OF_USB_BUFFER) >> 3) & 0x00ff);
     OutputEndPointDescriptorBlock[EDB(HID_OUTEP_ADDR)].EPBBAY  = (UINT8)(((OEP1_Y_BUFFER_ADDRESS - START_OF_USB_BUFFER) >> 3) & 0x00ff);
     OutputEndPointDescriptorBlock[EDB(HID_OUTEP_ADDR)].EPBCTX  = 0x00;
     OutputEndPointDescriptorBlock[EDB(HID_OUTEP_ADDR)].EPBCTY  = 0x00; // EPBCT_NAK for one buffer (no double)
     OutputEndPointDescriptorBlock[EDB(HID_OUTEP_ADDR)].EPSIZXY = MAX_PACKET_SIZE;
  #endif // _HID_
  #ifdef _CDC_
    /* Set settings for IEP2 */
    // enable endpoint 2 interrupt, input
    InputEndPointDescriptorBlock[EDB(CDC_INTEP_ADDR)].EPCNF   = EPCNF_USBIE |   // use Interrupt for this EP
                                                                EPCNF_UBME |    // enable this EP
                                                                EPCNF_DBUF;     // use double buffering (X and Y buffers)
    InputEndPointDescriptorBlock[EDB(CDC_INTEP_ADDR)].EPBBAX  = (UINT8)(((IEP2_X_BUFFER_ADDRESS - START_OF_USB_BUFFER) >> 3) & 0x00ff);
    InputEndPointDescriptorBlock[EDB(CDC_INTEP_ADDR)].EPBBAY  = (UINT8)(((IEP2_Y_BUFFER_ADDRESS - START_OF_USB_BUFFER) >> 3) & 0x00ff);
    InputEndPointDescriptorBlock[EDB(CDC_INTEP_ADDR)].EPBCTX  = EPBCNT_NAK;       // NAK: the buffer has no valid data to send to host
    InputEndPointDescriptorBlock[EDB(CDC_INTEP_ADDR)].EPBCTY  = EPBCNT_NAK;       // NAK: the buffer has no valid data to send to host
    InputEndPointDescriptorBlock[EDB(CDC_INTEP_ADDR)].EPSIZXY = MAX_PACKET_SIZE;
    /* Set settings for IEP3 */
    // enable endpoint 3 bulk, input
    InputEndPointDescriptorBlock[EDB(CDC_INEP_ADDR)].EPCNF   = EPCNF_USBIE |   // use Interrupt for this EP
                                                               EPCNF_UBME |    // enable this EP
                                                               EPCNF_DBUF;     // use double buffering (X and Y buffers)
    InputEndPointDescriptorBlock[EDB(CDC_INEP_ADDR)].EPBBAX  = (UINT8)(((IEP3_X_BUFFER_ADDRESS - START_OF_USB_BUFFER) >> 3) & 0x00ff);
    InputEndPointDescriptorBlock[EDB(CDC_INEP_ADDR)].EPBBAY  = (UINT8)(((IEP3_Y_BUFFER_ADDRESS - START_OF_USB_BUFFER) >> 3) & 0x00ff);
    InputEndPointDescriptorBlock[EDB(CDC_INEP_ADDR)].EPBCTX  = EPBCNT_NAK;       // NAK: the buffer has no valid data to send to host
    InputEndPointDescriptorBlock[EDB(CDC_INEP_ADDR)].EPBCTY  = EPBCNT_NAK;       // NAK: the buffer has no valid data to send to host
    InputEndPointDescriptorBlock[EDB(CDC_INEP_ADDR)].EPSIZXY = MAX_PACKET_SIZE;
    /* Set settings for OEP3 */
    // enable endpoint 3 bulk, output
    OutputEndPointDescriptorBlock[EDB(CDC_OUTEP_ADDR)].EPCNF   = EPCNF_USBIE |  // use Interrupt for this EP
                                                                 EPCNF_UBME |   // enable this EP
                                                                 EPCNF_DBUF;    // use double buffering (X and Y buffers)
    OutputEndPointDescriptorBlock[EDB(CDC_OUTEP_ADDR)].EPBBAX  = (UINT8)(((OEP3_X_BUFFER_ADDRESS - START_OF_USB_BUFFER) >> 3) & 0x00ff);
    OutputEndPointDescriptorBlock[EDB(CDC_OUTEP_ADDR)].EPBBAY  = (UINT8)(((OEP3_Y_BUFFER_ADDRESS - START_OF_USB_BUFFER) >> 3) & 0x00ff);
    OutputEndPointDescriptorBlock[EDB(CDC_OUTEP_ADDR)].EPBCTX  = 0x00;
    OutputEndPointDescriptorBlock[EDB(CDC_OUTEP_ADDR)].EPBCTY  = 0x00;
    OutputEndPointDescriptorBlock[EDB(CDC_OUTEP_ADDR)].EPSIZXY = MAX_PACKET_SIZE;
  #endif // _CDC_
  USBCTL |= FEN;                      // enable function
  USBIFG = 0;                         // make sure no interrupts are pending
  USBIE = SETUPIE | RSTRIE | SUSRIE;  // enable USB specific interrupts (setup, reset, suspend)
  USBKEYPID = 0x9600;                 // access to configuration registers disabled
  return USB_SUCCEED;
}


/* Instruct USB module to make itself available to the PC for connection, by pulling PUR high. */
UINT8 USBConnect(void)
{
  USBKEYPID = 0x9628;   // set KEY and PID to 0x9628 -> access to configuration registers enabled
  USBCNF |= PUR_EN;     // generate rising edge on DP -> the host enumerates our device as full speed device
  USBPWRCTL |= VBOFFIE; // enable interrupt VUSBoff
  USBKEYPID = 0x9600;   // access to configuration registers disabled
  // after this the enumeration may take place
  __no_operation();
  __no_operation();
  __no_operation();
  __no_operation();
  __no_operation();
  __no_operation();
  __no_operation();
  return USB_SUCCEED;
}


/* Force a disconnect from the PC by pulling PUR low. */
UINT8 USBDisconnect(void)
{
  USBKEYPID = 0x9628;     // set KEY and PID to 0x9628 -> access to configuration registers enabled
  USBCNF &= ~PUR_EN;      // disconnect pull up resistor - logical disconnect from HOST
  //USBCNF = 0;           // disable USB module
  USBPWRCTL &= ~VBOFFIE;  // disable interrupt VUSBoff
  USBKEYPID = 0x9600;     // access to configuration registers disabled
  EnumerationStatus = 0;  // not enumerated
  FunctionSuspended = FALSE; // device is not suspended
  return USB_SUCCEED;
}


/* Force a remote wakeup of the USB host. */
UINT8 USBForceRemoteWakeup(void)
{
  if (FunctionSuspended == FALSE) // device is not suspended
     return USB_NOT_SUSPENDED;
  if (RemoteWakeup == ENABLE) {
     USBCTL |= RWUP; // USB - Device Remote Wakeup Request - this bit is self-cleaned
     return USB_SUCCEED;
  }
  return USB_GENERAL_ERROR;
}


/* Returns the status of the USB connection. */
UINT8 USBConnectionInfo(void)
{
  UINT8 retVal = 0;
  if (USBPWRCTL & USBBGVBV)
     retVal |= USB_VBUS_PRESENT;
  if (EnumerationStatus == ENUMERATION_COMPLETE)
     retVal |= USB_ENUMERATED;
  //if () //sync frame are received
  //   kUSB_busActive;
  if (USBCNF & PUR_EN)
     retVal |= USB_PURHIGH;
  if (FunctionSuspended == TRUE)
     retVal |= USB_SUSPENDED;
  else
     retVal |= USB_NOT_SUSPENDED;
  return retVal;
}


/* Returns the state of the USB connection. */
UINT8 OSConnectionStateUSB(void)
{
  // If no VBUS present
  if (!(USBPWRCTL & USBBGVBV))
     return ST_USB_DISCONNECTED;
  // If VBUS present, but PUR is low
  if ((USBPWRCTL & USBBGVBV)&&(!(USBCNF & PUR_EN)))
     return ST_USB_CONNECTED_NO_ENUM;
  // If VBUS present, PUR is high, and enumeration is complete, and not suspended
  if ((USBPWRCTL & USBBGVBV) && (USBCNF & PUR_EN) && (EnumerationStatus == ENUMERATION_COMPLETE) &&
      (!(FunctionSuspended == TRUE))) 
     return ST_ENUM_ACTIVE;
  // If VBUS present, PUR is high, and enumeration is NOT complete, and  suspended
  if ((USBPWRCTL & USBBGVBV) && (USBCNF & PUR_EN) && (!(EnumerationStatus == ENUMERATION_COMPLETE)) &&
      (FunctionSuspended == TRUE))
     return ST_NOENUM_SUSPENDED;
  // If VBUS present, PUR is high, and enumeration is complete, and  suspended
  if ((USBPWRCTL & USBBGVBV) && (USBCNF & PUR_EN) && (EnumerationStatus == ENUMERATION_COMPLETE) &&
      (FunctionSuspended == TRUE))
     return ST_ENUM_SUSPENDED;
  // If VBUS present, PUR is high, but no enumeration yet
  if ((USBPWRCTL & USBBGVBV) && (USBCNF & PUR_EN) && (!(EnumerationStatus == ENUMERATION_COMPLETE)))
     return ST_ENUM_IN_PROGRESS;
  return ST_ERROR;
}


UINT8 USBSuspend(void)
{
  FunctionSuspended  = TRUE;
  USBKEYPID = 0x9628;         // set KEY and PID to 0x9628 -> access to configuration registers enabled
  USBCTL |= FRSTE;            // Function Reset Connection Enable
  USBIFG &= ~SUSRIFG;         // clear interrupt flag
  if (USB_DISABLE_XT_SUSPEND) {
     if (USB_PLL_XT == 2) {
        USBPLLCTL &= ~UPLLEN; // disable PLL
        UCSCTL6   |= XT2OFF;  // disable XT2
     }
     else {
        USBPLLCTL &= ~UPLLEN; // disable PLL
        UCSCTL6 |= XT1OFF;
     }
  }
  USBIE = RESRIE;             // disable USB specific interrupts (setup, suspend, reset),
                              // enable resume. If the reset occured during device in
                              // suspend, the resume-interrupt will come, after - reset
                              // interrupt.
  USBKEYPID = 0x9600;         // access to configuration registers disabled
  return USB_SUCCEED;
}


UINT8 USBResume(void)
{
  USBEnable();                        // enable PLL
  USBIFG &= ~(RESRIFG | SUSRIFG);     // clear interrupt flags
  USBIE = SETUPIE | RSTRIE | SUSRIE;  // enable USB specific interrupts (setup, reset, suspend)
  FunctionSuspended  = FALSE;
  return USB_SUCCEED;
}


void USBClearEndpointFeature(void)
{
  UINT8 endpointNumber;
  // EP is from EP1 to EP7 while C language start from 0
  endpointNumber = (SetupPacket.Index & EP_DESC_ADDR_EP_NUM);
  if (endpointNumber == 0x00)
     USBSendZeroLengthPacketOnIEP0();
  else {
     endpointNumber--;
     if (endpointNumber < MAX_ENDPOINT_NUMBER) {
        if ((SetupPacket.Index & EP_DESC_ADDR_DIR_IN) == EP_DESC_ADDR_DIR_IN) // input endpoint
           InputEndPointDescriptorBlock[endpointNumber].EPCNF &= ~(EPCNF_STALL | EPCNF_TOGGLE);
        else // output endpoint
           OutputEndPointDescriptorBlock[endpointNumber].EPCNF &= ~(EPCNF_STALL | EPCNF_TOGGLE);
        USBSendZeroLengthPacketOnIEP0();
     }
  }
}


void USBGetConfiguration(void)
{
  USBSendDataPacketOnEP0((UINT8 *)&ConfigurationNumber,1);
}


void USBGetDeviceDescriptor(void)
{
  USBSendDataPacketOnEP0((UINT8 *)&DeviceDescriptor,SIZEOF_DEVICE_DESCRIPTOR);
}


void USBGetConfigurationDescriptor(void)
{
  USBSendDataPacketOnEP0((UINT8 *)&ConfigurationDescriptorGroup,
                         sizeof(ConfigurationDescriptorGroup));
}


void USBGetStringDescriptor(void)
{
  UINT16 index;
  UINT8 val = (UINT8)SetupPacket.Value;
  #if (USB_STR_INDEX_SERNUM != 0)
     if (val == 0x03) {
        USBSendDataPacketOnEP0((UINT8 *)&SerialStringDescriptor,
                               SerialStringDescriptor[0]);
     }
     else {
  #endif
        index = 0x00;
        while (val-- >  0x00)
           index += StringDescriptor[index];
        USBSendDataPacketOnEP0((UINT8 *)&StringDescriptor[index],
                               StringDescriptor[index]);
  #if (USB_STR_INDEX_SERNUM != 0)
     }
  #endif
}


void USBGetInterface(void)
{
  /* not fully supported, return one UINT8, zero */
  USBRequestReturnData[0] = 0x00; // changed to report alternative setting UINT8
  USBRequestReturnData[1] = InterfaceNumber;
  USBSendDataPacketOnEP0((UINT8 *)&USBRequestReturnData[0],0x02);
}


void USBGetDeviceStatus(void)
{
  if ((ConfigurationDescriptorGroup[OFFSET_CONFIG_DESCRIPTOR_POWER] &
      CFG_DESC_ATTR_SELF_POWERED) == CFG_DESC_ATTR_SELF_POWERED)
     USBRequestReturnData[0] = DEVICE_STATUS_SELF_POWER;
  if (RemoteWakeup == ENABLE)
     USBRequestReturnData[0] |= DEVICE_STATUS_REMOTE_WAKEUP;
  /* Return self power status and remote wakeup status */
  USBSendDataPacketOnEP0((UINT8 *)&USBRequestReturnData[0],2);
}


void USBGetInterfaceStatus(void)
{
  // check bIndexL for index number (not supported)
  // Return two zero bytes
  USBRequestReturnData[0] = 0x00; // changed to support multiple interfaces
  USBRequestReturnData[1] = InterfaceNumber;
  USBSendDataPacketOnEP0((UINT8 *)&USBRequestReturnData[0],2);
}


void USBGetEndpointStatus(void)
{
  UINT8 EndpointNumber;
  // Endpoint number is bIndexL
  EndpointNumber = SetupPacket.Index & EP_DESC_ADDR_EP_NUM;
  if (EndpointNumber == 0x00) {
     if ((SetupPacket.Index & EP_DESC_ADDR_DIR_IN) == EP_DESC_ADDR_DIR_IN) // input endpoint 0
        USBRequestReturnData[0] = (UINT8)(EndPoint0DescriptorBlock.IEPCNFG & EPCNF_STALL);
     else // output endpoint 0
        USBRequestReturnData[0] = (UINT8)(EndPoint0DescriptorBlock.OEPCNFG & EPCNF_STALL);
     USBRequestReturnData[0] = USBRequestReturnData[0] >> 3; // STALL is on bit 3
     USBSendDataPacketOnEP0((UINT8 *)&USBRequestReturnData[0],0x02);
  }
  else {
     EndpointNumber--;
     // EP is from EP1 to EP7 while C language start from 0
     // Firmware should NOT response if specified endpoint is not supported. (charpter 8)
     if (EndpointNumber < MAX_ENDPOINT_NUMBER) {
        if (SetupPacket.Index & EP_DESC_ADDR_DIR_IN) // input endpoint
           USBRequestReturnData[0] = (UINT8)(InputEndPointDescriptorBlock[EndpointNumber].EPCNF & EPCNF_STALL);
        else // output endpoint
           USBRequestReturnData[0] = (UINT8)(OutputEndPointDescriptorBlock[EndpointNumber].EPCNF & EPCNF_STALL);
     }   // no response if endpoint is not supported.
     USBRequestReturnData[0] = USBRequestReturnData[0] >> 3; // STALL is on bit 3
     USBSendDataPacketOnEP0((UINT8 *)&USBRequestReturnData[0],0x02);
  }
}


void USBSetAddress(void)
{
  USBStallOEP0(); // control write without data stage
  // bValueL contains device address
  if (SetupPacket.Value < 128) {
     // hardware will update the address after status stage
     // therefore, firmware can set the address now.
     USBFUNADR = SetupPacket.Value;
     USBSendZeroLengthPacketOnIEP0();
  } else
     USBStallEndpoint0();
}


void USBSetConfiguration(void)
{
  USBStallOEP0(); // control write without data stage
  // configuration number is in bValueL
  // change the code if more than one configuration is supported
  ConfigurationNumber = SetupPacket.Value;
  USBSendZeroLengthPacketOnIEP0();
  if (ConfigurationNumber == 1)
      EnumerationStatus = ENUMERATION_COMPLETE; // set device as enumerated
  else
      EnumerationStatus = 0; //device is not configured == config # is zero
}


void USBClearDeviceFeature(void)
{
  // bValueL contains feature selector
  if (SetupPacket.Value == FEATURE_REMOTE_WAKEUP){
     RemoteWakeup = DISABLE;
     USBSendZeroLengthPacketOnIEP0();
  }
  else
     USBStallEndpoint0();
}


void USBSetDeviceFeature(void)
{
  // bValueL contains feature selector
  if (SetupPacket.Value == FEATURE_REMOTE_WAKEUP) {
     RemoteWakeup = ENABLE;
     USBSendZeroLengthPacketOnIEP0();
  }
  else
     USBStallEndpoint0();
}


void USBSetEndpointFeature(void)
{
  UINT8 EndpointNumber;
  // value contains feature selector
  // bIndexL contains endpoint number
  // Endpoint number is in low byte of wIndex
  if (SetupPacket.Value == FEATURE_ENDPOINT_STALL) {
     EndpointNumber = SetupPacket.Index & EP_DESC_ADDR_EP_NUM;
     if (EndpointNumber == 0x00)
        USBSendZeroLengthPacketOnIEP0();  // do nothing for endpoint 0
     else {
        EndpointNumber--;
        // Firmware should NOT response if specified endpoint is not supported. (charpter 8)
        if (EndpointNumber < MAX_ENDPOINT_NUMBER) {
           if (SetupPacket.Index & EP_DESC_ADDR_DIR_IN) // input endpoint
              InputEndPointDescriptorBlock[EndpointNumber].EPCNF |= EPCNF_STALL;
           else // output endpoint
              OutputEndPointDescriptorBlock[EndpointNumber].EPCNF |= EPCNF_STALL;
            USBSendZeroLengthPacketOnIEP0();
        } // no response if endpoint is not supported.
     }
  }
  else
     USBStallEndpoint0();
}


void USBSetInterface(void)
{
  // bValueL contains alternative setting
  // bIndexL contains interface number
  // change code if more than one interface is supported
  USBStallOEP0();                             // control write without data stage
  InterfaceNumber = SetupPacket.Index;
  USBSendZeroLengthPacketOnIEP0();
}


void USBInvalidRequest(void)
{
  // check if setup overwrite is set
  // if set, do nothing since we might decode it wrong
  // setup packet buffer could be modified by hardware if another setup packet
  // was sent while we are deocding setup packet
  if ((USBIFG & STPOWIFG) == 0x00)
     USBStallEndpoint0();
}


/* These functions is to be used ONLY by USB stack, and not by application */


/* Send stall handshake for in- and out-endpoint0 (control pipe) */
void USBStallEndpoint0(void)
{
  EndPoint0DescriptorBlock.IEPCNFG |= EPCNF_STALL;
  EndPoint0DescriptorBlock.OEPCNFG |= EPCNF_STALL;
}


/* Send stall handshake for out-endpoint0 (control pipe) */
void USBStallOEP0(void)
{
  // in standard USB request, there is not control write request with data stage
  // control write, stall output endpoint 0
  // wLength should be 0 in all cases
  EndPoint0DescriptorBlock.OEPCNFG |= EPCNF_STALL;
}


/* Send further data over control pipe if needed. Function is called from control-in IRQ.
** Do not call from user application. */
void USBSendNextPacketOnIEP0(void)
{
  UINT8 packetSize, index;
  EP0_DESCRIPTOR *descriptor = (EP0_DESCRIPTOR *)OSGetIODescriptor(OS_IO_USB_INPUT_ENDPOINT0);
  // First check if there are bytes remaining to be transferred
  if (descriptor->BytesRemaining != NO_MORE_DATA) {
     if (descriptor->BytesRemaining > EP0_PACKET_SIZE) {
        // More bytes are remaining than will fit in one packet
        // there will be More IN Stage
        packetSize = EP0_PACKET_SIZE;
        descriptor->BytesRemaining -= EP0_PACKET_SIZE;
        StatusAction = STATUS_ACTION_DATA_IN;
     } 
     else if (descriptor->BytesRemaining < EP0_PACKET_SIZE) {
        // The remaining data will fit in one packet.
        // This case will properly handle wBytesRemainingOnIEP0 == 0
        packetSize = (UINT8)descriptor->BytesRemaining;
        descriptor->BytesRemaining = NO_MORE_DATA;        // No more data need to be Txed
        StatusAction = STATUS_ACTION_NOTHING;
     } 
     else {
        packetSize = EP0_PACKET_SIZE;
        if (HostAskMoreDataThanAvailable == TRUE) {
           descriptor->BytesRemaining = 0;
           StatusAction = STATUS_ACTION_DATA_IN;
        } 
        else {
           descriptor->BytesRemaining = NO_MORE_DATA;
           StatusAction = STATUS_ACTION_NOTHING;
        }
     }
     for (index = 0; index < packetSize; index++) {
         IEP0Buffer[index] = *descriptor->NextByte;
         descriptor->NextByte++;
     }
     EndPoint0DescriptorBlock.IEPBCNT = packetSize;
  } 
  else 
     StatusAction = STATUS_ACTION_NOTHING;
}


/* Send data over control pipe to host. */
void USBSendDataPacketOnEP0(UINT8 * buffer, UINT16 size)
{
  UINT16 temp;
  EP0_DESCRIPTOR *descriptor = (EP0_DESCRIPTOR *)OSGetIODescriptor(OS_IO_USB_INPUT_ENDPOINT0);
  descriptor->NextByte = buffer;
  descriptor->BytesRemaining = size;
  EndPoint0DescriptorBlock.OEPBCNT = 0x00; // Clear endpoint 0 ByteCount
  temp = SetupPacket.Length;
  // Limit transfer size to wLength if needed
  // this prevent USB device sending 'more than require' data back to host
  if (descriptor->BytesRemaining >= temp) {
     descriptor->BytesRemaining = temp;
     HostAskMoreDataThanAvailable = FALSE;
  }
  else
     HostAskMoreDataThanAvailable = TRUE;
  USBSendNextPacketOnIEP0();
}


/* Receive further data from control pipe if needed.
** Function is called from control-out IRQ. Do not call from user application */
void USBReceiveNextPacketOnOEP0(void)
{
  UINT8 index, byte;
  EP0_DESCRIPTOR *descriptor = (EP0_DESCRIPTOR *)OSGetIODescriptor(OS_IO_USB_OUTPUT_ENDPOINT0);
  byte = EndPoint0DescriptorBlock.OEPBCNT & EPBCNT_UINT8CNT_MASK;
  if (descriptor->BytesRemaining >= (UINT16)byte) {
     for (index = 0; index < byte; index++) {
        *descriptor->NextByte = OEP0Buffer[index];
        descriptor->NextByte++;
     }
     descriptor->BytesRemaining -= (UINT16)byte;
     // clear the NAK bit for next packet
     if (descriptor->BytesRemaining > 0) {
        EndPoint0DescriptorBlock.OEPBCNT = 0x00; // Clear endpoint 0 ByteCount
        StatusAction = STATUS_ACTION_DATA_OUT;
     }
     else {
        USBStallOEP0();
        StatusAction = STATUS_ACTION_NOTHING;
     }
  }
  else {
     USBStallOEP0();
     StatusAction = STATUS_ACTION_NOTHING;
  }
}


/* Receive data from control pipe.
** Number of bytes to receive should be set with global varible "bytesRemainingOnOEP0" 
** before function is called. */
void USBReceiveDataPacketOnEP0(UINT8 *buffer)
{
  EP0_DESCRIPTOR *descriptor = (EP0_DESCRIPTOR *)OSGetIODescriptor(OS_IO_USB_OUTPUT_ENDPOINT0);
  descriptor->NextByte = buffer;
  descriptor->BytesRemaining = SetupPacket.Length;
  StatusAction = STATUS_ACTION_DATA_OUT;
  EndPoint0DescriptorBlock.OEPBCNT = 0x00; // Clear endpoint 0 ByteCount
}


/* Send zero length packet on control pipe. */
void USBSendZeroLengthPacketOnIEP0(void)
{
  EP0_DESCRIPTOR *descriptor = (EP0_DESCRIPTOR *)OSGetIODescriptor(OS_IO_USB_INPUT_ENDPOINT0);
  descriptor->BytesRemaining = NO_MORE_DATA;
  StatusAction = STATUS_ACTION_NOTHING;
  EndPoint0DescriptorBlock.IEPBCNT = 0x00;
}


static const DEVICE_REQUEST_COMPARE USBRequestList[] =
{
  #ifdef _CDC_
     //---- CDC Class Requests -----//
     // GET LINE CODING
     USB_REQ_TYPE_INPUT | USB_REQ_TYPE_CLASS | USB_REQ_TYPE_INTERFACE,
     USB_CDC_GET_LINE_CODING,
     0x00,0x00,                          // always zero
     INTERFACE_NUMBER_CDC,0x00,          // CDC interface is 0
     0x07,0x00,                          // Size of Structure (data length)
     0xff,&USBCdcGetLineCoding,
     // SET LINE CODING
     USB_REQ_TYPE_OUTPUT | USB_REQ_TYPE_CLASS | USB_REQ_TYPE_INTERFACE,
     USB_CDC_SET_LINE_CODING,
     0x00,0x00,                          // always zero
     INTERFACE_NUMBER_CDC,0x00,          // CDC interface is 0
     0x07,0x00,                          // Size of Structure (data length)
     0xff,&USBCdcSetLineCoding,
     // SET CONTROL LINE STATE
     USB_REQ_TYPE_OUTPUT | USB_REQ_TYPE_CLASS | USB_REQ_TYPE_INTERFACE,
     USB_CDC_SET_CONTROL_LINE_STATE,
     0xff,0xff,                          // Contains data
     INTERFACE_NUMBER_CDC,0x00,          // CDC interface is 0
     0x00,0x00,                          // No further data
     0xcf,&USBSetControlLineState,
  #endif // _CDC
  #ifdef _HID_
     //---- HID Class Requests -----//
     // GET REPORT
     USB_REQ_TYPE_INPUT | USB_REQ_TYPE_CLASS | USB_REQ_TYPE_INTERFACE,
     USB_REQ_GET_REPORT,
     0xff,0xff,
     INTERFACE_NUMBER_HID,0x00,
     0xff,0xff,
     0xcc,&USBHidGetReport,
     // SET REPORT
     USB_REQ_TYPE_OUTPUT | USB_REQ_TYPE_CLASS | USB_REQ_TYPE_INTERFACE,
     USB_REQ_SET_REPORT,
     0xff,0xFF,                          // bValueL is index and bValueH is type
     INTERFACE_NUMBER_HID,0x00,
     0xff,0xff,
     0xcc,&USBHidSetReport,
     // GET REPORT DESCRIPTOR
     USB_REQ_TYPE_INPUT | USB_REQ_TYPE_STANDARD | USB_REQ_TYPE_INTERFACE,
     USB_REQ_GET_DESCRIPTOR,
     0xff,DESC_TYPE_REPORT,              // bValueL is index and bValueH is type
     INTERFACE_NUMBER_HID,0x00,
     0xff,0xff,
     0xdc,&USBHidGetReportDescriptor,
     // GET HID DESCRIPTOR
     USB_REQ_TYPE_INPUT | USB_REQ_TYPE_STANDARD | USB_REQ_TYPE_INTERFACE,
     USB_REQ_GET_DESCRIPTOR,
     0xff,DESC_TYPE_HID,                 // bValueL is index and bValueH is type
     INTERFACE_NUMBER_HID,0x00,
     0xff,0xff,
     0xdc,&USBHidGetHidDescriptor,
  #endif // _HID_
     //---- USB Standard Requests -----//
     // clear device feature
     USB_REQ_TYPE_OUTPUT | USB_REQ_TYPE_STANDARD | USB_REQ_TYPE_DEVICE,
     USB_REQ_CLEAR_FEATURE,
     FEATURE_REMOTE_WAKEUP,0x00,         // feature selector
     0x00,0x00,
     0x00,0x00,
     0xff,&USBClearDeviceFeature,
     // clear endpoint feature
     USB_REQ_TYPE_OUTPUT | USB_REQ_TYPE_STANDARD | USB_REQ_TYPE_ENDPOINT,
     USB_REQ_CLEAR_FEATURE,
     FEATURE_ENDPOINT_STALL,0x00,
     0xff,0x00,
     0x00,0x00,
     0xf7,&USBClearEndpointFeature,
     // get configuration
     USB_REQ_TYPE_INPUT | USB_REQ_TYPE_STANDARD | USB_REQ_TYPE_DEVICE,
     USB_REQ_GET_CONFIGURATION,
     0x00,0x00,
     0x00,0x00,
     0x01,0x00,
     0xff,&USBGetConfiguration,
     // get device descriptor
     USB_REQ_TYPE_INPUT | USB_REQ_TYPE_STANDARD | USB_REQ_TYPE_DEVICE,
     USB_REQ_GET_DESCRIPTOR,
     0xff,DESC_TYPE_DEVICE,              // bValueL is index and bValueH is type
     0xff,0xff,
     0xff,0xff,
     0xd0,&USBGetDeviceDescriptor,
     // get configuration descriptor
     USB_REQ_TYPE_INPUT | USB_REQ_TYPE_STANDARD | USB_REQ_TYPE_DEVICE,
     USB_REQ_GET_DESCRIPTOR,
     0xff,DESC_TYPE_CONFIG,              // bValueL is index and bValueH is type
     0xff,0xff,
     0xff,0xff,
     0xd0,&USBGetConfigurationDescriptor,
     // get string descriptor
     USB_REQ_TYPE_INPUT | USB_REQ_TYPE_STANDARD | USB_REQ_TYPE_DEVICE,
     USB_REQ_GET_DESCRIPTOR,
     0xff,DESC_TYPE_STRING,              // bValueL is index and bValueH is type
     0xff,0xff,
     0xff,0xff,
     0xd0,&USBGetStringDescriptor,
     // get interface
     USB_REQ_TYPE_INPUT | USB_REQ_TYPE_STANDARD | USB_REQ_TYPE_INTERFACE,
     USB_REQ_GET_INTERFACE,
     0x00,0x00,
     0xff,0xff,
     0x01,0x00,
     0xf3,&USBGetInterface,
     // get device status
     USB_REQ_TYPE_INPUT | USB_REQ_TYPE_STANDARD | USB_REQ_TYPE_DEVICE,
     USB_REQ_GET_STATUS,
     0x00,0x00,
     0x00,0x00,
     0x02,0x00,
     0xff,&USBGetDeviceStatus,
     // get interface status
     USB_REQ_TYPE_INPUT | USB_REQ_TYPE_STANDARD | USB_REQ_TYPE_INTERFACE,
     USB_REQ_GET_STATUS,
     0x00,0x00,
     0xff,0x00,
     0x02,0x00,
     0xf7,&USBGetInterfaceStatus,
     // get endpoint status
     USB_REQ_TYPE_INPUT | USB_REQ_TYPE_STANDARD | USB_REQ_TYPE_ENDPOINT,
     USB_REQ_GET_STATUS,
     0x00,0x00,
     0xff,0x00,
     0x02,0x00,
     0xf7,&USBGetEndpointStatus,
     // set address
     USB_REQ_TYPE_OUTPUT | USB_REQ_TYPE_STANDARD | USB_REQ_TYPE_DEVICE,
     USB_REQ_SET_ADDRESS,
     0xff,0x00,
     0x00,0x00,
     0x00,0x00,
     0xdf,&USBSetAddress,
     // set configuration
     USB_REQ_TYPE_OUTPUT | USB_REQ_TYPE_STANDARD | USB_REQ_TYPE_DEVICE,
     USB_REQ_SET_CONFIGURATION,
     0xff,0x00,
     0x00,0x00,
     0x00,0x00,
     0xdf,&USBSetConfiguration,
     // set device feature
     USB_REQ_TYPE_OUTPUT | USB_REQ_TYPE_STANDARD | USB_REQ_TYPE_DEVICE,
     USB_REQ_SET_FEATURE,
     0xff,0x00,                      // feature selector
     0x00,0x00,
     0x00,0x00,
     0xdf,&USBSetDeviceFeature,
     // set endpoint feature
     USB_REQ_TYPE_OUTPUT | USB_REQ_TYPE_STANDARD | USB_REQ_TYPE_ENDPOINT,
     USB_REQ_SET_FEATURE,
     0xff,0x00,                      // feature selector
     0xff,0x00,                      // endpoint number <= 127
     0x00,0x00,
     0xd7,&USBSetEndpointFeature,
     // set interface
     USB_REQ_TYPE_OUTPUT | USB_REQ_TYPE_STANDARD | USB_REQ_TYPE_INTERFACE,
     USB_REQ_SET_INTERFACE,
     0xff,0x00,                      // feature selector
     0xff,0x00,                      // interface number
     0x00,0x00,
     0xd7,&USBSetInterface,
     // end of USB descriptor -- this one will be matched to any USB request
     //                          since bCompareMask is 0x00.
     0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
     0x00,&USBInvalidRequest     // end of list
};


/* Decode incoming USB setup packet and call corresponding function
** USBDecodeAndProcessUSBRequest is called from IRQ. Do not call from user application */
void USBDecodeAndProcessUSBRequest(void)
{
  UINT8  mask, result, i;
  const UINT8* requestList;
  UINT8  requestType, request;
  UINT32 addrOfFunction;
  // point to beginning of the matrix
  requestList = (UINT8 *)&USBRequestList[0];
  while (TRUE) {
     requestType = *requestList++;
     request     = *requestList++;
     if (((requestType == 0xff) && (request == 0xff)) ||
         (SetupPacket.RequestType == (USB_REQ_TYPE_INPUT | USB_REQ_TYPE_VENDOR | USB_REQ_TYPE_DEVICE)) ||
         (SetupPacket.RequestType == (USB_REQ_TYPE_OUTPUT | USB_REQ_TYPE_VENDOR | USB_REQ_TYPE_DEVICE))) {
        requestList -= 2;
        break;
     }
     if ((requestType == SetupPacket.RequestType) && (request == SetupPacket.Request)) {
        // compare the first two
        result = 0xc0;
        mask   = 0x20;
        // first two UINT8s matched, compare the rest
        for (i = 2; i < 8; i++) {
            if (*((UINT8*)&SetupPacket + i) == *requestList)
               result |= mask;
            requestList++;
            mask = mask >> 1;
        }
        // now we have the result
        if ((*requestList & result) == *requestList) {
           requestList -= 8;
           break;
        }
        else {
           requestList += (sizeof(DEVICE_REQUEST_COMPARE)-8);
        }
    }
    else
        requestList += (sizeof(DEVICE_REQUEST_COMPARE)-2);
  }
  // if another setup packet comes before we have the chance to process current
  // setup request, we return here without processing the request
  // this check is not necessary but still kept here to reduce response(or simulation) time
  if ((USBIFG & STPOWIFG) != 0x00)
     return;
  // now we found the match and jump to the function accordingly.
  // DEVICE_REQUEST_COMPARE  -->  member of structure
  // void(*USBFunction)(void) --> function pointer in the structure we want to call 
  addrOfFunction = *((UINT32 *)((requestList) + 10));
  // call function
  ((void (*)(void))addrOfFunction)();
}


void USBSetupPacketInterruptHandler(void *descriptor)
{
  UINT8 i;
  USBCTL |= FRSTE; // Function Reset Connection Enable - set enable after first
                   // setup packet was received
USBProcessNewSetupPacket:
  // copy the MSB of bmRequestType to DIR bit of USBCTL
  if ((SetupPacket.RequestType & USB_REQ_TYPE_INPUT) == USB_REQ_TYPE_INPUT)
     USBCTL |= DIR;
  else
     USBCTL &= ~DIR;
  StatusAction = STATUS_ACTION_NOTHING;
  // clear out return data buffer
  for (i = 0; i < USB_RETURN_DATA_LENGTH; i++)
      USBRequestReturnData[i] = 0x00;
  // decode and process the request
  USBDecodeAndProcessUSBRequest();
  // check if there is another setup packet pending
  // if it is, abandon current one by NAKing both data endpoint 0
  if ((USBIFG & STPOWIFG) != 0x00) {
     USBIFG &= ~(STPOWIFG | SETUPIFG);
     goto USBProcessNewSetupPacket;
  }
  USBIE |= SETUPIE; // Re-enable interrupt
}


void USBPWRVBUSoffHandler(void *descriptor)
{
  volatile UINT16 i;
  for (i =0; i < 1000; i++);
  if (!(USBPWRCTL & USBBGVBV)) {
     USBKEYPID   =    0x9628;     // set KEY and PID to 0x9628 -> access to configuration registers enabled
     EnumerationStatus = 0x00;    // device is not enumerated
     FunctionSuspended = FALSE;   // device is not suspended
     USBCNF     =    0;           // disable USB module
     USBPLLCTL  &=  ~UPLLEN;      // disable PLL
     USBPWRCTL &= ~(VBOFFIE + VBOFFIFG); // disable interrupt VBUSoff
     USBKEYPID   =    0x9600;     // access to configuration registers disabled
  }
  USBPWRCTL |= VBOFFIE; // Re-enable interrupt
  SetVCore(OldVCORE);
}


void USBPWRVBUSonHandler(void *descriptor)
{
  volatile UINT16 i;
  OldVCORE = GetVCore();
  SetVCore(3); // USB core requires the VCore set to 1.8 volt, independ of CPU clock frequency
  for (i = 0; i < 1000; i++);          // waiting till voltage will be stable
  USBKEYPID =  0x9628;                // set KEY and PID to 0x9628 -> access to configuration registers enabled
  USBPWRCTL |= VBOFFIE;               // enable interrupt VBUSoff
  USBPWRCTL &= ~ (VBONIFG + VBOFFIFG);             // clean int flag (bouncing)
  USBKEYPID =  0x9600;                // access to configuration registers disabled
  //We switch on USB and connect to the BUS
  if (USBEnable() == USB_SUCCEED) {
     USBReset();
     USBConnect();  // generate rising edge on DP -> the host enumerates our device as full speed device
  }
  USBPWRCTL |= VBONIE; // Re-enable interrupt
}


void USBIEP0InterruptHandler(void *descriptor)
{
  USBCTL |= FRSTE;                              // Function Reset Connection Enable
  EndPoint0DescriptorBlock.OEPBCNT = 0x00;     
  if (StatusAction == STATUS_ACTION_DATA_IN)
     USBSendNextPacketOnIEP0();
  else
     EndPoint0DescriptorBlock.IEPCNFG |= EPCNF_STALL; // no more data
  USBIEPIE |= BIT0; // Re-enable interrupt
}


void USBOEP0InterruptHandler(void *descriptor)
{
  USBCTL |= FRSTE;                              // Function Reset Connection Enable
  EndPoint0DescriptorBlock.IEPBCNT = 0x00;    
  if (StatusAction == STATUS_ACTION_DATA_OUT) {
     USBReceiveNextPacketOnOEP0();
     #ifdef _CDC_
        if (StatusAction == STATUS_ACTION_NOTHING && SetupPacket.Request == USB_CDC_SET_LINE_CODING)
           USBCdcSetLineCodingHandler(descriptor);
     #endif // _CDC_
  }
  else
     EndPoint0DescriptorBlock.OEPCNFG |= EPCNF_STALL; // no more data
  USBOEPIE |= BIT0; // Re-enable interrupt
}


void USBDoNothingHandler(void *descriptor)
{
}

#endif /* end of USB_DRIVER */

