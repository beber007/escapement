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
/* File Escapement_TimerEvent.c: Queue of events attached to an alarm of the RP2040 timer.
** Each event is inserted with a delay, and the task waiting on it is woken once the delay
** has expired.
**
** Two things are simpler than on the STM32. The counter counts 64 bits of microseconds and
** never wraps in practice: event times are kept as its lower 32 bits and compared by their
** signed difference, which is exact while every pending delay stays below 2^31, so no time
** ever has to be shifted. And the queue is guarded by masking interrupts for the length of
** a walk through at most nbNode nodes, where the STM32 driver helps interrupted operations
** complete, with load-linked and store-conditional on the timer registers themselves.
**
** The kernel owns alarms 0 and 1; an event manager takes alarm 2 or 3. The four alarms
** share INTE and INTF, which each side only touches through the atomic set and clear
** aliases.
** Platform version: RP2040 (Raspberry Pi Pico).
*/

#include "Escapement.h"
#include "Escapement_TimerEvent.h"

#define TIMER_BASE          0x40054000
#define TIMER_ALARM(n)      *((volatile UINT32 *)(TIMER_BASE + 0x10 + 4 * (n)))
#define TIMER_TIMERAWL      *((volatile UINT32 *)(TIMER_BASE + 0x28))
#define TIMER_ARMED         *((volatile UINT32 *)(TIMER_BASE + 0x20))
#define TIMER_INTR          *((volatile UINT32 *)(TIMER_BASE + 0x34))
#define TIMER_INTE_SET      *((volatile UINT32 *)(TIMER_BASE + 0x2000 + 0x38))
#define TIMER_INTF_SET      *((volatile UINT32 *)(TIMER_BASE + 0x2000 + 0x3C))
#define TIMER_INTF_CLR      *((volatile UINT32 *)(TIMER_BASE + 0x3000 + 0x3C))

#define RESETS_RESET        *((volatile UINT32 *)(0x4000C000 + 0x00))
#define RESETS_RESET_DONE   *((volatile UINT32 *)(0x4000C000 + 0x08))
#define RESETS_TIMER_BIT    (1u << 21)

#define NVIC_ISER           *((volatile UINT32 *)0xE000E100)
#define NVIC_ICPR           *((volatile UINT32 *)0xE000E280)
#define NVIC_IPR            ((volatile UINT32 *)0xE000E400)


typedef struct TIMER_EVENT_NODE { // Blocks that are in the event queue
  struct TIMER_EVENT_NODE *Next;
  void *Event;
  UINT32 Time;                    // Lower 32 bits of the counter when the event occurs
} TIMER_EVENT_NODE;

typedef struct TIMER_ISR_DATA {
  void (*TimerIntHandler)(struct TIMER_ISR_DATA *);
  UINT32 AlarmBit;                // Bit of the alarm in INTR, INTE and INTF
  volatile UINT32 *Alarm;         // ALARMn register
  TIMER_EVENT_NODE *EventQueue;   // Pending events, sorted by their time of occurrence
  TIMER_EVENT_NODE *FreeNodes;    // Pool of free nodes
} TIMER_ISR_DATA;


static void TimerIntHandler(TIMER_ISR_DATA *device);
static void ArmAlarm(TIMER_ISR_DATA *device);


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


/* OSInitTimerEvent: Creates the descriptor of an alarm used as an event manager. */
void OSInitTimerEvent(UINT8 nbNode, UINT8 priority, UINT16 interruptIndex)
{
  TIMER_ISR_DATA *device;
  UINT8 i, shift;
  UINT32 word;
  device = (TIMER_ISR_DATA *)OSMalloc(sizeof(TIMER_ISR_DATA));
  device->TimerIntHandler = TimerIntHandler;
  device->AlarmBit = 1u << (interruptIndex - OS_IO_TIMER_0);
  device->Alarm = &TIMER_ALARM(interruptIndex - OS_IO_TIMER_0);
  device->EventQueue = NULL;
  device->FreeNodes = (TIMER_EVENT_NODE *)OSMalloc(nbNode * sizeof(TIMER_EVENT_NODE));
  for (i = 0; i < nbNode - 1; i += 1)
     device->FreeNodes[i].Next = &device->FreeNodes[i+1];
  device->FreeNodes[i].Next = NULL;
  OSSetISRDescriptor(interruptIndex,device);
  /* The kernel releases the timer from reset too, later; enabling the interrupt of the
  ** alarm needs it now. */
  RESETS_RESET &= ~RESETS_TIMER_BIT;
  while ((RESETS_RESET_DONE & RESETS_TIMER_BIT) == 0);
  /* Clear what a previous program may have left on the alarm, see _OSInitializeTimer. */
  TIMER_ARMED = device->AlarmBit;
  TIMER_INTR = device->AlarmBit;
  TIMER_INTF_CLR = device->AlarmBit;
  TIMER_INTE_SET = device->AlarmBit;
  /* Priority in the 2 most significant bits of the byte of the interrupt. */
  shift = (interruptIndex & 0x3) << 3;
  word = NVIC_IPR[interruptIndex >> 2] & ~(0xFFu << shift);
  NVIC_IPR[interruptIndex >> 2] = word | ((UINT32)((priority << 6) & 0xFF) << shift);
  NVIC_ICPR = 1u << interruptIndex;
  NVIC_ISER = 1u << interruptIndex;
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
  node->Time = TIMER_TIMERAWL + delay;
  for (link = &device->EventQueue;
       *link != NULL && (INT32)((*link)->Time - node->Time) <= 0; link = &(*link)->Next);
  node->Next = *link;
  *link = node;
  if (device->EventQueue == node)
     ArmAlarm(device);
  LeaveCritical(primask);
  return TRUE;
} /* end of OSScheduleTimerEvent */


/* OSUnScheduleTimerEvent: Removes the first pending occurrence of an event. The alarm is
** left armed on a removed head: it then fires on an empty slot, and the handler arms the
** next event. */
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


/* ArmAlarm: Arms the alarm on the event at the head of the queue, interrupts masked. An
** alarm fires when the counter equals it, never on a time already past: such an event is
** handed to the handler by forcing the interrupt. */
static void ArmAlarm(TIMER_ISR_DATA *device)
{
  TIMER_EVENT_NODE *head = device->EventQueue;
  if (head == NULL)
     return;
  *device->Alarm = head->Time;
  if ((INT32)(head->Time - TIMER_TIMERAWL) <= 0)
     TIMER_INTF_SET = device->AlarmBit;
} /* end of ArmAlarm */


/* TimerIntHandler: Wakes the tasks of every event now due, then arms the alarm on the
** next one. */
static void TimerIntHandler(TIMER_ISR_DATA *device)
{
  TIMER_EVENT_NODE *node;
  void *event;
  UINT32 primask;
  TIMER_INTF_CLR = device->AlarmBit;  // release a possibly forced interrupt
  TIMER_INTR = device->AlarmBit;      // acknowledge
  while (TRUE) {
     primask = EnterCritical();
     node = device->EventQueue;
     if (node == NULL || (INT32)(node->Time - TIMER_TIMERAWL) > 0) {
        ArmAlarm(device);
        LeaveCritical(primask);
        return;
     }
     device->EventQueue = node->Next;
     event = node->Event;
     OSTrace(OS_TRACE_EVENT,device->AlarmBit,(UINT16)(TIMER_TIMERAWL - node->Time));
     node->Next = device->FreeNodes;
     device->FreeNodes = node;
     LeaveCritical(primask);
     OSScheduleSuspendedTask(event);
  }
} /* end of TimerIntHandler */
