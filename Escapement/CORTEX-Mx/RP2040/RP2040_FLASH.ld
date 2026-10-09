/* Copyright (c) 2026 Bertrand Hurst. Part of Escapement, distributed under the terms of
** LICENSE at the root of this repository.
*/
/* Linker script for an RP2040 firmware run from its flash, through the XIP; RP2040_SRAM.ld
** stays that of the images loaded over SWD.
**
** The flash appears at 0x10000000 (RP2040 datasheet, 2.2), 2 MB on the Pico. Its first
** 256 bytes are the second stage of the boot, with its CRC (Escapement_Boot2.S,
** tools/rp2040_boot2.py), which the bootrom runs from SRAM; that stage configures the XIP
** and enters the image through the vector table that follows, at 0x10000100, as a reset
** would. The code and the constants stay in the flash; .data is copied into SRAM by
** _OSResetHandler and .bss cleared there. Each application points VTOR at the table, as
** the second stage already did.
*/

ENTRY(_OSResetHandler)

MEMORY
{
    FLASH (rx) : ORIGIN = 0x10000000, LENGTH = 2048K
    SRAM (rwx) : ORIGIN = 0x20000000, LENGTH = 264K
}

SECTIONS
{
    .boot2 :
    {
        KEEP(*(.boot2))
    } > FLASH

    .text :
    {
        /* 256 byte aligned as VTOR requires, where the second stage looks for it. */
        KEEP(*(.isr_vector_general))  /* Cortex-Mx system exceptions */
        KEEP(*(.isr_vector_specific)) /* RP2040 peripheral interrupts */
        *(.text*)
        *(.rodata*)
        . = ALIGN(4);
        _etext = .;
    } > FLASH

    .data :
    {
        . = ALIGN(4);
        _data = .;
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
    } > SRAM

    /* As in RP2040_SRAM.ld: the region of the MPU below the stack. */
    .stack_guard (NOLOAD) :
    {
        . = ALIGN(1024);
        _OSStackGuard = .;
        . += 1024;
        _OSStackGuardEnd = .;
    } > SRAM
}

ASSERT(SIZEOF(.boot2) == 256, "the second stage must be 256 bytes, its CRC included")
ASSERT(ADDR(.text) == 0x10000100, "the vector table must follow the second stage")

/* Top of the stack and upper bound of the dynamic allocation of Escapement. */
_OSEndRAM = ORIGIN(SRAM) + LENGTH(SRAM);
