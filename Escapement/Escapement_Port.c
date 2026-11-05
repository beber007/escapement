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
/* File Escapement_Port.c: The parts of the processor layer that every architecture
** shares: the soft timer interrupt's handler, with the completion of a context switch
** left half prepared, and the dynamic memory allocation. They were the second half of
** Escapement_CortexMx.c, and Escapement_RISCV.c had a copy of them (issue #77); each
** architecture's layer now calls them from here. Every port links this file right after
** that layer, in the place they held in it.
** Platform version: All Cortex-Mx based microcontrollers, the Hazard3 cores of the RP2350.
*/

#include "Escapement.h"

extern void *_OSEndRAM; // First invalid address marking the end of the RAM memory
                        // (defined in linker file).

void _OSSoftTimerInterrupt(void); // Bound to SysTick (Escapement_CortexMx.c) or to
                                  // SPARE_IRQ_0 (Escapement_RISCV.c)
static void FinalizeContextSwitchPreparation(void);


/* _OSSoftTimerInterrupt: Low priority ISR bound to a SysTick exception. Cortex-Mx processors
** come with a simple 24-bit count down timer that can only be used to generate ticks at
** regular periods but cannot be used as interval timer without losing the time reference.
** This is because when a new interval is written into the SysTick reload value register,
** its value takes effect only when its previously loaded value reached 0. Escapement there-
** fore uses one of the peripheral timers provided by the microcontroller as its internal
** timer and raises a SysTick exception whenever this timer creates an interrupt (i.e.,
** when it overflows or detects a comparator match). The SysTick exception is executed at
** the lowest possible priority level (immediately above PendSV) so that the peripheral
** devices have minimum latencies.
** On the Hazard3 of the RP2350 the same handler is that of SPARE_IRQ_0, forced pending
** at the lowest level of the Xh3irq (_OSGenerateSoftTimerInterrupt, Escapement_RISCV.h),
** which _OSIOHandler of Escapement_RISCV.c calls. */
void _OSSoftTimerInterrupt(void)
{
  extern void _OSTimerInterruptHandler(void);  /* Defined in the generic kernel */
  #ifdef ESCAPEMENT_MEASURE_WCET
     _OSMeasurePause();                 // the scheduler's time is charged to no task
  #endif
  FinalizeContextSwitchPreparation();   // Correct possible inconsistent queue state
  OSTrace(OS_TRACE_SOFT_ENTER,0,0);
  _OSTimerInterruptHandler(); // Jump to the generic service routine of the timer
  OSTrace(OS_TRACE_SOFT_LEAVE,0,0);
  // The CLREX instruction is done in _OSContextSwapHandler (PendSV handler) prior to
  // returning to a user task; the Hazard3 clears the reservation itself on the way out
  // of the trap.
} /* end of _OSSoftTimerInterrupt */


typedef struct MinimalTCB {    // Template of the 1st fields of a TCB or ETCB
   struct MinimalTCB *Next[2]; // Next TCB in the list where this task is located
                               // [0]: ready queue link, [1]: arrival queue link
   UINT8 TaskState;            // Current state of the task
} MinimalTCB;
extern MinimalTCB *_OSActiveTask;

/* FinalizeContextSwitchPreparation: On entry of an interrupt handler, we need to assert
** that an application task is not in the middle of preparing to do a context switch. If
** this is the case, we need to supersede its actions and never return to that task
** instance.*/
void FinalizeContextSwitchPreparation(void)
{
  #define READYQ        0
  #define STATE_ZOMBIE  0x02
  #if defined(ESCAPEMENT_VERSION_SOFT) && SCHEDULER_REAL_TIME_MODE != DEADLINE_MONOTONIC_SCHEDULING
     #define STATE_INIT          0x00
     #define STATE_ACTIVATE      0x10
  #endif
  extern MinimalTCB *_OSQueueHead;
  #if defined(ESCAPEMENT_VERSION_SOFT) && SCHEDULER_REAL_TIME_MODE != DEADLINE_MONOTONIC_SCHEDULING
     extern MinimalTCB *_OSQueueTail;
  #endif
  extern BOOL _OSNoSaveContext;
  _OSActiveTask = _OSQueueHead->Next[READYQ];
  /* A finished task is in the middle of removing itself from the ready queue if its cur-
  ** rent state is set to STATE_ZOMBIE. If this is the case, the ready queue may be in an
  ** incoherent state and needs to be adjusted. The context of this task should also not
  ** be saved. */
  if (_OSActiveTask->TaskState & STATE_ZOMBIE) {
     /* Make the ready queue coherent, i.e. finish the work started by the interrupted
     ** task and don't save its context since it is terminated. */
     _OSActiveTask = _OSQueueHead->Next[READYQ] = _OSActiveTask->Next[READYQ];
     _OSNoSaveContext = TRUE; /* Don't save the context of the interrupted task */
  }
  #if defined(ESCAPEMENT_VERSION_SOFT) && SCHEDULER_REAL_TIME_MODE != DEADLINE_MONOTONIC_SCHEDULING
     else if (_OSActiveTask->TaskState & STATE_ACTIVATE) {
        /* Yes: The operations done by the task doing the promotion are
        **   (1)   OSQueueTail->Next[READYQ] = _OSActiveTask->Next[READYQ];
        **   (2)   _OSActiveTask->Next[READYQ] = OSQueueTail;
        **   (3)   _OSActiveTask->TaskState = STATE_INIT;
        ** So if OSQueueTail != _OSActiveTask->Next[READYQ], we need to do steps (1-3);
        **  otherwise, (2) is already done and we need only step (3) */
        if (_OSActiveTask->Next[READYQ] != _OSQueueTail) {
           _OSQueueTail->Next[READYQ] = _OSActiveTask->Next[READYQ];
           _OSActiveTask->Next[READYQ] = _OSQueueTail;
        }
        _OSActiveTask->TaskState = STATE_INIT;
        _OSNoSaveContext = TRUE; /* Don't save the context of the interrupted task */
     }
   #endif
} /* end of FinalizeContextSwitchPreparation */


/* DYNAMIC MEMORY ALLOCATIONS -------------------------------------------------------- */
/* Since most applications do all their dynamic memory allocations at initialization time
** and these allocations are never reclaimed, the malloc function provided by libc is
** overkill because there is no need to manage freed blocks. To correct this state of af-
** fairs, we use the following scheme: During initialization time, i.e., when main is ac-
** tive and the first task has not yet executed, the run-time stack starts at a specific
** address and all functions that are called to prepare the kernel are stacked below main.
** During this time, any dynamic memory block allocated to the application is done at the
** opposite end starting with the highest possible address. Once the kernel starts its
** execution, the run-time stack is adjusted so that it begins immediately below the al-
** lotted blocks.
** As well as providing a useful malloc function for the user application, this approach
** has several interesting advantages:
**  (1) Code size is definitively smaller and faster compared to the usual thread-safe
**      malloc/free pair;
**  (2) A stack overflow does not corrupt the allocated blocks, which lie above the
**      stack. It does reach the globals below it: on the Cortex-M33, MSPLIM makes it
**      fault there instead (_OSResetHandler), as the PMP makes it on the Hazard3
**      (Escapement_RISCV.c); the Cortex-M0+ has no such limit, and
**      writes over them unseen. ZottaOS's comment claimed no corruption at all;
**  (3) The run-time stack occupies its largest possible size.
**
** OSMalloc: This function can only be used while the main function is active. Allocation
** is done above the stack in a RAM memory size of OSMALLOC_INTERNAL_HEAP_SIZE bytes; the
** run-time stack of main starts immediately below the reserved memory for the allocated
** memory blocks. By proceeding in this way, all global memory blocks are kept at one end
** of RAM memory while leaving the remaining memory for the run-time stack.
** Parameter: (UINT16) requested memory block size in bytes.
** Returned value: (void *) Pointer to the base of the newly allocated block, and NULL in
**    case of an error, i.e., when more than OSMALLOC_INTERNAL_HEAP_SIZE bytes of memory
**    is consumed by dynamic allocations done in main.
** OSMalloc should NEVER be called from an application task or by a user defined ISR. */
void *OSMalloc(UINT16 request)
{
  extern void *_OSStackBasePointer;
  /* Rounded up to a word in 32 bits: in the 16 of the parameter, 65,533 to 65,535 bytes
  ** came to 0 and the block handed out was the one before (a review, 2026-10-11). */
  UINT32 size = ((UINT32)request + 3u) & ~3u;
  if (_OSStackBasePointer == NULL)
     _OSStackBasePointer = &_OSEndRAM; /* initialize to end of ram */
  _OSStackBasePointer = (UINT8 *)_OSStackBasePointer - size;
  /* Check that requested memory block doesn't overlap main's activation record. */
  // cppcheck-suppress arithOperationsOnVoidPointer ; the address is an integer here too
  if (_OSStackBasePointer <= (void *)((UINTPTR)&_OSEndRAM - OSMALLOC_INTERNAL_HEAP_SIZE)) {
     _OSStackBasePointer = (UINT8 *)_OSStackBasePointer + size;
     #ifdef DEBUG_MODE
        while (TRUE); // Memory overflow: increase OSMALLOC_INTERNAL_HEAP_SIZE in
        return 0;     // Escapement_Config.h
     #else
        return NULL;
     #endif
  }
  else
     return _OSStackBasePointer;
} /* end of OSMalloc */
