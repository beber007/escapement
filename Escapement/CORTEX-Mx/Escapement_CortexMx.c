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
/* File Escapement_CortexMx.c: Contains functions that are common to any Cortex-Mx family
** of microcontrollers: the system exceptions and the dispatch of the peripheral
** interrupts. The second part ZottaOS had here, the soft timer interrupt's handler and
** the dynamic memory allocation, is in Escapement_Port.c, which the RISC-V layer shares.
** Platform version: All Cortex-Mx based microcontrollers.
** Version date: March 2012
*/

#include "Escapement.h"


/* SYSTEM INTERRUPTS ----------------------------------------------------------------- */
extern void *_OSEndRAM; // First invalid address marking the end of the RAM memory
                        // (defined in linker file).
extern void _OSContextSwapHandler(void); // defined in Escapement_CortexMx_a.S

void _OSResetHandler(void); // Must be accessible by the linker file
static void NMIException(void);
static void HardFaultException(void);
static void MemManageException(void);
static void BusFaultException(void);
static void UsageFaultException(void);
static void SVCHandler(void);
static void DebugMonitor(void);
extern void _OSSoftTimerInterrupt(void); // defined in Escapement_Port.c


/* Cortex-Mx interrupt vector table and reset (see Cortex-Mx Technical Reference Manual,
** e.g. Sections 5.2 and 5.9.1, pages 5-3 and 5-19 for Cortex-M3) located at address 0.
** After the processor exits reset, it reads address 0 to find the initial SP value that
** it loads before proceeding to the content of address 4 to branch into the reset ISR
** _OSResetHandler. On the RP2040 and RP2350 an image in SRAM has the table follow the
** entry code there (RP2040_SRAM.ld, RP2350_SRAM.ld), and each application points VTOR
** at it in main; an image in the flash (make FLASH=1) has it in the flash, where the boot
** finds it. */
__attribute__ ((section(".isr_vector_general")))
void (* const CortexMxVectorTable[])(void) =
{
  // Initial stack pointer value, positioned at address 0
  // The address is made an integer before the subtraction, which cppcheck misreads.
  // cppcheck-suppress arithOperationsOnVoidPointer
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
  _OSContextSwapHandler, // PendSV exception defined in Escapement_CortexMx_a.S
  _OSSoftTimerInterrupt  // SysTick, defined in Escapement_Port.c
                         // External interrupts start after SysTick, at position 16
};


/* SYSTEM INTERRUPT HANDLERS */
/* Escapement requires a timer that can generate interruptions at variable intervals to
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
**      Timer interrupt handler (Fixed by TIMER_PRIORITY in Escapement_Config.h)
**      Peripheral interrupt handlers (whose execution time can be longer than the wrap-
**           around of the timer. The latency of these interrupts is increased because of
**           possible interrupt executions.)
**      Software timer interrupt handler (Lowest priority level + 1.) (SysTick is used as
**           a software interrupt for Escapement)
**      Context switch interrupt handler (executed at the lowest level of priority.)
**           (PendSV)
**   LOWEST PRIORITY LEVEL */

/* _OSResetHandler: This is the code that gets called when the processor first starts
** execution following a reset event. Only the absolutely necessary set is performed,
** after which the application supplied main() routine is called. */
void _OSResetHandler(void)
{
  #if defined(CORTEX_M3) || defined(CORTEX_M4) || defined(CORTEX_M33)
     /* CortexM3 registers setting the number interrupt priority levels. */
     #define AIRCR *((volatile UINT32 *)0xE000ED0C) // Application Interrupt/Reset Control Reg.
  #endif
  #define SHPR *((volatile UINT32 *)0xE000ED20) // System Handlers 14-15 Priority Register
  /* Segment start and end address defined in linker file */
  extern UINT32 _sidata; // Load address of the initialized globals, given by the
                         // linker script; equal to _data when the image is linked
                         // into RAM, in which case the copy below does nothing.
  extern UINT32 _data;  // RAM start location for the above globals
  extern UINT32 _edata; // RAM end location of previous globals
  extern UINT32 _bss;   // Global variables that are initialized to 0 and stored in RAM.
  extern UINT32 _ebss;  // RAM end address of the above.
  UINT32 *pulSrc, *pulDest;
  asm("CPSID I");       // Disable all interrupt
  /* Copy the data segment initializers from flash to SRAM */
  pulSrc = &_sidata;
  /* The linker's symbols delimit one area: comparing their addresses is what they are
  ** for, which cppcheck takes for two unrelated objects. */
  // cppcheck-suppress comparePointers
  for (pulDest = &_data; pulDest < &_edata; )
     *(pulDest++) = *(pulSrc++);
  /* Zero fill the bss segment  */
  // cppcheck-suppress comparePointers
  for (pulDest = &_bss; pulDest < &_ebss; )
     *(pulDest++) = 0;
  #if defined(CORTEX_M33)
     /* The one stack grows down from below the blocks OSMalloc hands out toward the
     ** globals, which end at _ebss. ARMv8-M checks the main stack pointer against MSPLIM
     ** on every change of it, and faults (UsageFault STKOF, a HardFault here) rather than
     ** write below it: an overflow stops the image instead of corrupting the kernel's
     ** variables unseen. MSPLIM ignores its three low bits; _ebss is taken up to 8. */
     __asm volatile ("MSR MSPLIM, %0" :: "r" (((UINT32)&_ebss + 7) & ~7u) : "memory");
  #elif defined(CORTEX_M0)
     /* The Cortex-M0+ has no stack limit, but the RP2040's has an MPU of 8 regions: one,
     ** no access and no execution, over the 1 KB the linker script keeps between the
     ** globals and the stack, faults the first store of a stack run past its end. A frame
     ** of 1 KB or more may step over it, and GCC may make one by inlining a function
     ** into itself. Every other address keeps the default map (PRIVDEFENA), the kernel
     ** and its tasks running privileged. HFNMIENA left at 0 turns the MPU off at the
     ** HardFault's priority: its stacking lands in the region, not in the globals, and
     ** the core runs the handler rather than lock up (on the Pico, 2026-10-05). */
     extern UINT32 _OSStackGuard;
     #define MPU_CTRL *((volatile UINT32 *)0xE000ED94)
     #define MPU_RNR  *((volatile UINT32 *)0xE000ED98)
     #define MPU_RBAR *((volatile UINT32 *)0xE000ED9C)
     #define MPU_RASR *((volatile UINT32 *)0xE000EDA0)
     MPU_RNR = 0;
     MPU_RBAR = (UINT32)&_OSStackGuard;
     MPU_RASR = (1u << 28) | (9u << 1) | 1u;   // XN, AP no access, 2^(9+1) bytes, enabled
     MPU_CTRL = 5u;                            // PRIVDEFENA, ENABLE
     __asm volatile ("DSB\n ISB" ::: "memory");
  #endif
  /* Initialize interrupts priority system */
  #if defined(CORTEX_M3) || defined(CORTEX_M4) || defined(CORTEX_M33)
     /* On ARMv8-M with its security extension, the bits of AIRCR this write clears keep
     ** faults and priorities as they are after reset, in the Secure state (BFHFNMINS,
     ** PRIS). */
     AIRCR = 0x05FA0000 | (PRIGROUP << 8); // Set preemption priority and subpriority
     /* TIMER_PRIORITY is a preemption level, which the ports shift into the upper bits of
     ** a priority byte; SysTick's byte below holds the level PRIGROUP + 1 bits up. Under
     ** DEBUG_MODE, the timer must come before SysTick's level: 0xFC >> 1 = 126, 0xF8 >> 2
     ** = 62, and so on. */
     switch (PRIGROUP) {
        case 0:   // indicates 7 bits of preemption priority, 1 bit of subpriority
           SHPR = 0xFCFE0000;  /* SysTick = FC;  PendSV = FE = lowest priority group */
           #ifdef DEBUG_MODE   /* Other peripherals should occupy the first 126 priority */
              if (TIMER_PRIORITY >= 126)      /* levels */
                 while (TRUE);
           #endif
           break;
        case 1:   // indicates 6 bits of preemption priority, 2 bits of subpriority
           SHPR = 0xF8FC0000;  /* Other peripherals should occupy the first 62 priority */
           #ifdef DEBUG_MODE   /* levels */
              if (TIMER_PRIORITY >= 62)
                 while (TRUE);
           #endif
           break;
        case 2:   // indicates 5 bits of preemption priority, 3 bits of subpriority
           SHPR = 0xF0F80000;  /* Other peripherals should occupy the first 30 priority */
           #ifdef DEBUG_MODE   /* levels */
              if (TIMER_PRIORITY >= 30)
                 while (TRUE);
           #endif
           break;
        case 3:   // indicates 4 bits of preemption priority, 4 bits of subpriority
           SHPR = 0xE0F00000;  /* Other peripherals should occupy the first 14 priority */
           #ifdef DEBUG_MODE   /* levels */
              if (TIMER_PRIORITY >= 14)
                 while (TRUE);
           #endif
           break;
        case 4:   // indicates 3 bits of preemption priority, 5 bits of subpriority
           SHPR = 0xC0E00000;  /* Other peripherals should occupy the first 6 priority */
           #ifdef DEBUG_MODE   /* levels */
              if (TIMER_PRIORITY >= 6)
                 while (TRUE);
           #endif
           break;
        case 5:   // indicates 2 bits of preemption priority, 6 bits of subpriority
           SHPR = 0x80C00000;  /* Other peripherals should occupy the first 2 priority */
           #ifdef DEBUG_MODE   /* levels */
              if (TIMER_PRIORITY >= 2)
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
     /* Unsigned literals: with LOWEST_PRIORITY_LEVEL at 3, shifting 2 left by 30 in
     ** a signed int overflows, which the standard leaves undefined. */
     SHPR = ((UINT32)(LOWEST_PRIORITY_LEVEL - 1) << 30) |
            ((UINT32)(LOWEST_PRIORITY_LEVEL) << 22);
     #ifdef DEBUG_MODE   /* The timer must come before SysTick, at LOWEST_PRIORITY_LEVEL - 1 */
        if (TIMER_PRIORITY >= LOWEST_PRIORITY_LEVEL - 1)
           while (TRUE);
     #endif
  #endif
  /* Call the application's entry point */
  asm("BL main");
} /* end of _OSResetHandler */


/* NMIException: Nonmaskable Interrupt (NMI) exception. Unused by Escapement. */
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


/* MemManageException: A memory management fault is an exception that is caused by a Memory
** Protection Unit (MPU) violation or by certain illegal accesses (e.g., trying to execute
** code from nonexecutable memory regions or writing to read-only regions). This exception
** can be triggered even if no MPU is present. */
void MemManageException(void)
{
  while (TRUE);
} /* end of MemManageException */


/* BusFaultException: A bus fault is an exception that occurs because of a memory related
** fault. This can be (1) an attempt to access an invalid memory region, (2) an attempt to
** transfer data to an unready device, e.g., accessing SDRAM without initializing its
** controller, (3) a data memory transaction with a transfer size not supported by the
** target device or with a device not accepting the transfer, e.g. because of a privileged
** access level. */
void BusFaultException(void)
{
  while (TRUE);
} /* end of BusFaultException */


/* UsageFaultException: A usage fault is an exception resulting from an illegal instruction
** execution. This includes an undefined instruction or illegal unaligned access errors. */
void UsageFaultException(void)
{
  while (TRUE);
} /* end of UsageFaultException */


/* SVCHandler: A supervisor call (SVC) is an exception that is triggered by the SVC in-
** struction, usually to access OS functionalities. Under Escapement, a task has full access
** to the kernel functions and device drivers, and hence SVCs are not defined. */
void SVCHandler(void)
{
  while (TRUE);
} /* end of SVCHandler */


/* DebugMonitor: Debug monitor exception used along with tools allowing breakpoints, watch
** points, or external debug requests. Unused in Escapement. */
void DebugMonitor(void)
{
  while (TRUE);
} /* end of DebugMonitor */
/* end of the Cortex-Mx system interrupt handler definitions */


/* _OSIOHandler: Default interrupt handler that calls the appropriate handler depending
** upon the interrupt source. Note that if no handler is associated with the interrupt
** source, the processor may go haywire after it proceeds to an invalid address. */
#ifdef ESCAPEMENT_VERSION_HARD_PA
   volatile BOOL _OSIdleAsleep = FALSE;   /* raised by the idle task before its WFI */
#endif

void _OSIOHandler(void)
{
  #if defined(CORTEX_M0)
     extern volatile BOOL _OSLLReserveBit;
  #endif
  /* Definition of a minimal peripheral descriptor to retrieve the peripheral interrupt
  ** handler routine */
  typedef struct MinimalIODescriptor {
     void (*PeripheralInterruptHandler)(struct MinimalIODescriptor *);
  } MinimalIODescriptor;
  extern void *_OSTabDevice[];
  MinimalIODescriptor *peripheralIODescriptor;
  /* Retrieve the specific handler from _OSTabDevice */
  #ifdef ESCAPEMENT_VERSION_HARD_PA
     /* Woken from a slow sleep: run the handler, and the kernel after it, at full speed. */
     if (OS_SLEEP_SPEED != OS_MAX_SPEED && _OSIdleAsleep)
        _OSRaiseSpeedOnWake();
  #endif
  peripheralIODescriptor = _OSTabDevice[(*((volatile UINT32 *)0xE000ED04) & 0x1FF) - 16];
  #ifdef DEBUG_MODE
     if (peripheralIODescriptor == NULL) {
        _OSDisableInterrupts();
        while (TRUE); // referring to an inexistent ISR descriptor.
     }
  #endif
  /* Call the specific handler. A missing descriptor is a fault of the application, which
  ** DEBUG_MODE traps above; cppcheck takes that test for one that should hold here. */
  // cppcheck-suppress nullPointerRedundantCheck
  peripheralIODescriptor->PeripheralInterruptHandler(peripheralIODescriptor);
  // Make all pending SC() fail
  #if defined(CORTEX_M3) || defined(CORTEX_M4) || defined(CORTEX_M33)
     __asm volatile ("CLREX" ::: "memory");
  #elif defined(CORTEX_M0)
     _OSLLReserveBit = FALSE;
  #endif
} /* end of _OSIOHandler */

