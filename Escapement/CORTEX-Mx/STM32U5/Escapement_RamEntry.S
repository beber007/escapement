/* Copyright (c) 2026 Bertrand Hurst. All rights reserved.
** Escapement - Lightweight Power-Aware Real-Time OS.
** Distributed under the terms of LICENSE at the root of this repository.
*/
/* File Escapement_RamEntry.S: Entry point of an STM32U5 firmware executing from SRAM,
** transposed from the RP2350's.
**
** A firmware in flash is started by a hardware reset, which loads the stack pointer and
** the reset vector from the first two words of the vector table. A firmware in SRAM is
** started by the debugger (load_image, then resume at 0x20000000, tools/unoq_load.sh),
** which sets only the program counter. The image therefore begins with code, which takes
** the stack pointer and the reset handler from the vector table that follows it. On the
** Arduino UNO Q this leaves the flash, and Arduino's firmware in it, untouched.
**
** Platform version: STM32U585 (Arduino UNO Q), any STM32U5.
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
