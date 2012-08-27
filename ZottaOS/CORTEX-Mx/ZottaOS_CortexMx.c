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
/* File ZottaOS_CortexMx.c: Contains functions that are common to any Cortex-Mx family
** of microcontrollers. The functions defined here are divided into 2 parts. The first
** part defines the system exceptions. The second part contains functions to support
** dynamic memory allocations.
** Platform version: All Cortex-Mx based microcontrollers.
** Version date: March 2012
** Authors: MIS-TIC
*/

#include "ZottaOS.h"


/* SYSTEM INTERRUPTS ----------------------------------------------------------------- */
extern void *_OSEndRAM; // First invalid address marking the end of the RAM memory
                        // (defined in linker file).
extern void _OSContextSwapHandler(void); // defined in ZottaOS_CortexMx_a.S

void _OSResetHandler(void); // Must be accessible by the linker file
static void NMIException(void);
static void HardFaultException(void);
static void MemManageException(void);
static void BusFaultException(void);
static void UsageFaultException(void);
static void SVCHandler(void);
static void DebugMonitor(void);
static void SoftTimerInterrupt(void);

static void FinalizeContextSwitchPreparation(void);

/* Cortex-Mx interrupt vector table and reset (see Cortex-Mx Technical Reference Manual,
** e.g. Sections 5.2 and 5.9.1, pages 5-3 and 5-19 for Cortex-M3) located at address 0.
** After the processor exits reset, it reads address 0 to find the initial SP value that
** it loads before proceeding to the content of address 4 to branch into the reset ISR
** _OSResetHandler. */
__attribute__ ((section(".isr_vector_general")))
void (* const CortexMxVectorTable[])(void) =
{
  // Initial stack pointer value, positioned at address 0
  (void (* const)(void))((UINTPTR)&_OSEndRAM - OSMALLOC_INTERNAL_HEAP_SIZE),
  _OSResetHandler,       // Reset, invoked on power up and warm resets
  NMIException,
  HardFaultException,
  MemManageException,
  BusFaultException,
  UsageFaultException,
  0, 0, 0, 0,            // Positions 7 to 10 are reserved
  SVCHandler,
  DebugMonitor,
  0,                     // Position 13 is reserved
  _OSContextSwapHandler, // PendSV exception defined in ZottaOS_CortexMx_a.S
  SoftTimerInterrupt     // SysTick
                         // External interrupts start after SysTick, at position 16
};


/* SYSTEM INTERRUPT HANDLERS */
/* ZottaOS requires a timer that can generate interruptions at variable intervals to
** determine the moments at which the various tasks will be carried out. The timer
** interrupts must have a higher priority level than all peripheral interruptions whose
** processing is longer than the time needed to do a full wraparound of the timer count-
** er. If this condition is not met, the internal temporal timings will be wrong. If the
** processing time a peripheral interrupt can be done in less time than it takes to wrap-
** around the timer, the priority level of the peripheral can be higher compared to that
** of the timer.
** To reduce the latency caused by the timer interrupts onto peripheral interrupts, the
** timer interrupt generates a software interruption (software interruption of the timer)
** whose priority is weaker. This software interrupt (_OSTimerInterruptHandler) basically
** processes task arrival times.
** A timer interrupt is thus done in 2 stages: (1) recuperate the interruption, adjust the
** time and then (2) propagate it to another at a weaker priority level for its processing.

** SUMMARY OF THE INTERRUPT PRIORITY LEVELS
**   HIGHEST PRIORITY LEVEL
**      Peripheral interrupt handlers (whose execution time cannot be longer than the
**           to wraparound the timer counter, otherwise the temporal basis will be lost.)
**      Timer interrupt handler (Fixed by TIMER_PRIORITY in NTRTOS_CortexMx.c) beber/claude
**      Peripheral interrupt handlers (whose execution time can be longer than the wrap-
**           around of the timer. The latency of these interrupts is increased because of
**           possible interrupt executions.)
**      Software timer interrupt handler (Lowest priority level + 1.) (SysTick is used as
**           a software interrupt for ZottaOS)
**      Context switch interrupt handler (executed at the lowest level of priority.)
**           (PendSV)
**   LOWEST PRIORITY LEVEL */

/* _OSReset_Handler: This is the code that gets called when the processor first starts
** execution following a reset event. Only the absolutely necessary set is performed,
** after which the application supplied main() routine is called. */
void _OSResetHandler(void)
{
  #if defined(CORTEX_M3) || defined(CORTEX_M4)
     /* CortexM3 registers setting the number interrupt priority levels. */
     #define AIRCR *((UINT32 *)0xE000ED0C) // Application Interrupt/Reset Control Register
  #endif
  #define SHPR *((UINT32 *)0xE000ED20)     // System Handlers 14-15 Priority Register
  /* Segment start and end address defined in linker file */
  extern UINT32 _etext; // Global variables in flash memory different than 0.
  extern UINT32 _data;  // RAM start location for the above globals
  extern UINT32 _edata; // RAM end location of previous globals
  extern UINT32 _bss;   // Global variables that are initialized to 0 and stored in RAM.
  extern UINT32 _ebss;  // RAM end address of the above.
  UINT32 *pulSrc, *pulDest;
  asm("CPSID I");       // Disable all interrupt
  /* Copy the data segment initializers from flash to SRAM */
  pulSrc = &_etext;
  for (pulDest = &_data; pulDest < &_edata; *(pulDest++) = *(pulSrc++));
  /* Zero fill the bss segment  */
  for (pulDest = &_bss; pulDest < &_ebss; *(pulDest++) = 0);
  /* Initialize interrupts priority system */
  #if defined(CORTEX_M3) || defined(CORTEX_M4)
     AIRCR = 0x05FA0000 | (PRIGROUP << 8); // Set preemption priority and subpriority
     switch (PRIGROUP) {
        case 0:   // indicates 7 bits of preemption priority, 1 bit of subpriority
           SHPR = 0xFCFE0000;  /* SysTick = FC;  PendSV = FE = lowest priority group */
           #ifdef DEBUG_MODE   /* Other peripherals should occupy the first 126 priority */
              if (TIMER_PRIORITY >= 0xFC)      /* levels */
                 while (TRUE);
           #endif
           break;
        case 1:   // indicates 6 bits of preemption priority, 2 bits of subpriority
           SHPR = 0xF8FC0000;  /* Other peripherals should occupy the first 62 priority */
           #ifdef DEBUG_MODE   /* levels */
              if (TIMER_PRIORITY >= 0xF8)
                 while (TRUE);
           #endif
           break;
        case 2:   // indicates 5 bits of preemption priority, 3 bits of subpriority
           SHPR = 0xF0F80000;  /* Other peripherals should occupy the first 30 priority */
           #ifdef DEBUG_MODE   /* levels */
              if (TIMER_PRIORITY >= 0xF0)
                 while (TRUE);
           #endif
           break;
        case 3:   // indicates 4 bits of preemption priority, 4 bits of subpriority
           SHPR = 0xE0F00000;  /* Other peripherals should occupy the first 14 priority */
           #ifdef DEBUG_MODE   /* levels */
              if (TIMER_PRIORITY >= 0xE0)
                 while (TRUE);
           #endif
           break;
        case 4:   // indicates 3 bits of preemption priority, 5 bits of subpriority
           SHPR = 0xC0E00000;  /* Other peripherals should occupy the first 6 priority */
           #ifdef DEBUG_MODE   /* levels */
              if (TIMER_PRIORITY >= 0xC0)
                 while (TRUE);
           #endif
           break;
        case 5:   // indicates 2 bits of preemption priority, 6 bits of subpriority
           SHPR = 0x80C00000;  /* Other peripherals should occupy the first 2 priority */
           #ifdef DEBUG_MODE   /* levels */
              if (TIMER_PRIORITY >= 0x80)
                 while (TRUE);
           #endif
           break;
        default:
           #ifdef DEBUG_MODE
              while (TRUE); // Error (at least 4 levels of preemption priority is needed)
           #endif
           break;
     }
  #elif defined(CORTEX_M0)
     SHPR = (LOWEST_PRIORITY_LEVEL - 1) << 30 | (LOWEST_PRIORITY_LEVEL) << 22;
  #endif
  /* Call the application's entry point */
  asm("BL main");
} /* end of _OSResetHandler */


/* NMIException: Nonmaskable Interrupt (NMI) exception. Unused by ZottaOS. */
void NMIException(void)
{
  while (TRUE);
} /* end of NMIException */


/* HardFaultException: A hard fault is an exception that is caused by usage faults, bus
** faults, and memory management faults if their handler cannot be executed. */
void HardFaultException(void)
{
  while (TRUE);
} /* end of HardFaultException */


/* MemManageException: A memory management fault is an exception the is caused by a Memory
** Protection Unit (MPU) violation or by certain illegal accesses (e.g., trying to execute
** code from nonexecutable memory regions or writing to read-only regions). This exception
** can be triggered even if no MPU is present. */
void MemManageException(void)
{
  while (TRUE);
} /* end of MemManageException */


/* BusFaultException: A bus fault is an exception that occurs because of a memory related
** fault. This can be (1) an attempt to access an invalid memory region, (2) an attempt to
** a transfer data to an unready device, e.g., accessing SDRAM without initializing its
** controller, (3) a data memory transaction with a transfer size not supported by the
** target device or with a device not accepting the transfer, e.g. because of a privileged
** access level. */
void BusFaultException(void)
{
  while (TRUE);
} /* end of BusFaultException */


/* UsageFaultException: A usage fault is an exception resulting from of an ill instruction
** execution. This includes an undefined instruction or illegal unaligned access errors. */
void UsageFaultException(void)
{
  while (TRUE);
} /* end of UsageFaultException */


/* SVCHandler: A supervisor call (SVC) is an exception that is triggered by the SVC in-
** struction, usually to access OS functionalities. Under ZottaOS, a task as full access
** to the kernel functions and device drivers, and hence SVCs are not defined. */
void SVCHandler(void)
{
  while (TRUE);
} /* end of SVCHandler */


/* DebugMonitor: Debug monitor exception used along with tools allowing breakpoints, watch
** points, or external debug requests. Unused in ZottaOS. */
void DebugMonitor(void)
{
  while (TRUE);
} /* end of DebugMonitor */
/* end of the Cortex-Mx system interrupt handler definitions */


/* SoftTimerInterrupt: Low priority ISR bound to a SysTick exception. Cortex-Mx processors
** come along a simple 24-bit count down timer that can only be used to generate ticks at
** regular periods but cannot be used as interval timer without loosing the time reference.
** This is because when a new interval is written into the SysTick reload value register,
** its value takes effect only when its previously loaded value reached 0. ZottaOS there-
** fore uses one of the peripheral timers provided by the microcontroller as its internal
** timer and raises a SysTick exception whenever this timer creates an interrupt (i.e.,
** when it overflows or detects a comparator match). The SysTick exception is executed at
** the lowest possible priority level (immediately above PendSV) so that the peripheral
** devices have minimum latencies.*/
void SoftTimerInterrupt(void)
{
  extern void _OSTimerInterruptHandler(void);  /* Defined in the generic kernel */
  FinalizeContextSwitchPreparation();   // Correct possible inconsistent queue state
  _OSTimerInterruptHandler(); // Jump to to the generic service routine of the timer
  // The CLREX instruction is done in _OSContextSwapHandler (PendSV handler) prior to
  // returning to a user task.
} /* end of SoftTimerInterrupt */


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
  #if defined(ZOTTAOS_VERSION_SOFT) && SCHEDULER_REAL_TIME_MODE != DEADLINE_MONOTONIC_SCHEDULING
     #define STATE_INIT          0x00
     #define STATE_ACTIVATE      0x10
  #endif
  extern MinimalTCB *_OSQueueHead;
  #if defined(ZOTTAOS_VERSION_SOFT) && SCHEDULER_REAL_TIME_MODE != DEADLINE_MONOTONIC_SCHEDULING
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
  #if defined(ZOTTAOS_VERSION_SOFT) && SCHEDULER_REAL_TIME_MODE != DEADLINE_MONOTONIC_SCHEDULING
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


/* _OSIOHandler: Default interrupt handler that calls the appropriate handler depending
** upon the interrupt source. Note that if no handler is associated with the interrupt
** source, the processor may go haywire after it proceeds to an invalid address. */
void _OSIOHandler(void)
{
  #if defined(CORTEX_M0)
     extern BOOL _OSLLReserveBit;
  #endif
  /* Definition of a minimal peripheral descriptor to retrieve the peripheral interrupt
  ** handler routine */
  typedef struct MinimalIODescriptor {
     void (*PeripheralInterruptHandler)(struct MinimalIODescriptor *);
  } MinimalIODescriptor;
  extern void *_OSTabDevice[];
  MinimalIODescriptor *peripheralIODescriptor;
  /* Retrieve the specific handler from _OSTabDevice */
  peripheralIODescriptor = _OSTabDevice[(*((UINT32 *)0xE000ED04) & 0x1FF) - 16];
  #ifdef DEBUG_MODE
     if (peripheralIODescriptor == NULL) {
        _OSDisableInterrupts();
        while (TRUE); // referring to an inexistent ISR descriptor.
     }
  #endif
  /* Call the specific handler */
  peripheralIODescriptor->PeripheralInterruptHandler(peripheralIODescriptor);
  // Make all pending SC() fail
  #if defined(CORTEX_M3) || defined(CORTEX_M4)
     __asm("CLREX;"); 
  #elif defined(CORTEX_M0)
     _OSLLReserveBit = FALSE;
  #endif
} /* end of _OSIOHandler */


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
**  (2) There is no memory corruption in case of a stack overflow;
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
void *OSMalloc(UINT16 size)
{
  extern void *_OSStackBasePointer;
  if (_OSStackBasePointer == NULL)
     _OSStackBasePointer = &_OSEndRAM; /* initialize to end of ram */
  if (size % 4 != 0)
     size += 4 - size % 4;
  _OSStackBasePointer = (UINT8 *)_OSStackBasePointer - size;
  /* Check that requested memory block doesn't overlap main's activation record. */
  if (_OSStackBasePointer <= (void *)((UINTPTR)&_OSEndRAM - OSMALLOC_INTERNAL_HEAP_SIZE)) {
     _OSStackBasePointer = (UINT8 *)_OSStackBasePointer + size;
     #ifdef DEBUG_MODE
        while (TRUE); // Memory overflow: increase OSMALLOC_INTERNAL_HEAP_SIZE in the
        return 0;     // linker file. (You can retrieve this file name from the makefile)
     #else
        return NULL;
     #endif
  }
  else
     return _OSStackBasePointer;
} /* end of OSMalloc */
