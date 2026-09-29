/* Copyright (c) 2026 Bertrand Hurst. All rights reserved.
** Escapement - Lightweight Power-Aware Real-Time OS.
** Distributed under the terms of LICENSE at the root of this repository.
*/
/* File Stop2EventWrapU5.c: The idle task in Stop 2 while the next arrival is that of an
** event-driven task beyond the 2^30 wrap of the kernel clock, for escapement_u5.robot.
**
** An event-driven task signalled before its period is over waits for it to end in the
** arrival queue, its time kept whole: it may lie beyond the next wrap, which a periodic
** task's never does (ArrivalQueueInsertTestKey, EscapementHard.c). The kernel arms TIM2's
** compare with that time, above the counter's 2^30 range, where it never matches. The
** idle task took it for the next event and slept on past the wrap, then set TIM2 back to
** just before it: the kernel's clock lost the overshoot, up to a whole sleep of LPTIM1
** (Escapement_Stop2.c, 2026-09-29).
**
** Here one event-driven task, of period P, has a timer event signal it again 10 ms after
** each start, so that it always waits in the arrival queue for the rest of its period;
** a periodic task of some four wraps' period gives the first signal. At each start the
** task reads TIM2, the kernel's time, and TIM5, the timer events' (Escapement_TimerEvent.c):
** both stand still in Stop 2 and are moved on by what LPTIM1 counted meanwhile, TIM5 by
** all of it. They stop and start a few instructions apart, which leaves TIM5 some 46 us a
** period behind here, each period the same within a microsecond or two (2026-09-29): the
** period across the wrap must fall in with the others. LPTIM1 itself, 16 bits at the
** scaled rate below, wraps every 2 ms, too short to time a period.
**
** Built with TIME_SCALE 1000 and the idle task's margins scaled as SleepWrapU5's, for
** escapement_u5_wrap.repl, which clocks TIM2, TIM5 and LPTIM1 a thousand times faster: a
** tick is a nanosecond, and the wrap comes every 1.07 s.
**
** Results, in words: 0 marker, 1 instances measured, 2 largest gap between two starts
** off P, in ns, 3 and 4 smallest and largest of TIM5's ns less TIM2's over one period,
** signed, 5 entries into Stop 2, 6 wake-ups past the next event, 7 1 if Stop 2 could not
** be set up (no LSE).
** Platform version: STM32U585 (Arduino UNO Q).
*/

#include "Escapement.h"
#include "Escapement_Timer.h"
#include "Escapement_Stop2.h"
#include "Escapement_TimerEvent.h"

#define MARKER      0x53574556u          /* "SWEV" */
#ifndef TIME_SCALE
   #define TIME_SCALE 1000             /* the Makefile builds it so (SCALED) */
#endif
#define PERIOD      (300000 * TIME_SCALE)  /* ticks, the event-driven task's */
#define SIGNAL      (10000 * TIME_SCALE)   /* ticks after a start, the next signal */
#define TIM5_CNT    *((volatile UINT32 *)(0x40000C00 + 0x24))   /* 32 bits, free */

volatile struct {
  UINT32 Marker, Instances, GapOffMax;
  INT32 SkewMin, SkewMax;
  UINT32 Entries, Late, NoLSE;
} Results;

static void EventTask(void *argument);
static void StartTask(void *argument);


int main(void)
{
  void *event;
  OSInitializeSystemClocks();
  Results.Marker = MARKER;
  OSInitTimerEvent(1,1,OS_IO_TIM5);
  Results.NoLSE = !OSInitStop2();
  event = OSCreateEventDescriptor();
  #if defined(ESCAPEMENT_VERSION_SOFT)
     OSCreateSynchronousTask(EventTask,0,PERIOD,0,event,event);
     OSCreateTask(StartTask,50 * TIME_SCALE,4,0,PERIOD,1,1,0,event);
  #else
     OSCreateSynchronousTask(EventTask,PERIOD,event,event);
     OSCreateTask(StartTask,4,0,PERIOD,event);
  #endif
  return OSStartMultitasking(NULL,NULL);
} /* end of main */


/* StartTask: The first signal; its next instance comes after the run. */
static void StartTask(void *argument)
{
  OSScheduleSuspendedTask(argument);
  OSEndTask();
} /* end of StartTask */


/* EventTask: The gap since the last start, on TIM2's 30 bits and on TIM5, then the
** next signal, before the period is over. */
static void EventTask(void *argument)
{
  static BOOL started = FALSE;
  static UINT32 lastTicks;
  static INT32 lastTime;
  UINT32 ticks = TIM5_CNT;
  INT32 time = _OSGetActualTime();
  UINT32 gap, off;
  INT32 skew;
  OS_STOP2_COUNTS counts;
  if (started) {
     gap = (UINT32)(time - lastTime) & 0x3FFFFFFF;
     off = gap > PERIOD ? gap - PERIOD : PERIOD - gap;
     if (off > Results.GapOffMax)
        Results.GapOffMax = off;
     skew = (INT32)(ticks - lastTicks - gap);
     if (Results.Instances == 0 || skew < Results.SkewMin)
        Results.SkewMin = skew;
     if (Results.Instances == 0 || skew > Results.SkewMax)
        Results.SkewMax = skew;
     Results.Instances += 1;
     OSGetStop2Counts(&counts);
     Results.Entries = counts.Entries;
     Results.Late = counts.Late;
  }
  started = TRUE;
  lastTicks = ticks;
  lastTime = time;
  OSScheduleTimerEvent(argument,SIGNAL,OS_IO_TIM5);
  OSSuspendSynchronousTask();
} /* end of EventTask */
