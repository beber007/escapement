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
/* File ZottaOS_stm32f10x_conf.h: ZottaOS configuration file.
** Platform version: All STM32F10x microcontrollers.
** Version identifier: March 2012
** Authors: MIS-TIC
*/

#ifndef _ZOTTAOS_CONFIG_STM32F10x_H_
#define _ZOTTAOS_CONFIG_STM32F10x_H_

#include "ZottaOS_Config.h"

#if defined(STM32F100X4_X6)
   #define STM32F10X_LD_VL */  /*!< STM32F10X_LD_VL: STM32 Low density Value Line devices */
#elif defined(STM32F100X8_XB)
   #define STM32F10X_MD_VL */  /*!< STM32F10X_MD_VL: STM32 Medium density Value Line devices */
#elif defined(STM32F100RC_RD_RE) || defined(STM32F100VC_VD_VE_ZC_ZD_ZE)
   #define STM32F10X_HD_VL */  /*!< STM32F10X_HD_VL: STM32 High density value line devices */
#elif defined(STM32F101X4_X6) || defined(STM32F102X4_X6) || defined(STM32F103X4_X6)
   #define STM32F10X_LD */     /*!< STM32F10X_LD: STM32 Low density devices */
#elif defined(STM32F101T8_TB) || defined(STM32F101C8_CB_R8_RB_V8_VB) || \
      defined(STM32F102X8_XB) || defined(STM32F103T8_TB) || \
      defined(STM32F103C8_CB_R8_RB_V8_VB)
   #define STM32F10X_MD */     /*!< STM32F10X_MD: STM32 Medium density devices */
#elif defined(STM32F101RC_RD_RE) || defined(STM32F101VC_VD_VE_ZC_ZD_ZE) || \
      defined(STM32F103RC_RD_RE) || defined(STM32F103VC_VD_VE_ZC_ZD_ZE)
   #define STM32F10X_HD */     /*!< STM32F10X_HD: STM32 High density devices */
#elif defined(STM32F101RF_RG) || defined(STM32F101VF_VG_ZF_ZG) || \
      defined(STM32F103RF_RG) || defined(STM32F103VF_VG_ZF_ZG)
   #define STM32F10X_XL */     /*!< STM32F10X_XL: STM32 XL-density devices */
#elif defined(STM32F105XX) || defined(STM32F107XX)
   #define STM32F10X_CL */     /*!< STM32F10X_CL: STM32 Connectivity line devices */
#endif

/* Uncomment/Comment the line below to enable/disable peripheral header file inclusion */
#include "stm32f10x_adc.h"
#include "stm32f10x_bkp.h"
#include "stm32f10x_can.h"
#include "stm32f10x_cec.h"
#include "stm32f10x_crc.h"
#include "stm32f10x_dac.h"
#include "stm32f10x_dbgmcu.h"
#include "stm32f10x_dma.h"
#include "stm32f10x_exti.h"
#include "stm32f10x_flash.h"
#include "stm32f10x_fsmc.h"
#include "stm32f10x_gpio.h"
#include "stm32f10x_i2c.h"
#include "stm32f10x_iwdg.h"
#include "stm32f10x_pwr.h"
#include "stm32f10x_rcc.h"
#include "stm32f10x_rtc.h"
#include "stm32f10x_sdio.h"
#include "stm32f10x_spi.h"
#include "stm32f10x_tim.h"
#include "stm32f10x_usart.h"
#include "stm32f10x_wwdg.h"
#include "misc.h" /* High level functions for NVIC and SysTick (add-on to CMSIS functions) */

#define assert_param(expr) ((void)0)

#endif /* _ZOTTAOS_CONFIG_STM32F10x_H_ */
