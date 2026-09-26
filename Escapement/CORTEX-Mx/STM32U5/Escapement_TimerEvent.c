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
** Authors: MIS-TIC
**
** Escapement - Lightweight Power-Aware Real-Time OS, derived from ZottaOS.
** Modifications Copyright (c) 2026 Bertrand Hurst, distributed under the same terms;
** see LICENSE and NOTICE at the root of this repository.
*/
/* File Escapement_TimerEvent.c: Queue of events attached to TIM5 of the STM32U575, the
** logic of the RP2350 port with a timer of this chip in place of an alarm (RM0456,
** general-purpose timers TIM2 to TIM5).
** Each event is inserted with a delay, and the task waiting on it is woken once the delay
** has expired.
**
** TIM5 counts microseconds on 32 bits, free, and channel 1 compares: an event time is
** the counter's value when it occurs, compared by signed difference, which is exact while
** every pending delay stays below 2^31. A comparator fires when the counter equals it,
** never on a time already past: such an event is handed to the handler by generating the
** compare event of the channel (CC1G). The queue is guarded by masking interrupts for the
** length of a walk through at most nbNode nodes.
** Platform version: STM32U585 (Arduino UNO Q), any STM32U5.
*/

#include "Escapement.h"
#include "Escapement_TimerEvent.h"

#define TIM5_BASE            0x40000C00
#define TIM_CR1              *((volatile UINT32 *)(TIM5_BASE + 0x00))
#define TIM_DIER             *((volatile UINT32 *)(TIM5_BASE + 0x0C))
#define TIM_SR               *((volatile UINT32 *)(TIM5_BASE + 0x10))
#define TIM_EGR              *((volatile UINT32 *)(TIM5_BASE + 0x14))
#define TIM_CNT              *((volatile UINT32 *)(TIM5_BASE + 0x24))
#define TIM_PSC              *((volatile UINT32 *)(TIM5_BASE + 0x28))
#define TIM_ARR              *((volatile UINT32 *)(TIM5_BASE + 0x2C))
#define TIM_CCR1             *((volatile UINT32 *)(TIM5_BASE + 0x34))

#define TIM_CR1_CEN          (1u << 0)
#define UPDATE_BIT           (1u << 0)   /* UG */
#define CC1_BIT              (1u << 1)   /* CC1IF, CC1IE, CC1G */

#define RCC_APB1ENR1         *((volatile UINT32 *)(0x46020C00 + 0x9C))
#define RCC_APB1ENR1_TIM5EN  (1u << 3)

#define NVIC_ISER(irq)       ((volatile UINT32 *)0xE000E100)[(irq) >> 5]
#define NVIC_ICPR(irq)       ((volatile UINT32 *)0xE000E280)[(irq) >> 5]
#define NVIC_BIT(irq)        (1u << ((irq) & 0x1F))
#define NVIC_IPR             ((volatile UINT8 *)0xE000E400)


typedef struct TIMER_EVENT_NODE { // Blocks that are in the event queue
  struct TIMER_EVENT_NODE *Next;
  void *Event;
  UINT32 Time;                    // Value of the counter when the event occurs
} TIMER_EVENT_NODE;

typedef struct TIMER_ISR_DATA {
  void (*TimerIntHandler)(struct TIMER_ISR_DATA *);
  TIMER_EVENT_NODE *EventQueue;   // Pending events, sorted by their time of occurrence
  TIMER_EVENT_NODE *FreeNodes;    // Pool of free nodes
} TIMER_ISR_DATA;


static void TimerIntHandler(TIMER_ISR_DATA *device);
static void ArmComparator(TIMER_ISR_DATA *device);


/* EnterCritical and LeaveCritical: Mask interrupts, restoring whatever state the caller
** had, since the entry points may be called with interrupts already masked. */
static UINT32 EnterCritical(void)
{
  UINT32 primask;
  __asm volatile ("MRS %0, PRIMASK" : "=r" (primask) :: "memory");
  _OSDisableInterrupts();
  return primask;
} /* end of EnterCritical */

static void LeaveCritical(UINT32 primask)
{
  __asm volatile ("MSR PRIMASK, %0" :: "r" (primask) : "memory");
} /* end of LeaveCritical */


/* OSInitTimerEvent: Creates the descriptor of TIM5 used as an event manager, and starts
** its counter. */
void OSInitTimerEvent(UINT8 nbNode, UINT8 priority, UINT16 interruptIndex)
{
  TIMER_ISR_DATA *device;
  UINT8 i;
  #ifdef DEBUG_MODE
     if (interruptIndex != OS_IO_TIM5)
        while (TRUE);                  // only TIM5 is an event manager on this port
  #endif
  device = (TIMER_ISR_DATA *)OSMalloc(sizeof(TIMER_ISR_DATA));
  device->TimerIntHandler = TimerIntHandler;
  device->EventQueue = NULL;
  device->FreeNodes = (TIMER_EVENT_NODE *)OSMalloc(nbNode * sizeof(TIMER_EVENT_NODE));
  for (i = 0; i < nbNode - 1; i += 1)
     device->FreeNodes[i].Next = &device->FreeNodes[i+1];
  device->FreeNodes[i].Next = NULL;
  OSSetISRDescriptor(interruptIndex,device);
  RCC_APB1ENR1 |= RCC_APB1ENR1_TIM5EN;
  (void)RCC_APB1ENR1;
  TIM_CR1 = 0;
  TIM_ARR = 0xFFFFFFFF;
  TIM_PSC = OS_SYSTEM_CLOCK_HZ / 1000000u - 1u;
  TIM_EGR = UPDATE_BIT;                // load the prescaler
  TIM_SR = 0;
  TIM_DIER = CC1_BIT;
  TIM_CR1 = TIM_CR1_CEN;
  /* Priority in the 4 most significant bits of the byte of the interrupt. */
  NVIC_IPR[interruptIndex] = (UINT8)(priority << 4);
  NVIC_ICPR(interruptIndex) = NVIC_BIT(interruptIndex);
  NVIC_ISER(interruptIndex) = NVIC_BIT(interruptIndex);
} /* end of OSInitTimerEvent */


/* OSScheduleTimerEvent: Inserts an event, sorted by its time of occurrence, behind any
** event due at the same time. */
BOOL OSScheduleTimerEvent(void *event, UINT32 delay, UINT16 interruptIndex)
{
  TIMER_ISR_DATA *device = (TIMER_ISR_DATA *)OSGetISRDescriptor(interruptIndex);
  TIMER_EVENT_NODE *node, **link;
  UINT32 primask = EnterCritical();
  if ((node = device->FreeNodes) == NULL) {
     LeaveCritical(primask);
     return FALSE;
  }
  device->FreeNodes = node->Next;
  node->Event = event;
  node->Time = TIM_CNT + delay;
  for (link = &device->EventQueue;
       *link != NULL && (INT32)((*link)->Time - node->Time) <= 0; link = &(*link)->Next);
  node->Next = *link;
  *link = node;
  if (device->EventQueue == node)
     ArmComparator(device);
  LeaveCritical(primask);
  return TRUE;
} /* end of OSScheduleTimerEvent */


/* OSUnScheduleTimerEvent: Removes the first pending occurrence of an event. The comparator
** is left armed on a removed head: it then fires on an empty slot, and the handler arms
** the next event. */
BOOL OSUnScheduleTimerEvent(void *event, UINT16 interruptIndex)
{
  TIMER_ISR_DATA *device = (TIMER_ISR_DATA *)OSGetISRDescriptor(interruptIndex);
  TIMER_EVENT_NODE *node, **link;
  UINT32 primask = EnterCritical();
  for (link = &device->EventQueue; *link != NULL && (*link)->Event != event;
       link = &(*link)->Next);
  if ((node = *link) != NULL) {
     *link = node->Next;
     node->Next = device->FreeNodes;
     device->FreeNodes = node;
  }
  LeaveCritical(primask);
  return node != NULL;
} /* end of OSUnScheduleTimerEvent */


/* ArmComparator: Arms the comparator on the event at the head of the queue, interrupts
** masked; an event already due is handed to the handler by generating the compare
** event. */
static void ArmComparator(TIMER_ISR_DATA *device)
{
  TIMER_EVENT_NODE *head = device->EventQueue;
  if (head == NULL)
     return;
  TIM_CCR1 = head->Time;
  if ((INT32)(head->Time - TIM_CNT) <= 0)
     TIM_EGR = CC1_BIT;
} /* end of ArmComparator */


/* TimerIntHandler: Wakes the tasks of every event now due, then arms the comparator on the
** next one. */
static void TimerIntHandler(TIMER_ISR_DATA *device)
{
  TIMER_EVENT_NODE *node;
  void *event;
  UINT32 primask;
  TIM_SR = ~CC1_BIT;                   // acknowledge (rc_w0)
  while (TRUE) {
     primask = EnterCritical();
     node = device->EventQueue;
     if (node == NULL || (INT32)(node->Time - TIM_CNT) > 0) {
        ArmComparator(device);
        LeaveCritical(primask);
        return;
     }
     device->EventQueue = node->Next;
     event = node->Event;
     OSTrace(OS_TRACE_EVENT,5,(UINT16)(TIM_CNT - node->Time));
     node->Next = device->FreeNodes;
     device->FreeNodes = node;
     LeaveCritical(primask);
     OSScheduleSuspendedTask(event);
  }
} /* end of TimerIntHandler */
