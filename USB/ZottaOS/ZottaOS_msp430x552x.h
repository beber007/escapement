/* Copyright (c) 2006-2009 MIS Institute of the HEIG affiliated to the University of
** Applied Sciences of Western Switzerland. All rights reserved.
** IN NO EVENT SHALL THE MIS INSTITUTE NOR THE HEIG NOR THE UNIVERSITY OF APPLIED
** SCIENCES OF WESTERN SWITZERLAND BE LIABLE TO ANY PARTY FOR DIRECT, INDIRECT, SPECIAL,
** INCIDENTAL, OR CONSEQUENTIAL DAMAGES ARISING OUT OF THE USE OF THIS SOFTWARE AND ITS
** DOCUMENTATION, EVEN IF THE MIS INSTITUTE OR THE HEIG OR THE UNIVERSITY OF APPLIED
** SCIENCES OF WESTERN SWITZERLAND HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
** THE MIS INSTITUTE, THE HEIG AND THE UNIVERSITY OF APPLIED SCIENCES OF WESTERN SWIT-
** ZERLAND SPECIFICALLY DISCLAIMS ANY WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
** IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE. THE SOFT-
** WARE PROVIDED HEREUNDER IS ON AN "AS IS" BASIS, AND THE MIS INSTITUTE NOR THE HEIG
** AND NOR THE UNIVERSITY OF APPLIED SCIENCES OF WESTERN SWITZERLAND HAVE NO OBLIGATION
** TO PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR MODIFICATIONS.
*/
/* File ZottaOS_msp430x552x.h: Holds interrupt definitions for msp430x552x devices.
** Created on March 12, 2010, by ZottaOS MSP430 Configuration Tool.
** Authors: MIS-TIC
*/
#ifndef ZOTTAOS_MSP430X552X_H_
#define ZOTTAOS_MSP430X552X_H_

/* Add all inclusion files needed to build an application. */
#include "ZottaOS.h"
#include "msp430.h"
#include "ZottaOS_Timer.h"

#define USB_DRIVER

/* The following symbol defines the maximum size of permanent allocations that are done
** by ZottaOS and by the application. See function OSAlloca. */
#define OSALLOCA_INTERNAL_HEAP_SIZE   0x400

#ifdef __LARGE_CODE_MODEL__
   /* Uncomment the following symbol if your application uses 20-bit data registers. */
   //#define SAVE_20_BIT_REGISTERS
#endif

/* Because msp430x552x does not use the same register names for Timer A, which are used by
** ZottaOS, we redefine them here. */
#define TACTL   TA0CTL     /* Timer A control */
#define TAR     TA0R       /* Timer A counter */
#define TACCR0  TA0CCR0    /* Timer A capture/compare 0 */
#define TAIV    TA0IV      /* Timer A interrupt vector */

/* The following symbols define the MSP430 family of microcontrollers, which can be used
** to create portable code between different families. */
#define OS_MSP430_FAMILY_1XX   0    /* msp430x1xx */
#define OS_MSP430_FAMILY_2XX   1    /* msp430x2xx */
#define OS_MSP430_FAMILY_4XX   2    /* msp430x4xx */
#define OS_MSP430_FAMILY_5XX   3    /* msp430x5xx or cc430x5xx */
#define OS_MSP430_FAMILY_6XX   4    /* cc430x6xx */
#define OS_MSP430_FAMILY       OS_MSP430_FAMILY_5XX

/* Entries into ZottaOS I/O interrupt vector. */

#define OS_IO_USB_PWR_DROP                 0
#define OS_IO_USB_PLL_LOCK                 2
#define OS_IO_USB_PLL_SIGNAL               4
#define OS_IO_USB_PLL_RANGE                6
#define OS_IO_USB_PWR_VBUSOn               8
#define OS_IO_USB_PWR_VBUSOff             10
#define OS_IO_USB_USB_TIMESTAMP           12
#define OS_IO_USB_INPUT_ENDPOINT0         14
#define OS_IO_USB_OUTPUT_ENDPOINT0        16
#define OS_IO_USB_RSTR                    18
#define OS_IO_USB_SUSR                    20
#define OS_IO_USB_RESR                    22
#define OS_IO_USB_SETUP_PACKET_RECEIVED   24 
#define OS_IO_USB_STPOW_PACKET_RECEIVED   26
#define OS_IO_USB_INPUT_ENDPOINT1         28 
#define OS_IO_USB_INPUT_ENDPOINT2         30
#define OS_IO_USB_INPUT_ENDPOINT3         32
#define OS_IO_USB_INPUT_ENDPOINT4         34
#define OS_IO_USB_INPUT_ENDPOINT5         36
#define OS_IO_USB_INPUT_ENDPOINT6         38
#define OS_IO_USB_INPUT_ENDPOINT7         40
#define OS_IO_USB_OUTPUT_ENDPOINT1        42
#define OS_IO_USB_OUTPUT_ENDPOINT2        44
#define OS_IO_USB_OUTPUT_ENDPOINT3        46
#define OS_IO_USB_OUTPUT_ENDPOINT4        48
#define OS_IO_USB_OUTPUT_ENDPOINT5        50
#define OS_IO_USB_OUTPUT_ENDPOINT6        52
#define OS_IO_USB_OUTPUT_ENDPOINT7        54
#define OS_IO_USB_NO_INTERRUPT            56


/* The next symbol is the size of the interrupt table. */
#define OS_IO_MAX 58 /* Defined as the last defined OS_IO_XXX + 2 */

#endif /* ZOTTAOS_MSP430X552X_H_ */

