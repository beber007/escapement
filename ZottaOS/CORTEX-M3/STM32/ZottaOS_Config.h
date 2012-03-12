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
/* File ZottaOS_Config.h: .
** Version identifier: March 2012
** Authors: MIS-TIC
*/

#ifndef ZOTTAOS_CONFIG_H_
#define ZOTTAOS_CONFIG_H_

//#define ZOTTAOS_VERSION_HARD
#define ZOTTAOS_VERSION_SOFT

/* Uncomment the line below that corresponds to your target STM32 device while leaving
** the others commented. */
 #if !defined (STM32F10X_LD) && !defined (STM32F10X_LD_VL) && !defined (STM32F10X_MD) && \
     !defined (STM32F10X_MD_VL) && !defined (STM32F10X_HD) && !defined (STM32F10X_HD_VL) &&\
     !defined (STM32F10X_XL) && !defined (STM32F10X_CL)
  /* #define STM32F10X_LD */     /*!< STM32F10X_LD: STM32 Low density devices */
  /* #define STM32F10X_LD_VL */  /*!< STM32F10X_LD_VL: STM32 Low density Value Line devices */
  /* #define STM32F10X_MD */     /*!< STM32F10X_MD: STM32 Medium density devices */
  /* #define STM32F10X_MD_VL */  /*!< STM32F10X_MD_VL: STM32 Medium density Value Line devices */
  /* #define STM32F10X_HD */     /*!< STM32F10X_HD: STM32 High density devices */
  #define STM32F10X_HD_VL     /*!< STM32F10X_HD_VL: STM32 High density value line devices */
  /* #define STM32F10X_XL */     /*!< STM32F10X_XL: STM32 XL-density devices */
  /* #define STM32F10X_CL */     /*!< STM32F10X_CL: STM32 Connectivity line devices */
#endif

/* The nested vector interrupt controller (NVIC) under Cortex-M3 allows dynamic prioriti-
** zation of interrupts with up to 256 levels that can be arranged into priority level
** groups where each group can be preempted. The NVIC is configurable by the pair (A,B),
** where A denotes the number of bits used to set the number of groups (number of priori-
** ties), and B is the number of bits used for the number of subpriorities within a
** group. A + B = 8 and A = [2..7]. Note that NTRTOS requires 3 priority groups for it-
** self. From highest to lowest priority, these are: one for the peripheral hardware tim-
** er, one for the software timer interrupt and a final one for the PendSV used to finali-
** ze a context switch.
** In the following, PRIGROUP determines the setting of the (A,B) pair, where PRIGROUP is
** a value in the range [0..5] yielding A = 7 - PRIGROUP and B = 8 - A.
** Note that some microcontrollers may have fewer than 8 bits to define the priority le-
** vels, for example STM-32 uses only 4 bits and PRIGROUP must be in the range [3..5]
** which then gives A = 7 - PRIGROUP and B = 4 - A. */
#define PRIGROUP (UINT32)3


/* Defines the priority group and level of the interval-timer. The timer's priority must
** be higher than that of SysTick. See _OSSetInterruptPriority(). */
#define TIMER_PRIORITY      (UINT8)0
#define TIMER_SUB_PRIORITY  (UINT8)0

#endif /* ZOTTAOS_CONFIG_H_ */
