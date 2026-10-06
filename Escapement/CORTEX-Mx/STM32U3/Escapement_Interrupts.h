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
/* File Escapement_Interrupts.h: Indices of the STM32U385 interrupt vector table, usable
** with OSSetISRDescriptor and OSGetISRDescriptor. They are the IRQ numbers of the chip,
** 0 to 124 (RM0487 rev. 3, 16.3, table 134, p. 627-632), named as there; they agree with
** STMicroelectronics' cmsis-device-u3, stm32u385xx.h, which leaves out the thirteen lines
** of peripherals the U375/385 does not have (the table's note 1: reserved there), marked
** below. 48, the TIM5 of the U5, is reserved: the timer events take TIM4.
** Platform version: STM32U385 (NUCLEO-U385RG-Q), any STM32U375/385.
*/

#ifndef ESCAPEMENT_INTERRUPTS_H
#define ESCAPEMENT_INTERRUPTS_H

#define OS_IO_WWDG                         0
#define OS_IO_PVD_PVM                      1
#define OS_IO_RTC                          2
#define OS_IO_RTC_S                        3
#define OS_IO_TAMP                         4
#define OS_IO_RAMCFG                       5
#define OS_IO_FLASH                        6
#define OS_IO_FLASH_S                      7
#define OS_IO_GTZC                         8
#define OS_IO_RCC                          9
#define OS_IO_RCC_S                       10
#define OS_IO_EXTI0                       11
#define OS_IO_EXTI1                       12
#define OS_IO_EXTI2                       13
#define OS_IO_EXTI3                       14
#define OS_IO_EXTI4                       15
#define OS_IO_EXTI5                       16
#define OS_IO_EXTI6                       17
#define OS_IO_EXTI7                       18
#define OS_IO_EXTI8                       19
#define OS_IO_EXTI9                       20
#define OS_IO_EXTI10                      21
#define OS_IO_EXTI11                      22
#define OS_IO_EXTI12                      23
#define OS_IO_EXTI13                      24
#define OS_IO_EXTI14                      25
#define OS_IO_EXTI15                      26
#define OS_IO_IWDG                        27
#define OS_IO_SAES                        28
#define OS_IO_GPDMA1_CH0                  29
#define OS_IO_GPDMA1_CH1                  30
#define OS_IO_GPDMA1_CH2                  31
#define OS_IO_GPDMA1_CH3                  32
#define OS_IO_GPDMA1_CH4                  33
#define OS_IO_GPDMA1_CH5                  34
#define OS_IO_GPDMA1_CH6                  35
#define OS_IO_GPDMA1_CH7                  36
#define OS_IO_ADC1                        37
#define OS_IO_DAC1                        38
#define OS_IO_FDCAN1_IT0                  39
#define OS_IO_FDCAN1_IT1                  40
#define OS_IO_TIM1_BRK_TERR_IERR          41
#define OS_IO_TIM1_UP                     42
#define OS_IO_TIM1_TRG_COM_DIR_IDX        43
#define OS_IO_TIM1_CC                     44
#define OS_IO_TIM2                        45
#define OS_IO_TIM3                        46
#define OS_IO_TIM4                        47
#define OS_IO_TIM6                        49
#define OS_IO_TIM7                        50
#define OS_IO_TIM12                       51   /* not on the U375/385 */
#define OS_IO_I3C1_EV                     53
#define OS_IO_I3C1_ER                     54
#define OS_IO_I2C1_EV                     55
#define OS_IO_I2C1_ER                     56
#define OS_IO_I2C2_EV                     57
#define OS_IO_I2C2_ER                     58
#define OS_IO_SPI1                        59
#define OS_IO_SPI2                        60
#define OS_IO_USART1                      61
#define OS_IO_USART2                      62   /* not on the U375/385 */
#define OS_IO_USART3                      63
#define OS_IO_UART4                       64
#define OS_IO_UART5                       65
#define OS_IO_LPUART1                     66
#define OS_IO_LPTIM1                      67
#define OS_IO_LPTIM2                      68
#define OS_IO_TIM15                       69
#define OS_IO_TIM16                       70
#define OS_IO_TIM17                       71
#define OS_IO_COMP                        72
#define OS_IO_USB                         73
#define OS_IO_CRS                         74
#define OS_IO_OCTOSPI1                    76
#define OS_IO_HSP1                        77   /* not on the U375/385 */
#define OS_IO_SDMMC1                      78
#define OS_IO_GPDMA1_CH8                  80
#define OS_IO_GPDMA1_CH9                  81
#define OS_IO_GPDMA1_CH10                 82
#define OS_IO_GPDMA1_CH11                 83
#define OS_IO_I2C3_EV                     88
#define OS_IO_I2C3_ER                     89
#define OS_IO_SAI1                        90
#define OS_IO_TSC                         92
#define OS_IO_AES                         93
#define OS_IO_RNG                         94
#define OS_IO_FPU                         95
#define OS_IO_HASH                        96
#define OS_IO_PKA                         97
#define OS_IO_LPTIM3                      98
#define OS_IO_SPI3                        99
#define OS_IO_I3C2_EV                    100
#define OS_IO_I3C2_ER                    101
#define OS_IO_TIM8_BRK_TERR_IERR         102   /* not on the U375/385 */
#define OS_IO_TIM8_UP                    103   /* not on the U375/385 */
#define OS_IO_TIM8_TRG_COM_DIR_IDX       104   /* not on the U375/385 */
#define OS_IO_TIM8_CC                    105   /* not on the U375/385 */
#define OS_IO_ICACHE                     107
#define OS_IO_LCD                        109   /* not on the U375/385 */
#define OS_IO_LPTIM4                     110
#define OS_IO_ADF1                       112
#define OS_IO_ADC2                       113
#define OS_IO_FDCAN2_IT0                 114   /* not on the U375/385 */
#define OS_IO_FDCAN2_IT1                 115   /* not on the U375/385 */
#define OS_IO_I2C4_EV                    116   /* not on the U375/385 */
#define OS_IO_I2C4_ER                    117   /* not on the U375/385 */
#define OS_IO_SPI4                       119   /* not on the U375/385 */
#define OS_IO_PWR                        123
#define OS_IO_PWR_S                      124

#define OS_IO_NB_ENTRIES     125

void OSSetISRDescriptor(UINT16 entry, void *descriptor);
void *OSGetISRDescriptor(UINT16 entry);

#endif /* ESCAPEMENT_INTERRUPTS_H */
