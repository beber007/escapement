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
/* File Escapement_Interrupts.h: Indices of the STM32U575 interrupt vector table, usable with
** OSSetISRDescriptor and OSGetISRDescriptor. They are the IRQ numbers of the chip (RM0456,
** the reference manual of the STM32U5, table of the NVIC; STMicroelectronics,
** cmsis-device-u5, stm32u575xx.h), the numbers left out being reserved.
** Platform version: STM32U585 (Arduino UNO Q), any STM32U5.
*/

#ifndef ESCAPEMENT_INTERRUPTS_H
#define ESCAPEMENT_INTERRUPTS_H

#define OS_IO_WWDG                     0
#define OS_IO_PVD_PVM                  1
#define OS_IO_RTC                      2
#define OS_IO_RTC_S                    3
#define OS_IO_TAMP                     4
#define OS_IO_RAMCFG                   5
#define OS_IO_FLASH                    6
#define OS_IO_FLASH_S                  7
#define OS_IO_GTZC                     8
#define OS_IO_RCC                      9
#define OS_IO_RCC_S                   10
#define OS_IO_EXTI0                   11
#define OS_IO_EXTI1                   12
#define OS_IO_EXTI2                   13
#define OS_IO_EXTI3                   14
#define OS_IO_EXTI4                   15
#define OS_IO_EXTI5                   16
#define OS_IO_EXTI6                   17
#define OS_IO_EXTI7                   18
#define OS_IO_EXTI8                   19
#define OS_IO_EXTI9                   20
#define OS_IO_EXTI10                  21
#define OS_IO_EXTI11                  22
#define OS_IO_EXTI12                  23
#define OS_IO_EXTI13                  24
#define OS_IO_EXTI14                  25
#define OS_IO_EXTI15                  26
#define OS_IO_IWDG                    27
#define OS_IO_GPDMA1_Channel0         29
#define OS_IO_GPDMA1_Channel1         30
#define OS_IO_GPDMA1_Channel2         31
#define OS_IO_GPDMA1_Channel3         32
#define OS_IO_GPDMA1_Channel4         33
#define OS_IO_GPDMA1_Channel5         34
#define OS_IO_GPDMA1_Channel6         35
#define OS_IO_GPDMA1_Channel7         36
#define OS_IO_ADC1                    37
#define OS_IO_DAC1                    38
#define OS_IO_FDCAN1_IT0              39
#define OS_IO_FDCAN1_IT1              40
#define OS_IO_TIM1_BRK                41
#define OS_IO_TIM1_UP                 42
#define OS_IO_TIM1_TRG_COM            43
#define OS_IO_TIM1_CC                 44
#define OS_IO_TIM2                    45
#define OS_IO_TIM3                    46
#define OS_IO_TIM4                    47
#define OS_IO_TIM5                    48
#define OS_IO_TIM6                    49
#define OS_IO_TIM7                    50
#define OS_IO_TIM8_BRK                51
#define OS_IO_TIM8_UP                 52
#define OS_IO_TIM8_TRG_COM            53
#define OS_IO_TIM8_CC                 54
#define OS_IO_I2C1_EV                 55
#define OS_IO_I2C1_ER                 56
#define OS_IO_I2C2_EV                 57
#define OS_IO_I2C2_ER                 58
#define OS_IO_SPI1                    59
#define OS_IO_SPI2                    60
#define OS_IO_USART1                  61
#define OS_IO_USART2                  62
#define OS_IO_USART3                  63
#define OS_IO_UART4                   64
#define OS_IO_UART5                   65
#define OS_IO_LPUART1                 66
#define OS_IO_LPTIM1                  67
#define OS_IO_LPTIM2                  68
#define OS_IO_TIM15                   69
#define OS_IO_TIM16                   70
#define OS_IO_TIM17                   71
#define OS_IO_COMP                    72
#define OS_IO_OTG_FS                  73
#define OS_IO_CRS                     74
#define OS_IO_FMC                     75
#define OS_IO_OCTOSPI1                76
#define OS_IO_PWR_S3WU                77
#define OS_IO_SDMMC1                  78
#define OS_IO_SDMMC2                  79
#define OS_IO_GPDMA1_Channel8         80
#define OS_IO_GPDMA1_Channel9         81
#define OS_IO_GPDMA1_Channel10        82
#define OS_IO_GPDMA1_Channel11        83
#define OS_IO_GPDMA1_Channel12        84
#define OS_IO_GPDMA1_Channel13        85
#define OS_IO_GPDMA1_Channel14        86
#define OS_IO_GPDMA1_Channel15        87
#define OS_IO_I2C3_EV                 88
#define OS_IO_I2C3_ER                 89
#define OS_IO_SAI1                    90
#define OS_IO_SAI2                    91
#define OS_IO_TSC                     92
#define OS_IO_RNG                     94
#define OS_IO_FPU                     95
#define OS_IO_HASH                    96
#define OS_IO_LPTIM3                  98
#define OS_IO_SPI3                    99
#define OS_IO_I2C4_ER                100
#define OS_IO_I2C4_EV                101
#define OS_IO_MDF1_FLT0              102
#define OS_IO_MDF1_FLT1              103
#define OS_IO_MDF1_FLT2              104
#define OS_IO_MDF1_FLT3              105
#define OS_IO_UCPD1                  106
#define OS_IO_ICACHE                 107
#define OS_IO_LPTIM4                 110
#define OS_IO_DCACHE1                111
#define OS_IO_ADF1                   112
#define OS_IO_ADC4                   113
#define OS_IO_LPDMA1_Channel0        114
#define OS_IO_LPDMA1_Channel1        115
#define OS_IO_LPDMA1_Channel2        116
#define OS_IO_LPDMA1_Channel3        117
#define OS_IO_DMA2D                  118
#define OS_IO_DCMI_PSSI              119
#define OS_IO_OCTOSPI2               120
#define OS_IO_MDF1_FLT4              121
#define OS_IO_MDF1_FLT5              122
#define OS_IO_CORDIC                 123
#define OS_IO_FMAC                   124
#define OS_IO_LSECSSD                125

#define OS_IO_NB_ENTRIES     126

void OSSetISRDescriptor(UINT16 entry, void *descriptor);
void *OSGetISRDescriptor(UINT16 entry);

#endif /* ESCAPEMENT_INTERRUPTS_H */
