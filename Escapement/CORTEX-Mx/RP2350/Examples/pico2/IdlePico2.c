/* Copyright (c) 2026 Bertrand Hurst. All rights reserved.
** Escapement - Lightweight Power-Aware Real-Time OS.
** Distributed under the terms of LICENSE at the root of this repository.
*/
/* File IdlePico2.c: The idle task of the RP2350 under one periodic task, what the PPK2 is
** to measure, as SleepU5 is on the STM32U5.
**
** A task every 100 ms does some 100 us of work and schedules a timer event 40 ms on, on
** alarm 2 of TIMER0, which wakes a second task. Between them the idle task sleeps: in a
** plain WFI, or built with make SLEEP_GATE=1 in SLEEP with PLL_SYS stopped
** (Escapement_SleepGate.c), from which each interrupt is taken once PLL_SYS is locked
** again. Each instance should start one period after the one before and each event come
** when it was due, within a few microseconds; the wake-up's cost shows in both.
**
** Results, in words: 0 marker, 1 instances measured, 2 largest gap between two starts off
** the period, in us, 3 events come, 4 largest gap between the time an event was due and
** the time it came, in us, early or late. In SLEEP the debugger reads zeros from the
** whole bus: hold core 1, not core 0, for each read, as tools/read_trace.py does on the
** RP2040.
** Platform version: RP2350 (Raspberry Pi Pico 2).
*/

#include "Escapement.h"
#include "Escapement_Timer.h"
#include "Escapement_TimerEvent.h"
#ifdef SLEEP_GATE
   #include "Escapement_SleepGate.h"
#endif

#define MARKER      0x49444C32u          /* "IDL2" */
#define PERIOD      100000               /* us */
#define WORK        4000                 /* loop turns, some 100 us at 150 MHz */
#define EVENT_DELAY 40000                /* us */
#define EVENT_TIMER_INDEX OS_IO_TIMER_2

#define VTOR        *((volatile UINT32 *)0xE000ED08)

volatile struct {
  UINT32 Marker, Instances, JitterMax, Events, EventOffMax;
} Results;

static INT32 EventDue;                   // the kernel's time the event is due at

static void WorkTask(void *argument);
static void EventTask(void *argument);


int main(void)
{
  void *event;
  extern void (* const CortexMxVectorTable[])(void);
  VTOR = (UINT32)CortexMxVectorTable;
  OSInitializeSystemClocks();
  Results.Marker = MARKER;
  OSInitTimerEvent(1,1,EVENT_TIMER_INDEX);
  #ifdef SLEEP_GATE
     OSInitSleepGate(0,0);
  #endif
  event = OSCreateEventDescriptor();
  #if defined(ESCAPEMENT_VERSION_SOFT)
     OSCreateSynchronousTask(EventTask,0,1000,0,event,NULL);
     OSCreateTask(WorkTask,0,0,PERIOD,PERIOD,1,1,0,event);
  #else
     OSCreateSynchronousTask(EventTask,1000,event,NULL);
     OSCreateTask(WorkTask,0,PERIOD,PERIOD,event);
  #endif
  return OSStartMultitasking(NULL,NULL);
} /* end of main */


/* WorkTask: The work, and the gap between two starts off the period, from the second
** instance on. */
static void WorkTask(void *argument)
{
  static BOOL started = FALSE;
  static INT32 last;
  INT32 now = _OSGetActualTime();
  UINT32 gap, off;
  volatile UINT32 i;
  if (started) {
     gap = (UINT32)(now - last) & 0x3FFFFFFF;                     // across the wrap
     off = gap > PERIOD ? gap - PERIOD : PERIOD - gap;
     if (off > Results.JitterMax)
        Results.JitterMax = off;
     Results.Instances += 1;
  }
  started = TRUE;
  last = now;
  for (i = 0; i < WORK; i += 1);
  EventDue = (_OSGetActualTime() + EVENT_DELAY) & 0x3FFFFFFF;
  OSScheduleTimerEvent(argument,EVENT_DELAY,EVENT_TIMER_INDEX);
  OSEndTask();
} /* end of WorkTask */


/* EventTask: The gap between the time the event was due and the time it came, early or
** late, on the kernel's 30 bits. */
static void EventTask(void *argument)
{
  INT32 off = (INT32)(((UINT32)_OSGetActualTime() - (UINT32)EventDue) << 2) >> 2;
  (void)argument;
  if (off < 0)
     off = -off;
  if ((UINT32)off > Results.EventOffMax)
     Results.EventOffMax = (UINT32)off;
  Results.Events += 1;
  OSSuspendSynchronousTask();
} /* end of EventTask */
