/* Copyright (c) 2026 Bertrand Hurst. All rights reserved.
** Escapement - Lightweight Power-Aware Real-Time OS.
** Distributed under the terms of LICENSE at the root of this repository.
*/
/* File SuspendPico2.c: The cycles an event-driven task takes to suspend itself, from the
** call of OSSuspendSynchronousTask to the task it preempted running again (issue #102).
** Since the timer's handler suspends the task, this covers the soft timer interrupt,
** its round and the context switch; before, the task's own steps and the context
** switch.
**
** A periodic task of period 1 ms signals the event-driven task, whose deadline, 100 µs
** on, comes first: the soft timer interrupt that the signal raises gives it the
** processor at once. The event-driven task reads the cycle counter, raises Pending, and
** suspends itself, no signal pending. The periodic task, back from its call, reads the
** counter, counts the difference while Pending is raised, and ends. The idle task does
** not sleep.
** The cycle counter is the DWT's CYCCNT on the Cortex-M33, which main starts, and mcycle
** on the Hazard3, which the RISC-V layer starts.
**
** Results holds the instances, then the sum, the least and the most of their cycles, the
** first ten left out, and the last one, as LatencyPico2's: tools/pico2_latency.py reads
** it over SWD.
** Platform version: RP2350 (Raspberry Pi Pico 2).
*/

#include "Escapement.h"

volatile struct {
  UINT32 Count, Sum, Min, Max, Last;
} Results = {0, 0, 0xFFFFFFFF, 0, 0};

static volatile UINT32 Since;
static volatile BOOL Pending = FALSE;
static void *Event;

static void Watch(void);
static void SignalTask(void *argument);
static void SuspendTask(void *argument);

#ifdef __riscv
   #define CYCLES() __extension__ ({ UINT32 c; \
              __asm volatile ("csrr %0, mcycle" : "=r"(c) :: "memory"); c; })
#else
   #define VTOR              *((volatile UINT32 *)0xE000ED08)
   #define DEMCR             *((volatile UINT32 *)0xE000EDFC)
   #define DEMCR_TRCENA      (1u << 24)
   #define DWT_CTRL          *((volatile UINT32 *)0xE0001000)
   #define DWT_CTRL_CYCCNTENA (1u << 0)
   #define DWT_CYCCNT        *((volatile UINT32 *)0xE0001004)
   #define CYCLES()          DWT_CYCCNT
#endif
#define WATCHDOG_CTRL_CLR *((volatile UINT32 *)(0x400D8000 + 0x3000))
#define WATCHDOG_ENABLE   (1u << 30)
#define SKIPPED           10   /* the first instances, before the loop settles */


int main(void)
{
  #ifndef __riscv
     extern void (* const CortexMxVectorTable[])(void);
     VTOR = (UINT32)CortexMxVectorTable;
     DEMCR |= DEMCR_TRCENA;
     DWT_CTRL |= DWT_CTRL_CYCCNTENA;
  #endif
  /* A firmware in flash may have armed the watchdog, which a reset of the processors
  ** leaves running. */
  WATCHDOG_CTRL_CLR = WATCHDOG_ENABLE;
  OSInitializeSystemClocks();
  Event = OSCreateEventDescriptor();
  #if defined(ESCAPEMENT_VERSION_SOFT)
     OSCreateTask(SignalTask,0,0,1000,1000,1,1,0,NULL);
     OSCreateSynchronousTask(SuspendTask,0,100,0,Event,NULL);
  #else
     OSCreateTask(SignalTask,0,1000,1000,NULL);
     OSCreateSynchronousTask(SuspendTask,100,Event,NULL);
  #endif
  _OSIdleHook = Watch;
  return OSStartMultitasking(NULL,NULL);
} /* end of main */


/* Watch: The idle task's loop, in place of its sleep. */
static void Watch(void)
{
  while (TRUE);
} /* end of Watch */


/* SignalTask: Wakes the event-driven task once a period, which preempts it, then counts
** the cycles from its suspension. */
static void SignalTask(void *argument)
{
  UINT32 cycles;
  (void)argument;
  OSScheduleSuspendedTask(Event);
  cycles = CYCLES() - Since;
  if (Pending) {
     Pending = FALSE;
     Results.Last = cycles;
     Results.Count += 1;
     if (Results.Count > SKIPPED) {
        Results.Sum += cycles;
        if (cycles < Results.Min)
           Results.Min = cycles;
        if (cycles > Results.Max)
           Results.Max = cycles;
     }
  }
  OSEndTask();
} /* end of SignalTask */


/* SuspendTask: Stamps the counter, then suspends itself. */
static void SuspendTask(void *argument)
{
  (void)argument;
  Since = CYCLES();
  Pending = TRUE;
  OSSuspendSynchronousTask();
} /* end of SuspendTask */
