/* Copyright (c) 2026 Bertrand Hurst. All rights reserved.
** Escapement - Lightweight Power-Aware Real-Time OS.
** Distributed under the terms of LICENSE at the root of this repository.
*/
/* File Escapement_RamEntry.S: Entry point of a firmware executing from SRAM.
**
** A firmware in flash is started by a hardware reset, which loads the stack pointer and
** the reset vector from the first two words of the vector table. A firmware in SRAM is
** started by the debugger (load_image, then resume at 0x20000000, docs/rp2040.md), which
** sets only the program counter and leaves the stack pointer where the bootrom left it.
** The image must therefore begin with code, not with the vector table, and that code has
** to set up the stack itself.
**
** It also sets ACTLR.EXTEXCLALL of core 0, as the pico-sdk does on each core
** (hardware_sync_spin_lock): the exclusive loads and stores then go through the global
** monitor, which sees the stores of the other core, rather than through a monitor local
** to this one. Without it the LL/SC pairs of the kernel hold on one core only, and the
** 3-slot buffer fails between the two (test/model/threeslot.py, two cores).
** Escapement_Core1.c does the same on core 1.
**
** Platform version: RP2350 (Raspberry Pi Pico 2).
*/

    .syntax unified
    .arch armv8-m.main   /* the Cortex-M33, its floating-point unit left out */
    .thumb

    .section .ram_entry, "ax"
    .align 2
    .global _OSRamEntry
    .thumb_func
_OSRamEntry:
    ldr  r0, =0xE000E008     /* ACTLR */
    ldr  r1, [r0]
    orr  r1, r1, #0x20000000 /* EXTEXCLALL */
    str  r1, [r0]
    dsb
    isb
    ldr  r0, =CortexMxVectorTable
    ldr  r1, [r0]            /* first word: top of stack */
    mov  sp, r1
    ldr  r1, [r0, #4]        /* second word: _OSResetHandler */
    bx   r1

    .align 2
    .ltorg
