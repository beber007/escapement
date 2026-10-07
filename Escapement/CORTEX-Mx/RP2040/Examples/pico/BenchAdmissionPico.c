/* Copyright (c) 2026 Bertrand Hurst. All rights reserved.
** Escapement - Lightweight Power-Aware Real-Time OS.
** Distributed under the terms of LICENSE at the root of this repository.
*/
/* File BenchAdmissionPico.c: Times the soft kernel's test of each optional instance on the
** RP2040 at 125 MHz, under EDF. Built with make KERNEL=SOFT bench-admission, which builds
** the kernel apart with ESCAPEMENT_MEASURE_ADMISSION_COST, the hook through which each
** test runs here (_OSMeasureAdmission).
**
** Eight (m,k)-firm tasks, the set tools/firm_admission.py's simulation found the most
** costly for the test by processor demand (DemandFits) among 1,500 sets of eight
** (2026-10-07): up to 169 counts of an interval in one decision, 44 in the mean. Each
** instance spins on the 1 us counter for a pseudo-random quarter of its WCET to all of
** it, so that the test is made at many instants. Built with COUNT, an event-driven task
** that is never signalled reserves 1/256 of the processor, which sends the kernel back to
** the count it made before 2026-10-07 (IsTaskSchedulable), on the same tasks.
**
** Each test is timed in cycles by SysTick, its counter running with no interrupt (the
** kernel only pends the exception by hand), interrupts masked around it: what is timed
** is the test, not what preempts it. Built with UNMASKED (BenchAdmissionUnmaskedPico),
** they are left as the kernel has them, which times the interrupts too but delays none.
** The cost of timing an empty test is measured first and taken off. Admission holds, as listed above its definition, the tests, those that
** admitted, the sum of their cycles, the most and the fewest, and a histogram by powers
** of two; AdmissionDone becomes 1 after ADMISSION_TESTS tests. tools/admission_cost.py
** loads the image, waits for it and prints them, without stopping a core.
** Platform version: RP2040.
*/

#include "Escapement.h"

#if !defined(ESCAPEMENT_VERSION_SOFT) || !defined(ESCAPEMENT_MEASURE_ADMISSION_COST) || \
    SCHEDULER_REAL_TIME_MODE == DEADLINE_MONOTONIC_SCHEDULING
   #error "build with make KERNEL=SOFT bench-admission, under EDF"
#endif

#define ADMISSION_TESTS 20000

#define TIMER_TIMERAWL  *((volatile UINT32 *)(0x40054000 + 0x28))
#define TIMER_DBGPAUSE  *((volatile UINT32 *)(0x40054000 + 0x2C))
#define SYST_CSR        *((volatile UINT32 *)0xE000E010)
#define SYST_RVR        *((volatile UINT32 *)0xE000E014)
#define SYST_CVR        *((volatile UINT32 *)0xE000E018)

/*  0: tests made          1: tests that admitted     2: cycles, summed
**  3: cycles, the most    4: cycles, the fewest      5: cycles of an empty test, taken off
**  6-21: tests of fewer than 2^(k+6) cycles, k from 0, the last of all the rest */
volatile UINT32 Admission[22];
volatile UINT32 AdmissionDone = 0;

typedef struct FirmTaskDef {
   UINT32 WCET;
   UINT32 Seed;
} FirmTaskDef;

/* The set, in microseconds, the kernel's ticks: (WCET, period, m, k), deadline the period. */
static const UINT32 Set[8][4] = {
   {128, 1107, 1, 1}, {4, 283, 4, 4}, {251, 2466, 1, 3}, {153, 1060, 1, 1},
   {70, 527, 4, 5}, {139, 1891, 1, 1}, {288, 2056, 3, 4}, {246, 1630, 3, 3}};
static FirmTaskDef Tasks[8];

static UINT32 TimeTest(BOOL test(void), BOOL *result)
{
  UINT32 primask, before, after;
  __asm volatile ("mrs %0, primask" : "=r" (primask));
  #ifndef UNMASKED
     __asm volatile ("cpsid i" ::: "memory");
  #endif
  before = SYST_CVR;
  *result = test();
  after = SYST_CVR;
  __asm volatile ("msr primask, %0" :: "r" (primask) : "memory");
  return (before - after) & 0xFFFFFF;   /* SysTick counts down, on 24 bits */
} /* end of TimeTest */

BOOL _OSMeasureAdmission(BOOL test(void))
{
  BOOL result;
  UINT32 cycles = TimeTest(test,&result), k;
  cycles = cycles > Admission[5] ? cycles - Admission[5] : 0;
  if (Admission[0] < ADMISSION_TESTS) {
     Admission[0] += 1;
     Admission[1] += result != FALSE;
     Admission[2] += cycles;
     if (cycles > Admission[3])
        Admission[3] = cycles;
     if (cycles < Admission[4])
        Admission[4] = cycles;
     for (k = 0; k < 15 && cycles >= (64u << k); k += 1);
     Admission[6 + k] += 1;
     if (Admission[0] == ADMISSION_TESTS)
        AdmissionDone = 1;
  }
  return result;
} /* end of _OSMeasureAdmission */

static BOOL EmptyTest(void)
{
  return FALSE;
} /* end of EmptyTest */

static void FirmTask(void *argument)
{
  FirmTaskDef *task = (FirmTaskDef *)argument;
  UINT32 start = TIMER_TIMERAWL, takes;
  /* The loader leaves core 1 halted, which pauses the counter the kernel set to pause
  ** with it: let it run, as no core is halted from here on. */
  TIMER_DBGPAUSE = 0;
  task->Seed = task->Seed * 1103515245u + 12345u;
  takes = task->WCET / 4 + (task->Seed >> 16) % (task->WCET - task->WCET / 4 + 1);
  while (TIMER_TIMERAWL - start < takes);
  OSEndTask();
} /* end of FirmTask */

#ifdef COUNT
static void EventTask(void *argument)
{
  (void)argument;
  OSSuspendSynchronousTask();
} /* end of EventTask */
#endif

#define VTOR *((volatile UINT32 *)0xE000ED08)


int main(void)
{
  extern void (* const CortexMxVectorTable[])(void);
  UINT32 i, cycles, least = 0xFFFFFF;
  BOOL result;
  VTOR = (UINT32)CortexMxVectorTable;
  OSInitializeSystemClocks();
  SYST_RVR = 0xFFFFFF;
  SYST_CVR = 0;
  SYST_CSR = 0x5;        /* the processor's clock, enabled, no interrupt */
  for (i = 0; i < 64; i += 1)
     if ((cycles = TimeTest(EmptyTest,&result)) < least)
        least = cycles;
  Admission[5] = least;
  Admission[4] = 0xFFFFFFFF;
  for (i = 0; i < 8; i += 1) {
     Tasks[i].WCET = Set[i][0];
     Tasks[i].Seed = i + 1;
     OSCreateTask(FirmTask, (INT32)Set[i][0], 0, (INT32)Set[i][1], (INT32)Set[i][1],
                  (UINT8)Set[i][2], (UINT8)Set[i][3], 0, &Tasks[i]);
  }
  #ifdef COUNT
     OSCreateSynchronousTask(EventTask, 1, 1000, 1, OSCreateEventDescriptor(), NULL);
  #endif
  return OSStartMultitasking(NULL,NULL);
} /* end of main */
