/* Copyright (c) 2026 Bertrand Hurst. All rights reserved.
** Escapement - Lightweight Power-Aware Real-Time OS.
** Distributed under the terms of LICENSE at the root of this repository.
*/
/* File SuspendPico.c: The cycles an event-driven task takes to suspend itself, from the
** call of OSSuspendSynchronousTask to the task it preempted running again (issue #102),
** as SuspendPico2.c counts them on the Pico 2. Since the timer's handler suspends the
** task, this covers the soft timer interrupt, its round and the context switch; before,
** the task's own steps and the context switch.
**
** A periodic task of period 1 ms signals the event-driven task, whose deadline, 100 µs
** on, comes first: the soft timer interrupt that the signal raises gives it the
** processor at once. The event-driven task reads the counter, raises Pending, and
** suspends itself, no signal pending. The periodic task, back from its call, reads the
** counter, counts the difference while Pending is raised, and ends. The idle task does
** not sleep.
** The RP2040 has no cycle counter: SysTick's counter, on the processor's clock, stands
** for one, running with no interrupt, as in BenchAdmissionPico.c (the kernel only pends
** the exception by hand). It counts down, on 24 bits.
**
** Results holds the instances, then the sum, the least and the most of their cycles, the
** first ten left out, and the last one, as SuspendPico2's: tools/pico_suspend.py reads it
** over SWD.
** Platform version: RP2040.
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

#define VTOR              *((volatile UINT32 *)0xE000ED08)
#define SYST_CSR          *((volatile UINT32 *)0xE000E010)
#define SYST_RVR          *((volatile UINT32 *)0xE000E014)
#define SYST_CVR          *((volatile UINT32 *)0xE000E018)
#define CYCLES()          SYST_CVR
#define TIMER_DBGPAUSE    *((volatile UINT32 *)(0x40054000 + 0x2C))
#define WATCHDOG_CTRL     *((volatile UINT32 *)(0x40058000 + 0x00))
#define WATCHDOG_ENABLE   (1u << 30)
#define SKIPPED           10   /* the first instances, before the loop settles */


int main(void)
{
  extern void (* const CortexMxVectorTable[])(void);
  VTOR = (UINT32)CortexMxVectorTable;
  /* A firmware in flash may have armed the watchdog, which a reset of the processors
  ** leaves running. */
  WATCHDOG_CTRL &= ~WATCHDOG_ENABLE;
  SYST_RVR = 0xFFFFFF;
  SYST_CVR = 0;
  SYST_CSR = 0x5;        /* the processor's clock, enabled, no interrupt */
  OSInitializeSystemClocks();
  #if defined(ESCAPEMENT_VERSION_HARD_PA)
     OSInitProcessorSpeed();
  #endif
  Event = OSCreateEventDescriptor();
  #if defined(ESCAPEMENT_VERSION_SOFT)
     OSCreateTask(SignalTask,0,0,1000,1000,1,1,0,NULL);
     OSCreateSynchronousTask(SuspendTask,0,100,0,Event,NULL);
  #elif defined(ESCAPEMENT_VERSION_HARD_PA)
     /* Its speed changes as the policy decides: the hard kernel's figure is the one to
     ** compare. */
     OSCreateTask(SignalTask,50,0,1000,1000,NULL);
     OSCreateSynchronousTask(SuspendTask,20,100,52,Event,NULL);
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
  /* The loader leaves core 1 halted, which pauses the counter the kernel set to pause
  ** with it: let it run, as no core is halted from here on (BenchAdmissionPico.c). */
  TIMER_DBGPAUSE = 0;
  OSScheduleSuspendedTask(Event);
  cycles = (Since - CYCLES()) & 0xFFFFFF;
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
