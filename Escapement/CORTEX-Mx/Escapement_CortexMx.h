/* Copyright (c) 2026 Bertrand Hurst. All rights reserved.
** Escapement - Lightweight Power-Aware Real-Time OS.
** Derived from prior work; see LICENSE and NOTICE at the root of this repository.
*/
/* File Escapement_CortexMx.c: Contains macros and defines that are common to any specific
** Cortex-Mx microcontroller.
** Platform version: All Cortex-Mx based microcontrollers.
** Version date: March 2012
*/

#ifndef _ESCAPEMENT_CORTEXMX_H_
#define _ESCAPEMENT_CORTEXMX_H_

/* Non-blocking algorithms use a marker that needs to be part of the address. These algo-
** rithms operate in RAM and need an address bit that is never used. */
#define MARKEDBIT    0x80000000u
#define UNMARKEDBIT  0x7FFFFFFFu

/* _OSIOHandler: Interrupt service routine called whenever an IRQ is raised. */
void _OSIOHandler(void);

/* _OSEnableInterrupts and _OSDisableInterrupts: Macros changing the state of the special
** register PRIMASK.These are provided to allow portable code between different microcon-
** trollers. */
#define _OSEnableInterrupts()  __asm("CPSIE i;")
#define _OSDisableInterrupts() __asm("CPSID i;")

/* _OSSleep: Sets the processor to its lowest possible sleep mode. */
#ifdef ESCAPEMENT_VERSION_HARD_PA
   #define _OSSleep() while (TRUE) { \
                         OSSetProcessorSpeed(OS_MAX_SPEED); \
                         __asm("WFI"); \
                      };
#else
   #define _OSSleep() while (TRUE) { __asm("WFI"); };
#endif

/* _OSScheduleTask: Generates a PendSV exception, which will interrupt and proceed at the
** lowest interrupt priority to handler _OSContextSwapHandler (defined in assembler in
** Escapement_CortexMx_a.S). Sets bit PENDSVSET of ICSR (0xE000ED04). */
#define _OSScheduleTask() (*((UINT32 *)0xE000ED04) = 0x10000000)  /* Note that the write
** bits of ICSR take effect only if they are set, hence an assignment is the proper way
** to set a bit. */

/* _OSGenerateSoftTimerInterrupt: Called by the timer peripheral to generate a SysTick
** exception, which will interrupt and continue with a smaller priority to handler
** _OSTimerInterruptHandler defined in EscapementHard.c or EscapementSoft.c. Sets bit PENDSTSET
** of ICSR (0xE000ED04). (See note in _OSScheduleTask) */
#define _OSGenerateSoftTimerInterrupt() (*((UINT32 *)0xE000ED04) = 0x4000000)

/* _OSClearSoftTimerInterrupt: Called by _OSTimerInterruptHandler binded to the SysTick
** exception to remove its interrupt pending status. In other words, a new SysTick excep-
** tion can be raised. Sets bit PENDSTCLR of ICSR (0xE000ED04). (See note in _OSSchedule-
** Task) */
#define _OSClearSoftTimerInterrupt() (*((UINT32 *)0xE000ED04) = 0x2000000)

#endif /* _ESCAPEMENT_CORTEXMX_H_ */
