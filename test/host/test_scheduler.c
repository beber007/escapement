/* Copyright (c) 2026 Bertrand Hurst. All rights reserved.
** Escapement - Lightweight Power-Aware Real-Time OS.
** Distributed under the terms of LICENSE at the root of this repository.
*/
/* File test_scheduler.c: Exercises the scheduler on the host.
**
** The kernel is compiled as it ships; only the target layer is simulated, so what runs
** here is the scheduling code itself. Time is a variable, which buys two things the
** board cannot give: task sets far larger than the three an example carries, and the
** 2^30 wraparound of the kernel clock, eighteen minutes away on hardware.
**
** Up to four runs, one per process since the kernel keeps its state in statics:
**   test_scheduler         ten tasks, the clock advanced one tick at a time
**   test_scheduler wrap    long periods, the clock jumped from one event to the next
**                          across three wraparounds
**   test_scheduler events  event-driven tasks woken by periodic tasks, by themselves
**                          and by a buffer slot filling up
**   test_scheduler firm    (m,k)-firm tasks under overload, soft kernel only
**
** Under the power-aware kernel every run also checks the speeds it asks for: always one
** of its operating points, and below the fastest as well as at it, since tasks that take
** no time leave room to slow down. The speed changes nothing to the time here.
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

/* Mirror of the head of the task control block, as far as the test reads it. Declared
** here rather than shared because the kernels keep it private; the compiler lays it out
** the same way from the same declaration. The layout is not the same in every build:
** under deadline-monotonic scheduling EscapementHard.c inserts a priority byte after the
** state and drops the two deadline fields, which moves everything after them — reading a
** deadline then returns a neighbouring pointer. That is exactly how this test found that
** the kernel was not scheduling the way the repository said it was. */
#if defined(ESCAPEMENT_VERSION_HARD) && SCHEDULER_REAL_TIME_MODE == DEADLINE_MONOTONIC_SCHEDULING
   typedef struct HostTCB {
     struct HostTCB *Next[2];
     UINT8 TaskState;
     UINT8 Priority;
     INT32 NextArrivalTimeLow;
     void (*TaskCodePtr)(void *);
     void *Argument;
     INT32 PeriodLow;
     UINT16 PeriodHigh;
     UINT16 NextArrivalTimeHigh;
   } HostTCB;
#elif defined(ESCAPEMENT_VERSION_HARD_PA) && SCHEDULER_REAL_TIME_MODE == DEADLINE_MONOTONIC_SCHEDULING
   /* The power-aware kernel keeps its deadline fields under deadline-monotonic scheduling,
   ** and adds the execution time before the cycle counts. */
   typedef struct HostTCB {
     struct HostTCB *Next[2];
     UINT8 TaskState;
     UINT8 Priority;
     INT32 NextArrivalTimeLow;
     void (*TaskCodePtr)(void *);
     void *Argument;
     INT32 PeriodLow;
     INT32 NextDeadline;
     INT32 Deadline;
     INT32 WCET;
     UINT16 PeriodHigh;
     UINT16 NextArrivalTimeHigh;
   } HostTCB;
#elif defined(ESCAPEMENT_VERSION_SOFT) && SCHEDULER_REAL_TIME_MODE == DEADLINE_MONOTONIC_SCHEDULING
   typedef struct HostTCB {
     struct HostTCB *Next[2];
     UINT8 TaskState;
     INT32 NextArrivalTimeLow;
     UINT8 Priority, StaticPriority;
     void (*TaskCodePtr)(void *);
     void *Argument;
     INT32 PeriodLow;
     INT32 NextDeadline;    /* only set for optional instances */
     INT32 Deadline;
     UINT16 PeriodHigh;
     INT32 WCET;
     UINT8 M, K;
     UINT16 NextArrivalTimeHigh;
   } HostTCB;
#else
   typedef struct HostTCB {
     struct HostTCB *Next[2];
     UINT8 TaskState;
     INT32 NextArrivalTimeLow;
     void (*TaskCodePtr)(void *);
     void *Argument;
     INT32 PeriodLow;
     INT32 NextDeadline;
     INT32 Deadline;
   } HostTCB;
#endif

/* The two kernels take different parameters to create a task. Under the soft kernel every
** periodic task here is (1,1)-firm, i.e. hard, with no declared execution time. Its
** event-driven tasks take their workload directly under deadline-monotonic scheduling;
** under EDF they take it too when it is given, and otherwise compute it as
** (wcet << 8) / aperiodicUtilization: half the workload at an utilisation of 128 gives it
** back. TestEvents takes the other path once. */
#if defined(ESCAPEMENT_VERSION_SOFT)
   #define CREATE_TASK(code, period, arg) OSCreateTask(code, 0, 0, period, period, 1, 1, 0, arg)
   #define CREATE_SYNCHRONOUS_TASK(code, workload, event, arg) \
              OSCreateSynchronousTask(code, (workload) / 2, workload, 128, event, arg)
#elif defined(ESCAPEMENT_VERSION_HARD_PA)
   /* The power-aware kernel is given execution times, a twentieth of each period: the
   ** ten tasks of TestTaskSet then declare half the processor. */
   #define CREATE_TASK(code, period, arg) OSCreateTask(code, (period) / 20, 0, period, period, arg)
   #define CREATE_SYNCHRONOUS_TASK(code, workload, event, arg) \
              OSCreateSynchronousTask(code, (workload) / 20, workload, 13, event, arg)
#else
   #define CREATE_TASK(code, period, arg) OSCreateTask(code, 0, period, period, arg)
   #define CREATE_SYNCHRONOUS_TASK(code, workload, event, arg) \
              OSCreateSynchronousTask(code, workload, event, arg)
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
static unsigned OrderBreaks = 0;
static HostTCB *IdleTCB = NULL;

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

/* CheckSpeeds: What the power-aware kernel asked of the processor during a run. When
** slowing down is expected, the speed must have changed and been both below the fastest
** and at it. */
static void CheckSpeeds(BOOL slowsDown)
{
  #if defined(ESCAPEMENT_VERSION_HARD_PA)
     extern unsigned HostSpeedChanges, HostSpeedsUsed, HostInvalidSpeeds;
     char label[80];
     Check("  every speed asked for is an operating point", HostInvalidSpeeds == 0);
     if (!slowsDown) {
        snprintf(label, sizeof label, "  the speed stays at the fastest: %u changes", HostSpeedChanges);
        Check(label, HostSpeedChanges == 0 && HostSpeedsUsed == 1u << OS_MAX_SPEED);
        return;
     }
     snprintf(label, sizeof label, "  the speed changes: %u times", HostSpeedChanges);
     Check(label, HostSpeedChanges > 0);
     Check("  both below the fastest and at it",
           (HostSpeedsUsed & (1u << OS_MAX_SPEED)) && (HostSpeedsUsed & ((1u << OS_MAX_SPEED) - 1)));
  #else
     (void)slowsDown;
  #endif
}

/* StartKernel: Starts the kernel the way a target does. OSStartMultitasking() elects the
** idle task, whose first run starts the timer and lets later signals raise the soft timer
** interrupt instead of waiting for the next tick. */
static void StartKernel(void (*f)(void *), void *arg)
{
  OSStartMultitasking(f, arg);
  IdleTCB = _OSActiveTask;
  IdleTCB->TaskCodePtr(NULL);
}

/* IsLate: Whether an elected task starts past its deadline. Under deadline-monotonic
** scheduling the kernels keep no deadline for a hard instance, and the deadline is the next
** arrival, all tasks here having their deadline equal to their period; an event-driven
** task keeps there the end of its current period too. */
static BOOL IsLate(const HostTCB *task, INT32 now)
{
  #if SCHEDULER_REAL_TIME_MODE == EARLIEST_DEADLINE_FIRST
     return task->NextDeadline < now;
  #else
     if (task->TaskState & TASKTYPE_BLOCKING)
        return task->NextArrivalTimeLow < now;
     return task->NextArrivalTimeHigh == 0 && task->NextArrivalTimeLow <= now;
  #endif
}

/* IsOutOfReach: Whether the absolute deadline, or under deadline-monotonic scheduling the
** next arrival, lies further than one relative deadline away, which a missing time shift
** at the 2^30 wrap would cause. Only periodic tasks carry the relative value to compare. */
static BOOL IsOutOfReach(const HostTCB *task, INT32 now)
{
  if (task->TaskState & TASKTYPE_BLOCKING)
     return FALSE;
  #if SCHEDULER_REAL_TIME_MODE == EARLIEST_DEADLINE_FIRST
     return task->NextDeadline - now > task->Deadline;
  #else
     return task->NextArrivalTimeHigh == 0 && task->NextArrivalTimeLow - now > task->PeriodLow;
  #endif
}

/* ServeSoftTimer: Runs the timer handler once for every soft interrupt requested. */
static void ServeSoftTimer(void)
{
  while (SoftTimerServed != HostSoftTimerRequests) {
     SoftTimerServed += 1;
     _OSTimerInterruptHandler();
  }
}

/* PriorityKey: What orders periodic tasks in the ready queue, the smaller first: the
** absolute deadline under EDF, the priority under deadline-monotonic scheduling. */
#if SCHEDULER_REAL_TIME_MODE == EARLIEST_DEADLINE_FIRST
   #define PriorityKey(task) ((task)->NextDeadline)
#else
   #define PriorityKey(task) ((INT32)(task)->Priority)
#endif

/* RunElected: Calls each task the scheduler elects until only the idle task is left, which
** StartKernel() recognised when it ran it first. All of it happens at one instant, so the
** periodic tasks released by the last timer interrupt must come in priority order; the
** event-driven ones may come in between, woken by the tasks before them. */
static void RunElected(void)
{
  BOOL started = FALSE;
  INT32 lastKey = 0;
  while (TRUE) {
     HostTCB *task;
     ServeSoftTimer();
     task = _OSActiveTask;
     if (task == NULL || task == IdleTCB)
        break;
     if (IsLate(task, HostClockNow()))
        LateArrivals += 1;
     if (IsOutOfReach(task, HostClockNow()))
        DeadlinesOutOfReach += 1;
     if (!(task->TaskState & TASKTYPE_BLOCKING)) {
        if (started && PriorityKey(task) < lastKey)
           OrderBreaks += 1;
        lastKey = PriorityKey(task);
        started = TRUE;
     }
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
  CREATE_TASK(CountingTask, period, (void *)(UINTPTR)NbTasks);
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
  Check("  tasks released together run in priority order", OrderBreaks == 0);
  Check("  no deadline missed", LateArrivals == 0);
  CheckSpeeds(TRUE);
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
  CheckSpeeds(TRUE);
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
  #if defined(ESCAPEMENT_VERSION_SOFT) && SCHEDULER_REAL_TIME_MODE == EARLIEST_DEADLINE_FIRST
     BOOL noWorkLoadRefused;
  #endif

  ToSignaled.Event = OSCreateEventDescriptor();
  CREATE_TASK(SignalerTask, 100, &ToSignaled);
  #if defined(ESCAPEMENT_VERSION_SOFT) && SCHEDULER_REAL_TIME_MODE == EARLIEST_DEADLINE_FIRST
     /* Before any utilization is declared, a workload of 0 leaves nothing to compute it
     ** from; a given one is taken as is, as TestTimerEventF4 does. */
     noWorkLoadRefused = !OSCreateSynchronousTask(SignaledTask, 0, 0, 0, ToSignaled.Event, NULL);
     OSCreateSynchronousTask(SignaledTask, 0, 20, 0, ToSignaled.Event, NULL);
  #else
     CREATE_SYNCHRONOUS_TASK(SignaledTask, 20, ToSignaled.Event, NULL);
  #endif

  CREATE_SYNCHRONOUS_TASK(SelfTask, 20, selfEvent, selfEvent);

  ToShared.Event = sharedEvent;
  CREATE_TASK(SignalerTask, 300, &ToShared);
  CREATE_SYNCHRONOUS_TASK(SharedTask, 60, sharedEvent, (void *)0);
  CREATE_SYNCHRONOUS_TASK(SharedTask, 60, sharedEvent, (void *)1);

  BufferPort = OSInitBuffer(SLOT_SIZE, OS_BUFFER_TYPE_3_SLOT, bufferEvent);
  CREATE_TASK(WriterTask, 50, NULL);
  CREATE_SYNCHRONOUS_TASK(ReaderTask, 40, bufferEvent, NULL);

  StartKernel(StartEvents, selfEvent);
  RunFor(duration);

  printf("\n%d ticks of simulated time, event-driven tasks\n\n", duration);
  #if defined(ESCAPEMENT_VERSION_SOFT) && SCHEDULER_REAL_TIME_MODE == EARLIEST_DEADLINE_FIRST
     Check("  no workload and no utilization: refused", noWorkLoadRefused);
  #endif
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
  /* The power-aware kernel only slows a task down when nothing can arrive before it ends
  ** (one task extension), and a waiting event-driven task can be woken at any time: with
  ** event-driven tasks in the set it keeps the fastest speed. */
  CheckSpeeds(FALSE);
}


#if defined(ESCAPEMENT_VERSION_SOFT)
/* (m,k)-FIRM TASKS -------------------------------------------------------------------- */
/* The soft kernel splits the instances of an (m,k)-firm task into mandatory ones, always
** run, and optional ones, run only if a schedulability test on the declared execution
** times says they will meet their deadline. That test reads the clock and the declared
** times, not the time a task actually takes, so a task set whose declared load exceeds the
** processor makes it drop instances even though the tasks here run in zero time. */

#define MK_INSTANCES 400

typedef struct FirmTask {
  INT32 Period;
  UINT8 M, K;
  UINT8 Ran[MK_INSTANCES];   /* which instances ran, numbered from the first arrival */
  unsigned Runs;
} FirmTask;

static FirmTask Firm[4];

static void FirmTaskCode(void *argument)
{
  FirmTask *task = (FirmTask *)argument;
  INT32 instance = HostClockNow() / task->Period;
  if (instance < MK_INSTANCES)
     task->Ran[instance] = 1;
  task->Runs += 1;
  OSEndTask();
}

static void CreateFirmTask(FirmTask *task, INT32 wcet, INT32 period, UINT8 m, UINT8 k)
{
  task->Period = period;
  task->M = m;
  task->K = k;
  OSCreateTask(FirmTaskCode, wcet, 0, period, period, m, k, 0, task);
}

/* ExactlyMandatory: Whether each aligned window of k instances has exactly m that ran,
** which is what remains when no optional instance can run. */
static BOOL ExactlyMandatory(const FirmTask *task, unsigned instances)
{
  unsigned i, j, ran;
  for (i = 0; i + task->K <= instances; i += task->K) {
     for (ran = 0, j = i; j < i + task->K; j += 1)
        ran += task->Ran[j];
     if (ran != task->M)
        return FALSE;
  }
  return TRUE;
}

/* FirmHolds: Whether every window of k consecutive instances, among the first ones, has at
** least m that ran. */
static BOOL FirmHolds(const FirmTask *task, unsigned instances)
{
  unsigned i, j, ran;
  for (i = 0; i + task->K <= instances; i += 1) {
     for (ran = 0, j = i; j < i + task->K; j += 1)
        ran += task->Ran[j];
     if (ran < task->M)
        return FALSE;
  }
  return TRUE;
}

static void TestFirm(void)
{
  INT32 duration = 30000;
  unsigned a = duration / 100, b = duration / 150, c = duration / 1000, d = duration / 1000;
  char label[80];

  /* Declared loads of 60 %, 60 % and 100 %, 220 % in all; 15 %, 20 % and 60 % for the
  ** mandatory instances alone. */
  CreateFirmTask(&Firm[0], 60, 100, 1, 4);
  CreateFirmTask(&Firm[1], 90, 150, 1, 3);
  /* Declaring its whole period as execution time, this task can never start an optional
  ** instance in time, so only its mandatory ones run: 3 of every 5 if their pattern is. */
  CreateFirmTask(&Firm[3], 1000, 1000, 3, 5);
  /* A task small enough for its optional instances to fit whatever the others do. */
  CreateFirmTask(&Firm[2], 1, 1000, 1, 2);

  StartKernel(NULL, NULL);
  RunFor(duration);

  printf("\n%d ticks of simulated time, (m,k)-firm tasks declaring 220 %% of the processor\n\n",
         duration);
  snprintf(label, sizeof label, "  (1,4) task: %u of %u instances, 1 in every 4 at least", Firm[0].Runs, a);
  Check(label, FirmHolds(&Firm[0], a));
  snprintf(label, sizeof label, "  (1,3) task: %u of %u instances, 1 in every 3 at least", Firm[1].Runs, b);
  Check(label, FirmHolds(&Firm[1], b));
  snprintf(label, sizeof label, "  (3,5) task: %u of %u instances, its mandatory ones only", Firm[3].Runs, d);
  Check(label, FirmHolds(&Firm[3], d) && ExactlyMandatory(&Firm[3], d));
  snprintf(label, sizeof label, "  the overload drops optional instances of the first two: %u",
           a + b - Firm[0].Runs - Firm[1].Runs);
  Check(label, Firm[0].Runs + Firm[1].Runs + 2 < a + b);
  snprintf(label, sizeof label, "  an optional instance that fits still runs: %u of %u", Firm[2].Runs, c);
  Check(label, WithinOne(Firm[2].Runs, c) && FirmHolds(&Firm[2], c) && Firm[2].Runs >= c);
  Check("  no deadline missed", LateArrivals == 0);
}
#endif


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
  #if defined(ESCAPEMENT_VERSION_SOFT)
     else if (argc > 1 && strcmp(argv[1], "firm") == 0)
        TestFirm();
  #endif
  else
     TestTaskSet();

  printf("\n%s\n", Failures ? "FAILURES" : "all checks passed");
  return Failures ? 1 : 0;
}
