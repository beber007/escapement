/* Copyright (c) 2026 Bertrand Hurst. Part of Escapement, distributed under the terms of
** LICENSE at the root of this repository.
*/
/* Linker script for an STM32U5 firmware running entirely from SRAM, transposed from
** RP2350_SRAM.ld, for the Arduino UNO Q: its flash holds Arduino's bootloader and
** firmware, which a load into SRAM leaves untouched, the board coming back to them at the
** next reset.
**
** The image is loaded by the debugger and started at its first word, the entry code of
** Escapement_RamEntry.S. The vector table follows it, aligned on 1024 bytes: VTOR takes a
** table aligned on the power of two above its size, and the 16 exceptions and 126
** interrupts of the STM32U575/U585 fill 568 bytes (ARMv8-M Architecture Reference Manual,
** VTOR). OSInitializeSystemClocks points VTOR at it. SRAM1, SRAM2 and SRAM3, 192, 64
** and 512 KB, follow one another from 0x20000000 (RM0456, memory map).
*/
ENTRY(_OSRamEntry)

MEMORY
{
    SRAM (rwx) : ORIGIN = 0x20000000, LENGTH = 768K
}

SECTIONS
{
    .text :
    {
        KEEP(*(.ram_entry))
        . = ALIGN(1024);
        KEEP(*(.isr_vector_general))  /* Cortex-M vector table */
        KEEP(*(.isr_vector_specific)) /* STM32U5 specific vector table */
        *(.text*)
        *(.rodata*)
        . = ALIGN(4);
        _etext = .;
    } > SRAM

    .data :
    {
        . = ALIGN(4);
        _data = .;
        *(vtable)
        *(.data*)
        . = ALIGN(4);
        _edata = .;
    } > SRAM

    /* Load address of .data: the run address itself, linked into SRAM, and the copy of
    ** _OSResetHandler a no-op. */
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
