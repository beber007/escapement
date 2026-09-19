/* Copyright (c) 2026 Bertrand Hurst. All rights reserved.
** Escapement - Lightweight Power-Aware Real-Time OS.
** Derived from prior work; see LICENSE and NOTICE at the root of this repository.
*/
/* ESCAPEMENT_CortexM3_STM32L152RB_Flash.ld: 
** Version date: June 2012
*/
ENTRY(_OSResetHandler)

MEMORY
{
    FLASH (rx) : ORIGIN = 0x00000000, LENGTH = 128K
    SRAM (rwx) : ORIGIN = 0x20000000, LENGTH = 16K
}

_OSEndRAM = ORIGIN(SRAM) + LENGTH(SRAM); /* Need to initialize stack pointer and Escapementos
                                            dynamic allocation system (OSMalloc) in
                                            Escapement_CortexMx.c file. */

SECTIONS
{
    .text :
    {
        KEEP(*(.isr_vector_general))  /* Cortex-M0 vector table */
        KEEP(*(.isr_vector_specific)) /* STM32 specific vector table */
        *(.text*)
        *(.rodata*)
        _etext = .;
    } > FLASH

    .data : AT (ADDR(.text) + SIZEOF(.text))
    {
        _data = .;
        *(vtable)
        *(.data*)
        _edata = .;
    } > SRAM

    .eh_frame :
    {
        KEEP (*(.eh_frame))
    } > SRAM

    .ARM.exidx :
    {
        *(.ARM.exidx* .gnu.linkonce.armexidx.*)
    } > SRAM
    
    .bss :
    {
        _bss = .;
        *(.bss*)
        *(COMMON)
        _ebss = .;
        . = ALIGN(4);
        _heap = .;
    } > SRAM
}
