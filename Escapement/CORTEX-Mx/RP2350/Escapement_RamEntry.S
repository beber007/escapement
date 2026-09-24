/* Copyright (c) 2026 Bertrand Hurst. All rights reserved.
** Escapement - Lightweight Power-Aware Real-Time OS.
** Distributed under the terms of LICENSE at the root of this repository.
*/
/* File Escapement_RamEntry.S: Entry point of a firmware executing from SRAM.
**
** A firmware in flash is started by a hardware reset, which loads the stack pointer and
** the reset vector from the first two words of the vector table. A firmware in SRAM is
** started by the bootrom, which simply branches to the first instruction of the image and
** leaves the stack pointer as it found it. The image must therefore begin with code, not
** with the vector table, and that code has to set up the stack itself.
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
    ldr  r0, =CortexMxVectorTable
    ldr  r1, [r0]            /* first word: top of stack */
    mov  sp, r1
    ldr  r1, [r0, #4]        /* second word: _OSResetHandler */
    bx   r1

    .align 2
    .ltorg
