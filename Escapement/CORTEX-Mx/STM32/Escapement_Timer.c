/* Copyright (c) 2026 Bertrand Hurst. All rights reserved.
** Escapement - Lightweight Power-Aware Real-Time OS.
** Derived from prior work; see LICENSE and NOTICE at the root of this repository.
*/
/* File Escapement_Timer.c: Hardware abstract timer layer. This file holds all timer related
**            functions needed by the Escapement family of kernels so that these can easily
**            be ported from one microcontroller to another.
** Platform version: All STM32 microcontrollers.
** Version date: March 2012
*/

#include "Escapement_CortexMx.h"
#include "Escapement.h"
#include "Escapement_Timer.h"


/* Define 16-bit or 32-bit timer */
#if defined(STM32F05XXX) && ESCAPEMENT_TIMER == OS_IO_TIM2 || \
    defined(STM32L1XXXX) && ESCAPEMENT_TIMER == OS_IO_TIM5 || \
    defined(STM32F2XXXX) && ESCAPEMENT_TIMER == OS_IO_TIM2 || \
    defined(STM32F2XXXX) && ESCAPEMENT_TIMER == OS_IO_TIM5 || \
    defined(STM32F4XXXX) && ESCAPEMENT_TIMER == OS_IO_TIM2 || \
    defined(STM32F4XXXX) && ESCAPEMENT_TIMER == OS_IO_TIM5
   #define ESCAPEMENT_TIMER_32
#else
   #define ESCAPEMENT_TIMER_16
#endif


/* Definitions of hardware registers */
#ifdef ESCAPEMENT_TIMER
   #if defined(STM32F05XXX)
      #if ESCAPEMENT_TIMER == OS_IO_TIM1
         #define TIME_BASE 0x40012C00
      #elif ESCAPEMENT_TIMER == OS_IO_TIM2
         #define TIME_BASE 0x40000000
      #elif ESCAPEMENT_TIMER == OS_IO_TIM3
         #define TIME_BASE 0x40000400
      #elif ESCAPEMENT_TIMER == OS_IO_TIM14
         #define TIME_BASE 0x40002000
      #elif ESCAPEMENT_TIMER == OS_IO_TIM15
         #define TIME_BASE 0x40014000
      #elif ESCAPEMENT_TIMER == OS_IO_TIM16
         #define TIME_BASE 0x40014400
      #elif ESCAPEMENT_TIMER == OS_IO_TIM17
         #define TIME_BASE 0x40014800
      #else
         #error Selected timer does not exist! (verify ESCAPEMENT_TIMER defined in Escapement_Config.h)
      #endif
   #elif defined(STM32L1XXXX)
      #if ESCAPEMENT_TIMER == OS_IO_TIM2
         #define TIME_BASE 0x40000000
      #elif ESCAPEMENT_TIMER == OS_IO_TIM3
         #define TIME_BASE 0x40000400
      #elif ESCAPEMENT_TIMER == OS_IO_TIM4
         #define TIME_BASE 0x40000800
      #elif ESCAPEMENT_TIMER == OS_IO_TIM5
         #define TIME_BASE 0x40000C00
      #elif ESCAPEMENT_TIMER == OS_IO_TIM9
         #define TIME_BASE 0x40010800
      #elif ESCAPEMENT_TIMER == OS_IO_TIM10
         #define TIME_BASE 0x40010C00
      #elif ESCAPEMENT_TIMER == OS_IO_TIM11
         #define TIME_BASE 0x40011000
      #else
         #error Selected timer does not exist! (verify ESCAPEMENT_TIMER defined in Escapement_Config.h)
      #endif
   #elif defined(STM32F1XXXX)
      #if ESCAPEMENT_TIMER == OS_IO_TIM1
         #define TIME_BASE 0x40012C00
      #elif ESCAPEMENT_TIMER == OS_IO_TIM2
         #define TIME_BASE 0x40000000
      #elif ESCAPEMENT_TIMER == OS_IO_TIM3
         #define TIME_BASE 0x40000400
      #elif ESCAPEMENT_TIMER == OS_IO_TIM4
         #define TIME_BASE 0x40000800
      #elif ESCAPEMENT_TIMER == OS_IO_TIM5
         #define TIME_BASE 0x40000C00
      #elif ESCAPEMENT_TIMER == OS_IO_TIM8
         #define TIME_BASE 0x40013400
      #elif ESCAPEMENT_TIMER == OS_IO_TIM9
         #define TIME_BASE 0x40014C00
      #elif ESCAPEMENT_TIMER == OS_IO_TIM10
         #define TIME_BASE 0x40015000
      #elif ESCAPEMENT_TIMER == OS_IO_TIM11
         #define TIME_BASE 0x40015400
      #elif ESCAPEMENT_TIMER == OS_IO_TIM12
         #define TIME_BASE 0x40001800
      #elif ESCAPEMENT_TIMER == OS_IO_TIM13
         #define TIME_BASE 0x40001C00
      #elif ESCAPEMENT_TIMER == OS_IO_TIM14
         #define TIME_BASE 0x40002000
      #elif ESCAPEMENT_TIMER == OS_IO_TIM15
         #define TIME_BASE 0x40014000
      #elif ESCAPEMENT_TIMER == OS_IO_TIM16
         #define TIME_BASE 0x40014400
      #elif ESCAPEMENT_TIMER == OS_IO_TIM17
         #define TIME_BASE 0x40014800
      #else
         #error Selected timer does not exist! (verify ESCAPEMENT_TIMER defined in Escapement_Config.h)
      #endif
   #elif defined(STM32F2XXXX) || defined(STM32F4XXXX)
      #if ESCAPEMENT_TIMER == OS_IO_TIM1
         #define TIME_BASE 0x40010000
      #elif ESCAPEMENT_TIMER == OS_IO_TIM2
         #define TIME_BASE 0x40000000
      #elif ESCAPEMENT_TIMER == OS_IO_TIM3
         #define TIME_BASE 0x40000400
      #elif ESCAPEMENT_TIMER == OS_IO_TIM4
         #define TIME_BASE 0x40000800
      #elif ESCAPEMENT_TIMER == OS_IO_TIM5
         #define TIME_BASE 0x40000C00
      #elif ESCAPEMENT_TIMER == OS_IO_TIM8
         #define TIME_BASE 0x40010400
      #elif ESCAPEMENT_TIMER == OS_IO_TIM9
         #define TIME_BASE 0x40014000
      #elif ESCAPEMENT_TIMER == OS_IO_TIM10
         #define TIME_BASE 0x40014400
      #elif ESCAPEMENT_TIMER == OS_IO_TIM11
         #define TIME_BASE 0x40014800
      #elif ESCAPEMENT_TIMER == OS_IO_TIM12
         #define TIME_BASE 0x40001800
      #elif ESCAPEMENT_TIMER == OS_IO_TIM13
         #define TIME_BASE 0x40001C00
      #elif ESCAPEMENT_TIMER == OS_IO_TIM14
         #define TIME_BASE 0x40002000
      #else
         #error Selected timer does not exist! (verify ESCAPEMENT_TIMER defined in Escapement_Config.h)
      #endif
   #endif
#else
   #error You must select a timer for Escapement! (define ESCAPEMENT_TIMER in Escapement_Config.h)
#endif


#if defined(STM32F05XXX)
   #if ESCAPEMENT_TIMER == OS_IO_TIM1 || ESCAPEMENT_TIMER ==  OS_IO_TIM15 || \
       ESCAPEMENT_TIMER == OS_IO_TIM16 || ESCAPEMENT_TIMER == OS_IO_TIM17
      #define CLK_ENABLE *((volatile UINT32 *)0x40021018) // RCC_APB2ENR
      #if ESCAPEMENT_TIMER == OS_IO_TIM1
         #define CLK_ENABLE_BIT 0x0800
      #elif ESCAPEMENT_TIMER == OS_IO_TIM15
         #define CLK_ENABLE_BIT 0x10000
      #elif ESCAPEMENT_TIMER ==  OS_IO_TIM16
         #define CLK_ENABLE_BIT 0x20000
      #elif ESCAPEMENT_TIMER == OS_IO_TIM17
         #define CLK_ENABLE_BIT 0x40000
      #endif
   #elif ESCAPEMENT_TIMER ==  OS_IO_TIM2 || ESCAPEMENT_TIMER == OS_IO_TIM3 || \
         ESCAPEMENT_TIMER == OS_IO_TIM14
      #define CLK_ENABLE *((volatile UINT32 *)0x4002101C) // RCC_APB1ENR
      #if ESCAPEMENT_TIMER ==  OS_IO_TIM2
         #define CLK_ENABLE_BIT 0x1
      #elif ESCAPEMENT_TIMER == OS_IO_TIM3
         #define CLK_ENABLE_BIT 0x2
      #elif ESCAPEMENT_TIMER == OS_IO_TIM14
         #define CLK_ENABLE_BIT 0x100
      #endif
   #endif
#elif defined(STM32L1XXXX)
   #if ESCAPEMENT_TIMER == OS_IO_TIM9 || ESCAPEMENT_TIMER ==  OS_IO_TIM10 || \
       ESCAPEMENT_TIMER == OS_IO_TIM11
      #define CLK_ENABLE *((volatile UINT32 *)0x40023820) // RCC_APB2ENR
      #if ESCAPEMENT_TIMER == OS_IO_TIM9
         #define CLK_ENABLE_BIT 0x4
      #elif ESCAPEMENT_TIMER ==  OS_IO_TIM10
         #define CLK_ENABLE_BIT 0x8
      #elif ESCAPEMENT_TIMER == OS_IO_TIM11
         #define CLK_ENABLE_BIT 0x10
      #endif
   #elif ESCAPEMENT_TIMER ==  OS_IO_TIM2 || ESCAPEMENT_TIMER == OS_IO_TIM3 || \
         ESCAPEMENT_TIMER == OS_IO_TIM4 || ESCAPEMENT_TIMER == OS_IO_TIM5
      #define CLK_ENABLE *((volatile UINT32 *)0x40023824) // RCC_APB1ENR
      #if ESCAPEMENT_TIMER ==  OS_IO_TIM2
         #define CLK_ENABLE_BIT 0x1
      #elif ESCAPEMENT_TIMER == OS_IO_TIM3
         #define CLK_ENABLE_BIT 0x2
      #elif ESCAPEMENT_TIMER == OS_IO_TIM4
         #define CLK_ENABLE_BIT 0x4
      #elif ESCAPEMENT_TIMER == OS_IO_TIM5
         #define CLK_ENABLE_BIT 0x8
      #endif
   #endif
#elif defined(STM32F1XXXX)
   #if ESCAPEMENT_TIMER == OS_IO_TIM1 || ESCAPEMENT_TIMER ==  OS_IO_TIM8 || \
       ESCAPEMENT_TIMER == OS_IO_TIM9 || ESCAPEMENT_TIMER == OS_IO_TIM10 || ESCAPEMENT_TIMER == OS_IO_TIM11 || \
       ESCAPEMENT_TIMER == OS_IO_TIM15 || ESCAPEMENT_TIMER == OS_IO_TIM16 || ESCAPEMENT_TIMER == OS_IO_TIM17
      #define CLK_ENABLE *((volatile UINT32 *)0x40021018) // RCC_APB2ENR
      #if ESCAPEMENT_TIMER == OS_IO_TIM1
         #define CLK_ENABLE_BIT 0x800
      #elif ESCAPEMENT_TIMER ==  OS_IO_TIM8
         #define CLK_ENABLE_BIT 0x2000
      #elif ESCAPEMENT_TIMER == OS_IO_TIM9
         #define CLK_ENABLE_BIT 0x80000
      #elif ESCAPEMENT_TIMER == OS_IO_TIM10
         #define CLK_ENABLE_BIT 0x100000
      #elif ESCAPEMENT_TIMER == OS_IO_TIM11
         #define CLK_ENABLE_BIT 0x200000
      #elif ESCAPEMENT_TIMER == OS_IO_TIM15
         #define CLK_ENABLE_BIT 0x10000
      #elif ESCAPEMENT_TIMER == OS_IO_TIM16
         #define CLK_ENABLE_BIT 0x20000
      #elif ESCAPEMENT_TIMER == OS_IO_TIM17
         #define CLK_ENABLE_BIT 0x40000
      #endif
   #elif ESCAPEMENT_TIMER ==  OS_IO_TIM2 || ESCAPEMENT_TIMER == OS_IO_TIM3 || \
         ESCAPEMENT_TIMER == OS_IO_TIM4 || ESCAPEMENT_TIMER == OS_IO_TIM5 || \
         ESCAPEMENT_TIMER == OS_IO_TIM12 || ESCAPEMENT_TIMER == OS_IO_TIM13 || \
         ESCAPEMENT_TIMER == OS_IO_TIM14
      #define CLK_ENABLE *((volatile UINT32 *)0x4002101C) // RCC_APB1ENR
      #if ESCAPEMENT_TIMER ==  OS_IO_TIM2
         #define CLK_ENABLE_BIT 0x1
      #elif ESCAPEMENT_TIMER == OS_IO_TIM3
         #define CLK_ENABLE_BIT 0x2
      #elif ESCAPEMENT_TIMER == OS_IO_TIM4
         #define CLK_ENABLE_BIT 0x4
      #elif ESCAPEMENT_TIMER == OS_IO_TIM5
         #define CLK_ENABLE_BIT 0x8
      #elif ESCAPEMENT_TIMER == OS_IO_TIM12
         #define CLK_ENABLE_BIT 0x40
      #elif ESCAPEMENT_TIMER == OS_IO_TIM13
         #define CLK_ENABLE_BIT 0x80
      #elif ESCAPEMENT_TIMER == OS_IO_TIM14
         #define CLK_ENABLE_BIT 0x100
      #endif
   #endif
#elif defined(STM32F2XXXX) || defined(STM32F4XXXX)
   #if ESCAPEMENT_TIMER == OS_IO_TIM1 || ESCAPEMENT_TIMER == OS_IO_TIM8 || \
       ESCAPEMENT_TIMER == OS_IO_TIM9 || ESCAPEMENT_TIMER == OS_IO_TIM10 || ESCAPEMENT_TIMER == OS_IO_TIM11
      #define CLK_ENABLE *((volatile UINT32 *)0x40023844) // RCC_APB2ENR
      #if ESCAPEMENT_TIMER == OS_IO_TIM1
         #define CLK_ENABLE_BIT 0x1
      #elif ESCAPEMENT_TIMER == OS_IO_TIM8
         #define CLK_ENABLE_BIT 0x2
      #elif ESCAPEMENT_TIMER == OS_IO_TIM9
         #define CLK_ENABLE_BIT 0x10000
      #elif ESCAPEMENT_TIMER == OS_IO_TIM10
         #define CLK_ENABLE_BIT 0x20000
      #elif ESCAPEMENT_TIMER == OS_IO_TIM11
         #define CLK_ENABLE_BIT 0x40000
      #endif
   #elif ESCAPEMENT_TIMER == OS_IO_TIM2 || ESCAPEMENT_TIMER == OS_IO_TIM3 || \
         ESCAPEMENT_TIMER == OS_IO_TIM4 || ESCAPEMENT_TIMER == OS_IO_TIM5 || \
         ESCAPEMENT_TIMER == OS_IO_TIM12 || ESCAPEMENT_TIMER == OS_IO_TIM13 || \
         ESCAPEMENT_TIMER == OS_IO_TIM14
      #define CLK_ENABLE *((volatile UINT32 *)0x40023840) // RCC_APB1ENR
      #if ESCAPEMENT_TIMER ==  OS_IO_TIM2
         #define CLK_ENABLE_BIT 0x1
      #elif ESCAPEMENT_TIMER == OS_IO_TIM3
         #define CLK_ENABLE_BIT 0x2
      #elif ESCAPEMENT_TIMER == OS_IO_TIM4
         #define CLK_ENABLE_BIT 0x4
      #elif ESCAPEMENT_TIMER == OS_IO_TIM5
         #define CLK_ENABLE_BIT 0x8
      #elif ESCAPEMENT_TIMER == OS_IO_TIM12
         #define CLK_ENABLE_BIT 0x40
      #elif ESCAPEMENT_TIMER == OS_IO_TIM13
         #define CLK_ENABLE_BIT 0x80
      #elif ESCAPEMENT_TIMER == OS_IO_TIM14
         #define CLK_ENABLE_BIT 0x100
      #endif
   #endif
#else
   #error STM32 version undefined
#endif


/* Memory mapped timer registers relative to the base address of the timer */
#define TIM_CONTROL1         *((volatile UINT16 *)(TIME_BASE + 0x00))
#define TIM_INT_ENABLE       *((volatile UINT16 *)(TIME_BASE + 0x0C))
#define TIM_STATUS           *((volatile UINT16 *)(TIME_BASE + 0x10))
#define TIM_EVENT_GENERATION *((volatile UINT16 *)(TIME_BASE + 0x14))
#ifdef ESCAPEMENT_TIMER_32
   #define TIM_COUNTER       *((volatile UINT32 *)(TIME_BASE + 0x24))
#elif defined(ESCAPEMENT_TIMER_16)
   #define TIM_COUNTER       *((volatile UINT16 *)(TIME_BASE + 0x24))
#endif
#define TIM_PRESCALER        *((volatile UINT16 *)(TIME_BASE + 0x28))
#ifdef ESCAPEMENT_TIMER_32
   #define TIM_AUTORELOAD    *((volatile UINT32 *)(TIME_BASE + 0x2C))
   #define TIM_COMPARATOR    *((volatile UINT32 *)(TIME_BASE + 0x34))
#elif defined(ESCAPEMENT_TIMER_16)
   #define TIM_AUTORELOAD    *((volatile UINT16 *)(TIME_BASE + 0x2C))
   #define TIM_COMPARATOR    *((volatile UINT16 *)(TIME_BASE + 0x34))
#endif

#define UPDATE_INT_BIT      0x1
#define COMPARATOR_INT_BIT  0x2

/* Minimal interrupt descriptor to call an interrupt handler */
typedef struct TIMER_ISR_DATA {
  void (*TimerIntHandler)(struct TIMER_ISR_DATA *);
} TIMER_ISR_DATA;

#if ESCAPEMENT_TIMER == OS_IO_TIM1 || ESCAPEMENT_TIMER == OS_IO_TIM8
   static void TimerHandler_CC(struct TIMER_ISR_DATA *descriptor); // Interrupt handler for comparator interrupt
   static void TimerHandler_Up(struct TIMER_ISR_DATA *descriptor); // Interrupt handler for overflow
#else
   static void TimerHandler(struct TIMER_ISR_DATA *descriptor); // Interrupt handler for comparator and overflow interrupt
#endif


#ifdef ESCAPEMENT_TIMER_16
   /* System wall clock. This variable stores the most-significant 16 bits of the current
   ** time. The lower 16 bits are directly taken from the timer counter register. To get
   ** the current time, use _OSGetActualTime. */
   static volatile INT16 Time;
#endif

/* Timer interrupt cause marked by the ISR and that is used by the lower priority handler
** ( _OSTimerInterruptHandler) to process the interrupt. */
volatile BOOL _OSOverflowInterruptFlag = FALSE;
volatile BOOL _OSComparatorInterruptFlag = FALSE;

/* _OSInitializeTimer: Initializes the timer which starts counting as soon as Escapement is
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
  #ifdef ESCAPEMENT_TIMER_32
     TIM_AUTORELOAD = 0x3FFFFFFF;          // Set the autoreload value (2^30 - 1)
  #elif defined(ESCAPEMENT_TIMER_16)
     TIM_AUTORELOAD = 0xFFFF;              // Set the autoreload value (2^16 - 1)
  #endif
  /* Set the timer clock prescaler to ESCAPEMENT_TIMER_PRESCALER. This value is defined in
  ** file Escapement_Config.h. */
  TIM_PRESCALER = ESCAPEMENT_TIMER_PRESCALER;
  TIM_EVENT_GENERATION = UPDATE_INT_BIT;   // Generate an update event to reload the prescaler
  TIM_STATUS = ~(UPDATE_INT_BIT | COMPARATOR_INT_BIT);   // Clear update flag
  TIM_INT_ENABLE |= UPDATE_INT_BIT | COMPARATOR_INT_BIT; // Enable update interrupt
  /* Initialize Cortex-Mx Nested Vectored Interrupt Controller */
  #if defined(CORTEX_M3) || defined(CORTEX_M4)
     /* Compute the priority (Only 4 bits are used for priority on STM32). PRIGROUP de-
     ** fines the interrupt priority levels of the microcontroller (see Escapement_Config.h),
     ** where 7-PRIGROUP is the number priority bits, and PRIGROUP-3 is the number of
     ** sub-priority bits */
     tmpPriority = TIMER_PRIORITY << (PRIGROUP - 3);
     tmpPriority |=  TIMER_SUB_PRIORITY & (0x0F >> (7 - PRIGROUP));
  #endif
  /* Set the IRQ priority */
  intPriorityLevel = (UINT8 *)(IRQ_PRIORITY_REGISTER + ((ESCAPEMENT_TIMER) & 0xFF));
  #if defined(CORTEX_M3) || defined(CORTEX_M4)
     *intPriorityLevel = tmpPriority << 4; // Only 4 MSB bits are used on STM32
  #elif defined(CORTEX_M0)
     *intPriorityLevel = TIMER_PRIORITY;
  #endif
  /* Enable the IRQ channels */
  #if defined(CORTEX_M3) || defined(CORTEX_M4)
     intSetEnable = (UINT32 *)IRQ_SET_ENABLE_REGISTER + (UINT32)(((ESCAPEMENT_TIMER) & 0xFF) / 32);
     *intSetEnable |= 0x01 << (((ESCAPEMENT_TIMER) & 0xFF) % 32);
  #elif defined(CORTEX_M0)
     *(UINT32 *)IRQ_SET_ENABLE_REGISTER |= 0x01 << ((ESCAPEMENT_TIMER) & 0xFF);
  #endif
  /* Initialize Escapement internal interrupt structure */
  #if ESCAPEMENT_TIMER == OS_IO_TIM1
     /* Comparator interrupt */
     device = (TIMER_ISR_DATA *)OSMalloc(sizeof(TIMER_ISR_DATA));
     device->TimerIntHandler = TimerHandler_CC;
     OSSetISRDescriptor(OS_IO_TIM1_CC,device);
     /* Update interrupt */
     device = (TIMER_ISR_DATA *)OSMalloc(sizeof(TIMER_ISR_DATA));
     device->TimerIntHandler = TimerHandler_Up;
     OSSetISRDescriptor(OS_IO_TIM1_UP,device);
     /* Set the IRQ priority */
     intPriorityLevel = (UINT8 *)(IRQ_PRIORITY_REGISTER + ((OS_IO_TIM1_UP) & 0xFF));
     #if defined(CORTEX_M3) || defined(CORTEX_M4)
        *intPriorityLevel = tmpPriority << 4; // Only 4 MSB bits are used on STM32
     #elif defined(CORTEX_M0)
        *intPriorityLevel = priority;
     #endif
     /* Enable the IRQ channels */
     #if defined(CORTEX_M3) || defined(CORTEX_M4)
        intSetEnable = (UINT32 *)IRQ_SET_ENABLE_REGISTER + (UINT32)(((OS_IO_TIM1_UP) & 0xFF) / 32);
        *intSetEnable |= 0x01 << (((OS_IO_TIM1_UP) & 0xFF) % 32);
     #elif defined(CORTEX_M0)
        *(UINT32 *)IRQ_SET_ENABLE_REGISTER |= 0x01 << ((OS_IO_TIM1_UP) & 0xFF);
     #endif
  #elif ESCAPEMENT_TIMER == OS_IO_TIM2 || ESCAPEMENT_TIMER == OS_IO_TIM3 || \
        ESCAPEMENT_TIMER == OS_IO_TIM4 || ESCAPEMENT_TIMER == OS_IO_TIM5 || \
        ESCAPEMENT_TIMER == OS_IO_TIM9 || ESCAPEMENT_TIMER == OS_IO_TIM10 || \
        ESCAPEMENT_TIMER == OS_IO_TIM11 || ESCAPEMENT_TIMER == OS_IO_TIM12 || \
        ESCAPEMENT_TIMER == OS_IO_TIM13 || ESCAPEMENT_TIMER == OS_IO_TIM14 || \
        ESCAPEMENT_TIMER == OS_IO_TIM15 || ESCAPEMENT_TIMER == OS_IO_TIM16 || \
        ESCAPEMENT_TIMER == OS_IO_TIM17
     device = (TIMER_ISR_DATA *)OSMalloc(sizeof(TIMER_ISR_DATA));
     device->TimerIntHandler = TimerHandler;
     OSSetISRDescriptor(ESCAPEMENT_TIMER,device);
  #elif ESCAPEMENT_TIMER == OS_IO_TIM8
     /* Comparator interrupt */
     device = (TIMER_ISR_DATA *)OSMalloc(sizeof(TIMER_ISR_DATA));
     device->TimerIntHandler = TimerHandler_CC;
     OSSetISRDescriptor(OS_IO_TIM8_CC,device);
     /* Update interrupt */
     device = (TIMER_ISR_DATA *)OSMalloc(sizeof(TIMER_ISR_DATA));
     device->TimerIntHandler = TimerHandler_Up;
     OSSetISRDescriptor(OS_IO_TIM8_UP,device);
     /* Set the IRQ priority */
     intPriorityLevel = (UINT8 *)(IRQ_PRIORITY_REGISTER + ((OS_IO_TIM8_UP) & 0xFF));
     #if defined(CORTEX_M3) || defined(CORTEX_M4)
        *intPriorityLevel = tmpPriority << 4; // Only 4 MSB bits are used on STM32
     #elif defined(CORTEX_M0)
        *intPriorityLevel = priority;
     #endif
     /* Enable the IRQ channels */
     #if defined(CORTEX_M3) || defined(CORTEX_M4)
        intSetEnable = (UINT32 *)IRQ_SET_ENABLE_REGISTER + (UINT32)(((OS_IO_TIM8_UP) & 0xFF) / 32);
        *intSetEnable |= 0x01 << (((OS_IO_TIM8_UP) & 0xFF) % 32);
     #elif defined(CORTEX_M0)
        *(UINT32 *)IRQ_SET_ENABLE_REGISTER |= 0x01 << ((OS_IO_TIM8_UP) & 0xFF);
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
#if ESCAPEMENT_TIMER == OS_IO_TIM1 || ESCAPEMENT_TIMER == OS_IO_TIM8
   void TimerHandler_Up(struct TIMER_ISR_DATA *descriptor)
   { // Timer overflow ISR
     TIM_STATUS = ~UPDATE_INT_BIT;  // Clear interrupt flag
     #ifdef ESCAPEMENT_TIMER_16
        Time += 1;                  // Increment most significant word of Time
     #endif
     _OSOverflowInterruptFlag = TRUE;
     _OSGenerateSoftTimerInterrupt();
   } /* end of TimerHandler_Up */

   void TimerHandler_CC(struct TIMER_ISR_DATA *descriptor)
   { // Comparator match ISR
     TIM_STATUS = ~COMPARATOR_INT_BIT;  // Clear interrupt flag
     TIM_COMPARATOR = 0;                // Disable timer comparator
     _OSComparatorInterruptFlag = TRUE;
     _OSGenerateSoftTimerInterrupt();
   } /* end of TimerHandler_CC */
#else
   void TimerHandler(struct TIMER_ISR_DATA *descriptor)
   {
     if (TIM_STATUS & UPDATE_INT_BIT) {    // Is timer overflow interrupt?
        TIM_STATUS = ~UPDATE_INT_BIT;      // Clear interrupt flag
        #ifdef ESCAPEMENT_TIMER_16
           Time += 1;       // Increment most significant word of Time
        #endif
        _OSOverflowInterruptFlag = TRUE;
        _OSGenerateSoftTimerInterrupt();
     }
     if (TIM_STATUS & COMPARATOR_INT_BIT) { // Is comparator interrupt pending?
        TIM_STATUS = ~COMPARATOR_INT_BIT;   // Clear interrupt flag
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
  #ifdef ESCAPEMENT_TIMER_32
     TIM_COMPARATOR = nextArrival;
     if (TIM_COMPARATOR > TIM_COUNTER)
        return TRUE;
     TIM_COMPARATOR = 0;
     return FALSE;
  #elif defined(ESCAPEMENT_TIMER_16)
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
  #ifdef ESCAPEMENT_TIMER_32
     if (_OSOverflowInterruptFlag) {
        _OSOverflowInterruptFlag = FALSE;
        return TRUE;
     }
     else
        return FALSE;
  #elif defined(ESCAPEMENT_TIMER_16)
     INT16 tmp, time;
     if (_OSOverflowInterruptFlag) {
        _OSOverflowInterruptFlag = FALSE;
        if ((tmp = shiftTimeLimit >> 16) <= Time) {
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


/* _OSGetActualTime: Retrieves the current time. When Escapement' interval timer is confi-
** gured for 16-bit timer, this function combines the 16 bits of the timer counter with
** the global variable Time to yield the current time. This function should never be
** called from an ISR having higher priority than ESCAPEMENT_TIMER_16. */
INT32 _OSGetActualTime(void)
{
  #ifdef ESCAPEMENT_TIMER_32
     return TIM_COUNTER;
  #elif defined(ESCAPEMENT_TIMER_16)
     INT16 currentTime;
     INT32 tmp;
     do {
        currentTime = Time;
        tmp = (INT32)currentTime << 16 | TIM_COUNTER;
     } while (currentTime != Time);
     return tmp;
  #endif
} /* end of _OSGetActualTime */
