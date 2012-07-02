/* Copyright (c) 2012 MIS Institute of the HEIG-VD affiliated to the University of
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
/* File TimerEvent.c: Implements an event queue that is associated with a timer device.
** Events that are inserted into the queue are specified along with a delay, and as soon
** as this delay has expired, the event is scheduled.
** Platform version: All STM32 microcontrollers.
** Version date: April 2012
** Authors: MIS-TIC */

#include "ZottaOS_CortexMx.h"
#include "ZottaOS.h"
#include "ZottaOS_TimerEvent.h"

/* Because events are sorted by the time at which they occur, and that this time is mon-
** otonically increasing, the relative time reference wraparounds. We therefore need to
** periodically shift the occurrence times. This is done very SHIFT_TIME_LIMIT tics. */
#define SHIFT_TIME_LIMIT    0x40000000 // = 2^30
#define SHIFT_TIME_LIMIT_16 0x00004000 // = 2^30 >> 16

#ifdef OS_IO_TIM1
   #if defined(STM32F1XXXX) ||  defined(STM32F05XXX)
      #define BASE_TIM1 0x40012C00
   #elif defined(STM32F2XXXX) || defined(STM32F4XXXX)
      #define BASE_TIM1 0x40010000
   #else
      #error Base address unspecified
   #endif
#endif
#ifdef OS_IO_TIM2
   #define BASE_TIM2 0x40000000
#endif
#ifdef OS_IO_TIM3
   #define BASE_TIM3 0x40000400
#endif
#ifdef OS_IO_TIM4
   #define BASE_TIM4 0x40000800
#endif
#ifdef OS_IO_TIM5
   #define BASE_TIM5 0x40000C00
#endif
#ifdef OS_IO_TIM8
   #if defined(STM32F1XXXX)
      #define BASE_TIM8 0x40013400
   #elif defined(STM32F2XXXX) || defined(STM32F4XXXX)
      #define BASE_TIM8 0x40010400
   #else
      #error Base address unspecified
   #endif
#endif
#ifdef OS_IO_TIM9
   #if defined(STM32L1XXXX)
      #define BASE_TIM9 0x40010800
   #elif defined(STM32F1XXXX)
      #define BASE_TIM9 0x40014C00
   #elif defined(STM32F2XXXX) || defined(STM32F4XXXX)
      #define BASE_TIM9 0x40014000
   #else
      #error Base address unspecified
   #endif
#endif
#ifdef OS_IO_TIM10
   #if defined(STM32L1XXXX)
      #define BASE_TIM10 0x40010C00
   #elif defined(STM32F1XXXX)
      #define BASE_TIM10 0x40015000
   #elif defined(STM32F2XXXX) || defined(STM32F4XXXX)
      #define BASE_TIM10 0x40014400
   #else
      #error Base address unspecified
   #endif
#endif
#ifdef OS_IO_TIM11
   #if defined(STM32L1XXXX)
      #define BASE_TIM11 0x40011000
   #elif defined(STM32F1XXXX)
      #define BASE_TIM11 0x40015400
   #elif defined(STM32F2XXXX) || defined(STM32F4XXXX)
      #define BASE_TIM11 0x40014800
   #else
      #error Base address unspecified
   #endif
#endif
#ifdef OS_IO_TIM12
   #define BASE_TIM12 0x40001800
#endif
#ifdef OS_IO_TIM13
   #define BASE_TIM13 0x40001C00
#endif
#ifdef OS_IO_TIM14
   #define BASE_TIM14 0x40002000
#endif
#ifdef OS_IO_TIM15
   #define BASE_TIM15 0x40014000
#endif
#ifdef OS_IO_TIM16
   #define BASE_TIM16 0x40014400
#endif
#ifdef OS_IO_TIM17
   #define BASE_TIM17 0x40014800
#endif

#define OFFSET_CONTROL1         0x00
#define OFFSET_INT_ENABLE       0x0C
#define OFFSET_STATUS           0x10
#define OFFSET_EVENT_GENERATION 0x14
#define OFFSET_COUNTER          0x24
#define OFFSET_PRESCALER        0x28
#define OFFSET_AUTORELOAD       0x2C
#define OFFSET_COMPARATOR       0x34


typedef struct TIMER_EVENT_NODE { // Blocks that are in the event queue
  struct TIMER_EVENT_NODE *Next;
  void *Event;
  INT32 Time;
} TIMER_EVENT_NODE;

typedef struct TIMER_ISR_DATA {
  void (*TimerIntHandler)(struct TIMER_ISR_DATA *);
  UINT32 *ClkEnable;
  UINT32 ClkEnableBit;
  UINT32 Base;
  void *PendingQueueOperation;  // Interrupted operations that are not complete
  TIMER_EVENT_NODE *EventQueue; // List of events sorted by their time of occurrence
  TIMER_EVENT_NODE *FreeNodes;  // Pool of free nodes used to link events
  INT16 Time;                   // 16-bit MSB for 16-bit counters or version number for 32-bit counters
} TIMER_ISR_DATA;


typedef enum {InsertEventOp,DeleteEventOp} EVENTOP;

typedef struct {
  EVENTOP EventOp;
  volatile BOOL Done;
  TIMER_EVENT_NODE *Left;
  TIMER_EVENT_NODE *Node;
  volatile BOOL GeneratedInterrupt;
  INT16 Version; // Saved timer shift version (32-bit counters) or 16-bit MSB for 16-bit counters
  INT32 Time;    // Interrupt time of the event
} INSERTQUEUE_OP;

typedef struct {
  EVENTOP EventOp;
  volatile BOOL Done;
  TIMER_EVENT_NODE *Left;
  TIMER_EVENT_NODE *Node;
  volatile BOOL GenerateInterrupt;
  void *Event;
} DELETEQUEUE_OP;


static void InsertQueueHelper(INSERTQUEUE_OP *des, TIMER_ISR_DATA *device, BOOL genInt);
static void DeleteQueueHelper(DELETEQUEUE_OP *des);
static TIMER_EVENT_NODE *GetFreeNode(TIMER_ISR_DATA *device);
static void ReleaseNode(TIMER_ISR_DATA *device, TIMER_EVENT_NODE *node);
static void TimerIntHandler(TIMER_ISR_DATA *device);
static UINT8 GetInterruptSubIndex(UINT8 interruptIndex);

/* OSInitTimerEvent: Creates an ISR descriptor block holding the specifics of a timer
** device that is used as an event handler and which can schedule a list of event at
** their occurrence time. */
#if defined(CORTEX_M3) || defined(CORTEX_M4)
void OSInitTimerEvent(UINT8 nbNode, UINT16 prescaler, UINT8 priority, UINT8 subpriority,
                      UINT8 interruptIndex)
#elif defined(CORTEX_M0)
void OSInitTimerEvent(UINT8 nbNode, UINT16 prescaler, UINT8 priority, UINT8 interruptIndex)
#endif
{
  #define IRQ_SET_ENABLE_REGISTER 0xE000E100 // 0xE000E100 to 0xE000E11C (see CortexMx_TRM)
  #define IRQ_PRIORITY_REGISTER   0xE000E400 // 0xE000E400 to 0xE000E41F (see CortexMx_TRM)
  TIMER_ISR_DATA *device;
  #if defined(CORTEX_M3) || defined(CORTEX_M4)
     UINT8 tmppriority;
     UINT32 *intSetEnable;
  #endif
  UINT8 i, *intPriorityLevel;
  /*** Initialize timer event descriptor ***/
  device = (TIMER_ISR_DATA *)OSMalloc(sizeof(TIMER_ISR_DATA));
  device->TimerIntHandler = TimerIntHandler;
  device->Time = 0;
  device->PendingQueueOperation = NULL;
  /* Initialize event queue */
  device->EventQueue = NULL;
  /* Create a pool of free event nodes */
  device->FreeNodes = (TIMER_EVENT_NODE *)OSMalloc(nbNode * sizeof(TIMER_EVENT_NODE));
  for (i = 0; i < nbNode - 1; i += 1)
     device->FreeNodes[i].Next = &device->FreeNodes[i+1];
  device->FreeNodes[i].Next = NULL;
  OSSetTimerISRDescriptor(interruptIndex,GetInterruptSubIndex(interruptIndex),device);
  /* Initialize descriptor specific timer device entry */
  switch (interruptIndex) {
     #ifdef OS_IO_TIM1
     case OS_IO_TIM1:
         /* Set timer registers base address */
        device->Base = BASE_TIM1;
        /* Set enable clock bit and register (RCC_APB2ENR) */
        device->ClkEnableBit = 0x1;
        #if defined(STM32F1XXXX) || defined(STM32F05XXX)
           device->ClkEnable = (UINT32 *)0x40021018;
           device->ClkEnableBit = 0x800;
        #elif defined(STM32F2XXXX) || defined(STM32F4XXXX)
           device->ClkEnable = (UINT32 *)0x40023844;
        #endif
        break;
     #endif
     #ifdef OS_IO_TIM2
     case OS_IO_TIM2:
         /* Set timer registers base address */
        device->Base = BASE_TIM2;
        /* Set enable clock bit and register (RCC_APB1ENR) */
        device->ClkEnableBit = 0x1;
        #if defined(STM32L1XXXX)
           device->ClkEnable = (UINT32 *)0x40023824;
        #elif defined(STM32F1XXXX) || defined(STM32F05XXX)
           device->ClkEnable = (UINT32 *)0x4002101C;
        #elif defined(STM32F2XXXX) || defined(STM32F4XXXX)
           device->ClkEnable = (UINT32 *)0x40023840;
        #endif
        break;
     #endif
     #ifdef OS_IO_TIM3
     case OS_IO_TIM3:
        /* Set timer registers base address */
        device->Base = BASE_TIM3;
        /* Set enable clock bit and register (RCC_APB1ENR) */
        device->ClkEnableBit = 0x2;
        #if defined(STM32L1XXXX)
           device->ClkEnable = (UINT32 *)0x40023824;
        #elif defined(STM32F1XXXX) || defined(STM32F05XXX)
           device->ClkEnable = (UINT32 *)0x4002101C;
        #elif defined(STM32F2XXXX) || defined(STM32F4XXXX)
           device->ClkEnable = (UINT32 *)0x40023840;
        #endif
        break;
     #endif
     #ifdef OS_IO_TIM4
     case OS_IO_TIM4:
         /* Set timer registers base address */
        device->Base = BASE_TIM4;
        /* Set enable clock bit and register (RCC_APB1ENR) */
        device->ClkEnableBit = 0x4;
        #if defined(STM32L1XXXX)
           device->ClkEnable = (UINT32 *)0x40023824;
        #elif defined(STM32F1XXXX)
           device->ClkEnable = (UINT32 *)0x4002101C;
        #elif defined(STM32F2XXXX) || defined(STM32F4XXXX)
           device->ClkEnable = (UINT32 *)0x40023840;
        #endif
        break;
     #endif
     #ifdef OS_IO_TIM5
     case OS_IO_TIM5:
        /* Set timer registers base address */
        device->Base = BASE_TIM5;
        /* Set enable clock bit and register (RCC_APB1ENR) */
        device->ClkEnableBit = 0x8;
        #if defined(STM32L1XXXX)
           device->ClkEnable = (UINT32 *)0x40023824;
        #elif defined(STM32F1XXXX)
           device->ClkEnable = (UINT32 *)0x4002101C;
        #elif defined(STM32F2XXXX) || defined(STM32F4XXXX)
           device->ClkEnable = (UINT32 *)0x40023840;
        #endif
        break;
     #endif
     #ifdef OS_IO_TIM8
     case OS_IO_TIM8:
        /* Set timer registers base address */
        device->Base = BASE_TIM8;
        /* Set enable clock bit and register (RCC_APB2ENR) */
        #if defined(STM32F1XXXX)
           device->ClkEnableBit = 0x2000;
           device->ClkEnable = (UINT32 *)0x40021018;
        #elif defined(STM32F2XXXX) || defined(STM32F4XXXX)
           device->ClkEnableBit = 0x2;
           device->ClkEnable = (UINT32 *)0x40023844;
        #endif
        break;
     #endif
     #ifdef OS_IO_TIM9
     case OS_IO_TIM9:
        /* Set timer registers base address */
        device->Base = BASE_TIM9;
        /* Set enable clock bit and register (RCC_APB2ENR) */
        #if defined(STM32L1XXXX)
           device->ClkEnableBit = 0x04;
           device->ClkEnable = (UINT32 *)0x40023820;
        #elif defined(STM32F1XXXX)
           device->ClkEnableBit = 0x080000;
           device->ClkEnable = (UINT32 *)0x40021018;
        #elif defined(STM32F2XXXX) || defined(STM32F4XXXX)
           device->ClkEnableBit = 0x10000;
           device->ClkEnable = (UINT32 *)0x40023844;
        #endif
        break;
     #endif
     #ifdef OS_IO_TIM10
     case OS_IO_TIM10:
        /* Set timer registers base address */
        device->Base = BASE_TIM10;
        /* Set enable clock bit and register (RCC_APB2ENR) */
        #if defined(STM32L1XXXX)
           device->ClkEnableBit = 0x08;
           device->ClkEnable = (UINT32 *)0x40023820;
        #elif defined(STM32F1XXXX)
           device->ClkEnableBit = 0x100000;
           device->ClkEnable = (UINT32 *)0x40021018;
        #elif defined(STM32F2XXXX) || defined(STM32F4XXXX)
           device->ClkEnableBit = 0x20000;
           device->ClkEnable = (UINT32 *)0x40023844;
        #endif
        break;
     #endif
     #ifdef OS_IO_TIM11
     case OS_IO_TIM11:
        /* Set timer registers base address */
        device->Base = BASE_TIM11;
        /* Set enable clock bit and register (RCC_APB2ENR) */
        #if defined(STM32L1XXXX)
        device->ClkEnableBit = 0x10;
           device->ClkEnable = (UINT32 *)0x40023820;
        #elif defined(STM32F1XXXX)
           device->ClkEnableBit = 0x200000;
           device->ClkEnable = (UINT32 *)0x40021018;
        #elif defined(STM32F2XXXX) || defined(STM32F4XXXX)
           device->ClkEnableBit = 0x40000;
           device->ClkEnable = (UINT32 *)0x40023844;
        #endif
        break;
     #endif
     #ifdef OS_IO_TIM12
     case OS_IO_TIM12:
        /* Set timer registers base address */
        device->Base = BASE_TIM12;
        /* Set enable clock bit and register (RCC_APB1ENR) */
        device->ClkEnableBit = 0x40;
        #if defined(STM32F1XXXX)
           device->ClkEnable = (UINT32 *)0x4002101C;
        #elif defined(STM32F2XXXX) || defined(STM32F4XXXX)
           device->ClkEnable = (UINT32 *)0x40023840;
        #endif
        break;
     #endif
     #ifdef OS_IO_TIM13
     case OS_IO_TIM13:
        /* Set timer registers base address */
        device->Base = BASE_TIM13;
        /* Set enable clock bit and register (RCC_APB1ENR) */
        device->ClkEnableBit = 0x80;
        #if defined(STM32F1XXXX)
           device->ClkEnable = (UINT32 *)0x4002101C;
        #elif defined(STM32F2XXXX) || defined(STM32F4XXXX)
           device->ClkEnable = (UINT32 *)0x40023840;
        #endif
        break;
     #endif
     #ifdef OS_IO_TIM14
     case OS_IO_TIM14:
        /* Set timer registers base address */
        device->Base = BASE_TIM14;
        /* Set enable clock bit and register (RCC_APB1ENR) */
        device->ClkEnableBit = 0x100;
        #if defined(STM32F1XXXX) || defined(STM32F05XXX)
           device->ClkEnable = (UINT32 *)0x4002101C;
        #elif defined(STM32F2XXXX) || defined(STM32F4XXXX)
           device->ClkEnable = (UINT32 *)0x40023840;
        #endif
        break;
     #endif
     #ifdef OS_IO_TIM15
     case OS_IO_TIM15:
        /* Set timer registers base address */
        device->Base = BASE_TIM15;
        /* Set enable clock bit and register (RCC_APB2ENR) */
        device->ClkEnableBit = 0x10000;
        device->ClkEnable = (UINT32 *)0x40021018;
        break;
     #endif
     #ifdef OS_IO_TIM16
     case OS_IO_TIM16:
        /* Set timer registers base address */
        device->Base = BASE_TIM16;
        /* Set enable clock bit and register (RCC_APB2ENR) */
        device->ClkEnableBit = 0x20000;
        device->ClkEnable = (UINT32 *)0x40021018;
        break;
     #endif
     #ifdef OS_IO_TIM17
     case OS_IO_TIM17:
        /* Set timer registers base address */
        device->Base = BASE_TIM17;
        /* Set enable clock bit and register (RCC_APB2ENR) */
        device->ClkEnableBit = 0x40000;
        device->ClkEnable = (UINT32 *)0x40021018;
        break;
     #endif
     default:
     break;
  }
  /*** Initialize Cortex-Mx Nested Vectored Interrupt Controller ***/
  #if defined(CORTEX_M3) || defined(CORTEX_M4)
     /* Compute the priority (Only 4 bits are used for priority on STM32) */
     tmppriority = priority << (PRIGROUP - 3); // (PRIGROUP - 3) is the number of sub priorty bits
     tmppriority |=  subpriority & (0x0F >> (7 - PRIGROUP)); // (7 - PRIGROUP) is the number priorty bits
  #endif
  /* Set the IRQ priority */
  intPriorityLevel = (UINT8 *)(IRQ_PRIORITY_REGISTER + interruptIndex);
  #if defined(CORTEX_M3) || defined(CORTEX_M4)
     *intPriorityLevel = tmppriority << 4; // Only 4 MSB bits are used on STM32
  #elif defined(CORTEX_M0)
     *intPriorityLevel = priority;
  #endif
  /* Enable the IRQ channels */
  #if defined(CORTEX_M3) || defined(CORTEX_M4)
     intSetEnable = (UINT32 *)IRQ_SET_ENABLE_REGISTER + (UINT32)(interruptIndex / 32);
     *intSetEnable |= 0x01 << (interruptIndex % 32);
  #elif defined(CORTEX_M0)
     *(UINT32 *)IRQ_SET_ENABLE_REGISTER |= 0x01 << interruptIndex;
  #endif
  /*** Initialize update interruption for timer 1 and timer 8 ***/
  #if defined(OS_IO_TIM1)
     if (interruptIndex == OS_IO_TIM1) {
         /* Register timer descriptor */
         OSSetTimerISRDescriptor(OS_IO_TIM1_UP,0,device);
         /* Set the IRQ priority */
         intPriorityLevel = (UINT8 *)(IRQ_PRIORITY_REGISTER + OS_IO_TIM1_UP);
         #if defined(CORTEX_M3) || defined(CORTEX_M4)
            *intPriorityLevel = tmppriority << 4; // Only 4 MSB bits are used on STM32
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
     }
  #endif
  #if defined(OS_IO_TIM8)
     if (interruptIndex == OS_IO_TIM8) {
         /* Register timer descriptor */
         OSSetTimerISRDescriptor(OS_IO_TIM8_UP,0,device);
         /* Set the IRQ priority */
         intPriorityLevel = (UINT8 *)(IRQ_PRIORITY_REGISTER + OS_IO_TIM8_UP);
         #if defined(CORTEX_M3) || defined(CORTEX_M4)
            *intPriorityLevel = tmppriority << 4; // Only 4 MSB bits are used on STM32
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
     }
  #endif
  /*** Initialize hardware timer ***/
  *device->ClkEnable |= device->ClkEnableBit; // Enable timer clock
  /* Initialize autoreload timer register */
  #if defined(STM32L1XXXX) && defined(OS_IO_TIM5)
     if (interruptIndex == OS_IO_TIM5) {
  #elif defined(STM32F2XXXX) || defined(STM32F4XXXX)
     if (interruptIndex == OS_IO_TIM2 || interruptIndex == OS_IO_TIM5) {
  #endif
  #if defined(STM32L1XXXX) && defined(OS_IO_TIM5) ||  defined(STM32F2XXXX) || defined(STM32F4XXXX)
        /* For 32-bit counter version, set the autoreload timer value to (2^30 - 1) */
        *(UINT32 *)(device->Base + OFFSET_AUTORELOAD) = SHIFT_TIME_LIMIT - 1;
     }
     else {
  #endif
        /* For 16-bit counter version, set the autoreload timer value to (2^16 -1) */
        *(UINT16 *)(device->Base + OFFSET_AUTORELOAD) = 0xFFFF;
  #if defined(STM32L1XXXX) && defined(OS_IO_TIM5) ||  defined(STM32F2XXXX) || defined(STM32F4XXXX)
     }
  #endif
  /* Set prescaler register */
  *(UINT16 *)(device->Base + OFFSET_PRESCALER) = prescaler;
  /* Generate an update event to force reload of registers value */
  *(UINT16 *)(device->Base + OFFSET_EVENT_GENERATION) = 1;
  /* Clear interrupts flags and enable interrupts */
  *(UINT16 *)(device->Base + OFFSET_STATUS) = (UINT16)~3;
  *(UINT16 *)(device->Base + OFFSET_INT_ENABLE) |= 3;
  /* Start the timer counter */
  *(UINT16 *)(device->Base + OFFSET_CONTROL1) |= 1;
} /* end of OSInitTimerEvent */


/* GetInterruptSubIndex: returns 1 if interrupt vector is share with timer 1 or timer 8. */
UINT8 GetInterruptSubIndex(UINT8 interruptIndex)
{
  switch (interruptIndex) {
  #ifdef OS_IO_TIM9
     case OS_IO_TIM9:
     #ifdef OS_IO_TIM1_BRK_TIM9
        return 1;
     #else
        return 0;
     #endif
  #endif
  #ifdef OS_IO_TIM10
     case OS_IO_TIM10:
     #ifdef OS_IO_TIM1_UP_TIM10
        return 1;
     #else
        return 0;
     #endif
  #endif
  #ifdef OS_IO_TIM11
     case OS_IO_TIM11:
     #ifdef OS_IO_TIM1_TRG_COM_TIM11
        return 1;
     #else
        return 0;
     #endif
  #endif
  #ifdef OS_IO_TIM12
     case OS_IO_TIM12:
     #ifdef OS_IO_TIM8_BRK_TIM12
        return 1;
     #else
        return 0;
     #endif
  #endif
  #ifdef OS_IO_TIM13
     case OS_IO_TIM13:
     #ifdef OS_IO_TIM8_UP_TIM13
        return 1;
     #else
        return 0;
     #endif
  #endif
  #ifdef OS_IO_TIM14
     case OS_IO_TIM14:
     #ifdef OS_IO_TIM8_TRG_COM_TIM14
        return 1;
     #else
        return 0;
     #endif
  #endif
  #ifdef OS_IO_TIM15
     case OS_IO_TIM15:
        return 1;
  #endif
  #ifdef OS_IO_TIM16
     case OS_IO_TIM16:
        return 1;
  #endif
  #ifdef OS_IO_TIM17
     case OS_IO_TIM17:
        return 1;
  #endif
     default:
     return 0;
  }
} /* end of GetInterruptSubIndex */


/* OSScheduleTimerEvent: Entry point to insert an event into the event list associated
** with a timer. */
BOOL OSScheduleTimerEvent(void *event, UINT32 delay, UINT8 interruptIndex)
{
  TIMER_EVENT_NODE *timerEventNode;
  TIMER_ISR_DATA *device;
  INSERTQUEUE_OP des, *pendingOp;
  device = (TIMER_ISR_DATA *)OSGetTimerISRDescriptor(interruptIndex,GetInterruptSubIndex(interruptIndex));
  if ((timerEventNode = GetFreeNode(device)) == NULL)
     return FALSE;
  /* Because the timer ISR can shift the current time, the time at which this event oc-
  ** curs is done in 2 steps. The time is determined here and then transferred to the
  ** event at the last moment so that if there is a time shift, it can be detected and
  ** corrected. */
  #if defined(STM32L1XXXX) && defined(OS_IO_TIM5)
     if (device->Base == BASE_TIM5) {
  #elif defined(STM32F2XXXX) || defined(STM32F4XXXX)
     if (device->Base == BASE_TIM2 || device->Base == BASE_TIM5) {
  #endif
  #if defined(STM32L1XXXX) && defined(OS_IO_TIM5) ||  defined(STM32F2XXXX) || defined(STM32F4XXXX)
        /* 32-bit counter version */
        do {
           des.Version = device->Time; // Save current version number to detect current time shifting
           des.Time = *(UINT32 *)(device->Base + OFFSET_COUNTER) + delay;
        } while (des.Version != device->Time);
     }
     else {
  #endif
        /* 16-bit counter version */
        do {
           des.Version = device->Time; // Save current 16-bit MSB time to detect current time shifting
           des.Time = *(UINT16 *)(device->Base + OFFSET_COUNTER) + delay + (des.Version << 16);
        } while (des.Version != device->Time);
  #if defined(STM32L1XXXX) && defined(OS_IO_TIM5) ||  defined(STM32F2XXXX) || defined(STM32F4XXXX)
     }
  #endif
  timerEventNode->Time = -1;   // Set at an uninitialized sentinel value
  timerEventNode->Event = event;
  des.EventOp = InsertEventOp; // Prepare the insertion so that other task may complete
  des.Done = FALSE;            // it.
  des.Left = (TIMER_EVENT_NODE *)&device->EventQueue;
  des.Node = timerEventNode;
  des.GeneratedInterrupt = FALSE;
  pendingOp = (INSERTQUEUE_OP *)device->PendingQueueOperation;
  if (pendingOp != NULL) {       // Is there an incomplete operation under way?
     if (pendingOp->EventOp == InsertEventOp)
        InsertQueueHelper(pendingOp,device,TRUE);
     else
        DeleteQueueHelper((DELETEQUEUE_OP *)pendingOp);
  }
  /* Mark the node to allow a single insertion of the node */
  timerEventNode->Next = (TIMER_EVENT_NODE *)&des;
  device->PendingQueueOperation = &des; // Post the insertion for other tasks
  InsertQueueHelper(&des,device,TRUE);  // Do the insertion
  device->PendingQueueOperation = NULL;
  return TRUE;
} /* end of OSScheduleTimerEvent */


/* InsertQueueHelper: Performs the insertion described by a descriptor into a sorted
** queue. Whenever an enqueuer is interrupted in the midst of the insertion by another
** task, the latter task inserts the node of the interrupted enqueuer before completing
** its own operation.
** Parameters:
**   (1) (INSERTQUEUE_OP *) insertion operation to perform;
**   (2) (TIMER_ISR_DATA *) descriptor to the timer in order to retrieve the current time
**          and to generate an interrupt if needed;
**   (3) (BOOL) TRUE when not called from the timer ISR. */
void InsertQueueHelper(INSERTQUEUE_OP *des, TIMER_ISR_DATA *device, BOOL genInterrupt)
{
  INT32 scheduledTime;
  TIMER_EVENT_NODE *right, *left = des->Left;
  /* Finish completing the time at which the event takes place. This is done here so that
  ** the timer ISR can correct this time. Because the timer ISR shifts its current time
  ** to avoid overflow, the event occurrence time may not be finalized without the ISR's
  ** awareness. */
  #if defined(STM32L1XXXX) && defined(OS_IO_TIM5)
     if (device->Base == BASE_TIM5) {
  #elif defined(STM32F2XXXX) || defined(STM32F4XXXX)
     if (device->Base == BASE_TIM2 || device->Base == BASE_TIM5) {
  #endif
  #if defined(STM32L1XXXX) && defined(OS_IO_TIM5) ||  defined(STM32F2XXXX) || defined(STM32F4XXXX)
        /* 32-bit counter version */
        if (OSINT32_LL(&des->Node->Time) == -1) {
           if (des->Version == device->Time) // If version backup is different than current a time shift occurred
              scheduledTime = des->Time;
           else
              scheduledTime = des->Time - SHIFT_TIME_LIMIT;
           while (OSINT32_LL(&des->Node->Time) == -1)
              if (OSINT32_SC(&des->Node->Time,scheduledTime))
                 break;
        }
     }
     else {
  #endif
        /* 16-bit counter version */
        if (OSINT32_LL(&des->Node->Time) == -1) {
           if (des->Version > device->Time) // If 16-bit MSB backup is greater than current a time shift occurred
              scheduledTime = des->Time - SHIFT_TIME_LIMIT;
           else
              scheduledTime = des->Time;
           while (OSINT32_LL(&des->Node->Time) == -1)
              if (OSINT32_SC(&des->Node->Time,scheduledTime))
                 break;
        }
  #if defined(STM32L1XXXX) && defined(OS_IO_TIM5) ||  defined(STM32F2XXXX) || defined(STM32F4XXXX)
     }
  #endif
  while (TRUE) {         // Loop until done
     right = left->Next; // Get adjacent node
     if (des->Done)      // Has a higher priority task finished the work?
        break;
     else if (right == des->Node) { // Is the node already in the queue?
        des->Done = TRUE;           // Mark that node has been inserted
        break;
     }
     else if (right == NULL || des->Node->Time < right->Time) {
        UINTPTR *successorNode = (UINTPTR *)&des->Node->Next;
        while (OSUINTPTR_LL(successorNode) == (UINTPTR)des)
           if (OSUINTPTR_SC(successorNode,(UINTPTR)right))
              break;
        while (OSUINTPTR_LL((UINTPTR *)&left->Next) == (UINTPTR)right) {
           /* At this point an ABA problem can arise: Say that a task inserts node N
           ** between nodes A and B, and gets preempted. Another task completes the in-
           ** sertion, and then removes N. In this case, node N must not be reinserted.
           ** To correct this flaw, we simply need to check if another task completed the
           ** current insertion. */
           if (des->Done || OSUINTPTR_SC((UINTPTR *)&left->Next,(UINTPTR)des->Node))
              break;
        }
        des->Done = TRUE; // Mark that node has been inserted
        break;
     }
     else
        left = des->Left = right;
  }
  /* Generate a timer comparator interrupt if the inserted event is the first one. */
  if (genInterrupt &&  device->EventQueue == des->Node) {
     #if defined(STM32L1XXXX) && defined(OS_IO_TIM5)
        if (device->Base == BASE_TIM5) {
     #elif defined(STM32F2XXXX) || defined(STM32F4XXXX)
        if (device->Base == BASE_TIM2 || device->Base == BASE_TIM5) {
     #endif
     #if defined(STM32L1XXXX) && defined(OS_IO_TIM5) ||  defined(STM32F2XXXX) || defined(STM32F4XXXX)
           /* 32-bit counter version */
           do {
              OSUINT32_LL((UINT32 *)(device->Base + OFFSET_COMPARATOR));
              if (des->GeneratedInterrupt) return;
           } while (!OSUINT32_SC((UINT32 *)(device->Base + OFFSET_COMPARATOR),des->Node->Time));
           if (*(UINT32 *)(device->Base + OFFSET_COMPARATOR) <= *(UINT32 *)(device->Base + OFFSET_COUNTER))
              do {
                 OSUINT16_LL((UINT16 *)(device->Base + OFFSET_EVENT_GENERATION));
                 if (des->GeneratedInterrupt) return;
              } while (!OSUINT16_SC((UINT16 *)(device->Base + OFFSET_EVENT_GENERATION),2));
        }
        else {
     #endif
           /* 16-bit counter version */
           do {
              OSUINT16_LL((UINT16 *)(device->Base + OFFSET_COMPARATOR));
              if (des->GeneratedInterrupt) return;
           } while (!OSUINT16_SC((UINT16 *)(device->Base + OFFSET_COMPARATOR),des->Node->Time));
           if (*(UINT16 *)(device->Base + OFFSET_COMPARATOR) <= *(UINT16 *)(device->Base + OFFSET_COUNTER))
              do {
                 OSUINT16_LL((UINT16 *)(device->Base + OFFSET_EVENT_GENERATION));
                 if (des->GeneratedInterrupt) return;
              } while (!OSUINT16_SC((UINT16 *)(device->Base + OFFSET_EVENT_GENERATION),2));
     #if defined(STM32L1XXXX) && defined(OS_IO_TIM5) ||  defined(STM32F2XXXX) || defined(STM32F4XXXX)
        }
     #endif
  }
  des->GeneratedInterrupt = TRUE;
} /* end of InsertQueueHelper */


/* OSUnScheduleTimerEvent: Entry point to remove the first occurrence of a specified
** event from the list associated with a timer. */
BOOL OSUnScheduleTimerEvent(void *event, UINT8 interruptIndex)
{
  DELETEQUEUE_OP des, *pendingOp;
  TIMER_ISR_DATA *device;
  des.EventOp = DeleteEventOp;  // Prepare the removal so that other task may complete
  des.Done = FALSE;             // it.
  device = (TIMER_ISR_DATA *)OSGetTimerISRDescriptor(interruptIndex,GetInterruptSubIndex(interruptIndex));
  des.Left = (TIMER_EVENT_NODE *)&device->EventQueue;
  des.Event = event;
  des.Node = (TIMER_EVENT_NODE *)&des;
  pendingOp = device->PendingQueueOperation;
  if (pendingOp != NULL) {      // Is there an incomplete operation under way?
     if (pendingOp->EventOp == InsertEventOp)
        InsertQueueHelper((INSERTQUEUE_OP *)pendingOp,device,TRUE);
     else
        DeleteQueueHelper(pendingOp);
  }
  device->PendingQueueOperation = &des;    // Post the removal for other tasks
  DeleteQueueHelper(&des);                 // Do the operation
  device->PendingQueueOperation = NULL;
  if (des.Node != (TIMER_EVENT_NODE *)&des) {
     ReleaseNode(device,des.Node);
     return TRUE;
  }
  else
     return FALSE;
} /* end of OSUnScheduleTimerEvent */


/* DeleteQueueHelper: Performs the delete operation described by a descriptor from a list
** of events. Whenever a deleter is interrupted in the midst of this operation by another
** task, the latter task removes the node of the interrupted deleter before completing
** its own operation. */
void DeleteQueueHelper(DELETEQUEUE_OP *des)
{
  TIMER_EVENT_NODE *right, *left = des->Left;
  while (TRUE) {             // Loop until done
     right = left->Next;     // Get adjacent node
     if (des->Done)          // Has a higher priority task finished the work?
        break;
     else if (right == NULL) {  // Is the seeked node already removed from the queue?
        des->Done = TRUE;       // Mark that node has been inserted
        break;
     }
     else if (des->Event == right->Event) {
        while (OSUINTPTR_LL((UINTPTR *)&des->Node) == (UINTPTR)des && !des->Done)
           if (OSUINTPTR_SC((UINTPTR *)&des->Node,(UINTPTR)right))
              break;
        while (OSUINTPTR_LL((UINTPTR *)&left->Next) == (UINTPTR)right)
           if (des->Done || OSUINTPTR_SC((UINTPTR *)&left->Next,(UINTPTR)right->Next))
              break;
        des->Done = TRUE;    // Mark that the node has been removed
        break;
     }
     else
        left = des->Left = right;
  }
} /* end of DeleteQueueHelper */


/* GetFreeNode: : Returns a node from the pool of free nodes. NULL is returned when
** none are available. The free node list is considered to be a stack of nodes. */
TIMER_EVENT_NODE *GetFreeNode(TIMER_ISR_DATA *des)
{
  TIMER_EVENT_NODE *node;
  while ((node = (TIMER_EVENT_NODE *)OSUINTPTR_LL((UINTPTR *)&des->FreeNodes)) != NULL)
     if (OSUINTPTR_SC((UINTPTR *)&des->FreeNodes,(UINTPTR)node->Next))
        break;
  return node;
} /* end of GetFreeNode */


/* ReleaseNode: Returns a node to the pool of free nodes. */
void ReleaseNode(TIMER_ISR_DATA *device, TIMER_EVENT_NODE *freeNode)
{
  while (TRUE) {
     freeNode->Next = (TIMER_EVENT_NODE *)OSUINTPTR_LL((UINTPTR *)&device->FreeNodes);
     if (OSUINTPTR_SC((UINTPTR *)&device->FreeNodes,(UINTPTR)freeNode))
        return;
  }
} /* end of ReleaseNode */


void TimerIntHandler(TIMER_ISR_DATA *device)
{
  TIMER_EVENT_NODE *eventNode;
  INSERTQUEUE_OP *pendingOp;
  INT32 timeMSB;
  /* First finalize any pending operation in case there is an inserted node that already
  ** has its occurrence time but is not yet in the queue. */
  pendingOp = (INSERTQUEUE_OP *)device->PendingQueueOperation;
  if (pendingOp != NULL) {       // Is there an incomplete operation under way?
     if (pendingOp->EventOp == InsertEventOp)
        InsertQueueHelper(pendingOp,device,FALSE);
     else
        DeleteQueueHelper((DELETEQUEUE_OP *)pendingOp);
  }
  device->PendingQueueOperation = NULL;
  do {
     /* shifting of the temporal values. */
     if (*(UINT16 *)(device->Base + OFFSET_STATUS) & 1) { // Is timer overflow interrupt?
        *(UINT16 *)(device->Base + OFFSET_STATUS) &= ~1;  // Clear interrupt flag
        #if defined(STM32L1XXXX) && defined(OS_IO_TIM5)
           if (device->Base == BASE_TIM5) {
        #elif defined(STM32F2XXXX) || defined(STM32F4XXXX)
           if (device->Base == BASE_TIM2 || device->Base == BASE_TIM5) {
        #endif
        #if defined(STM32L1XXXX) && defined(OS_IO_TIM5) ||  defined(STM32F2XXXX) || defined(STM32F4XXXX)
              /* 32-bit counter version */
              eventNode = device->EventQueue;
              while (eventNode != NULL) {
                 eventNode->Time -= SHIFT_TIME_LIMIT;
                 eventNode = eventNode->Next;
              }
              device->Time += 1; // Increment version number
           }
           else {
        #endif
              /* 16-bit counter version */
              device->Time += 1; // Increment MSB of time
              if (device->Time >= SHIFT_TIME_LIMIT_16) { // Shift all time value
                 eventNode = device->EventQueue;
                 while (eventNode != NULL) {
                    eventNode->Time -= SHIFT_TIME_LIMIT;
                    eventNode = eventNode->Next;
                 }
                 device->Time -= SHIFT_TIME_LIMIT_16;
              }
        #if defined(STM32L1XXXX) && defined(OS_IO_TIM5) ||  defined(STM32F2XXXX) || defined(STM32F4XXXX)
           }
        #endif
     }
     while (TRUE) {
        /* Disable timer comparator */
        #if defined(STM32L1XXXX) && defined(OS_IO_TIM5)
           if (device->Base == BASE_TIM5) {
        #elif defined(STM32F2XXXX) || defined(STM32F4XXXX)
           if (device->Base == BASE_TIM2 || device->Base == BASE_TIM5) {
        #endif
        #if defined(STM32L1XXXX) && defined(OS_IO_TIM5) ||  defined(STM32F2XXXX) || defined(STM32F4XXXX)
              /* 32-bit counter version */
              *(UINT32 *)(device->Base + OFFSET_COMPARATOR) = 0;
           }
           else {
        #endif
              /* 16-bit counter version */
              *(UINT16 *)(device->Base + OFFSET_COMPARATOR) = 0;
        #if defined(STM32L1XXXX) && defined(OS_IO_TIM5) ||  defined(STM32F2XXXX) || defined(STM32F4XXXX)
           }
        #endif
        /* Clear interrupt flag */
        *(UINT16 *)(device->Base + OFFSET_STATUS) &= ~2;
        #if defined(STM32L1XXXX) && defined(OS_IO_TIM5)
           if (device->Base == BASE_TIM5) {
        #elif defined(STM32F2XXXX) || defined(STM32F4XXXX)
           if (device->Base == BASE_TIM2 || device->Base == BASE_TIM5) {
        #endif
        #if defined(STM32L1XXXX) && defined(OS_IO_TIM5) ||  defined(STM32F2XXXX) || defined(STM32F4XXXX)
               /* 32-bit counter version */
              /* Schedule events that now occur */
              while ((eventNode = device->EventQueue) != NULL && eventNode->Time <= *(UINT32 *)(device->Base + OFFSET_COUNTER)) {
                 OSScheduleSuspendedTask(eventNode->Event);
                 device->EventQueue = eventNode->Next;
                 ReleaseNode(device,eventNode);
              }
              /* Program the next timer comparator */
              if (eventNode != NULL) {
                 *(UINT32 *)(device->Base + OFFSET_COMPARATOR) = eventNode->Time;
                 if (*(UINT32 *)(device->Base + OFFSET_COMPARATOR) > *(UINT32 *)(device->Base + OFFSET_COUNTER))
                    break;
              }
              else
                 break;
           }
           else {
        #endif
              /* 16-bit counter version */
              /* Schedule events that now occur */
              timeMSB = device->Time << 16;
              while ((eventNode = device->EventQueue) != NULL && eventNode->Time <=
                                 (timeMSB | *(UINT16 *)(device->Base + OFFSET_COUNTER))) {
                 OSScheduleSuspendedTask(eventNode->Event);
                 device->EventQueue = eventNode->Next;
                 ReleaseNode(device,eventNode);
              }
              /* Program the next timer comparator */
              if (eventNode != NULL && (eventNode->Time & 0xFFFF0000) == timeMSB) {
                 *(UINT16 *)(device->Base + OFFSET_COMPARATOR) = (UINT16)eventNode->Time;
                 if (*(UINT16 *)(device->Base + OFFSET_COMPARATOR) >
                                               *(UINT16 *)(device->Base + OFFSET_COUNTER))
                    break;
              }
              else
                 break;
        #if defined(STM32L1XXXX) && defined(OS_IO_TIM5) ||  defined(STM32F2XXXX) || defined(STM32F4XXXX)
           }
        #endif
     }
  } while ((*(UINT16 *)(device->Base + OFFSET_STATUS) & 3) != 0);
} /* end of TimerIntHandler */

