/* Copyright (c) 2026 Bertrand Hurst. All rights reserved.
** Escapement - Lightweight Power-Aware Real-Time OS.
** Distributed under the terms of LICENSE at the root of this repository.
*/
/* File Escapement_RamEntry.S: Entry point of an STM32U3 firmware executing from SRAM,
** that of the STM32U5 port.
**
** A firmware in flash is started by a hardware reset, which loads the stack pointer and
** the reset vector from the first two words of the vector table. A firmware in SRAM is
** started by the debugger (load_image, then resume at 0x20000000), which sets only the
** program counter. The image therefore begins with code, which takes
** the stack pointer and the reset handler from the vector table that follows it, and
** leaves the flash untouched.
**
** Platform version: STM32U385 (NUCLEO-U385RG-Q), any STM32U375/385.
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
