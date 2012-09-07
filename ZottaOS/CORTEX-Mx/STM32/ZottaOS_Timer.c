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
/* File ZottaOS_Timer.c: Hardware abstract timer layer. This file holds all timer related
**            functions needed by the ZottaOS family of kernels so that these can easily
**            be ported from one microcontroller to another.
** Platform version: All STM32 microcontrollers.
** Version date: March 2012
** Authors: MIS-TIC
*/

#include "ZottaOS_CortexMx.h"
#include "ZottaOS.h"
#include "ZottaOS_Timer.h"


/* Define 16-bit or 32-bit timer */
#if defined(STM32F05XXX) && ZOTTAOS_TIMER == OS_IO_TIM2 || \
    defined(STM32L1XXXX) && ZOTTAOS_TIMER == OS_IO_TIM5 || \
    defined(STM32F2XXXX) && ZOTTAOS_TIMER == OS_IO_TIM2 || \
    defined(STM32F2XXXX) && ZOTTAOS_TIMER == OS_IO_TIM5 || \
    defined(STM32F4XXXX) && ZOTTAOS_TIMER == OS_IO_TIM2 || \
    defined(STM32F4XXXX) && ZOTTAOS_TIMER == OS_IO_TIM5
   #define ZOTTAOS_TIMER_32
#else
   #define ZOTTAOS_TIMER_16
#endif


/* Definitions of hardware registers */
#ifdef ZOTTAOS_TIMER
   #if defined(STM32F05XXX)
      #if ZOTTAOS_TIMER == OS_IO_TIM1
         #define TIME_BASE 0x40012C00
      #elif ZOTTAOS_TIMER == OS_IO_TIM2
         #define TIME_BASE 0x40000000
      #elif ZOTTAOS_TIMER == OS_IO_TIM3
         #define TIME_BASE 0x40000400
      #elif ZOTTAOS_TIMER == OS_IO_TIM14
         #define TIME_BASE 0x40002000
      #elif ZOTTAOS_TIMER == OS_IO_TIM15
         #define TIME_BASE 0x40014000
      #elif ZOTTAOS_TIMER == OS_IO_TIM16
         #define TIME_BASE 0x40014400
      #elif ZOTTAOS_TIMER == OS_IO_TIM17
         #define TIME_BASE 0x40014800
      #else
         #error Selected timer does not exist! (verify ZOTTAOS_TIMER defined in ZottaOS_Config.h)
      #endif
   #elif defined(STM32L1XXXX)
      #if ZOTTAOS_TIMER == OS_IO_TIM2
         #define TIME_BASE 0x40000000
      #elif ZOTTAOS_TIMER == OS_IO_TIM3
         #define TIME_BASE 0x40000400
      #elif ZOTTAOS_TIMER == OS_IO_TIM4
         #define TIME_BASE 0x40000800
      #elif ZOTTAOS_TIMER == OS_IO_TIM5
         #define TIME_BASE 0x40000C00
      #elif ZOTTAOS_TIMER == OS_IO_TIM9
         #define TIME_BASE 0x40010800
      #elif ZOTTAOS_TIMER == OS_IO_TIM10
         #define TIME_BASE 0x40010C00
      #elif ZOTTAOS_TIMER == OS_IO_TIM11
         #define TIME_BASE 0x40011000
      #else
         #error Selected timer does not exist! (verify ZOTTAOS_TIMER defined in ZottaOS_Config.h)
      #endif
   #elif defined(STM32F1XXXX)
      #if ZOTTAOS_TIMER == OS_IO_TIM1
         #define TIME_BASE 0x40012C00
      #elif ZOTTAOS_TIMER == OS_IO_TIM2
         #define TIME_BASE 0x40000000
      #elif ZOTTAOS_TIMER == OS_IO_TIM3
         #define TIME_BASE 0x40000400
      #elif ZOTTAOS_TIMER == OS_IO_TIM4
         #define TIME_BASE 0x40000800
      #elif ZOTTAOS_TIMER == OS_IO_TIM5
         #define TIME_BASE 0x40000C00
      #elif ZOTTAOS_TIMER == OS_IO_TIM8
         #define TIME_BASE 0x40013400
      #elif ZOTTAOS_TIMER == OS_IO_TIM9
         #define TIME_BASE 0x40014C00
      #elif ZOTTAOS_TIMER == OS_IO_TIM10
         #define TIME_BASE 0x40015000
      #elif ZOTTAOS_TIMER == OS_IO_TIM11
         #define TIME_BASE 0x40015400
      #elif ZOTTAOS_TIMER == OS_IO_TIM12
         #define TIME_BASE 0x40001800
      #elif ZOTTAOS_TIMER == OS_IO_TIM13
         #define TIME_BASE 0x40001C00
      #elif ZOTTAOS_TIMER == OS_IO_TIM14
         #define TIME_BASE 0x40002000
      #elif ZOTTAOS_TIMER == OS_IO_TIM15
         #define TIME_BASE 0x40014000
      #elif ZOTTAOS_TIMER == OS_IO_TIM16
         #define TIME_BASE 0x40014400
      #elif ZOTTAOS_TIMER == OS_IO_TIM17
         #define TIME_BASE 0x40014800
      #else
         #error Selected timer does not exist! (verify ZOTTAOS_TIMER defined in ZottaOS_Config.h)
      #endif
   #elif defined(STM32F2XXXX) || defined(STM32F4XXXX)
      #if ZOTTAOS_TIMER == OS_IO_TIM1
         #define TIME_BASE 0x40010000
      #elif ZOTTAOS_TIMER == OS_IO_TIM2
         #define TIME_BASE 0x40000000
      #elif ZOTTAOS_TIMER == OS_IO_TIM3
         #define TIME_BASE 0x40000400
      #elif ZOTTAOS_TIMER == OS_IO_TIM4
         #define TIME_BASE 0x40000800
      #elif ZOTTAOS_TIMER == OS_IO_TIM5
         #define TIME_BASE 0x40000C00
      #elif ZOTTAOS_TIMER == OS_IO_TIM8
         #define TIME_BASE 0x40010400
      #elif ZOTTAOS_TIMER == OS_IO_TIM9
         #define TIME_BASE 0x40014000
      #elif ZOTTAOS_TIMER == OS_IO_TIM10
         #define TIME_BASE 0x40014400
      #elif ZOTTAOS_TIMER == OS_IO_TIM11
         #define TIME_BASE 0x40014800
      #elif ZOTTAOS_TIMER == OS_IO_TIM12
         #define TIME_BASE 0x40001800
      #elif ZOTTAOS_TIMER == OS_IO_TIM13
         #define TIME_BASE 0x40001C00
      #elif ZOTTAOS_TIMER == OS_IO_TIM14
         #define TIME_BASE 0x40002000
      #else
         #error Selected timer does not exist! (verify ZOTTAOS_TIMER defined in ZottaOS_Config.h)
      #endif
   #endif
#else
   #error You must select a timer for ZottaOS! (define ZOTTAOS_TIMER in ZottaOS_Config.h)
#endif


#if defined(STM32F05XXX)
   #if ZOTTAOS_TIMER == OS_IO_TIM1 || ZOTTAOS_TIMER ==  OS_IO_TIM15 || \
       ZOTTAOS_TIMER == OS_IO_TIM16 || ZOTTAOS_TIMER == OS_IO_TIM17
      #define CLK_ENABLE *((UINT32 *)0x40021018) // RCC_APB2ENR
      #if ZOTTAOS_TIMER == OS_IO_TIM1
         #define CLK_ENABLE_BIT 0x0800
      #elif ZOTTAOS_TIMER == OS_IO_TIM15
         #define CLK_ENABLE_BIT 0x10000
      #elif ZOTTAOS_TIMER ==  OS_IO_TIM16
         #define CLK_ENABLE_BIT 0x20000
      #elif ZOTTAOS_TIMER == OS_IO_TIM17
         #define CLK_ENABLE_BIT 0x40000
      #endif
   #elif ZOTTAOS_TIMER ==  OS_IO_TIM2 || ZOTTAOS_TIMER == OS_IO_TIM3 || \
         ZOTTAOS_TIMER == OS_IO_TIM14
      #define CLK_ENABLE *((UINT32 *)0x4002101C) // RCC_APB1ENR
      #if ZOTTAOS_TIMER ==  OS_IO_TIM2
         #define CLK_ENABLE_BIT 0x1
      #elif ZOTTAOS_TIMER == OS_IO_TIM3
         #define CLK_ENABLE_BIT 0x2
      #elif ZOTTAOS_TIMER == OS_IO_TIM14
         #define CLK_ENABLE_BIT 0x100
      #endif
   #endif
#elif defined(STM32L1XXXX)
   #if ZOTTAOS_TIMER == OS_IO_TIM9 || ZOTTAOS_TIMER ==  OS_IO_TIM10 || \
       ZOTTAOS_TIMER == OS_IO_TIM11
      #define CLK_ENABLE *((UINT32 *)0x40023820) // RCC_APB2ENR
      #if ZOTTAOS_TIMER == OS_IO_TIM9
         #define CLK_ENABLE_BIT 0x4
      #elif ZOTTAOS_TIMER ==  OS_IO_TIM10
         #define CLK_ENABLE_BIT 0x8
      #elif ZOTTAOS_TIMER == OS_IO_TIM11
         #define CLK_ENABLE_BIT 0x10
      #endif
   #elif ZOTTAOS_TIMER ==  OS_IO_TIM2 || ZOTTAOS_TIMER == OS_IO_TIM3 || \
         ZOTTAOS_TIMER == OS_IO_TIM4 || ZOTTAOS_TIMER == OS_IO_TIM5
      #define CLK_ENABLE *((UINT32 *)0x40023824) // RCC_APB1ENR
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
      #define CLK_ENABLE *((UINT32 *)0x40021018) // RCC_APB2ENR
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
      #define CLK_ENABLE *((UINT32 *)0x40023844) // RCC_APB2ENR
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
      #define CLK_ENABLE *((UINT32 *)0x40023840) // RCC_APB1ENR
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


/* Memory mapped timer registers relative to the base address of the timer */
#define TIM_CONTROL1         *((UINT16 *)(TIME_BASE + 0x00))
#define TIM_INT_ENABLE       *((UINT16 *)(TIME_BASE + 0x0C))
#define TIM_STATUS           *((UINT16 *)(TIME_BASE + 0x10))
#define TIM_EVENT_GENERATION *((UINT16 *)(TIME_BASE + 0x14))
#ifdef ZOTTAOS_TIMER_32
   #define TIM_COUNTER       *((UINT32 *)(TIME_BASE + 0x24))
#elif defined(ZOTTAOS_TIMER_16)
   #define TIM_COUNTER       *((UINT16 *)(TIME_BASE + 0x24))
#endif
#define TIM_PRESCALER        *((UINT16 *)(TIME_BASE + 0x28))
#ifdef ZOTTAOS_TIMER_32
   #define TIM_AUTORELOAD    *((UINT32 *)(TIME_BASE + 0x2C))
   #define TIM_COMPARATOR    *((UINT32 *)(TIME_BASE + 0x34))
#elif defined(ZOTTAOS_TIMER_16)
   #define TIM_AUTORELOAD    *((UINT16 *)(TIME_BASE + 0x2C))
   #define TIM_COMPARATOR    *((UINT16 *)(TIME_BASE + 0x34))
#endif

#define UPDATE_INT_BIT      0x1
#define COMPARATOR_INT_BIT  0x2

/* Minimal interrupt descriptor to call an interrupt handler */
typedef struct TIMER_ISR_DATA {
  void (*TimerIntHandler)(struct TIMER_ISR_DATA *);
} TIMER_ISR_DATA;

#if ZOTTAOS_TIMER == OS_IO_TIM1 || ZOTTAOS_TIMER == OS_IO_TIM8
   static void TimerHandler_CC(struct TIMER_ISR_DATA *descriptor); // Interrupt handler for comparator interrupt
   static void TimerHandler_Up(struct TIMER_ISR_DATA *descriptor); // Interrupt handler for overflow
#else
   static void TimerHandler(struct TIMER_ISR_DATA *descriptor); // Interrupt handler for comparator and overflow interrupt
#endif


#ifdef ZOTTAOS_TIMER_16
   /* System wall clock. This variable stores the most-significant 16 bits of the current
   ** time. The lower 16 bits are directly taken from the timer counter register. To get
   ** the current time, use _OSGetActualTime. */
   static volatile INT16 Time;
#endif

/* Timer interrupt cause marked by the ISR and that is used by the lower priority handler
** ( _OSTimerInterruptHandler) to process the interrupt. */
volatile BOOL _OSOverflowInterruptFlag = FALSE;
volatile BOOL _OSComparatorInterruptFlag = FALSE;

/* _OSInitializeTimer: Initializes the timer which starts counting as soon as ZottaOS is
** ready to process the first arrival. When the kernel, i.e. when OSStartMultitasking()
** is called, the last operation that is done is to set the timer handler and then start
** the idle task which is the only ready task in the system at that time. The first time
** the idle task executes, it calls _OSStartTimer().
** After _OSInitializeTimer() is called the timer's input divider is selected but it is
** halted. */
void _OSInitializeTimer(void)
{
  #define IRQ_SET_ENABLE_REGISTER 0xE000E100 // 0xE000E100 to 0xE000E11C (see CortexM3_TRM)
  #define IRQ_PRIORITY_REGISTER   0xE000E400 // 0xE000E400 to 0xE000E41F (see CortexM3_TRM)
  TIMER_ISR_DATA *device;
  #if defined(CORTEX_M3) || defined(CORTEX_M4)
     UINT8 tmpPriority;
     UINT32 *intSetEnable;
  #endif
  UINT8 *intPriorityLevel;
  CLK_ENABLE |= CLK_ENABLE_BIT;            // Enable the clock for timer
  #ifdef ZOTTAOS_TIMER_32
     TIM_AUTORELOAD = 0x3FFFFFFF;          // Set the autoreload value (2^30 - 1)
  #elif defined(ZOTTAOS_TIMER_16)
     TIM_AUTORELOAD = 0xFFFF;              // Set the autoreload value (2^16 - 1)
  #endif
  /* Set the timer clock prescaler to ZOTTAOS_TIMER_PRESCALER. This value is defined in
  ** file ZottaOS_Config.h. */
  TIM_PRESCALER = ZOTTAOS_TIMER_PRESCALER;
  TIM_EVENT_GENERATION = UPDATE_INT_BIT;   // Generate an update event to reload the prescaler
  TIM_STATUS = ~(UPDATE_INT_BIT | COMPARATOR_INT_BIT);   // Clear update flag
  TIM_INT_ENABLE |= UPDATE_INT_BIT | COMPARATOR_INT_BIT; // Enable update interrupt
  /* Initialize Cortex-Mx Nested Vectored Interrupt Controller */
  #if defined(CORTEX_M3) || defined(CORTEX_M4)
     /* Compute the priority (Only 4 bits are used for priority on STM32). PRIGROUP de-
     ** fines the interrupt priority levels of the microcontroller (see ZottaOS_Config.h),
     ** where 7-PRIGROUP is the number priority bits, and PRIGROUP-3 is the number of
     ** sub-priority bits */
     tmpPriority = TIMER_PRIORITY << (PRIGROUP - 3);
     tmpPriority |=  TIMER_SUB_PRIORITY & (0x0F >> (7 - PRIGROUP));
  #endif
  /* Set the IRQ priority */
  intPriorityLevel = (UINT8 *)(IRQ_PRIORITY_REGISTER + ZOTTAOS_TIMER);
  #if defined(CORTEX_M3) || defined(CORTEX_M4)
     *intPriorityLevel = tmpPriority << 4; // Only 4 MSB bits are used on STM32
  #elif defined(CORTEX_M0)
     *intPriorityLevel = TIMER_PRIORITY;
  #endif
  /* Enable the IRQ channels */
  #if defined(CORTEX_M3) || defined(CORTEX_M4)
     intSetEnable = (UINT32 *)IRQ_SET_ENABLE_REGISTER + (UINT32)(ZOTTAOS_TIMER / 32);
     *intSetEnable |= 0x01 << (ZOTTAOS_TIMER % 32);
  #elif defined(CORTEX_M0)
     *(UINT32 *)IRQ_SET_ENABLE_REGISTER |= 0x01 << ZOTTAOS_TIMER;
  #endif
  /* Initialize ZottaOS internal interrupt structure */
  #if ZOTTAOS_TIMER == OS_IO_TIM1
     /* Comparator interrupt */
     device = (TIMER_ISR_DATA *)OSMalloc(sizeof(TIMER_ISR_DATA));
     device->TimerIntHandler = TimerHandler_CC;
     OSSetISRDescriptor(OS_IO_TIM1_CC,device);
     /* Update interrupt */
     device = (TIMER_ISR_DATA *)OSMalloc(sizeof(TIMER_ISR_DATA));
     device->TimerIntHandler = TimerHandler_Up;
     OSSetISRDescriptor(OS_IO_TIM1_UP,device);
     /* Set the IRQ priority */
     intPriorityLevel = (UINT8 *)(IRQ_PRIORITY_REGISTER + OS_IO_TIM1_UP);
     #if defined(CORTEX_M3) || defined(CORTEX_M4)
        *intPriorityLevel = tmpPriority << 4; // Only 4 MSB bits are used on STM32
     #elif defined(CORTEX_M0)
        *intPriorityLevel = priority;
     #endif
     /* Enable the IRQ channels */
     #if defined(CORTEX_M3) || defined(CORTEX_M4)
        intSetEnable = (UINT32 *)IRQ_SET_ENABLE_REGISTER + (UINT32)(OS_IO_TIM1_UP / 32);
        *intSetEnable |= 0x01 << (OS_IO_TIM1_UP % 32);
     #elif defined(CORTEX_M0)
        *(UINT32 *)IRQ_SET_ENABLE_REGISTER |= 0x01 << OS_IO_TIM1_UP;
     #endif
  #elif ZOTTAOS_TIMER == OS_IO_TIM2 || ZOTTAOS_TIMER == OS_IO_TIM3 || \
        ZOTTAOS_TIMER == OS_IO_TIM4 || ZOTTAOS_TIMER == OS_IO_TIM5 || \
        ZOTTAOS_TIMER == OS_IO_TIM9 || ZOTTAOS_TIMER == OS_IO_TIM10 || \
        ZOTTAOS_TIMER == OS_IO_TIM11 || ZOTTAOS_TIMER == OS_IO_TIM12 || \
        ZOTTAOS_TIMER == OS_IO_TIM13 || ZOTTAOS_TIMER == OS_IO_TIM14 || \
        ZOTTAOS_TIMER == OS_IO_TIM15 || ZOTTAOS_TIMER == OS_IO_TIM16 || \
        ZOTTAOS_TIMER == OS_IO_TIM17
     device = (TIMER_ISR_DATA *)OSMalloc(sizeof(TIMER_ISR_DATA));
     device->TimerIntHandler = TimerHandler;
     OSSetISRDescriptor(ZOTTAOS_TIMER,device);
  #elif ZOTTAOS_TIMER == OS_IO_TIM8
     /* Comparator interrupt */
     device = (TIMER_ISR_DATA *)OSMalloc(sizeof(TIMER_ISR_DATA));
     device->TimerIntHandler = TimerHandler_CC;
     OSSetISRDescriptor(OS_IO_TIM8_CC,device);
     /* Update interrupt */
     device = (TIMER_ISR_DATA *)OSMalloc(sizeof(TIMER_ISR_DATA));
     device->TimerIntHandler = TimerHandler_Up;
     OSSetISRDescriptor(OS_IO_TIM8_UP,device);
     /* Set the IRQ priority */
     intPriorityLevel = (UINT8 *)(IRQ_PRIORITY_REGISTER + OS_IO_TIM8_UP);
     #if defined(CORTEX_M3) || defined(CORTEX_M4)
        *intPriorityLevel = tmpPriority << 4; // Only 4 MSB bits are used on STM32
     #elif defined(CORTEX_M0)
        *intPriorityLevel = priority;
     #endif
     /* Enable the IRQ channels */
     #if defined(CORTEX_M3) || defined(CORTEX_M4)
        intSetEnable = (UINT32 *)IRQ_SET_ENABLE_REGISTER + (UINT32)(OS_IO_TIM8_UP / 32);
        *intSetEnable |= 0x01 << (OS_IO_TIM1_UP % 32);
     #elif defined(CORTEX_M0)
        *(UINT32 *)IRQ_SET_ENABLE_REGISTER |= 0x01 << OS_IO_TIM8_UP;
     #endif
  #endif
} /* end of _OSInitializeTimer */


/* _OSStartTimer: Starts the interval timer by setting it to up mode. This function is
** called only once when the kernel is ready to schedule the first application task. */
void _OSStartTimer(void)
{
  TIM_CONTROL1 |= 1;                          // Enable the TIM Counter
  TIM_EVENT_GENERATION |= COMPARATOR_INT_BIT; // Generate the first comparator interrupt
} /* end of _OSStartTimer */


/* _TimerHandler: Catches STM-32 Timer interrupts and generates a software timer interrupt
** which is then carried out at a lower priority.
** Note: This function could have been written in assembler to reduce interrupt latencies.
** Note: Timers 1 and 8 are have their overflow and comparator match interrupts bound to
** 2 different IRQs, and are therefore considered separately. */
#if ZOTTAOS_TIMER == OS_IO_TIM1 || ZOTTAOS_TIMER == OS_IO_TIM8
   void TimerHandler_Up(struct TIMER_ISR_DATA *descriptor)
   { // Timer overflow ISR
     TIM_STATUS &= ~UPDATE_INT_BIT; // Clear interrupt flag
     #ifdef ZOTTAOS_TIMER_16
        Time += 1;                  // Increment most significant word of Time
     #endif
     _OSOverflowInterruptFlag = TRUE;
     _OSGenerateSoftTimerInterrupt();
   } /* end of TimerHandler_Up */

   void TimerHandler_CC(struct TIMER_ISR_DATA *descriptor)
   { // Comparator match ISR
     TIM_STATUS &= ~COMPARATOR_INT_BIT; // Clear interrupt flag
     TIM_COMPARATOR = 0;                // Disable timer comparator
     _OSComparatorInterruptFlag = TRUE;
     _OSGenerateSoftTimerInterrupt();
   } /* end of TimerHandler_CC */
#else
   void TimerHandler(struct TIMER_ISR_DATA *descriptor)
   {
     if (TIM_STATUS & UPDATE_INT_BIT) {    // Is timer overflow interrupt?
        TIM_STATUS &= ~UPDATE_INT_BIT;     // Clear interrupt flag
        #ifdef ZOTTAOS_TIMER_16
           Time += 1;       // Increment most significant word of Time
        #endif
        _OSOverflowInterruptFlag = TRUE;
        _OSGenerateSoftTimerInterrupt();
     }
     if (TIM_STATUS & COMPARATOR_INT_BIT) { // Is comparator interrupt pending?
        TIM_STATUS &= ~COMPARATOR_INT_BIT;  // Clear interrupt flag
        TIM_COMPARATOR = 0; // Disable timer comparator
        _OSComparatorInterruptFlag = TRUE;
        _OSGenerateSoftTimerInterrupt();
     }
   } /* end of TimerHandler */
#endif


/* _OSSetTimer: Sets the timer comparator to the next time event interval. This function
 * is called by the software timer interrupt handler when it finishes processing the cur-
 * rent interrupt and prepares its next interrupt. */
BOOL _OSSetTimer(INT32 nextArrival)
{
  #ifdef ZOTTAOS_TIMER_32
     TIM_COMPARATOR = nextArrival;
     if (TIM_COMPARATOR > TIM_COUNTER)
        return TRUE;
     TIM_COMPARATOR = 0;
     return FALSE;
  #elif defined(ZOTTAOS_TIMER_16)
     if (nextArrival >> 16 == Time) {
        TIM_COMPARATOR = (UINT16)nextArrival;
        if (TIM_COMPARATOR > TIM_COUNTER)
           return TRUE;
        TIM_COMPARATOR = 0;
        return FALSE;
     }
     else
        return TRUE;
#endif
} /* end of _OSSetTimer */


/* _OSTimerIsOverflow: Returns true if the increment to internal system wall clock over-
** flows and all time related values should be shifted. */
BOOL _OSTimerIsOverflow(INT32 shiftTimeLimit)
{
  #ifdef ZOTTAOS_TIMER_32
     if (_OSOverflowInterruptFlag) {
        _OSOverflowInterruptFlag = FALSE;
        return TRUE;
     }
     else
        return FALSE;
  #elif defined(ZOTTAOS_TIMER_16)
     INT16 tmp, time;
     if (_OSOverflowInterruptFlag) {
        _OSOverflowInterruptFlag = FALSE;
        if ((tmp = shiftTimeLimit >> 16) < Time) {
           do {
              time = OSINT16_LL((INT16 *)&Time);
              time -= tmp;
           } while (!OSINT16_SC((INT16 *)&Time,time));
           return TRUE;
        }
     }
     return FALSE;
  #endif
} /* end of _OSTimerIsOverflow */


/* _OSGetActualTime: Retrieves the current time. When ZottaOS' interval timer is confi-
** gured for 16-bit timer, this function combines the 16 bits of the timer counter with
** the global variable Time to yield the current time. This function should never be
** called from an ISR having higher priority than ZOTTAOS_TIMER_16. */
INT32 _OSGetActualTime(void)
{
  #ifdef ZOTTAOS_TIMER_32
     return TIM_COUNTER;
  #elif defined(ZOTTAOS_TIMER_16)
     INT16 currentTime;
     INT32 tmp;
     do {
        currentTime = Time;
        tmp = (INT32)currentTime << 16 | TIM_COUNTER;
     } while (currentTime != Time);
     return tmp;
  #endif
} /* end of _OSGetActualTime */
