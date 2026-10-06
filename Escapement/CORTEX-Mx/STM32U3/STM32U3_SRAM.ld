/* Copyright (c) 2026 Bertrand Hurst. Part of Escapement, distributed under the terms of
** LICENSE at the root of this repository.
*/
/* Linker script for an STM32U385 firmware running entirely from SRAM, that of the STM32U5
** port: the image is loaded by the debugger and started at its first word, the entry
** code of Escapement_RamEntry.S, which leaves the flash of the NUCLEO-U385RG-Q as it was.
**
** The vector table follows the entry code, aligned on 1024 bytes: VTOR takes a table
** aligned on the power of two above its size, and the 16 exceptions and 125 interrupts of
** the STM32U385 fill 564 bytes (ARMv8-M Architecture Reference Manual, VTOR; RM0487 rev.
** 3, table 134). OSInitializeSystemClocks points VTOR at it. SRAM1, 192 KB, and SRAM2,
** 64 KB at 0x20030000, follow one another from 0x20000000 (RM0487, table 5, p. 116;
** stm32u385xx.h). The option bytes ST ships leave both unerased by a system reset
** (FLASH_OPTR = 0x1FEF F0AA, SRAM_RST and SRAM2_RST at 1, RM0487 7.10.14, p. 280), so an
** image survives the debugger's "reset halt"; a board whose options were changed may
** not keep it.
*/
ENTRY(_OSRamEntry)

MEMORY
{
    SRAM (rwx) : ORIGIN = 0x20000000, LENGTH = 256K
}

SECTIONS
{
    .text :
    {
        KEEP(*(.ram_entry))
        . = ALIGN(1024);
        KEEP(*(.isr_vector_general))  /* Cortex-M vector table */
        KEEP(*(.isr_vector_specific)) /* STM32U3 specific vector table */
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
