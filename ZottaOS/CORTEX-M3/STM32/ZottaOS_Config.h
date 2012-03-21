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
/* File ZottaOS_Config.h: ZottaOS configuration file.
** Platform version: All STM32 microcontrollers.
** Version identifier: March 2012
** Authors: MIS-TIC
*/

#ifndef ZOTTAOS_CONFIG_H_
#define ZOTTAOS_CONFIG_H_


/* Selects which version to be used ZottaOS */
#define ZOTTAOS_VERSION_HARD
//#define ZOTTAOS_VERSION_SOFT


/* The nested vector interrupt controller (NVIC) under Cortex-M3 allows dynamic prioriti-
** zation of interrupts with up to 256 levels that can be arranged into priority level
** groups where each group can be preempted. The NVIC is configurable by the pair (A,B),
** where A denotes the number of bits used to set the number of groups (number of priori-
** ties), and B is the number of bits used for the number of subpriorities within a
** group. A + B = 8 and A = [2..7]. Note that ZottaOS requires 3 priority groups for it-
** self. From highest to lowest priority, these are: one for the peripheral hardware tim-
** er, one for the software timer interrupt and a final one for the PendSV used to finali-
** ze a context switch.
** In the following, PRIGROUP determines the setting of the (A,B) pair, where PRIGROUP is
** a value in the range [0..5] yielding A = 7 - PRIGROUP and B = 8 - A.
** Note that some microcontrollers may have fewer than 8 bits to define the priority le-
** vels, for example STM-32 uses only 4 bits and PRIGROUP must be in the range [3..5]
** which then gives A = 7 - PRIGROUP and B = 4 - A. */
#define PRIGROUP (UINT32)3


/* Select the interval-timer */
#define ZOTTAOS_TIMER OS_IO_TIM1
//#define ZOTTAOS_TIMER OS_IO_TIM3

/* Define the interval-timer prescaler */
#define ZOTTAOS_TIMER_PRESCALER 71

/* Defines the priority group and level of the interval-timer. The timer's priority must
** be higher than that of SysTick. See _OSSetInterruptPriority(). */
#define TIMER_PRIORITY      (UINT8)0
#define TIMER_SUB_PRIORITY  (UINT8)0


/* Uncomment the line below that corresponds to your target STM32 device while leaving
** the others commented. */

/*------------------------------------- STM32L1XXXX ------------------------------------*/

/* STM32L151C6, STM32L151R6, STM32L151V6, STM32L151C8, STM32L151R8, STM32L151V8,
** STM32L151CB, STM32L151RB and STM32L151VB */
//#define STM32L151X6_X8_XB

/* STM32L152C6, STM32L152R6, STM32L152V6, STM32L152C8, STM32L152R8, STM32L152V8, \
** STM32L152CB, STM32L152RB and STM32L152VB */
//#define STM32L152X6_X8_XB

/* STM32L151RC, STM32L151VC, STM32L151ZC and STM32L151QC */
//#define STM32L151XC

/* STM32L152RC, STM32L152VC, STM32L152ZC and STM32L152QC */
//#define STM32L152XC

/* STM32L151RD, STM32L151VD, STM32L151ZD and STM32L151QD */
//#define STM32L151XD

/* STM32L152RD, STM32L152VD, STM32L152ZD and STM32L152QD */
//#define STM32L152XD

/* STM32L162RD, STM32L162VD, STM32L162ZD and STM32L162QD */
//#define STM32L162XD

/*------------------------------------- STM32F10XXX ------------------------------------*/

 /* STM32F100C4, STM32F100C6, STM32F100R4 and STM32F100R6 */
//#define STM32F100X4_X6

/* STM32F100C8, STM32F100CB, STM32F100R8, STM32F100RB, STM32F100V8 and STM32F100VB */
//#define STM32F100X8_XB

/* STM32F100RC, STM32F100RD and STM32F100RE */
//#define STM32F100RC_RD_RE

/* STM32F100VC, STM32F100VD, STM32F100VE, STM32F100ZC, STM32F100ZD, STM32F100ZE */
//#define STM32F100VC_VD_VE_ZC_ZD_ZE

/* STM32F101T4, STM32F101T6, STM32F101C4, STM32F101C8, STM32F101R4 and STM32F100R6 */
//#define STM32F101X4_X6

/* STM32F101T8 and STM32F101T8_TB */
//#define STM32F101T8_TB

/* STM32F101C8, STM32F101CB, STM32F101R8, STM32F101CRB, STM32F101V8 and STM32F101VB */
//#define STM32F101C8_CB_R8_RB_V8_VB

/* STM32F101RC, STM32F101RD and STM32F101RE */
//#define STM32F101RC_RD_RE

/* STM32F101VC, STM32F101VD, STM32F101VE, STM32F101ZC, STM32F101ZD and STM32F101ZE */
//#define STM32F101VC_VD_VE_ZC_ZD_ZE

/* STM32F101RF and STM32F101RF_RG */
//#define STM32F101RF_RG

/* STM32F101VF, STM32F101VG, STM32F101ZF and STM32F101ZG */
//#define STM32F101VF_VG_ZF_ZG

/* STM32F102C4, STM32F102C6, STM32F102R4 and STM32F102R6 */
//#define STM32F102X4_X6

/* STM32F102C8, STM32F102CB, STM32F102R8 and STM32F102RB */
//#define STM32F102X8_XB

/* STM32F103T4, STM32F103T6, STM32F103C4, STM32F103C6, STM32F103R4 and STM32F103R6*/
//#define STM32F103X4_X6

/* STM32F103T8 and STM32F103TB*/
//#define STM32F103T8_TB

/* STM32F103C8, STM32F103CB, STM32F103R8, STM32F103RB, STM32F103V8 and STM32F103VB */
//#define STM32F103C8_CB_R8_RB_V8_VB

/* STM32F103RC, STM32F103RD and STM32F103RE */
#define STM32F103RC_RD_RE

/* STM32F103VC, STM32F103VD, STM32F103VE, STM32F103ZC, STM32F103ZD and STM32F103ZE */
//#define STM32F103VC_VD_VE_ZC_ZD_ZE

/* STM32F103RF and STM32F103RG */
//#define STM32F103RF_RG

/* STM32F103VF, STM32F103VG, STM32F103ZF and STM32F103ZG */
//#define STM32F103VF_VG_ZF_ZG

/* STM32F105R8, STM32F105RB, STM32F105RB, STM32F105V8, STM32F105VB and STM32F105RB */
//#define STM32F105XX

/* STM32F107R8, STM32F107RB, STM32F107RB, STM32F107V8, STM32F107VB and STM32F107RB */
//#define STM32F107XX

/*------------------------------------- STM32F2XXXX ------------------------------------*/

/* STM32F205RB, STM32F205RC, STM32F205RE, STM32F205RF and STM32F205RG */
//#define STM32F205RX

/* STM32F205VB, STM32F205VC, STM32F205VE, STM32F205VF, STM32F205VG, STM32F205ZC, \
** STM32F205ZE, STM32F205ZF and STM32F205ZG */
//#define STM32F205VX_ZX

/* STM32F207RC, STM32F207RE, STM32F207RF, STM32F207RG, STM32F207VC, STM32F207VE, \
** STM32F207VF, STM32F207VG, STM32F207ZC, STM32F207ZE, STM32F207ZF and STM32F207ZG */
//#define STM32F207XX

/* STM32F215RE and STM32F215RG */
//#define STM32F215RX

/* STM32F215VE, STM32F215VG, STM32F215ZE and STM32F215ZG */
//#define STM32F215VX_ZX

/* STM32F217VE, STM32F217VG, STM32F217ZE, STM32F217ZG, STM32F217LE and STM32F217LG */
//#define STM32F217XX

/*------------------------------------- STM32F4XXXX ------------------------------------*/

//#define STM32F405RG

/* STM32F405VG and STM32F405ZG */
//#define STM32F405VG_ZG

/* STM32F407VE, STM32F407VG, STM32F407Ze, STM32F407ZG, STM32F407LE and STM32F407LG */
//#define STM32F407XX

//#define STM32F415RG

/* STM32F415VG and STM32F415ZG */
//#define STM32F415VG_ZG

/* STM32F417VE, STM32F417VG, STM32F417ZE, STM32F417ZG, STM32F417LE and STM32F417LG */
//#define STM32F417XX


#if defined(STM32L151X6_X8_XB) || defined(STM32L152X6_X8_XB) || defined(STM32L151XC) || \
    defined(STM32L152XC) || defined(STM32L151XD) || defined(STM32L152XD) || \
    defined(STM32L162XD)
   #define STM32L1XXXX
#elif defined(STM32F100X4_X6) || defined(STM32F100X8_XB) || \
      defined(STM32F100RC_RD_RE) || defined(STM32F100VC_VD_VE_ZC_ZD_ZE) || \
      defined(STM32F101X4_X6) || defined(STM32F101T8_TB) || \
      defined(STM32F101C8_CB_R8_RB_V8_VB) || defined(STM32F101RC_RD_RE) || \
      defined(STM32F101VC_VD_VE_ZC_ZD_ZE) || defined(STM32F101RF_RG) || \
      defined(STM32F101VF_VG_ZF_ZG) || defined(STM32F102X4_X6) || \
      defined(STM32F102X8_XB) || defined(STM32F103X4_X6) || defined(STM32F103T8_TB) || \
      defined(STM32F103C8_CB_R8_RB_V8_VB) || defined(STM32F103RC_RD_RE) || \
      defined(STM32F103VC_VD_VE_ZC_ZD_ZE) || defined(STM32F103RF_RG) || \
      defined(STM32F103VF_VG_ZF_ZG) || defined(STM32F105XX) || defined(STM32F107XX)
   #define STM32F1XXXX
#elif defined(STM32F205RX) || defined(STM32F205VX_ZX) || defined(STM32F207XX) || \
      defined(STM32F215RX) || defined(STM32F215VX_ZX) || defined(STM32F217XX)
   #define STM32F2XXXX
#elif defined(STM32F405RG) || defined(STM32F405VG_ZG) || defined(STM32F407XX)|| \
      defined(STM32F415RG) || defined(STM32F415VG_ZG) || defined(STM32F417XX)
   #define STM32F4XXXX
#else
   #error STM32 version undefined
#endif

#endif /* ZOTTAOS_CONFIG_H_ */
