/* Script de link pour un firmware RP2040 entierement en SRAM.
**
** Le RP2040 demarre normalement depuis une flash QSPI externe, ce qui impose d'ecrire un
** second etage de boot de 256 octets avec son CRC. Executer depuis la SRAM evite cet
** etage — c'est ce que fait le type de build « no_flash » du SDK — et sort au passage la
** flash de la mesure de consommation, ce qui sert la variante power-aware.
**
** La table des vecteurs se trouve donc en 0x20000000 et l'application doit y pointer VTOR
** avant d'autoriser les interruptions.
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
        /* Le bootrom branche sur le premier mot de l'image : il lui faut du code, pas la
        ** table des vecteurs. Celle-ci suit, alignee sur 256 octets comme VTOR l'exige. */
        KEEP(*(.ram_entry))
        . = ALIGN(256);
        KEEP(*(.isr_vector_general))  /* exceptions systeme Cortex-Mx */
        KEEP(*(.isr_vector_specific)) /* interruptions peripheriques RP2040 */
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

/* Sommet de la pile et limite haute de l'allocation dynamique d'Escapement. */
_OSEndRAM = ORIGIN(SRAM) + LENGTH(SRAM);
