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
** Up to nine runs, one per process since the kernel keeps its state in statics:
**   test_scheduler         ten tasks, the clock advanced one tick at a time
**   test_scheduler wrap    long periods, the clock jumped from one event to the next
**                          across three wraparounds
**   test_scheduler events  event-driven tasks woken by periodic tasks, by themselves
**                          and by a buffer slot filling up
**   test_scheduler busy    tasks that take time, each instance its WCET
**   test_scheduler early   the same, instances ending before their WCET
**   test_scheduler slack   the same, one instance leaving time to others
**   test_scheduler expiry  the same, the time left running out while the processor idles
**   test_scheduler reclaim the same, the time left slowing down a task that is not the last
**   test_scheduler firm    (m,k)-firm tasks under overload, soft kernel only
**
** Under the power-aware kernel every run also checks the speeds it asks for: always one
** of its operating points; below the fastest as well as at it where slowing down is
** possible; the fastest only with event-driven tasks in the set, or under DRA when tasks
** leave it nothing to reclaim. Only the runs of tasks that take time let the speed
** stretch the time a task takes.
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
#elif defined(ESCAPEMENT_VERSION_HARD_PA) && SCHEDULER_REAL_TIME_MODE == EARLIEST_DEADLINE_FIRST_STAR
   /* EDF*, which DRA and DR_OTE impose, keeps the arrival that breaks ties between equal
   ** deadlines, and those two the link and times of their simulation queue. */
   typedef struct HostTCB {
     struct HostTCB *Next[2];
     UINT8 TaskState;
     INT32 NextArrivalTimeLow;
     void (*TaskCodePtr)(void *);
     void *Argument;
     INT32 CurrentArrivalTimeLow;
     #if POWER_MANAGEMENT == DRA || POWER_MANAGEMENT == DR_OTE
        struct HostTCB *NextSim;
        INT32 WCET;
        INT32 CompletionTime;
     #endif
     INT32 PeriodLow;
     INT32 NextDeadline;
     INT32 Deadline;
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

/* Scheduling by deadline, EDF or the EDF* of the power-aware kernel, rather than by
** priority. */
#define BY_DEADLINE (SCHEDULER_REAL_TIME_MODE != DEADLINE_MONOTONIC_SCHEDULING)

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
  #if BY_DEADLINE
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
  #if BY_DEADLINE
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
#if BY_DEADLINE
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
  /* DRA gives a task only the time left unused by instances of earlier deadline that have
  ** ended, and here each instance is alone in the ready queue: it keeps the fastest speed.
  ** The others also stretch the last task up to the next arrival. */
  #if defined(ESCAPEMENT_VERSION_HARD_PA) && POWER_MANAGEMENT == DRA
     CheckSpeeds(FALSE);
  #else
     CheckSpeeds(TRUE);
  #endif
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


/* TASKS THAT TAKE TIME ---------------------------------------------------------------- */
/* Everywhere else a task ends the moment it is called, which says what the scheduler
** elects but not whether the speed it picks leaves each task the time it needs. Here each
** instance has work to do, counted in 256ths of a tick at the fastest speed: the test
** hands the processor to the elected task until the next timer event, doing as much work
** as the speed allows — a ratio of _OSSlowdownRatios per tick below the fastest — and
** calls the task only when its work is done, so that it ends at that instant. A timer
** event in between is a preemption: the kernel elects another task, and the work left
** waits for its turn. The deadlines are checked against what the test itself knows of the
** arrivals, not against the kernel's fields. The kernel's own code takes no time, nor does
** a change of speed: what is checked is the policy, not its cost on a processor.
**   busy   every instance takes its WCET: the case each policy must survive
**   early  instances end between a quarter of their WCET and all of it, as real tasks do,
**          which leaves the reclaiming policies time to slow down
**   slack  a task of low priority ends early just before two of higher priority arrive:
**          the time it leaves may go to tasks of lower priority than it only, since they
**          alone counted its WCET in their response time
**   expiry a task ends early and the processor then idles: the time it leaves lasts
**          only as long as the WCET it stands for would have run, and is gone by the
**          next arrivals
**   reclaim the first of three tasks ends early each time: the time it leaves may slow
**          down the second, which the third still follows, and DM_SLACK must use it */

typedef enum { TIMED_BUSY, TIMED_EARLY, TIMED_SLACK, TIMED_EXPIRY, TIMED_RECLAIM } TimedMode;

typedef struct TimedTask {
  INT32 WCET, Period, Deadline;
  INT32 Takes;                /* what each instance takes, 0 when the mode decides */
  int Only;                   /* the one instance that takes it, -1 for all of them */
  unsigned Instance;          /* the one running or next to run, numbered from 0 */
  INT32 Work;                 /* left to do by that instance, in 256ths of a tick */
  unsigned Misses, EarlyStarts;
  INT32 Slow;                 /* ticks run below the fastest speed */
} TimedTask;

#define TIMED_TASKS 4
static TimedTask Timed[TIMED_TASKS];
static unsigned NbTimed;
static TimedMode TimedRun;
static UINT32 TimedSeed = 12345;
static long long BusyAt[8], IdleTime;

#if defined(ESCAPEMENT_VERSION_SOFT)
   #define CREATE_TIMED_TASK(code, wcet, period, deadline, arg) \
              OSCreateTask(code, wcet, 0, period, deadline, 1, 1, 0, arg)
#elif defined(ESCAPEMENT_VERSION_HARD_PA)
   #define CREATE_TIMED_TASK(code, wcet, period, deadline, arg) \
              OSCreateTask(code, wcet, 0, period, deadline, arg)
#else
   #define CREATE_TIMED_TASK(code, wcet, period, deadline, arg) \
              OSCreateTask(code, 0, period, deadline, arg)
#endif

/* Speed: The one the processor runs at, and the work it does per tick. */
static UINT8 Speed(void)
{
  #if defined(ESCAPEMENT_VERSION_HARD_PA)
     return OSGetProcessorSpeed();
  #else
     return 0;
  #endif
}

static INT32 WorkPerTick(UINT8 speed)
{
  #if defined(ESCAPEMENT_VERSION_HARD_PA)
     extern const UINT8 _OSSlowdownRatios[];
     if (speed != OS_MAX_SPEED)
        return _OSSlowdownRatios[speed];
  #endif
  (void)speed;
  return 256;
}

/* NextWork: What the next instance takes, in 256ths of a tick. */
static INT32 NextWork(const TimedTask *task)
{
  INT32 quarter = task->WCET / 4;
  if (task->Takes != 0 && (task->Only < 0 || task->Instance == (unsigned)task->Only))
     return task->Takes * 256;
  if (TimedRun != TIMED_EARLY)
     return task->WCET * 256;
  TimedSeed = TimedSeed * 1103515245u + 12345u;
  return (quarter + (INT32)((TimedSeed >> 16) % (UINT32)(task->WCET - quarter + 1))) * 256;
}

/* TimedTaskCode: Called once the work of the instance is done. */
static void TimedTaskCode(void *argument)
{
  TimedTask *task = (TimedTask *)argument;
  if (HostClockNow() > (INT32)task->Instance * task->Period + task->Deadline)
     task->Misses += 1;
  task->Instance += 1;
  task->Work = NextWork(task);
  OSEndTask();
}

/* RunTimed: Gives the processor to the elected task from one timer event to the next. */
static void RunTimed(INT32 duration)
{
  _OSTimerInterruptHandler();        /* the arrivals at time zero */
  while (HostClockNow() < duration) {
     INT32 now, event, step;
     HostTCB *active;
     ServeSoftTimer();
     now = HostClockNow();
     event = now + HostTicksToNextEvent();
     step = (event < duration ? event : duration) - now;
     active = _OSActiveTask;
     if (active != IdleTCB) {
        TimedTask *task = (TimedTask *)active->Argument;
        UINT8 speed = Speed();
        INT32 rate = WorkPerTick(speed), toEnd = (task->Work + rate - 1) / rate;
        if (now < (INT32)task->Instance * task->Period)
           task->EarlyStarts += 1;
        if (toEnd < step)
           step = toEnd;
        task->Work -= step * rate;
        BusyAt[speed] += step;
        #if defined(ESCAPEMENT_VERSION_HARD_PA)
           if (speed != OS_MAX_SPEED)
              task->Slow += step;
        #endif
        HostAdvanceBy(step);
        if (task->Work <= 0)
           active->TaskCodePtr(active->Argument);
     }
     else {
        IdleTime += step;
        HostAdvanceBy(step);
     }
     if (HostClockNow() == event)
        _OSTimerInterruptHandler();
  }
}

static void CreateTimedTask(INT32 wcet, INT32 period, INT32 deadline, INT32 takes, int only)
{
  TimedTask *task = &Timed[NbTimed++];
  task->WCET = wcet;
  task->Period = period;
  task->Deadline = deadline;
  task->Takes = takes;
  task->Only = only;
  task->Work = NextWork(task);
  CREATE_TIMED_TASK(TimedTaskCode, wcet, period, deadline, task);
}

static void TestTimed(TimedMode mode)
{
  static const char *const what[] = {"their WCET", "a quarter of their WCET to all of it",
                                     "their WCET but one, which ends early",
                                     "their WCET but one, which ends early before an idle time",
                                     "their WCET but the first, which ends early"};
  INT32 duration;
  long long busy = 0;
  unsigned i, misses = 0, earlyStarts = 0;
  char label[80];

  TimedRun = mode;
  if (mode == TIMED_BUSY || mode == TIMED_EARLY) {
     /* Deadline-monotonic scheduling meets these deadlines too: its worst response times
     ** are 200, 500, 1300 and 2700 ticks. The processor is loaded at 70 %. */
     duration = 240000;              /* twenty hyperperiods */
     CreateTimedTask(200, 1000, 1000, 0, -1);
     CreateTimedTask(300, 1500, 1500, 0, -1);
     CreateTimedTask(600, 4000, 4000, 0, -1);
     CreateTimedTask(900, 6000, 6000, 0, -1);
  }
  else if (mode == TIMED_SLACK) {
     /* Worst response times 200, 450 and 1900 ticks under deadline-monotonic scheduling,
     ** against deadlines of 400, 500 and 4000. The first instance of the last task runs
     ** from 450 to 990 and leaves 460 ticks of its WCET, when the two others arrive at
     ** 1000. Given to the first of them, that time stretches its 200 ticks past its
     ** deadline at 1400, which no response time allowed for. */
     duration = 40000;
     CreateTimedTask(200, 1000, 400, 0, -1);
     CreateTimedTask(250, 1000, 500, 0, -1);
     CreateTimedTask(1000, 4000, 4000, 540, -1);
  }
  else if (mode == TIMED_EXPIRY) {
     /* Worst response times 100, 300, 350 and 700 ticks under deadline-monotonic
     ** scheduling, against deadlines of 600, 630, 700 and 720. The third instance of the
     ** first task runs from 1600 to 1651 and leaves 80 ticks of its WCET; the processor
     ** idles from then to 2000, which uses them up. A slack that did not run out would
     ** still be there at 2250 and stretch the 50 ticks of the task of period 750 to 126
     ** at the middle speed; the last task, which has to fit between the three others,
     ** would end at 2776, past its deadline at 2720. A kernel made so passed every other
     ** run; a search over random task sets found this one. */
     duration = 24000;               /* two hyperperiods */
     CreateTimedTask(100, 800, 600, 20, 2);
     CreateTimedTask(200, 1200, 630, 0, -1);
     CreateTimedTask(50, 750, 700, 0, -1);
     CreateTimedTask(350, 2000, 720, 0, -1);
  }
  else {
     /* Worst response times 1000, 1200 and 1600 ticks under deadline-monotonic
     ** scheduling, against deadlines of 2000, 2500 and 3000. The first task ends after
     ** 100 of its 1000 ticks and leaves the rest to the two others. OTE slows down only
     ** the last task of a busy period, the third here; the second, which the third
     ** follows, can slow down only on that time, as DM_SLACK gives it: 900 ticks of
     ** slack and 200 of its own run its 200 ticks at the middle speed. A DM_SLACK that
     ** reclaimed nothing picked the speeds of OTE and passed every other run. */
     duration = 40000;
     CreateTimedTask(1000, 4000, 2000, 100, -1);
     CreateTimedTask(200, 4000, 2500, 0, -1);
     CreateTimedTask(400, 4000, 3000, 0, -1);
  }
  StartKernel(NULL, NULL);
  RunTimed(duration);

  printf("\n%d ticks of simulated time, tasks taking %s\n\n", duration, what[mode]);
  for (i = 0; i < NbTimed; i += 1) {
     misses += Timed[i].Misses;
     earlyStarts += Timed[i].EarlyStarts;
     snprintf(label, sizeof label, "  period %5d: %u instances ended (expected %d)",
              Timed[i].Period, Timed[i].Instance, duration / Timed[i].Period);
     Check(label, WithinOne(Timed[i].Instance, (unsigned)(duration / Timed[i].Period)));
  }
  Check("  no instance started before its arrival", earlyStarts == 0);
  snprintf(label, sizeof label, "  no deadline missed: %u", misses);
  Check(label, misses == 0);
  for (i = 0; i < 8; i += 1)
     busy += BusyAt[i];
  #if defined(ESCAPEMENT_VERSION_HARD_PA)
     printf("  busy %.1f %% of the time: %.1f %% at 12 MHz, %.1f %% at 50, %.1f %% at 125\n",
            100.0 * busy / duration, 100.0 * BusyAt[0] / duration,
            100.0 * BusyAt[1] / duration, 100.0 * BusyAt[2] / duration);
     /* Taking their WCET, the tasks leave DRA nothing to reclaim; the others can still
     ** stretch the last task of a busy period to the next arrival. What the slack, expiry
     ** and reclaim runs check is the deadlines, and under DM_SLACK that reclaim slows the
     ** second task down. */
     if (mode == TIMED_SLACK || mode == TIMED_EXPIRY || mode == TIMED_RECLAIM) {
        extern unsigned HostInvalidSpeeds;
        Check("  every speed asked for is an operating point", HostInvalidSpeeds == 0);
        #if POWER_MANAGEMENT == DM_SLACK
           if (mode == TIMED_RECLAIM) {
              snprintf(label, sizeof label, "  the second task slows down: %d ticks below the fastest",
                       Timed[1].Slow);
              Check(label, Timed[1].Slow > 0);
           }
        #endif
     }
     else
        #if POWER_MANAGEMENT == DRA
           CheckSpeeds(mode == TIMED_EARLY);
        #else
           CheckSpeeds(TRUE);
        #endif
  #else
     printf("  busy %.1f %% of the time\n", 100.0 * busy / duration);
  #endif
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
  ** instance in time, so only its mandatory ones run: exactly 3 in each aligned window of
  ** 5, which ExactlyMandatory checks. */
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
** test has no say; each run takes well under a second. */
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
  else if (argc > 1 && strcmp(argv[1], "busy") == 0)
     TestTimed(TIMED_BUSY);
  else if (argc > 1 && strcmp(argv[1], "early") == 0)
     TestTimed(TIMED_EARLY);
  else if (argc > 1 && strcmp(argv[1], "slack") == 0)
     TestTimed(TIMED_SLACK);
  else if (argc > 1 && strcmp(argv[1], "expiry") == 0)
     TestTimed(TIMED_EXPIRY);
  else if (argc > 1 && strcmp(argv[1], "reclaim") == 0)
     TestTimed(TIMED_RECLAIM);
  #if defined(ESCAPEMENT_VERSION_SOFT)
     else if (argc > 1 && strcmp(argv[1], "firm") == 0)
        TestFirm();
  #endif
  else
     TestTaskSet();

  printf("\n%s\n", Failures ? "FAILURES" : "all checks passed");
  return Failures ? 1 : 0;
}
