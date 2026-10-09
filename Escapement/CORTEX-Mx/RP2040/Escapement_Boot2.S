/* Copyright (c) 2026 Bertrand Hurst. All rights reserved.
** Escapement - Lightweight Power-Aware Real-Time OS.
** Distributed under the terms of LICENSE at the root of this repository.
*/
/* File Escapement_Boot2.S: The second stage of the boot of a firmware in flash.
**
** The bootrom of the RP2040 reads the first 256 bytes of the flash with 03h serial reads,
** checks the CRC32 of the first 252 against the last 4, copies them into SRAM and jumps
** into them (RP2040 datasheet, 2.8.1.3); tools/rp2040_boot2.py appends that CRC. This
** stage's only job is to configure the SSI for reading the flash through the XIP, then
** to enter the image through its vector table, which RP2040_FLASH.ld places just after.
**
** The read is the W25Q16JV's Fast Read Quad Output, 6Bh: the instruction and the 24 bits
** of address in serial, eight dummy clocks, then the data on the four lines (W25Q16JV
** datasheet, revision D, 8.2, "Fast Read Quad Output (6Bh)"). Its Fast Read Quad I/O,
** EBh, sends the address on four lines too, and the pico-sdk's boot2 uses it in a
** continuous read mode, M7-0 = A0h, that drops the instruction from every read after the
** first; that datasheet has M7-0 set to Fxh and describes no such mode, so it is not
** used. 6Bh needs the Quad Enable bit of Status Register-2 (7.1), set here as a volatile
** bit, 50h then 31h (8.2, "Write Enable for Volatile Status Register (50h)" and "Write
** Status Register-1 (01h), Status Register-2 (31h)..."), at each boot: the flash's
** non-volatile bits are never written.
**
** The SSI runs at clk_sys / 4: 31.25 MHz at the 125 MHz of the kernel, well under the
** 104 MHz of the W25Q16JV (9.6, FR), and the bootrom's ring oscillator's around 6 MHz
** gives 1.5 MHz until the kernel takes the PLL. At that rate the pads are set first, as
** the pico-sdk's boot2 sets them: SCLK driven at 8 mA with a fast slew, the Schmitt
** triggers of SD0 to SD3 off, which delay the data read (RP2040 datasheet, 2.19,
** PADS_QSPI).
**
** It runs from SRAM, at whatever address the bootrom copied it to, and so loads only
** relative to the PC. At most 252 bytes.
**
** Platform version: RP2040 (Raspberry Pi Pico).
*/

    .syntax unified
    .cpu cortex-m0plus
    .thumb

    .equ PADS_QSPI,     0x40020000
    .equ PADS_SCLK,     0x04
    .equ PADS_SD0,      0x08
    .equ PADS_CLR,      0x3000          /* atomic clear alias (2.1.2) */
    .equ PADS_SCLK_FAST, ((1 << 6) | (2 << 4) | 1) /* IE, DRIVE 8 mA, SLEWFAST */
    .equ PADS_SCHMITT,  0x02

    .equ SSI_BASE,      0x18000000
    .equ CTRLR0,        0x00
    .equ CTRLR1,        0x04
    .equ SSIENR,        0x08
    .equ BAUDR,         0x14
    .equ SR,            0x28
    .equ DR0,           0x60
    .equ SPI_CTRLR0,    0xf4
    .equ SR_BUSY,       0x01
    .equ SR_TFE,        0x04

    /* CTRLR0 (4.10.13): DFS_32 in 20:16, TMOD in 9:8, SPI_FRF in 22:21. */
    .equ CTRLR0_STD_8,  (7 << 16)                          /* standard SPI, 8 bits, TX and RX */
    .equ CTRLR0_XIP,    ((2 << 21) | (31 << 16) | (3 << 8)) /* quad, 32 bits, EEPROM read */
    /* SPI_CTRLR0: the instruction 6Bh, 8 wait cycles, an 8-bit instruction, 24 bits of
    ** address (ADDR_L 6), both sent in standard SPI (TRANS_TYPE 0). */
    .equ SPI_CTRLR0_XIP, ((0x6b << 24) | (8 << 11) | (2 << 8) | (6 << 2))

    .section .boot2, "ax"
    .align 2
    .global _OSBoot2
    .thumb_func
_OSBoot2:
    ldr  r3, =PADS_QSPI
    movs r0, #PADS_SCLK_FAST
    str  r0, [r3, #PADS_SCLK]
    ldr  r3, =PADS_QSPI + PADS_CLR + PADS_SD0
    movs r0, #PADS_SCHMITT
    str  r0, [r3, #0]           /* SD0 to SD3, one word apart */
    str  r0, [r3, #4]
    str  r0, [r3, #8]
    str  r0, [r3, #12]
    ldr  r3, =SSI_BASE
    movs r0, #0
    str  r0, [r3, #SSIENR]      /* the SSI is configured only while disabled */
    movs r0, #4
    str  r0, [r3, #BAUDR]
    ldr  r0, =CTRLR0_STD_8
    str  r0, [r3, #CTRLR0]
    movs r0, #1
    str  r0, [r3, #SSIENR]

    /* Quad Enable, volatile: 50h, then 31h 02h. The SSI raises /CS when its FIFO runs
    ** dry, between the two once the first is through. */
    movs r0, #0x50
    str  r0, [r3, #DR0]
    bl   wait
    ldr  r0, [r3, #DR0]
    movs r0, #0x31
    str  r0, [r3, #DR0]
    movs r0, #0x02
    str  r0, [r3, #DR0]
    bl   wait
    ldr  r0, [r3, #DR0]
    ldr  r0, [r3, #DR0]

    /* The XIP: each read of the flash's window made a 6Bh read of 32 bits. */
    movs r0, #0
    str  r0, [r3, #SSIENR]
    ldr  r0, =CTRLR0_XIP
    str  r0, [r3, #CTRLR0]
    movs r0, #0
    str  r0, [r3, #CTRLR1]      /* NDF: one data frame per read */
    ldr  r0, =SPI_CTRLR0_XIP
    ldr  r1, =SSI_BASE + SPI_CTRLR0
    str  r0, [r1]
    movs r0, #1
    str  r0, [r3, #SSIENR]

    /* Enter the image as a reset would: VTOR, the stack and the reset handler from the
    ** table. */
    ldr  r0, =0x10000100
    ldr  r1, =0xE000ED08        /* VTOR */
    str  r0, [r1]
    ldmia r0!, {r1, r2}
    msr  msp, r1
    bx   r2

/* wait: until the SSI has sent its FIFO and is no longer busy. */
    .thumb_func
wait:
    ldr  r1, [r3, #SR]
    movs r2, #SR_TFE
    tst  r1, r2
    beq  wait
    movs r2, #SR_BUSY
    tst  r1, r2
    bne  wait
    bx   lr

    .align 2
    .ltorg
