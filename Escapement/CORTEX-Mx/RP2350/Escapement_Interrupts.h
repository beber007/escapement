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
/* File Escapement_Interrupts.h: Indices of the RP2350 interrupt vector table, usable with
** OSSetISRDescriptor and OSGetISRDescriptor. They are the IRQ numbers of the chip (RP2350
** datasheet; pico-sdk, src/rp2350/hardware_regs/include/hardware/regs/intctrl.h).
** Platform version: RP2350 (Raspberry Pi Pico 2).
*/

#ifndef ESCAPEMENT_INTERRUPTS_H
#define ESCAPEMENT_INTERRUPTS_H

#define OS_IO_TIMER_0         0   /* TIMER0, alarms 0 to 3 */
#define OS_IO_TIMER_1         1
#define OS_IO_TIMER_2         2
#define OS_IO_TIMER_3         3
#define OS_IO_TIMER1_0        4   /* TIMER1, alarms 0 to 3 */
#define OS_IO_TIMER1_1        5
#define OS_IO_TIMER1_2        6
#define OS_IO_TIMER1_3        7
#define OS_IO_PWM_WRAP_0      8
#define OS_IO_PWM_WRAP_1      9
#define OS_IO_DMA_0          10
#define OS_IO_DMA_1          11
#define OS_IO_DMA_2          12
#define OS_IO_DMA_3          13
#define OS_IO_USBCTRL        14
#define OS_IO_PIO0_0         15
#define OS_IO_PIO0_1         16
#define OS_IO_PIO1_0         17
#define OS_IO_PIO1_1         18
#define OS_IO_PIO2_0         19
#define OS_IO_PIO2_1         20
#define OS_IO_BANK0          21
#define OS_IO_BANK0_NS       22
#define OS_IO_QSPI           23
#define OS_IO_QSPI_NS        24
#define OS_IO_SIO_FIFO       25
#define OS_IO_SIO_BELL       26
#define OS_IO_SIO_FIFO_NS    27
#define OS_IO_SIO_BELL_NS    28
#define OS_IO_SIO_MTIMECMP   29
#define OS_IO_CLOCKS         30
#define OS_IO_SPI0           31
#define OS_IO_SPI1           32
#define OS_IO_UART0          33
#define OS_IO_UART1          34
#define OS_IO_ADC_FIFO       35
#define OS_IO_I2C0           36
#define OS_IO_I2C1           37
#define OS_IO_OTP            38
#define OS_IO_TRNG           39
#define OS_IO_PROC0_CTI      40
#define OS_IO_PROC1_CTI      41
#define OS_IO_PLL_SYS        42
#define OS_IO_PLL_USB        43
#define OS_IO_POWMAN_POW     44
#define OS_IO_POWMAN_TIMER   45
#define OS_IO_SPARE_0        46
#define OS_IO_SPARE_1        47
#define OS_IO_SPARE_2        48
#define OS_IO_SPARE_3        49
#define OS_IO_SPARE_4        50
#define OS_IO_SPARE_5        51

#define OS_IO_NB_ENTRIES     52

void OSSetISRDescriptor(UINT16 entry, void *descriptor);
void *OSGetISRDescriptor(UINT16 entry);

#endif /* ESCAPEMENT_INTERRUPTS_H */
