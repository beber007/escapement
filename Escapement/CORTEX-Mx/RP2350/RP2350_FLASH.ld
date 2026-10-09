/* Copyright (c) 2026 Bertrand Hurst. Part of Escapement, distributed under the terms of
** LICENSE at the root of this repository.
*/
/* Linker script for an RP2350 firmware run from its flash, through the XIP, as
** STM32U5_FLASH.ld is for the NUCLEO-U575ZI-Q; RP2350_SRAM.ld stays that of the images
** loaded over SWD.
**
** The flash appears at 0x10000000 (RP2350 datasheet, 2.2), 4 MB on the Pico 2. The
** bootrom boots it when an IMAGE_DEF block lies in its first 4 kB (Escapement_ImageDef.S),
** which here follows the vector table, and enters it at _OSRamEntry, which the block
** names. The code and the constants stay in the flash; .data is copied into SRAM by
** _OSResetHandler and .bss cleared there. Each application points VTOR at the table, in
** the flash now, as it does in SRAM.
*/

ENTRY(_OSRamEntry)

MEMORY
{
    FLASH (rx) : ORIGIN = 0x10000000, LENGTH = 4096K
    SRAM (rwx) : ORIGIN = 0x20000000, LENGTH = 520K
}

SECTIONS
{
    .text :
    {
        /* The table first, at the start of the flash, which is aligned as VTOR
        ** requires; then the block, well within the first 4 kB. */
        KEEP(*(.isr_vector_general))  /* Cortex-Mx system exceptions */
        KEEP(*(.isr_vector_specific)) /* RP2350 peripheral interrupts */
        KEEP(*(.image_def))
        KEEP(*(.ram_entry))
        /* The reset handler copies .data, the kernel with it: it stays here. */
        *(.text._OSResetHandler)
        *(EXCLUDE_FILE(*Escapement*.o) .text*)
        *(EXCLUDE_FILE(*Escapement*.o) .rodata*)
        . = ALIGN(4);
        _etext = .;
    } > FLASH

    .data :
    {
        . = ALIGN(4);
        _data = .;
        /* The kernel and the port, code and constants, run from SRAM: read through the
        ** XIP, the first rounds of the kernel, its code not yet in the cache, took up to
        ** 856 us on the Pico rather than a few, and a task overran its first period
        ** (docs/rp2040.md, "The Pico from its flash"). The application stays in the flash. */
        *Escapement*.o(.text* .rodata*)
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
}

ASSERT(ADDR(.text) == 0x10000000, "the vector table must start the flash")

/* Top of the stack and upper bound of the dynamic allocation of Escapement. */
_OSEndRAM = ORIGIN(SRAM) + LENGTH(SRAM);
