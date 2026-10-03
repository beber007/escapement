/* Copyright (c) 2026 Bertrand Hurst. Part of Escapement, distributed under the terms of
** LICENSE at the root of this repository.
*/
/* Linker script for an STM32U5 firmware run from its flash, for a board whose flash may
** be written: the NUCLEO-U575ZI-Q, whose current is measured after a power cycle with no
** debugger connected since, which an image in SRAM would not survive (docs/stm32u5.md).
** STM32U5_SRAM.ld stays the UNO Q's, whose flash holds Arduino's firmware.
**
** The chip boots from 0x08000000 (FLASH_OPTR.nBOOT0 and NSBOOTADD0R, 0x0800007F on the
** NUCLEO, read on 2026-10-03): the vector table comes first, its first word the top of
** the stack and its second _OSResetHandler, which copies .data from the flash into SRAM
** and clears .bss (Escapement_CortexMx.c). The code and the constants stay in the flash;
** 2 MB on the U575 and the U585, then SRAM1 to SRAM3 from 0x20000000, 768 KB (RM0456,
** memory map). OSInitializeSystemClocks points VTOR at the table, as for an image in SRAM.
*/
ENTRY(_OSResetHandler)

MEMORY
{
    FLASH (rx)  : ORIGIN = 0x08000000, LENGTH = 2048K
    SRAM (rwx)  : ORIGIN = 0x20000000, LENGTH = 768K
}

SECTIONS
{
    .text :
    {
        KEEP(*(.isr_vector_general))  /* Cortex-M vector table */
        KEEP(*(.isr_vector_specific)) /* STM32U5 specific vector table */
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
