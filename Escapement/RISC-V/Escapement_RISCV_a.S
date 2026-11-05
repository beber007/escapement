/* Copyright (c) 2026 Bertrand Hurst. All rights reserved.
** Escapement - Lightweight Power-Aware Real-Time OS.
** Distributed under the terms of LICENSE at the root of this repository.
*/
/* File Escapement_RISCV_a.S: The vector table, the external interrupt handler and the
** context switch of the RISC-V layer (issue #77).
**
** The external interrupt handler follows the one of the pico-sdk (crt0_riscv.S,
** isr_riscv_machine_external_irq, BSD-3-Clause), which the RP2350 datasheet describes
** (3.8.6.1.5): save, clear the timer's and software interrupt's enables with
** meicontext.clearts, take the next interrupt from meinext, re-enable preemption, call,
** and loop until none is left at this level.
**
** The context switch is Escapement_CortexMx_a.S's, carried over. The Cortex-M stacks
** eight registers on its own as it takes an exception; the Hazard3 stacks none, and the
** software interrupt's handler saves the registers a call does not keep into the same
** frame the external one makes. That frame plays the part of the Cortex-M's: a task is
** only ever switched away from in this handler, so a task preempted carries such a
** frame, under which lie the registers a call keeps, saved by hand. See the table of
** _OSContextSwapHandler in Escapement_CortexMx_a.S, which holds here unchanged:
**
**   _OSNoSaveContext  TaskState RUNNING            TaskState INIT
**   TRUE              wipe the stack, return to    wipe the stack, start the
**                     the preempted task           new task
**   FALSE             return to the task           save the registers kept, start
**                                                  the new task
**
** Frame of an interrupt, 80 bytes to keep the stack aligned on 16 (RISC-V psABI):
**   0 ra, 4 t0, 8 t1, 12 t2, 16 a0 ... 44 a7, 48 t3 ... 60 t6, 64 mepc, 68 mstatus,
**   72 meicontext (the external handler's only).
** Registers kept, saved by hand above a frame, 64 bytes:
**   0 the previous _OSStackBasePointer, 4 s0 ... 48 s11.
**
** Platform version: Hazard3 (RV32IMAC), RP2350.
*/

#define _ASM_

#include "Escapement_Config.h"
#include "Escapement_Modes.h"
#include "Escapement_RISCV.h"

#ifdef ESCAPEMENT_MEASURE_WCET
   #error "the execution times (make WCET=1) are not measured on RISC-V yet"
#endif

#define FRAME      80
#define KEPT       64
#define F_MEPC     64
#define F_MSTATUS  68
#define F_CONTEXT  72

/* A new task starts in machine mode with interrupts enabled: mret puts MPIE into MIE. */
#define MSTATUS_TASK 0x1880   /* MPP = 3, MPIE */

.macro SAVE_CALLER
    sw ra, 0(sp);  sw t0, 4(sp);  sw t1, 8(sp);  sw t2, 12(sp)
    sw a0, 16(sp); sw a1, 20(sp); sw a2, 24(sp); sw a3, 28(sp)
    sw a4, 32(sp); sw a5, 36(sp); sw a6, 40(sp); sw a7, 44(sp)
    sw t3, 48(sp); sw t4, 52(sp); sw t5, 56(sp); sw t6, 60(sp)
.endm
.macro RESTORE_CALLER_BUT_A0
    lw ra, 0(sp);  lw t0, 4(sp);  lw t1, 8(sp);  lw t2, 12(sp)
                   lw a1, 20(sp); lw a2, 24(sp); lw a3, 28(sp)
    lw a4, 32(sp); lw a5, 36(sp); lw a6, 40(sp); lw a7, 44(sp)
    lw t3, 48(sp); lw t4, 52(sp); lw t5, 56(sp); lw t6, 60(sp)
.endm

.global _OSVectorTable
.global _OSContextSwapHandler
.extern _OSNoSaveContext
.extern _OSStackBasePointer
.extern _OSActiveTask
.extern _OSIOHandler
.extern _OSFault

/* The vector table, which mtvec points at in vectored mode: exceptions at its start, then
** one entry per cause (RP2350 datasheet, 3.8.4). It is aligned on 64 bytes, the size of
** the table rounded up to a power of two. */
.section .text._OSVectorTable, "ax"
.p2align 6
_OSVectorTable:
.option push
.option norvc
    j    ExceptionHandler           /* +0x00: exceptions */
    .word 0, 0
    j    _OSContextSwapHandler      /* +0x0c: machine software interrupt, cause 3 */
    .word 0, 0, 0
    j    ExceptionHandler           /* +0x1c: machine timer interrupt, never enabled */
    .word 0, 0, 0
.option pop
    /* +0x2c: machine external interrupt, cause 11, the handler inlined. */
ExternalHandler:
    addi sp, sp, -FRAME
    SAVE_CALLER
    csrr a0, mepc
    csrr a1, mstatus
    sw   a0, F_MEPC(sp)
    sw   a1, F_MSTATUS(sp)
ExternalSaveContext:
    /* meicontext.clearts (bit 1) clears mie.msie and mie.mtie and returns them in the
    ** value read: the context switch cannot preempt this handler, and is let in again
    ** only as the outermost one restores what it read here. */
    csrrsi a2, OS_CSR_MEICONTEXT, 2
    sw   a2, F_CONTEXT(sp)
    csrrsi a0, OS_CSR_MEINEXT, 1     /* the next interrupt, meicontext updated with it */
    bltz a0, ExternalDone
ExternalDispatch:
    csrsi mstatus, 8                 /* preemption by a higher level from here on */
    srli a0, a0, 2                   /* meinext holds the number shifted by 2 */
    call _OSIOHandler
    csrci mstatus, 8
    csrrsi a0, OS_CSR_MEINEXT, 1
    bgez a0, ExternalDispatch
ExternalDone:
    lw   a0, F_MEPC(sp)
    lw   a1, F_MSTATUS(sp)
    lw   a2, F_CONTEXT(sp)
    csrw mepc, a0
    csrw mstatus, a1
    csrw OS_CSR_MEICONTEXT, a2
    RESTORE_CALLER_BUT_A0
    /* One more pending at this level: take it without leaving, and so without the cost of
    ** a return and an entry. meicontext was restored, so its enables are saved again. */
    csrr a0, OS_CSR_MEINEXT
    bgez a0, ExternalSaveContext
    lw   a0, 16(sp)
    addi sp, sp, FRAME
    mret

/* ExceptionHandler: An exception, a fault of the application or of the kernel. Its cause,
** address and value go to _OSFault (Escapement_RISCV.c) for the debugger, and the core
** stops there, interrupts masked. No stack is used: sp may be what faulted. */
ExceptionHandler:
    la   t0, _OSFault
    csrr t1, mcause
    sw   t1, 0(t0)
    csrr t1, mepc
    sw   t1, 4(t0)
    csrr t1, mtval
    sw   t1, 8(t0)
1:  j    1b

/* _OSTaskReturned: Where a task that returns goes, its ra set so by the context switch: a
** task ends with OSEndTask, and returning is an application's fault, made an exception
** here as the Cortex-M makes it one. */
_OSTaskReturned:
    unimp

/* _OSContextSwapHandler: The machine software interrupt. Entered with interrupts masked,
** which they stay: the switch takes some forty instructions, which a higher level waits
** for, where PendSV could be preempted. */
.section .text._OSContextSwapHandler, "ax"
_OSContextSwapHandler:
    addi sp, sp, -FRAME
    SAVE_CALLER
    csrr a0, mepc
    csrr a1, mstatus
    sw   a0, F_MEPC(sp)
    sw   a1, F_MSTATUS(sp)
    /* Clear this core's request: the SIO holds it as a level until written. */
    csrr t0, mhartid
    li   t1, 0x100
    sll  t1, t1, t0
    li   t2, OS_SIO_SOFTIRQ
    sw   t1, 0(t2)
    la   a0, _OSNoSaveContext
    la   a2, _OSStackBasePointer
    la   a1, _OSActiveTask
    lw   a1, 0(a1)                   /* a1 <- _OSActiveTask */
    lbu  a3, 0(a0)
    bnez a3, SwapClearStack          /* _OSNoSaveContext */
    lbu  t2, OS_TCB_STATE_OFFSET(a1)
    andi a3, t2, 1
    bnez a3, SwapEnd                 /* the active task is running: back to it */
    /* Save the registers kept by the task preempted, then start the new one. */
    lw   a3, 0(a2)
    addi sp, sp, -KEPT
    sw   a3, 0(sp)
    sw   s0, 4(sp);  sw s1, 8(sp);   sw s2, 12(sp);  sw s3, 16(sp)
    sw   s4, 20(sp); sw s5, 24(sp);  sw s6, 28(sp);  sw s7, 32(sp)
    sw   s8, 36(sp); sw s9, 40(sp);  sw s10, 44(sp); sw s11, 48(sp)
    sw   sp, 0(a2)                   /* _OSStackBasePointer <- sp */
    j    SwapCreateNewContext
SwapClearStack:
    sb   zero, 0(a0)                 /* _OSNoSaveContext <- FALSE */
    lw   sp, 0(a2)                   /* the stack wiped down to _OSStackBasePointer */
    andi sp, sp, -16                 /* OSMalloc leaves it on 4 bytes; the psABI wants 16 */
    lbu  t2, OS_TCB_STATE_OFFSET(a1)
    andi a3, t2, 1
    bnez a3, SwapRestoreContext
SwapCreateNewContext:
    /* The state's other bits kept, as on the Cortex-M0 (Escapement_CortexMx_a.S). */
    ori  t2, t2, 1
    sb   t2, OS_TCB_STATE_OFFSET(a1)
    lw   t0, OS_TCB_ENTRY_OFFSET(a1)
    lw   t1, OS_TCB_ARGUMENT_OFFSET(a1)
    /* A frame for the new task, as if an interrupt had stopped it at its first
    ** instruction: its argument in a0, its return address a trap, since a task never
    ** returns and ends with OSEndTask. */
    addi sp, sp, -FRAME
    sw   t1, 16(sp)
    sw   t0, F_MEPC(sp)
    li   t3, MSTATUS_TASK
    sw   t3, F_MSTATUS(sp)
    la   t4, _OSTaskReturned
    sw   t4, 0(sp)
    j    SwapEnd
SwapRestoreContext:
    lw   a3, 0(sp)
    lw   s0, 4(sp);  lw s1, 8(sp);   lw s2, 12(sp);  lw s3, 16(sp)
    lw   s4, 20(sp); lw s5, 24(sp);  lw s6, 28(sp);  lw s7, 32(sp)
    lw   s8, 36(sp); lw s9, 40(sp);  lw s10, 44(sp); lw s11, 48(sp)
    addi sp, sp, KEPT
    sw   a3, 0(a2)                   /* _OSStackBasePointer <- the previous one */
SwapEnd:
    /* No reservation to clear: the Hazard3 clears it on the way out of a trap
    ** (hazard3_core.v), where the Cortex-M3 needs a CLREX. */
    lw   a0, F_MEPC(sp)
    lw   a1, F_MSTATUS(sp)
    csrw mepc, a0
    csrw mstatus, a1
    RESTORE_CALLER_BUT_A0
    lw   a0, 16(sp)
    addi sp, sp, FRAME
    mret
