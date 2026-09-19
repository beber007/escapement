/* Copyright (c) 2026 Bertrand Hurst. All rights reserved.
** Escapement - Lightweight Power-Aware Real-Time OS.
** Derived from prior work; see LICENSE and NOTICE at the root of this repository.
*/
/* File Escapement_msp430f5419A_36A_38A.h: Holds interrupt definitions for msp430f5419A_36A_38A
**                                      devices and Escapement specifics.
** Created on June 22, 2011, by Escapement MSP430 Configurator Tool.
*/
#ifndef ESCAPEMENT_MSP430F5419A_36A_38A_H_
#define ESCAPEMENT_MSP430F5419A_36A_38A_H_

/* Escapement kernel selection. The defines given below are meant to write portable appli-
** cations from one kernel to another.
** Caution: Additional defines can be included when Escapement Configurator Tool generates
** this file for a specific kernel. */
#define ESCAPEMENT_VERSION_HARD      0
#define ESCAPEMENT_VERSION_SOFT      1
#define ESCAPEMENT_VERSION_HARD_PA   2  /* Power-aware versions of the above */
#define ESCAPEMENT_VERSION_SOFT_PA   3
#define ESCAPEMENT_VERSION           ESCAPEMENT_VERSION_HARD_PA

/* The following symbol defines the maximum size of permanent allocations performed by
** OSMalloc while main is in execution. This value can be increased if more than 512
** bytes are needed, and decreased if the run-time stack overflows before or when
** OSStartMultitasking is called. Note that there is no point optimizing this value as
** the run-time stack pointer is readjusted within OSStartMultitasking so that the stack
** can take all the remaining RAM memory not occupied by the dynamic memory allocations
** and the application's global variables.
** (Also see function OSMalloc in Escapement_msp430f5419A_36A_38A.asm) */
#define OSMALLOC_INTERNAL_HEAP_SIZE   0x200

#ifdef __LARGE_CODE_MODEL__
   /* Uncomment the following symbol if your application uses 20-bit data registers. */
   //#define SAVE_20_BIT_REGISTERS
#endif

/* Defines for Escapement internal timer TIMER0_A */
#define OSTimerControlRegister    TA0CTL
#define OSTimerCounter            TA0R
#define OSTimerCompareRegister    TA0CCR0
#define OSTimerSourceEnable       (TASSEL_1 | TAIE)

/* The following symbols define the MSP430 family of microcontrollers, which can be used
** to create portable code between different families. */
#define OS_MSP430_FAMILY_1XX   0    /* msp430x1xx */
#define OS_MSP430_FAMILY_2XX   1    /* msp430x2xx */
#define OS_MSP430_FAMILY_4XX   2    /* msp430x4xx */
#define OS_MSP430_FAMILY_5XX   3    /* msp430x5xx or cc430x5xx */
#define OS_MSP430_FAMILY_6XX   4    /* msp430x6xx or cc430x6xx */
#define OS_MSP430_FAMILY       OS_MSP430_FAMILY_5XX

/* OSInitializeSystemClocks: Performs all clock module initializations. This function
** should be called by main prior to calling OSStartMultitasking(). Specific clock
** source and divider initializations can be found in file Escapement_msp430f5419A_36A_38A.asm. */
void OSInitializeSystemClocks(void);

#define OS_8MHZ_SPEED  0  // all
#define OS_12MHZ_SPEED 1  // all
//#define OS_16MHZ_SPEED 2  // cc430
#define OS_20MHZ_SPEED 2  // msp430x55x & msp430x54xA
//#define OS_20MHZ_SPEED 3  // cc430
#define OS_25MHZ_SPEED 3  // msp430x55x & msp430x54xA

/* OSGetCurrentSpeed: */
unsigned char OSGetProcessorSpeed(void);

/* OSGetCurrentSpeed: */
void OSSetProcessorSpeed(unsigned char speed);

/* Entries into the kernel ISR interrupt vector. */

#define OS_IO_PORT2_0                0  /* Port 2 Pin 0 */

/* The next symbol is the size of the interrupt table. */
#define OS_IO_MAX 2 /* Defined as the last defined OS_IO_XXX + 2 */

#endif /* ESCAPEMENT_MSP430F5419A_36A_38A_H_ */

