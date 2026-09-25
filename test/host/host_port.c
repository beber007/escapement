/* Copyright (c) 2026 Bertrand Hurst. All rights reserved.
** Escapement - Lightweight Power-Aware Real-Time OS.
** Distributed under the terms of LICENSE at the root of this repository.
*/
/* File host_port.c: The layer the kernel expects from a target, implemented on the host.
**
** The clock is a variable the test advances, which is the whole point: it can jump from
** one event to the next, so the 2^30 wraparound of the kernel clock, eighteen minutes
** away on hardware, costs only the activations in between, and a task set of thirty is
** no harder to run than one of three.
**
** The atomics are plain accesses. This build is single threaded and nothing preempts it,
** so a load-linked keeps its reservation unless a test asks for store-conditionals to
** fail (HostFailingSC).
*/

#include <stdlib.h>
#include <string.h>
#include "Escapement.h"

unsigned HostContextSwitchesRequested = 0;
unsigned HostSoftTimerRequests = 0;
unsigned HostClockWraps = 0;

/* The kernel reads these to learn why the timer interrupted; on a target they are set by
** the interrupt handler of the peripheral. */
volatile BOOL _OSOverflowInterruptFlag = FALSE;
volatile BOOL _OSComparatorInterruptFlag = FALSE;

static INT32 Clock = 0;            /* the kernel clock, modulo 2^30 */
static INT32 ArmedDeadline = -1;   /* what _OSSetTimer last armed */
static BOOL  OverflowPending = FALSE;

#define TIME_LIMIT 0x40000000      /* 2^30, the wraparound of the kernel clock */


void _OSInitializeTimer(void) { }

void _OSStartTimer(void)
{
  Clock = 0;
  ArmedDeadline = -1;
  OverflowPending = FALSE;
}

INT32 _OSGetActualTime(void) { return Clock; }

BOOL _OSTimerIsOverflow(INT32 shiftTimeLimit)
{
  (void)shiftTimeLimit;
  if (OverflowPending) {
     OverflowPending = FALSE;
     return TRUE;
  }
  return FALSE;
}

BOOL _OSSetTimer(INT32 nextArrivalTime)
{
  ArmedDeadline = nextArrivalTime;
  return nextArrivalTime > Clock;
}

INT32 HostClockNow(void) { return Clock; }

/* HostTicksToNextEvent: Returns how far the clock must move to reach the next instant the
** hardware would interrupt: the deadline the kernel armed, or else the wraparound of the
** counter. An arrival beyond the wraparound is not armed until the time shift brings it
** within reach, so the overflow is then the only event left. */
INT32 HostTicksToNextEvent(void)
{
  if (ArmedDeadline > Clock)
     return ArmedDeadline - Clock;
  return TIME_LIMIT - Clock;
}

/* HostAdvanceBy: Moves the clock forward, wrapping at 2^30 like the counter does. */
void HostAdvanceBy(INT32 delta)
{
  Clock += delta;
  if (Clock >= TIME_LIMIT) {
     Clock -= TIME_LIMIT;
     OverflowPending = TRUE;
     ArmedDeadline = -1;   /* armed before the shift, so it no longer means anything */
     HostClockWraps += 1;
  }
}

/* Interrupt table, present only because the kernel refers to it. */
void *_OSTabDevice[OS_IO_NB_ENTRIES] = { 0 };
void OSSetISRDescriptor(UINT16 entry, void *descriptor) { _OSTabDevice[entry] = descriptor; }
void *OSGetISRDescriptor(UINT16 entry) { return _OSTabDevice[entry]; }
void _OSIOHandler(void) { }

/* Allocation: the kernel never frees, so neither does this. */
void *OSMalloc(UINT16 size) { return calloc(1, size); }

/* Atomics. Single threaded and never preempted here, so a reservation holds — unless a
** test sets HostFailingSC to make that many store-conditionals fail, as an interrupt
** between the LL and the SC does on the target. */
unsigned HostFailingSC = 0;
#define SC(a, v) do { if (HostFailingSC > 0) { HostFailingSC -= 1; return FALSE; } \
                      *(a) = (v); return TRUE; } while (0)
UINT8  OSUINT8_LL(UINT8 *a)   { return *a; }
BOOL   OSUINT8_SC(UINT8 *a, UINT8 v)   { SC(a, v); }
UINT16 OSUINT16_LL(UINT16 *a) { return *a; }
BOOL   OSUINT16_SC(UINT16 *a, UINT16 v) { SC(a, v); }
INT16  OSINT16_LL(INT16 *a)   { return *a; }
BOOL   OSINT16_SC(INT16 *a, INT16 v)   { SC(a, v); }
UINT32 OSUINT32_LL(UINT32 *a) { return *a; }
BOOL   OSUINT32_SC(UINT32 *a, UINT32 v) { SC(a, v); }
INT32  OSINT32_LL(INT32 *a)   { return *a; }
BOOL   OSINT32_SC(INT32 *a, INT32 v)   { SC(a, v); }
UINTPTR OSUINTPTR_LL(UINTPTR *a) { return *a; }
BOOL   OSUINTPTR_SC(UINTPTR *a, UINTPTR v) { SC(a, v); }


#ifdef ESCAPEMENT_VERSION_HARD_PA
/* Work done at each operating point relative to the fastest, times 256, as on the RP2040. */
const UINT8 _OSSlowdownRatios[] = {24, 102};

UINT8 HostSpeed = OS_MAX_SPEED;
unsigned HostSpeedChanges = 0;
unsigned HostSpeedsUsed = 0;         /* bit n set once speed n has been selected */
unsigned HostInvalidSpeeds = 0;

UINT8 OSGetProcessorSpeed(void) { return HostSpeed; }

void OSSetProcessorSpeed(UINT8 speed)
{
  if (speed > OS_MAX_SPEED) {
     HostInvalidSpeeds += 1;
     return;
  }
  if (speed != HostSpeed)
     HostSpeedChanges += 1;
  HostSpeed = speed;
  HostSpeedsUsed |= 1u << speed;
}
#endif
