/* Copyright (c) 2026 Bertrand Hurst. All rights reserved.
** Escapement - Lightweight Power-Aware Real-Time OS.
** Derived from prior work; see LICENSE and NOTICE at the root of this repository.
*/
/* ESCAPEMENT_CortexM3_STM32F103RC_Flash.ld: 
** Version date: March 2012
*/
ENTRY(_OSResetHandler)

MEMORY
{
    FLASH (rx) : ORIGIN = 0x00000000, LENGTH = 256K
    SRAM (rwx) : ORIGIN = 0x20000000, LENGTH = 48K
}

_OSEndRAM = ORIGIN(SRAM) + LENGTH(SRAM); /* Need to initialize stack pointer and Escapementos
                                            dynamic allocation system (OSMalloc) in
                                            Escapement_CortexM3.c file. */

SECTIONS
{
    .text :
    {
        KEEP(*(.isr_vector_general))  /* Cortex-M3 vector table */
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


    /* Load address of .data: where the initial values sit in the image. On a
    ** target running from flash this is just past .text; on one linked into SRAM
    ** it is the run address itself, and the copy below becomes a no-op. Taking it
    ** from LOADADDR rather than from _etext keeps it exact whatever padding the
    ** linker inserts at the end of .text. */
    _sidata = LOADADDR(.data);
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
