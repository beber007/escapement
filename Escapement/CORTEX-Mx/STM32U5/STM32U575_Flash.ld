/* Copyright (c) 2006-2012 MIS Institute of the HEIG-VD affiliated to the University of
** Applied Sciences of Western Switzerland. All rights reserved.
** Permission to use, copy, modify, and distribute this software and its documentation
** for any purpose, without fee, and without written agreement is hereby granted, pro-
** vided that the above copyright notice, the following three sentences and the authors
** appear in all copies of this software and in the software where it is used.
** IN NO EVENT SHALL THE MIS INSTITUTE NOR THE HEIG-VD NOR THE UNIVERSITY OF APPLIED
** SCIENCES OF WESTERN SWITZERLAND BE LIABLE TO ANY PARTY FOR DIRECT, INDIRECT, SPECIAL,
** INCIDENTAL, OR CONSEQUENTIAL DAMAGES ARISING OUT OF THE USE OF THIS SOFTWARE AND ITS
** DOCUMENTATION, EVEN IF THE MIS INSTITUTE OR THE HEIG-VD OR THE UNIVERSITY OF APPLIED
** SCIENCES OF WESTERN SWITZERLAND HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
** THE MIS INSTITUTE, THE HEIG-VD AND THE UNIVERSITY OF APPLIED SCIENCES OF WESTERN SWIT-
** ZERLAND SPECIFICALLY DISCLAIMS ANY WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
** IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE. THE SOFT-
** WARE PROVIDED HEREUNDER IS ON AN "AS IS" BASIS, AND THE MIS INSTITUTE NOR THE HEIG-VD
** AND NOR THE UNIVERSITY OF APPLIED SCIENCES OF WESTERN SWITZERLAND HAVE NO OBLIGATION
** TO PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR MODIFICATIONS.
** Authors: MIS-TIC
**
** Escapement - Lightweight Power-Aware Real-Time OS, derived from ZottaOS.
** Modifications Copyright (c) 2026 Bertrand Hurst, distributed under the same terms;
** see LICENSE and NOTICE at the root of this repository.
*/
/* STM32U575_Flash.ld: Memory layout of the STM32U575 examples, run from flash. With
** TrustZone off, the default of the chip, the core boots at the start of the flash,
** 0x08000000, where the vector table goes; SRAM1, SRAM2 and SRAM3, 192, 64 and 512 KB,
** follow one another from 0x20000000 (RM0456, memory map).
*/
ENTRY(_OSResetHandler)

MEMORY
{
    FLASH (rx) : ORIGIN = 0x08000000, LENGTH = 2048K
    SRAM (rwx) : ORIGIN = 0x20000000, LENGTH = 768K
}

_OSEndRAM = ORIGIN(SRAM) + LENGTH(SRAM); /* End of the RAM: top of the stack and of the heap
                                            of OSMalloc (Escapement_CortexMx.c). */

SECTIONS
{
    .text :
    {
        KEEP(*(.isr_vector_general))  /* Cortex-M vector table */
        KEEP(*(.isr_vector_specific)) /* STM32U5 specific vector table */
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


    /* Load address of .data: where its initial values sit in flash, just past .text,
    ** from which _OSResetHandler copies them into SRAM. Taking it from LOADADDR rather
    ** than from _etext keeps it exact whatever padding the linker inserts at the end of
    ** .text. */
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
