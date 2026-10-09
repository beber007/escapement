/* Copyright (c) 2026 Bertrand Hurst. Part of Escapement, distributed under the terms of
** LICENSE at the root of this repository.
*/
/* Linker script for an STM32U385 firmware run from its flash, STM32U5_FLASH.ld on the
** NUCLEO-U385RG-Q: for its current, measured after a power cycle with no debugger
** connected since, which an image in SRAM would not survive (docs/stm32u3.md).
**
** The chip boots from 0x08000000, FLASH_BOOT0R at the value ST ships, 0x0800007F
** (RM0487, 7.10.16, p. 285), the board's to confirm: the vector table comes first, its
** first word the top of the stack and its second _OSResetHandler, which copies .data from
** the flash into SRAM and clears .bss (Escapement_CortexMx.c). The code and the constants
** stay in the flash, 1 MB on the U385RG, then SRAM1 and SRAM2 from 0x20000000, 256 KB
** (RM0487, table 5, p. 116). OSInitializeSystemClocks points VTOR at the table, as for an
** image in SRAM, and turns the instruction cache on (8.7.1, p. 320).
*/
ENTRY(_OSResetHandler)

MEMORY
{
    FLASH (rx)  : ORIGIN = 0x08000000, LENGTH = 1024K
    SRAM (rwx)  : ORIGIN = 0x20000000, LENGTH = 256K
}

SECTIONS
{
    .text :
    {
        KEEP(*(.isr_vector_general))  /* Cortex-M vector table */
        KEEP(*(.isr_vector_specific)) /* STM32U3 specific vector table */
        *(.text*)
        *(.rodata*)
        . = ALIGN(4);
        _etext = .;
    } > FLASH

    .data :
    {
        . = ALIGN(4);
        _data = .;
        *(vtable)
        *(.data*)
        . = ALIGN(4);
        _edata = .;
    } > SRAM AT > FLASH

    /* Load address of .data, in the flash, which _OSResetHandler copies from. */
    _sidata = LOADADDR(.data);

    .bss :
    {
        . = ALIGN(4);
        _bss = .;
        *(.bss*)
        *(COMMON)
        . = ALIGN(4);
        _ebss = .;
        _heap = .;
    } > SRAM
}

/* Top of the stack and upper bound of the dynamic allocation of Escapement. */
_OSEndRAM = ORIGIN(SRAM) + LENGTH(SRAM);
