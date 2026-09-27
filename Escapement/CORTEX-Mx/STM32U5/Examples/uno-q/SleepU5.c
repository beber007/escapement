/* Copyright (c) 2026 Bertrand Hurst. All rights reserved.
** Escapement - Lightweight Power-Aware Real-Time OS.
** Distributed under the terms of LICENSE at the root of this repository.
*/
/* File SleepU5.c: The idle task in Stop 2 (Escapement_Stop2.c) under one periodic task,
** what the PPK2 is to measure.
**
** A task every 100 ms lights the green of LED3 for some 100 us of work, and reads TIM2, the
** kernel's microsecond, and LPTIM1: between two instances the idle task sleeps in Stop 2,
** TIM2 standing still, and moves TIM2 on by what LPTIM1 counted on waking. The sums of
** both over the run then agree within the two crystals, 15 ppm on the board (docs/
** stm32u5.md), plus what each sleep loses; and each instance starts one period after the
** one before, within a few microseconds, if the wake-up comes in time. Each instance
** also has a timer event of TIM5 wake a second task 40 ms on, which reads TIM2 in turn:
** the idle task sleeps in Stop 2 up to the event as up to the next instance, TIM5 moved
** on with TIM2, and the event comes when it was due, within a few microseconds.
**
** Built with make PHASES=n, the idle task alternates n seconds in Stop 2, D13 high, and n
** seconds in Sleep, D13 low (OSAllowStop2), the load the same: what the PPK2 compares,
** its digital input on D13 telling the phases apart in the one record.
**
** Every ten instances a line of text goes to Linux on LPUART1, which sends nothing back:
** "SLEEP" and, in hexadecimal, the words 1 to 10 of Results. The idle task stays in Sleep
** for the few milliseconds of its sending.
**
** Results, in words: 0 marker, 1 instances measured, 2 ticks of LPTIM1 summed, 3 us of
** TIM2 summed, 4 largest gap between two starts off the period, in us, 5 entries into
** Stop 2, 6 largest wake-up in ticks, 7 wake-ups past the next event, 8 1 if Stop 2
** could not be set up (no LSE), 9 timer events come, 10 largest gap between the time an
** event was due and the time it came, in us, early or late.
** Platform version: STM32U585 (Arduino UNO Q).
*/

#include "Escapement.h"
#include "Escapement_Timer.h"
#include "Escapement_LPTimer.h"
#include "Escapement_Stop2.h"
#include "Escapement_UART.h"
#include "Escapement_TimerEvent.h"
#include "BoardU5.h"

#define MARKER      0x534C5050u          /* "SLPP" */
#define PERIOD      100000               /* us */
#define WORK        4000                 /* loop turns, some 100 us at 160 MHz */
#define EVENT_DELAY 40000                /* us */
#define REPORT_SIZE 96

volatile struct {
  UINT32 Marker, Instances, Ticks, Micros, JitterMax, Entries, WakeMaxTicks, Late, NoLSE;
  UINT32 Events, EventOffMax;
} Results;

static INT32 EventDue;                   // TIM2's time the event is due at

static void SleepTask(void *argument);
static void EventTask(void *argument);
static void Report(void);


int main(void)
{
  void *event;
  OSInitializeSystemClocks();
  Results.Marker = MARKER;
  InitializeFlag(FLAG1_PIN);
  #ifdef SLEEP_PHASES
     InitializeFlag(FLAG3_PIN);
     SetPin(FLAG3_PIN);
  #endif
  OSInitUART(1,REPORT_SIZE,NULL,OS_IO_LPUART1);
  OSInitTimerEvent(1,1,OS_IO_TIM5);
  Results.NoLSE = !OSInitStop2();
  event = OSCreateEventDescriptor();
  #if defined(ESCAPEMENT_VERSION_SOFT)
     OSCreateSynchronousTask(EventTask,0,1000,0,event,NULL);
     OSCreateTask(SleepTask,50,0,PERIOD,PERIOD,1,1,0,event);
  #else
     OSCreateSynchronousTask(EventTask,1000,event,NULL);
     OSCreateTask(SleepTask,0,PERIOD,PERIOD,event);
  #endif
  return OSStartMultitasking(NULL,NULL);
} /* end of main */


/* SleepTask: The work, both counts summed from the second instance on, the gap between
** the starts, and a report every ten instances. */
static void SleepTask(void *argument)
{
  static BOOL started = FALSE;
  static UINT16 lastTicks;
  static INT32 lastMicros;
  UINT16 ticks = OSGetLPTimer();
  INT32 micros = _OSGetActualTime();
  UINT32 gap, off;
  volatile UINT32 i;
  OS_STOP2_COUNTS counts;
  SetPin(FLAG1_PIN);
  if (started) {
     gap = (UINT32)(micros - lastMicros) & 0x3FFFFFFF;                // across the wrap
     off = gap > PERIOD ? gap - PERIOD : PERIOD - gap;
     if (off > Results.JitterMax)
        Results.JitterMax = off;
     Results.Ticks += (UINT16)(ticks - lastTicks);
     Results.Micros += gap;
     Results.Instances += 1;
     if (Results.Instances % 10 == 0) {
        OSGetStop2Counts(&counts);
        Results.Entries = counts.Entries;
        Results.WakeMaxTicks = counts.WakeMaxTicks;
        Results.Late = counts.Late;
        Report();
     }
  }
  #ifdef SLEEP_PHASES
     /* The phase changes at a start: the next sleep is the first of the new one. */
     if (Results.Instances % (SLEEP_PHASES * 1000000 / PERIOD) == 0) {
        if ((Results.Instances / (SLEEP_PHASES * 1000000 / PERIOD)) % 2 == 0) {
           OSAllowStop2(TRUE);
           SetPin(FLAG3_PIN);
        }
        else {
           OSAllowStop2(FALSE);
           ClearPin(FLAG3_PIN);
        }
     }
  #endif
  started = TRUE;
  lastTicks = ticks;
  lastMicros = micros;
  for (i = 0; i < WORK; i += 1);
  EventDue = (_OSGetActualTime() + EVENT_DELAY) & 0x3FFFFFFF;
  OSScheduleTimerEvent(argument,EVENT_DELAY,OS_IO_TIM5);
  ClearPin(FLAG1_PIN);
  OSEndTask();
} /* end of SleepTask */


/* EventTask: The gap between the time the event was due and the time it came, early or
** late, on TIM2's 30 bits. */
static void EventTask(void *argument)
{
  INT32 off = (INT32)(((UINT32)_OSGetActualTime() - (UINT32)EventDue) << 2) >> 2;
  (void)argument;
  if (off < 0)
     off = -off;
  if ((UINT32)off > Results.EventOffMax)
     Results.EventOffMax = off;
  Results.Events += 1;
  OSSuspendSynchronousTask();
} /* end of EventTask */


/* PutHex: A number in hexadecimal, without leading zeros, and a space after it. */
static UINT8 *PutHex(UINT8 *p, UINT32 value)
{
  INT32 shift = 28;
  while (shift > 0 && (value >> shift) == 0)
     shift -= 4;
  for (; shift >= 0; shift -= 4)
     *p++ = "0123456789abcdef"[value >> shift & 0xF];
  *p++ = ' ';
  return p;
} /* end of PutHex */


/* Report: Words 1 to 10 of Results as a line of text to Linux; none if the line before is
** still being sent. */
static void Report(void)
{
  UINT8 *line = (UINT8 *)OSGetFreeNodeUART(OS_IO_LPUART1), *p;
  if (line == NULL)
     return;
  p = line;
  *p++ = 'S'; *p++ = 'L'; *p++ = 'E'; *p++ = 'E'; *p++ = 'P'; *p++ = ' ';
  p = PutHex(p,Results.Instances);
  p = PutHex(p,Results.Ticks);
  p = PutHex(p,Results.Micros);
  p = PutHex(p,Results.JitterMax);
  p = PutHex(p,Results.Entries);
  p = PutHex(p,Results.WakeMaxTicks);
  p = PutHex(p,Results.Late);
  p = PutHex(p,Results.NoLSE);
  p = PutHex(p,Results.Events);
  p = PutHex(p,Results.EventOffMax);
  p[-1] = '\n';
  OSEnqueueUART(line,(UINT8)(p - line),OS_IO_LPUART1);
} /* end of Report */
