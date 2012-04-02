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
/* File ZottaOS_Interrupts.h:
** Platform version: All STM32 microcontrollers.
** Version date: March 2012
** Authors: MIS-TIC
*/

#ifndef _ZOTTAOS_INTERRUPTS_
#define _ZOTTAOS_INTERRUPTS_

#include "ZottaOS_Config.h"


/* ------------------------------ GLOBAL ---------------------------------------------- */
  #define OS_IO_WWDG                   0  /* Window WatchDog */
  #define OS_IO_PVD                    1  PVD through EXTI Line detection */
#if defined(STM32L1XXXX)
  #define OS_IO_TAMPER_STAMP           2  /* Tamper and TimeStamp through the EXTI line*/
#elif defined(STM32F2XXXX) || defined(STM32F4XXXX)
  #define OS_IO_TAMP_STAMP             2  /* Tamper and TimeStamp through the EXTI line*/
#elif defined(STM32F1XXXX)
  #define OS_IO_TAMPER                 2  /* Tamper */
#else
  #error STM32 version undefined
#endif
#if defined(STM32L1XXXX) || defined(STM32F2XXXX) || defined(STM32F4XXXX)
  #define OS_IO_RTC_WKUP               3  /* RTC Wakeup  through the EXTI line */
#elif defined(STM32F1XXXX)
  #define OS_IO_RTC                    3  /* RTC global */
#else
  #error STM32 version undefined
#endif
  #define OS_IO_FLASH                  4  /* FLASH global */
  #define OS_IO_RCC                    5  /* RCC global */
  #define OS_IO_EXTI0                  6  /* EXTI Line0 */
  #define OS_IO_EXTI1                  7  /* EXTI Line1 */
  #define OS_IO_EXTI2                  8  /* EXTI Line2 */
  #define OS_IO_EXTI3                  9  /* EXTI Line3 */
  #define OS_IO_EXTI4                 10  /* EXTI Line4 */
#if defined(STM32L1XXXX) || defined(STM32F1XXXX)
  #define OS_IO_DMA1_Channel1         11  /* DMA1 Channel 1 global */
  #define OS_IO_DMA1_Channel2         12  /* DMA1 Channel 2 global */
  #define OS_IO_DMA1_Channel3         13  /* DMA1 Channel 3 global */
  #define OS_IO_DMA1_Channel4         14  /* DMA1 Channel 4 global */
  #define OS_IO_DMA1_Channel5         15  /* DMA1 Channel 5 global */
  #define OS_IO_DMA1_Channel6         16  /* DMA1 Channel 6 global */
  #define OS_IO_DMA1_Channel7         17  /* DMA1 Channel 7 global */
#elif defined(STM32F2XXXX) || defined(STM32F4XXXX)
  #define OS_IO_DMA1_Stream0          11  /* DMA1 Stream 0 global */
  #define OS_IO_DMA1_Stream1          12  /* DMA1 Stream 1 global */
  #define OS_IO_DMA1_Stream2          13  /* DMA1 Stream 2 global */
  #define OS_IO_DMA1_Stream3          14  /* DMA1 Stream 3 global */
  #define OS_IO_DMA1_Stream4          15  /* DMA1 Stream 4 global */
  #define OS_IO_DMA1_Stream5          16  /* DMA1 Stream 5 global */
  #define OS_IO_DMA1_Stream6          17  /* DMA1 Stream 6 global */
#else
  #error STM32 version undefined
#endif
/* ------------------------------ STM32L1 --------------------------------------------- */
#if defined(STM32L1XXXX)
  #define OS_IO_ADC1                  18  /* ADC1 global */
  #define OS_IO_USB_HP                19  /* USB High Priority */
  #define OS_IO_USB_LP                20  /* USB Low Priority */
  #define OS_IO_DAC                   21  /* DAC */
  #if defined(STM32L151XC) || defined(STM32L152XC) || defined(STM32L151XD) || \
      defined(STM32L152XD) || defined(STM32L162XD)
     #define OS_IO_COMP_CA            22  /* Comparator and Channel acquisition
                                             through EXTI Line */
  #else
     #define OS_IO_COMP               22  /* Comparator through EXTI Line  */
  #endif
  #define OS_IO_EXTI9_5               23  /* External Line[9:5] */
  #if defined(STM32L152X6_X8_XB) || defined(STM32L152XC) || defined(STM32L152XD) || \
      defined(STM32L162XD)
     #define OS_IO_LCD                24  /* LCD */
  #endif
  #define OS_IO_TIM9                  25  /* TIM9 global */
  #define OS_IO_TIM10                 26  /* TIM10 global */
  #define OS_IO_TIM11                 27  /* TIM11 global */
  #define OS_IO_TIM2                  28  /* TIM2 global */
  #define OS_IO_TIM3                  29  /* TIM3 global */
  #define OS_IO_TIM4                  30  /* TIM4 global */
  #define OS_IO_I2C1_EV               31  /* I2C1 Event */
  #define OS_IO_I2C1_ER               32  /* I2C1 Error */
  #define OS_IO_I2C2_EV               33  /* I2C2 Event */
  #define OS_IO_I2C2_ER               34  /* I2C2 Error */
  #define OS_IO_SPI1                  35  /* SPI1 global */
  #define OS_IO_SPI2                  36  /* SPI2 global */
  #define OS_IO_USART1                37  /* USART1 global */
  #define OS_IO_USART2                38  /* USART2 global */
  #define OS_IO_USART3                39  /* USART3 global */
  #define OS_IO_EXTI15_10             40  /* External Line[15:10]  */
  #define OS_IO_RTC_Alarm             41  /* RTC Alarm through EXTI Line */
  #define OS_IO_USB_FS_WKUP           42  /* USB FS WakeUp from suspend through EXTI
                                             Line */
  #define OS_IO_TIM6                  43  /* TIM6 global */
  #define OS_IO_TIM7                  44  /* TIM7 global */
  #if defined(STM32L151XD) || defined(STM32L152XD) || defined(STM32L162XD)
     #define OS_IO_SDIO               45  /* SDIO global */
  #endif
  #if defined(STM32L151XC) || defined(STM32L152XC) || defined(STM32L151XD) || \
      defined(STM32L152XD) || defined(STM32L162XD)
     #define OS_IO_TIM5               46  /* TIM5 global */
     #define OS_IO_SPI3               47  /* SPI3 global */
  #endif
  #if defined(STM32L151XD) || defined(STM32L152XD) || defined(STM32L162XD)
     #define OS_IO_UART4              48  /* UART4 global */
     #define OS_IO_UART5              49  /* UART5 global */
  #endif
  #if defined(STM32L151XC) || defined(STM32L152XC) || defined(STM32L151XD) || \
      defined(STM32L152XD) || defined(STM32L162XD)
     #define OS_IO_DMA2_Channel1      50  /* DMA2 Channel 1 global */
     #define OS_IO_DMA2_Channel2      51  /* DMA2 Channel 2 global */
     #define OS_IO_DMA2_Channel3      52  /* DMA2 Channel 3 global */
     #define OS_IO_DMA2_Channel4      53  /* DMA2 Channel 4 global */
     #define OS_IO_DMA2_Channel5      54  /* DMA2 Channel 5 global */
  #endif
  #if defined(STM32L162XD)
     #define OS_IO_AES                55  /* AES global */
  #endif
  #if defined(STM32L151XC) || defined(STM32L152XC) || defined(STM32L151XD) || \
      defined(STM32L152XD) || defined(STM32L162XD)
     #define OS_IO_COMP_ACQ           56  /* Comparator Channel Acquisition global */
  #endif
/* ------------------------------ STM32F1 --------------------------------------------- */
#elif defined(STM32F1XXXX)
  #if defined(STM32F100X4_X6) || defined(STM32F100X8_XB) || \
      defined(STM32F100RC_RD_RE) || defined(STM32F100VC_VD_VE_ZC_ZD_ZE) || \
      defined(STM32F101X4_X6) || defined(STM32F101T8_TB) || \
      defined(STM32F101C8_CB_R8_RB_V8_VB) || defined(STM32F101RC_RD_RE) || \
      defined(STM32F101VC_VD_VE_ZC_ZD_ZE) || defined(STM32F102X4_X6) || \
      defined(STM32F102X8_XB)
     #define OS_IO_ADC1               18 /* ADC1 global */
  #elif defined(STM32F101RF_RG) || defined(STM32F101VF_VG_ZF_ZG) || \
        defined(STM32F103X4_X6) || defined(STM32F103T8_TB) || \
        defined(STM32F103C8_CB_R8_RB_V8_VB) || defined(STM32F105XX) || \
        defined(STM32F107XX)
     #define OS_IO_ADC1_2             18 /* ADC1 and ADC2 global */
  #endif
  #if defined(STM32F103X4_X6) || defined(STM32F103T8_TB) || \
      defined(STM32F103C8_CB_R8_RB_V8_VB) || defined(STM32F103RC_RD_RE) || \
      defined(STM32F103VC_VD_VE_ZC_ZD_ZE) || defined(STM32F103RF_RG) || \
      defined(STM32F103VF_VG_ZF_ZG)
     #define OS_IO_USB_HP_CAN1_TX     19 /* USB Device High Priority or CAN1 TX */
  #elif defined(STM32F102X4_X6) || defined(STM32F102X8_XB)
     #define OS_IO_USB_HP             19 /* USB Device High Priority */
  #elif defined(STM32F105XX) || defined(STM32F107XX)
     #define OS_IO_CAN1_TX            19 /* CAN1 TX */
  #endif
  #if defined(STM32F103X4_X6) || defined(STM32F103T8_TB) || \
      defined(STM32F103C8_CB_R8_RB_V8_VB) || defined(STM32F103RC_RD_RE) || \
      defined(STM32F103VC_VD_VE_ZC_ZD_ZE) || defined(STM32F103RF_RG) || \
      defined(STM32F103VF_VG_ZF_ZG)
     #define OS_IO_USB_LP_CAN1_RX0    20 /* USB Device Low Priority or CAN1 RX0 */
  #elif defined(STM32F102X4_X6) || defined(STM32F102X8_XB)
     #define OS_IO_USB_LP             20 /* USB Device Low Priority */
  #elif defined(STM32F105XX) || defined(STM32F107XX)
     #define OS_IO_CAN1_RX0           20 /* CAN1 RX0 */
  #endif
  #if defined(STM32F103X4_X6) || defined(STM32F103T8_TB) || \
      defined(STM32F103C8_CB_R8_RB_V8_VB) || defined(STM32F103RC_RD_RE) || \
      defined(STM32F103VC_VD_VE_ZC_ZD_ZE) || defined(STM32F103RF_RG) || \
      defined(STM32F103VF_VG_ZF_ZG) || defined(STM32F105XX) || defined(STM32F107XX)
     #define OS_IO_CAN1_RX1           21 /* CAN1 RX1 */
  #endif
  #if defined(STM32F103X4_X6) || defined(STM32F103T8_TB) || \
      defined(STM32F103C8_CB_R8_RB_V8_VB) || defined(STM32F103RC_RD_RE) || \
      defined(STM32F103VC_VD_VE_ZC_ZD_ZE) || defined(STM32F103RF_RG) || \
      defined(STM32F103VF_VG_ZF_ZG) || defined(STM32F105XX) || defined(STM32F107XX)
     #define OS_IO_CAN1_SCE           22 /* CAN1 SCE */
  #endif
  #define OS_IO_EXTI9_5               23 /* External Line[9:5] */
  #if defined(STM32F103X4_X6) || defined(STM32F103T8_TB) || \
      defined(STM32F103C8_CB_R8_RB_V8_VB) || defined(STM32F103RC_RD_RE) || \
      defined(STM32F103VC_VD_VE_ZC_ZD_ZE) || defined(STM32F105XX) || defined(STM32F107XX)
     #define OS_IO_TIM1_BRK           24 /* TIM1 Break */
     #define OS_IO_TIM1_UP            25 /* TIM1 Update */
     #define OS_IO_TIM1_TRG_COM       26 /* TIM1 Trigger and Commutation */
     #define OS_IO_TIM1_CC            27 /* TIM1 Capture Compare */
     #define OS_IO_TIM1               OS_IO_TIM1_CC
  #elif defined(STM32F100X4_X6) || defined(STM32F100X8_XB) || \
        defined(STM32F100RC_RD_RE) || defined(STM32F100VC_VD_VE_ZC_ZD_ZE)
     #define OS_IO_TIM1_BRK_TIM15     24 /* TIM1 Break and TIM15 */
     #define OS_IO_TIM15 OS_IO_TIM1_BRK_TIM15
     #define OS_IO_TIM1_UP_TIM16      25 /* TIM1 Update and TIM16 */
     #define OS_IO_TIM1_UP            OS_IO_TIM1_UP_TIM16
     #define OS_IO_TIM16              OS_IO_TIM1_UP_TIM16
     #define OS_IO_TIM1_TRG_COM_TIM17 26 /* TIM1 Trigger and Commutation and TIM17 */
     #define OS_IO_TIM17              OS_IO_TIM1_TRG_COM_TIM17
     #define OS_IO_TIM1_CC            27 /* TIM1 Capture Compare */
     #define OS_IO_TIM1               OS_IO_TIM1_CC
  #elif defined (STM32F101RF_RG)|| defined(STM32F101VF_VG_ZF_ZG) || \
        defined(STM32F103RF_RG) || defined(STM32F103VF_VG_ZF_ZG)
     #define OS_IO_TIM1_BRK_TIM9      24 /* TIM1 Break  and TIM9 global */
     #define OS_IO_TIM9 OS_IO_TIM1_BRK_TIM9
     #define OS_IO_TIM1_UP_TIM10      25 /* TIM1 Update and TIM10 global */
     #define OS_IO_TIM1_UP            OS_IO_TIM1_UP_TIM10
     #define OS_IO_TIM10              OS_IO_TIM1_UP_TIM10
     #define OS_IO_TIM1_TRG_COM_TIM11 26 /* TIM1 Trigger and Commutation  and TIM11
                                            global */
     #define OS_IO_TIM11 OS_IO_TIM1_TRG_COM_TIM11
     #define OS_IO_TIM1_CC            27 /* TIM1 Capture Compare */
     #define OS_IO_TIM1               OS_IO_TIM1_CC
  #endif
  #define OS_IO_TIM2                  28 /* TIM2 global */
  #define OS_IO_TIM3                  29 /* TIM3 global */
  #if defined (STM32F100X8_XB) || defined (STM32F100RC_RD_RE) || \
      defined (STM32F100VC_VD_VE_ZC_ZD_ZE) || defined(STM32F101T8_TB) || \
      defined(STM32F101C8_CB_R8_RB_V8_VB) || defined(STM32F101RC_RD_RE) || \
      defined(STM32F101VC_VD_VE_ZC_ZD_ZE) || defined(STM32F102X8_XB) || \
      defined(STM32F103T8_TB) || defined(STM32F103C8_CB_R8_RB_V8_VB) || \
      defined(STM32F103RC_RD_RE) || defined(STM32F103VC_VD_VE_ZC_ZD_ZE) || \
      defined (STM32F101RF_RG)|| defined(STM32F101VF_VG_ZF_ZG) || \
      defined(STM32F103RF_RG) || defined(STM32F103VF_VG_ZF_ZG) || \
      defined(STM32F105XX) || defined(STM32F107XX)
     #define OS_IO_TIM4               30 /* TIM4 global */
  #endif
  #define OS_IO_I2C1_EV               31 /* I2C1 Event */
  #define OS_IO_I2C1_ER               32 /* I2C1 Error */
  #if defined(STM32F100X8_XB) || defined(STM32F100RC_RD_RE) || \
      defined(STM32F100VC_VD_VE_ZC_ZD_ZE) || defined(STM32F101C8_CB_R8_RB_V8_VB) || \
      defined(STM32F101RC_RD_RE) || defined(STM32F101VC_VD_VE_ZC_ZD_ZE) || \
      defined(STM32F101RF_RG) || defined(STM32F101VF_VG_ZF_ZG) || \
      defined(STM32F102X8_XB) || defined(STM32F103C8_CB_R8_RB_V8_VB) || \
      defined(STM32F103RC_RD_RE) || defined(STM32F103VC_VD_VE_ZC_ZD_ZE) || \
      defined(STM32F103RF_RG) || defined(STM32F103VF_VG_ZF_ZG) || \
      defined(STM32F105XX) || defined(STM32F107XX)
     #define OS_IO_I2C2_EV            33 /* I2C2 Event */
     #define OS_IO_I2C2_ER            34 /* I2C2 Error */
  #endif
  #define OS_IO_SPI1                  35 /* SPI1 global */
  #if defined(STM32F100X8_XB) || defined(STM32F100RC_RD_RE) || \
      defined(STM32F100VC_VD_VE_ZC_ZD_ZE) || defined(STM32F101C8_CB_R8_RB_V8_VB) || \
      defined(STM32F101RC_RD_RE) || defined(STM32F101VC_VD_VE_ZC_ZD_ZE) || \
      defined(STM32F101RF_RG) || defined(STM32F101VF_VG_ZF_ZG) || \
      defined(STM32F102X8_XB) || defined(STM32F103C8_CB_R8_RB_V8_VB) || \
      defined(STM32F103RC_RD_RE) || defined(STM32F103VC_VD_VE_ZC_ZD_ZE) || \
      defined(STM32F103RF_RG) || defined(STM32F103VF_VG_ZF_ZG) || \
      defined(STM32F105XX) || defined(STM32F107XX)
     #define OS_IO_SPI2               36 /* SPI2 global */
  #endif
  #define OS_IO_USART1                37 /* USART1 global */
  #define OS_IO_USART2                38 /* USART2 global */
  #if defined(STM32F100X8_XB) || defined(STM32F100RC_RD_RE) || \
      defined(STM32F100VC_VD_VE_ZC_ZD_ZE) || defined(STM32F101C8_CB_R8_RB_V8_VB) || \
      defined(STM32F101RC_RD_RE) || defined(STM32F101VC_VD_VE_ZC_ZD_ZE) || \
      defined(STM32F101RF_RG) || defined(STM32F101VF_VG_ZF_ZG) || \
      defined(STM32F102X8_XB) || defined(STM32F103C8_CB_R8_RB_V8_VB) || \
      defined(STM32F103RC_RD_RE) || defined(STM32F103VC_VD_VE_ZC_ZD_ZE) || \
      defined(STM32F103RF_RG) || defined(STM32F103VF_VG_ZF_ZG) || \
      defined(STM32F105XX) || defined(STM32F107XX)
     #define OS_IO_USART3             39 /* USART3 global */
  #endif
  #define OS_IO_EXTI15_10             40 /* External Line[15:10] */
  #define OS_IO_RTCAlarm              41 /* RTC Alarm through EXTI Line */
  #if defined(STM32F103X4_X6) || defined(STM32F103T8_TB) || \
      defined(STM32F103C8_CB_R8_RB_V8_VB) || defined(STM32F103RC_RD_RE) || \
      defined(STM32F103VC_VD_VE_ZC_ZD_ZE) || defined(STM32F103RF_RG) || \
      defined(STM32F103VF_VG_ZF_ZG) || defined(STM32F102X4_X6) || defined(STM32F102X8_XB)
     #define OS_IO_USBWakeUp          42 /* USB Device WakeUp from suspend through EXTI
                                            Line */
  #elif defined(STM32F100X4_X6) || defined(STM32F100X8_XB) || \
        defined(STM32F100RC_RD_RE) || defined(STM32F100VC_VD_VE_ZC_ZD_ZE)
     #define OS_IO_CEC                42 /* HDMI-CEC */
  #elif defined(STM32F105XX) || defined(STM32F107XX)
     #define OS_IO_OTG_FS_WKUP        42 /* USB OTG FS WakeUp from suspend through EXTI
                                            Line */
  #endif
  #if defined(STM32F101RC_RD_RE) || defined(STM32F101VC_VD_VE_ZC_ZD_ZE) || \
      defined(STM32F103RC_RD_RE) || defined(STM32F103VC_VD_VE_ZC_ZD_ZE)
     #define OS_IO_TIM8_BRK           43 /* TIM8 Break */
     #define OS_IO_TIM8_UP            44 /* TIM8 Update */
     #define OS_IO_TIM8_TRG_COM       45 /* TIM8 Trigger and Commutation */
     #define OS_IO_TIM8_CC            46 /* TIM8 Capture Compare */
     #define OS_IO_TIM8               OS_IO_TIM8_CC
  #elif defined(STM32F100RC_RD_RE) || defined(STM32F100VC_VD_VE_ZC_ZD_ZE )
     #define OS_IO_TIM12              43 /* TIM12 global */
     #define OS_IO_TIM13              44 /* TIM13 global */
     #define OS_IO_TIM14              45 /* TIM14 global */
  #elif defined(STM32F101RF_RG) || defined(STM32F101VF_VG_ZF_ZG) || \
        defined(STM32F103RF_RG) || defined(STM32F103VF_VG_ZF_ZG)
     #define OS_IO_TIM8_BRK_TIM12     43 /* TIM8 Break and TIM12 global */
     #define OS_IO_TIM12              OS_IO_TIM8_BRK_TIM12
     #define OS_IO_TIM8_UP_TIM13      44 /* TIM8 Update and TIM13 global */
     #define OS_IO_TIM8_UP            OS_IO_TIM8_UP_TIM13
     #define OS_IO_TIM13              OS_IO_TIM8_UP_TIM13
     #define OS_IO_TIM8_TRG_COM_TIM14 45 /* TIM8 Trigger and Commutation  and TIM14
                                            global */
     #define OS_IO_TIM14              OS_IO_TIM8_TRG_COM_TIM14
     #define OS_IO_TIM8_CC            46 /* TIM8 Capture Compare */
     #define OS_IO_TIM8               OS_IO_TIM8_CC
  #endif
  #if defined(STM32F103RC_RD_RE) || defined(STM32F103VC_VD_VE_ZC_ZD_ZE) || \
      defined(STM32F103RF_RG) || defined(STM32F103VF_VG_ZF_ZG)
     #define OS_IO_ADC3               47 /* ADC3 global */
  #endif
  #if defined(STM32F100VC_VD_VE_ZC_ZD_ZE) || defined(STM32F101VC_VD_VE_ZC_ZD_ZE) || \
      defined(STM32F101VF_VG_ZF_ZG) || defined(STM32F103VC_VD_VE_ZC_ZD_ZE) || \
      defined(STM32F103VF_VG_ZF_ZG)
     #define OS_IO_FSMC               48 /* FSMC global */
  #endif
  #if defined(STM32F103RC_RD_RE) || defined(STM32F103VC_VD_VE_ZC_ZD_ZE) || \
      defined(STM32F103RF_RG) || defined(STM32F103VF_VG_ZF_ZG)
     #define OS_IO_SDIO               49 /* SDIO global */
  #endif
  #if defined(STM32F100RC_RD_RE) || defined(STM32F100VC_VD_VE_ZC_ZD_ZE) || \
      defined(STM32F101RC_RD_RE) || defined(STM32F101VC_VD_VE_ZC_ZD_ZE) || \
      defined(STM32F101RF_RG) || defined(STM32F101VF_VG_ZF_ZG) || \
      defined(STM32F103RC_RD_RE) || defined(STM32F103VC_VD_VE_ZC_ZD_ZE) || \
      defined(STM32F103RF_RG) || defined(STM32F103VF_VG_ZF_ZG) || \
      defined(STM32F105XX) || defined(STM32F107XX)
     #define OS_IO_TIM5               50 /* TIM5 global */
     #define OS_IO_SPI3               51 /* SPI3 global */
  #endif
  #if defined(STM32F100RC_RD_RE) || defined(STM32F100VC_VD_VE_ZC_ZD_ZE) || \
      defined(STM32F101RC_RD_RE) || defined(STM32F101VC_VD_VE_ZC_ZD_ZE) || \
      defined(STM32F101RF_RG) || defined(STM32F101VF_VG_ZF_ZG) || \
      defined(STM32F103RC_RD_RE) || defined(STM32F103VC_VD_VE_ZC_ZD_ZE) || \
      defined(STM32F103RF_RG) || defined(STM32F103VF_VG_ZF_ZG) || \
      defined(STM32F105XX) || defined(STM32F107XX)
     #define OS_IO_UART4              52 /* UART4 global */
     #define OS_IO_UART5              53 /* UART5 global */
  #endif
  #if defined(STM32F100X4_X6) || defined(STM32F100X8_XB) || \
      defined(STM32F100RC_RD_RE) || defined(STM32F100VC_VD_VE_ZC_ZD_ZE)
     #define OS_IO_TIM6_DAC           54 /* TIM6 and DAC underrun */
  #elif defined(STM32F101RC_RD_RE) || defined(STM32F101VC_VD_VE_ZC_ZD_ZE) || \
        defined(STM32F101RF_RG) || defined(STM32F101VF_VG_ZF_ZG) || \
        defined(STM32F103RC_RD_RE) || defined(STM32F103VC_VD_VE_ZC_ZD_ZE) || \
        defined(STM32F103RF_RG) || defined(STM32F103VF_VG_ZF_ZG) || \
        defined(STM32F105XX) || defined(STM32F107XX)
     #define OS_IO_TIM6               54 /* TIM6 global */
  #endif
  #if defined(STM32F100X4_X6) || defined(STM32F100X8_XB) || \
      defined(STM32F100RC_RD_RE) || defined(STM32F100VC_VD_VE_ZC_ZD_ZE) || \
      defined(STM32F101RC_RD_RE) || defined(STM32F101VC_VD_VE_ZC_ZD_ZE) || \
      defined(STM32F101RF_RG) || defined(STM32F101VF_VG_ZF_ZG) || \
      defined(STM32F103RC_RD_RE) || defined(STM32F103VC_VD_VE_ZC_ZD_ZE) || \
      defined(STM32F103RF_RG) || defined(STM32F103VF_VG_ZF_ZG) || \
      defined(STM32F105XX) || defined(STM32F107XX)
     #define OS_IO_TIM7               55 /* TIM7 */
  #endif
  #if defined(STM32F100RC_RD_RE) || defined(STM32F100VC_VD_VE_ZC_ZD_ZE) || \
      defined(STM32F101RC_RD_RE) || defined(STM32F101VC_VD_VE_ZC_ZD_ZE) || \
      defined(STM32F101RF_RG) || defined(STM32F101VF_VG_ZF_ZG) || \
      defined(STM32F103RC_RD_RE) || defined(STM32F103VC_VD_VE_ZC_ZD_ZE) || \
      defined(STM32F103RF_RG) || defined(STM32F103VF_VG_ZF_ZG) || \
      defined(STM32F105XX) || defined(STM32F107XX)
     #define OS_IO_DMA2_Channel1      56 /* DMA2 Channel 1 global */
     #define OS_IO_DMA2_Channel2      57 /* DMA2 Channel 2 global */
     #define OS_IO_DMA2_Channel3      58 /* DMA2 Channel 3 global */
  #endif
  #if defined(STM32F100RC_RD_RE) || defined(STM32F100VC_VD_VE_ZC_ZD_ZE) || \
      defined(STM32F101RC_RD_RE) || defined(STM32F101VC_VD_VE_ZC_ZD_ZE) || \
      defined(STM32F101RF_RG) || defined(STM32F101VF_VG_ZF_ZG) || \
      defined(STM32F103RC_RD_RE) || defined(STM32F103VC_VD_VE_ZC_ZD_ZE) || \
      defined(STM32F103RF_RG) || defined(STM32F103VF_VG_ZF_ZG)
     #define OS_IO_DMA2_Channel4_5    59 /* DMA2 Channel 4 and Channel 5 global */
  #elif defined(STM32F105XX) || defined(STM32F107XX)
     #define OS_IO_DMA2_Channel4      59 /* DMA2 Channel 4 global */
  #endif
  #if defined(STM32F100RC_RD_RE) || defined(STM32F100VC_VD_VE_ZC_ZD_ZE)
     #define OS_IO_DMA2_Channel5      60 /* DMA2 Channel 5 global  (DMA2 Channel 5 is
                                            mapped at position 60 only if the MISC_REMAP
                                            bit in the AFIO_MAPR2 register is set) */
  #elif defined(STM32F105XX) || defined(STM32F107XX)
     #define OS_IO_DMA2_Channel5      60 /* DMA2 Channel 5 global */
  #endif
  #if defined(STM32F107XX)
     #define OS_IO_ETH                61 /* Ethernet global */
     #define OS_IO_ETH_WKUP           62 /* Ethernet Wakeup through EXTI line */
  #endif
  #if defined(STM32F105XX) || defined(STM32F107XX)
     #define OS_IO_CAN2_TX            63 /* CAN2 TX */
     #define OS_IO_CAN2_RX0           64 /* CAN2 RX0 */
     #define OS_IO_CAN2_RX1           65 /* CAN2 RX1 */
     #define OS_IO_CAN2_SCE           66 /* CAN2 SCE */
     #define OS_IO_OTG_FS             67 /* USB OTG FS global */
  #endif
/* ------------------------------ STM32F2 and STM32F4 --------------------------------- */
#elif defined(STM32F2XXXX) || defined(STM32F4XXXX)
  #define OS_IO_ADC                   18  /* ADC1, ADC2 and ADC3 global  */
  #define OS_IO_CAN1_TX               19  /* CAN1 TX */
  #define OS_IO_CAN1_RX0              20  /* CAN1 RX0 */
  #define OS_IO_CAN1_RX1              21  /* CAN1 RX1 */
  #define OS_IO_CAN1_SCE              22  /* CAN1 SCE */
  #define OS_IO_EXTI9_5               23  /* External Line[9:5]  */
  #define OS_IO_TIM1_BRK_TIM9         24  /* TIM1 Break and TIM9 global */
  #define OS_IO_TIM9                  OS_IO_TIM1_BRK_TIM9
  #define OS_IO_TIM1_UP_TIM10         25  /* TIM1 Update and TIM10 global */
  #define OS_IO_TIM1_UP               OS_IO_TIM1_UP_TIM10
  #define OS_IO_TIM10                 OS_IO_TIM1_UP_TIM10
  #define OS_IO_TIM1_TRG_COM_TIM11    26  /* TIM1 Trigger and Commutation and TIM11
                                             global */
  #define OS_IO_TIM11                 OS_IO_TIM1_TRG_COM_TIM11
  #define OS_IO_TIM1_CC               27  /* TIM1 Capture Compare */
  #define OS_IO_TIM1                  OS_IO_TIM1_CC
  #define OS_IO_TIM2                  28  /* TIM2 global */
  #define OS_IO_TIM3                  29  /* TIM3 global */
  #define OS_IO_TIM4                  30  /* TIM4 global */
  #define OS_IO_I2C1_EV               31  /* I2C1 Event */
  #define OS_IO_I2C1_ER               32  /* I2C1 Error */
  #define OS_IO_I2C2_EV               33  /* I2C2 Event */
  #define OS_IO_I2C2_ER               34  /* I2C2 Error */
  #define OS_IO_SPI1                  35  /* SPI1 global */
  #define OS_IO_SPI2                  36  /* SPI2 global */
  #define OS_IO_USART1                37  /* USART1 global */
  #define OS_IO_USART2                38  /* USART2 global */
  #define OS_IO_USART3                39  /* USART3 global */
  #define OS_IO_EXTI15_10             40  /* External Line[15:10]  */
  #define OS_IO_RTC_Alarm             41  /* RTC Alarm (A and B) through EXTI Line */
  #define OS_IO_OTG_FS_WKUP           42  /* USB OTG FS Wakeup through EXTI line */
  #define OS_IO_TIM8_BRK_TIM12        43  /* TIM8 Break and TIM12 global */
  #define OS_IO_TIM12                 OS_IO_TIM8_BRK_TIM12
  #define OS_IO_TIM8_UP_TIM13         44  /* TIM8 Update and TIM13 global */
  #define OS_IO_TIM8_UP               OS_IO_TIM8_UP_TIM13
  #define OS_IO_TIM13                 OS_IO_TIM8_UP_TIM13
  #define OS_IO_TIM8_TRG_COM_TIM14    45  /* TIM8 Trigger and Commutation and TIM14
                                             global */
  #define OS_IO_TIM14                 OS_IO_TIM8_TRG_COM_TIM14
  #define OS_IO_TIM8_CC               46  /* TIM8 Capture Compare */
  #define OS_IO_TIM8                  OS_IO_TIM8_CC
  #define OS_IO_DMA1_Stream7          47  /* DMA1 Stream7 */
  #if defined(STM32F205VX_ZX) || defined(STM32F215VX_ZX) || defined(STM32F207XX) || \
      defined(STM32F217XX) || defined(STM32F405VX_ZX) || defined(STM32F415VX_ZX) || \
      defined(STM32F407XX) || defined(STM32F417XX)
     #define OS_IO_FSMC               48  /* FSMC global */
  #endif
  #define OS_IO_SDIO                  49  /* SDIO global */
  #define OS_IO_TIM5                  50  /* TIM5 global */
  #define OS_IO_SPI3                  51  /* SPI3 global */
  #define OS_IO_UART4                 52  /* UART4 global */
  #define OS_IO_UART5                 53  /* UART5 global */
  #define OS_IO_TIM6_DAC              54  /* TIM6 global and DAC1&2 underrun error   */
  #define OS_IO_TIM7                  55  /* TIM7 global */
  #define OS_IO_DMA2_Stream0          56  /* DMA2 Stream 0 global */
  #define OS_IO_DMA2_Stream1          57  /* DMA2 Stream 1 global */
  #define OS_IO_DMA2_Stream2          58  /* DMA2 Stream 2 global */
  #define OS_IO_DMA2_Stream3          59  /* DMA2 Stream 3 global */
  #define OS_IO_DMA2_Stream4          60  /* DMA2 Stream 4 global */
  #if defined(STM32F207XX) || defined(STM32F217XX) || defined(STM32F407XX) || \
      defined(STM32F417XX)
     #define OS_IO_ETH                61  /* Ethernet global */
     #define OS_IO_ETH_WKUP           62  /* Ethernet Wakeup through EXTI line */
  #endif
  #define OS_IO_CAN2_TX               63  /* CAN2 TX */
  #define OS_IO_CAN2_RX0              64  /* CAN2 RX0 */
  #define OS_IO_CAN2_RX1              65  /* CAN2 RX1 */
  #define OS_IO_CAN2_SCE              66  /* CAN2 SCE */
  #if defined(STM32F207XX) || defined(STM32F217XX) || defined(STM32F407XX) || \
      defined(STM32F417XX)
     #define OS_IO_OTG_FS             67  /* USB OTG FS global */
  #endif
  #define OS_IO_DMA2_Stream5          68  /* DMA2 Stream 5 global */
  #define OS_IO_DMA2_Stream6          69  /* DMA2 Stream 6 global */
  #define OS_IO_DMA2_Stream7          70  /* DMA2 Stream 7 global */
  #define OS_IO_USART6                71  /* USART6 global */
  #define OS_IO_I2C3_EV               72  /* I2C3 event */
  #define OS_IO_I2C3_ER               73  /* I2C3 error */
  #define OS_IO_OTG_HS_EP1_OUT        74  /* USB OTG HS End Point 1 Out global */
  #define OS_IO_OTG_HS_EP1_IN         75  /* USB OTG HS End Point 1 In global */
  #define OS_IO_OTG_HS_WKUP           76  /* USB OTG HS Wakeup through EXTI */
  #define OS_IO_OTG_HS                77  /* USB OTG HS global */
  #if defined(STM32F207XX) || defined(STM32F217XX) || defined(STM32F407XX) || \
      defined(STM32F417XX)
     #define OS_IO_DCMI               78  /* DCMI global */
  #endif
  #if defined(STM32F215RX) || defined(STM32F215VX_ZX) || defined(STM32F217XX) || \
      defined(STM32F415RG) || defined(STM32F415VG_ZG) || defined(STM32F417XX)
     #define OS_IO_CRYP               79  /* CRYP crypto global */
  #endif
  #define OS_IO_HASH_RNG              80  /* Hash and Rng global */
  #if defined(STM32F4XXXX)
     #define OS_IO_FPU                81  /* FPU global */
  #endif
#else
  #error STM32 version undefined
#endif


#endif /* _ZOTTAOS_INTERRUPTS_ */

