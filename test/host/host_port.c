/* Copyright (c) 2026 Bertrand Hurst. All rights reserved.
** Escapement - Lightweight Power-Aware Real-Time OS.
** Derived from prior work; see LICENSE and NOTICE at the root of this repository.
*/
/* File host_port.c: The layer the kernel expects from a target, implemented on the host.
**
** The clock is a variable the test advances, which is the whole point: the 2^30
** wraparound of the kernel clock is eighteen minutes away on hardware and one assignment
** away here, and a task set of thirty is no harder to run than one of three.
**
** The atomics are plain accesses. This build is single threaded and nothing preempts it,
** so a load-linked never loses its reservation.
*/

#include <stdlib.h>
#include <string.h>
#include "Escapement.h"

unsigned HostContextSwitchesRequested = 0;
unsigned HostSoftTimerRequests = 0;

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

/* HostAdvanceToNextDeadline: Moves the clock to the deadline the kernel armed, crossing
** the 2^30 boundary the way the hardware would — the counter wraps and the overflow is
** reported once. Returns the time reached, or -1 when the kernel armed nothing. */
INT32 HostAdvanceToNextDeadline(void)
{
  if (ArmedDeadline < 0)
     return -1;
  if (ArmedDeadline > Clock)
     Clock = ArmedDeadline;
  else {
     /* The deadline lies beyond the wraparound. */
     Clock = ArmedDeadline;
     OverflowPending = TRUE;
  }
  ArmedDeadline = -1;
  return Clock;
}

/* HostAdvanceBy: Moves the clock forward, wrapping at 2^30 like the counter does. */
void HostAdvanceBy(INT32 delta)
{
  Clock += delta;
  if (Clock >= TIME_LIMIT) {
     Clock -= TIME_LIMIT;
     OverflowPending = TRUE;
  }
}

/* Interrupt table, present only because the kernel refers to it. */
void *_OSTabDevice[OS_IO_NB_ENTRIES] = { 0 };
void OSSetISRDescriptor(UINT16 entry, void *descriptor) { _OSTabDevice[entry] = descriptor; }
void *OSGetISRDescriptor(UINT16 entry) { return _OSTabDevice[entry]; }
void _OSIOHandler(void) { }

/* Allocation: the kernel never frees, so neither does this. */
void *OSMalloc(UINT16 size) { return calloc(1, size); }

/* Atomics. Single threaded and never preempted here, so a reservation always holds. */
UINT8  OSUINT8_LL(UINT8 *a)   { return *a; }
BOOL   OSUINT8_SC(UINT8 *a, UINT8 v)   { *a = v; return TRUE; }
UINT16 OSUINT16_LL(UINT16 *a) { return *a; }
BOOL   OSUINT16_SC(UINT16 *a, UINT16 v) { *a = v; return TRUE; }
INT16  OSINT16_LL(INT16 *a)   { return *a; }
BOOL   OSINT16_SC(INT16 *a, INT16 v)   { *a = v; return TRUE; }
UINT32 OSUINT32_LL(UINT32 *a) { return *a; }
BOOL   OSUINT32_SC(UINT32 *a, UINT32 v) { *a = v; return TRUE; }
INT32  OSINT32_LL(INT32 *a)   { return *a; }
BOOL   OSINT32_SC(INT32 *a, INT32 v)   { *a = v; return TRUE; }
UINTPTR OSUINTPTR_LL(UINTPTR *a) { return *a; }
BOOL   OSUINTPTR_SC(UINTPTR *a, UINTPTR v) { *a = v; return TRUE; }
