/* Copyright (c) 2026 Bertrand Hurst. All rights reserved.
** Escapement - Lightweight Power-Aware Real-Time OS.
** Derived from prior work; see LICENSE and NOTICE at the root of this repository.
*/
/* File Escapement_Interrupts.h: Indices of the RP2040 interrupt vector table, usable with
** OSSetISRDescriptor and OSGetISRDescriptor.
** Platform version: RP2040 (Raspberry Pi Pico).
*/

#ifndef ESCAPEMENT_INTERRUPTS_H
#define ESCAPEMENT_INTERRUPTS_H

#define OS_IO_TIMER_0        0
#define OS_IO_TIMER_1        1
#define OS_IO_TIMER_2        2
#define OS_IO_TIMER_3        3
#define OS_IO_PWM_WRAP       4
#define OS_IO_USBCTRL        5
#define OS_IO_XIP            6
#define OS_IO_PIO0_0         7
#define OS_IO_PIO0_1         8
#define OS_IO_PIO1_0         9
#define OS_IO_PIO1_1        10
#define OS_IO_DMA_0         11
#define OS_IO_DMA_1         12
#define OS_IO_BANK0         13
#define OS_IO_QSPI          14
#define OS_IO_SIO_PROC0     15
#define OS_IO_SIO_PROC1     16
#define OS_IO_CLOCKS        17
#define OS_IO_SPI0          18
#define OS_IO_SPI1          19
#define OS_IO_UART0         20
#define OS_IO_UART1         21
#define OS_IO_ADC_FIFO      22
#define OS_IO_I2C0          23
#define OS_IO_I2C1          24
#define OS_IO_RTC           25

#define OS_IO_NB_ENTRIES    26

void OSSetISRDescriptor(UINT16 entry, void *descriptor);
void *OSGetISRDescriptor(UINT16 entry);

#endif /* ESCAPEMENT_INTERRUPTS_H */
