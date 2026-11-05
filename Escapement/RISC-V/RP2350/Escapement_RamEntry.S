/* Copyright (c) 2026 Bertrand Hurst. All rights reserved.
** Escapement - Lightweight Power-Aware Real-Time OS.
** Distributed under the terms of LICENSE at the root of this repository.
*/
/* File Escapement_RamEntry.S: Entry point of a RISC-V firmware executing from SRAM, and
** the block that has the bootrom start it on a Hazard3 (issue #77).
**
** The Arm image is started by the debugger at its first word. A RISC-V one is booted by
** the bootrom: the debugger loads it from the Arm side, writes a RAM image boot into the
** watchdog's scratch registers, and resumes (tools, #77). The bootrom then looks for an
** IMAGE_DEF in SRAM, switches the cores to RISC-V as it says, and jumps to its entry
** point (RP2350 datasheet, 5.9.3). Checked on the Pico 2 on 2026-11-05 with a prototype.
**
** The stack starts below the blocks OSMalloc hands out, as CortexMxVectorTable's first
** word has it on the Cortex-M. No global pointer: the linker script defines no
** __global_pointer$, so the linker relaxes no access to it.
** Platform version: Hazard3 (RV32IMAC), RP2350.
*/

#define _ASM_

#include "Escapement_Config.h"

    .section .ram_entry, "ax"
    .global _OSRamEntry
_OSRamEntry:
    la   sp, _OSEndRAM
    li   t0, OSMALLOC_INTERNAL_HEAP_SIZE
    sub  sp, sp, t0
    j    _OSResetHandler

    .section .image_def, "a"
    .p2align 2
image_def:
    .word 0xffffded3          /* PICOBIN_BLOCK_MARKER_START */
    .byte 0x42                /* PICOBIN_BLOCK_ITEM_1BS_IMAGE_TYPE */
    .byte 1                   /* one word */
    .hword 0x1101             /* EXE, RISC-V, RP2350 (5.9.3.1) */
    .byte 0x44                /* PICOBIN_BLOCK_ITEM_1BS_ENTRY_POINT (5.9.3.4) */
    .byte 3                   /* three words */
    .hword 0
    .word _OSRamEntry         /* the initial PC */
    .word _OSEndRAM           /* the initial SP, which _OSRamEntry sets again */
    .byte 0xff                /* PICOBIN_BLOCK_ITEM_2BS_LAST */
    .hword 4                  /* the words of the items above */
    .byte 0
    .word 0                   /* the next block: this one, a loop of one block */
    .word 0xab123579          /* PICOBIN_BLOCK_MARKER_END */
