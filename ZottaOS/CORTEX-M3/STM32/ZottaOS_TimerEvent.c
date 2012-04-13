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
/* File TimerEvent.c: Implements an event queue that is associated with a timer device.
** Events that are inserted into the queue are specified along with a delay, and as soon
** as this delay has expired, the event is scheduled.
** Platform version: All STM32 microcontrollers.
** Version date: March 2012
** Authors: MIS-TIC */

#include "ZottaOS_CortexM3.h"
#include "ZottaOS.h"
#include "ZottaOS_TimerEvent.h"

/* Because the timer continues ticking, when we wish to set a new value for the timer
** comparator, the difference in time between the new value and the previous must be such
** that when the assignment is done, the timer has not passed the comparator value. This
** is guaranteed by TIMER_OFFSET. */
#define TIMER_OFFSET 2
/* Because events are sorted by the time at which they occur, and that this time is mon-
** otonically increasing, the relative time reference wraparounds. We therefore need to
** periodically shift the occurrence times. This is done very SHIFT_TIME_LIMIT tics. */
#define SHIFT_TIME_LIMIT 0x40000000 // = 2^30

#ifdef OS_IO_TIM1
   #if defined(STM32F1XXXX)
      #define BASE_TIM1 0x40012C00
   #elif defined(STM32F2XXXX) || defined(STM32F4XXXX)
      #define BASE_TIM1 0x40010000
   #else
      #error Base address must be specified
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
      #error Base address must be specified
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
      #error Base address must be specified
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
      #error Base address must be specified
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
      #error Base address must be specified
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

typedef struct { // Queue of events with their associated time of occurrence
  TIMER_EVENT_NODE *Next;
} TIMER_EVENT_NODE_HEAD;

typedef struct TIMER_ISR_DATA {
  void (*TimerIntHandler)(struct TIMER_ISR_DATA *);
  UINT32 *ClkEnable;
  UINT32 ClkEnableBit;
  UINT32 Base;
  void *PendingQueueOperation;       // Interrupted operations that are not complete
  TIMER_EVENT_NODE_HEAD *EventQueue; // List of events sorted by their time of occurrence
  TIMER_EVENT_NODE *FreeNodes;       // Pool of free nodes used to link events
  UINT16 Version;                    // Number of timer shifts
  INT32 Time;                        // Current time (allocated only on 16-bit timers)
} TIMER_ISR_DATA;


typedef enum {InsertEventOp,DeleteEventOp} EVENTOP;

typedef struct {
  EVENTOP EventOp;
  volatile BOOL Done;
  TIMER_EVENT_NODE *Left;
  TIMER_EVENT_NODE *Node;
  volatile BOOL GeneratedInterrupt;
  UINT16 Version;                    // Saved timer shift version
  INT32 Time;                        // Interrupt time of the event
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
void OSInitTimerEvent(UINT8 nbNode, UINT16 prescaler, UINT8 priority, UINT8 subpriority, UINT8 interruptIndex)
{
  TIMER_ISR_DATA *device;
  UINT8 i, tmppriority, *intPriorityLevel;
  UINT32 *intSetEnable;
  #if defined(STM32L1XXXX)
     if (interruptIndex == OS_IO_TIM5) {
  #elif defined(STM32F2XXXX) || defined(STM32F4XXXX)
     if (interruptIndex == OS_IO_TIM2 || interruptIndex == OS_IO_TIM5) {
  #endif
  #if defined(STM32L1XXXX) ||  defined(STM32F2XXXX) || defined(STM32F4XXXX)
        /* The current wall clock is entirely given in the timers's counter register.
        ** Hence we do not need to keep track of the current time. */
        device = (TIMER_ISR_DATA *)OSMalloc(sizeof(TIMER_ISR_DATA) - sizeof(INT32));
     }
     else {
  #endif
        /* On 16-bit timers, the current is given by the sum of the timer's counter
        ** augmented by the number of times that this counter overflowed. This overflow
        ** value is stored in the devices's Time field. */
        device = (TIMER_ISR_DATA *)OSMalloc(sizeof(TIMER_ISR_DATA));
  #if defined(STM32L1XXXX) ||  defined(STM32F2XXXX) || defined(STM32F4XXXX)
     }
  #endif

  /* Compute the IRQ priority */
  tmppriority = priority << (PRIGROUP - 3);
  // le nombre 3 correspond au nombre maximum de bits pour la priorité moins le nombre de bit implémenté soit (7 -4 pour le stm32)
  tmppriority |=  subpriority & (0x0F >> (7 - PRIGROUP));
  // (7 - PRIGROUP) correspond aux nombres de bits pour la priorité.
  // (4 correpond à 8 moins le nombre de bit implémenter dans le STM32(4))

  switch (interruptIndex) { /* Specific timer device registers */
     #ifdef OS_IO_TIM1
     case OS_IO_TIM1:
        #if defined(STM32F1XXXX)
           device->ClkEnable = (UINT32 *)0x40021018; // RCC_APB2ENR
           device->ClkEnableBit = 0x800;
        #elif defined(STM32F2XXXX) || defined(STM32F4XXXX)
           device->ClkEnable = (UINT32 *)0x40023844; // RCC_APB2ENR
           //ClkEnable = (UINT32 *)0x40023864; // RCC_APB2LPENR
           device->ClkEnableBit = 0x1;
        #endif
        device->Base = BASE_TIM1;
        OSSetISRDescriptor(OS_IO_TIM1_UP,0,device);
        OSSetISRDescriptor(OS_IO_TIM1_CC,0,device);

        intSetEnable = (UINT32 *)0xE000E100 + (UINT32)(OS_IO_TIM1_UP / 32);
        *intSetEnable |= 0x01 << (OS_IO_TIM1_UP % 32); // Enable the IRQ channels
        intPriorityLevel = (UINT8 *)(0xE000E400 + OS_IO_TIM1_UP);
        *intPriorityLevel = tmppriority << 0x04; // Set the IRQ priority
        intSetEnable = (UINT32 *)0xE000E100 + (UINT32)(OS_IO_TIM1_CC / 32);
        *intSetEnable |= 0x01 << (OS_IO_TIM1_CC % 32); // Enable the IRQ channels
        intPriorityLevel = (UINT8 *)(0xE000E400 + OS_IO_TIM1_CC);
        *intPriorityLevel = tmppriority << 0x04; // Set the IRQ priority

        break;
     #endif
     #ifdef OS_IO_TIM2
     case OS_IO_TIM2:
        #if defined(STM32L1XXXX)
           device->ClkEnable = (UINT32 *)0x40023824; // RCC_APB1ENR
           //device->ClkEnable = (UINT32 *)0x40023830; // RCC_APB1LPENR
        #elif defined(STM32F1XXXX)
           device->ClkEnable = (UINT32 *)0x4002101C; // RCC_APB1ENR
        #elif defined(STM32F2XXXX) || defined(STM32F4XXXX)
           device->ClkEnable = (UINT32 *)0x40023840; // RCC_APB1ENR
           //device->ClkEnable = (UINT32 *)0x40023860; // RCC_APB1LPENR
        #endif
        device->ClkEnableBit = 0x1;
        device->Base = BASE_TIM2;
        OSSetISRDescriptor(OS_IO_TIM2,0,device);

        intSetEnable = (UINT32 *)0xE000E100 + (UINT32)(interruptIndex / 32);
        *intSetEnable |= 0x01 << (interruptIndex % 32); // Enable the IRQ channels
        intPriorityLevel = (UINT8 *)(0xE000E400 + interruptIndex);
        *intPriorityLevel = tmppriority << 0x04; // Set the IRQ priority

        break;
     #endif
     #ifdef OS_IO_TIM3
     case OS_IO_TIM3:
        #if defined(STM32L1XXXX)
           device->ClkEnable = (UINT32 *)0x40023824; // RCC_APB1ENR
           //device->ClkEnable = (UINT32 *)0x40023830; // RCC_APB1LPENR
        #elif defined(STM32F1XXXX)
           device->ClkEnable = (UINT32 *)0x4002101C; // RCC_APB1ENR
        #elif defined(STM32F2XXXX) || defined(STM32F4XXXX)
           device->ClkEnable = (UINT32 *)0x40023840; // RCC_APB1ENR
           //device->ClkEnable = (UINT32 *)0x40023860; // RCC_APB1LPENR
        #endif
        device->ClkEnableBit = 0x2;
        device->Base = BASE_TIM3;
        OSSetISRDescriptor(OS_IO_TIM3,0,device);

        intSetEnable = (UINT32 *)0xE000E100 + (UINT32)(interruptIndex / 32);
        *intSetEnable |= 0x01 << (interruptIndex % 32); // Enable the IRQ channels
        intPriorityLevel = (UINT8 *)(0xE000E400 + interruptIndex);
        *intPriorityLevel = tmppriority << 0x04; // Set the IRQ priority

        break;
     #endif
     #ifdef OS_IO_TIM4
     case OS_IO_TIM4:
        #if defined(STM32L1XXXX)
           device->ClkEnable = (UINT32 *)0x40023824; // RCC_APB1ENR
           //device->ClkEnable = (UINT32 *)0x40023830; // RCC_APB1LPENR
        #elif defined(STM32F1XXXX)
           device->ClkEnable = (UINT32 *)0x4002101C; // RCC_APB1ENR
        #elif defined(STM32F2XXXX) || defined(STM32F4XXXX)
           device->ClkEnable = (UINT32 *)0x40023840; // RCC_APB1ENR
           //device->ClkEnable = (UINT32 *)0x40023860; // RCC_APB1LPENR
        #endif
        device->ClkEnableBit = 0x4;
        device->Base = BASE_TIM4;
        OSSetISRDescriptor(OS_IO_TIM4,0,device);

        intSetEnable = (UINT32 *)0xE000E100 + (UINT32)(interruptIndex / 32);
        *intSetEnable |= 0x01 << (interruptIndex % 32); // Enable the IRQ channels
        intPriorityLevel = (UINT8 *)(0xE000E400 + interruptIndex);
        *intPriorityLevel = tmppriority << 0x04; // Set the IRQ priority

        break;
     #endif
     #ifdef OS_IO_TIM5
     case OS_IO_TIM5:
        #if defined(STM32L1XXXX)
           device->ClkEnable = (UINT32 *)0x40023824; // RCC_APB1ENR
           //device->ClkEnable = (UINT32 *)0x40023830; // RCC_APB1LPENR
        #elif defined(STM32F1XXXX)
           device->ClkEnable = (UINT32 *)0x4002101C; // RCC_APB1ENR
        #elif defined(STM32F2XXXX) || defined(STM32F4XXXX)
           device->ClkEnable = (UINT32 *)0x40023840; // RCC_APB1ENR
           //device->ClkEnable = (UINT32 *)0x40023860; // RCC_APB1LPENR
        #endif
        device->ClkEnableBit = 0x8;
        device->Base = BASE_TIM5;
        OSSetISRDescriptor(OS_IO_TIM5,0,device);

        intSetEnable = (UINT32 *)0xE000E100 + (UINT32)(interruptIndex / 32);
        *intSetEnable |= 0x01 << (interruptIndex % 32); // Enable the IRQ channels
        intPriorityLevel = (UINT8 *)(0xE000E400 + interruptIndex);
        *intPriorityLevel = tmppriority << 0x04; // Set the IRQ priority

        break;
     #endif
     #ifdef OS_IO_TIM8
     case OS_IO_TIM8:
        #if defined(STM32F1XXXX)
           device->ClkEnable = (UINT32 *)0x40021018; // RCC_APB2ENR
           device->ClkEnableBit = 0x2000;
        #elif defined(STM32F2XXXX) || defined(STM32F4XXXX)
           device->ClkEnable = (UINT32 *)0x40023844; // RCC_APB2ENR
           //device->ClkEnable = (UINT32 *)0x40023864; // RCC_APB2LPENR
           device->ClkEnableBit = 0x2;
        #endif
        device->Base = BASE_TIM8;
        OSSetISRDescriptor(OS_IO_TIM8_UP,0,device);
        OSSetISRDescriptor(OS_IO_TIM8_CC,0,device);

        intSetEnable = (UINT32 *)0xE000E100 + (UINT32)(OS_IO_TIM8_UP / 32);
        *intSetEnable |= 0x01 << (OS_IO_TIM8_UP % 32); // Enable the IRQ channels
        intPriorityLevel = (UINT8 *)(0xE000E400 + OS_IO_TIM8_UP);
        *intPriorityLevel = tmppriority << 0x04; // Set the IRQ priority
        intSetEnable = (UINT32 *)0xE000E100 + (UINT32)(OS_IO_TIM8_CC / 32);
        *intSetEnable |= 0x01 << (OS_IO_TIM8_CC % 32); // Enable the IRQ channels
        intPriorityLevel = (UINT8 *)(0xE000E400 + OS_IO_TIM8_CC);
        *intPriorityLevel = tmppriority << 0x04; // Set the IRQ priority

        break;
     #endif
     #ifdef OS_IO_TIM9
     case OS_IO_TIM9:
        #if defined(STM32L1XXXX)
           device->ClkEnable = (UINT32 *)0x40023820; // RCC_APB2ENR
           //device->ClkEnable = (UINT32 *)0x4002382C; // RCC_APB2LPENR
           device->ClkEnableBit = 0x04
        #elif defined(STM32F1XXXX)
           device->ClkEnable = (UINT32 *)0x40021018; // RCC_APB2ENR
           device->ClkEnableBit = 0x080000;
        #elif defined(STM32F2XXXX) || defined(STM32F4XXXX)
           device->ClkEnable = (UINT32 *)0x40023844; // RCC_APB2ENR
           //device->ClkEnable = (UINT32 *)0x40023864; // RCC_APB2LPENR
           device->ClkEnableBit = 0x10000;
        #endif
        device->Base = BASE_TIM9;

        #ifdef OS_IO_TIM1_BRK_TIM9
           OSSetISRDescriptor(OS_IO_TIM1_BRK_TIM9,1,device);
        #else
           OSSetISRDescriptor(OS_IO_TIM9,0,device);
        #endif

        intSetEnable = (UINT32 *)0xE000E100 + (UINT32)(interruptIndex / 32);
        *intSetEnable |= 0x01 << (interruptIndex % 32); // Enable the IRQ channels
        intPriorityLevel = (UINT8 *)(0xE000E400 + interruptIndex);
        *intPriorityLevel = tmppriority << 0x04; // Set the IRQ priority

        break;
     #endif
     #ifdef OS_IO_TIM10
     case OS_IO_TIM10:
        #if defined(STM32L1XXXX)
           device->ClkEnable = (UINT32 *)0x40023820; // RCC_APB2ENR
           //device->ClkEnable = (UINT32 *)0x4002382C; // RCC_APB2LPENR
           device->ClkEnableBit = 0x08
        #elif defined(STM32F1XXXX)
           device->ClkEnable = (UINT32 *)0x40021018; // RCC_APB2ENR
           device->ClkEnableBit = 0x100000;
        #elif defined(STM32F2XXXX) || defined(STM32F4XXXX)
           device->ClkEnable = (UINT32 *)0x40023844; // RCC_APB2ENR
           //device->ClkEnable = (UINT32 *)0x40023864; // RCC_APB2LPENR
           device->ClkEnableBit = 0x20000;
        #endif
        device->Base = BASE_TIM10;

        #ifdef OS_IO_TIM1_UP_TIM10
           OSSetISRDescriptor(OS_IO_TIM1_UP_TIM10,1,device);
        #else
           OSSetISRDescriptor(OS_IO_TIM10,0,device);
        #endif

        intSetEnable = (UINT32 *)0xE000E100 + (UINT32)(interruptIndex / 32);
        *intSetEnable |= 0x01 << (interruptIndex % 32); // Enable the IRQ channels
        intPriorityLevel = (UINT8 *)(0xE000E400 + interruptIndex);
        *intPriorityLevel = tmppriority << 0x04; // Set the IRQ priority

        break;
     #endif
     #ifdef OS_IO_TIM11
     case OS_IO_TIM11:
        #if defined(STM32L1XXXX)
           device->ClkEnable = (UINT32 *)0x40023820; // RCC_APB2ENR
           //device->ClkEnable = (UINT32 *)0x4002382C; // RCC_APB2LPENR
           device->ClkEnableBit = 0x10
        #elif defined(STM32F1XXXX)
           device->ClkEnable = (UINT32 *)0x40021018; // RCC_APB2ENR
           device->ClkEnableBit = 0x200000;
        #elif defined(STM32F2XXXX) || defined(STM32F4XXXX)
           device->ClkEnable = (UINT32 *)0x40023844; // RCC_APB2ENR
           //device->ClkEnable = (UINT32 *)0x40023864; // RCC_APB2LPENR
           device->ClkEnableBit = 0x40000;
        #endif
        device->Base = BASE_TIM11;

        #ifdef OS_IO_TIM1_TRG_COM_TIM11
           OSSetISRDescriptor(OS_IO_TIM1_TRG_COM_TIM11,1,device);
        #else
           OSSetISRDescriptor(OS_IO_TIM11,0,device);
        #endif

        intSetEnable = (UINT32 *)0xE000E100 + (UINT32)(interruptIndex / 32);
        *intSetEnable |= 0x01 << (interruptIndex % 32); // Enable the IRQ channels
        intPriorityLevel = (UINT8 *)(0xE000E400 + interruptIndex);
        *intPriorityLevel = tmppriority << 0x04; // Set the IRQ priority

        break;
     #endif
     #ifdef OS_IO_TIM12
     case OS_IO_TIM12:
        #if defined(STM32F1XXXX)
           device->ClkEnable = (UINT32 *)0x4002101C; // RCC_APB1ENR
        #elif defined(STM32F2XXXX) || defined(STM32F4XXXX)
           device->ClkEnable = (UINT32 *)0x40023840; // RCC_APB1ENR
           //device->ClkEnable = (UINT32 *)0x40023860; // RCC_APB1LPENR
        #endif
        device->ClkEnableBit = 0x40;
        device->Base = BASE_TIM12;

        #ifdef OS_IO_TIM8_BRK_TIM12
           OSSetISRDescriptor(OS_IO_TIM8_BRK_TIM12,1,device);
        #else
           OSSetISRDescriptor(OS_IO_TIM12,0,device);
        #endif

        intSetEnable = (UINT32 *)0xE000E100 + (UINT32)(interruptIndex / 32);
        *intSetEnable |= 0x01 << (interruptIndex % 32); // Enable the IRQ channels
        intPriorityLevel = (UINT8 *)(0xE000E400 + interruptIndex);
        *intPriorityLevel = tmppriority << 0x04; // Set the IRQ priority

        break;
     #endif
     #ifdef OS_IO_TIM13
     case OS_IO_TIM13:
        #if defined(STM32F1XXXX)
           device->ClkEnable = (UINT32 *)0x4002101C; // RCC_APB1ENR
        #elif defined(STM32F2XXXX) || defined(STM32F4XXXX)
           device->ClkEnable = (UINT32 *)0x40023840; // RCC_APB1ENR
           //device->ClkEnable = (UINT32 *)0x40023860; // RCC_APB1LPENR
        #endif
        device->ClkEnableBit = 0x80;
        device->Base = BASE_TIM13;

        #ifdef OS_IO_TIM8_UP_TIM13
           OSSetISRDescriptor(OS_IO_TIM8_UP_TIM13,1,device);
        #else
           OSSetISRDescriptor(OS_IO_TIM13,0,device);
        #endif

        intSetEnable = (UINT32 *)0xE000E100 + (UINT32)(interruptIndex / 32);
        *intSetEnable |= 0x01 << (interruptIndex % 32); // Enable the IRQ channels
        intPriorityLevel = (UINT8 *)(0xE000E400 + interruptIndex);
        *intPriorityLevel = tmppriority << 0x04; // Set the IRQ priority

        break;
     #endif
     #ifdef OS_IO_TIM14
     case OS_IO_TIM14:
        #if defined(STM32F1XXXX)
           device->ClkEnable = (UINT32 *)0x4002101C; // RCC_APB1ENR
        #elif defined(STM32F2XXXX) || defined(STM32F4XXXX)
           device->ClkEnable = (UINT32 *)0x40023840; // RCC_APB1ENR
           //device->ClkEnable = (UINT32 *)0x40023860; // RCC_APB1LPENR
        #endif
        device->ClkEnableBit = 0x100;
        device->Base = BASE_TIM14;

        #ifdef OS_IO_TIM8_TRG_COM_TIM14
           OSSetISRDescriptor(OS_IO_TIM8_TRG_COM_TIM14,1,device);
        #else
           OSSetISRDescriptor(OS_IO_TIM14,0,device);
        #endif

        intSetEnable = (UINT32 *)0xE000E100 + (UINT32)(interruptIndex / 32);
        *intSetEnable |= 0x01 << (interruptIndex % 32); // Enable the IRQ channels
        intPriorityLevel = (UINT8 *)(0xE000E400 + interruptIndex);
        *intPriorityLevel = tmppriority << 0x04; // Set the IRQ priority

        break;
     #endif
     #ifdef OS_IO_TIM15
     case OS_IO_TIM15:
        device->ClkEnable = (UINT32 *)0x40021018; // RCC_APB2ENR
        device->ClkEnableBit = 0x10000;
        device->Base = BASE_TIM15;

        OSSetISRDescriptor(OS_IO_TIM1_BRK_TIM15,1,device);

        intSetEnable = (UINT32 *)0xE000E100 + (UINT32)(interruptIndex / 32);
        *intSetEnable |= 0x01 << (interruptIndex % 32); // Enable the IRQ channels
        intPriorityLevel = (UINT8 *)(0xE000E400 + interruptIndex);
        *intPriorityLevel = tmppriority << 0x04; // Set the IRQ priority

        break;
     #endif
     #ifdef OS_IO_TIM16
     case OS_IO_TIM16:
        device->ClkEnable = (UINT32 *)0x40021018; // RCC_APB2ENR
        device->ClkEnableBit = 0x20000;
        device->Base = BASE_TIM16;

        OSSetISRDescriptor(OS_IO_TIM1_UP_TIM16,1,device);

        intSetEnable = (UINT32 *)0xE000E100 + (UINT32)(interruptIndex / 32);
        *intSetEnable |= 0x01 << (interruptIndex % 32); // Enable the IRQ channels
        intPriorityLevel = (UINT8 *)(0xE000E400 + interruptIndex);
        *intPriorityLevel = tmppriority << 0x04; // Set the IRQ priority

        break;
     #endif
     #ifdef OS_IO_TIM17
     case OS_IO_TIM17:
        device->ClkEnable = (UINT32 *)0x40021018; // RCC_APB2ENR
        device->ClkEnableBit = 0x40000;
        device->Base = BASE_TIM17;

        OSSetISRDescriptor(OS_IO_TIM1_TRG_COM_TIM17,1,device);

        intSetEnable = (UINT32 *)0xE000E100 + (UINT32)(interruptIndex / 32);
        *intSetEnable |= 0x01 << (interruptIndex % 32); // Enable the IRQ channels
        intPriorityLevel = (UINT8 *)(0xE000E400 + interruptIndex);
        *intPriorityLevel = tmppriority << 0x04; // Set the IRQ priority

        break;
     #endif
     default:
     break;
  }

  device->TimerIntHandler = TimerIntHandler;
  *device->ClkEnable |= device->ClkEnableBit;

  #if defined(STM32L1XXXX)
     if (interruptIndex == OS_IO_TIM5) {
  #elif defined(STM32F2XXXX) || defined(STM32F4XXXX)
     if (interruptIndex == OS_IO_TIM2 || interruptIndex == OS_IO_TIM5) {
  #endif
  #if defined(STM32L1XXXX) ||  defined(STM32F2XXXX) || defined(STM32F4XXXX)
        *(UINT32 *)(device->Base + OFFSET_AUTORELOAD) = 0x3FFFFFFF; // Set the autoreload value (2^30 - 1)
     }
     else {
  #endif
        *(UINT16 *)(device->Base + OFFSET_AUTORELOAD) = 0xFFFF; // Set the autoreload value
  #if defined(STM32L1XXXX) ||  defined(STM32F2XXXX) || defined(STM32F4XXXX)
     }
  #endif


  *(UINT16 *)(device->Base + OFFSET_PRESCALER) = prescaler; // Set the prescaler value
  *(UINT16 *)(device->Base + OFFSET_EVENT_GENERATION) = 1; // Generate an update event to reload the prescaler
  *(UINT16 *)(device->Base + OFFSET_STATUS) = (UINT16)~3; // Clear update flag
  *(UINT16 *)(device->Base + OFFSET_INT_ENABLE) |= 3; // Enable update interrupt

  device->Time = 0;
  device->PendingQueueOperation = NULL;
  /* Initialize event queue */
  device->EventQueue = (TIMER_EVENT_NODE_HEAD *)OSMalloc(sizeof(TIMER_EVENT_NODE_HEAD));
  device->EventQueue->Next = NULL;
  /* Create a pool of free event nodes */
  device->FreeNodes = (TIMER_EVENT_NODE *)OSMalloc(nbNode * sizeof(TIMER_EVENT_NODE));
  for (i = 0; i < nbNode - 1; i += 1)
     device->FreeNodes[i].Next = &device->FreeNodes[i+1];
  device->FreeNodes[i].Next = NULL;

  *(UINT16 *)(device->Base + OFFSET_CONTROL1) |= 1; // Enable the TIM Counter
} /* end of OSInitTimerEvent */


/* OSScheduleTimerEvent: Entry point to insert an event into the event list associated
** with a timer. */
BOOL OSScheduleTimerEvent(void *event, UINT32 delay, UINT8 interruptIndex)
{
  TIMER_EVENT_NODE *timerEventNode;
  TIMER_ISR_DATA *device;
  INSERTQUEUE_OP des, *pendingOp;
  INT32 timerTime;
  device = (TIMER_ISR_DATA *)OSGetISRDescriptor(interruptIndex,GetInterruptSubIndex(interruptIndex));
  if ((timerEventNode = GetFreeNode(device)) == NULL)
     return FALSE;
  /* Because the timer ISR can shift the current time, the time at which this event oc-
  ** curs is done in 2 steps. The time is determined here and then transferred to the
  ** event at the last moment so that if there is a time shift, it can be detected and
  ** corrected. */
  #if defined(STM32L1XXXX)
     if (device->Base == BASE_TIM5) {
  #elif defined(STM32F2XXXX) || defined(STM32F4XXXX)
     if (device->Base == BASE_TIM2 || device->Base == BASE_TIM5) {
  #endif
  #if defined(STM32L1XXXX) ||  defined(STM32F2XXXX) || defined(STM32F4XXXX)
        do {
           des.Version = device->Version;
           des.Time = *(UINT32 *)(device->Base + OFFSET_COUNTER) + delay;
        } while (des.Version != device->Version);
     }
     else {
  #endif
        do {
           timerTime = device->Time;
           des.Time = *(UINT16 *)(device->Base + OFFSET_COUNTER) + delay + timerTime;
           des.Version = device->Version;
        } while (timerTime != device->Time);
  #if defined(STM32L1XXXX) ||  defined(STM32F2XXXX) || defined(STM32F4XXXX)
     }
  #endif
  timerEventNode->Time = -1;     // Set at an uninitialized sentinel value
  timerEventNode->Event = event;
  des.EventOp = InsertEventOp;   // Prepare the insertion so that other task may complete
  des.Done = FALSE;              // it.
  des.Left = (TIMER_EVENT_NODE *)device->EventQueue;
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
  device->PendingQueueOperation = &des;    // Post the insertion for other tasks
  InsertQueueHelper(&des,device,TRUE);     // Do the insertion
  device->PendingQueueOperation = NULL;
  return TRUE;
} /* end of OSScheduleTimerEvent */


/* GetInterruptSubIndex: . */
UINT8 GetInterruptSubIndex(UINT8 interruptIndex)
{
  switch (interruptIndex) { /* Specific timer device registers */
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
  if (des->Version == device->Version)
     scheduledTime = des->Time;
  else
     scheduledTime = des->Time - SHIFT_TIME_LIMIT;
  while (OSINT32_LL(&des->Node->Time) == -1)
     if (OSINT32_SC(&des->Node->Time,scheduledTime))
        break;
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
  if (genInterrupt)
     while (device->EventQueue->Next == des->Node) {
         OSUINT16_LL((UINT16 *)(device->Base + OFFSET_EVENT_GENERATION));
         if (des->GeneratedInterrupt || OSUINT16_SC((UINT16 *)(device->Base + OFFSET_EVENT_GENERATION),2))
            break;
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
  device = (TIMER_ISR_DATA *)OSGetISRDescriptor(interruptIndex,GetInterruptSubIndex(interruptIndex));
  des.Left = (TIMER_EVENT_NODE *)device->EventQueue;
  des.Event = event;
  des.Node = (TIMER_EVENT_NODE *)&des;
  pendingOp = device->PendingQueueOperation;
  if (pendingOp != NULL) {      // Is there an incomplete operation under way?
     if (pendingOp->EventOp == InsertEventOp)
        InsertQueueHelper((INSERTQUEUE_OP *)pendingOp,device,TRUE);
     else
        DeleteQueueHelper(pendingOp);
  }
  device->PendingQueueOperation = &des;    // Post the insertion for other tasks
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


/* TimerIntHandler: ISR routine called whenever the timer device gets an interrupt. */
void TimerIntHandler(TIMER_ISR_DATA *device)
{
  UINT16 time16;
  TIMER_EVENT_NODE *eventNode;
  INSERTQUEUE_OP *pendingOp;
  #if defined(STM32L1XXXX)
    if (device->Base == BASE_TIM5) {
  #elif defined(STM32F2XXXX) || defined(STM32F4XXXX)
     if (device->Base == BASE_TIM2 || device->Base == BASE_TIM5) {
  #endif
  #if defined(STM32L1XXXX) ||  defined(STM32F2XXXX) || defined(STM32F4XXXX)
        *(UINT32 *)(device->Base + OFFSET_COMPARATOR) = 0; // Disable timer comparator
     }
     else {
  #endif
        *(UINT16 *)(device->Base + OFFSET_COMPARATOR) = 0; // Disable timer comparator
  #if defined(STM32L1XXXX) ||  defined(STM32F2XXXX) || defined(STM32F4XXXX)
     }
  #endif
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
  /* Test interrupt source. For 32-bit timer counters, there are 2 interrupt events: an
  ** overflow event for which all temporal values must be shifted, and a comparator event.
  ** In addition to the above, 16-bit counters also have a timer increment event, which
  ** may cause shifting of the temporal values. */
  if (*(UINT16 *)(device->Base + OFFSET_STATUS) & 2)   // Is comparator interrupt pending?
     *(UINT16 *)(device->Base + OFFSET_STATUS) &= ~2;  // Clear interrupt flag
  if (*(UINT16 *)(device->Base + OFFSET_STATUS) & 1) { // Is timer overflow interrupt?
     *(UINT16 *)(device->Base + OFFSET_STATUS) &= ~1;  // Clear interrupt flag
     #if defined(STM32L1XXXX)
        if (device->Base == BASE_TIM5) {
     #elif defined(STM32F2XXXX) || defined(STM32F4XXXX)
        if (device->Base == BASE_TIM2 || device->Base == BASE_TIM5) {
     #endif
     #if defined(STM32L1XXXX) ||  defined(STM32F2XXXX) || defined(STM32F4XXXX)
           eventNode = device->EventQueue->Next;
           while (eventNode != NULL) {
              eventNode->Time -= SHIFT_TIME_LIMIT;
              eventNode = eventNode->Next;
           }
           device->Version += 1;
        }
        else {
     #endif
           device->Time += 0x10000;    // Increment most significant word of Time
           if (device->Time >= SHIFT_TIME_LIMIT) { // Shift all time value
              eventNode = device->EventQueue->Next;
              while (eventNode != NULL) {
                 eventNode->Time -= SHIFT_TIME_LIMIT;
                 eventNode = eventNode->Next;
              }
              device->Time -= SHIFT_TIME_LIMIT;
              device->Version += 1;
           }
     #if defined(STM32L1XXXX) ||  defined(STM32F2XXXX) || defined(STM32F4XXXX)
        }
     #endif
  }
  #if defined(STM32L1XXXX)
     if (device->Base == BASE_TIM5) {
  #elif defined(STM32F2XXXX) || defined(STM32F4XXXX)
     if (device->Base == BASE_TIM2 || device->Base == BASE_TIM5) {
  #endif
  #if defined(STM32L1XXXX) ||  defined(STM32F2XXXX) || defined(STM32F4XXXX)
        // Schedule events that now occur
        eventNode = device->EventQueue->Next;
        while (eventNode != NULL && eventNode->Time <= *(UINT32 *)(device->Base + OFFSET_COUNTER)) {
           OSScheduleSuspendedTask(eventNode->Event);
           device->EventQueue->Next = eventNode->Next;
           ReleaseNode(device,eventNode);
           eventNode = device->EventQueue->Next;
        }
        // Program timer comparator
        if (eventNode != NULL) {
           do {
              OSUINT32_LL((UINT32 *)(device->Base + OFFSET_COMPARATOR));
              if (eventNode->Time - TIMER_OFFSET < *(UINT32 *)(device->Base + OFFSET_COUNTER)) {
                 *(UINT16 *)(device->Base + OFFSET_EVENT_GENERATION) = 2;
                 break;
              }
           } while (!OSUINT32_SC((UINT32 *)(device->Base + OFFSET_COMPARATOR),eventNode->Time));
        }
     }
     else {
  #endif
        // Schedule events that now occur
        eventNode = device->EventQueue->Next;
        while (eventNode != NULL && eventNode->Time <= (device->Time | *(UINT16 *)(device->Base + OFFSET_COUNTER))) {
           OSScheduleSuspendedTask(eventNode->Event);
           device->EventQueue->Next = eventNode->Next;
           ReleaseNode(device,eventNode);
           eventNode = device->EventQueue->Next;
        }
        if (eventNode != NULL && (eventNode->Time & 0xFFFF0000) == device->Time) {
           time16 = (UINT16)eventNode->Time;
           do {
              OSUINT16_LL((UINT16 *)(device->Base + OFFSET_COMPARATOR));
              if (time16 - TIMER_OFFSET < *(UINT16 *)(device->Base + OFFSET_COUNTER)) {
                 *(UINT16 *)(device->Base + OFFSET_EVENT_GENERATION) |= 2;
                 break;
              }
           } while (!OSUINT16_SC((UINT16 *)(device->Base + OFFSET_COMPARATOR),time16));
        }
  #if defined(STM32L1XXXX) ||  defined(STM32F2XXXX) || defined(STM32F4XXXX)
     }
  #endif
} /* end of TimerIntHandler */
