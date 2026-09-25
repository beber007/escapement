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
** Authors: MIS-TIC
**
** Escapement - Lightweight Power-Aware Real-Time OS, derived from ZottaOS.
** Modifications Copyright (c) 2026 Bertrand Hurst, distributed under the same terms;
** see LICENSE and NOTICE at the root of this repository.
*/
/* File Escapement_CortexMx.h: Contains macros and defines that are common to any specific
** Cortex-Mx microcontroller.
** Platform version: All Cortex-Mx based microcontrollers.
** Version date: March 2012
*/

#ifndef _ESCAPEMENT_CORTEXMX_H_
#define _ESCAPEMENT_CORTEXMX_H_

#include "Escapement_Modes.h"   /* before the tests of SCHEDULER_REAL_TIME_MODE below */

/* Offsets the context switch reads out of a task control block. _OSContextSwapHandler is
** written in assembler and cannot see the C structure, so it addresses these fields by
** hand. The kernel variants check them with _Static_assert against their own TCB, which
** turns a silent mismatch into a build failure: a field added ahead of them, such as the
** third list link the power-aware variant once put after Next[], moves every one of them
** four bytes along. */
#if defined(ESCAPEMENT_VERSION_SOFT) && SCHEDULER_REAL_TIME_MODE == DEADLINE_MONOTONIC_SCHEDULING
   #define OS_TCB_STATE_OFFSET     8
   #define OS_TCB_ENTRY_OFFSET    20
   #define OS_TCB_ARGUMENT_OFFSET 24
#else
   #define OS_TCB_STATE_OFFSET     8
   #define OS_TCB_ENTRY_OFFSET    16
   #define OS_TCB_ARGUMENT_OFFSET 20
#endif

/* OSCheckTCBLayout: Placed by each kernel variant right after its TCB definition. Only
** meaningful where the assembler context switch actually runs: a host build of the kernel,
** for testing the scheduler, has wider pointers and a different layout, and no assembler
** that cares. */
#if !defined(_ASM_) && (defined(CORTEX_M0) || defined(CORTEX_M3) || defined(CORTEX_M4) || defined(CORTEX_M33))
   #define OSCheckTCBLayout() \
      _Static_assert(__builtin_offsetof(TCB,TaskState) == OS_TCB_STATE_OFFSET, \
                     "TaskState moved; Escapement_CortexMx_a.S reads it at another offset"); \
      _Static_assert(__builtin_offsetof(TCB,TaskCodePtr) == OS_TCB_ENTRY_OFFSET, \
                     "TaskCodePtr moved; Escapement_CortexMx_a.S reads it at another offset"); \
      _Static_assert(__builtin_offsetof(TCB,Argument) == OS_TCB_ARGUMENT_OFFSET, \
                     "Argument moved; Escapement_CortexMx_a.S reads it at another offset")
#else
   #define OSCheckTCBLayout() struct OSCheckTCBLayoutNotApplicable
#endif

/* Non-blocking algorithms use a marker that needs to be part of the address. These algo-
** rithms operate in RAM and need an address bit that is never used. */
#define MARKEDBIT    0x80000000u
#define UNMARKEDBIT  0x7FFFFFFFu

/* _OSIOHandler: Interrupt service routine called whenever an IRQ is raised. */
void _OSIOHandler(void);

/* _OSEnableInterrupts and _OSDisableInterrupts: Macros changing the state of the special
** register PRIMASK. These are provided to allow portable code between different microcon-
** trollers.
** The memory clobber is what makes them critical sections as far as the compiler is
** concerned. Without it, masking interrupts only constrains the processor, not the code
** generator: reads and writes may legally be moved across the boundary, and what the
** section was protecting is then protected only by the compiler's goodwill. */
#define _OSEnableInterrupts()  __asm volatile ("CPSIE i" ::: "memory")
#define _OSDisableInterrupts() __asm volatile ("CPSID i" ::: "memory")

/* _OSSleep: Sets the processor to its lowest possible sleep mode. */
#ifdef ESCAPEMENT_VERSION_HARD_PA
   /* Operating point the idle task sleeps at, the maximum unless make SLEEP_SPEED=n
   ** chooses another (docs/rp2040.md). Sleeping slower, the idle task raises a flag before
   ** each WFI, and the interrupt that wakes it raises the speed to the maximum as it enters
   ** (_OSIOHandler), so that little runs slow; the loop sets the sleep speed again each
   ** time the idle task resumes. Only the idle task is sped up that way: the kernel counts
   ** the work of an interrupted task at the speed it ran at.
   ** The comparison with OS_MAX_SPEED is made in C, not by the preprocessor: this file is
   ** read before the port defines the operating points, where an undefined name would
   ** count as 0. The compiler drops the dead branch. */
   #ifndef OS_SLEEP_SPEED
      #define OS_SLEEP_SPEED OS_MAX_SPEED
   #endif
   #ifndef _ASM_
      extern volatile BOOL _OSIdleAsleep;
   #endif
   #define _OSSleep() while (TRUE) { \
                         OSSetProcessorSpeed(OS_SLEEP_SPEED); \
                         if (OS_SLEEP_SPEED != OS_MAX_SPEED) \
                            _OSIdleAsleep = TRUE; \
                         __asm volatile ("WFI" ::: "memory"); \
                      };
#else
   #define _OSSleep() while (TRUE) { __asm volatile ("WFI" ::: "memory"); };
#endif

/* _OSScheduleTask: Generates a PendSV exception, which will interrupt and proceed at the
** lowest interrupt priority to handler _OSContextSwapHandler (defined in assembler in
** Escapement_CortexMx_a.S). Sets bit PENDSVSET of ICSR (0xE000ED04). */
/* Note that the write bits of ICSR take effect only if they are set, hence an assignment
** is the proper way to set a bit.
** The barriers are not decoration. Pending an exception does not take it: the write has
** to reach the NVIC, and the processor has to see the pending state before it executes
** what follows. Callers such as OSEndTask depend on never returning, and an optimised
** epilogue puts the return one instruction after the store — the task then returns to an
** EXC_RETURN value in LR from thread mode, which faults with INVPC. */
#define _OSScheduleTask() \
   do { \
     *((volatile UINT32 *)0xE000ED04) = 0x10000000; \
     __asm volatile ("dsb" ::: "memory"); \
     __asm volatile ("isb" ::: "memory"); \
   } while (0)

/* _OSGenerateSoftTimerInterrupt: Called by the timer peripheral to generate a SysTick
** exception, which will interrupt and continue with a smaller priority to handler
** _OSTimerInterruptHandler defined in EscapementHard.c, EscapementSoft.c or
** EscapementHardPA.c. Sets bit PENDSTSET
** of ICSR (0xE000ED04). (See note in _OSScheduleTask) */
#define _OSGenerateSoftTimerInterrupt() \
   do { \
     *((volatile UINT32 *)0xE000ED04) = 0x4000000; \
     __asm volatile ("dsb" ::: "memory"); \
     __asm volatile ("isb" ::: "memory"); \
   } while (0)

/* _OSClearSoftTimerInterrupt: Called by _OSTimerInterruptHandler bound to the SysTick
** exception to remove its interrupt pending status. In other words, a new SysTick excep-
** tion can be raised. Sets bit PENDSTCLR of ICSR (0xE000ED04). (See note in _OSSchedule-
** Task) */
#define _OSClearSoftTimerInterrupt() (*((volatile UINT32 *)0xE000ED04) = 0x2000000)

#endif /* _ESCAPEMENT_CORTEXMX_H_ */
