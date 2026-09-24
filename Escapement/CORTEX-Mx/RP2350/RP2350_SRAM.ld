/* Copyright (c) 2026 Bertrand Hurst. Part of Escapement, distributed under the terms of
** LICENSE at the root of this repository.
*/
/* Linker script for an RP2350 firmware running entirely from SRAM, transposed from
** RP2040_SRAM.ld.
**
** The image is loaded into SRAM by the debugger and started at its first word, as on the
** RP2040: nothing is written to the flash, which the bootrom of the RP2350 would only boot
** with a block describing the image; the flash also stays out of the power measurement.
**
** The vector table follows the entry code, in SRAM, and the application must point VTOR
** at it before enabling interrupts.
*/

ENTRY(_OSRamEntry)

MEMORY
{
    SRAM (rwx) : ORIGIN = 0x20000000, LENGTH = 520K
}

SECTIONS
{
    .text :
    {
        /* The debugger starts the image at its first word: it needs code there, not the
        ** vector table. That table follows, aligned as VTOR requires: on a power of two
        ** no smaller than the table, which with 16 system entries and 52 interrupts takes
        ** 272 bytes, hence 512, where the 42 entries of the RP2040 fitted in 256. */
        KEEP(*(.ram_entry))
        . = ALIGN(512);
        KEEP(*(.isr_vector_general))  /* Cortex-Mx system exceptions */
        KEEP(*(.isr_vector_specific)) /* RP2350 peripheral interrupts */
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
