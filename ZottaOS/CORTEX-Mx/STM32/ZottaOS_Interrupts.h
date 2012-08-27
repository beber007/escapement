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
*/
/* File ZottaOS_Interrupts.h: Holds the peripheral device interrupt entries.
** Platform version: All STM32 microcontrollers.
** Version date: March 2012
** Authors: MIS-TIC
*/

#ifndef _ZOTTAOS_INTERRUPTS_
#define _ZOTTAOS_INTERRUPTS_

#include "ZottaOS_Config.h"

/* ------------------------------ Common to all STM32XXX ---------------------------- */
#define OS_IO_WWDG                     0 /* Window watchdog */
#define OS_IO_PVD                      1 /* Programmable voltage detector */
#if defined(STM32F05XXX)
  #define OS_IO_RTC                    2 /* RTC through EXTI line */
#elif defined(STM32L1XXXX)
  #define OS_IO_TAMPER_STAMP           2 /* Tamper and timestamp through EXTI line */
#elif defined(STM32F2XXXX) || defined(STM32F4XXXX)
  #define OS_IO_TAMP_STAMP             2 /* Tamper and timestamp through EXTI line */
#elif defined(STM32F1XXXX)
  #define OS_IO_TAMPER                 2 /* Tamper detection */
#else
  #error STM32 version undefined
#endif
#if defined(STM32F05XXX)
  #define OS_IO_FLASH                  3 /* Flash memory */
#elif defined(STM32L1XXXX) || defined(STM32F2XXXX) || defined(STM32F4XXXX)
  #define OS_IO_RTC_WKUP               3 /* RTC wake-up through EXTI line */
#elif defined(STM32F1XXXX)
  #define OS_IO_RTC                    3 /* RTC (global) */
#else
  #error STM32 version undefined
#endif
#if defined(STM32F05XXX)
  #define OS_IO_RCC                    4 /* Reset and clock control */
  #define OS_IO_EXTI0_1                5 /* EXTI lines 0 and 1 */
  #define OS_IO_EXTI2_3                6 /* EXTI lines 2 and 3 */
  #define OS_IO_EXTI4_15               7 /* EXTI lines 4 to 15 */
  #define OS_IO_TS                     8 /* Touch sensing */
  #define OS_IO_DMA1_Channel1          9 /* DMA 1 channel 1 */
  #define OS_IO_DMA1_Channel2_3       10 /* DMA 1 channels 2 and 3 */
#elif defined(STM32L1XXXX) || defined(STM32F1XXXX) || defined(STM32F2XXXX) || defined(STM32F4XXXX)
  #define OS_IO_FLASH                  4 /* Flash memory (global) */
  #define OS_IO_RCC                    5 /* Reset and clock control (global) */
  #define OS_IO_EXTI0                  6 /* EXTI line 0 */
  #define OS_IO_EXTI1                  7 /* EXTI line 1 */
  #define OS_IO_EXTI2                  8 /* EXTI line 2 */
  #define OS_IO_EXTI3                  9 /* EXTI line 3 */
  #define OS_IO_EXTI4                 10 /* EXTI line 4 */
#else
  #error STM32 version undefined
#endif
#if defined(STM32F05XXX)
  #define OS_IO_DMA1_Channel4_5       11 /* DMA 1 channels 4 and 5 */
  #define OS_IO_ADC1_COMP             12 /* ADC 1, and comparators 1 and 2 */
  #define OS_IO_TIM1_BRK_UP_TRG_COM   13 /* Timer 1 break, update, trigger & commutation */
  #define OS_IO_TIM1_UP               OS_IO_TIM1_BRK_UP_TRG_COM
  #define OS_IO_TIM1_CC               14 /* Timer 1 capture/compare */
  #define OS_IO_TIM1                  OS_IO_TIM1_CC
  #define OS_IO_TIM2                  15 /* Timer 2 */
  #define OS_IO_TIM3                  16 /* Timer 3 */
  #if defined(STM32F051X4) || defined(STM32F051K6_K8_C6_R6) || defined(STM32F051C8_R8)
     #define OS_IO_TIM6_DAC           17 /* Timer 6 and DAC */
  #endif
#elif defined(STM32L1XXXX) || defined(STM32F1XXXX)
  #define OS_IO_DMA1_Channel1         11 /* DMA 1 channel 1 (global) */
  #define OS_IO_DMA1_Channel2         12 /* DMA 1 channel 2 (global) */
  #define OS_IO_DMA1_Channel3         13 /* DMA 1 channel 3 (global) */
  #define OS_IO_DMA1_Channel4         14 /* DMA 1 channel 4 (global) */
  #define OS_IO_DMA1_Channel5         15 /* DMA 1 channel 5 (global) */
  #define OS_IO_DMA1_Channel6         16 /* DMA 1 channel 6 (global) */
  #define OS_IO_DMA1_Channel7         17 /* DMA 1 channel 7 (global) */
#elif defined(STM32F2XXXX) || defined(STM32F4XXXX)
  #define OS_IO_DMA1_Stream0          11 /* DMA 1 stream 0 (global) */
  #define OS_IO_DMA1_Stream1          12 /* DMA 1 stream 1 (global) */
  #define OS_IO_DMA1_Stream2          13 /* DMA 1 stream 2 (global) */
  #define OS_IO_DMA1_Stream3          14 /* DMA 1 stream 3 (global) */
  #define OS_IO_DMA1_Stream4          15 /* DMA 1 stream 4 (global) */
  #define OS_IO_DMA1_Stream5          16 /* DMA 1 stream 5 (global) */
  #define OS_IO_DMA1_Stream6          17 /* DMA 1 stream 6 (global) */
#else
  #error STM32 version undefined
#endif
/* ------------------------------ STM32F0 ------------------------------------------- */
#if defined(STM32F05XXX)
  #define OS_IO_TIM14                 19 /* Timer 14 */
  #if defined(STM32F051X4) || defined(STM32F051K6_K8_C6_R6) || defined(STM32F051C8_R8)
     #define OS_IO_TIM15              20 /* Timer 15 */
  #endif
  #define OS_IO_TIM16                 21 /* Timer 16 */
  #define OS_IO_TIM17                 22 /* Timer 17 */
  #define OS_IO_I2C1                  23 /* I2C 1 */
  #if defined(STM32F051C8_R8)
     #define OS_IO_I2C2               24 /* I2C 2 */
  #endif
  #define OS_IO_SPI1                  25 /* SPI 1 */
  #if defined(STM32F051C8_R8)
     #define OS_IO_SPI2               26 /* SPI 2 */
  #endif
  #define OS_IO_USART1                27 /* USART 1 */
  #if defined(STM32F051K6_K8_C6_R6) || defined(STM32F051C8_R8)
     #define OS_IO_USART2             28 /* USART 2 */
  #endif
  #if defined(STM32F051X4) || defined(STM32F051K6_K8_C6_R6) || defined(STM32F051C8_R8)
     #define OS_IO_CEC                30 /* High-definition multimedia interface (HDMI)/
                                            consumer electronics control (CEC) */
  #endif
/* ------------------------------ STM32L1 ------------------------------------------- */
#elif defined(STM32L1XXXX)
  #define OS_IO_ADC1                  18 /* ADC 1 (global) */
  #define OS_IO_USB_HP                19 /* USB device high priority */
  #define OS_IO_USB_LP                20 /* USB device low priority */
  #define OS_IO_DAC                   21 /* DAC */
  #if defined(STM32L151XC) || defined(STM32L152XC) || defined(STM32L151XD) || \
      defined(STM32L152XD) || defined(STM32L162XD)
     #define OS_IO_COMP_CA            22 /* Comparator and channel acquisition
                                            through EXTI line */
  #else
     #define OS_IO_COMP               22 /* Comparator through EXTI line */
  #endif
  #define OS_IO_EXTI9_5               23 /* External lines 5 to 9 */
  #if defined(STM32L152X6_X8_XB) || defined(STM32L152XC) || defined(STM32L152XD) || \
      defined(STM32L162XD)
     #define OS_IO_LCD                24 /* LCD */
  #endif
  #define OS_IO_TIM9                  25 /* Timer 9 (global) */
  #define OS_IO_TIM10                 26 /* Timer 10 (global) */
  #define OS_IO_TIM11                 27 /* Timer 11 (global) */
  #define OS_IO_TIM2                  28 /* Timer 2 (global) */
  #define OS_IO_TIM3                  29 /* Timer 3 (global) */
  #define OS_IO_TIM4                  30 /* Timer 4 (global) */
  #define OS_IO_I2C1_EV               31 /* I2C 1 event */
  #define OS_IO_I2C1_ER               32 /* I2C 1 error */
  #define OS_IO_I2C2_EV               33 /* I2C 2 event */
  #define OS_IO_I2C2_ER               34 /* I2C 2 error */
  #define OS_IO_SPI1                  35 /* SPI 1 (global) */
  #define OS_IO_SPI2                  36 /* SPI 2 (global) */
  #define OS_IO_USART1                37 /* USART 1 (global) */
  #define OS_IO_USART2                38 /* USART 2 (global) */
  #define OS_IO_USART3                39 /* USART 3 (global) */
  #define OS_IO_EXTI15_10             40 /* External lines 10 to 15 */
  #define OS_IO_RTC_Alarm             41 /* RTC alarm through EXTI line */
  #define OS_IO_USB_FS_WKUP           42 /* USB full-speed wake-up from suspend through
                                            EXTI line */
  #define OS_IO_TIM6                  43 /* Timer 6 (global) */
  #define OS_IO_TIM7                  44 /* Timer 7 (global) */
  #if defined(STM32L151XD) || defined(STM32L152XD) || defined(STM32L162XD)
     #define OS_IO_SDIO               45 /* Secure digital input/output interface (global) */
  #endif
  #if defined(STM32L151XC) || defined(STM32L152XC) || defined(STM32L151XD) || \
      defined(STM32L152XD) || defined(STM32L162XD)
     #define OS_IO_TIM5               46 /* Timer 5 (global) */
     #define OS_IO_SPI3               47 /* SPI 3 (global) */
  #endif
  #if defined(STM32L151XD) || defined(STM32L152XD) || defined(STM32L162XD)
     #define OS_IO_UART4              48 /* UART 4 (global) */
     #define OS_IO_UART5              49 /* UART 5 (global) */
  #endif
  #if defined(STM32L151XC) || defined(STM32L152XC) || defined(STM32L151XD) || \
      defined(STM32L152XD) || defined(STM32L162XD)
     #define OS_IO_DMA2_Channel1      50 /* DMA 2 channel 1 (global) */
     #define OS_IO_DMA2_Channel2      51 /* DMA 2 channel 2 (global) */
     #define OS_IO_DMA2_Channel3      52 /* DMA 2 channel 3 (global) */
     #define OS_IO_DMA2_Channel4      53 /* DMA 2 channel 4 (global) */
     #define OS_IO_DMA2_Channel5      54 /* DMA 2 channel 5 (global) */
  #endif
  #if defined(STM32L162XD)
     #define OS_IO_AES                55 /* Advanced encryption standard (AES) hardware
                                            accelerator (global) */
  #endif
  #if defined(STM32L151XC) || defined(STM32L152XC) || defined(STM32L151XD) || \
      defined(STM32L152XD) || defined(STM32L162XD)
     #define OS_IO_COMP_ACQ           56 /* Comparator channel acquisition (global) */
  #endif
/* ------------------------------ STM32F1 ------------------------------------------- */
#elif defined(STM32F1XXXX)
  #if defined(STM32F100X4_X6) || defined(STM32F100X8_XB) || \
      defined(STM32F100RC_RD_RE) || defined(STM32F100VC_VD_VE_ZC_ZD_ZE) || \
      defined(STM32F101X4_X6) || defined(STM32F101T8_TB) || \
      defined(STM32F101C8_CB_R8_RB_V8_VB) || defined(STM32F101RC_RD_RE) || \
      defined(STM32F101VC_VD_VE_ZC_ZD_ZE) || defined(STM32F102X4_X6) || \
      defined(STM32F102X8_XB)
     #define OS_IO_ADC1               18 /* ADC 1 (global) */
  #elif defined(STM32F101RF_RG) || defined(STM32F101VF_VG_ZF_ZG) || \
        defined(STM32F103X4_X6) || defined(STM32F103T8_TB) || \
        defined(STM32F103C8_CB_R8_RB_V8_VB) || defined(STM32F105XX) || \
        defined(STM32F107XX)
     #define OS_IO_ADC1_2             18 /* ADC 1 and 2 (global) */
  #endif
  #if defined(STM32F103X4_X6) || defined(STM32F103T8_TB) || \
      defined(STM32F103C8_CB_R8_RB_V8_VB) || defined(STM32F103RC_RD_RE) || \
      defined(STM32F103VC_VD_VE_ZC_ZD_ZE) || defined(STM32F103RF_RG) || \
      defined(STM32F103VF_VG_ZF_ZG)
     #define OS_IO_USB_HP_CAN1_TX     19 /* USB device high priority or CAN 1 TX */
  #elif defined(STM32F102X4_X6) || defined(STM32F102X8_XB)
     #define OS_IO_USB_HP             19 /* USB device high priority */
  #elif defined(STM32F105XX) || defined(STM32F107XX)
     #define OS_IO_CAN1_TX            19 /* CAN 1 TX */
  #endif
  #if defined(STM32F103X4_X6) || defined(STM32F103T8_TB) || \
      defined(STM32F103C8_CB_R8_RB_V8_VB) || defined(STM32F103RC_RD_RE) || \
      defined(STM32F103VC_VD_VE_ZC_ZD_ZE) || defined(STM32F103RF_RG) || \
      defined(STM32F103VF_VG_ZF_ZG)
     #define OS_IO_USB_LP_CAN1_RX0    20 /* USB device low priority or CAN 1 RX0 */
  #elif defined(STM32F102X4_X6) || defined(STM32F102X8_XB)
     #define OS_IO_USB_LP             20 /* USB device low priority */
  #elif defined(STM32F105XX) || defined(STM32F107XX)
     #define OS_IO_CAN1_RX0           20 /* CAN 1 RX0 */
  #endif
  #if defined(STM32F103X4_X6) || defined(STM32F103T8_TB) || \
      defined(STM32F103C8_CB_R8_RB_V8_VB) || defined(STM32F103RC_RD_RE) || \
      defined(STM32F103VC_VD_VE_ZC_ZD_ZE) || defined(STM32F103RF_RG) || \
      defined(STM32F103VF_VG_ZF_ZG) || defined(STM32F105XX) || defined(STM32F107XX)
     #define OS_IO_CAN1_RX1           21 /* CAN 1 RX1 */
  #endif
  #if defined(STM32F103X4_X6) || defined(STM32F103T8_TB) || \
      defined(STM32F103C8_CB_R8_RB_V8_VB) || defined(STM32F103RC_RD_RE) || \
      defined(STM32F103VC_VD_VE_ZC_ZD_ZE) || defined(STM32F103RF_RG) || \
      defined(STM32F103VF_VG_ZF_ZG) || defined(STM32F105XX) || defined(STM32F107XX)
     #define OS_IO_CAN1_SCE           22 /* CAN 1 SCE */
  #endif
  #define OS_IO_EXTI9_5               23 /* External lines 5 to 9 */
  #if defined(STM32F103X4_X6) || defined(STM32F103T8_TB) || \
      defined(STM32F103C8_CB_R8_RB_V8_VB) || defined(STM32F103RC_RD_RE) || \
      defined(STM32F103VC_VD_VE_ZC_ZD_ZE) || defined(STM32F105XX) || defined(STM32F107XX)
     #define OS_IO_TIM1_BRK           24 /* Timer 1 break */
     #define OS_IO_TIM1_UP            25 /* Timer 1 update */
     #define OS_IO_TIM1_TRG_COM       26 /* Timer 1 trigger and commutation */
     #define OS_IO_TIM1_CC            27 /* Timer 1 capture/compare */
     #define OS_IO_TIM1               OS_IO_TIM1_CC
  #elif defined(STM32F100X4_X6) || defined(STM32F100X8_XB) || \
        defined(STM32F100RC_RD_RE) || defined(STM32F100VC_VD_VE_ZC_ZD_ZE)
     #define OS_IO_TIM1_BRK_TIM15     24 /* Timer 1 break and timer 15 */
     #define OS_IO_TIM1_BRK           (OS_IO_TIM1_BRK_TIM15 | 0x100)
     #define OS_IO_TIM15              (OS_IO_TIM1_BRK_TIM15 | 0x200)
     #define OS_IO_TIM1_UP_TIM16      25 /* Timer 1 update and timer 16 */
     #define OS_IO_TIM1_UP            (OS_IO_TIM1_UP_TIM16 | 0x100)
     #define OS_IO_TIM16              (OS_IO_TIM1_UP_TIM16 | 0x200)
     #define OS_IO_TIM1_TRG_COM_TIM17 26 /* Timer 1 trigger and commutation, and timer 17 */
     #define OS_IO_TIM1_TRG_COM       (OS_IO_TIM1_TRG_COM_TIM17 | 0x100)
     #define OS_IO_TIM17              (OS_IO_TIM1_TRG_COM_TIM17 | 0x200)
     #define OS_IO_TIM1_CC            27 /* Timer 1 capture/compare */
     #define OS_IO_TIM1               OS_IO_TIM1_CC
  #elif defined (STM32F101RF_RG)|| defined(STM32F101VF_VG_ZF_ZG)
     #define OS_IO_TIM9               24 /* Timer 9 (global) */
     #define OS_IO_TIM10              25 /* Timer 10 (global) */
     #define OS_IO_TIM11              26 /* Timer 11 (global) */
  #elif defined(STM32F103RF_RG) || defined(STM32F103VF_VG_ZF_ZG)
     #define OS_IO_TIM1_BRK_TIM9      24 /* Timer 1 break and timer 9 (global) */
     #define OS_IO_TIM1_BRK           (OS_IO_TIM1_BRK_TIM9 | 0x100)
     #define OS_IO_TIM9               (OS_IO_TIM1_BRK_TIM9 | 0x200)
     #define OS_IO_TIM1_UP_TIM10      25 /* Timer 1 update and timer 10 (global) */
     #define OS_IO_TIM1_UP            (OS_IO_TIM1_UP_TIM10 | 0x100)
     #define OS_IO_TIM10              (OS_IO_TIM1_UP_TIM10 | 0x200)
     #define OS_IO_TIM1_TRG_COM_TIM11 26 /* Timer 1 trigger and commutation, and
                                            timer 11 (global) */
     #define OS_IO_TIM1_TRG_COM       (OS_IO_TIM1_TRG_COM_TIM11 | 0x100)
     #define OS_IO_TIM11              (OS_IO_TIM1_TRG_COM_TIM11 | 0x200)
     #define OS_IO_TIM1_CC            27 /* Timer 1 capture/compare */
     #define OS_IO_TIM1               OS_IO_TIM1_CC
  #endif
  #define OS_IO_TIM2                  28 /* Timer 2 (global) */
  #define OS_IO_TIM3                  29 /* Timer 3 (global) */
  #if defined (STM32F100X8_XB) || defined (STM32F100RC_RD_RE) || \
      defined (STM32F100VC_VD_VE_ZC_ZD_ZE) || defined(STM32F101T8_TB) || \
      defined(STM32F101C8_CB_R8_RB_V8_VB) || defined(STM32F101RC_RD_RE) || \
      defined(STM32F101VC_VD_VE_ZC_ZD_ZE) || defined(STM32F102X8_XB) || \
      defined(STM32F103T8_TB) || defined(STM32F103C8_CB_R8_RB_V8_VB) || \
      defined(STM32F103RC_RD_RE) || defined(STM32F103VC_VD_VE_ZC_ZD_ZE) || \
      defined (STM32F101RF_RG)|| defined(STM32F101VF_VG_ZF_ZG) || \
      defined(STM32F103RF_RG) || defined(STM32F103VF_VG_ZF_ZG) || \
      defined(STM32F105XX) || defined(STM32F107XX)
     #define OS_IO_TIM4               30 /* Timer 4 (global) */
  #endif
  #define OS_IO_I2C1_EV               31 /* I2C 1 event */
  #define OS_IO_I2C1_ER               32 /* I2C 1 error */
  #if defined(STM32F100X8_XB) || defined(STM32F100RC_RD_RE) || \
      defined(STM32F100VC_VD_VE_ZC_ZD_ZE) || defined(STM32F101C8_CB_R8_RB_V8_VB) || \
      defined(STM32F101RC_RD_RE) || defined(STM32F101VC_VD_VE_ZC_ZD_ZE) || \
      defined(STM32F101RF_RG) || defined(STM32F101VF_VG_ZF_ZG) || \
      defined(STM32F102X8_XB) || defined(STM32F103C8_CB_R8_RB_V8_VB) || \
      defined(STM32F103RC_RD_RE) || defined(STM32F103VC_VD_VE_ZC_ZD_ZE) || \
      defined(STM32F103RF_RG) || defined(STM32F103VF_VG_ZF_ZG) || \
      defined(STM32F105XX) || defined(STM32F107XX)
     #define OS_IO_I2C2_EV            33 /* I2C 2 event */
     #define OS_IO_I2C2_ER            34 /* I2C 2 error */
  #endif
  #define OS_IO_SPI1                  35 /* SPI 1 (global) */
  #if defined(STM32F100X8_XB) || defined(STM32F100RC_RD_RE) || \
      defined(STM32F100VC_VD_VE_ZC_ZD_ZE) || defined(STM32F101C8_CB_R8_RB_V8_VB) || \
      defined(STM32F101RC_RD_RE) || defined(STM32F101VC_VD_VE_ZC_ZD_ZE) || \
      defined(STM32F101RF_RG) || defined(STM32F101VF_VG_ZF_ZG) || \
      defined(STM32F102X8_XB) || defined(STM32F103C8_CB_R8_RB_V8_VB) || \
      defined(STM32F103RC_RD_RE) || defined(STM32F103VC_VD_VE_ZC_ZD_ZE) || \
      defined(STM32F103RF_RG) || defined(STM32F103VF_VG_ZF_ZG) || \
      defined(STM32F105XX) || defined(STM32F107XX)
     #define OS_IO_SPI2               36 /* SPI 2 (global) */
  #endif
  #define OS_IO_USART1                37 /* USART 1 (global) */
  #define OS_IO_USART2                38 /* USART 2 (global) */
  #if defined(STM32F100X8_XB) || defined(STM32F100RC_RD_RE) || \
      defined(STM32F100VC_VD_VE_ZC_ZD_ZE) || defined(STM32F101C8_CB_R8_RB_V8_VB) || \
      defined(STM32F101RC_RD_RE) || defined(STM32F101VC_VD_VE_ZC_ZD_ZE) || \
      defined(STM32F101RF_RG) || defined(STM32F101VF_VG_ZF_ZG) || \
      defined(STM32F102X8_XB) || defined(STM32F103C8_CB_R8_RB_V8_VB) || \
      defined(STM32F103RC_RD_RE) || defined(STM32F103VC_VD_VE_ZC_ZD_ZE) || \
      defined(STM32F103RF_RG) || defined(STM32F103VF_VG_ZF_ZG) || \
      defined(STM32F105XX) || defined(STM32F107XX)
     #define OS_IO_USART3             39 /* USART 3 (global) */
  #endif
  #define OS_IO_EXTI15_10             40 /* External lines 10 to 15 */
  #define OS_IO_RTCAlarm              41 /* RTC alarm through EXTI line */
  #if defined(STM32F103X4_X6) || defined(STM32F103T8_TB) || \
      defined(STM32F103C8_CB_R8_RB_V8_VB) || defined(STM32F103RC_RD_RE) || \
      defined(STM32F103VC_VD_VE_ZC_ZD_ZE) || defined(STM32F103RF_RG) || \
      defined(STM32F103VF_VG_ZF_ZG) || defined(STM32F102X4_X6) || defined(STM32F102X8_XB)
     #define OS_IO_USBWakeUp          42 /* USB device wake-up from suspend through EXTI
                                            line */
  #elif defined(STM32F100X4_X6) || defined(STM32F100X8_XB) || \
        defined(STM32F100RC_RD_RE) || defined(STM32F100VC_VD_VE_ZC_ZD_ZE)
     #define OS_IO_CEC                42 /* High-definition multimedia interface (HDMI)/
                                            consumer electronics control (CEC) */
  #elif defined(STM32F105XX) || defined(STM32F107XX)
     #define OS_IO_OTG_FS_WKUP        42 /* USB on-the-go full-speed wake-up from suspend
                                            through EXTI line */
  #endif
  #if defined(STM32F103RC_RD_RE) || defined(STM32F103VC_VD_VE_ZC_ZD_ZE)
     #define OS_IO_TIM8_BRK           43 /* Timer 8 break */
     #define OS_IO_TIM8_UP            44 /* Timer 8 update */
     #define OS_IO_TIM8_TRG_COM       45 /* Timer 8 trigger and commutation */
     #define OS_IO_TIM8_CC            46 /* Timer 8 capture/compare */
     #define OS_IO_TIM8               OS_IO_TIM8_CC
  #elif defined(STM32F101RF_RG) || defined(STM32F101VF_VG_ZF_ZG) || \
        defined(STM32F100RC_RD_RE) || defined(STM32F100VC_VD_VE_ZC_ZD_ZE )
     #define OS_IO_TIM12              43 /* Timer 12 (global) */
     #define OS_IO_TIM13              44 /* Timer 13 (global) */
     #define OS_IO_TIM14              45 /* Timer 14 (global) */
  #elif defined(STM32F103RF_RG) || defined(STM32F103VF_VG_ZF_ZG)
     #define OS_IO_TIM8_BRK_TIM12     43 /* Timer 8 break and timer 12 (global) */
     #define OS_IO_TIM8_BRK           (OS_IO_TIM8_BRK_TIM12 | 0x100)
     #define OS_IO_TIM12              (OS_IO_TIM8_BRK_TIM12 | 0x200)
     #define OS_IO_TIM8_UP_TIM13      44 /* Timer 8 update and timer 13 (global) */
     #define OS_IO_TIM8_UP            (OS_IO_TIM8_UP_TIM13 | 0x100)
     #define OS_IO_TIM13              (OS_IO_TIM8_UP_TIM13 | 0x200)
     #define OS_IO_TIM8_TRG_COM_TIM14 45 /* Timer 8 trigger and commutation, and
                                            timer 14 (global) */
     #define OS_IO_TIM8_TRG_COM       (OS_IO_TIM8_TRG_COM_TIM14 | 0x100)
     #define OS_IO_TIM14              (OS_IO_TIM8_TRG_COM_TIM14 | 0x200)
     #define OS_IO_TIM8_CC            46 /* Timer 8 capture/compare */
     #define OS_IO_TIM8               OS_IO_TIM8_CC
  #endif
  #if defined(STM32F103RC_RD_RE) || defined(STM32F103VC_VD_VE_ZC_ZD_ZE) || \
      defined(STM32F103RF_RG) || defined(STM32F103VF_VG_ZF_ZG)
     #define OS_IO_ADC3               47 /* ADC 3 (global) */
  #endif
  #if defined(STM32F100VC_VD_VE_ZC_ZD_ZE) || defined(STM32F101VC_VD_VE_ZC_ZD_ZE) || \
      defined(STM32F101VF_VG_ZF_ZG) || defined(STM32F103VC_VD_VE_ZC_ZD_ZE) || \
      defined(STM32F103VF_VG_ZF_ZG)
     #define OS_IO_FSMC               48 /* Flexible static memory controller (global) */
  #endif
  #if defined(STM32F103RC_RD_RE) || defined(STM32F103VC_VD_VE_ZC_ZD_ZE) || \
      defined(STM32F103RF_RG) || defined(STM32F103VF_VG_ZF_ZG)
     #define OS_IO_SDIO               49 /* Secure digital input/output interface (global) */
  #endif
  #if defined(STM32F100RC_RD_RE) || defined(STM32F100VC_VD_VE_ZC_ZD_ZE) || \
      defined(STM32F101RC_RD_RE) || defined(STM32F101VC_VD_VE_ZC_ZD_ZE) || \
      defined(STM32F101RF_RG) || defined(STM32F101VF_VG_ZF_ZG) || \
      defined(STM32F103RC_RD_RE) || defined(STM32F103VC_VD_VE_ZC_ZD_ZE) || \
      defined(STM32F103RF_RG) || defined(STM32F103VF_VG_ZF_ZG) || \
      defined(STM32F105XX) || defined(STM32F107XX)
     #define OS_IO_TIM5               50 /* Timer 5 (global) */
     #define OS_IO_SPI3               51 /* SPI 3 (global) */
  #endif
  #if defined(STM32F100RC_RD_RE) || defined(STM32F100VC_VD_VE_ZC_ZD_ZE) || \
      defined(STM32F101RC_RD_RE) || defined(STM32F101VC_VD_VE_ZC_ZD_ZE) || \
      defined(STM32F101RF_RG) || defined(STM32F101VF_VG_ZF_ZG) || \
      defined(STM32F103RC_RD_RE) || defined(STM32F103VC_VD_VE_ZC_ZD_ZE) || \
      defined(STM32F103RF_RG) || defined(STM32F103VF_VG_ZF_ZG) || \
      defined(STM32F105XX) || defined(STM32F107XX)
     #define OS_IO_UART4              52 /* UART 4 (global) */
     #define OS_IO_UART5              53 /* UART 5 (global) */
  #endif
  #if defined(STM32F100X4_X6) || defined(STM32F100X8_XB) || \
      defined(STM32F100RC_RD_RE) || defined(STM32F100VC_VD_VE_ZC_ZD_ZE)
     #define OS_IO_TIM6_DAC           54 /* Timer 6 and DAC underrun */
  #elif defined(STM32F101RC_RD_RE) || defined(STM32F101VC_VD_VE_ZC_ZD_ZE) || \
        defined(STM32F101RF_RG) || defined(STM32F101VF_VG_ZF_ZG) || \
        defined(STM32F103RC_RD_RE) || defined(STM32F103VC_VD_VE_ZC_ZD_ZE) || \
        defined(STM32F103RF_RG) || defined(STM32F103VF_VG_ZF_ZG) || \
        defined(STM32F105XX) || defined(STM32F107XX)
     #define OS_IO_TIM6               54 /* Timer 6 (global) */
  #endif
  #if defined(STM32F100X4_X6) || defined(STM32F100X8_XB) || \
      defined(STM32F100RC_RD_RE) || defined(STM32F100VC_VD_VE_ZC_ZD_ZE) || \
      defined(STM32F101RC_RD_RE) || defined(STM32F101VC_VD_VE_ZC_ZD_ZE) || \
      defined(STM32F101RF_RG) || defined(STM32F101VF_VG_ZF_ZG) || \
      defined(STM32F103RC_RD_RE) || defined(STM32F103VC_VD_VE_ZC_ZD_ZE) || \
      defined(STM32F103RF_RG) || defined(STM32F103VF_VG_ZF_ZG) || \
      defined(STM32F105XX) || defined(STM32F107XX)
     #define OS_IO_TIM7               55 /* Timer 7 */
  #endif
  #if defined(STM32F100RC_RD_RE) || defined(STM32F100VC_VD_VE_ZC_ZD_ZE) || \
      defined(STM32F101RC_RD_RE) || defined(STM32F101VC_VD_VE_ZC_ZD_ZE) || \
      defined(STM32F101RF_RG) || defined(STM32F101VF_VG_ZF_ZG) || \
      defined(STM32F103RC_RD_RE) || defined(STM32F103VC_VD_VE_ZC_ZD_ZE) || \
      defined(STM32F103RF_RG) || defined(STM32F103VF_VG_ZF_ZG) || \
      defined(STM32F105XX) || defined(STM32F107XX)
     #define OS_IO_DMA2_Channel1      56 /* DMA 2 channel 1 (global) */
     #define OS_IO_DMA2_Channel2      57 /* DMA 2 channel 2 (global) */
     #define OS_IO_DMA2_Channel3      58 /* DMA 2 channel 3 (global) */
  #endif
  #if defined(STM32F100RC_RD_RE) || defined(STM32F100VC_VD_VE_ZC_ZD_ZE) || \
      defined(STM32F101RC_RD_RE) || defined(STM32F101VC_VD_VE_ZC_ZD_ZE) || \
      defined(STM32F101RF_RG) || defined(STM32F101VF_VG_ZF_ZG) || \
      defined(STM32F103RC_RD_RE) || defined(STM32F103VC_VD_VE_ZC_ZD_ZE) || \
      defined(STM32F103RF_RG) || defined(STM32F103VF_VG_ZF_ZG)
     #define OS_IO_DMA2_Channel4_5    59 /* DMA 2 channels 4 and 5 (global) */
  #elif defined(STM32F105XX) || defined(STM32F107XX)
     #define OS_IO_DMA2_Channel4      59 /* DMA 2 channel 4 (global) */
  #endif
  #if defined(STM32F100RC_RD_RE) || defined(STM32F100VC_VD_VE_ZC_ZD_ZE)
     #define OS_IO_DMA2_Channel5      60 /* DMA 2 channel 5 (global) (Channel 5 is mapped
                                            at position 60 only if the MISC_REMAP bit in
                                            the AFIO_MAPR2 register is set) */
  #elif defined(STM32F105XX) || defined(STM32F107XX)
     #define OS_IO_DMA2_Channel5      60 /* DMA 2 channel 5 (global) */
  #endif
  #if defined(STM32F107XX)
     #define OS_IO_ETH                61 /* Ethernet (global) */
     #define OS_IO_ETH_WKUP           62 /* Ethernet wake-up through EXTI line */
  #endif
  #if defined(STM32F105XX) || defined(STM32F107XX)
     #define OS_IO_CAN2_TX            63 /* CAN 2 TX */
     #define OS_IO_CAN2_RX0           64 /* CAN 2 RX0 */
     #define OS_IO_CAN2_RX1           65 /* CAN 2 RX1 */
     #define OS_IO_CAN2_SCE           66 /* CAN 2 SCE */
     #define OS_IO_OTG_FS             67 /* USB on-the-go full-speed (global) */
  #endif
/* ------------------------------ STM32F2 and STM32F4 ------------------------------- */
#elif defined(STM32F2XXXX) || defined(STM32F4XXXX)
  #define OS_IO_ADC                   18 /* ADC 1, 2 and 3 (global) */
  #define OS_IO_CAN1_TX               19 /* CAN 1 TX */
  #define OS_IO_CAN1_RX0              20 /* CAN 1 RX0 */
  #define OS_IO_CAN1_RX1              21 /* CAN 1 RX1 */
  #define OS_IO_CAN1_SCE              22 /* CAN 1 SCE */
  #define OS_IO_EXTI9_5               23 /* External lines 5 to 9 */
  #define OS_IO_TIM1_BRK_TIM9         24 /* Timer 1 break and timer 9 (global) */
  #define OS_IO_TIM1_BRK              (OS_IO_TIM1_BRK_TIM9 | 0x100)
  #define OS_IO_TIM9                  (OS_IO_TIM1_BRK_TIM9 | 0x200)
  #define OS_IO_TIM1_UP_TIM10         25 /* Timer 1 update and timer 10 (global) */
  #define OS_IO_TIM1_UP               (OS_IO_TIM1_UP_TIM10 | 0x100) 
  #define OS_IO_TIM10                 (OS_IO_TIM1_UP_TIM10 | 0x200)
  #define OS_IO_TIM1_TRG_COM_TIM11    26 /* Timer 1 trigger and commutation, and
                                            timer 11 (global) */
  #define OS_IO_TIM1_TRG_COM          (OS_IO_TIM1_TRG_COM_TIM11 | 0x100)
  #define OS_IO_TIM11                 (OS_IO_TIM1_TRG_COM_TIM11 | 0x100)
  #define OS_IO_TIM1_CC               27 /* Timer 1 capture/compare */
  #define OS_IO_TIM1                  OS_IO_TIM1_CC
  #define OS_IO_TIM2                  28 /* Timer 2 (global) */
  #define OS_IO_TIM3                  29 /* Timer 3 (global) */
  #define OS_IO_TIM4                  30 /* Timer 4 (global) */
  #define OS_IO_I2C1_EV               31 /* I2C 1 event */
  #define OS_IO_I2C1_ER               32 /* I2C 1 error */
  #define OS_IO_I2C2_EV               33 /* I2C 2 event */
  #define OS_IO_I2C2_ER               34 /* I2C 2 error */
  #define OS_IO_SPI1                  35 /* SPI 1 (global) */
  #define OS_IO_SPI2                  36 /* SPI 2 (global) */
  #define OS_IO_USART1                37 /* USART 1 (global) */
  #define OS_IO_USART2                38 /* USART 2 (global) */
  #define OS_IO_USART3                39 /* USART 3 (global) */
  #define OS_IO_EXTI15_10             40 /* External lines 10 to 15 */
  #define OS_IO_RTC_Alarm             41 /* RTC alarm (A and B) through EXTI line */
  #define OS_IO_OTG_FS_WKUP           42 /* USB on-the-go full-speed wake-up from suspend
                                            through EXTI line */
  #define OS_IO_TIM8_BRK_TIM12        43 /* Timer 8 break and timer 12 (global) */
  #define OS_IO_TIM8_BRK              (OS_IO_TIM8_BRK_TIM12 | 0x100)
  #define OS_IO_TIM12                 (OS_IO_TIM8_BRK_TIM12 | 0x200)
  #define OS_IO_TIM8_UP_TIM13         44 /* Timer 8 update and timer 13 (global) */
  #define OS_IO_TIM8_UP               (OS_IO_TIM8_UP_TIM13 | 0x100)
  #define OS_IO_TIM13                 (OS_IO_TIM8_UP_TIM13 | 0x200)
  #define OS_IO_TIM8_TRG_COM_TIM14    45 /* Timer 8 trigger and commutation, and
                                            timer 14 (global) */
  #define OS_IO_TIM8_TRG_COM          (OS_IO_TIM8_TRG_COM_TIM14 | 0x100)
  #define OS_IO_TIM14                 (OS_IO_TIM8_TRG_COM_TIM14 | 0x200)
  #define OS_IO_TIM8_CC               46 /* Timer 8 capture/compare */
  #define OS_IO_TIM8                  OS_IO_TIM8_CC
  #define OS_IO_DMA1_Stream7          47 /* DMA 1 stream 7 */
  #if defined(STM32F205VX_ZX) || defined(STM32F215VX_ZX) || defined(STM32F207XX) || \
      defined(STM32F217XX) || defined(STM32F405VX_ZX) || defined(STM32F415VX_ZX) || \
      defined(STM32F407XX) || defined(STM32F417XX)
     #define OS_IO_FSMC               48  /* Flexible static memory controller (global) */
  #endif
  #define OS_IO_SDIO                  49 /* Secure digital input/output interface (global) */
  #define OS_IO_TIM5                  50 /* Timer 5 (global) */
  #define OS_IO_SPI3                  51 /* SPI 3 (global) */
  #define OS_IO_UART4                 52 /* UART 4 (global) */
  #define OS_IO_UART5                 53 /* UART 5 (global) */
  #define OS_IO_TIM6_DAC              54 /* Timer 6 (global) and DAC 1 & 2 underrun error */
  #define OS_IO_TIM7                  55 /* Timer 7 (global) */
  #define OS_IO_DMA2_Stream0          56 /* DMA 2 stream 0 (global) */
  #define OS_IO_DMA2_Stream1          57 /* DMA 2 stream 1 (global) */
  #define OS_IO_DMA2_Stream2          58 /* DMA 2 stream 2 (global) */
  #define OS_IO_DMA2_Stream3          59 /* DMA 2 stream 3 (global) */
  #define OS_IO_DMA2_Stream4          60 /* DMA 2 stream 4 (global) */
  #if defined(STM32F207XX) || defined(STM32F217XX) || defined(STM32F407XX) || \
      defined(STM32F417XX)
     #define OS_IO_ETH                61 /* Ethernet (global) */
     #define OS_IO_ETH_WKUP           62 /* Ethernet wake-up through EXTI line */
  #endif
  #define OS_IO_CAN2_TX               63 /* CAN 2 TX */
  #define OS_IO_CAN2_RX0              64 /* CAN 2 RX0 */
  #define OS_IO_CAN2_RX1              65 /* CAN 2 RX1 */
  #define OS_IO_CAN2_SCE              66 /* CAN 2 SCE */
  #if defined(STM32F207XX) || defined(STM32F217XX) || defined(STM32F407XX) || \
      defined(STM32F417XX)
     #define OS_IO_OTG_FS             67 /* USB on-the-go full-speed (global) */
  #endif
  #define OS_IO_DMA2_Stream5          68 /* DMA 2 stream 5 (global) */
  #define OS_IO_DMA2_Stream6          69 /* DMA 2 stream 6 (global) */
  #define OS_IO_DMA2_Stream7          70 /* DMA 2 stream 7 (global) */
  #define OS_IO_USART6                71 /* USART 6 (global) */
  #define OS_IO_I2C3_EV               72 /* I2C 3 event */
  #define OS_IO_I2C3_ER               73 /* I2C 3 error */
  #define OS_IO_OTG_HS_EP1_OUT        74 /* USB on-the-go HS end point 1 out (global) */
  #define OS_IO_OTG_HS_EP1_IN         75 /* USB on-the-go HS end point 1 in (global) */
  #define OS_IO_OTG_HS_WKUP           76 /* USB on-the-go HS wake-up through EXTI */
  #define OS_IO_OTG_HS                77 /* USB on-the-go high-speed (global) */
  #if defined(STM32F207XX) || defined(STM32F217XX) || defined(STM32F407XX) || \
      defined(STM32F417XX)
     #define OS_IO_DCMI               78 /* Digital camera interface (global) */
  #endif
  #if defined(STM32F215RX) || defined(STM32F215VX_ZX) || defined(STM32F217XX) || \
      defined(STM32F415RG) || defined(STM32F415VG_ZG) || defined(STM32F417XX)
     #define OS_IO_CRYP               79 /* Cryptographic processor (global) */
  #endif
  #define OS_IO_HASH_RNG              80 /* Hash processor and random number generator
                                            (global) */
  #if defined(STM32F4XXXX)
     #define OS_IO_FPU                81 /* Floating-point unit (global) */
  #endif
#else
  #error STM32 version undefined
#endif

#endif /* _ZOTTAOS_INTERRUPTS_ */

