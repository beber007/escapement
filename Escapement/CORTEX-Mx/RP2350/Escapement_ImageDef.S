/* Copyright (c) 2026 Bertrand Hurst. All rights reserved.
** Escapement - Lightweight Power-Aware Real-Time OS.
** Distributed under the terms of LICENSE at the root of this repository.
*/
/* File Escapement_ImageDef.S: The block that makes a firmware in flash bootable.
**
** The bootrom of the RP2350 boots from the flash only an image with a valid IMAGE_DEF
** block within its first 4 kB (RP2350 datasheet, 5.2.7 and 5.9.5.1), where the RP2040
** wanted a checksummed boot2. RP2350_FLASH.ld places this one just after the vector
** table. It holds two items: the image type, the minimum the bootrom accepts (an Arm
** executable, Secure, for the RP2350), and an entry point, _OSRamEntry, with its stack.
** Without the entry point the bootrom would enter through the vector table's reset
** handler; _OSRamEntry first sets ACTLR.EXTEXCLALL, which the kernel's LL/SC pairs need
** across the two cores (Escapement_RamEntry.S), and then branches to that handler, as
** it does for an image loaded into SRAM.
**
** The bootrom enters the image in the QSPI mode it found to read the flash with, and
** leaves it so (5.2.7): the image runs in that mode.
**
** Platform version: RP2350 (Raspberry Pi Pico 2).
*/

    .syntax unified
    .arch armv8-m.main
    .thumb

    .section .image_def, "a"
    .align 2
image_def:
    .word 0xffffded3          /* PICOBIN_BLOCK_MARKER_START */
    .byte 0x42                /* PICOBIN_BLOCK_ITEM_1BS_IMAGE_TYPE */
    .byte 1                   /* one word */
    .hword 0x1021             /* EXE, Secure, Arm, RP2350 (5.9.3.1) */
    .byte 0x44                /* PICOBIN_BLOCK_ITEM_1BS_ENTRY_POINT (5.9.3.4) */
    .byte 3                   /* three words */
    .hword 0
    .word _OSRamEntry         /* the initial PC, its Thumb bit set by the assembler */
    .word _OSEndRAM           /* the initial SP, which _OSRamEntry sets again */
    .byte 0xff                /* PICOBIN_BLOCK_ITEM_2BS_LAST */
    .hword 4                  /* the words of the items above */
    .byte 0
    .word 0                   /* the next block: this one, a loop of one block */
    .word 0xab123579          /* PICOBIN_BLOCK_MARKER_END */
