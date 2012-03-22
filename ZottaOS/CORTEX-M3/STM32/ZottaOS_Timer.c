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
/* File ZottaOS_Timer.c: Hardware abstract timer layer. This file holds all timer
**          related functions needed by the ZottaOS family of kernels so that these can
**          easily be ported from one MSP to another and also to other microcontrollers.
** Platform version: All STM32 microcontrollers.
** Version date: March 2012
** Authors: MIS-TIC
*/

#include "ZottaOS_CortexM3.h"
#include "ZottaOS.h"
#include "ZottaOS_Timer.h"


/* Definitions of hardware registers */

#ifdef ZOTTAOS_TIMER
   #if defined(STM32L1XXXX)
      #if ZOTTAOS_TIMER == OS_IO_TIM11
         #define TIME_BASE 0x40011000
      #elif ZOTTAOS_TIMER == OS_IO_TIM10
         #define TIME_BASE 0x40010C00
      #elif ZOTTAOS_TIMER == OS_IO_TIM9
         #define TIME_BASE 0x40010800
      #elif ZOTTAOS_TIMER == OS_IO_TIM5
         #define TIME_BASE 0x40000C00
      #elif ZOTTAOS_TIMER == OS_IO_TIM4
         #define TIME_BASE 0x40000800
      #elif ZOTTAOS_TIMER == OS_IO_TIM3
         #define TIME_BASE 0x40000400
      #elif ZOTTAOS_TIMER == OS_IO_TIM2
         #define TIME_BASE 0x40000000
      #else
         #error Selected timer does not exist! (verify ZOTTAOS_TIMER defined in ZottaOS_Config.h)
      #endif
   #elif defined(STM32F1XXXX)
      #if ZOTTAOS_TIMER == OS_IO_TIM17
         #define TIME_BASE 0x40014800
      #elif ZOTTAOS_TIMER == OS_IO_TIM16
         #define TIME_BASE 0x40014400
      #elif ZOTTAOS_TIMER == OS_IO_TIM15
         #define TIME_BASE 0x40014000
      #elif ZOTTAOS_TIMER == OS_IO_TIM14
         #define TIME_BASE 0x40002000
      #elif ZOTTAOS_TIMER == OS_IO_TIM13
         #define TIME_BASE 0x40001C00
      #elif ZOTTAOS_TIMER == OS_IO_TIM12
         #define TIME_BASE 0x40001800
      #elif ZOTTAOS_TIMER == OS_IO_TIM11
         #define TIME_BASE 0x40015400
      #elif ZOTTAOS_TIMER == OS_IO_TIM10
         #define TIME_BASE 0x40015000
      #elif ZOTTAOS_TIMER == OS_IO_TIM9
         #define TIME_BASE 0x40014C00
      #elif ZOTTAOS_TIMER == OS_IO_TIM8
         #define TIME_BASE 0x40013400
      #elif ZOTTAOS_TIMER == OS_IO_TIM5
         #define TIME_BASE 0x40000C00
      #elif ZOTTAOS_TIMER == OS_IO_TIM34
         #define TIME_BASE 0x40000800
      #elif ZOTTAOS_TIMER == OS_IO_TIM3
         #define TIME_BASE 0x40000400
      #elif ZOTTAOS_TIMER == OS_IO_TIM2
         #define TIME_BASE 0x40000000
      #elif ZOTTAOS_TIMER == OS_IO_TIM1
         #define TIME_BASE 0x40012C00
      #else
         #error Selected timer does not exist! (verify ZOTTAOS_TIMER defined in ZottaOS_Config.h)
      #endif
   #elif defined(STM32F2XXXX) || defined(STM32F4XXXX)
      #if ZOTTAOS_TIMER == OS_IO_TIM14
         #define TIME_BASE 0x40002000
      #elif ZOTTAOS_TIMER == OS_IO_TIM13
         #define TIME_BASE 0x40001C00
      #elif ZOTTAOS_TIMER == OS_IO_TIM12
         #define TIME_BASE 0x40001800
      #elif ZOTTAOS_TIMER == OS_IO_TIM11
         #define TIME_BASE 0x40014800
      #elif ZOTTAOS_TIMER == OS_IO_TIM10
         #define TIME_BASE 0x40014400
      #elif ZOTTAOS_TIMER == OS_IO_TIM9
         #define TIME_BASE 0x40014000
      #elif ZOTTAOS_TIMER == OS_IO_TIM8
         #define TIME_BASE 0x40010400
      #elif ZOTTAOS_TIMER == OS_IO_TIM5
         #define TIME_BASE 0x40000C00
      #elif ZOTTAOS_TIMER == OS_IO_TIM4
         #define TIME_BASE 0x40000800
      #elif ZOTTAOS_TIMER == OS_IO_TIM3
         #define TIME_BASE 0x40000400
      #elif ZOTTAOS_TIMER == OS_IO_TIM2
         #define TIME_BASE 0x40000000
      #elif ZOTTAOS_TIMER == OS_IO_TIM1
         #define TIME_BASE 0x40010000
      #else
         #error Selected timer does not exist! (verify ZOTTAOS_TIMER defined in ZottaOS_Config.h)
      #endif
   #endif
#else
   #error You must select a timer for ZottaOS! (define ZOTTAOS_TIMER in ZottaOS_Config.h)
#endif


#if defined(STM32L1XXXX)
   #if ZOTTAOS_TIMER == OS_IO_TIM9 || ZOTTAOS_TIMER ==  OS_IO_TIM10 || \
       ZOTTAOS_TIMER == OS_IO_TIM11
      #define CLK_ENABLE   *((UINT32 *)0x40023820) // RCC_APB2ENR
      //#define CLK_ENABLE   *((UINT32 *)0x4002382C) // RCC_APB2LPENR
      #if ZOTTAOS_TIMER == OS_IO_TIM9
         #define CLK_ENABLE_BIT 0x4
      #elif ZOTTAOS_TIMER ==  OS_IO_TIM10
         #define CLK_ENABLE_BIT 0x8
      #elif ZOTTAOS_TIMER == OS_IO_TIM11
         #define CLK_ENABLE_BIT 0x10
      #endif
   #elif ZOTTAOS_TIMER ==  OS_IO_TIM2 || ZOTTAOS_TIMER == OS_IO_TIM3 || \
         ZOTTAOS_TIMER == OS_IO_TIM4 || ZOTTAOS_TIMER == OS_IO_TIM5
      #define CLK_ENABLE   *((UINT32 *)0x40023824) // RCC_APB1ENR
      //#define CLK_ENABLE   *((UINT32 *)0x40023830) // RCC_APB1LPENR
      #if ZOTTAOS_TIMER ==  OS_IO_TIM2
         #define CLK_ENABLE_BIT 0x1
      #elif ZOTTAOS_TIMER == OS_IO_TIM3
         #define CLK_ENABLE_BIT 0x2
      #elif ZOTTAOS_TIMER == OS_IO_TIM4
         #define CLK_ENABLE_BIT 0x4
      #elif ZOTTAOS_TIMER == OS_IO_TIM5
         #define CLK_ENABLE_BIT 0x8
      #endif
   #endif
#elif defined(STM32F1XXXX)
   #if ZOTTAOS_TIMER == OS_IO_TIM1 || ZOTTAOS_TIMER ==  OS_IO_TIM8 || \
       ZOTTAOS_TIMER == OS_IO_TIM9 || ZOTTAOS_TIMER == OS_IO_TIM10 || ZOTTAOS_TIMER == OS_IO_TIM11 || \
       ZOTTAOS_TIMER == OS_IO_TIM15 || ZOTTAOS_TIMER == OS_IO_TIM16 || ZOTTAOS_TIMER == OS_IO_TIM17
      #define CLK_ENABLE   *((UINT32 *)0x40021018) // RCC_APB2ENR
      #if ZOTTAOS_TIMER == OS_IO_TIM1
         #define CLK_ENABLE_BIT 0x800
      #elif ZOTTAOS_TIMER ==  OS_IO_TIM8
         #define CLK_ENABLE_BIT 0x2000
      #elif ZOTTAOS_TIMER == OS_IO_TIM9
         #define CLK_ENABLE_BIT 0x80000
      #elif ZOTTAOS_TIMER == OS_IO_TIM10
         #define CLK_ENABLE_BIT 0x100000
      #elif ZOTTAOS_TIMER == OS_IO_TIM11
         #define CLK_ENABLE_BIT 0x200000
      #elif ZOTTAOS_TIMER == OS_IO_TIM15
         #define CLK_ENABLE_BIT 0x10000
      #elif ZOTTAOS_TIMER == OS_IO_TIM16
         #define CLK_ENABLE_BIT 0x20000
      #elif ZOTTAOS_TIMER == OS_IO_TIM17
         #define CLK_ENABLE_BIT 0x40000
      #endif
   #elif ZOTTAOS_TIMER ==  OS_IO_TIM2 || ZOTTAOS_TIMER == OS_IO_TIM3 || \
         ZOTTAOS_TIMER == OS_IO_TIM4 || ZOTTAOS_TIMER == OS_IO_TIM5 || \
         ZOTTAOS_TIMER == OS_IO_TIM12 || ZOTTAOS_TIMER == OS_IO_TIM13 || \
         ZOTTAOS_TIMER == OS_IO_TIM14
      #define CLK_ENABLE *((UINT32 *)0x4002101C) // RCC_APB1ENR
      #if ZOTTAOS_TIMER ==  OS_IO_TIM2
         #define CLK_ENABLE_BIT 0x1
      #elif ZOTTAOS_TIMER == OS_IO_TIM3
         #define CLK_ENABLE_BIT 0x2
      #elif ZOTTAOS_TIMER == OS_IO_TIM4
         #define CLK_ENABLE_BIT 0x4
      #elif ZOTTAOS_TIMER == OS_IO_TIM5
         #define CLK_ENABLE_BIT 0x8
      #elif ZOTTAOS_TIMER == OS_IO_TIM12
         #define CLK_ENABLE_BIT 0x40
      #elif ZOTTAOS_TIMER == OS_IO_TIM13
         #define CLK_ENABLE_BIT 0x80
      #elif ZOTTAOS_TIMER == OS_IO_TIM14
         #define CLK_ENABLE_BIT 0x100
      #endif
   #endif
#elif defined(STM32F2XXXX) || defined(STM32F4XXXX)
   #if ZOTTAOS_TIMER == OS_IO_TIM1 || ZOTTAOS_TIMER == OS_IO_TIM8 || \
       ZOTTAOS_TIMER == OS_IO_TIM9 || ZOTTAOS_TIMER == OS_IO_TIM10 || ZOTTAOS_TIMER == OS_IO_TIM11
      #define CLK_ENABLE   *((UINT32 *)0x40023844) // RCC_APB2ENR
      //#define CLK_ENABLE   *((UINT32 *)0x40023864) // RCC_APB2LPENR
      #if ZOTTAOS_TIMER == OS_IO_TIM1
         #define CLK_ENABLE_BIT 0x1
      #elif ZOTTAOS_TIMER == OS_IO_TIM8
         #define CLK_ENABLE_BIT 0x2
      #elif ZOTTAOS_TIMER == OS_IO_TIM9
         #define CLK_ENABLE_BIT 0x10000
      #elif ZOTTAOS_TIMER == OS_IO_TIM10
         #define CLK_ENABLE_BIT 0x20000
      #elif ZOTTAOS_TIMER == OS_IO_TIM11
         #define CLK_ENABLE_BIT 0x40000
      #endif
   #elif ZOTTAOS_TIMER == OS_IO_TIM2 || ZOTTAOS_TIMER == OS_IO_TIM3 || \
         ZOTTAOS_TIMER == OS_IO_TIM4 || ZOTTAOS_TIMER == OS_IO_TIM5 || \
         ZOTTAOS_TIMER == OS_IO_TIM12 || ZOTTAOS_TIMER == OS_IO_TIM13 || \
         ZOTTAOS_TIMER == OS_IO_TIM14
      #define CLK_ENABLE   *((UINT32 *)0x40023840) // RCC_APB1ENR
      //#define CLK_ENABLE   *((UINT32 *)0x40023860) // RCC_APB1LPENR
      #if ZOTTAOS_TIMER ==  OS_IO_TIM2
         #define CLK_ENABLE_BIT 0x1
      #elif ZOTTAOS_TIMER == OS_IO_TIM3
         #define CLK_ENABLE_BIT 0x2
      #elif ZOTTAOS_TIMER == OS_IO_TIM4
         #define CLK_ENABLE_BIT 0x4
      #elif ZOTTAOS_TIMER == OS_IO_TIM5
         #define CLK_ENABLE_BIT 0x8
      #elif ZOTTAOS_TIMER == OS_IO_TIM12
         #define CLK_ENABLE_BIT 0x40
      #elif ZOTTAOS_TIMER == OS_IO_TIM13
         #define CLK_ENABLE_BIT 0x80
      #elif ZOTTAOS_TIMER == OS_IO_TIM14
         #define CLK_ENABLE_BIT 0x100
      #endif
   #endif
#else
   #error STM32 version undefined
#endif


#define TIM_CONTROL1         *((UINT16 *)(TIME_BASE + 0x00))
#define TIM_INT_ENABLE       *((UINT16 *)(TIME_BASE + 0x0C))
#define TIM_STATUS           *((UINT16 *)(TIME_BASE + 0x10))
#define TIM_EVENT_GENERATION *((UINT16 *)(TIME_BASE + 0x14))
#define TIM_COUNTER          *((UINT16 *)(TIME_BASE + 0x24))
#define TIM_PRESCALER        *((UINT16 *)(TIME_BASE + 0x28))
#define TIM_AUTORELOAD       *((UINT16 *)(TIME_BASE + 0x2C))
#define TIM_COMPARATOR       *((UINT16 *)(TIME_BASE + 0x34))


/* System wall clock. This variable stores the most-significant 16 bits of the current
** time. The lower 16 bits are directly taken from the timer counter register. To get the
** current time, use _OSTimerGet. */
static volatile INT32 Time;


/* _OSInitializeTimer: Initializes the timer which starts counting as soon as ZottaOS is
** ready to process the first arrival. When the kernel, i.e. when OSStartMultitasking()
** is called, the last operation that is done is to set the timer handler and then start
** the idle task which is the only ready task in the system at that time. The first time
** the idle task executes, it calls _OSStartTimer().
** After _OSInitializeTimer() is called the timer's input divider is selected but it is
** halted.
** Note that the timer ISR updates the wall clock _OSTime with the current value of its
** comparator register CCR and adds 1, and at the very first interrupt, _OSTime must be
** set 0. This is done by initializing CCR with 0 - 1 = 0xFFFF. */
void _OSInitializeTimer(void)
{
  UINT8 tmppriority;
  #if ZOTTAOS_TIMER == OS_IO_TIM1 || ZOTTAOS_TIMER == OS_IO_TIM8
     UINT8 *intPriorityLevel_cc, *intPriorityLevel_up;
     UINT32 *intSetEnable_cc, *intSetEnable_up;
  #else
     UINT8 *intPriorityLevel;
     UINT32 *intSetEnable;
  #endif
  #if ZOTTAOS_TIMER == OS_IO_TIM1
     #if defined(STM32F103X4_X6) || defined(STM32F103T8_TB) || \
         defined(STM32F103C8_CB_R8_RB_V8_VB) || defined(STM32F103RC_RD_RE) || \
         defined(STM32F103VC_VD_VE_ZC_ZD_ZE) || defined(STM32F105XX) || defined(STM32F107XX)
        intSetEnable_up = (UINT32 *)0xE000E100 + (UINT32)(OS_IO_TIM1_UP / 32);
        intPriorityLevel_up = (UINT8 *)(0xE000E400 + OS_IO_TIM1_UP);
     #elif defined(STM32F100X4_X6) || defined(STM32F100X8_XB) || \
           defined(STM32F100RC_RD_RE) || defined(STM32F100VC_VD_VE_ZC_ZD_ZE)
        intSetEnable_up = (UINT32 *)0xE000E100 + (UINT32)(OS_IO_TIM1_UP_TIM16 / 32);
        intPriorityLevel_up = (UINT8 *)(0xE000E400 + OS_IO_TIM1_UP_TIM16);
     #elif defined (STM32F101RF_RG)|| defined(STM32F101VF_VG_ZF_ZG) || \
           defined(STM32F103RF_RG) || defined(STM32F103VF_VG_ZF_ZG) || \
           defined(STM32F2XXXX) || defined(STM32F4XXXX)
        intSetEnable_up = (UINT32 *)0xE000E100 + (UINT32)(OS_IO_TIM1_UP_TIM10 / 32);
        intPriorityLevel_up = (UINT8 *)(0xE000E400 + OS_IO_TIM1_UP_TIM10);
     #endif
     intSetEnable_cc = (UINT32 *)0xE000E100 + (UINT32)(OS_IO_TIM1_CC / 32);
     intPriorityLevel_cc = (UINT8 *)(0xE000E400 + OS_IO_TIM1_CC);
  #elif ZOTTAOS_TIMER == OS_IO_TIM8
     #if defined(STM32F101RC_RD_RE) || defined(STM32F101VC_VD_VE_ZC_ZD_ZE) || \
         defined(STM32F103RC_RD_RE) || defined(STM32F103VC_VD_VE_ZC_ZD_ZE)
        intSetEnable_up = (UINT32 *)0xE000E100 + (UINT32)(OS_IO_TIM8_UP / 32);
        intPriorityLevel_up = (UINT8 *)(0xE000E400 + OS_IO_TIM8_UP);
     #elif defined(STM32F101RF_RG) || defined(STM32F101VF_VG_ZF_ZG) || \
           defined(STM32F103RF_RG) || defined(STM32F103VF_VG_ZF_ZG) || \
           defined(STM32F2XXXX) || defined(STM32F4XXXX)
        intSetEnable_up = (UINT32 *)0xE000E100 + (UINT32)(OS_IO_TIM8_UP_TIM13 / 32);
        intPriorityLevel_up = (UINT8 *)(0xE000E400 + OS_IO_TIM8_UP_TIM13);
     #endif
     intSetEnable_cc = (UINT32 *)0xE000E100 + (UINT32)(OS_IO_TIM8_CC / 32);
     intPriorityLevel_cc = (UINT8 *)(0xE000E400 + OS_IO_TIM8_CC);
  #else
     intSetEnable = (UINT32 *)0xE000E100 + (UINT32)(ZOTTAOS_TIMER / 32);
     intPriorityLevel = (UINT8 *)(0xE000E400 + ZOTTAOS_TIMER);
  #endif
  /* */
  CLK_ENABLE |= CLK_ENABLE_BIT;                   // Enable the clock for timer
  TIM_AUTORELOAD = 0xFFFF;                        // Set the autoreload value
  TIM_PRESCALER = ZOTTAOS_TIMER_PRESCALER;        // Set the prescaler value
  TIM_EVENT_GENERATION = 1;                       // Generate an update event to reload
                                                  // the prescaler
  TIM_STATUS = (UINT16)~3;                        // Clear update flag
  TIM_INT_ENABLE |= 3;                            // Enable update interrupt

  /* Compute the IRQ priority */
  tmppriority = TIMER_PRIORITY << (PRIGROUP - 3);
  // le nombre 3 correspond au nombre maximum de bits pour la priorité moins le nombre de bit implémenté soit (7 -4 pour le stm32)
  tmppriority |=  TIMER_SUB_PRIORITY & (0x0F >> (7 - PRIGROUP));
  // (7 - PRIGROUP) correspond aux nombres de bits pour la priorité.
  #if ZOTTAOS_TIMER == OS_IO_TIM1 || ZOTTAOS_TIMER == OS_IO_TIM8
     *intPriorityLevel_up = tmppriority << 0x04;       // Set the IRQ priority
     *intPriorityLevel_cc = tmppriority << 0x04;       // Set the IRQ priority
     // (4 correpond à 8 moins le nombre de bit implémenter dans le STM32(4))
  #else
     *intPriorityLevel = tmppriority << 0x04;       // Set the IRQ priority
  #endif
  #if ZOTTAOS_TIMER == OS_IO_TIM1
     #if defined(STM32F103X4_X6) || defined(STM32F103T8_TB) || \
         defined(STM32F103C8_CB_R8_RB_V8_VB) || defined(STM32F103RC_RD_RE) || \
         defined(STM32F103VC_VD_VE_ZC_ZD_ZE) || defined(STM32F105XX) || defined(STM32F107XX)
        *intSetEnable_up |= 0x01 << (OS_IO_TIM1_UP % 32); // Enable the IRQ channels
     #elif defined(STM32F100X4_X6) || defined(STM32F100X8_XB) || \
           defined(STM32F100RC_RD_RE) || defined(STM32F100VC_VD_VE_ZC_ZD_ZE)
        *intSetEnable_up |= 0x01 << (OS_IO_TIM1_UP_TIM16 % 32); // Enable the IRQ channels
     #elif defined (STM32F101RF_RG)|| defined(STM32F101VF_VG_ZF_ZG) || \
           defined(STM32F103RF_RG) || defined(STM32F103VF_VG_ZF_ZG) || \
           defined(STM32F2XXXX) || defined(STM32F4XXXX)
        *intSetEnable_up |= 0x01 << (OS_IO_TIM1_UP_TIM10 % 32); // Enable the IRQ channels
     #endif
     *intSetEnable_cc |= 0x01 << (OS_IO_TIM1_CC % 32); // Enable the IRQ channels
  #elif ZOTTAOS_TIMER == OS_IO_TIM8
     #if defined(STM32F101RC_RD_RE) || defined(STM32F101VC_VD_VE_ZC_ZD_ZE) || \
         defined(STM32F103RC_RD_RE) || defined(STM32F103VC_VD_VE_ZC_ZD_ZE)
        *intSetEnable_up |= 0x01 << (OS_IO_TIM8_UP % 32); // Enable the IRQ channels
     #elif defined(STM32F101RF_RG) || defined(STM32F101VF_VG_ZF_ZG) || \
           defined(STM32F103RF_RG) || defined(STM32F103VF_VG_ZF_ZG) || \
           defined(STM32F2XXXX) || defined(STM32F4XXXX)
        *intSetEnable_up |= 0x01 << (OS_IO_TIM8_UP_TIM13 % 32); // Enable the IRQ channels
     #endif
     *intSetEnable_cc |= 0x01 << (OS_IO_TIM8_CC % 32); // Enable the IRQ channels
  #else
     intSetEnable = (UINT32 *)0xE000E100 + (UINT32)(ZOTTAOS_TIMER / 32);
  #endif
} /* end of _OSInitializeTimer */


/* _OSStartTimer: Starts the interval timer by setting it to up mode. This function is
** called only once when the kernel is ready to schedule the first application task. */
void _OSStartTimer(void)
{
  TIM_CONTROL1 |= 1;       // Enable the TIM Counter
  TIM_EVENT_GENERATION |= (UINT16)2; // Generate the first comparator interrupt
} /* end of _OSStartTimer */


/* _OSTimerShift: Shifts the global Time variable. This is a value greater than 2^16. */
void _OSTimerShift(INT32 shiftTimeLimit)
{
  Time -= shiftTimeLimit;
} /* end of _OSTimerShift */


/* OSGetActualTime: Retrieve the current time. Combines the 16 bits of the timer counter
** with the global variable Time to yield the current time. */
INT32 OSGetActualTime(void)
{
  INT32 currentTime, tmp;
  do {
     currentTime = Time;
     tmp = currentTime | TIM_COUNTER;
  } while (currentTime != Time);
  return tmp;
} /* end of OSGetActualTime */

#if ZOTTAOS_TIMER == OS_IO_TIM1 || ZOTTAOS_TIMER == OS_IO_TIM8
   /* _OSTimerHandler: Catches a STM-32 Timer interrupt and generates a software timer
   ** interrupt which is than carried out at a lower priority.
   ** Note: This function could have been written in assembler to reduce interrupt latencies. */
   void _OSTimerHandler_up(void)
   {
     TIM_COMPARATOR = 0; // Disable timer comparator
     TIM_STATUS &= ~1;   // Clear interrupt flag
     Time += 0x10000;    // Increment most significant word of Time
     _OSGenerateSoftTimerInterrupt();
   } /* end of _OSTimerHandler_up */


   /* _OSTimerHandler: Catches a STM-32 Timer interrupt and generates a software timer
   ** interrupt which is than carried out at a lower priority.
   ** Note: This function could have been written in assembler to reduce interrupt latencies. */
   void _OSTimerHandler_cc(void)
   {
     TIM_COMPARATOR = 0; // Disable timer comparator
     TIM_STATUS &= ~2;   // Clear interrupt flag
     _OSGenerateSoftTimerInterrupt();
   } /* end of _OSTimerHandler_cc */


#else
   /* _OSTimerHandler: Catches a STM-32 Timer interrupt and generates a software timer
   ** interrupt which is than carried out at a lower priority.
   ** Note: This function could have been written in assembler to reduce interrupt latencies. */
   void _OSTimerHandler(void)
   {
     TIM_COMPARATOR = 0; // Disable timer comparator
     /* Test interrupt source */
     if (TIM_STATUS & 2)    // Is comparator interrupt pending?
        TIM_STATUS &= ~2;   // Clear interrupt flag
     if (TIM_STATUS & 1) {  // Is timer overflow interrupt?
        TIM_STATUS &= ~1;   // Clear interrupt flag
        Time += 0x10000;    // Increment most significant word of Time
     }
     _OSGenerateSoftTimerInterrupt();
   } /* end of _OSTimerHandler */
#endif


/* Because the timer continues ticking, when we wish set a new value for the timer compa-
** rator, the difference in time between the new value and the previous must be such that
** when the assignment is done, the timer has not passed the comparator value. This is
** guaranteed by TIMER_OFFSET. */
/* The number of core cycles to considered in order to obtain TIMER_OFFSET are given in
** function _OSSetTimerComparator, and comprised between markers START_TIMER_OFFSET and
** END_TIMER_OFFSET. */
#define TIMER_OFFSET 5

#define INFINITY32 0x0000FFFF
#define INFINITY16 0xFFFF
#define INFINITY32_OFFSET 0x0000FFFA // = INFINITY32 - TIMER_OFFSET

/* _OSSetTimer: Sets the timer comparator to the next time event interval. This function
 * is called by the software timer interrupt handler when it finishes processing the cur-
 * rent interrupt and prepares its next interrupt. */
void _OSSetTimer(INT32 nextArrival)
{
  INT32 time32;
  UINT16 time16;
  while (TRUE) {
     OSUINT16_LL(&TIM_COMPARATOR);
     if ((time32 =  nextArrival - Time) < INFINITY32_OFFSET) {
         if (time32 > 0) {
           time16 = (UINT16)time32;
           if (time16 > TIMER_OFFSET) {
              if (TIM_COUNTER > time16 - TIMER_OFFSET) {       // START_TIMER_OFFSET
                 if (time16 > INFINITY16 - TIMER_OFFSET)
                    break;
                 else
                    time16 = TIM_COUNTER + TIMER_OFFSET;
              }
           }
           else
              time16 = TIM_COUNTER + TIMER_OFFSET;
        }
        else
           break;
        if (OSUINT16_SC(&TIM_COMPARATOR,time16))              // END_TIMER_OFFSET
           break;
        /* If we get here, the task has been interrupted by a source different than
        ** the interval timer. */
     }
     else
        break;
  }
} /* end of _OSSetTimer */
