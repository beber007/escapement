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
/* File Escapement_Interrupts.h: Indices of the RP2040 interrupt vector table, usable with
** OSSetISRDescriptor and OSGetISRDescriptor. They are the IRQ numbers of the chip (RP2040
** datasheet; pico-sdk, src/rp2040/hardware_regs/include/hardware/regs/intctrl.h).
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
