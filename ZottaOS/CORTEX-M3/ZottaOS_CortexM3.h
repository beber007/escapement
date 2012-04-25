/* Copyright (c) 2006-2012 MIS Institute of the HEIG-VD affiliated to the University of
** Applied Sciences of Western Switzerland. All rights reserved.
** Permission to use, copy, modify, and distribute this software and its documentation
** for any purpose, without fee, and without written agreement is hereby granted, pro-
** vided that the above copyright notice, the following three sentences and the authors
** appear in all copies of this software and in the software where it is used.
** IN NO EVENT SHALL THE MIS INSTITUTE NOR THE HEIG-VD NOR THE UNIVERSITY OF APPLIED
** SCIENCES OF WESTERN SWITZERLAND BE LIABLE TO ANY PARTY FOR DIRECT, INDIRECT, SPECIAL,
** INCIDENTAL, OR CONSEQUENTIAL DAMAGES ARISING OUT OF THE USE OF THIS SOFTWARE AND ITS
** DOCUMENTATION, EVEN IF THE MIS INSTITUTE OR THE HEIG-VD OR THE UNIVERSITY OF APPLIED
** SCIENCES OF WESTERN SWITZERLAND HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
** THE MIS INSTITUTE, THE HEIG-VD AND THE UNIVERSITY OF APPLIED SCIENCES OF WESTERN SWIT-
** ZERLAND SPECIFICALLY DISCLAIMS ANY WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
** IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE. THE SOFT-
** WARE PROVIDED HEREUNDER IS ON AN "AS IS" BASIS, AND THE MIS INSTITUTE NOR THE HEIG-VD
** AND NOR THE UNIVERSITY OF APPLIED SCIENCES OF WESTERN SWITZERLAND HAVE NO OBLIGATION
** TO PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR MODIFICATIONS.
*/
/* File ZottaOS_STM32.c: Contains functions that are specific to the STM-32 family of
** microcontrollers. The functions defined here are divided into 2 parts. The first part
** contains functions to support the time and the timer. The second part consists of the
** the microcontroller's interrupt routines that start at position 16 of the interrupt
** table and that are specific to the microcontroller at hand.
** Version date: March 2012
** Authors: MIS-TIC
*/

#ifndef _ZOTTAOS_CORTEXM3_H_
#define _ZOTTAOS_CORTEXM3_H_

/* Non-blocking algorithms use a marker that needs to be part of the address. These algo-
** rithms operate in RAM and for which a MSP430 or CC430 MSB address is never used. */
#define MARKEDBIT    0x80000000u
#define UNMARKEDBIT  0x7FFFFFFFu


/* _OSIOHandler: Routine de traitement générique des interruptions. A noter que si
** plusieurs timer utilisant le même vecteur sont utilisé, alors c'est la routine
** _OSIOHandlerMultiTimer qui est utilisée. _OSIOHandlerMultiTimer est definit dans
**  ZottaOS_Timer.c */
void _OSIOHandler(void);

/* _OSEnableInterrupts and _OSDisableInterrupts: . */
#define _OSEnableInterrupts()  __asm("CPSIE i;")
#define _OSDisableInterrupts() __asm("CPSID i;")

/* _OSSleep: . */
#define _OSSleep() while (TRUE) { __asm("WFI"); } // Request Wait For Interrupt

/* _OSScheduleTask: Generates a PendSV exception which will interrupt and proceed with the
** lowest interrupt priority to handler _OSContextSwapHandler (defined in this file). */
#define _OSScheduleTask() (*((UINT32 *)0xE000ED04) = 0x10000000)

/* _OSGenerateSoftTimerInterrupt: Called by the timer peripheral to generate a SysTick
** exception which will interrupt and continue with a smaller priority to handler Timer-
** InterruptHandler defined in NTRTOS_CortexM3.c. */
#define _OSGenerateSoftTimerInterrupt() (*((UINT32 *)0xE000ED04) = 0x4000000)

#endif /* _ZOTTAOS_CORTEXM3_H_ */
