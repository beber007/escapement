/* Copyright (c) 2026 Bertrand Hurst. Part of Escapement, distributed under the terms of
** LICENSE at the root of this repository.
*/
/* Linker script for an RP2350 firmware running from SRAM on the Hazard3 cores (issue
** #77), transposed from CORTEX-Mx/RP2350/RP2350_SRAM.ld.
**
** The entry code comes first, then the IMAGE_DEF the bootrom looks for, then the vector
** table, which aligns itself on 64 bytes (Escapement_RISCV_a.S). After the globals, 1 KB
** aligned on its size, _OSStackGuard, which the PMP keeps from any access
** (_OSResetHandler): a stack run past its end faults there rather than write over them.
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
        KEEP(*(.ram_entry))
        KEEP(*(.image_def))
        KEEP(*(.text._OSVectorTable))
        *(.text*)
        *(.rodata*)
        *(.srodata*)
        . = ALIGN(4);
        _etext = .;
    } > SRAM

    .data :
    {
        . = ALIGN(4);
        _data = .;
        *(.data*)
        *(.sdata*)
        . = ALIGN(4);
        _edata = .;
    } > SRAM

    _sidata = LOADADDR(.data);
    .bss :
    {
        . = ALIGN(4);
        _bss = .;
        *(.sbss*)
        *(.bss*)
        *(COMMON)
        . = ALIGN(4);
        _ebss = .;
    } > SRAM

    .stack_guard (NOLOAD) :
    {
        . = ALIGN(1024);
        _OSStackGuard = .;
        . += 1024;
        _OSStackGuardEnd = .;
    } > SRAM
}

/* Top of the stack and upper bound of the dynamic allocation of Escapement. */
_OSEndRAM = ORIGIN(SRAM) + LENGTH(SRAM);
