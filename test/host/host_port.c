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

#include <stdio.h>
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


/* What the target needs of the kernel at start, which the host would otherwise let pass:
** the timer initialised before it is started, and started with interrupts masked, so
** that no interrupt runs before the time origin is set; the idle task sleeping. Left out,
** each passed every test, and the target would not run (a reading of the surviving
** mutants, 2026-10-06). */
unsigned HostTimerInitialised = 0, HostTimerStarted = 0, HostSlept = 0;
static void HostPortFails(const char *what)
{
  printf("FAILED: %s\n", what);
  exit(1);
}

void _OSInitializeTimer(void) { HostTimerInitialised = 1; }

void _OSStartTimer(void)
{
  if (!HostTimerInitialised)
     HostPortFails("the timer started before it was initialised");
  if (!HostMasked)
     HostPortFails("the timer started with interrupts unmasked");
  HostTimerStarted = 1;
  Clock = 0;
  ArmedDeadline = -1;
  OverflowPending = FALSE;
}

/* A test may set HostTimeReadHook to take an interrupt once the time is read, before the
** kernel uses it. */
void (*HostTimeReadHook)(void) = NULL;
INT32 _OSGetActualTime(void)
{
  INT32 time = Clock;
  if (HostTimeReadHook)
     HostTimeReadHook();
  return time;
}

/* HostSetClock: Sets the clock, for a test to start its tasks at any phase of the
** counter. */
void HostSetClock(INT32 time) { Clock = time; }

/* A test may set HostOverflowCheckHook to run once the kernel has found no overflow, in
** the window between that test and its reading of the time: the counter may wrap there. */
void (*HostOverflowCheckHook)(void) = NULL;

BOOL _OSTimerIsOverflow(INT32 shiftTimeLimit)
{
  (void)shiftTimeLimit;
  if (OverflowPending) {
     OverflowPending = FALSE;
     _OSOverflowInterruptFlag = FALSE;
     return TRUE;
  }
  if (HostOverflowCheckHook)
     HostOverflowCheckHook();
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
** counter, whichever comes first. A periodic arrival beyond the wraparound is not armed
** until the time shift brings it within reach; an event-driven task waiting for its
** previous deadline keeps one time, which may lie past it and is armed so: the clock
** stopped there skipped the interrupt of the wraparound (a reading of the surviving
** mutants, 2026-10-06). */
INT32 HostTicksToNextEvent(void)
{
  if (ArmedDeadline > Clock && ArmedDeadline < TIME_LIMIT)
     return ArmedDeadline - Clock;
  return TIME_LIMIT - Clock;
}

/* HostAdvanceBy: Moves the clock forward, wrapping at 2^30 like the counter does. */
void HostAdvanceBy(INT32 delta)
{
  Clock += delta;
  /* The comparator's flag rises at the deadline armed, as on the target, where a handler
  ** that never clears it loops forever. */
  if (ArmedDeadline >= 0 && ArmedDeadline < TIME_LIMIT && Clock >= ArmedDeadline)
     _OSComparatorInterruptFlag = TRUE;
  if (Clock >= TIME_LIMIT) {
     Clock -= TIME_LIMIT;
     OverflowPending = TRUE;
     _OSOverflowInterruptFlag = TRUE;   /* as the overflow interrupt of a target does */
     ArmedDeadline = -1;   /* armed before the shift, so it no longer means anything */
     HostClockWraps += 1;
  }
}

/* Interrupt table, present only because the kernel refers to it. */
void *_OSTabDevice[OS_IO_NB_ENTRIES] = { 0 };
void OSSetISRDescriptor(UINT16 entry, void *descriptor) { _OSTabDevice[entry] = descriptor; }
void *OSGetISRDescriptor(UINT16 entry) { return _OSTabDevice[entry]; }
void _OSIOHandler(void) { }

/* Allocation: the kernel never frees, so neither does this. The target's OSMalloc hands
** out SRAM as the boot or the previous image left it, and so does this: each block is
** filled with 0xA5, so that a field the kernel forgets to set does not read as 0. Zeroed,
** as calloc gave them until 2026-10-04, the blocks hid some thirty statements of the
** hard kernel that set a field: left out, each passed every test (tools/mutants.py).
** Filled with one byte, though, every word reads the same, so that two indices a kernel
** forgot to set come out equal, as an empty queue's are, and a size forgotten reads huge,
** so that no queue wraps in a test. HOST_FILL_COUNT gives each word a count of its own,
** from 2, neither 0 nor 1, the bytes past the last whole word 0xA5: small counts, though,
** leave the upper bytes of a word 0, which a narrow field forgotten then reads as if set.
** Neither fill sees all (a reading of the surviving mutants, 2026-10-06): test_ipc runs
** under both. A test may set a byte instead, or -1 for zeros. */
int HostMallocFill = 0xA5;
int HostMallocBudget = -1;       /* allocations left before OSMalloc fails, -1 for no limit */
void *OSMalloc(UINT16 size)
{
  void *block;
  if (HostMallocBudget == 0)
     return NULL;
  if (HostMallocBudget > 0)
     HostMallocBudget -= 1;
  block = calloc(1, size);
  if (block != NULL && HostMallocFill == HOST_FILL_COUNT) {
     static UINT32 count = 2;      /* small: a size or an index left so wraps early */
     UINT16 k;
     memset(block, 0xA5, size);
     for (k = 0; k + 4 <= size; k += 4, count += 1)
        memcpy((char *)block + k, &count, 4);
  }
  else if (block != NULL && HostMallocFill >= 0)
     memset(block, HostMallocFill, size);
  return block;
}

void (*HostBarrierHook)(void) = NULL;
void (*HostSoftTimerHook)(void) = NULL;

/* The kernels' CompilerBarrier, the build turns into a call of this (Makefile): it marks
** where the kernel orders stores that the timer handler reads, which is where a test may
** take that interrupt (HostCompilerBarrierHook). */
void (*HostCompilerBarrierHook)(void) = NULL;
void HostCompilerBarrier(void)
{
  __asm volatile ("" ::: "memory");
  if (HostCompilerBarrierHook)
     HostCompilerBarrierHook();
}
unsigned HostMasked = 0, HostSoftTimerHeld = 0;

/* HostUnmask: Interrupts unmasked; an interrupt held meanwhile is taken now, then the
** soft timer interrupt it or the code masked may have raised. */
void (*HostUnmaskHook)(void) = NULL;
void HostUnmask(void)
{
  HostMasked = 0;
  if (HostUnmaskHook)
     HostUnmaskHook();
  if (HostSoftTimerHeld && HostSoftTimerHook) {
     HostSoftTimerHeld = 0;
     HostSoftTimerHook();
  }
}

#ifndef HOST_LITMUS          /* the litmus bench takes the exclusives of AArch64 */
/* Atomics. Single threaded and never preempted here, so a reservation holds — unless a
** test sets HostFailingSC to make that many store-conditionals fail, as an interrupt
** between the LL and the SC does on the target. */
unsigned HostFailingSC = 0;
unsigned HostPassingSC = 0;      /* store-conditionals let through before those that fail */
/* An LL reserves, its SC consumes the reservation. A test may set HostLLHook to run
** code between an LL and its SC, as an interrupt does; returning from it loses the
** reservation, as the return from an interrupt does on the target, so the SC fails. A
** hook that finds interrupts masked (HostMasked) holds its interrupt until they are
** unmasked, where HostUnmaskHook takes it.
** The hook returns whether it ran anything. */
BOOL (*HostLLHook)(void) = NULL;
static BOOL Reserved = FALSE;
#define LL(a) do { Reserved = TRUE; value = *(a); \
                   if (HostLLHook && HostLLHook()) Reserved = FALSE; \
                   return value; } while (0)
#define SC(a, v) do { if (!Reserved) return FALSE; \
                      Reserved = FALSE; \
                      if (HostPassingSC > 0) HostPassingSC -= 1; \
                      else if (HostFailingSC > 0) { HostFailingSC -= 1; return FALSE; } \
                      *(a) = (v); return TRUE; } while (0)
/* HostLoseReservation: What any interrupt does to a reservation on the target. */
void HostLoseReservation(void) { Reserved = FALSE; }

UINT8  OSUINT8_LL(UINT8 *a)   { UINT8 value; LL(a); }
BOOL   OSUINT8_SC(UINT8 *a, UINT8 v)   { SC(a, v); }
UINT16 OSUINT16_LL(UINT16 *a) { UINT16 value; LL(a); }
BOOL   OSUINT16_SC(UINT16 *a, UINT16 v) { SC(a, v); }
INT16  OSINT16_LL(INT16 *a)   { INT16 value; LL(a); }
BOOL   OSINT16_SC(INT16 *a, INT16 v)   { SC(a, v); }
UINT32 OSUINT32_LL(UINT32 *a) { UINT32 value; LL(a); }
BOOL   OSUINT32_SC(UINT32 *a, UINT32 v) { SC(a, v); }
INT32  OSINT32_LL(INT32 *a)   { INT32 value; LL(a); }
BOOL   OSINT32_SC(INT32 *a, INT32 v)   { SC(a, v); }
UINTPTR OSUINTPTR_LL(UINTPTR *a) { UINTPTR value; LL(a); }
BOOL   OSUINTPTR_SC(UINTPTR *a, UINTPTR v) { SC(a, v); }
#endif


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
