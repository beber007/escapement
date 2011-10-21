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
/* File descriptors.h: .
** Version identifier: April 2010
** Authors: MIS-TIC
*/
/* (c)2009 by Texas Instruments Incorporated, All Rights Reserved. */

#define _CDC_          // Needed for CDC inteface
//#define _HID_          // Needed for HID interface

/* These constants configure the API stack and help define the USB descriptors. Refer to
** Sec. 6 of the MSP430 USB CDC API Programmer's Guide for descriptions of these
** constants. */

/* Vendor ID, 0x2047 for Texas Instruments Incorporated (MSP430 Group) */
#define USB_VID 0x2047

#ifdef _CDC_
    #define USB_PID                             0x0300 // Product ID (PID), 
                                                       // 0xF400 for CDC stack
    #define SIZEOF_CONFIGURATION_DESCRIPTOR     53     // wTotalLength, CDC
    #define INTERFACE_NUMBER_CDC                0
    #define CDC_INTEP_ADDR                      0x82
    #define CDC_OUTEP_ADDR                      0x03
    #define CDC_INEP_ADDR                       0x83

    #ifdef _HID_
      #error Only ONE of interfaces should be defined: _HID_ or _CDC_!
    #endif
#else
    #ifdef _HID_
        #define USB_PID                         0x0301 // Product ID (PID),
                                                       // 0xF401 for HID stack
        #define SIZEOF_CONFIGURATION_DESCRIPTOR 41     // wTotalLength, HID
        #define START_HID_DESCRIPTOR            18
        #define INTERFACE_NUMBER_HID            0
        #define HID_OUTEP_ADDR                  0x01
        #define HID_INEP_ADDR                   0x81
    #else
        #error No interface HID or CDC defined! Define it.
    #endif
#endif

/*----------------------------------------------------------------------------+
| Firmware Version                                                            |
| How to detect version number of the FW running on MSP430?                   |
| on Windows Open ControlPanel->Systems->Hardware->DeviceManager->Ports->     |
|         Msp430->ApplicationUART->Details                                    |
+----------------------------------------------------------------------------*/
#define VER_FW_H 0x01 // Device release number -- high and low bytes,
                      // in binary-coded decimal
#define VER_FW_L 0x19 // This is reported in the bcdDevice field of the device descriptor

// MCLK frequency of MCU, in Hz
// For running higher frequencies the Vcore voltage adjustment may required.
// Please refer to Data Sheet of the MSP430 device you use
#define USB_MCLK_FREQ 8000000               // MCLK frequency of MCU, in Hz

#define MAX_PACKET_SIZE          0x40       // Max size of the USB packets.
//#define MAX_PACKET_SIZE          0x20       // Max size of the USB packets.
//#define MAX_PACKET_SIZE          0x10       // Max size of the USB packets.
//#define MAX_PACKET_SIZE          0x08       // Max size of the USB packets.


/* If a serial number is to be reported, set this to the index within the string
** descriptor of the dummy serial number string.  It will then be automatically
** handled by the API. If no serial number is to be reported, set this to 0. */
#define USB_STR_INDEX_SERNUM 3


#define USB_PLL_XT 2 // Defines which XT is used by the PLL (1=XT1, 2=XT2)
#define USB_XT_FREQ USBPLL_SETCLK_4_0 // Indicates the freq of the crystal on the
                                      // oscillator indicated by USB_PLL_XT
#define USB_DISABLE_XT_SUSPEND 1 // If non-zero, then USB_suspend() will disable the
                                 // oscillator that is designated by USB_PLL_XT; if 
                                 // zero, USB_suspend won't affect the oscillator

#define USB_SUPPORT_REM_WAKE 0x20 // If remote wakeup is to be supported, set this to
                                  // 0x20.  If not, set to 0x00. All other values are
                                  // prohibited.

/* Controls whether the device reports itself to be self-powered to any degree.  If so,
** set to 0x40.  If the device is fully supplied by the bus, set to 0x00.  All other
** values are prohibited. */
#define USB_SUPPORT_SELF_POWERED 0x40


/* Controls how much power the device reports it will draw from VBUS.  Expressed in 2mA
** units; that is, the number of mA communicated is twice the value of this field. */
#define USB_MAX_POWER 0x32

/* Set to 0xFF if no DMA channel will be used 0..7 for selected DMA channel */
#define USB_DMA_TX 0x1 
#define USB_DMA_RX 0x1

#define USB_NUM_CONFIGURATIONS 1 // Number of implemented interfaces. This is fixed to 1.


/* DESCRIPTOR CONSTANTS */
#define SIZEOF_DEVICE_DESCRIPTOR  0x12
#define SIZEOF_REPORT_DESCRIPTOR  36

#define CONFIG_STRING_INDEX       4
#define INTF_STRING_INDEX         5

/* OUTWARD DECLARATIONS */

/* Calculates the endpoint descriptor block number from given address */
#define EDB(addr) ((addr&0x07)-1)

extern UINT8 const DeviceDescriptor[SIZEOF_DEVICE_DESCRIPTOR];
extern UINT8 const ConfigurationDescriptorGroup[SIZEOF_CONFIGURATION_DESCRIPTOR];
extern UINT8 const ReportDescriptor[SIZEOF_REPORT_DESCRIPTOR];
extern UINT8 const StringDescriptor[];
