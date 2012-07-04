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
/* File ZottaOS_CortexMx.c: Contains functions that are common to any specific Cortex-Mx
** microcontroller.
** Version date: March 2012
** Authors: MIS-TIC
*/

#ifndef _ZOTTAOS_CORTEXMX_H_
#define _ZOTTAOS_CORTEXMX_H_

/* Non-blocking algorithms use a marker that needs to be part of the address. These algo-
** rithms operate in RAM and need an address bit that is never used. */
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

/* _OSSleep: Sets the processor to its lowest possible sleep mode. */
#define _OSSleep() while (TRUE) { __asm("WFI"); } // Request Wait For Interrupt

/* _OSScheduleTask: Generates a PendSV exception which will interrupt and proceed with the
** lowest interrupt priority to handler _OSContextSwapHandler (defined in assembler in
** ZottaOS_CortexMx_a.S). 0xE000ED04 is address of ICSR register. */
#define _OSScheduleTask() (*((UINT32 *)0xE000ED04) = 0x10000000)

/* _OSGenerateSoftTimerInterrupt: Called by the timer peripheral to generate a SysTick
** exception which will interrupt and continue with a smaller priority to handler Timer-
** InterruptHandler defined in ZottaOSHard.c or ZottaOSSoft.c. 0xE000ED04 is address of
** ICSR register. */
#define _OSGenerateSoftTimerInterrupt() (*((UINT32 *)0xE000ED04) = 0x4000000)


/* _OSClearSoftTimerInterrupt: . 0xE000ED04 is address of ICSR register. */
#define _OSClearSoftTimerInterrupt() (*((UINT32 *)0xE000ED04) = 0x2000000)

#endif /* _ZOTTAOS_CORTEXMX_H_ */
