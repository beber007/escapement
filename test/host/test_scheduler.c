/* Copyright (c) 2026 Bertrand Hurst. All rights reserved.
** Escapement - Lightweight Power-Aware Real-Time OS.
** Derived from prior work; see LICENSE and NOTICE at the root of this repository.
*/
/* File test_scheduler.c: Exercises the scheduler on the host.
**
** The kernel is compiled as it ships; only the target layer is simulated, so what runs
** here is the scheduling code itself. Time is a variable, which buys two things the
** board cannot give: task sets far larger than the three an example carries, and the
** 2^30 wraparound of the kernel clock, eighteen minutes away on hardware.
**
** Tasks do not run on a stack of their own — there is no context switch here. The test
** calls the elected task itself, which is enough to observe what the scheduler decided.
*/

#include <stdio.h>
#include <string.h>
#include "Escapement.h"

/* Mirror of the task control block of EscapementHard.c. Declared here rather than shared
** because the kernel keeps it private; the compiler lays it out the same way from the
** same declaration. */
typedef struct HostTCB {
  struct HostTCB *Next[2];
  UINT8 TaskState;
  INT32 NextArrivalTimeLow;
  void (*TaskCodePtr)(void *);
  void *Argument;
  INT32 PeriodLow;
  INT32 NextDeadline;
  INT32 Deadline;
  UINT16 PeriodHigh;
  UINT16 NextArrivalTimeHigh;
} HostTCB;

/* Only the first three fields of this view are trusted. Reading the deadline back gave
** values that cannot be deadlines — a pointer where an integer belongs — so either the
** layout or what _OSActiveTask points at is not what this mirror assumes. Checking that
** no deadline is ever missed therefore waits until that is understood; counting
** activations needs none of it. */

#define TASKTYPE_BLOCKING 0x08   /* set on the idle sentinel */

extern HostTCB *_OSActiveTask;
extern void _OSTimerInterruptHandler(void);
extern void HostAdvanceBy(INT32 delta);

#define MAX_TASKS 40
static unsigned Activations[MAX_TASKS];
static INT32    Periods[MAX_TASKS];
static unsigned NbTasks = 0;
static unsigned Failures = 0;

static void CountingTask(void *argument)
{
  Activations[(UINTPTR)argument] += 1;
  OSEndTask();
}

static void Check(const char *what, int ok)
{
  printf("%-58s %s\n", what, ok ? "ok" : "FAILED");
  if (!ok) Failures += 1;
}

/* RunFor: Advances the clock the way the hardware would and lets the kernel schedule,
** calling each elected task. Returns the number of scheduling rounds. */
static unsigned RunFor(INT32 duration)
{
  INT32 target = HostClockNow() + duration;
  unsigned rounds = 0;
  while (HostClockNow() < target) {
     HostAdvanceBy(1);
     _OSTimerInterruptHandler();
     rounds += 1;
     while (_OSActiveTask != NULL && !(_OSActiveTask->TaskState & TASKTYPE_BLOCKING)) {
        HostTCB *task = _OSActiveTask;
        task->TaskCodePtr(task->Argument);
        if (_OSActiveTask == task)     /* the task did not end: stop rather than spin */
           break;
     }
  }
  return rounds;
}

static void CreateTask(INT32 period)
{
  Periods[NbTasks] = period;
  OSCreateTask(CountingTask, 0, period, period, (void *)(UINTPTR)NbTasks);
  NbTasks += 1;
}

int main(void)
{
  unsigned i;
  INT32 duration = 200000;

  /* A set of ten periodic tasks, co-prime enough that their arrivals interleave. */
  CreateTask(100);  CreateTask(150);  CreateTask(200);  CreateTask(250);
  CreateTask(300);  CreateTask(400);  CreateTask(600);  CreateTask(750);
  CreateTask(1000); CreateTask(1200);

  OSStartMultitasking(NULL, NULL);
  _OSStartTimer();

  RunFor(duration);

  printf("\n%u tasks, %d ticks of simulated time\n\n", NbTasks, duration);
  for (i = 0; i < NbTasks; i += 1) {
     unsigned expected = (unsigned)(duration / Periods[i]);
     char label[80];
     /* One activation either way is the boundary of the window, not a missed deadline. */
     snprintf(label, sizeof label, "  period %5d: %u activations (expected %u)",
              Periods[i], Activations[i], expected);
     Check(label, Activations[i] + 1 >= expected && Activations[i] <= expected + 1);
  }

  printf("\n%s\n", Failures ? "FAILURES" : "all checks passed");
  return Failures ? 1 : 0;
}
