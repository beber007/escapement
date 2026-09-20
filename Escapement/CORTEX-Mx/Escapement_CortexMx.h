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

/* Offsets the context switch reads out of a task control block. _OSContextSwapHandler is
** written in assembler and cannot see the C structure, so it addresses these fields by
** hand. The kernel variants check them with _Static_assert against their own TCB, which
** turns a silent mismatch into a build failure: the power-aware variant, for one, gives
** its TCB a third list link under the DRA and DR_OTE algorithms, which moves every field
** four bytes further along. */
#if defined(ESCAPEMENT_VERSION_SOFT) && SCHEDULER_REAL_TIME_MODE == DEADLINE_MONOTONIC_SCHEDULING
   #define OS_TCB_STATE_OFFSET     8
   #define OS_TCB_ENTRY_OFFSET    20
   #define OS_TCB_ARGUMENT_OFFSET 24
#else
   #define OS_TCB_STATE_OFFSET     8
   #define OS_TCB_ENTRY_OFFSET    16
   #define OS_TCB_ARGUMENT_OFFSET 20
#endif

/* OSCheckTCBLayout: Placed by each kernel variant right after its TCB definition. */
#ifndef _ASM_
   #define OSCheckTCBLayout() \
      _Static_assert(__builtin_offsetof(TCB,TaskState) == OS_TCB_STATE_OFFSET, \
                     "TaskState moved; Escapement_CortexMx_a.S reads it at another offset"); \
      _Static_assert(__builtin_offsetof(TCB,TaskCodePtr) == OS_TCB_ENTRY_OFFSET, \
                     "TaskCodePtr moved; Escapement_CortexMx_a.S reads it at another offset"); \
      _Static_assert(__builtin_offsetof(TCB,Argument) == OS_TCB_ARGUMENT_OFFSET, \
                     "Argument moved; Escapement_CortexMx_a.S reads it at another offset")
#endif

/* Non-blocking algorithms use a marker that needs to be part of the address. These algo-
** rithms operate in RAM and need an address bit that is never used. */
#define MARKEDBIT    0x80000000u
#define UNMARKEDBIT  0x7FFFFFFFu

/* _OSIOHandler: Interrupt service routine called whenever an IRQ is raised. */
void _OSIOHandler(void);

/* _OSEnableInterrupts and _OSDisableInterrupts: Macros changing the state of the special
** register PRIMASK.These are provided to allow portable code between different microcon-
** trollers.
** The memory clobber is what makes them critical sections as far as the compiler is
** concerned. Without it, masking interrupts only constrains the processor, not the code
** generator: reads and writes may legally be moved across the boundary, and what the
** section was protecting is then protected only by the compiler's goodwill. */
#define _OSEnableInterrupts()  __asm volatile ("CPSIE i" ::: "memory")
#define _OSDisableInterrupts() __asm volatile ("CPSID i" ::: "memory")

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
** _OSTimerInterruptHandler defined in EscapementHard.c or EscapementSoft.c. Sets bit PENDSTSET
** of ICSR (0xE000ED04). (See note in _OSScheduleTask) */
#define _OSGenerateSoftTimerInterrupt() \
   do { \
     *((volatile UINT32 *)0xE000ED04) = 0x4000000; \
     __asm volatile ("dsb" ::: "memory"); \
     __asm volatile ("isb" ::: "memory"); \
   } while (0)

/* _OSClearSoftTimerInterrupt: Called by _OSTimerInterruptHandler binded to the SysTick
** exception to remove its interrupt pending status. In other words, a new SysTick excep-
** tion can be raised. Sets bit PENDSTCLR of ICSR (0xE000ED04). (See note in _OSSchedule-
** Task) */
#define _OSClearSoftTimerInterrupt() (*((volatile UINT32 *)0xE000ED04) = 0x2000000)

#endif /* _ESCAPEMENT_CORTEXMX_H_ */
