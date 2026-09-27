/* Copyright (c) 2026 Bertrand Hurst. All rights reserved.
** Escapement - Lightweight Power-Aware Real-Time OS.
** Distributed under the terms of LICENSE at the root of this repository.
*/
/* File TestLPTimerU5.c: LPTIM1 on the 32.768 kHz crystal (Escapement_LPTimer.c) against
** TIM2, the kernel's microsecond, both on crystals of their own, the LSE and the HSE.
**
** A task every 250 ms reads both, and sums what each counted since the instance before:
** the ratio of the sums is 32768 ticks for 1,000,000 us, within what the two crystals
** allow. Each instance also sets the compare of LPTIM1 some 10 ms ahead, and finds at the
** next one that the flag has risen: what will wake the chip from Stop 2.
**
** Results, in words: 0 marker, 1 instances measured, 2 ticks of LPTIM1 summed, 3 us of
** TIM2 summed, 4 compares found not reached, 5 1 if LPTIM1 could not start (no LSE).
** Platform version: STM32U585 (Arduino UNO Q).
*/

#include "Escapement.h"
#include "Escapement_Timer.h"
#include "Escapement_LPTimer.h"

#define MARKER  0x4C505449u              /* "LPTI" */
#define PERIOD  250000                   /* us: 8192 ticks, well inside the 16 bits */

volatile struct {
  UINT32 Marker, Instances, Ticks, Micros, CompareMissed, NoLSE;
} Results;

static void MeasureTask(void *argument);


int main(void)
{
  OSInitializeSystemClocks();
  Results.Marker = MARKER;
  Results.NoLSE = !OSInitLPTimer();
  #if defined(ESCAPEMENT_VERSION_SOFT)
     OSCreateTask(MeasureTask,50,0,PERIOD,PERIOD,1,1,0,NULL);
  #else
     OSCreateTask(MeasureTask,0,PERIOD,PERIOD,NULL);
  #endif
  return OSStartMultitasking(NULL,NULL);
} /* end of main */


/* MeasureTask: Both counts, summed from the second instance on, and the compare. */
static void MeasureTask(void *argument)
{
  static BOOL started = FALSE;
  static UINT16 lastTicks;
  static INT32 lastMicros;
  UINT16 ticks = OSGetLPTimer();
  INT32 micros = _OSGetActualTime();
  (void)argument;
  if (started) {
     Results.Ticks += (UINT16)(ticks - lastTicks);
     Results.Micros += (UINT32)(micros - lastMicros) & 0x3FFFFFFF;   // across the wrap
     Results.Instances += 1;
     if (!OSLPTimerCompared())
        Results.CompareMissed += 1;
  }
  started = TRUE;
  lastTicks = ticks;
  lastMicros = micros;
  OSSetLPTimerCompare((UINT16)(ticks + OS_LPTIMER_HZ / 100));      // some 10 ms ahead
  OSEndTask();
} /* end of MeasureTask */
