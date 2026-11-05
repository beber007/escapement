/* Copyright (c) 2026 Bertrand Hurst. All rights reserved.
** Escapement - Lightweight Power-Aware Real-Time OS.
** Distributed under the terms of LICENSE at the root of this repository.
*/
/* File Escapement_RISCV.h: The macros and defines of the RISC-V layer, the counterpart of
** Escapement_CortexMx.h for the Hazard3 cores of the RP2350 (issue #77). The port's
** Escapement_Processor.h includes this file in place of that one when the compiler
** builds for RISC-V.
**
** The kernel needs three levels of interrupt, which the Cortex-M gives as the timer's
** interrupt, SysTick and PendSV. On the Hazard3:
** - the timer's alarm is an external interrupt, at the same number as on Arm (RP2350
**   datasheet, 3.8.4.2), at a high level of the Xh3irq controller;
** - the soft timer interrupt, which takes the arrivals, is SPARE_IRQ_0 (46), an external
**   interrupt tied to no peripheral, forced pending through meifa and held at the lowest
**   level: it runs last before the core returns to the task (3.8.6.1.2);
** - the context switch is the machine software interrupt (MSI), set through the SIO's
**   RISCV_SOFTIRQ (3.8.4.2.1). The external interrupt handler clears its enable while it
**   runs (meicontext.clearts), and only the outermost one puts it back: the MSI is taken
**   once every external handler has returned, as PendSV is. Checked on the Pico 2 on
**   2026-11-05, an interrupt of a higher level nested in the soft timer's included.
**
** Platform version: Hazard3 (RV32IMAC), RP2350.
*/

#ifndef _ESCAPEMENT_RISCV_H_
#define _ESCAPEMENT_RISCV_H_

#include "Escapement_Modes.h"   /* before the tests of SCHEDULER_REAL_TIME_MODE below */

/* Offsets the context switch reads out of a task control block: the same as on the
** Cortex-M, the pointers being 32 bits wide here too. Escapement_RISCV_a.S reads them by
** hand, and OSCheckTCBLayout checks them against each kernel's TCB. */
#if defined(ESCAPEMENT_VERSION_SOFT) && SCHEDULER_REAL_TIME_MODE == DEADLINE_MONOTONIC_SCHEDULING
   #define OS_TCB_STATE_OFFSET     8
   #define OS_TCB_ENTRY_OFFSET    20
   #define OS_TCB_ARGUMENT_OFFSET 24
#else
   #define OS_TCB_STATE_OFFSET     8
   #define OS_TCB_ENTRY_OFFSET    16
   #define OS_TCB_ARGUMENT_OFFSET 20
#endif

#if !defined(_ASM_)
   #define OSCheckTCBLayout() \
      _Static_assert(__builtin_offsetof(TCB,TaskState) == OS_TCB_STATE_OFFSET, \
                     "TaskState moved; Escapement_RISCV_a.S reads it at another offset"); \
      _Static_assert(__builtin_offsetof(TCB,TaskCodePtr) == OS_TCB_ENTRY_OFFSET, \
                     "TaskCodePtr moved; Escapement_RISCV_a.S reads it at another offset"); \
      _Static_assert(__builtin_offsetof(TCB,Argument) == OS_TCB_ARGUMENT_OFFSET, \
                     "Argument moved; Escapement_RISCV_a.S reads it at another offset")
#endif

/* The bit the non-blocking algorithms mark an address with: SRAM lies below 0x80000000. */
#define MARKEDBIT    0x80000000u
#define UNMARKEDBIT  0x7FFFFFFFu

/* The Xh3irq's control and status registers (RP2350 datasheet, 3.8.6.1), and the SIO's
** register of the software interrupt. */
#define OS_CSR_MEIEA      0xbe0
#define OS_CSR_MEIPA      0xbe1
#define OS_CSR_MEIFA      0xbe2
#define OS_CSR_MEIPRA     0xbe3
#define OS_CSR_MEINEXT    0xbe4
#define OS_CSR_MEICONTEXT 0xbe5
#define OS_CSR_PMPCFGM0   0xbd0
#define OS_SIO_SOFTIRQ    0xd00001a0u  /* bit n sets core n's MSI, bit 8 + n clears it */
#define OS_IRQ_SOFT_TIMER 46           /* SPARE_IRQ_0 */
#define OS_IRQ_COUNT      52

#ifndef _ASM_

#define OS_CSR_STR(x) #x
#define OS_CSR_XSTR(x) OS_CSR_STR(x)
#define _OSCSRSet(csr,value) \
   __asm volatile ("csrs " OS_CSR_XSTR(csr) ", %0" :: "r"((UINT32)(value)) : "memory")
#define _OSCSRClear(csr,value) \
   __asm volatile ("csrc " OS_CSR_XSTR(csr) ", %0" :: "r"((UINT32)(value)) : "memory")

/* _OSIOHandler: Called by the external interrupt handler of Escapement_RISCV_a.S with
** the number of the interrupt it takes. */
void _OSIOHandler(UINT32 irq);

/* _OSEnableInterrupts and _OSDisableInterrupts: mstatus.MIE, with the memory clobber that
** makes them critical sections for the compiler too. _OSMaskInterrupts returns mstatus as
** it was, of which _OSRestoreInterrupts puts back MIE alone. */
#define _OSEnableInterrupts()  __asm volatile ("csrsi mstatus, 8" ::: "memory")
#define _OSDisableInterrupts() __asm volatile ("csrci mstatus, 8" ::: "memory")
#define _OSMaskInterrupts() __extension__ ({ UINT32 mstatus; \
           __asm volatile ("csrrci %0, mstatus, 8" : "=r"(mstatus) :: "memory"); mstatus; })
#define _OSRestoreInterrupts(mstatus) \
   __asm volatile ("csrs mstatus, %0" :: "r"((mstatus) & 8u) : "memory")

/* Peripheral interrupts, under the names of Escapement_CortexMx.h, for the drivers the two
** architectures share. Those take a word of 32 interrupts and a mask within it; the
** Xh3irq's arrays show 16 at a time, through a window chosen by the low bits of the value
** written (3.8.6.1.1): each half of the mask goes through its own window, and a constant
** mask leaves only the halves it has. */
#define OS_XH3IRQ_SET(csr,word,bits) \
   do { \
     UINT32 b = (bits); \
     if (b & 0xFFFFu) _OSCSRSet(csr, ((word) * 2) | ((b & 0xFFFFu) << 16)); \
     if (b >> 16)     _OSCSRSet(csr, ((word) * 2 + 1) | (b & 0xFFFF0000u)); \
   } while (0)
#define OS_XH3IRQ_CLEAR(csr,word,bits) \
   do { \
     UINT32 b = (bits); \
     if (b & 0xFFFFu) _OSCSRClear(csr, ((word) * 2) | ((b & 0xFFFFu) << 16)); \
     if (b >> 16)     _OSCSRClear(csr, ((word) * 2 + 1) | (b & 0xFFFF0000u)); \
   } while (0)
#define _OS_IRQ_WORD(irq) ((irq) >> 5)
#define _OS_IRQ_BIT(irq)  (1u << ((irq) & 0x1F))
#define _OSIRQEnable(word,bits)       OS_XH3IRQ_SET(OS_CSR_MEIEA,word,bits)
#define _OSIRQSetPending(word,bits)   OS_XH3IRQ_SET(OS_CSR_MEIFA,word,bits)
#define _OSIRQClearPending(word,bits) OS_XH3IRQ_CLEAR(OS_CSR_MEIFA,word,bits)
/* _OSIRQSetPriority: Level 0 is the highest, as on the Cortex-M. The Xh3irq counts the
** other way, 15 the highest (3.8.6.1.4), and keeps level 0 for the soft timer interrupt
** alone: a peripheral's level n becomes 15 - n, at least 1. The field is cleared before
** it is set, as the pico-sdk does (irq_set_priority): there is no write of one field. */
#define _OSIRQSetPriority(irq,level) \
   do { \
     UINT32 n = (irq), p = 15u - ((level) > 14u ? 14u : (level)); \
     _OSCSRClear(OS_CSR_MEIPRA, (n >> 2) | ((0xFu << (4 * (n & 3))) << 16)); \
     _OSCSRSet(OS_CSR_MEIPRA, (n >> 2) | ((p << (4 * (n & 3))) << 16)); \
   } while (0)

/* _OSSleep: One wait for an interrupt, and the idle task's loop of them; a port may
** define _OSSleep again. wfi wakes on an interrupt pending and enabled in mie, whatever
** mstatus.MIE (3.8.1). */
#define _OSSleepOnce() __asm volatile ("wfi" ::: "memory")
#define _OSSleep() while (TRUE) { _OSSleepOnce(); };

/* _OSScheduleTask: Sets this core's MSI, which runs the context switch of
** Escapement_RISCV_a.S. The interrupt is not taken at once: on the Pico 2, two more
** instructions ran after the store to the SIO (2026-11-05). OSEndTask must never return,
** so this waits until mip.MSIP shows it pending; with interrupts enabled it is then
** taken at the next instruction, and with them masked it waits there. */
#define _OSScheduleTask() \
   do { \
     UINT32 hart, pending; \
     __asm volatile ("csrr %0, mhartid" : "=r"(hart)); \
     *((volatile UINT32 *)OS_SIO_SOFTIRQ) = 1u << hart; \
     do \
        __asm volatile ("csrr %0, mip" : "=r"(pending) :: "memory"); \
     while ((pending & 8u) == 0); \
   } while (0)

/* _OSGenerateSoftTimerInterrupt: Forces the soft timer's interrupt pending. The Xh3irq
** clears the force itself when meinext returns that interrupt (3.8.6.1.2). */
#define _OSGenerateSoftTimerInterrupt() \
   _OSCSRSet(OS_CSR_MEIFA, (OS_IRQ_SOFT_TIMER / 16) | ((1u << (OS_IRQ_SOFT_TIMER % 16)) << 16))

/* _OSClearSoftTimerInterrupt: Drops a soft timer interrupt forced again while its handler
** ran, as clearing SysTick's pending state does on the Cortex-M. */
#define _OSClearSoftTimerInterrupt() \
   _OSCSRClear(OS_CSR_MEIFA, (OS_IRQ_SOFT_TIMER / 16) | ((1u << (OS_IRQ_SOFT_TIMER % 16)) << 16))

#endif /* _ASM_ */

#endif /* _ESCAPEMENT_RISCV_H_ */
