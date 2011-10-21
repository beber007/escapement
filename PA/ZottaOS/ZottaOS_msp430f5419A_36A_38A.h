/* Copyright (c) 2006-2011 MIS Institute of the HEIG affiliated to the University of
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
/* File ZottaOS_msp430f5419A_36A_38A.h: Holds interrupt definitions for msp430f5419A_36A_38A
**                                      devices and ZottaOS specifics.
** Created on June 22, 2011, by ZottaOS MSP430 Configurator Tool.
** Authors: MIS-TIC
*/
#ifndef ZOTTAOS_MSP430F5419A_36A_38A_H_
#define ZOTTAOS_MSP430F5419A_36A_38A_H_

/* ZottaOS kernel selection. The defines given below are meant to write portable appli-
** cations from one kernel to another.
** Caution: Additional defines can be included when ZottaOS Configurator Tool generates
** this file for a specific kernel. */
#define ZOTTAOS_VERSION_HARD      0
#define ZOTTAOS_VERSION_SOFT      1
#define ZOTTAOS_VERSION_HARD_PA   2  /* Power-aware versions of the above */
#define ZOTTAOS_VERSION_SOFT_PA   3
#define ZOTTAOS_VERSION           ZOTTAOS_VERSION_HARD_PA

/* The following symbol defines the maximum size of permanent allocations performed by
** OSMalloc while main is in execution. This value can be increased if more than 512
** bytes are needed, and decreased if the run-time stack overflows before or when
** OSStartMultitasking is called. Note that there is no point optimizing this value as
** the run-time stack pointer is readjusted within OSStartMultitasking so that the stack
** can take all the remaining RAM memory not occupied by the dynamic memory allocations
** and the application's global variables.
** (Also see function OSMalloc in ZottaOS_msp430f5419A_36A_38A.asm) */
#define OSMALLOC_INTERNAL_HEAP_SIZE   0x200

#ifdef __LARGE_CODE_MODEL__
   /* Uncomment the following symbol if your application uses 20-bit data registers. */
   //#define SAVE_20_BIT_REGISTERS
#endif

/* Defines for ZottaOS internal timer TIMER0_A */
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
** source and divider initializations can be found in file ZottaOS_msp430f5419A_36A_38A.asm. */
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

#endif /* ZOTTAOS_MSP430F5419A_36A_38A_H_ */

