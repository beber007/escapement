/* Copyright (c) 2026 Bertrand Hurst. Part of Escapement, distributed under the terms of
** LICENSE at the root of this repository.
*/
/* Linker script for an RP2040 firmware running entirely from SRAM.
**
** The RP2040 normally boots from an external QSPI flash, which requires a 256 byte second
** stage bootloader with its CRC. Executing from SRAM avoids that stage — this is what the
** no_flash build type of the SDK does — and incidentally takes the flash out of the power
** measurement, which serves the power-aware variant.
**
** The vector table follows the entry code, in SRAM, and the application must point VTOR
** at it before enabling interrupts.
*/

ENTRY(_OSRamEntry)

MEMORY
{
    SRAM (rwx) : ORIGIN = 0x20000000, LENGTH = 264K
}

SECTIONS
{
    .text :
    {
        /* The debugger starts the image at its first word: it needs code there, not the
        ** vector table. That table follows, 256 byte aligned as VTOR requires. */
        KEEP(*(.ram_entry))
        . = ALIGN(256);
        KEEP(*(.isr_vector_general))  /* Cortex-Mx system exceptions */
        KEEP(*(.isr_vector_specific)) /* RP2040 peripheral interrupts */
        *(.text*)
        *(.rodata*)
        . = ALIGN(4);
        _etext = .;
    } > SRAM

    .data :
    {
        . = ALIGN(4);
        _data = .;
        *(.data*)
        . = ALIGN(4);
        _edata = .;
    } > SRAM


    /* Load address of .data: where the initial values sit in the image. On a
    ** target running from flash this is just past .text; on one linked into SRAM
    ** it is the run address itself, and the copy below becomes a no-op. Taking it
    ** from LOADADDR rather than from _etext keeps it exact whatever padding the
    ** linker inserts at the end of .text. */
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
}

/* Top of the stack and upper bound of the dynamic allocation of Escapement. */
_OSEndRAM = ORIGIN(SRAM) + LENGTH(SRAM);
