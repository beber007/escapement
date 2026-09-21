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
** Three runs, one per process since the kernel keeps its state in statics:
**   test_scheduler         ten tasks, the clock advanced one tick at a time
**   test_scheduler wrap    long periods, the clock jumped from one event to the next
**                          across three wraparounds
**   test_scheduler events  event-driven tasks woken by periodic tasks, by themselves
**                          and by a buffer slot filling up
**
** Tasks do not run on a stack of their own — there is no context switch here. The test
** calls the elected task itself, which is enough to observe what the scheduler decided.
** A soft timer interrupt requested by a task is served as soon as that task returns.
*/

#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
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

/* This view holds only under earliest-deadline-first scheduling. Under deadline-monotonic
** the kernel inserts a priority byte after the state and drops the two deadline fields,
** which moves everything after them — reading a deadline then returns a neighbouring
** pointer. That is exactly how this test found that the kernel was not scheduling the way
** the repository said it was. */
#if SCHEDULER_REAL_TIME_MODE != EARLIEST_DEADLINE_FIRST
   #error this mirror of the task control block assumes earliest-deadline-first
#endif

#define TASKTYPE_BLOCKING 0x08   /* set on event-driven tasks and on the idle sentinel */

extern HostTCB *_OSActiveTask;
extern void _OSTimerInterruptHandler(void);
extern void HostAdvanceBy(INT32 delta);
extern INT32 HostTicksToNextEvent(void);
extern unsigned HostClockWraps;
extern unsigned HostSoftTimerRequests;

#define MAX_TASKS 40
static unsigned Activations[MAX_TASKS];
static INT32    Periods[MAX_TASKS];
static unsigned NbTasks = 0;
static unsigned Failures = 0;
static unsigned LateArrivals = 0;
static unsigned DeadlinesOutOfReach = 0;
static unsigned SoftTimerServed = 0;

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

/* StartKernel: Starts the kernel the way a target does. OSStartMultitasking() elects the
** idle task, whose first run starts the timer and lets later signals raise the soft timer
** interrupt instead of waiting for the next tick. */
static void StartKernel(void (*f)(void *), void *arg)
{
  OSStartMultitasking(f, arg);
  _OSActiveTask->TaskCodePtr(NULL);
}

/* ServeSoftTimer: Runs the timer handler once for every soft interrupt requested. */
static void ServeSoftTimer(void)
{
  while (SoftTimerServed != HostSoftTimerRequests) {
     SoftTimerServed += 1;
     _OSTimerInterruptHandler();
  }
}

/* RunElected: Calls each task the scheduler elects until only the idle task is left. The
** idle task is the tail sentinel of the ready queue, the one entry without a successor. */
static void RunElected(void)
{
  while (TRUE) {
     HostTCB *task;
     ServeSoftTimer();
     task = _OSActiveTask;
     if (task == NULL || task->Next[0] == NULL)
        break;
     if (task->NextDeadline < HostClockNow())
        LateArrivals += 1;
     /* An absolute deadline further than one relative deadline away was not shifted. An
     ** event-driven task has no relative deadline in that field. */
     if (!(task->TaskState & TASKTYPE_BLOCKING) &&
         task->NextDeadline - HostClockNow() > task->Deadline)
        DeadlinesOutOfReach += 1;
     task->TaskCodePtr(task->Argument);
     if (_OSActiveTask == task)     /* the task did not end: stop rather than spin */
        break;
  }
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
     RunElected();
  }
  return rounds;
}

/* RunAcross: Same, but the clock jumps straight to the next timer event, so the duration
** may span wraparounds. Time elapsed is counted apart, the kernel clock being modulo 2^30.
** Two things the hardware does are imitated around each wraparound, since the time shift
** has work to do only then: an arrival due in the last LATENCY ticks is served after the
** counter wrapped, as interrupt latency can have it, and tasks elected just before are
** called only after, as if still running, so their deadlines sit in the ready queue. */
#define LATENCY 100
static INT32 TicksToNextInterrupt(void)
{
  INT32 delta = HostTicksToNextEvent();
  if (HostClockNow() + delta > 0x40000000 - LATENCY)
     delta = 0x40000000 - HostClockNow();
  return delta;
}

static void RunAcross(long long duration)
{
  long long elapsed = 0;
  _OSTimerInterruptHandler();        /* the arrivals at time zero */
  RunElected();
  while (TRUE) {
     INT32 delta = TicksToNextInterrupt();
     if (elapsed + delta > duration)
        break;
     HostAdvanceBy(delta);
     elapsed += delta;
     _OSTimerInterruptHandler();
     if (HostClockNow() + TicksToNextInterrupt() != 0x40000000)
        RunElected();
  }
}

static void CreateTask(INT32 period)
{
  Periods[NbTasks] = period;
  OSCreateTask(CountingTask, 0, period, period, (void *)(UINTPTR)NbTasks);
  NbTasks += 1;
}

/* CheckActivations: One activation either way is the boundary of the window, not a
** missed deadline. */
static void CheckActivations(long long duration)
{
  unsigned i;
  for (i = 0; i < NbTasks; i += 1) {
     unsigned expected = (unsigned)(duration / Periods[i]);
     char label[80];
     snprintf(label, sizeof label, "  period %9d: %u activations (expected %u)",
              Periods[i], Activations[i], expected);
     Check(label, Activations[i] + 1 >= expected && Activations[i] <= expected + 1);
  }
}

static void TestTaskSet(void)
{
  INT32 duration = 200000;

  /* A set of ten periodic tasks, co-prime enough that their arrivals interleave. */
  CreateTask(100);  CreateTask(150);  CreateTask(200);  CreateTask(250);
  CreateTask(300);  CreateTask(400);  CreateTask(600);  CreateTask(750);
  CreateTask(1000); CreateTask(1200);

  StartKernel(NULL, NULL);

  RunFor(duration);

  printf("\n%u tasks, %d ticks of simulated time\n\n", NbTasks, duration);
  CheckActivations(duration);
  Check("  no deadline missed", LateArrivals == 0);
}

/* TestWrap: The kernel shifts every temporal variable back by 2^30 when its counter wraps,
** and an arrival beyond the wraparound waits in the arrival queue with a cycle count
** instead of being armed. The periods are prime to one another, so arrivals fall on both
** sides of each boundary; 2^29 - 1 lands one arrival a few ticks short of every
** wraparound, which the latency above then serves after it. */
static void TestWrap(void)
{
  long long duration = 3LL * 0x40000000 + 1000000;

  CreateTask(1000003);   CreateTask(1999993);   CreateTask(4999999);
  CreateTask(99999989);  CreateTask(536870911); CreateTask(700000001);

  StartKernel(NULL, NULL);

  RunAcross(duration);

  printf("\n%u tasks, %lld ticks of simulated time\n\n", NbTasks, duration);
  CheckActivations(duration);
  Check("  the kernel clock wrapped three times", HostClockWraps == 3);
  Check("  no deadline missed", LateArrivals == 0);
  Check("  every deadline shifted with the clock", DeadlinesOutOfReach == 0);
}

/* EVENT-DRIVEN TASKS ------------------------------------------------------------------ */
/* Four event-driven tasks, each woken a different way, share the processor with three
** periodic tasks. The workloads keep the event-driven load at about 60 %, since under EDF
** their deadlines follow one another (GetSuspendedSchedulingDeadline). */

typedef struct Signaler {
  void *Event;
  unsigned Runs;
} Signaler;

#define SELF_SIGNALS 5
#define SLOT_SIZE    4

static Signaler ToSignaled, ToShared;
static void *BufferPort;
static unsigned SignaledRuns, SelfRuns, SharedRuns[2], SharedOrderBreaks;
static unsigned WriterRuns, ReaderRuns, ReaderMismatches;
static int LastShared = -1;

/* SignalerTask: A periodic task that signals an event at every instance. */
static void SignalerTask(void *argument)
{
  Signaler *signaler = (Signaler *)argument;
  signaler->Runs += 1;
  OSScheduleSuspendedTask(signaler->Event);
  OSEndTask();
}

/* SignaledTask: Woken once per signal. */
static void SignaledTask(void *argument)
{
  (void)argument;
  SignaledRuns += 1;
  OSSuspendSynchronousTask();
}

/* SelfTask: Woken first by a signal given before the timer starts, then signals itself
** while it runs, which the kernel must remember until it suspends. */
static void SelfTask(void *event)
{
  SelfRuns += 1;
  if (SelfRuns <= SELF_SIGNALS)
     OSScheduleSuspendedTask(event);
  OSSuspendSynchronousTask();
}

/* SharedTask: Two instances wait on the same event; each signal wakes the one that has
** waited longest, so they must take turns. */
static void SharedTask(void *argument)
{
  int who = (int)(UINTPTR)argument;
  SharedRuns[who] += 1;
  if (who == LastShared)
     SharedOrderBreaks += 1;
  LastShared = who;
  OSSuspendSynchronousTask();
}

/* WriterTask: Writes one byte per instance; every SLOT_SIZE bytes a slot fills up. */
static void WriterTask(void *argument)
{
  UINT8 byte = (UINT8)WriterRuns;
  (void)argument;
  WriterRuns += 1;
  if (OSWriteBuffer(BufferPort, &byte, 1) != 1)
     ReaderMismatches += 1;
  OSEndTask();
}

/* ReaderTask: Woken by the buffer each time a slot fills up. It must find SLOT_SIZE
** consecutive bytes starting on a slot boundary, and nothing more to read once it has
** taken them with OS_READ_ONLY_ONCE. */
static void ReaderTask(void *argument)
{
  UINT8 data[SLOT_SIZE], i;
  (void)argument;
  ReaderRuns += 1;
  if (OSGetCopyBuffer(BufferPort, OS_READ_ONLY_ONCE, data) != SLOT_SIZE || data[0] % SLOT_SIZE)
     ReaderMismatches += 1;
  for (i = 1; i < SLOT_SIZE; i += 1)
     if (data[i] != (UINT8)(data[0] + i))
        ReaderMismatches += 1;
  if (OSGetCopyBuffer(BufferPort, OS_READ_ONLY_ONCE, data) != 0)
     ReaderMismatches += 1;
  OSSuspendSynchronousTask();
}

/* StartEvents: Called by OSStartMultitasking() before the timer starts. */
static void StartEvents(void *event)
{
  OSScheduleSuspendedTask(event);
}

/* Within one of the count expected: the run stops between a signal and its handling. */
static int WithinOne(unsigned count, unsigned expected)
{
  return count + 1 >= expected && count <= expected + 1;
}

static void TestEvents(void)
{
  INT32 duration = 30000;
  void *selfEvent = OSCreateEventDescriptor(), *sharedEvent = OSCreateEventDescriptor();
  void *bufferEvent = OSCreateEventDescriptor();
  char label[80];

  ToSignaled.Event = OSCreateEventDescriptor();
  OSCreateTask(SignalerTask, 0, 100, 100, &ToSignaled);
  OSCreateSynchronousTask(SignaledTask, 20, ToSignaled.Event, NULL);

  OSCreateSynchronousTask(SelfTask, 20, selfEvent, selfEvent);

  ToShared.Event = sharedEvent;
  OSCreateTask(SignalerTask, 0, 300, 300, &ToShared);
  OSCreateSynchronousTask(SharedTask, 60, sharedEvent, (void *)0);
  OSCreateSynchronousTask(SharedTask, 60, sharedEvent, (void *)1);

  BufferPort = OSInitBuffer(SLOT_SIZE, OS_BUFFER_TYPE_3_SLOT, bufferEvent);
  OSCreateTask(WriterTask, 0, 50, 50, NULL);
  OSCreateSynchronousTask(ReaderTask, 40, bufferEvent, NULL);

  StartKernel(StartEvents, selfEvent);
  RunFor(duration);

  printf("\n%d ticks of simulated time, event-driven tasks\n\n", duration);
  snprintf(label, sizeof label, "  one wake-up per signal: %u for %u", SignaledRuns, ToSignaled.Runs);
  Check(label, SignaledRuns <= ToSignaled.Runs && SignaledRuns + 1 >= ToSignaled.Runs);
  snprintf(label, sizeof label, "  signal before the timer, then to itself: %u runs", SelfRuns);
  Check(label, SelfRuns == SELF_SIGNALS + 1);
  snprintf(label, sizeof label, "  two waiters take turns: %u + %u for %u",
           SharedRuns[0], SharedRuns[1], ToShared.Runs);
  Check(label, SharedOrderBreaks == 0 && WithinOne(SharedRuns[0], SharedRuns[1]) &&
               SharedRuns[0] + SharedRuns[1] <= ToShared.Runs &&
               SharedRuns[0] + SharedRuns[1] + 1 >= ToShared.Runs);
  snprintf(label, sizeof label, "  a full slot wakes its reader: %u for %u bytes",
           ReaderRuns, WriterRuns);
  Check(label, ReaderMismatches == 0 && WithinOne(ReaderRuns, WriterRuns / SLOT_SIZE));
  Check("  no deadline missed", LateArrivals == 0);
}


/* A kernel that mishandles time can loop forever inside its interrupt handler, where the
** test has no say; both runs take well under a second. */
static void Timeout(int signal)
{
  static const char message[] = "\nFAILED: the scheduler did not return within 10 s\n";
  (void)signal;
  write(STDOUT_FILENO, message, sizeof message - 1);
  _exit(1);
}

int main(int argc, char *argv[])
{
  setvbuf(stdout, NULL, _IOLBF, 0);   /* keep what was printed if the kernel crashes */
  signal(SIGALRM, Timeout);
  alarm(10);
  if (argc > 1 && strcmp(argv[1], "wrap") == 0)
     TestWrap();
  else if (argc > 1 && strcmp(argv[1], "events") == 0)
     TestEvents();
  else
     TestTaskSet();

  printf("\n%s\n", Failures ? "FAILURES" : "all checks passed");
  return Failures ? 1 : 0;
}
