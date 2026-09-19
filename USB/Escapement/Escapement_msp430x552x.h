/* Copyright (c) 2026 Bertrand Hurst. All rights reserved.
** Escapement - Lightweight Power-Aware Real-Time OS.
** Derived from prior work; see LICENSE and NOTICE at the root of this repository.
*/
/* File Escapement_msp430x552x.h: Holds interrupt definitions for msp430x552x devices.
** Created on March 12, 2010, by Escapement MSP430 Configuration Tool.
*/
#ifndef ESCAPEMENT_MSP430X552X_H_
#define ESCAPEMENT_MSP430X552X_H_

/* Add all inclusion files needed to build an application. */
#include "Escapement.h"
#include "msp430.h"
#include "Escapement_Timer.h"

#define USB_DRIVER

/* The following symbol defines the maximum size of permanent allocations that are done
** by Escapement and by the application. See function OSAlloca. */
#define OSALLOCA_INTERNAL_HEAP_SIZE   0x400

#ifdef __LARGE_CODE_MODEL__
   /* Uncomment the following symbol if your application uses 20-bit data registers. */
   //#define SAVE_20_BIT_REGISTERS
#endif

/* Because msp430x552x does not use the same register names for Timer A, which are used by
** Escapement, we redefine them here. */
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

/* Entries into Escapement I/O interrupt vector. */

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

#endif /* ESCAPEMENT_MSP430X552X_H_ */

