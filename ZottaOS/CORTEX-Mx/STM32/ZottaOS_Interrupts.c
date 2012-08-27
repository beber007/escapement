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
/* File ZottaOS_Interrupts.c: Defines the internal vector interruption tables that bind
**   peripheral device handlers to a particular entry and that can be manipulated via
**   functions OSSetISRDescriptor and OSGetISRDescriptor.
** Platform version: All STM32 microcontrollers.
** Version date: March 2012
** Authors: MIS-TIC
*/

#include "ZottaOS.h"

/* There are 2 different tables, which together determine the appropriate ISR associated
** with a device. The first table is composed of the list of peripheral interrupts speci-
** fic to a particular family of STM32 and is appended to the 15 system exceptions of the
** Cortex-Mx. The entries in this table are stored in Flash and can be _OSIOHandler (de-
** fined in ZottaOS_CortexMx.c) or UndefinedInterrupt (defined in this file). Basically
** _OSIOHandler extracts the IRQ number (0 through 239), extracts a ZottaOS ISR descriptor
** located in a 2nd table called _OSTabDevice, and then calls the function located in the
** first field of this descriptor. An IRQ can also map onto 2 different devices. In this
** the function called by _OSIOHandler should check which device raised the interrupt. */


/* IRQ HANDLER TABLE WHEN AN INTERRUPT IS RAISED (1st table) ***************************/
#ifdef DEBUG_MODE
/* UndefinedInterrupt: Dummy ISR to fall into an infinite loop when referring to an in-
** existent entry of the interrupt vector table. */
static void UndefinedInterrupt(void)
{
  UINT32 intNum;
  _OSDisableInterrupts();
  intNum  = (*((UINT32 *)0xE000ED04) & 0x1FF);
  intNum  -= 16;
  while (TRUE); // referring to an inexistent entry of the interrupt vector table.
} /* end of undefinedInterrupt */
#endif


/* STM32 specific interrupt vector table, which is appended by the linker to the end of
** table CortexM3VectorTable defined in ZottaOS_CortexM3.c. */
__attribute__ ((section(".isr_vector_specific")))
void (* const STM32VectorTable[])(void) = {
  _OSIOHandler,    /* 0  OS_IO_WWDG */
  _OSIOHandler,    /* 1  OS_IO_PVD */
  #if defined(STM32F05XXX)
     _OSIOHandler,    /* 2  OS_IO_RTC */
     _OSIOHandler,    /* 3  OS_IO_FLASH */
  #elif defined(STM32L1XXXX) || defined(STM32F2XXXX) || defined(STM32F4XXXX)
     _OSIOHandler,    /* 2  OS_IO_TAMP_STAMP */
     _OSIOHandler,    /* 3  OS_IO_RTC_WKUP */
  #elif defined(STM32F1XXXX)
     _OSIOHandler,    /* 2  OS_IO_TAMPER */
     _OSIOHandler,    /* 3  OS_IO_RTC */
  #else
      #error STM32 version undefined
  #endif
  #if defined(STM32F05XXX)
     _OSIOHandler,    /* 4  OS_IO_RCC */
     _OSIOHandler,    /* 5  OS_IO_EXTI0_1 */
     _OSIOHandler,    /* 6  OS_IO_EXTI2_3 */
     _OSIOHandler,    /* 7  OS_IO_EXTI4_15 */
     _OSIOHandler,    /* 8  OS_IO_TS */
     _OSIOHandler,    /* 9  OS_IO_DMA1_Channel1 */
     _OSIOHandler,    /* 10 OS_IO_DMA1_Channel2_3 */
  #elif defined(STM32L1XXXX) || defined(STM32F1XXXX) || defined(STM32F2XXXX) || \
        defined(STM32F4XXXX)
     _OSIOHandler,    /* 4  OS_IO_FLASH */
     _OSIOHandler,    /* 5  OS_IO_RCC */
     _OSIOHandler,    /* 6  OS_IO_EXTI0 */
     _OSIOHandler,    /* 7  OS_IO_EXTI1 */
     _OSIOHandler,    /* 8  OS_IO_EXTI2 */
     _OSIOHandler,    /* 9  OS_IO_EXTI3 */
     _OSIOHandler,    /* 10 OS_IO_EXTI4 */
  #else
      #error STM32 version undefined
  #endif
  #if defined(STM32F05XXX)
     _OSIOHandler,    /* 11 OS_IO_DMA1_Channel4_5 */
     _OSIOHandler,    /* 12 ADC1_COMP */
     _OSIOHandler,    /* 13 TIM1_BRK_UP_TRG_COM */
     _OSIOHandler,    /* 14 TIM1_CC */
     _OSIOHandler,    /* 15 TIM2 */
     _OSIOHandler,    /* 16 TIM3 */
     #if defined(STM32F051X4) || defined(STM32F051K6_K8_C6_R6) || defined(STM32F051C8_R8)
        _OSIOHandler, /* 17 TIM6_DAC */
     #elif defined(DEBUG_MODE)
        UndefinedInterrupt,
     #else
        NULL,
     #endif
  #elif defined(STM32F2XXXX) || defined(STM32F4XXXX)
     _OSIOHandler,    /* 11 OS_IO_DMA1_Stream0 */
     _OSIOHandler,    /* 12 OS_IO_DMA1_Stream1 */
     _OSIOHandler,    /* 13 OS_IO_DMA1_Stream2 */
     _OSIOHandler,    /* 14 OS_IO_DMA1_Stream3 */
     _OSIOHandler,    /* 15 OS_IO_DMA1_Stream4 */
     _OSIOHandler,    /* 16 OS_IO_DMA1_Stream5 */
     _OSIOHandler,    /* 17 OS_IO_DMA1_Stream6 */
  #elif defined(STM32L1XXXX) || defined(STM32F1XXXX)
     _OSIOHandler,    /* 11 OS_IO_DMA1_Channel1 */
     _OSIOHandler,    /* 12 OS_IO_DMA1_Channel2 */
     _OSIOHandler,    /* 13 OS_IO_DMA1_Channel3 */
     _OSIOHandler,    /* 14 OS_IO_DMA1_Channel4 */
     _OSIOHandler,    /* 15 OS_IO_DMA1_Channel5 */
     _OSIOHandler,    /* 16 OS_IO_DMA1_Channel6 */
     _OSIOHandler,    /* 17 OS_IO_DMA1_Channel7 */
  #else
     #error STM32 version undefined
  #endif
  /* ------------------------------ STM32F0 ------------------------------------------ */
  #if defined(STM32F05XXX)
     #if defined(DEBUG_MODE)
        UndefinedInterrupt,
     #else
        NULL,
     #endif
     _OSIOHandler,    /* 19 OS_IO_TIM14 */
     #if defined(STM32F051X4) || defined(STM32F051K6_K8_C6_R6) || defined(STM32F051C8_R8)
        _OSIOHandler, /* 20 OS_IO_TIM15 */
     #elif defined(DEBUG_MODE)
        UndefinedInterrupt,
     #else
        NULL,
     #endif
     _OSIOHandler,    /* 21 OS_IO_TIM16 */
     _OSIOHandler,    /* 22 OS_IO_TIM17 */
     _OSIOHandler,    /* 23 OS_IO_I2C1 */
     #if defined(STM32F051C8_R8)
        _OSIOHandler, /* 24 OS_IO_I2C2 */
     #elif defined(DEBUG_MODE)
        UndefinedInterrupt,
     #else
        NULL,
     #endif
     _OSIOHandler,    /* 25 OS_IO_SPI1 */
     #if defined(STM32F051C8_R8)
        _OSIOHandler, /* 26 OS_IO_SPI2 */
     #elif defined(DEBUG_MODE)
        UndefinedInterrupt,
     #else
        NULL,
     #endif
     _OSIOHandler,    /* 27 OS_IO_USART1 */
     #if defined(STM32F051K6_K8_C6_R6) || defined(STM32F051C8_R8)
        _OSIOHandler, /* 28 OS_IO_USART2 */
     #elif defined(DEBUG_MODE)
        UndefinedInterrupt,
     #else
        NULL,
     #endif
     #if defined(DEBUG_MODE)
        UndefinedInterrupt,
     #else
        NULL,
     #endif
     #if defined(STM32F051X4) || defined(STM32F051K6_K8_C6_R6) || defined(STM32F051C8_R8)
        _OSIOHandler, /* 30 OS_IO_CEC */
     #elif defined(DEBUG_MODE)
        UndefinedInterrupt,
     #else
        NULL,
     #endif
  /* ------------------------------ STM32L1 ------------------------------------------ */
  #elif defined(STM32L1XXXX)
     _OSIOHandler,    /* 18 OS_IO_ADC1 */
     _OSIOHandler,    /* 19 OS_IO_USB_HP */
     _OSIOHandler,    /* 20 OS_IO_USB_LP */
     _OSIOHandler,    /* 21 OS_IO_DAC */
     #if defined(STM32L151XC) || defined(STM32L152XC) || defined(STM32L151XD) || \
         defined(STM32L152XD) || defined(STM32L162XD)
        _OSIOHandler, /* 22 OS_IO_COMP_CA */
     #else
        _OSIOHandler, /* 22 OS_IO_COMP */
     #endif
     _OSIOHandler,    /* 23 OS_IO_EXTI9_5 */
     #if defined(STM32L152X6_X8_XB) || defined(STM32L152XC) || defined(STM32L152XD) || \
         defined(STM32L162XD)
        _OSIOHandler, /* 24 OS_IO_LCD */
     #elif defined(DEBUG_MODE)
        UndefinedInterrupt,
     #else
        NULL,
     #endif
     _OSIOHandler,    /* 25 OS_IO_TIM9 */
     _OSIOHandler,    /* 26 OS_IO_TIM10 */
     _OSIOHandler,    /* 27 OS_IO_TIM11 */
     _OSIOHandler,    /* 28 OS_IO_TIM2 */
     _OSIOHandler,    /* 29 OS_IO_TIM3 */
     _OSIOHandler,    /* 30 OS_IO_TIM4 */
     _OSIOHandler,    /* 31 OS_IO_I2C1_EV */
     _OSIOHandler,    /* 32 OS_IO_I2C1_ER */
     _OSIOHandler,    /* 33 OS_IO_I2C2_EV */
     _OSIOHandler,    /* 34 OS_IO_I2C2_ER */
     _OSIOHandler,    /* 35 OS_IO_SPI1 */
     _OSIOHandler,    /* 36 OS_IO_SPI2 */
     _OSIOHandler,    /* 37 OS_IO_USART1 */
     _OSIOHandler,    /* 38 OS_IO_USART2 */
     _OSIOHandler,    /* 39 OS_IO_USART3 */
     _OSIOHandler,    /* 40 OS_IO_EXTI15_10 */
     _OSIOHandler,    /* 41 OS_IO_RTC_Alarm */
     _OSIOHandler,    /* 42 OS_IO_USB_FS_WKUP */
     _OSIOHandler,    /* 43 OS_IO_TIM6 */
     _OSIOHandler,    /* 44 OS_IO_TIM7 */
     #if defined(STM32L151XD) || defined(STM32L152XD) || defined(STM32L162XD)
        _OSIOHandler, /* 45 OS_IO_SDIO */
     #elif defined(DEBUG_MODE)
        UndefinedInterrupt,
     #else
        NULL,
     #endif
     #if defined(STM32L151XC) || defined(STM32L152XC) || defined(STM32L151XD) || \
         defined(STM32L152XD) || defined(STM32L162XD)
        _OSIOHandler, /* 46 OS_IO_TIM5 */
        _OSIOHandler, /* 47 OS_IO_SPI3 */
     #elif defined(DEBUG_MODE)
        UndefinedInterrupt,
        UndefinedInterrupt,
     #else
        NULL,
        NULL,
     #endif
     #if defined(STM32L151XD) || defined(STM32L152XD) || defined(STM32L162XD)
        _OSIOHandler, /* 48 OS_IO_UART4 */
        _OSIOHandler, /* 49 OS_IO_UART5 */
     #elif defined(DEBUG_MODE)
        UndefinedInterrupt,
        UndefinedInterrupt,
     #else
        NULL,
        NULL,
     #endif
     #if defined(STM32L151XC) || defined(STM32L152XC) || defined(STM32L151XD) || \
         defined(STM32L152XD) || defined(STM32L162XD)
        _OSIOHandler, /* 50 OS_IO_DMA2_Channel1 */
        _OSIOHandler, /* 51 OS_IO_DMA2_Channel2 */
        _OSIOHandler, /* 52 OS_IO_DMA2_Channel3 */
        _OSIOHandler, /* 53 OS_IO_DMA2_Channel4 */
        _OSIOHandler, /* 54 OS_IO_DMA2_Channel5 */
     #elif defined(DEBUG_MODE)
        UndefinedInterrupt,
        UndefinedInterrupt,
        UndefinedInterrupt,
        UndefinedInterrupt,
        UndefinedInterrupt,
     #else
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
     #endif
     #if defined(STM32L162XD)
        _OSIOHandler, /* 55 OS_IO_AES */
     #elif defined(DEBUG_MODE)
        UndefinedInterrupt,
     #else
        NULL,
     #endif
     #if defined(STM32L151XC) || defined(STM32L152XC) || defined(STM32L151XD) || \
         defined(STM32L152XD) || defined(STM32L162XD)
        _OSIOHandler, /* 56 OS_IO_COMP_ACQ */
     #elif defined(DEBUG_MODE)
        UndefinedInterrupt,
     #else
        NULL,
     #endif
  /* ------------------------------ STM32F1 ------------------------------------------ */
  #elif defined(STM32F1XXXX)
     #if defined(STM32F100X4_X6) || defined(STM32F100X8_XB) || \
         defined(STM32F100RC_RD_RE) || defined(STM32F100VC_VD_VE_ZC_ZD_ZE) || \
         defined(STM32F101X4_X6) || defined(STM32F101T8_TB) || \
         defined(STM32F101C8_CB_R8_RB_V8_VB) || defined(STM32F101RC_RD_RE) || \
         defined(STM32F101VC_VD_VE_ZC_ZD_ZE) || defined(STM32F102X4_X6) || \
         defined(STM32F102X8_XB)
        _OSIOHandler, /* 18 OS_IO_ADC1 */
     #elif defined(STM32F101RF_RG) || defined(STM32F101VF_VG_ZF_ZG) || \
           defined(STM32F103X4_X6) || defined(STM32F103T8_TB) || \
           defined(STM32F103C8_CB_R8_RB_V8_VB) || defined(STM32F105XX) || \
           defined(STM32F107XX)
        _OSIOHandler, /* 18 OS_IO_ADC1_2 */
     #elif defined(DEBUG_MODE)
        UndefinedInterrupt,
     #else
        NULL,
     #endif
     #if defined(STM32F103X4_X6) || defined(STM32F103T8_TB) || \
         defined(STM32F103C8_CB_R8_RB_V8_VB) || defined(STM32F103RC_RD_RE) || \
         defined(STM32F103VC_VD_VE_ZC_ZD_ZE) || defined(STM32F103RF_RG) || \
         defined(STM32F103VF_VG_ZF_ZG)
        _OSIOHandler, /* 19 OS_IO_USB_HP_CAN1_TX */
     #elif defined(STM32F102X4_X6) || defined(STM32F102X8_XB)
        _OSIOHandler, /* 19 OS_IO_USB_HP */
     #elif defined(STM32F105XX) || defined(STM32F107XX)
        _OSIOHandler, /* 19 OS_IO_CAN1_TX */
     #elif defined(DEBUG_MODE)
        UndefinedInterrupt,
     #else
        NULL,
     #endif
     #if defined(STM32F103X4_X6) || defined(STM32F103T8_TB) || \
         defined(STM32F103C8_CB_R8_RB_V8_VB) || defined(STM32F103RC_RD_RE) || \
         defined(STM32F103VC_VD_VE_ZC_ZD_ZE) || defined(STM32F103RF_RG) || \
         defined(STM32F103VF_VG_ZF_ZG)
        _OSIOHandler, /* 20 OS_IO_USB_LP_CAN1_RX0 */
     #elif defined(STM32F102X4_X6) || defined(STM32F102X8_XB)
        _OSIOHandler, /* 20 OS_IO_USB_LP */
     #elif defined(STM32F105XX) || defined(STM32F107XX)
        _OSIOHandler, /* 20 OS_IO_CAN1_RX0 */
     #elif defined(DEBUG_MODE)
        UndefinedInterrupt,
     #else
        NULL,
     #endif
     #if defined(STM32F103X4_X6) || defined(STM32F103T8_TB) || \
         defined(STM32F103C8_CB_R8_RB_V8_VB) || defined(STM32F103RC_RD_RE) || \
         defined(STM32F103VC_VD_VE_ZC_ZD_ZE) || defined(STM32F103RF_RG) || \
         defined(STM32F103VF_VG_ZF_ZG) || defined(STM32F105XX) || defined(STM32F107XX)
        _OSIOHandler, /* 21 OS_IO_CAN1_RX1 */
     #elif defined(DEBUG_MODE)
        UndefinedInterrupt,
     #else
        NULL,
     #endif
     #if defined(STM32F103X4_X6) || defined(STM32F103T8_TB) || \
         defined(STM32F103C8_CB_R8_RB_V8_VB) || defined(STM32F103RC_RD_RE) || \
         defined(STM32F103VC_VD_VE_ZC_ZD_ZE) || defined(STM32F103RF_RG) || \
         defined(STM32F103VF_VG_ZF_ZG) || defined(STM32F105XX) || defined(STM32F107XX)
        _OSIOHandler, /* 22 OS_IO_CAN1_SCE */
     #elif defined(DEBUG_MODE)
        UndefinedInterrupt,
     #else
        NULL,
     #endif
     _OSIOHandler,    /* 23 OS_IO_EXTI9_5 */
     #if defined(STM32F103X4_X6) || defined(STM32F103T8_TB) || \
         defined(STM32F103C8_CB_R8_RB_V8_VB) || defined(STM32F103RC_RD_RE) || \
         defined(STM32F103VC_VD_VE_ZC_ZD_ZE) || defined(STM32F105XX) || \
         defined(STM32F107XX)
        _OSIOHandler, /* 24 OS_IO_TIM1_BRK */
        _OSIOHandler, /* 25 OS_IO_TIM1_UP */
        _OSIOHandler, /* 26 OS_IO_TIM1_TRG_COM */
        _OSIOHandler, /* 27 OS_IO_TIM1_CC */
     #elif defined(STM32F100X4_X6) || defined(STM32F100X8_XB) || \
           defined(STM32F100RC_RD_RE) || defined(STM32F100VC_VD_VE_ZC_ZD_ZE)
        _OSIOHandler, /* 24 OS_IO_TIM1_BRK_TIM15 */
        _OSIOHandler, /* 25 OS_IO_TIM1_UP_TIM16 */
        _OSIOHandler, /* 26 OS_IO_TIM1_TRG_COM_TIM17 */
        _OSIOHandler, /* 27 OS_IO_TIM1_CC */
     #elif defined (STM32F101RF_RG)|| defined(STM32F101VF_VG_ZF_ZG)
        _OSIOHandler, /* 24 OS_IO_TIM9 */
        _OSIOHandler, /* 25 OS_IO__TIM10 */
        _OSIOHandler, /* 26 OS_IO_TIM11 */
        #if defined(DEBUG_MODE)
           UndefinedInterrupt,
        #else
           NULL,
        #endif
     #elif defined(STM32F103RF_RG) || defined(STM32F103VF_VG_ZF_ZG)
        _OSIOHandler, /* 24 OS_IO_TIM1_BRK_TIM9 */
        _OSIOHandler, /* 25 OS_IO_TIM1_UP_TIM10 */
        _OSIOHandler, /* 26 OS_IO_TIM1_TRG_COM_TIM11 */
        _OSIOHandler, /* 27 OS_IO_TIM1_CC */
     #elif defined(DEBUG_MODE)
        UndefinedInterrupt,
        UndefinedInterrupt,
        UndefinedInterrupt,
        UndefinedInterrupt,
     #else
        NULL,
        NULL,
        NULL,
        NULL,
     #endif
     _OSIOHandler,    /* 28 OS_IO_TIM2 */
     _OSIOHandler,    /* 29 OS_IO_TIM3 */
     #if defined (STM32F100X8_XB) || defined (STM32F100RC_RD_RE) || \
         defined (STM32F100VC_VD_VE_ZC_ZD_ZE) || defined(STM32F101T8_TB) || \
         defined(STM32F101C8_CB_R8_RB_V8_VB) || defined(STM32F101RC_RD_RE) || \
         defined(STM32F101VC_VD_VE_ZC_ZD_ZE) || defined(STM32F102X8_XB) || \
         defined(STM32F103T8_TB) || defined(STM32F103C8_CB_R8_RB_V8_VB) || \
         defined(STM32F103RC_RD_RE) || defined(STM32F103VC_VD_VE_ZC_ZD_ZE) || \
         defined (STM32F101RF_RG)|| defined(STM32F101VF_VG_ZF_ZG) || \
         defined(STM32F103RF_RG) || defined(STM32F103VF_VG_ZF_ZG) || \
         defined(STM32F105XX) || defined(STM32F107XX)
        _OSIOHandler, /* 30 OS_IO_TIM4 */
     #elif defined(DEBUG_MODE)
        UndefinedInterrupt,
     #else
        NULL,
     #endif
     _OSIOHandler,    /* 31 OS_IO_I2C1_EV */
     _OSIOHandler,    /* 32 OS_IO_I2C1_ER */
     #if defined(STM32F100X8_XB) || defined(STM32F100RC_RD_RE) || \
         defined(STM32F100VC_VD_VE_ZC_ZD_ZE) || defined(STM32F101C8_CB_R8_RB_V8_VB) || \
         defined(STM32F101RC_RD_RE) || defined(STM32F101VC_VD_VE_ZC_ZD_ZE) || \
         defined(STM32F101RF_RG) || defined(STM32F101VF_VG_ZF_ZG) || \
         defined(STM32F102X8_XB) || defined(STM32F103C8_CB_R8_RB_V8_VB) || \
         defined(STM32F103RC_RD_RE) || defined(STM32F103VC_VD_VE_ZC_ZD_ZE) || \
         defined(STM32F103RF_RG) || defined(STM32F103VF_VG_ZF_ZG) || \
         defined(STM32F105XX) || defined(STM32F107XX)
        _OSIOHandler, /* 33 OS_IO_I2C2_EV */
        _OSIOHandler, /* 34 OS_IO_I2C2_ER */
     #elif defined(DEBUG_MODE)
        UndefinedInterrupt,
        UndefinedInterrupt,
     #else
        NULL,
        NULL,
     #endif
     _OSIOHandler,    /* 35 OS_IO_SPI1 */
     #if defined(STM32F100X8_XB) || defined(STM32F100RC_RD_RE) || \
         defined(STM32F100VC_VD_VE_ZC_ZD_ZE) || defined(STM32F101C8_CB_R8_RB_V8_VB) || \
         defined(STM32F101RC_RD_RE) || defined(STM32F101VC_VD_VE_ZC_ZD_ZE) || \
         defined(STM32F101RF_RG) || defined(STM32F101VF_VG_ZF_ZG) || \
         defined(STM32F102X8_XB) || defined(STM32F103C8_CB_R8_RB_V8_VB) || \
         defined(STM32F103RC_RD_RE) || defined(STM32F103VC_VD_VE_ZC_ZD_ZE) || \
         defined(STM32F103RF_RG) || defined(STM32F103VF_VG_ZF_ZG) || \
         defined(STM32F105XX) || defined(STM32F107XX)
        _OSIOHandler, /* 36 OS_IO_SPI2 */
     #elif defined(DEBUG_MODE)
        UndefinedInterrupt,
     #else
        NULL,
     #endif
     _OSIOHandler,    /* 37 OS_IO_USART1 */
     _OSIOHandler,    /* 38 OS_IO_USART2 */
     #if defined(STM32F100X8_XB) || defined(STM32F100RC_RD_RE) || \
         defined(STM32F100VC_VD_VE_ZC_ZD_ZE) || defined(STM32F101C8_CB_R8_RB_V8_VB) || \
         defined(STM32F101RC_RD_RE) || defined(STM32F101VC_VD_VE_ZC_ZD_ZE) || \
         defined(STM32F101RF_RG) || defined(STM32F101VF_VG_ZF_ZG) || \
         defined(STM32F102X8_XB) || defined(STM32F103C8_CB_R8_RB_V8_VB) || \
         defined(STM32F103RC_RD_RE) || defined(STM32F103VC_VD_VE_ZC_ZD_ZE) || \
         defined(STM32F103RF_RG) || defined(STM32F103VF_VG_ZF_ZG) || \
         defined(STM32F105XX) || defined(STM32F107XX)
        _OSIOHandler, /* 39 OS_IO_USART3 */
     #elif defined(DEBUG_MODE)
        UndefinedInterrupt,
     #else
        NULL,
     #endif
     _OSIOHandler,    /* 40 OS_IO_EXTI15_10 */
     _OSIOHandler,    /* 41 OS_IO_RTCAlarm */
     #if defined(STM32F103X4_X6) || defined(STM32F103T8_TB) || \
         defined(STM32F103C8_CB_R8_RB_V8_VB) || defined(STM32F103RC_RD_RE) || \
         defined(STM32F103VC_VD_VE_ZC_ZD_ZE) || defined(STM32F103RF_RG) || \
         defined(STM32F103VF_VG_ZF_ZG) || defined(STM32F102X4_X6) || defined(STM32F102X8_XB)
        _OSIOHandler, /* 42 S_IO_USBWakeUp */
     #elif defined(STM32F100X4_X6) || defined(STM32F100X8_XB) || \
           defined(STM32F100RC_RD_RE) || defined(STM32F100VC_VD_VE_ZC_ZD_ZE)
        _OSIOHandler, /* 42 OS_IO_CEC */
     #elif defined(STM32F105XX) || defined(STM32F107XX)
        _OSIOHandler, /* 42 OS_IO_OTG_FS_WKUP */
     #elif defined(DEBUG_MODE)
        UndefinedInterrupt,
     #else
        NULL,
     #endif
     #if defined(STM32F103RC_RD_RE) || defined(STM32F103VC_VD_VE_ZC_ZD_ZE)
        _OSIOHandler, /* 43 OS_IO_TIM8_BRK */
        _OSIOHandler, /* 44 OS_IO_TIM8_UP */
        _OSIOHandler, /* 45 OS_IO_TIM8_TRG_COM */
        _OSIOHandler, /* 46 OS_IO_TIM8_CC */
     #elif defined(STM32F101RF_RG) || defined(STM32F101VF_VG_ZF_ZG) || \
           defined(STM32F100RC_RD_RE) || defined(STM32F100VC_VD_VE_ZC_ZD_ZE )
        _OSIOHandler, /* 43 OS_IO_TIM12 */
        _OSIOHandler, /* 44 OS_IO_TIM13 */
        _OSIOHandler, /* 45 OS_IO_TIM14 */
        #if defined(DEBUG_MODE)
           UndefinedInterrupt,
        #else
           NULL,
        #endif
     #elif defined(STM32F103RF_RG) || defined(STM32F103VF_VG_ZF_ZG)
        _OSIOHandler, /* 43 OS_IO_TIM8_BRK_TIM12 */
        _OSIOHandler, /* 44 OS_IO_TIM8_UP_TIM13 */
        _OSIOHandler, /* 45 OS_IO_TIM8_TRG_COM_TIM14 */
        _OSIOHandler, /* 46 OS_IO_TIM8_CC */
     #elif defined(DEBUG_MODE)
        UndefinedInterrupt,
        UndefinedInterrupt,
        UndefinedInterrupt,
        UndefinedInterrupt,
     #else
        NULL,
        NULL,
        NULL,
        NULL,
     #endif
     #if defined(STM32F103RC_RD_RE) || defined(STM32F103VC_VD_VE_ZC_ZD_ZE) || \
         defined(STM32F103RF_RG) || defined(STM32F103VF_VG_ZF_ZG)
        _OSIOHandler, /* 47 OS_IO_ADC3 */
     #elif defined(DEBUG_MODE)
        UndefinedInterrupt,
     #else
        NULL,
     #endif
     #if defined(STM32F100VC_VD_VE_ZC_ZD_ZE) || defined(STM32F101VC_VD_VE_ZC_ZD_ZE) || \
         defined(STM32F101VF_VG_ZF_ZG) || defined(STM32F103VC_VD_VE_ZC_ZD_ZE) || \
         defined(STM32F103VF_VG_ZF_ZG)
        _OSIOHandler, /* 48 OS_IO_FSMC */
     #elif defined(DEBUG_MODE)
        UndefinedInterrupt,
     #else
        NULL,
     #endif
     #if defined(STM32F103RC_RD_RE) || defined(STM32F103VC_VD_VE_ZC_ZD_ZE) || \
         defined(STM32F103RF_RG) || defined(STM32F103VF_VG_ZF_ZG)
        _OSIOHandler, /* 49 OS_IO_SDIO */
     #elif defined(DEBUG_MODE)
        UndefinedInterrupt,
     #else
        NULL,
     #endif
     #if defined(STM32F100RC_RD_RE) || defined(STM32F100VC_VD_VE_ZC_ZD_ZE) || \
         defined(STM32F101RC_RD_RE) || defined(STM32F101VC_VD_VE_ZC_ZD_ZE) || \
         defined(STM32F101RF_RG) || defined(STM32F101VF_VG_ZF_ZG) || \
         defined(STM32F103RC_RD_RE) || defined(STM32F103VC_VD_VE_ZC_ZD_ZE) || \
         defined(STM32F103RF_RG) || defined(STM32F103VF_VG_ZF_ZG) || \
         defined(STM32F105XX) || defined(STM32F107XX)
        _OSIOHandler, /* 50 OS_IO_TIM5 */
        _OSIOHandler, /* 51 OS_IO_SPI3 */
     #elif defined(DEBUG_MODE)
        UndefinedInterrupt,
        UndefinedInterrupt,
     #else
        NULL,
        NULL,
     #endif
     #if defined(STM32F100RC_RD_RE) || defined(STM32F100VC_VD_VE_ZC_ZD_ZE) || \
         defined(STM32F101RC_RD_RE) || defined(STM32F101VC_VD_VE_ZC_ZD_ZE) || \
         defined(STM32F101RF_RG) || defined(STM32F101VF_VG_ZF_ZG) || \
         defined(STM32F103RC_RD_RE) || defined(STM32F103VC_VD_VE_ZC_ZD_ZE) || \
         defined(STM32F103RF_RG) || defined(STM32F103VF_VG_ZF_ZG) || \
         defined(STM32F105XX) || defined(STM32F107XX)
        _OSIOHandler, /* 52 OS_IO_UART4 */
        _OSIOHandler, /* 53 OS_IO_UART5 */
     #elif defined(DEBUG_MODE)
        UndefinedInterrupt,
        UndefinedInterrupt,
     #else
        NULL,
        NULL,
     #endif
     #if defined(STM32F100X4_X6) || defined(STM32F100X8_XB) || \
         defined(STM32F100RC_RD_RE) || defined(STM32F100VC_VD_VE_ZC_ZD_ZE)
        _OSIOHandler, /* 54 OS_IO_TIM6_DAC */
     #elif defined(STM32F101RC_RD_RE) || defined(STM32F101VC_VD_VE_ZC_ZD_ZE) || \
           defined(STM32F101RF_RG) || defined(STM32F101VF_VG_ZF_ZG) || \
           defined(STM32F103RC_RD_RE) || defined(STM32F103VC_VD_VE_ZC_ZD_ZE) || \
           defined(STM32F103RF_RG) || defined(STM32F103VF_VG_ZF_ZG) || \
           defined(STM32F105XX) || defined(STM32F107XX)
        _OSIOHandler, /* 54 OS_IO_TIM6 */
     #elif defined(DEBUG_MODE)
        UndefinedInterrupt,
     #else
        NULL,
     #endif
     #if defined(STM32F100X4_X6) || defined(STM32F100X8_XB) || \
         defined(STM32F100RC_RD_RE) || defined(STM32F100VC_VD_VE_ZC_ZD_ZE) || \
         defined(STM32F101RC_RD_RE) || defined(STM32F101VC_VD_VE_ZC_ZD_ZE) || \
         defined(STM32F101RF_RG) || defined(STM32F101VF_VG_ZF_ZG) || \
         defined(STM32F103RC_RD_RE) || defined(STM32F103VC_VD_VE_ZC_ZD_ZE) || \
         defined(STM32F103RF_RG) || defined(STM32F103VF_VG_ZF_ZG) || \
         defined(STM32F105XX) || defined(STM32F107XX)
        _OSIOHandler, /* 55 OS_IO_TIM7 */
     #elif defined(DEBUG_MODE)
        UndefinedInterrupt,
     #else
        NULL,
     #endif
     #if defined(STM32F100RC_RD_RE) || defined(STM32F100VC_VD_VE_ZC_ZD_ZE) || \
         defined(STM32F101RC_RD_RE) || defined(STM32F101VC_VD_VE_ZC_ZD_ZE) || \
         defined(STM32F101RF_RG) || defined(STM32F101VF_VG_ZF_ZG) || \
         defined(STM32F103RC_RD_RE) || defined(STM32F103VC_VD_VE_ZC_ZD_ZE) || \
         defined(STM32F103RF_RG) || defined(STM32F103VF_VG_ZF_ZG) || \
         defined(STM32F105XX) || defined(STM32F107XX)
        _OSIOHandler, /* 56 OS_IO_DMA2_Channel1 */
        _OSIOHandler, /* 57 OS_IO_DMA2_Channel2 */
        _OSIOHandler, /* 58 OS_IO_DMA2_Channel3 */
     #elif defined(DEBUG_MODE)
        UndefinedInterrupt,
        UndefinedInterrupt,
        UndefinedInterrupt,
     #else
        NULL,
        NULL,
        NULL,
     #endif
     #if defined(STM32F100RC_RD_RE) || defined(STM32F100VC_VD_VE_ZC_ZD_ZE) || \
         defined(STM32F101RC_RD_RE) || defined(STM32F101VC_VD_VE_ZC_ZD_ZE) || \
         defined(STM32F101RF_RG) || defined(STM32F101VF_VG_ZF_ZG) || \
         defined(STM32F103RC_RD_RE) || defined(STM32F103VC_VD_VE_ZC_ZD_ZE) || \
         defined(STM32F103RF_RG) || defined(STM32F103VF_VG_ZF_ZG)
        _OSIOHandler, /* 59 OS_IO_DMA2_Channel4_5 */
     #elif defined(STM32F105XX) || defined(STM32F107XX)
        _OSIOHandler, /* 59 OS_IO_DMA2_Channel4 */
     #elif defined(DEBUG_MODE)
        UndefinedInterrupt,
     #else
        NULL,
     #endif
     #if defined(STM32F100RC_RD_RE) || defined(STM32F100VC_VD_VE_ZC_ZD_ZE)
        _OSIOHandler, /* 60 OS_IO_DMA2_Channel5 */
     #elif defined(STM32F105XX) || defined(STM32F107XX)
        _OSIOHandler, /* 60 OS_IO_DMA2_Channel5 */
     #elif defined(DEBUG_MODE)
        UndefinedInterrupt,
     #else
        NULL,
     #endif
     #if defined(STM32F107XX)
        _OSIOHandler, /* 61 OS_IO_ETH */
        _OSIOHandler, /* 62 OS_IO_ETH_WKUP */
     #elif defined(DEBUG_MODE)
        UndefinedInterrupt,
        UndefinedInterrupt,
     #else
        NULL,
        NULL,
     #endif
     #if defined(STM32F105XX) || defined(STM32F107XX)
        _OSIOHandler, /* 63 OS_IO_CAN2_TX */
        _OSIOHandler, /* 64 OS_IO_CAN2_RX0 */
        _OSIOHandler, /* 65 OS_IO_CAN2_RX1 */
        _OSIOHandler, /* 66 OS_IO_CAN2_SCE */
        _OSIOHandler, /* 67 OS_IO_OTG_FS */
     #elif defined(DEBUG_MODE)
        UndefinedInterrupt,
        UndefinedInterrupt,
        UndefinedInterrupt,
        UndefinedInterrupt,
        UndefinedInterrupt,
     #else
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
     #endif
  /* ------------------------------ STM32F2 and STM32F4 ------------------------------ */
  #elif defined(STM32F2XXXX) || defined(STM32F4XXXX)
     _OSIOHandler,    /* 18 OS_IO_ADC  */
     _OSIOHandler,    /* 19 OS_IO_CAN1_TX */
     _OSIOHandler,    /* 20 OS_IO_CAN1_RX0 */
     _OSIOHandler,    /* 21 OS_IO_CAN1_RX1 */
     _OSIOHandler,    /* 22 OS_IO_CAN1_SCE */
     _OSIOHandler,    /* 23 OS_IO_EXTI9_5 */
     _OSIOHandler,    /* 24 OS_IO_TIM1_BRK_TIM9 */
     _OSIOHandler,    /* 25 OS_IO_TIM1_UP_TIM10 */
     _OSIOHandler,    /* 26 OS_IO_TIM1_TRG_COM_TIM11 */
     _OSIOHandler,    /* 27 OS_IO_TIM1_CC */
     _OSIOHandler,    /* 28 OS_IO_TIM2 */
     _OSIOHandler,    /* 29 OS_IO_TIM3 */
     _OSIOHandler,    /* 30 OS_IO_TIM4 */
     _OSIOHandler,    /* 31 OS_IO_I2C1_EV */
     _OSIOHandler,    /* 32 OS_IO_I2C1_ER */
     _OSIOHandler,    /* 33 OS_IO_I2C2_EV */
     _OSIOHandler,    /* 34 OS_IO_I2C2_ER */
     _OSIOHandler,    /* 35 OS_IO_SPI1 */
     _OSIOHandler,    /* 36 OS_IO_SPI2 */
     _OSIOHandler,    /* 37 OS_IO_USART1 */
     _OSIOHandler,    /* 38 OS_IO_USART2 */
     _OSIOHandler,    /* 39 OS_IO_USART3 */
     _OSIOHandler,    /* 40 OS_IO_EXTI15_10 */
     _OSIOHandler,    /* 41 OS_IO_RTC_Alarm */
     _OSIOHandler,    /* 42 OS_IO_OTG_FS_WKUP */
     _OSIOHandler,    /* 43 OS_IO_TIM8_BRK_TIM12 */
     _OSIOHandler,    /* 44 OS_IO_TIM8_UP_TIM13 */
     _OSIOHandler,    /* 45 OS_IO_TIM8_TRG_COM_TIM14 */
     _OSIOHandler,    /* 46 OS_IO_TIM8_CC */
     _OSIOHandler,    /* 47 OS_IO_DMA1_Stream7 */
     #if defined(STM32F205VX_ZX) || defined(STM32F215VX_ZX) || defined(STM32F207XX) || \
         defined(STM32F217XX) || defined(STM32F405VX_ZX) || defined(STM32F415VX_ZX) || \
         defined(STM32F407XX) || defined(STM32F417XX)
        _OSIOHandler, /* 48 OS_IO_FSMC */
     #elif defined(DEBUG_MODE)
        UndefinedInterrupt,
     #else
        NULL,
     #endif
     _OSIOHandler,    /* 49 OS_IO_SDIO */
     _OSIOHandler,    /* 50 OS_IO_TIM5 */
     _OSIOHandler,    /* 51 OS_IO_SPI3 */
     _OSIOHandler,    /* 52 OS_IO_UART4 */
     _OSIOHandler,    /* 53 OS_IO_UART5 */
     _OSIOHandler,    /* 54 OS_IO_TIM6_DAC */
     _OSIOHandler,    /* 55 OS_IO_TIM7 */
     _OSIOHandler,    /* 56 OS_IO_DMA2_Stream0 */
     _OSIOHandler,    /* 57 OS_IO_DMA2_Stream1 */
     _OSIOHandler,    /* 58 OS_IO_DMA2_Stream2 */
     _OSIOHandler,    /* 59 OS_IO_DMA2_Stream3 */
     _OSIOHandler,    /* 60 OS_IO_DMA2_Stream4 */
     #if defined(STM32F207XX) || defined(STM32F217XX) || defined(STM32F407XX) || \
         defined(STM32F417XX)
        _OSIOHandler, /* 61 OS_IO_ETH */
        _OSIOHandler, /* 62 OS_IO_ETH_WKUP */
     #elif defined(DEBUG_MODE)
        UndefinedInterrupt,
        UndefinedInterrupt,
     #else
        NULL,
        NULL,
     #endif
     _OSIOHandler,    /* 63 OS_IO_CAN2_TX */
     _OSIOHandler,    /* 64 OS_IO_CAN2_RX0 */
     _OSIOHandler,    /* 65 OS_IO_CAN2_RX1 */
     _OSIOHandler,    /* 66 OS_IO_CAN2_SCE */
     #if defined(STM32F207XX) || defined(STM32F217XX) || defined(STM32F407XX) || \
         defined(STM32F417XX)
        _OSIOHandler, /* 67 OS_IO_OTG_FS */
     #elif defined(DEBUG_MODE)
        UndefinedInterrupt,
     #else
        NULL,
     #endif
     _OSIOHandler,    /* 68 OS_IO_DMA2_Stream5 */
     _OSIOHandler,    /* 69 OS_IO_DMA2_Stream6 */
     _OSIOHandler,    /* 70 OS_IO_DMA2_Stream7 */
     _OSIOHandler,    /* 71 OS_IO_USART6 */
     _OSIOHandler,    /* 72 OS_IO_I2C3_EV */
     _OSIOHandler,    /* 73 OS_IO_I2C3_ER */
     _OSIOHandler,    /* 74 OS_IO_OTG_HS_EP1_OUT */
     _OSIOHandler,    /* 75 OS_IO_OTG_HS_EP1_IN */
     _OSIOHandler,    /* 76 OS_IO_OTG_HS_WKUP */
     _OSIOHandler,    /* 77 OS_IO_OTG_HS */
     #if defined(STM32F207XX) || defined(STM32F217XX) || defined(STM32F407XX) || \
         defined(STM32F417XX)
        _OSIOHandler, /* 78 OS_IO_DCMI */
     #elif defined(DEBUG_MODE)
        UndefinedInterrupt,
     #else
        NULL,
     #endif
     #if defined(STM32F215RX) || defined(STM32F215VX_ZX) || defined(STM32F217XX) || \
         defined(STM32F415RG) || defined(STM32F415VG_ZG) || defined(STM32F417XX)
        _OSIOHandler, /* 79 OS_IO_CRYP */
     #elif defined(DEBUG_MODE)
        UndefinedInterrupt,
     #else
        NULL,
     #endif
     _OSIOHandler,    /* 80 OS_IO_HASH_RNG */
     #if defined(STM32F4XXXX)
        _OSIOHandler, /* 81 OS_IO_FPU */
     #elif defined(DEBUG_MODE)
        UndefinedInterrupt,
     #else
        NULL,
     #endif
  #else
     #error STM32 version undefined
  #endif
};


/* TABLE OF ISR DESCRIPTORS CONTROLLABLE BY OSSetISRDescriptor AND OSGetISRDescriptor ***
** (2nd table) *************************************************************************/
/* Initially each entry is either NULL (no initialized ISR descriptor) or set to point to
** a descriptor with a function that can redirect the raised interrupt to the proper ISR
** when an IRQ refers to several different devices. (For the STM32, we have limited our-
** selves to multiple sources that have at least a timer that can be selected as the in-
** ternal timer for ZottaOS.) */

/* IRQ bound to 2 different timer devices */
#define TIMER_ISR_TAB_SIZE 2   // Number of timers bound to the same IRQ value (always 2)
typedef struct TIMERSELECT {
   void (*TimerSelector)(struct TIMERSELECT *);  // Selector function (_OSTimerSelectorHandler)
   void *TimerISRDescriptor[TIMER_ISR_TAB_SIZE]; // Specified ISR handlers for the sources
   UINT32 BaseRegister[TIMER_ISR_TAB_SIZE];      // Starting address of the timer registers
   UINT16 FirstEntryMask;                        // Interrupt enable bits of an interrupt
} TIMERSELECT;
static void _OSTimerSelectorHandler(struct TIMERSELECT *timerSelect);
/* How does this work?
** For example IRQ25 on STM32F205xx is simultaneously bound to Timer 1 Update and to Timer
** 10 interrupts (TIM1_UP_TIM10). When IRQ25 is raised, it is caused by Timer 1 if
**   (1) its update interrupt is enable (bit 0 of register TIM1_DIER)
**   (2) an update interrupt is pending (bit 0 of register TIM1_SR),
** i.e. TIM1_DIER & TIM1_SR & 0x1. The last term is mask bit. Hence, we store in the des-
** criptor used by _OSTimerSelectorHandler the base address of Timer 1 registers along
** with the mask, and if an update interrupt is detected, we simply call the dedicated
** ISR handler (first field of the first entry of TimerISRDescriptor). */

#if defined(STM32F050XX)
   void *_OSTabDevice[28] = {
      NULL,                /*  0  OS_IO_WWDG */
      NULL,                /*  1  OS_IO_PVD */
      NULL,                /*  2  OS_IO_RTC */
      NULL,                /*  3  OS_IO_FLASH */
      NULL,                /*  4  OS_IO_RCC */
      NULL,                /*  5  OS_IO_EXTI0_1 */
      NULL,                /*  6  OS_IO_EXTI2_3 */
      NULL,                /*  7  OS_IO_EXTI4_15 */
      NULL,                /*  8  OS_IO_TS */
      NULL,                /*  9  OS_IO_DMA1_Channel1 */
      NULL,                /* 10  OS_IO_DMA1_Channel2_3 */
      NULL,                /* 11  OS_IO_DMA1_Channel4_5 */
      NULL,                /* 12  OS_IO_ADC1_COMP */
      NULL,                /* 13  OS_IO_TIM1_BRK_UP_TRG_COM */
      NULL,                /* 14  OS_IO_TIM1_CC */
      NULL,                /* 15  OS_IO_TIM2 */
      NULL,                /* 16  OS_IO_TIM3 */
      NULL,
      NULL,
      NULL,                /* 19  OS_IO_TIM14 */
      NULL,
      NULL,                /* 21  OS_IO_TIM16 */
      NULL,                /* 22  OS_IO_TIM17 */
      NULL,                /* 23  OS_IO_I2C1 */
      NULL,
      NULL,                /* 25  OS_IO_SPI1 */
      NULL,
      NULL                 /* 27  OS_IO_USART1 */
   };
#elif defined(STM32F051X4) || defined(STM32F051K6_K8_C6_R6) || defined(STM32F051C8_R8)
   void *_OSTabDevice[31] = {
      NULL,                /*  0  OS_IO_WWDG */
      NULL,                /*  1  OS_IO_PVD */
      NULL,                /*  2  OS_IO_RTC */
      NULL,                /*  3  OS_IO_FLASH */
      NULL,                /*  4  OS_IO_RCC */
      NULL,                /*  5  OS_IO_EXTI0_1 */
      NULL,                /*  6  OS_IO_EXTI2_3 */
      NULL,                /*  7  OS_IO_EXTI4_15 */
      NULL,                /*  8  OS_IO_TS */
      NULL,                /*  9  OS_IO_DMA1_Channel1 */
      NULL,                /* 10  OS_IO_DMA1_Channel2_3 */
      NULL,                /* 11  OS_IO_DMA1_Channel4_5 */
      NULL,                /* 12  OS_IO_ADC1_COMP */
      NULL,                /* 13  OS_IO_TIM1_BRK_UP_TRG_COM */
      NULL,                /* 14  OS_IO_TIM1_CC */
      NULL,                /* 15  OS_IO_TIM2 */
      NULL,                /* 16  OS_IO_TIM3 */
      NULL,                /* 17  OS_IO_TIM6_DAC */
      NULL,
      NULL,                /* 19  OS_IO_TIM14 */
      NULL,                /* 20  OS_IO_TIM15 */
      NULL,                /* 21  OS_IO_TIM16 */
      NULL,                /* 22  OS_IO_TIM17 */
      NULL,                /* 23  OS_IO_I2C1 */
      #if defined(STM32F051C8_R8)
         NULL,             /* 24  OS_IO_I2C2 */
      #else
         NULL,
      #endif
      NULL,                /* 25  OS_IO_SPI1 */
      #if defined(STM32F051C8_R8)
         NULL,             /* 26  OS_IO_SPI2 */
      #else
         NULL,
      #endif
      NULL,                /* 27  OS_IO_USART1 */
      #if defined(STM32F051K6_K8_C6_R6) || defined(STM32F051C8_R8)
         NULL,             /* 28  OS_IO_USART2 */
      #else
         NULL,
      #endif
      NULL,
      NULL                 /* 30  OS_IO_CEC */
   };
#elif defined(STM32L151X6_X8_XB) || defined(STM32L152X6_X8_XB)
   void *_OSTabDevice[45] = {
      NULL,                /*  0  OS_IO_WWDG */
      NULL,                /*  1  OS_IO_PVD */
      NULL,                /*  2  OS_IO_TAMP_STAMP */
      NULL,                /*  3  OS_IO_RTC_WKUP */
      NULL,                /*  4  OS_IO_FLASH */
      NULL,                /*  5  OS_IO_RCC */
      NULL,                /*  6  OS_IO_EXTI0 */
      NULL,                /*  7  OS_IO_EXTI1 */
      NULL,                /*  8  OS_IO_EXTI2 */
      NULL,                /*  9  OS_IO_EXTI3 */
      NULL,                /* 10  OS_IO_EXTI4 */
      NULL,                /* 11  OS_IO_DMA1_Channel1 */
      NULL,                /* 12  OS_IO_DMA1_Channel2 */
      NULL,                /* 13  OS_IO_DMA1_Channel3 */
      NULL,                /* 14  OS_IO_DMA1_Channel4 */
      NULL,                /* 15  OS_IO_DMA1_Channel5 */
      NULL,                /* 16  OS_IO_DMA1_Channel6 */
      NULL,                /* 17  OS_IO_DMA1_Channel7 */
      NULL,                /* 18  OS_IO_ADC1 */
      NULL,                /* 19  OS_IO_USB_HP */
      NULL,                /* 20  OS_IO_USB_LP */
      NULL,                /* 21  OS_IO_DAC */
      NULL,                /* 22  OS_IO_COMP */
      NULL,                /* 23  OS_IO_EXTI9_5 */
      #if defined(STM32L152X6_X8_XB)
         NULL,             /* 24  OS_IO_LCD */
      #else
         NULL,
      #endif
      NULL,                /* 25  OS_IO_TIM9 */
      NULL,                /* 26  OS_IO_TIM10 */
      NULL,                /* 27  OS_IO_TIM11 */
      NULL,                /* 28  OS_IO_TIM2 */
      NULL,                /* 29  OS_IO_TIM3 */
      NULL,                /* 30  OS_IO_TIM4 */
      NULL,                /* 31  OS_IO_I2C1_EV */
      NULL,                /* 32  OS_IO_I2C1_ER */
      NULL,                /* 33  OS_IO_I2C2_EV */
      NULL,                /* 34  OS_IO_I2C2_ER */
      NULL,                /* 35  OS_IO_SPI1 */
      NULL,                /* 36  OS_IO_SPI2 */
      NULL,                /* 37  OS_IO_USART1 */
      NULL,                /* 38  OS_IO_USART2 */
      NULL,                /* 39  OS_IO_USART3 */
      NULL,                /* 40  OS_IO_EXTI15_10 */
      NULL,                /* 41  OS_IO_RTC_Alarm */
      NULL,                /* 42  OS_IO_USB_FS_WKUP */
      NULL,                /* 43  OS_IO_TIM6 */
      NULL                 /* 44  OS_IO_TIM7 */
   };
#elif defined(STM32L151XC) || defined(STM32L152XC) || defined(STM32L151XD) || \
      defined(STM32L152XD) || defined(STM32L162XD)
   void *_OSTabDevice[57] = {
      NULL,                /*  0  OS_IO_WWDG */
      NULL,                /*  1  OS_IO_PVD */
      NULL,                /*  2  OS_IO_TAMP_STAMP */
      NULL,                /*  3  OS_IO_RTC_WKUP */
      NULL,                /*  4  OS_IO_FLASH */
      NULL,                /*  5  OS_IO_RCC */
      NULL,                /*  6  OS_IO_EXTI0 */
      NULL,                /*  7  OS_IO_EXTI1 */
      NULL,                /*  8  OS_IO_EXTI2 */
      NULL,                /*  9  OS_IO_EXTI3 */
      NULL,                /* 10  OS_IO_EXTI4 */
      NULL,                /* 11  OS_IO_DMA1_Channel1 */
      NULL,                /* 12  OS_IO_DMA1_Channel2 */
      NULL,                /* 13  OS_IO_DMA1_Channel3 */
      NULL,                /* 14  OS_IO_DMA1_Channel4 */
      NULL,                /* 15  OS_IO_DMA1_Channel5 */
      NULL,                /* 16  OS_IO_DMA1_Channel6 */
      NULL,                /* 17  OS_IO_DMA1_Channel7 */
      NULL,                /* 18  OS_IO_ADC1 */
      NULL,                /* 19  OS_IO_USB_HP */
      NULL,                /* 20  OS_IO_USB_LP */
      NULL,                /* 21  OS_IO_DAC */
      NULL,                /* 22  OS_IO_COMP_CA */
      NULL,                /* 23  OS_IO_EXTI9_5 */
      #if defined(STM32L152XC) || defined(STM32L152XD) || defined(STM32L162XD)
         NULL,             /* 24  OS_IO_LCD */
      #else
         NULL,
      #endif
      NULL,                /* 25  OS_IO_TIM9 */
      NULL,                /* 26  OS_IO_TIM10 */
      NULL,                /* 27  OS_IO_TIM11 */
      NULL,                /* 28  OS_IO_TIM2 */
      NULL,                /* 29  OS_IO_TIM3 */
      NULL,                /* 30  OS_IO_TIM4 */
      NULL,                /* 31  OS_IO_I2C1_EV */
      NULL,                /* 32  OS_IO_I2C1_ER */
      NULL,                /* 33  OS_IO_I2C2_EV */
      NULL,                /* 34  OS_IO_I2C2_ER */
      NULL,                /* 35  OS_IO_SPI1 */
      NULL,                /* 36  OS_IO_SPI2 */
      NULL,                /* 37  OS_IO_USART1 */
      NULL,                /* 38  OS_IO_USART2 */
      NULL,                /* 39  OS_IO_USART3 */
      NULL,                /* 40  OS_IO_EXTI15_10 */
      NULL,                /* 41  OS_IO_RTC_Alarm */
      NULL,                /* 42  OS_IO_USB_FS_WKUP */
      NULL,                /* 43  OS_IO_TIM6 */
      NULL,                /* 44  OS_IO_TIM7 */
      #if defined(STM32L151XD) || defined(STM32L152XD) || defined(STM32L162XD)
         NULL,             /* 45  OS_IO_SDIO */
      #else
         NULL,
      #endif
      NULL,                /* 46  OS_IO_TIM5 */
      NULL,                /* 47  OS_IO_SPI3 */
      #if defined(STM32L151XD) || defined(STM32L152XD) || defined(STM32L162XD)
         NULL,             /* 48  OS_IO_UART4 */
         NULL,             /* 49  OS_IO_UART5 */
      #else
         NULL,
         NULL,
      #endif
      NULL,                /* 50  OS_IO_DMA2_Channel1*/
      NULL,                /* 51  OS_IO_DMA2_Channel2 */
      NULL,                /* 52  OS_IO_DMA2_Channel3 */
      NULL,                /* 53  OS_IO_DMA2_Channel4 */
      NULL,                /* 54  OS_IO_DMA2_Channel5 */
      #if defined(STM32L162XD)
         NULL,             /* 55  OS_IO_AES */
      #else
         NULL,
      #endif
      NULL                 /* 56  OS_IO_COMP_ACQ */
   };
#elif defined(STM32F101T8_TB) || defined(STM32F101C8_CB_R8_RB_V8_VB) || \
      defined(STM32F101X4_X6)
   void *_OSTabDevice[42] = {
      NULL,                /*  0  OS_IO_WWDG */
      NULL,                /*  1  OS_IO_PVD */
      NULL,                /*  2  OS_IO_TAMPER */
      NULL,                /*  3  OS_IO_RTC */
      NULL,                /*  4  OS_IO_FLASH */
      NULL,                /*  5  OS_IO_RCC */
      NULL,                /*  6  OS_IO_EXTI0 */
      NULL,                /*  7  OS_IO_EXTI1 */
      NULL,                /*  8  OS_IO_EXTI2 */
      NULL,                /*  9  OS_IO_EXTI3 */
      NULL,                /* 10  OS_IO_EXTI4 */
      NULL,                /* 11  OS_IO_DMA1_Channel1 */
      NULL,                /* 12  OS_IO_DMA1_Channel2 */
      NULL,                /* 13  OS_IO_DMA1_Channel3 */
      NULL,                /* 14  OS_IO_DMA1_Channel4 */
      NULL,                /* 15  OS_IO_DMA1_Channel5 */
      NULL,                /* 16  OS_IO_DMA1_Channel6 */
      NULL,                /* 17  OS_IO_DMA1_Channel7 */
      NULL,                /* 18  OS_IO_ADC1 */
      NULL,
      NULL,
      NULL,
      NULL,
      NULL,                /* 23  OS_IO_EXTI9_5 */
      NULL,
      NULL,
      NULL,
      NULL,
      NULL,                /* 28  OS_IO_TIM2 */
      NULL,                /* 29  OS_IO_TIM3 */
      #if defined(STM32F101T8_TB) || defined(STM32F101C8_CB_R8_RB_V8_VB)
         NULL,             /* 30  OS_IO_TIM4 */
      #else
         NULL,
      #endif
      NULL,                /* 31  OS_IO_I2C1_EV */
      NULL,                /* 32  OS_IO_I2C1_ER */
      #if defined(STM32F101C8_CB_R8_RB_V8_VB)
         NULL,             /* 33  OS_IO_I2C2_EV */
         NULL,             /* 34  OS_IO_I2C2_ER */
      #else
         NULL,
         NULL,
      #endif
      NULL,                /* 35  OS_IO_SPI1 */
      #if defined(STM32F101C8_CB_R8_RB_V8_VB)
         NULL,             /* 36  OS_IO_SPI2 */
      #else
         NULL,
      #endif
      NULL,                /* 37  OS_IO_USART1 */
      NULL,                /* 38  OS_IO_USART2 */
      #if defined(STM32F101C8_CB_R8_RB_V8_VB)
         NULL,             /* 39  OS_IO_USART3 */
      #else
         NULL,
      #endif
      NULL,                /* 40  OS_IO_EXTI15_10 */
      NULL                 /* 41  OS_IO_RTCAlarm */
   };
#elif defined(STM32F103X4_X6) || defined(STM32F103T8_TB) || \
      defined(STM32F103C8_CB_R8_RB_V8_VB) || defined(STM32F102X4_X6) || \
      defined(STM32F102X8_XB)
   void *_OSTabDevice[43] = {
      NULL,                /*  0  OS_IO_WWDG */
      NULL,                /*  1  OS_IO_PVD */
      NULL,                /*  2  OS_IO_TAMPER */
      NULL,                /*  3  OS_IO_RTC */
      NULL,                /*  4  OS_IO_FLASH */
      NULL,                /*  5  OS_IO_RCC */
      NULL,                /*  6  OS_IO_EXTI0 */
      NULL,                /*  7  OS_IO_EXTI1 */
      NULL,                /*  8  OS_IO_EXTI2 */
      NULL,                /*  9  OS_IO_EXTI3 */
      NULL,                /* 10  OS_IO_EXTI4 */
      NULL,                /* 11  OS_IO_DMA1_Channel1 */
      NULL,                /* 12  OS_IO_DMA1_Channel2 */
      NULL,                /* 13  OS_IO_DMA1_Channel3 */
      NULL,                /* 14  OS_IO_DMA1_Channel4 */
      NULL,                /* 15  OS_IO_DMA1_Channel5 */
      NULL,                /* 16  OS_IO_DMA1_Channel6 */
      NULL,                /* 17  OS_IO_DMA1_Channel7 */
      NULL,                /* 18  OS_IO_ADC1 */
      NULL,
      #if defined(STM32F103X4_X6) || defined(STM32F103T8_TB) || \
          defined(STM32F103C8_CB_R8_RB_V8_VB)
         NULL,             /* 19  OS_IO_USB_HP_CAN1_TX */
      #elif defined(STM32F102X4_X6) || defined(STM32F102X8_XB)
         NULL,             /* 19  OS_IO_USB_HP */
      #else
         NULL,
      #if defined(STM32F103X4_X6) || defined(STM32F103T8_TB) || \
          defined(STM32F103C8_CB_R8_RB_V8_VB)
         NULL,             /* 20  OS_IO_USB_LP_CAN1_RX0 */
      #elif defined(STM32F102X4_X6) || defined(STM32F102X8_XB)
         NULL,             /* 20  OS_IO_USB_LP */
      #else
         NULL,
      #endif
      #if defined(STM32F103X4_X6) || defined(STM32F103T8_TB) || \
          defined(STM32F103C8_CB_R8_RB_V8_VB)
         NULL,             /* 21  OS_IO_CAN1_RX1 */
      #else
         NULL,
      #endif
      #if defined(STM32F103X4_X6) || defined(STM32F103T8_TB) || \
          defined(STM32F103C8_CB_R8_RB_V8_VB)
         NULL,             /* 22  OS_IO_CAN1_SCE */
      #else
         NULL,
      #endif
      NULL,                /* 23  OS_IO_EXTI9_5 */
      #if defined(STM32F103X4_X6) || defined(STM32F103T8_TB) || \
          defined(STM32F103C8_CB_R8_RB_V8_VB)
         NULL,             /* 24  OS_IO_TIM1_BRK */
         NULL,             /* 25  OS_IO_TIM1_UP */
         NULL,             /* 26  OS_IO_TIM1_TRG_COM */
         NULL,             /* 27  OS_IO_TIM1_CC */
      #else
         NULL,
         NULL,
         NULL,
         NULL,
      #endif
      NULL,                /* 28  OS_IO_TIM2 */
      NULL,                /* 29  OS_IO_TIM3 */
      #if defined(STM32F102X8_XB) || defined(STM32F103T8_TB) || \
          defined(STM32F103C8_CB_R8_RB_V8_VB)
         NULL,             /* 30  OS_IO_TIM4 */
      #else
         NULL,
      #endif
      NULL,                /* 31  OS_IO_I2C1_EV */
      NULL,                /* 32  OS_IO_I2C1_ER */
      #if defined(STM32F102X8_XB) || defined(STM32F103C8_CB_R8_RB_V8_VB)
         NULL,             /* 33  OS_IO_I2C2_EV */
         NULL,             /* 34  OS_IO_I2C2_ER */
      #else
         NULL,
         NULL,
      #endif
      NULL,                /* 35  OS_IO_SPI1 */
      #if defined(STM32F102X8_XB) || defined(STM32F103C8_CB_R8_RB_V8_VB)
         NULL,             /* 36  OS_IO_SPI2 */
      #else
        NULL,
      #endif
      NULL,                /* 37  OS_IO_USART1 */
      NULL,                /* 38  OS_IO_USART2 */
      #if defined(STM32F102X8_XB) || defined(STM32F103C8_CB_R8_RB_V8_VB)
         NULL,             /* 39  OS_IO_USART3 */
      #else
         NULL,
      #endif
      NULL,                /* 40  OS_IO_EXTI15_10 */
      NULL,                /* 41  OS_IO_RTCAlarm */
      NULL                 /* 42  OS_IO_USBWakeUp */
   };
#elif defined(STM32F100X4_X6) || defined(STM32F100X8_XB)
   static TIMERSELECT Timer15ISRSelect = {_OSTimerSelectorHandler,{NULL,NULL},{0x40012C00,0x40014000},0x80};
   static TIMERSELECT Timer16ISRSelect = {_OSTimerSelectorHandler,{NULL,NULL},{0x40012C00,0x40014400},0x01};
   static TIMERSELECT Timer17ISRSelect = {_OSTimerSelectorHandler,{NULL,NULL},{0x40012C00,0x40014800},0x40};
   void *_OSTabDevice[56] = {
      NULL,                /*  0  OS_IO_WWDG */
      NULL,                /*  1  OS_IO_PVD */
      NULL,                /*  2  OS_IO_TAMPER */
      NULL,                /*  3  OS_IO_RTC */
      NULL,                /*  4  OS_IO_FLASH */
      NULL,                /*  5  OS_IO_RCC */
      NULL,                /*  6  OS_IO_EXTI0 */
      NULL,                /*  7  OS_IO_EXTI1 */
      NULL,                /*  8  OS_IO_EXTI2 */
      NULL,                /*  9  OS_IO_EXTI3 */
      NULL,                /* 10  OS_IO_EXTI4 */
      NULL,                /* 11  OS_IO_DMA1_Channel1 */
      NULL,                /* 12  OS_IO_DMA1_Channel2 */
      NULL,                /* 13  OS_IO_DMA1_Channel3 */
      NULL,                /* 14  OS_IO_DMA1_Channel4 */
      NULL,                /* 15  OS_IO_DMA1_Channel5 */
      NULL,                /* 16  OS_IO_DMA1_Channel6 */
      NULL,                /* 17  OS_IO_DMA1_Channel7 */
      NULL,                /* 18  OS_IO_ADC1 */
      NULL,
      NULL,
      NULL,
      NULL,
      NULL,                /* 23  OS_IO_EXTI9_5 */
      Timer15ISRSelect,    /* 24  OS_IO_TIM1_BRK_TIM15 (Timer1-Break & Timer15) */
      Timer16ISRSelect,    /* 25  OS_IO_TIM1_UP_TIM16 (Timer1-Update & Timer16) */
      Timer17ISRSelect,    /* 26  OS_IO_TIM1_TRG_COM_TIM17 (Timer1-Trigger-Commutation & Timer17 */
      NULL,                /* 27  OS_IO_TIM1_CC */
      NULL,                /* 28  OS_IO_TIM2 */
      NULL,                /* 29  OS_IO_TIM3 */
      #if defined (STM32F100X8_XB)
         NULL,             /* 30  OS_IO_TIM4 */
      #else
         NULL,
      #endif
      NULL,                /* 31  OS_IO_I2C1_EV */
      NULL,                /* 32  OS_IO_I2C1_ER */
      #if defined(STM32F100X8_XB)
         NULL,             /* 33  OS_IO_I2C2_EV */
         NULL,             /* 34  OS_IO_I2C2_ER */
      #else
         NULL,
         NULL,
      #endif
      NULL,                /* 35  OS_IO_SPI1 */
      #if defined(STM32F100X8_XB)
         NULL,             /* 36  OS_IO_SPI2 */
      #else
         NULL,
      #endif
      NULL,                /* 37  OS_IO_USART1 */
      NULL,                /* 38  OS_IO_USART2 */
      #if defined(STM32F100X8_XB)
         NULL,             /* 39  OS_IO_USART3 */
      #else
          NULL,
      #endif
      NULL,                /* 40  OS_IO_EXTI15_10 */
      NULL,                /* 41  OS_IO_RTCAlarm */
      NULL,                /* 42  OS_IO_CEC */
      NULL,
      NULL,
      NULL,
      NULL,
      NULL,
      NULL,
      NULL,
      NULL,
      NULL,
      NULL,
      NULL,
      NULL,                /* 54  OS_IO_TIM6_DAC */
      NULL                 /* 55  OS_IO_TIM7 */
   };
#elif defined(STM32F101RC_RD_RE) || defined(STM32F101VC_VD_VE_ZC_ZD_ZE) || \
      defined(STM32F101RF_RG) || defined(STM32F101VF_VG_ZF_ZG) || \
      defined(STM32F103RC_RD_RE) || defined(STM32F103VC_VD_VE_ZC_ZD_ZE) || \
      defined(STM32F103RF_RG) || defined(STM32F103VF_VG_ZF_ZG)
   #if defined(STM32F103RF_RG) || defined(STM32F103VF_VG_ZF_ZG)
      static TIMERSELECT Timer9ISRSelect  = {_OSTimerSelectorHandler,{NULL,NULL},{0x40012C00,0x40014C00},0x80};
      static TIMERSELECT Timer10ISRSelect = {_OSTimerSelectorHandler,{NULL,NULL},{0x40012C00,0x40015000},0x01};
      static TIMERSELECT Timer11ISRSelect = {_OSTimerSelectorHandler,{NULL,NULL},{0x40012C00,0x40015400},0x40};
      static TIMERSELECT Timer12ISRSelect = {_OSTimerSelectorHandler,{NULL,NULL},{0x40013400,0x40001800},0x80};
      static TIMERSELECT Timer13ISRSelect = {_OSTimerSelectorHandler,{NULL,NULL},{0x40013400,0x40001C00},0x01};
      static TIMERSELECT Timer14ISRSelect = {_OSTimerSelectorHandler,{NULL,NULL},{0x40013400,0x40002000},0x40};
   #endif
   void *_OSTabDevice[60] = {
      NULL,                /*  0  OS_IO_WWDG */
      NULL,                /*  1  OS_IO_PVD */
      NULL,                /*  2  OS_IO_TAMPER */
      NULL,                /*  3  OS_IO_RTC */
      NULL,                /*  4  OS_IO_FLASH */
      NULL,                /*  5  OS_IO_RCC */
      NULL,                /*  6  OS_IO_EXTI0 */
      NULL,                /*  7  OS_IO_EXTI1 */
      NULL,                /*  8  OS_IO_EXTI2 */
      NULL,                /*  9  OS_IO_EXTI3 */
      NULL,                /* 10  OS_IO_EXTI4 */
      NULL,                /* 11  OS_IO_DMA1_Channel1 */
      NULL,                /* 12  OS_IO_DMA1_Channel2 */
      NULL,                /* 13  OS_IO_DMA1_Channel3 */
      NULL,                /* 14  OS_IO_DMA1_Channel4 */
      NULL,                /* 15  OS_IO_DMA1_Channel5 */
      NULL,                /* 16  OS_IO_DMA1_Channel6 */
      NULL,                /* 17  OS_IO_DMA1_Channel7 */
      NULL,                /* 18  OS_IO_ADC1 */
      NULL,                /* 19  OS_IO_USB_HP_CAN1_TX */
      NULL,                /* 20  OS_IO_USB_LP_CAN1_RX0 */
      NULL,                /* 21  OS_IO_CAN1_RX1 */
      NULL,                /* 22  OS_IO_CAN1_SCE */
      NULL,                /* 23  OS_IO_EXTI9_5 */
      #if defined(STM32F103RC_RD_RE) || defined(STM32F103VC_VD_VE_ZC_ZD_ZE)
         NULL,             /* 24  OS_IO_TIM1_BRK */
         NULL,             /* 25  OS_IO_TIM1_UP */
         NULL,             /* 26  OS_IO_TIM1_TRG_COM */
         NULL,             /* 27  OS_IO_TIM1_CC */
      #elif defined (STM32F101RF_RG)|| defined(STM32F101VF_VG_ZF_ZG)
         NULL,             /* 24  OS_IO_TIM9 */
         NULL,             /* 25  OS_IO_TIM10 */
         NULL,             /* 26  OS_IO_TIM11 */
         NULL,
      #elif defined(STM32F103RF_RG) || defined(STM32F103VF_VG_ZF_ZG)
         Timer9ISRSelect,  /* OS_IO_TIM1_BRK_TIM9 (Timer1-Break & Timer9) */
         Timer10ISRSelect, /* OS_IO_TIM1_UP_TIM10 (Timer1-Update & Timer10) */
         Timer11ISRSelect, /* OS_IO_TIM1_TRG_COM_TIM11 (Timer1-Trigger-Commutation, & Timer11 */
         NULL,             /* 27  OS_IO_TIM1_CC */
      #else
         NULL,
         NULL,
         NULL,
         NULL,
      #endif
      NULL,                /* 28  OS_IO_TIM2 */
      NULL,                /* 29  OS_IO_TIM3 */
      NULL,                /* 30  OS_IO_TIM4 */
      NULL,                /* 31  OS_IO_I2C1_EV */
      NULL,                /* 32  OS_IO_I2C1_ER */
      NULL,                /* 33  OS_IO_I2C2_EV */
      NULL,                /* 34  OS_IO_I2C2_ER */
      NULL,                /* 35  OS_IO_SPI1 */
      NULL,                /* 36  OS_IO_SPI2 */
      NULL,                /* 37  OS_IO_USART1 */
      NULL,                /* 38  OS_IO_USART2 */
      NULL,                /* 39  OS_IO_USART3 */
      NULL,                /* 40  OS_IO_EXTI15_10 */
      NULL,                /* 41  OS_IO_RTCAlarm */
      NULL,                /* 42  OS_IO_USBWakeUp */
      #if defined(STM32F103RC_RD_RE) || defined(STM32F103VC_VD_VE_ZC_ZD_ZE)
         NULL,             /* 43  OS_IO_TIM8_BRK */
         NULL,             /* 44  OS_IO_TIM8_UP */
         NULL,             /* 45  OS_IO_TIM8_TRG_COM */
         NULL,             /* 46  OS_IO_TIM8_CC */
      #elif defined(STM32F101RF_RG) || defined(STM32F101VF_VG_ZF_ZG)
         NULL,             /* 43  OS_IO_TIM12 */
         NULL,             /* 44  OS_IO_TIM13 */
         NULL,             /* 45  OS_IO_TIM14 */
         NULL,
      #elif defined(STM32F103RF_RG) || defined(STM32F103VF_VG_ZF_ZG)
         Timer12ISRSelect, /* 43  OS_IO_TIM8_BRK_TIM12 (Timer8-Break & Timer12) */
         Timer13ISRSelect, /* 44  OS_IO_TIM8_UP_TIM13 (Timer8-Update & Timer13) */
         Timer14ISRSelect, /* 45  OS_IO_TIM8_TRG_COM_TIM14 (Timer8-Trigger-Commutation & Timer14) */
         NULL,             /* 46  OS_IO_TIM8_CC */
      #else
         NULL,
         NULL,
         NULL,
         NULL,
      #endif
      NULL,                /* 47  OS_IO_ADC3 */
      NULL,                /* 48  OS_IO_FSMC */
      NULL,                /* 49  OS_IO_SDIO */
      NULL,                /* 50  OS_IO_TIM5 */
      NULL,                /* 51  OS_IO_SPI3 */
      NULL,                /* 52  OS_IO_UART4 */
      NULL,                /* 53  OS_IO_UART5 */
      NULL,                /* 54  OS_IO_TIM6 */
      NULL,                /* 55  OS_IO_TIM7 */
      NULL,                /* 56  OS_IO_DMA2_Channel1 */
      NULL,                /* 57  OS_IO_DMA2_Channel2 */
      NULL,                /* 58  OS_IO_DMA2_Channel3 */
      NULL                 /* 59  OS_IO_DMA2_Channel4_5 */
   };
#elif defined(STM32F100RC_RD_RE) || defined(STM32F100VC_VD_VE_ZC_ZD_ZE)
   static TIMERSELECT Timer15ISRSelect = {_OSTimerSelectorHandler,{NULL,NULL},{0x40012C00,0x40014000},0x80};
   static TIMERSELECT Timer16ISRSelect = {_OSTimerSelectorHandler,{NULL,NULL},{0x40012C00,0x40014400},0x01};
   static TIMERSELECT Timer17ISRSelect = {_OSTimerSelectorHandler,{NULL,NULL},{0x40012C00,0x40014800},0x40};
   void *_OSTabDevice[61] = {
      NULL,                /*  0  OS_IO_WWDG */
      NULL,                /*  1  OS_IO_PVD */
      NULL,                /*  2  OS_IO_TAMPER */
      NULL,                /*  3  OS_IO_RTC */
      NULL,                /*  4  OS_IO_FLASH */
      NULL,                /*  5  OS_IO_RCC */
      NULL,                /*  6  OS_IO_EXTI0 */
      NULL,                /*  7  OS_IO_EXTI1 */
      NULL,                /*  8  OS_IO_EXTI2 */
      NULL,                /*  9  OS_IO_EXTI3 */
      NULL,                /* 10  OS_IO_EXTI4 */
      NULL,                /* 11  OS_IO_DMA1_Channel1 */
      NULL,                /* 12  OS_IO_DMA1_Channel2 */
      NULL,                /* 13  OS_IO_DMA1_Channel3 */
      NULL,                /* 14  OS_IO_DMA1_Channel4 */
      NULL,                /* 15  OS_IO_DMA1_Channel5 */
      NULL,                /* 16  OS_IO_DMA1_Channel6 */
      NULL,                /* 17  OS_IO_DMA1_Channel7 */
      NULL,                /* 18  OS_IO_ADC1 */
      NULL,
      NULL,
      NULL,
      NULL,
      NULL,                /* 23  OS_IO_EXTI9_5 */
      Timer15ISRSelect,    /* 24  OS_IO_TIM1_BRK_TIM15 (Timer1-Break & Timer15) */
      Timer16ISRSelect,    /* 25  OS_IO_TIM1_UP_TIM16 (Timer1-Update & Timer16) */
      Timer17ISRSelect,    /* 26  OS_IO_TIM1_TRG_COM_TIM17 (Timer1-Trigger-Commutation & Timer17) */
      NULL,                /* 27  OS_IO_TIM1_CC */
      NULL,                /* 28  OS_IO_TIM2 */
      NULL,                /* 29  OS_IO_TIM3 */
      NULL,                /* 30  OS_IO_TIM4 */
      NULL,                /* 31  OS_IO_I2C1_EV */
      NULL,                /* 32  OS_IO_I2C1_ER */
      NULL,                /* 33  OS_IO_I2C2_EV */
      NULL,                /* 34  OS_IO_I2C2_ER */
      NULL,                /* 35  OS_IO_SPI1 */
      NULL,                /* 36  OS_IO_SPI2 */
      NULL,                /* 37  OS_IO_USART1 */
      NULL,                /* 38  OS_IO_USART2 */
      NULL,                /* 39  OS_IO_USART3 */
      NULL,                /* 40  OS_IO_EXTI15_10 */
      NULL,                /* 41  OS_IO_RTCAlarm */
      NULL,                /* 42  OS_IO_CEC */
      NULL,                /* 43  OS_IO_TIM12 */
      NULL,                /* 44  OS_IO_TIM13 */
      NULL,                /* 45  OS_IO_TIM14 */
      NULL,
      NULL,
      NULL,
      NULL,
      NULL,                /* 50  OS_IO_TIM5 */
      NULL,                /* 51  OS_IO_SPI3 */
      NULL,                /* 52  OS_IO_UART4 */
      NULL,                /* 53  OS_IO_UART5 */
      NULL,                /* 54  OS_IO_TIM6_DAC */
      NULL,                /* 55  OS_IO_TIM7 */
      NULL,                /* 56  OS_IO_DMA2_Channel1 */
      NULL,                /* 57  OS_IO_DMA2_Channel2 */
      NULL,                /* 58  OS_IO_DMA2_Channel3 */
      NULL,                /* 59  OS_IO_DMA2_Channel4_5 */
      NULL                 /* 60  OS_IO_DMA2_Channel5 */
   };
#elif defined(STM32F105XX) || defined(STM32F107XX)
   void *_OSTabDevice[68] = {
      NULL,                /*  0  OS_IO_WWDG */
      NULL,                /*  1  OS_IO_PVD */
      NULL,                /*  2  OS_IO_TAMPER */
      NULL,                /*  3  OS_IO_RTC */
      NULL,                /*  4  OS_IO_FLASH */
      NULL,                /*  5  OS_IO_RCC */
      NULL,                /*  6  OS_IO_EXTI0 */
      NULL,                /*  7  OS_IO_EXTI1 */
      NULL,                /*  8  OS_IO_EXTI2 */
      NULL,                /*  9  OS_IO_EXTI3 */
      NULL,                /* 10  OS_IO_EXTI4 */
      NULL,                /* 11  OS_IO_DMA1_Channel1 */
      NULL,                /* 12  OS_IO_DMA1_Channel2 */
      NULL,                /* 13  OS_IO_DMA1_Channel3 */
      NULL,                /* 14  OS_IO_DMA1_Channel4 */
      NULL,                /* 15  OS_IO_DMA1_Channel5 */
      NULL,                /* 16  OS_IO_DMA1_Channel6 */
      NULL,                /* 17  OS_IO_DMA1_Channel7 */
      NULL,                /* 18  OS_IO_ADC1_2 */
      NULL,                /* 19  OS_IO_CAN1_TX */
      NULL,                /* 20  OS_IO_CAN1_RX0 */
      NULL,                /* 21  OS_IO_CAN1_RX1 */
      NULL,                /* 22  OS_IO_CAN1_SCE */
      NULL,                /* 23  OS_IO_EXTI9_5 */
      NULL,                /* 24  OS_IO_TIM1_BRK */
      NULL,                /* 25  OS_IO_TIM1_UP */
      NULL,                /* 26  OS_IO_TIM1_TRG_COM */
      NULL,                /* 27  OS_IO_TIM1_CC */
      NULL,                /* 28  OS_IO_TIM2 */
      NULL,                /* 29  OS_IO_TIM3 */
      NULL,                /* 30  OS_IO_TIM4 */
      NULL,                /* 31  OS_IO_I2C1_EV */
      NULL,                /* 32  OS_IO_I2C1_ER */
      NULL,                /* 33  OS_IO_I2C2_EV */
      NULL,                /* 34  OS_IO_I2C2_ER */
      NULL,                /* 35  OS_IO_SPI1 */
      NULL,                /* 36  OS_IO_SPI2 */
      NULL,                /* 37  OS_IO_USART1 */
      NULL,                /* 38  OS_IO_USART2 */
      NULL,                /* 39  OS_IO_USART3 */
      NULL,                /* 40  OS_IO_EXTI15_10 */
      NULL,                /* 41  OS_IO_RTCAlarm */
      NULL,                /* 42  OS_IO_OTG_FS_WKUP */
      NULL,
      NULL,
      NULL,
      NULL,
      NULL,
      NULL,
      NULL,
      NULL,                /* 50  OS_IO_TIM5 */
      NULL,                /* 51  OS_IO_SPI3 */
      NULL,                /* 52  OS_IO_UART4 */
      NULL,                /* 53  OS_IO_UART5 */
      NULL,                /* 54  OS_IO_TIM6 */
      NULL,                /* 55  OS_IO_TIM7 */
      NULL,                /* 56  OS_IO_DMA2_Channel1 */
      NULL,                /* 57  OS_IO_DMA2_Channel2 */
      NULL,                /* 58  OS_IO_DMA2_Channel3 */
      NULL,                /* 59  OS_IO_DMA2_Channel4 */
      NULL,                /* 60  OS_IO_DMA2_Channel5 */
      NULL,                /* 61  OS_IO_ETH */
      NULL,                /* 62  OS_IO_ETH_WKUP */
      NULL,                /* 63  OS_IO_CAN2_TX */
      NULL,                /* 64  OS_IO_CAN2_RX0 */
      NULL,                /* 65  OS_IO_CAN2_RX1 */
      NULL,                /* 66  OS_IO_CAN2_SCE */
      NULL                 /* 67  OS_IO_OTG_FS */
   };
#elif defined(STM32F2XXXX)
   static TIMERSELECT Timer9ISRSelect  = {_OSTimerSelectorHandler,{NULL,NULL},{0x40010000,0x40014000},0x80};
   static TIMERSELECT Timer10ISRSelect = {_OSTimerSelectorHandler,{NULL,NULL},{0x40010000,0x40014400},0x01};
   static TIMERSELECT Timer11ISRSelect = {_OSTimerSelectorHandler,{NULL,NULL},{0x40010000,0x40014800},0x40};
   static TIMERSELECT Timer12ISRSelect = {_OSTimerSelectorHandler,{NULL,NULL},{0x40010400,0x40001800},0x80};
   static TIMERSELECT Timer13ISRSelect = {_OSTimerSelectorHandler,{NULL,NULL},{0x40010400,0x40001C00},0x01};
   static TIMERSELECT Timer14ISRSelect = {_OSTimerSelectorHandler,{NULL,NULL},{0x40010400,0x40002000},0x40};
   void *_OSTabDevice[81] = {
      NULL,                /*  0  OS_IO_WWDG */
      NULL,                /*  1  OS_IO_PVD */
      NULL,                /*  2  OS_IO_TAMP_STAMP */
      NULL,                /*  3  OS_IO_RTC_WKUP */
      NULL,                /*  4  OS_IO_FLASH */
      NULL,                /*  5  OS_IO_RCC */
      NULL,                /*  6  OS_IO_EXTI0 */
      NULL,                /*  7  OS_IO_EXTI1 */
      NULL,                /*  8  OS_IO_EXTI2 */
      NULL,                /*  9  OS_IO_EXTI3 */
      NULL,                /* 10  OS_IO_EXTI4 */
      NULL,                /* 11  OS_IO_DMA1_Stream0 */
      NULL,                /* 12  OS_IO_DMA1_Stream1 */
      NULL,                /* 13  OS_IO_DMA1_Stream2 */
      NULL,                /* 14  OS_IO_DMA1_Stream3 */
      NULL,                /* 15  OS_IO_DMA1_Stream4 */
      NULL,                /* 16  OS_IO_DMA1_Stream5 */
      NULL,                /* 17  OS_IO_DMA1_Stream6 */
      NULL,                /* 18  OS_IO_ADC */
      NULL,                /* 19  OS_IO_CAN1_TX */
      NULL,                /* 20  OS_IO_CAN1_RX0 */
      NULL,                /* 21  OS_IO_CAN1_RX1 */
      NULL,                /* 22  OS_IO_CAN1_SCE */
      NULL,                /* 23  OS_IO_EXTI9_5 */
      &Timer9ISRSelect,    /* 24  OS_IO_TIM1_BRK_TIM9 (Timer1-Break & Timer9) */
      &Timer10ISRSelect,   /* 25  OS_IO_TIM1_UP_TIM10 (Timer1-Update & Timer10) */
      &Timer11ISRSelect,   /* 26  OS_IO_TIM1_TRG_COM_TIM11 (Timer1-Trigger-Commutation & Timer11) */
      NULL,                /* 27  OS_IO_TIM1_CC */
      NULL,                /* 28  OS_IO_TIM2 */
      NULL,                /* 29  OS_IO_TIM3 */
      NULL,                /* 30  OS_IO_TIM4 */
      NULL,                /* 31  OS_IO_I2C1_EV */
      NULL,                /* 32  OS_IO_I2C1_ER */
      NULL,                /* 33  OS_IO_I2C2_EV */
      NULL,                /* 34  OS_IO_I2C2_ER */
      NULL,                /* 35  OS_IO_SPI1 */
      NULL,                /* 36  OS_IO_SPI2 */
      NULL,                /* 37  OS_IO_USART1 */
      NULL,                /* 38  OS_IO_USART2 */
      NULL,                /* 39  OS_IO_USART3 */
      NULL,                /* 40  OS_IO_EXTI15_10 */
      NULL,                /* 41  OS_IO_RTCAlarm */
      NULL,                /* 42  OS_IO_OTG_FS_WKUP */
      &Timer12ISRSelect,   /* 43  OS_IO_TIM8_BRK_TIM12 (Timer8-Break & Timer12) */
      &Timer13ISRSelect,   /* 44  OS_IO_TIM8_UP_TIM13 (Timer8-Update & Timer13) */
      &Timer14ISRSelect,   /* 45  OS_IO_TIM8_TRG_COM_TIM14 (Timer8-Trigger-Commutation & Timer14 global */
      NULL,                /* 46  OS_IO_TIM8_CC */
      NULL,                /* 47  OS_IO_DMA1_Stream7 */
      NULL,                /* 48  OS_IO_FSMC */
      NULL,                /* 49  OS_IO_SDIO */
      NULL,                /* 50  OS_IO_TIM5 */
      NULL,                /* 51  OS_IO_SPI3 */
      NULL,                /* 52  OS_IO_UART4 */
      NULL,                /* 53  OS_IO_UART5 */
      NULL,                /* 54  OS_IO_TIM6_DAC */
      NULL,                /* 55  OS_IO_TIM7 */
      NULL,                /* 56  OS_IO_DMA2_Stream0 */
      NULL,                /* 57  OS_IO_DMA2_Stream1 */
      NULL,                /* 58  OS_IO_DMA2_Stream2 */
      NULL,                /* 59  OS_IO_DMA2_Stream3 */
      NULL,                /* 60  OS_IO_DMA2_Stream4 */
      NULL,                /* 61  OS_IO_ETH */
      NULL,                /* 62  OS_IO_ETH_WKUP */
      NULL,                /* 63  OS_IO_CAN2_TX */
      NULL,                /* 64  OS_IO_CAN2_RX0 */
      NULL,                /* 65  OS_IO_CAN2_RX1 */
      NULL,                /* 66  OS_IO_CAN2_SCE */
      NULL,                /* 67  OS_IO_OTG_FS */
      NULL,                /* 68  OS_IO_DMA2_Stream5 */
      NULL,                /* 69  OS_IO_DMA2_Stream6 */
      NULL,                /* 70  OS_IO_DMA2_Stream7 */
      NULL,                /* 71  OS_IO_USART6 */
      NULL,                /* 72  OS_IO_I2C3_EV */
      NULL,                /* 73  OS_IO_I2C3_ER */
      NULL,                /* 74  OS_IO_OTG_HS_EP1_OUT */
      NULL,                /* 75  OS_IO_OTG_HS_EP1_IN */
      NULL,                /* 76  OS_IO_OTG_HS_WKUP */
      NULL,                /* 77  OS_IO_OTG_HS */
      NULL,                /* 78  OS_IO_DCMI */
      NULL,                /* 79  OS_IO_CRYP */
      NULL                 /* 80  OS_IO_HASH_RNG */
   };
#elif defined(STM32F4XXXX)
   static TIMERSELECT Timer9ISRSelect  = {_OSTimerSelectorHandler,{NULL,NULL},{0x40010000,0x40014000},0x80};
   static TIMERSELECT Timer10ISRSelect = {_OSTimerSelectorHandler,{NULL,NULL},{0x40010000,0x40014400},0x01};
   static TIMERSELECT Timer11ISRSelect = {_OSTimerSelectorHandler,{NULL,NULL},{0x40010000,0x40014800},0x40};
   static TIMERSELECT Timer12ISRSelect = {_OSTimerSelectorHandler,{NULL,NULL},{0x40010400,0x40001800},0x80};
   static TIMERSELECT Timer13ISRSelect = {_OSTimerSelectorHandler,{NULL,NULL},{0x40010400,0x40001C00},0x01};
   static TIMERSELECT Timer14ISRSelect = {_OSTimerSelectorHandler,{NULL,NULL},{0x40010400,0x40002000},0x40};
   void *_OSTabDevice[82] = {
      NULL,                /*  0  OS_IO_WWDG */
      NULL,                /*  1  OS_IO_PVD */
      NULL,                /*  2  OS_IO_TAMP_STAMP */
      NULL,                /*  3  OS_IO_RTC_WKUP */
      NULL,                /*  4  OS_IO_FLASH */
      NULL,                /*  5  OS_IO_RCC */
      NULL,                /*  6  OS_IO_EXTI0 */
      NULL,                /*  7  OS_IO_EXTI1 */
      NULL,                /*  8  OS_IO_EXTI2 */
      NULL,                /*  9  OS_IO_EXTI3 */
      NULL,                /* 10  OS_IO_EXTI4 */
      NULL,                /* 11  OS_IO_DMA1_Stream0 */
      NULL,                /* 12  OS_IO_DMA1_Stream1 */
      NULL,                /* 13  OS_IO_DMA1_Stream2 */
      NULL,                /* 14  OS_IO_DMA1_Stream3 */
      NULL,                /* 15  OS_IO_DMA1_Stream4 */
      NULL,                /* 16  OS_IO_DMA1_Stream5 */
      NULL,                /* 17  OS_IO_DMA1_Stream6 */
      NULL,                /* 18  OS_IO_ADC */
      NULL,                /* 19  OS_IO_CAN1_TX */
      NULL,                /* 20  OS_IO_CAN1_RX0 */
      NULL,                /* 21  OS_IO_CAN1_RX1 */
      NULL,                /* 22  OS_IO_CAN1_SCE */
      NULL,                /* 23  OS_IO_EXTI9_5 */
      &Timer9ISRSelect,    /* 24  OS_IO_TIM1_BRK_TIM9 (Timer1-Break & Timer9) */
      &Timer10ISRSelect,   /* 25  OS_IO_TIM1_UP_TIM10 (Timer1-Update & Timer10) */
      &Timer11ISRSelect,   /* 26  OS_IO_TIM1_TRG_COM_TIM11 (Timer1-Trigger-Commutation & Timer11) */
      NULL,                /* 27  OS_IO_TIM1_CC */
      NULL,                /* 28  OS_IO_TIM2 */
      NULL,                /* 29  OS_IO_TIM3 */
      NULL,                /* 30  OS_IO_TIM4 */
      NULL,                /* 31  OS_IO_I2C1_EV */
      NULL,                /* 32  OS_IO_I2C1_ER */
      NULL,                /* 33  OS_IO_I2C2_EV */
      NULL,                /* 34  OS_IO_I2C2_ER */
      NULL,                /* 35  OS_IO_SPI1 */
      NULL,                /* 36  OS_IO_SPI2 */
      NULL,                /* 37  OS_IO_USART1 */
      NULL,                /* 38  OS_IO_USART2 */
      NULL,                /* 39  OS_IO_USART3 */
      NULL,                /* 40  OS_IO_EXTI15_10 */
      NULL,                /* 41  OS_IO_RTCAlarm */
      NULL,                /* 42  OS_IO_OTG_FS_WKUP */
      &Timer12ISRSelect,   /* 43  OS_IO_TIM8_BRK_TIM12 (Timer8-Break & Timer12) */
      &Timer13ISRSelect,   /* 44  OS_IO_TIM8_UP_TIM13 (Timer8-Update & TIM13) */
      &Timer14ISRSelect,   /* 45  OS_IO_TIM8_TRG_COM_TIM14 (Timer8-Trigger-Commutation & Timer14) */
      NULL,                /* 46  OS_IO_TIM8_CC */
      NULL,                /* 47  OS_IO_DMA1_Stream7 */
      NULL,                /* 48  OS_IO_FSMC */
      NULL,                /* 49  OS_IO_SDIO */
      NULL,                /* 50  OS_IO_TIM5 */
      NULL,                /* 51  OS_IO_SPI3 */
      NULL,                /* 52  OS_IO_UART4 */
      NULL,                /* 53  OS_IO_UART5 */
      NULL,                /* 54  OS_IO_TIM6_DAC */
      NULL,                /* 55  OS_IO_TIM7 */
      NULL,                /* 56  OS_IO_DMA2_Stream0 */
      NULL,                /* 57  OS_IO_DMA2_Stream1 */
      NULL,                /* 58  OS_IO_DMA2_Stream2 */
      NULL,                /* 59  OS_IO_DMA2_Stream3 */
      NULL,                /* 60  OS_IO_DMA2_Stream4 */
      NULL,                /* 61  OS_IO_ETH */
      NULL,                /* 62  OS_IO_ETH_WKUP */
      NULL,                /* 63  OS_IO_CAN2_TX */
      NULL,                /* 64  OS_IO_CAN2_RX0 */
      NULL,                /* 65  OS_IO_CAN2_RX1 */
      NULL,                /* 66  OS_IO_CAN2_SCE */
      NULL,                /* 67  OS_IO_OTG_FS */
      NULL,                /* 68  OS_IO_DMA2_Stream5 */
      NULL,                /* 69  OS_IO_DMA2_Stream6 */
      NULL,                /* 70  OS_IO_DMA2_Stream7 */
      NULL,                /* 71  OS_IO_USART6 */
      NULL,                /* 72  OS_IO_I2C3_EV */
      NULL,                /* 73  OS_IO_I2C3_ER */
      NULL,                /* 74  OS_IO_OTG_HS_EP1_OUT */
      NULL,                /* 75  OS_IO_OTG_HS_EP1_IN */
      NULL,                /* 76  OS_IO_OTG_HS_WKUP */
      NULL,                /* 77  OS_IO_OTG_HS */
      NULL,                /* 78  OS_IO_DCMI */
      NULL,                /* 79  OS_IO_CRYP */
      NULL,                /* 80  OS_IO_HASH_RNG */
      NULL                 /* 81  OS_IO_FPU */
   };
#else
  #error STM32 version undefined
#endif


/* _OSTimerSelector: Called by _OSIOHandler when a multiple source interrupt involving a
** timer raises an interrupt and transfers the call to it. */
void _OSTimerSelectorHandler(struct TIMERSELECT *timerSelect)
{
  #define OFFSET_STATUS 0x10 // Offset to retrieve the interrupt status register
  #define OFFSET_ENABLE 0x0C // Offset to retrieve the interrupt enable bit register
  void (*peripheralIODescriptor)void *);

  /* The first timer device bound the IRQ may also be bound to one of its specific inter-
  ** rupt sources (break, update or trigger-commutation). This is why we also need to mask
  ** the specific source of the IRQ. */
  if (*(UINT32 *)(timerSelect->BaseRegister[0] + OFFSET_STATUS) &
      *(UINT32 *)(timerSelect->BaseRegister[0] + OFFSET_ENABLE) &
      timerSelect->FirstEntryMask) {  // Did the first timer device raise the interrupt?
     peripheralIODescriptor = timerSelect->TimerISRDescriptor[0];
     peripheralIODescriptor->TimerIntHandler(peripheralIODescriptor);
  }
  /* The second device bound to the IRQ is global to the device (i.e. the interrupt may
  ** be caused for any reason related to the device); no mask is therefore required. */
  if (*(UINT32 *)(timerSelect->BaseRegister[1] + OFFSET_STATUS) &
      *(UINT32 *)(timerSelect->BaseRegister[1] + OFFSET_ENABLE)) {
     peripheralIODescriptor = timerSelect->TimerISRDescriptor[1];
     peripheralIODescriptor->TimerIntHandler(peripheralIODescriptor);
  }
} /* end of _OSTimerSelectorHandler */


/* OSSetISRDescriptor: Associates an ISR descriptor with an _OSTabDevice entry.
** Parameters:
**   (1) (UINT16) index of the _OSTabDevice entry:
**       For entries bound to more than a single interrupt source but involving at least
**       a timer, for instance entry TIM1_TRG_COM_ TIM11 STM32F205xx, the lower byte iden-
**       tifies the table entry and the upper byte, which can be 1 or 2, indicates the re-
**       quested timer.
**   (2) (void *) ISR descriptor for the specified interrupt.
** Returned value: none. */
void OSSetISRDescriptor(UINT16 entry, void *descriptor)
{
  UINT8 index = entry & 0xFF;
  if (_OSTabDevice[index] != NULL &&
         ((TIMERSELECT *)_OSTabDevice[index])->TimerSelector == _OSTimerSelectorHandler)
     ((void **)(_OSTabDevice[index]))[entry >> 8] = descriptor;
  else
     _OSTabDevice[index] = descriptor;
} /* end of OSSetISRDescriptor */


/* OSGetISRDescriptor: Returns the ISR descriptor associated with an _OSTabDevice entry.
** Parameter: (UINT16) index of _OSTabDevice where the ISR descriptor is held. For entries
**    bound to 2 timers, i.e. for interrupts that combine 2 different timer devices, the
**    lower byte identifies the table entry and the upper byte indicates the wanted timer.
** Returned value: (void *) The requested ISR descriptor is returned. If no previous
**    OSSetIODescriptor was previously made for the specified entry, the returned value
**    is NULL. */
void *OSGetISRDescriptor(UINT16 entry)
{
  UINT8 index = entry & 0xFF;
  if (_OSTabDevice[index] != NULL &&
         ((TIMERSELECT *)_OSTabDevice[index])->TimerSelector == _OSTimerSelectorHandler)
     return ((void **)(_OSTabDevice[index]))[entry >> 8];
  else
     return _OSTabDevice[index];
} /* end of OSGetISRDescriptor */
