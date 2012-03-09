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
/* File NTRTOS_STM32.c: Contains functions that are specific to the STM-32 family of
** microcontrollers. The functions defined here are divided into 2 parts. The first part
** contains functions to support the time and the timer. The second part consists of the
** the microcontroller's interrupt routines that start at position 16 of the interrupt
** table and that are specific to the microcontroller at hand.
** Version date: March 2012
** Authors: MIS-TIC
*/

#include "ZottaOS_Types.h"
#include "ZottaOS_CortexM3.h"
#include "ZottaOS_Interrupts.h"


/* ATOMIC INSTRUCTIONS --------------------------------------------------------------- */
/* OSUINT8_LL, OSUINT16_LL, OSINT16_LL, OSUINT32_LL and OSUINT32_LL: The LL functions are
** used in conjunction with their corresponding SC functions call to provide
** synchronization support for ZottaOS. The LL/SC pair of functions works very much like
** simple a get and a set function. The LL functions, in addition of returning the
** contents of a memory location, have the effect of setting a user transparent
** reservation bit. If this bit is still set when an SC function is executed, the store of
** SC occurs; otherwise the store fails and the specified memory location is left
** unchanged (see OSUINT8_SC, OSUINT16_SC, OSINT16_SC, OSUINT32_SC and OSINT32_SC).
** The LL function is semantically equivalent to the atomic execution of the following
** code:
**    TYPE OSTYPE_LL(TYPE *memAddr) {
**       reserveBit = TRUE;
**       return *memAddr;
**    }
** where TYPE can be one of UINT8, UINT16, INT16, UINT32 or INT32.
** Parameter: (TYPE *) Address to a memory location that holds the value to read, where
** TYPE can be one of UINT8, UINT16, INT16, UINT32 or INT32.
** Returned value: (TYPE) The contents stored in the memory location specified by the pa-
**    rameter. */

inline UINT8 OSUINT8_LL(UINT8 *memAddr)
{
  UINT8 tmp;
  asm volatile ("LDREXB %0,[%1]":"=&b"(tmp):"r"(memAddr));
  return tmp;
}

inline UINT16 OSUINT16_LL(UINT16 *memAddr)
{
  UINT16 tmp;
  asm volatile ("LDREXH %0,[%1]" : "=&b"(tmp) : "r"(memAddr));
  return tmp;
}

inline INT16 OSINT16_LL(INT16 *memAddr)
{
  INT16 tmp;
  asm volatile ("LDREXH %0,[%1]" : "=&b"(tmp) : "r"(memAddr));
  return tmp;
}

inline UINT32 OSUINT32_LL(UINT32 *memAddr)
{
  UINT32 tmp;
  asm volatile ("LDREX %0,[%1]" : "=&b"(tmp) : "r"(memAddr));
  return tmp;
}

inline INT32 OSINT32_LL(INT32 *memAddr)
{
  INT32 tmp;
  asm volatile ("LDREX %0,[%1]" : "=&b"(tmp) : "r"(memAddr));
  return tmp;
}
/* end of OSUINT8_LL, OSUINT16_LL, OSINT16_LL, OSUINT32_LL and OSINT32_LL */


/* OSUINT8_SC, OSUINT16_SC, OSINT16_SC, OSUINT32_SC and OSUINT32_SC: Store Memory Location
** if Reserved. If the reservation bit is set by a previous call to an LL function, the
** second parameter is written into the memory location specified by the first parameter.
** SC functions are semantically equivalent to the atomic execution of the following code
**    BOOL OStype_SC(TYPE *memAddr, TYPE newValue) {
**       if (reserveBit) {
**          *memAddr = newValue
**          reserveBit = FALSE;
**          return TRUE;
**       }
**       else
**          return FALSE;
**     }
** where TYPE can be one of UINT8, UINT16, INT16, UINT32 or INT32.
** Parameters:
**   (1) (TYPE *) Address to a memory location that holds the value to modify, where TYPE
**                can be one of UINT8, UINT16, INT16, UINT32 or INT32.
**   (2) (TYPE) Value to insert into the memory location specified by the above parameter
**              if and only if the reservation bit is still set.
** Returned value: (BOOL) TRUE if the store took place and FALSE otherwise. */

inline BOOL OSUINT8_SC(UINT8 *memAddr, register UINT8 newVal)
{
  register BOOL tmp;
  asm volatile ("STREXB %0,%2,[%1]\n"
                "SUB %0,#1" : "=&b"(tmp) : "r"(memAddr),"r"(newVal));
  return tmp;
}

inline BOOL OSUINT16_SC(UINT16 *memAddr, register UINT16 newVal)
{
  register BOOL tmp;
  asm volatile ("STREXH %0,%2,[%1]\n"
                "SUB %0,#1" : "=&b"(tmp) : "r"(memAddr),"r"(newVal));
  return tmp;
}

inline BOOL OSINT16_SC(INT16 *memAddr, register INT16 newVal)
{
  register BOOL tmp;
  asm volatile ("STREXH %0,%2,[%1]\n"
                "SUB %0,#1" : "=&b"(tmp) : "r"(memAddr),"r"(newVal));
  return tmp;
}

inline BOOL OSUINT32_SC(UINT32 *memAddr, register UINT32 newVal)
{
  register BOOL tmp;
  asm volatile ("STREX %0,%2,[%1]\n"
                "SUB %0,#1" : "=&b"(tmp) : "r"(memAddr),"r"(newVal));
  return tmp;
}

inline BOOL OSINT32_SC(INT32 *memAddr, register INT32 newVal)
{
  register BOOL tmp;
  asm volatile ("STREX %0,%2,[%1]\n"
                "SUB %0,#1" : "=&b"(tmp) : "r"(memAddr),"r"(newVal));
  return tmp;
}
/* end of OSUINT8_SC, OSUINT16_SC, OSINT16_SC, OSUINT32_SC and OSINT32_SC */


/* SYSTEM INTERRUPT ------------------------------------------------------------------ */

/* Stack addresses defined in linker file */
extern void *_OSStartStack;      // Starting address of the run-time stack
extern void *_OSStartMainStack;  // Ditto but during initialization phase

extern void _OSContextSwapHandler(void); /* defined in ZottaOS_CortexM3_a.S */

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

/* Cortex-M
 * 3 interrupt vector table and reset (see Cortex-M3 Technical Reference Manual
** Sections 5.2 and 5.9.1, pages 5-3 and 5-19) */
__attribute__ ((section(".isr_vector_general")))
void (* const CortextM3VectorTable[])(void) =
{
  (void (* const)(void))&_OSStartMainStack, // Initial stack pointer, position 0
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
  _OSContextSwapHandler, // PendSV exception
  SoftTimerInterrupt     // SysTick
                         // External interrupts start after SysTick, at position 16
};


/* Cortex registers setting the number interrupt priority levels. */
#define AIRCR *((UINT32 *)0xE000ED0C) // Application Interrupt/Reset Control Register
#define SHPR *((UINT16 *)0xE000ED22)  // System Handlers 14-15 Priority Register

/* Definitions of the Cortex-M3 system interrupt handlers */
/* _OSReset_Handler: This is the code that gets called when the processor first starts
** execution following a reset event. Only the absolutely necessary set is performed,
** after which the application supplied main() routine is called. */
void _OSResetHandler(void)
{
  /* Segment start and end address defined in linker file */
  extern UINT32 _etext; // Global variables in flash memory different than 0.
  extern UINT32 _data;  // RAM start location for the above globals
  extern UINT32 _edata; // RAM end location of previous globals
  extern UINT32 _bss;   // Global variables that are initialized to 0 and stored in RAM.
  extern UINT32 _ebss;  // RAM end address of the above.
  extern void SystemInit (void); // Defined in system_stm32f10x.c */
  UINT32 *pulSrc, *pulDest;
  asm("CPSID I");       // Disable all interrupt
  /* Copy the data segment initializers from flash to SRAM */
  pulSrc = &_etext;
  for (pulDest = &_data; pulDest < &_edata; *(pulDest++) = *(pulSrc++));
  /* Zero fill the bss segment  */
  for (pulDest = &_bss; pulDest < &_ebss; *(pulDest++) = 0);
  /* Initialize interrupts priority system */
  AIRCR = 0x05FA0000 | (PRIGROUP << 8); // Set pre-emption priority and subpriority
  switch (PRIGROUP) {
     case 0:   // indicates 7 bits of pre-emption priority, 1 bit of subpriority
        SHPR = 0xFCFE;  // SysTick = FC;  PendSV = FE = lowest priority group
        #ifdef DEBUG_MODE
           if (TIMER_PRIORITY >= 0xFC)
              while (TRUE);
        #endif
        break;
     case 1:   // indicates 6 bits of pre-emption priority, 2 bits of subpriority
        SHPR = 0xF8FC;
        #ifdef DEBUG_MODE
           if (TIMER_PRIORITY >= 0xF8)
              while (TRUE);
        #endif
        break;
     case 2:   // indicates 5 bits of pre-emption priority, 3 bits of subpriority
        SHPR = 0xF0F8;
        #ifdef DEBUG_MODE
           if (TIMER_PRIORITY >= 0xF0)
              while (TRUE);
        #endif
        break;
     case 3:   // indicates 4 bits of pre-emption priority, 4 bits of subpriority
        SHPR = 0xE0F0;
        #ifdef DEBUG_MODE
           if (TIMER_PRIORITY >= 0xE0)
              while (TRUE);
        #endif
        break;
     case 4:   // indicates 3 bits of pre-emption priority, 5 bits of subpriority
        SHPR = 0xC0E0;
        #ifdef DEBUG_MODE
           if (TIMER_PRIORITY >= 0xC0)
              while (TRUE);
        #endif
        break;
     case 5:   // indicates 2 bits of pre-emption priority, 6 bits of subpriority
        SHPR = 0x80C0;
        #ifdef DEBUG_MODE
           if (TIMER_PRIORITY >= 0x80)
              while (TRUE);
        #endif
        break;
     default:
        #ifdef DEBUG_MODE
           while (TRUE); // Error (at least 4 levels of pre-emption priority is need)
        #endif
        break;
  }

  SystemInit();

  /* Call the application's entry point */
  asm("B main");
} /* end of _OSResetHandler */


void NMIException(void)
{
  while (TRUE);
} /* end of NMIException */


void HardFaultException(void)
{
  while (TRUE);
} /* end of HardFaultException */


void MemManageException(void)
{
  while (TRUE);
} /* end of MemManageException */


void BusFaultException(void)
{
  while (TRUE);
} /* end of BusFaultException */


void UsageFaultException(void)
{
  while (TRUE);
} /* end of UsageFaultException */


void SVCHandler(void)
{
  while (TRUE);
} /* end of SVCHandler */


void DebugMonitor(void)
{
  while (TRUE);
} /* end of DebugMonitor */
/* end of the Cortex-M3 system interrupt handler definitions */


#define READYQ       0

#define STATE_ZOMBIE 0x02
#if defined(ZOTTAOS_VERSION_SOFT)
   #define STATE_INIT          0x00
   #define STATE_ACTIVATE      0x10
#endif

typedef struct MinimalTCB {
  struct MinimalTCB *Next[2]; // Next TCB in the list where this task is located
                              // [0]: ready queue link, [1]: arrival queue link
  UINT8 TaskState;            // Current state of the task
} MinimalTCB;


/* FinalizeContextSwitchPreparation: On entry of an interrupt handler, we need to assert
** that an application task is not in the middle of preparing to do a context switch. If
** this is the case, we need to supersede its actions and never return to that task
** instance.*/
void FinalizeContextSwitchPreparation(void)
{
  extern MinimalTCB *_OSActiveTask;
  extern MinimalTCB *_OSQueueHead;
  #if defined(ZOTTAOS_VERSION_SOFT)
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
  #if defined(ZOTTAOS_VERSION_SOFT)
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


/* SoftTimerInterrupt: .*/
void SoftTimerInterrupt(void)
{
  extern void _OSTimerInterruptHandler(void); /* Defined in ZottaOSHard.c or ZottaOSSoft.c */
  FinalizeContextSwitchPreparation();
  _OSTimerInterruptHandler(); // Jump to to the generic service routine of the timer
  __asm("CLREX;");            // Make all pending SC() fail
} /* end of SoftTimerInterrupt */


/* Definition of a minimal peripheral descriptor to retrieve the peripheral interrupt
** handler*/
typedef struct MinimalIODescriptor {
  void (*PeripheralInterruptHandler)(struct MinimalIODescriptor *);
} MinimalIODescriptor;

/* _OSIOHandler: Default interrupt handler that calls the appropriate handler depending
** upon the interrupt source. Note that if no handler is associated with the interrupt
** source, the processor may go haywire after it proceeds to a non valid address. */
void _OSIOHandler(void)
{
  /* Global interrupt vector with one entry per source which is defined in either
  ** ZottaOS_msp430xxx.asm or ZottaOS_cc430xxx.asm */
  extern void *_OSTabDevice[];
  MinimalIODescriptor *peripheralIODescriptor;
  /* Retrieve the specific handler from IODescriptorTab */
  peripheralIODescriptor = _OSTabDevice[(*((UINT32 *)0xE000ED04) & 0x1FF) - 16];
  /* Call the specific handler */
  peripheralIODescriptor->PeripheralInterruptHandler(peripheralIODescriptor);
  if (_OSActiveTask != NULL) { /* If the kernel has already started */
     FinalizeContextSwitchPreparation();
     _OSScheduleTask(); // Generate context swap software interrupt
  }
  __asm("CLREX;"); // Make all pending SC() fail
} /* end of _OSIOHandler */


/* DYNAMIC MEMORY ALLOCATIONs -------------------------------------------------------- */
/* Since most applications do all their dynamic memory allocations at initialization time
** and these allocations are never reclaimed, the malloc function provided by libc is an
** overkill because there is no need to manage free blocks. To correct this state of af-
** fairs, we use the following scheme: During initialization time, i.e., when main is ac-
** tive and the first task has not yet executed, the stack starts at the middle of the
** memory, all functions that are called to prepare the kernel are stacked above main.
** During this time, any dynamic memory block allocated to the application is done at the
** base of the stack using function AllocaMain(). Once the kernel starts its execution,
** the stack is adjusted so that it evolves above the allocated blocks. As well as provi-
** ding a useful alloca function for the user application, this approach has several in-
** teresting advantages:
** (1) Code size is definitively smaller and faster compared to the malloc/free pair;
** (2) Code is reentrant and needs no synchronization lock;
** (3) There is no memory corruption in case of a stack overflow. */

/* OSMalloc: These functions are used while the main function is active. Allo-
** cation is done at the bottom of the stack; however, the run-time stack of main starts
** at the middle of the memory (or at the address specified by OSStartMainStack). By pro-
** ceeding in this way, all global memory blocks are kept at the bottom of the stack.
** Parameter: (UINT32) requested memory block size in bytes.
** Returned value: (void *) Pointer to the base of the newly allocated block, and NULL in
**    case of an error, i.e., when more than half of the memory is consumed by dynamic
**    allocations done in main. */
void *OSMalloc(UINT16 size)
{
  extern void *_OSStackBasePointer;
  if (_OSStackBasePointer == NULL)
     _OSStackBasePointer = &_OSStartStack;
  if (size % 4 != 0)
     size += 4 - size % 4;
  _OSStackBasePointer = (UINT8 *)_OSStackBasePointer - size;
  /* Check that requested memory block doesn't overlap main's activation record. */
  if (_OSStackBasePointer <= (void *)&_OSStartMainStack) {
     _OSStackBasePointer = (UINT8 *)_OSStackBasePointer + size;
     #ifdef DEBUG_MODE
        while (TRUE); // Memory overflow: decrease START_MAIN_STACK
        return 0;     // To avoid compiler warning
     #else
        return NULL;
     #endif
  }
  else
     return _OSStackBasePointer;
} /* end of OSMalloc */
