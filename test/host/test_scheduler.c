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
** Up to twenty-three runs, one per process since the kernel keeps its state in statics:
**   test_scheduler           ten tasks, the clock advanced one tick at a time
**   test_scheduler create    what the kernel refuses to create
**   test_scheduler priority  an event-driven task created before tasks of shorter deadline
**   test_scheduler wrap      long periods, the clock jumped from one event to the next
**                            across three wraparounds
**   test_scheduler wrapinside the same, the counter wrapping inside the timer handler
**   test_scheduler wrapsim   the same, an instance ending just before a wraparound
**   test_scheduler wrapevents the same, an event-driven task waiting across them
**   test_scheduler lull      one event-driven task woken every 400 s, nothing in between
**   test_scheduler events    event-driven tasks woken by periodic tasks, by themselves
**                            and by a buffer slot filling up
**   test_scheduler suspend   an event-driven task ending at its deadline, the soft timer
**                            interrupt taken inside it
**   test_scheduler signalinside an interrupt signaling an event-driven task as it suspends
**   test_scheduler busy      tasks that take time, each instance its WCET
**   test_scheduler early     the same, instances ending before their WCET
**   test_scheduler slack     the same, one instance leaving time to others
**   test_scheduler expiry    the same, the time left running out while the processor idles
**   test_scheduler reclaim   the same, the time left slowing down a task that is not the last
**   test_scheduler reuse     the same, that time wanted by two tasks in turn
**   test_scheduler overrun   the same, a task taking six times its WCET
**   test_scheduler minspeed  the same, the power-aware kernel kept above its slowest speed
**   test_scheduler firm      (m,k)-firm tasks under overload, soft kernel only
**   test_scheduler firmwrap  optional instances across the wraparound, soft kernel only
**   test_scheduler firmwrapend an instance ending at the very tick of the wraparound,
**                            soft kernel under EDF only
**   test_scheduler firmlong  an optional instance of 2^23 ticks, soft kernel only
**   test_scheduler firmevents (m,k)-firm tasks beside an event-driven one, soft kernel only
**
** Under the power-aware kernel every run also checks the speeds it asks for: always one
** of its operating points; below the fastest as well as at it where slowing down is
** possible; the fastest only with event-driven tasks in the set, or under DRA when tasks
** leave it nothing to reclaim. Only the runs of tasks that take time let the speed
** stretch the time a task takes.
**
** Tasks do not run on a stack of their own — there is no context switch here. The test
** calls the elected task itself, which is enough to observe what the scheduler decided.
** A soft timer interrupt requested by a task is served as soon as that task returns, or,
** where a run asks for it, at once inside the task, the tasks it elects running on top.
*/

#include <setjmp.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#include "Escapement.h"

/* ChildExit: A child process ends with _exit, which skips the handler that writes its
** profile under tools/coverage.py: what only a child ran, the ends at the wrap of the
** counter among it, counted for nothing until 2026-10-06. The profile is written first. */
#ifdef HOST_COVERAGE
   int __llvm_profile_write_file(void);
#endif
static void ChildExit(int status)
{
  #ifdef HOST_COVERAGE
     (void)__llvm_profile_write_file();
  #endif
  _exit(status);
}

/* Mirror of the head of the task control block, as far as the test reads it. Declared
** here rather than shared because the kernels keep it private; the compiler lays it out
** the same way from the same declaration. The layout is not the same in every build:
** under deadline-monotonic scheduling EscapementHard.c inserts a priority byte after the
** state and drops the two deadline fields, which moves everything after them — reading a
** deadline through the wrong mirror returns a neighbouring pointer. */
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
#elif defined(ESCAPEMENT_VERSION_HARD_PA) && POWER_MANAGEMENT == DM_SLACK
   /* DM_SLACK keeps the execution time and the work left before the period. */
   typedef struct HostTCB {
     struct HostTCB *Next[2];
     UINT8 TaskState;
     UINT8 Priority;
     INT32 NextArrivalTimeLow;
     void (*TaskCodePtr)(void *);
     void *Argument;
     INT32 WCET;
     INT32 RemainingWork;
     INT32 PeriodLow;
     INT32 NextDeadline;
     INT32 Deadline;
     UINT16 PeriodHigh;
     UINT16 NextArrivalTimeHigh;
   } HostTCB;
#elif defined(ESCAPEMENT_VERSION_HARD_PA) && SCHEDULER_REAL_TIME_MODE == DEADLINE_MONOTONIC_SCHEDULING
   /* The power-aware kernel keeps its deadline fields under deadline-monotonic scheduling,
   ** and under OTE adds the execution time before the cycle counts. */
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

extern HostTCB *_OSActiveTask, *_OSQueueHead;
extern BOOL _OSNoSaveContext;
extern void _OSTimerInterruptHandler(void);

/* What the kernel must ask of the target each time, which the host's port only counts: a
** task that ends, by OSEndTask or OSSuspendSynchronousTask, has its context left unsaved
** (_OSNoSaveContext) and asks for a context switch; the timer handler asks for one before
** it returns. Left out, each passed every run until 2026-10-04 (tools/mutants.py): on the
** target the next task would not be elected, or a context that ended would be restored. */
static unsigned EndBreaks = 0, HandlerBreaks = 0;
extern unsigned HostTimerStarted;
static void TimerHandler(void)
{
  unsigned asked = HostContextSwitchesRequested;
  if (!HostTimerStarted) {
     printf("FAILED: the timer's handler ran before the timer started\n");
     exit(1);
  }
  _OSTimerInterruptHandler();
  if (HostContextSwitchesRequested == asked)
     HandlerBreaks += 1;
}
extern void HostAdvanceBy(INT32 delta);
extern void (*HostOverflowCheckHook)(void);
extern void (*HostTimeReadHook)(void);
extern void HostSetClock(INT32 time);
extern void HostLoseReservation(void);
extern INT32 HostTicksToNextEvent(void);
extern unsigned HostClockWraps;
extern unsigned HostSoftTimerRequests, HostSoftTimerCleared;
extern void *_OSStackBasePointer;
extern void (*HostSoftTimerClearHook)(void);

#define MAX_TASKS 40
static unsigned Activations[MAX_TASKS];
static INT32    Periods[MAX_TASKS];
static unsigned NbTasks = 0;
static unsigned Failures = 0;
static unsigned LateArrivals = 0;
static unsigned DeadlinesOutOfReach = 0;
static unsigned SoftTimerServed = 0;
static unsigned OrderBreaks = 0;
static unsigned QueueBreaks = 0;
static HostTCB *IdleTCB = NULL;
static jmp_buf TaskFrame;

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
  unsigned asked = HostContextSwitchesRequested;
  OSStartMultitasking(f, arg);
  /* On the target the start returns into the idle task through a context switch, with
  ** interrupts unmasked; the idle task then sleeps (a reading of the surviving mutants,
  ** 2026-10-06). */
  if (HostContextSwitchesRequested == asked || HostMasked || _OSStackBasePointer == NULL) {
     printf("FAILED: the start asked no context switch, left interrupts masked, or left the"
            " stack's base unset\n");
     exit(1);
  }
  IdleTCB = _OSActiveTask;
  _OSNoSaveContext = FALSE;   /* cleared by the context switch to the idle task */
  IdleTCB->TaskCodePtr(NULL);
  if (HostMasked || HostSlept == 0) {
     printf("FAILED: the idle task left interrupts masked, or never slept\n");
     exit(1);
  }
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
  #elif defined(ESCAPEMENT_VERSION_HARD_PA)
     /* The power-aware kernel keeps the absolute deadline under deadline-monotonic
     ** scheduling too, to bound how far it slows a task down. */
     return task->NextDeadline - now > task->Deadline ||
            (task->NextArrivalTimeHigh == 0 && task->NextArrivalTimeLow - now > task->PeriodLow);
  #else
     return task->NextArrivalTimeHigh == 0 && task->NextArrivalTimeLow - now > task->PeriodLow;
  #endif
}

/* ServeSoftTimer: Runs the timer handler once for every soft interrupt requested, but
** those its PENDSTCLR dropped. */
static void ServeSoftTimer(void)
{
  while (SoftTimerServed != HostSoftTimerRequests) {
     if ((int)(HostSoftTimerCleared - SoftTimerServed) > 0) {
        SoftTimerServed = HostSoftTimerCleared;
        continue;
     }
     SoftTimerServed += 1;
     TimerHandler();
  }
}

/* ReadyQueueHolds: Whether the ready queue leads from its head to the idle task, its
** tail, without a loop: a task inserted twice breaks it. */
static BOOL ReadyQueueHolds(void)
{
  HostTCB *task = _OSQueueHead;
  unsigned steps;
  for (steps = 0; steps <= MAX_TASKS + 1; steps += 1)
     if ((task = task->Next[0]) == IdleTCB)
        return TRUE;
  return FALSE;
}

/* PriorityKey: What orders periodic tasks in the ready queue, the smaller first: the
** absolute deadline under EDF, the priority under deadline-monotonic scheduling. */
#if BY_DEADLINE
   #define PriorityKey(task) ((task)->NextDeadline)
#else
   #define PriorityKey(task) ((INT32)(task)->Priority)
#endif

/* ElectedNoZombie: A task the kernel elects to run is never a zombie, one that has left
** the ready queue, but while it ends, its context not to be saved: an event-driven task
** put back in the ready queue still a zombie was run, and the next timer interrupt
** discarded a context (a reading of the surviving mutants, 2026-10-06). */
static void ElectedNoZombie(HostTCB *task, BOOL ending)
{
  if ((task->TaskState & 0x02) && !ending) {                /* STATE_ZOMBIE */
     printf("FAILED: a zombie elected\n");
     exit(1);
  }
}

/* RunElected: Calls each task the scheduler elects until only the idle task is left, which
** StartKernel() recognised when it ran it first. All of it happens at one instant, so the
** periodic tasks released by the last timer interrupt must come in priority order; the
** event-driven ones may come in between, woken by the tasks before them. */
static void RunElected(HostTCB *interrupted)
{
  BOOL started = FALSE;
  INT32 lastKey = 0;
  while (TRUE) {
     HostTCB *task;
     ServeSoftTimer();
     task = _OSActiveTask;
     if (task == NULL || task == IdleTCB || task == interrupted)
        break;
     ElectedNoZombie(task, FALSE);
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
     if (setjmp(TaskFrame) != 0) {  /* the kernel ended the task in its place */
        HostLLHook = NULL;          /* hooks the task would have removed */
        HostSoftTimerHook = NULL;
        HostUnmaskHook = NULL;
        _OSNoSaveContext = FALSE;   /* cleared by the context switch that follows */
        continue;
     }
     {
        unsigned asked = HostContextSwitchesRequested;
        task->TaskCodePtr(task->Argument);
        if (_OSActiveTask != task &&
            (!_OSNoSaveContext || HostContextSwitchesRequested == asked))
           EndBreaks += 1;
     }
     _OSNoSaveContext = FALSE;
     if (!ReadyQueueHolds()) {
        QueueBreaks += 1;
        break;
     }
     if (_OSActiveTask == task)     /* the task did not end: stop rather than spin */
        break;
  }
}

/* Finalize: What FinalizeContextSwitchPreparation (Escapement_CortexMx.c) does on entry
** of the soft timer interrupt: completes a task found removing itself from the ready
** queue, or, in the soft kernel under EDF, promoting itself, and discards its context. */
static void Finalize(void)
{
  _OSActiveTask = _OSQueueHead->Next[0];
  if (_OSActiveTask->TaskState & 0x02) {                  /* STATE_ZOMBIE */
     _OSActiveTask = _OSQueueHead->Next[0] = _OSActiveTask->Next[0];
     _OSNoSaveContext = TRUE;
  }
  #if defined(ESCAPEMENT_VERSION_SOFT) && BY_DEADLINE
     else if (_OSActiveTask->TaskState & 0x10) {          /* STATE_ACTIVATE */
        extern HostTCB *_OSQueueTail;
        if (_OSActiveTask->Next[0] != _OSQueueTail) {
           _OSQueueTail->Next[0] = _OSActiveTask->Next[0];
           _OSActiveTask->Next[0] = _OSQueueTail;
        }
        _OSActiveTask->TaskState = 0x00;                  /* STATE_INIT */
        _OSNoSaveContext = TRUE;
     }
  #endif
}

/* SoftTimerNow: Takes a soft timer interrupt at once, inside the task that raised it. A
** task that raised it while ending, its context no longer to be saved (_OSNoSaveContext),
** is gone: the context switch never returns to it, and the test does not either.
** Otherwise the tasks the handler elected run first, on top of the interrupted one as on
** the single stack of the target, until it is at the head of the ready queue again. */
static void SoftTimerNow(void)
{
  HostTCB *interrupted = _OSActiveTask;
  jmp_buf frame;
  HostSoftTimerHook = NULL;
  SoftTimerServed += 1;
  Finalize();
  TimerHandler();
  if (_OSNoSaveContext)
     longjmp(TaskFrame, 1);
  memcpy(frame, TaskFrame, sizeof frame);
  RunElected(interrupted);
  // cppcheck-suppress uninitvar ; set by the first memcpy, which cppcheck does not follow
  memcpy(TaskFrame, frame, sizeof frame);
}

/* RunFor: Advances the clock the way the hardware would and lets the kernel schedule,
** calling each elected task. Returns the number of scheduling rounds. */
static unsigned RunFor(INT32 duration)
{
  INT32 target = HostClockNow() + duration;
  unsigned rounds = 0;
  while (HostClockNow() < target) {
     HostAdvanceBy(1);
     TimerHandler();
     rounds += 1;
     RunElected(NULL);
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

/* SimOutOfReach: Under DRA and DR_OTE, the simulation queue keeps an instance that ended
** before its WCET until the simulation has used that WCET up, and its deadline must follow
** the clock at the wraparound as those of the ready queue do. */
static unsigned SimDeadlinesOutOfReach = 0;
static void CheckSimQueue(void)
{
  #if defined(ESCAPEMENT_VERSION_HARD_PA) && (POWER_MANAGEMENT == DRA || POWER_MANAGEMENT == DR_OTE)
     HostTCB *task = _OSQueueHead;
     unsigned steps;
     for (steps = 0; steps <= MAX_TASKS + 1 && (task = task->NextSim) != IdleTCB; steps += 1)
        if ((task->TaskState & TASKTYPE_BLOCKING) == 0 &&
            task->NextDeadline - HostClockNow() > task->Deadline)
           SimDeadlinesOutOfReach += 1;
     if (steps > MAX_TASKS + 1)
        QueueBreaks += 1;
  #endif
}

/* DueAfter: Whether the elected task's deadline lies more than ticks ahead, as IsLate
** reads it; the idle task, never due, is not held. */
static BOOL DueAfter(const HostTCB *task, INT32 ticks)
{
  INT32 now = HostClockNow();
  if (task == NULL || task == IdleTCB)
     return FALSE;
  if (task->TaskState & TASKTYPE_BLOCKING)
     return task->NextArrivalTimeLow - now > ticks;
  #if BY_DEADLINE || defined(ESCAPEMENT_VERSION_SOFT) || defined(ESCAPEMENT_VERSION_HARD_PA)
     return task->NextDeadline - now > ticks;
  #else
     /* The hard kernel under DM keeps no deadline: the next arrival stands for it, the
     ** tasks here due at their period, and has none to shift in the ready queue. */
     return task->NextArrivalTimeHigh == 0 && task->NextArrivalTimeLow - now > ticks;
  #endif
}

static INT32 HookedTicks = 0;        /* moved by a hook inside the handler, see wrapinside */
static void RunAcross(long long duration)
{
  long long elapsed = 0;
  TimerHandler();        /* the arrivals at time zero */
  RunElected(NULL);
  while (TRUE) {
     INT32 delta = TicksToNextInterrupt();
     if (elapsed + delta > duration)
        break;
     HostAdvanceBy(delta);
     elapsed += delta;
     TimerHandler();
     elapsed += HookedTicks;
     HookedTicks = 0;
     CheckSimQueue();
     /* Held over the wrap only when its deadline lies beyond it: its deadline then sits in
     ** the ready queue while the kernel shifts the times, which only that reaches. Held
     ** whenever the wrap was the next interrupt, a task of a period past 2^30, released at
     ** 1000 and due at 2000, ran a whole turn late (longperiod, 2026-10-04); held only
     ** within LATENCY of the wrap, no task sat in the ready queue across it, and the shift
     ** of its deadlines ran in no test. */
     if (HostClockNow() + TicksToNextInterrupt() != 0x40000000 ||
         !DueAfter(_OSActiveTask, 0x40000000 - HostClockNow()))
        RunElected(NULL);
  }
}

static void CreateTask(INT32 period)
{
  Periods[NbTasks] = period;
  CREATE_TASK(CountingTask, period, (void *)(UINTPTR)NbTasks);
  NbTasks += 1;
}

/* Within one of the count expected: the run stops between a signal and its handling. */
static int WithinOne(unsigned count, unsigned expected)
{
  return count + 1 >= expected && count <= expected + 1;
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

/* WrapInside: The counter wraps once the handler has found no overflow and before it
** reads the time, as it can on a target: the handler entered for an arrival due shortly
** before the wraparound reads a time from after it, while its arrivals are not shifted
** yet. The overflow interrupt, of higher priority, has raised its flag meanwhile, which
** the handler tests again before it returns. */
#define WRAP_WINDOW 1000
static unsigned WrapsInside = 0;
static void WrapInside(void)
{
  INT32 now = HostClockNow();
  if (now >= 0x40000000 - WRAP_WINDOW) {
     HookedTicks += 0x40000000 - now + 50;
     HostAdvanceBy(0x40000000 - now + 50);
     WrapsInside += 1;
  }
}

/* TestWrapInside: TestWrap's run, with a task of period 1000 whose last arrival before each
** wraparound, 824 ticks short of it (2^30 modulo 1000), finds the counter wrapping inside
** the handler. The arrival is then served late by that much, within its deadline. */
static void TestWrapInside(void)
{
  long long duration = 3LL * 0x40000000 + 1000000;

  CreateTask(1000);      CreateTask(1999993);   CreateTask(99999989);
  CreateTask(700000001);

  StartKernel(NULL, NULL);
  HostOverflowCheckHook = WrapInside;
  RunAcross(duration);
  HostOverflowCheckHook = NULL;

  printf("\n%u tasks, %lld ticks of simulated time, the counter wrapping inside the handler\n\n",
         NbTasks, duration);
  CheckActivations(duration);
  Check("  the counter wrapped inside the handler at each wraparound", WrapsInside == 3);
  Check("  the kernel clock wrapped three times", HostClockWraps == 3);
  Check("  no deadline missed", LateArrivals == 0);
  Check("  every deadline shifted with the clock", DeadlinesOutOfReach == 0);
}

/* TestWrapSim: A task of period 536870000 is released and ends 1824 ticks short of the
** first wraparound, between arrivals of a task of period 1000 that keep the test running
** it at once. Its WCET, a twentieth of its period, keeps it in the simulation queue of
** DRA and DR_OTE well past the wraparound. */
static void TestWrapSim(void)
{
  long long duration = 3LL * 0x40000000 + 1000000;

  CreateTask(1000);
  CreateTask(536870000);

  StartKernel(NULL, NULL);
  RunAcross(duration);

  printf("\n%u tasks, %lld ticks of simulated time, an instance ending before the wraparound\n\n",
         NbTasks, duration);
  CheckActivations(duration);
  Check("  the kernel clock wrapped three times", HostClockWraps == 3);
  Check("  no deadline missed", LateArrivals == 0);
  Check("  every deadline shifted with the clock",
        DeadlinesOutOfReach == 0 && SimDeadlinesOutOfReach == 0);
  Check("  the simulation queue stays whole", QueueBreaks == 0);
}

/* TestWrapEvents: An event-driven task that signals itself waits in the arrival queue
** until the end of its period, or under EDF its deadline, and that time may lie beyond the
** next wraparound, where a periodic task counts wraparounds apart. The two must still
** come in the order of their times: a periodic task queued behind an event-driven one
** due later is released late, or released again while still ready. */
static unsigned LoopRuns;
static void StartEvents(void *event);
static void LoopingTask(void *event)
{
  LoopRuns += 1;
  OSScheduleSuspendedTask(event);
  OSSuspendSynchronousTask();
}

static void TestWrapEvents(void)
{
  long long duration = 3LL * 0x40000000 + 1000000;
  void *event = OSCreateEventDescriptor();

  CreateTask(10007);
  CREATE_SYNCHRONOUS_TASK(LoopingTask, 50000, event, event);

  StartKernel(StartEvents, event);
  RunAcross(duration);

  printf("\n%u tasks, %lld ticks of simulated time, one event-driven task signalling itself\n\n",
         NbTasks + 1, duration);
  CheckActivations(duration);
  Check("  the kernel clock wrapped three times", HostClockWraps == 3);
  Check("  the event-driven task keeps running", LoopRuns > duration / 50000 / 2);
  Check("  no deadline missed", LateArrivals == 0);
}

/* TestCreate: What the kernel cannot count with is refused when the task is created,
** rather than hanging or corrupting it later: a period of 0 releases a task again within
** the same handler, a remainder of 2^30 or more or 65535 turns of 2^30 overflow the
** arrival time, a deadline of 2^30 or more the absolute deadline under EDF. An event-
** driven task needs an event, and a workload from 1 tick to below 2^30. */
#if defined(ESCAPEMENT_VERSION_SOFT)
   #define TRY_TASK(cycles, offset, deadline) \
              OSCreateTask(CountingTask, 1, cycles, offset, deadline, 1, 1, 0, NULL)
#elif defined(ESCAPEMENT_VERSION_HARD_PA)
   #define TRY_TASK(cycles, offset, deadline) \
              OSCreateTask(CountingTask, 1, cycles, offset, deadline, NULL)
#else
   #define TRY_TASK(cycles, offset, deadline) \
              OSCreateTask(CountingTask, cycles, offset, deadline, NULL)
#endif

static void EventTask(void *argument)
{
  (void)argument;
  OSSuspendSynchronousTask();
}

static void TestCreate(void)
{
  void *event = OSCreateEventDescriptor();
  unsigned i, accepted;

  printf("\ntasks the kernel refuses to create\n\n");
  /* Before any other task: the kernel is set up, then the control block of the task cannot
  ** be allocated. Created later, the task meets a full queue or priorities first. */
  HostMallocBudget = 0;
  Check("  the first event-driven task when memory runs out",
        !CREATE_SYNCHRONOUS_TASK(EventTask, 20, event, NULL));
  HostMallocBudget = -1;
  #if defined(ESCAPEMENT_VERSION_SOFT)
     Check("  a negative WCET", !OSCreateTask(CountingTask, -1, 0, 100, 100, 1, 1, 0, NULL));
     Check("  a WCET past the deadline",
           !OSCreateTask(CountingTask, 101, 0, 200, 100, 1, 1, 0, NULL));
     Check("  an event-driven task with a negative WCET",
           !OSCreateSynchronousTask(EventTask, -1, 100, 0, event, NULL));
     Check("  an event-driven task with a WCET past its workload",
           !OSCreateSynchronousTask(EventTask, 101, 100, 0, event, NULL));
  #elif defined(ESCAPEMENT_VERSION_HARD_PA)
     Check("  a negative WCET", !OSCreateTask(CountingTask, -1, 0, 100, 100, NULL));
     Check("  a WCET past the deadline", !OSCreateTask(CountingTask, 101, 0, 200, 100, NULL));
     Check("  an event-driven task with a negative WCET",
           !OSCreateSynchronousTask(EventTask, -1, 100, 13, event, NULL));
     Check("  an event-driven task with a WCET past its workload",
           !OSCreateSynchronousTask(EventTask, 101, 100, 13, event, NULL));
  #endif
  #if defined(ESCAPEMENT_VERSION_SOFT) && BY_DEADLINE
     /* Its share of the processor, the smallest n / 256 with n * workload >= WCET * 256,
     ** would be 256 / 256, which a byte does not hold. */
     Check("  an event-driven task whose WCET is its workload",
           !OSCreateSynchronousTask(EventTask, 100, 100, 0, event, NULL));
  #endif
  Check("  a period of 0", !TRY_TASK(0, 0, 1));
  Check("  a negative remainder", !TRY_TASK(1, -5, 100));
  Check("  a remainder of 2^30", !TRY_TASK(0, 0x40000000, 100));
  Check("  65535 turns of 2^30", !TRY_TASK(0xFFFF, 0, 100));
  Check("  a deadline of 0", !TRY_TASK(0, 100, 0));
  Check("  a deadline past the period", !TRY_TASK(0, 100, 101));
  Check("  a deadline of 2^30", !TRY_TASK(1, 0, 0x40000000));
  Check("  but a deadline equal to a period below 2^30", TRY_TASK(0, 100, 100));
  Check("  and a deadline below 2^30 in a longer period", TRY_TASK(0xFFFE, 5, 0x3FFFFFFF));
  #if defined(ESCAPEMENT_VERSION_SOFT)
     Check("  m of 0", !OSCreateTask(CountingTask, 1, 0, 100, 100, 0, 1, 0, NULL));
     Check("  k of 0", !OSCreateTask(CountingTask, 1, 0, 100, 100, 1, 0, 0, NULL));
     Check("  m above k", !OSCreateTask(CountingTask, 1, 0, 100, 100, 3, 2, 0, NULL));
     Check("  a first instance beyond what the arrival time counts",
           !OSCreateTask(CountingTask, 1, 0x8000, 0, 100, 1, 2, 2, NULL));
     Check("  but a (1,2)-firm task starting at instance 1",
           OSCreateTask(CountingTask, 1, 0, 100, 100, 1, 2, 1, NULL));
  #endif
  Check("  an event-driven task without its event",
        !CREATE_SYNCHRONOUS_TASK(EventTask, 20, NULL, NULL));
  Check("  a workload of 0", !CREATE_SYNCHRONOUS_TASK(EventTask, 0, event, NULL));
  Check("  a workload of 2^30", !CREATE_SYNCHRONOUS_TASK(EventTask, 0x40000000, event, NULL));
  Check("  but a workload of 20", CREATE_SYNCHRONOUS_TASK(EventTask, 20, event, NULL));
  /* The queue of an event counts its tasks in a byte, and so do the priorities under
  ** deadline-monotonic scheduling, the soft kernel adding the number of tasks to the
  ** priority of an optional instance. */
  for (i = 0, accepted = 1; i < 300; i += 1)
     accepted += CREATE_SYNCHRONOUS_TASK(EventTask, 20, event, NULL);
  /* Memory used up: each creation says so, and so does the start of the kernel when it
  ** cannot allocate the queue of an event. */
  HostMallocBudget = 0;
  Check("  a task when memory runs out", !TRY_TASK(0, 100, 100));
  Check("  an event descriptor when memory runs out", OSCreateEventDescriptor() == NULL);
  Check("  an event-driven task when memory runs out",
        !CREATE_SYNCHRONOUS_TASK(EventTask, 20, event, NULL));
  /* OSStartMultitasking returns on the host even when it starts: that it elected no task
  ** is what tells it gave up. */
  Check("  the start of the kernel when memory runs out",
        !OSStartMultitasking(NULL, NULL) && _OSActiveTask == NULL);
  HostMallocBudget = -1;
  #if BY_DEADLINE
     Check("  more than 255 tasks waiting on one event", accepted == 255);
  #elif defined(ESCAPEMENT_VERSION_SOFT)
     Check("  more than 127 tasks in all", accepted + 3 == 127);  /* three periodic */
  #else
     Check("  more than 255 tasks in all", accepted + 2 == 255);
  #endif
}

/* TestSignals: Signals of an event coalesce while its task has not run: the first wakes
** the task, the second is kept, and the third finds it kept and adds nothing, so the task
** runs twice. An event no task was created for has no queue: signalling it does nothing. */
static unsigned CoalescedRuns;
static void CoalescedTask(void *argument)
{
  (void)argument;
  CoalescedRuns += 1;
  OSSuspendSynchronousTask();
}

static void TestSignals(void)
{
  void *event = OSCreateEventDescriptor(), *alone = OSCreateEventDescriptor();
  char label[80];

  CREATE_SYNCHRONOUS_TASK(CoalescedTask, 100, event, NULL);
  StartKernel(NULL, NULL);
  OSScheduleSuspendedTask(alone);
  OSScheduleSuspendedTask(event);
  OSScheduleSuspendedTask(event);
  OSScheduleSuspendedTask(event);
  RunElected(NULL);
  RunFor(1000);

  printf("\nthree signals before the task runs, and an event without a task\n\n");
  snprintf(label, sizeof label, "  the task ran twice: %u", CoalescedRuns);
  Check(label, CoalescedRuns == 2);
  Check("  the ready queue stays whole", QueueBreaks == 0);
}

/* TestNoTask: The kernel started with no task created sets itself up there, and runs the
** idle task. */
static void TestNoTask(void)
{
  printf("\nthe kernel started without a task\n\n");
  /* It returns on the host, FALSE, as it does when it gives up: the task it elected
  ** tells the two apart. */
  OSStartMultitasking(NULL, NULL);
  Check("  it starts, the idle task elected",
        _OSActiveTask != NULL && (_OSActiveTask->TaskState & TASKTYPE_BLOCKING) != 0);
  /* Nothing allocated before: the start must set the base the first context switch
  ** starts the idle task's stack at (mutant 226 of EscapementHard.c). */
  Check("  the stack's base set for the first context switch", _OSStackBasePointer != NULL);
}

/* TestCreateBounds: Tasks the kernel must accept, at the edges of what it refuses: a
** period of one whole turn of 2^30 and no remainder, a deadline of one tick, a deadline
** past the remainder in a period of a turn or more, a workload of one tick. Each edge
** moved by one was accepted by no run before (tools/mutants.py, 2026-10-04). */
static void TestCreateBounds(void)
{
  void *event = OSCreateEventDescriptor();
  printf("\ntasks the kernel must accept, at the edges\n\n");
  Check("  a period of one turn of 2^30, no remainder", TRY_TASK(1, 0, 100));
  Check("  a period and a deadline of one tick", TRY_TASK(0, 1, 1));
  Check("  a turn and 5 ticks, a deadline of 100", TRY_TASK(1, 5, 100));
  Check("  an event-driven task of workload 1", CREATE_SYNCHRONOUS_TASK(EventTask, 1, event, NULL));
}

/* TestLongPeriod: A task of period one turn of 2^30 and 1000 ticks, which counts its turns
** apart (PeriodHigh, NextArrivalTimeHigh), run across three wraps of the clock: released
** at 0, 2^30 + 1000, 2^31 + 2000 and 3 * 2^30 + 3000, four times, each on time. Until
** RunAcross was fixed the same day, each ran a whole turn after its release: the harness
** held a task elected whenever the wrap was its next interrupt. */
#if defined(ESCAPEMENT_VERSION_SOFT)
   #define CREATE_LONG_TASK(cycles, offset, deadline, arg) \
              OSCreateTask(CountingTask, 0, cycles, offset, deadline, 1, 1, 0, arg)
#elif defined(ESCAPEMENT_VERSION_HARD_PA)
   #define CREATE_LONG_TASK(cycles, offset, deadline, arg) \
              OSCreateTask(CountingTask, 0, cycles, offset, deadline, arg)
#else
   #define CREATE_LONG_TASK(cycles, offset, deadline, arg) \
              OSCreateTask(CountingTask, cycles, offset, deadline, arg)
#endif
static void TestLongPeriod(void)
{
  long long duration = 3LL * 0x40000000 + 100000;
  char label[80];

  CREATE_LONG_TASK(1, 1000, 1000, (void *)0);
  StartKernel(NULL, NULL);
  RunAcross(duration);

  printf("\n%lld ticks of simulated time, a period past 2^30\n\n", duration);
  Check("  the kernel clock wrapped three times", HostClockWraps == 3);
  snprintf(label, sizeof label, "  the task ran four times: %u", Activations[0]);
  Check(label, Activations[0] == 4);
  Check("  no deadline missed", LateArrivals == 0);
}

/* TestTwoSignals: Two events signalled at the same instant, before the timer handler
** runs: both tasks wait in its list of tasks to release, and both must run. */
static unsigned FirstRuns, SecondRuns;
static void FirstTask(void *argument) { (void)argument; FirstRuns += 1; OSSuspendSynchronousTask(); }
static void SecondTask(void *argument) { (void)argument; SecondRuns += 1; OSSuspendSynchronousTask(); }

static void TestTwoSignals(void)
{
  void *first = OSCreateEventDescriptor(), *second = OSCreateEventDescriptor();

  CREATE_SYNCHRONOUS_TASK(FirstTask, 100, first, NULL);
  CREATE_SYNCHRONOUS_TASK(SecondTask, 100, second, NULL);
  StartKernel(NULL, NULL);
  OSScheduleSuspendedTask(first);
  OSScheduleSuspendedTask(second);
  RunElected(NULL);
  RunFor(1000);

  printf("\ntwo events signalled at once\n\n");
  Check("  both tasks ran, once each", FirstRuns == 1 && SecondRuns == 1);
  Check("  the ready queue stays whole", QueueBreaks == 0);
}

/* TestPriority: An event-driven task created first, with a workload longer than the
** deadlines of the periodic tasks created after it, is signalled before the timer starts
** and released with them at time 0: it must run after them. Its priority is its rank
** among the deadlines, which the tasks created after it change. */
static char RunOrder[8];
static unsigned NbRun;
static void OrderedTask(void *argument)
{
  if (NbRun < sizeof RunOrder)
     RunOrder[NbRun++] = (char)(UINTPTR)argument;
  OSEndTask();
}

static void OrderedEventTask(void *argument)
{
  if (NbRun < sizeof RunOrder)
     RunOrder[NbRun++] = (char)(UINTPTR)argument;
  OSSuspendSynchronousTask();
}

static void TestPriority(void)
{
  void *event = OSCreateEventDescriptor();
  CREATE_SYNCHRONOUS_TASK(OrderedEventTask, 1000, event, (void *)'E');
  CREATE_TASK(OrderedTask, 50, (void *)'A');
  CREATE_TASK(OrderedTask, 100, (void *)'T');

  StartKernel(StartEvents, event);
  RunFor(1);

  printf("\nan event-driven task created before tasks of shorter deadline\n\n");
  printf("  ran first: %.3s\n", RunOrder);
  Check("  it runs after them", NbRun >= 3 && memcmp(RunOrder, "ATE", 3) == 0);
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

static Signaler ToSignaled, ToShared, ToLull, ToTurns;
#define MAX_TURNS 100
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
  if (!HostMasked) {   /* the start runs its function before unmasking interrupts */
     printf("FAILED: the start's function ran with interrupts unmasked\n");
     exit(1);
  }
  OSScheduleSuspendedTask(event);
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
  // cppcheck-suppress intToPointerCast ; the argument is a number, the task casts it back
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


/* TestEventTurns: Three event-driven tasks wait for one event, which a periodic task
** signals 60 times: they take turns, in the order they queued, each time. The indices of
** the event's queue wrap at a multiple of its length (GetFIFOArrayMaxIndex), set when the
** kernel starts (mutant 236 of EscapementHard.c left it to what OSMalloc held). OSMalloc
** gives each word a small count here, and the event's descriptor is the first allocated:
** that bound would be 8, which 3 does not divide, the queue taking the wrong slot after 8
** operations. */
static int TurnOrder[MAX_TURNS];
static unsigned NbTurns;
static void TurnTask(void *argument)
{
  if (NbTurns < MAX_TURNS)
     TurnOrder[NbTurns] = (int)(UINTPTR)argument;
  NbTurns += 1;
  OSSuspendSynchronousTask();
}

static void TestEventTurns(void)
{
  void *event = OSCreateEventDescriptor();
  unsigned i, breaks = 0;
  char label[80];

  ToTurns.Event = event;
  CREATE_TASK(SignalerTask, 100, &ToTurns);
  for (i = 0; i < 3; i += 1)
     // cppcheck-suppress intToPointerCast ; the argument is a number, the task casts it back
     CREATE_SYNCHRONOUS_TASK(TurnTask, 60, event, (void *)(UINTPTR)i);
  StartKernel(NULL, NULL);
  RunFor(6000);

  printf("\n6000 ticks of simulated time, three event-driven tasks on one event\n\n");
  snprintf(label, sizeof label, "  one wake-up per signal: %u for %u", NbTurns, ToTurns.Runs);
  Check(label, NbTurns <= ToTurns.Runs && NbTurns + 1 >= ToTurns.Runs && NbTurns >= 50);
  for (i = 3; i < NbTurns && i < MAX_TURNS; i += 1)
     if (TurnOrder[i] != TurnOrder[i - 3])
        breaks += 1;
  snprintf(label, sizeof label, "  they take turns: %u out of order", breaks);
  Check(label, breaks == 0 && TurnOrder[0] != TurnOrder[1] && TurnOrder[1] != TurnOrder[2] &&
               TurnOrder[0] != TurnOrder[2]);
}

/* TestLull: A task of 400 s signals an event-driven task, and nothing else happens in
** between: the time jumps from one event to the next, and the power-aware kernel's
** reclaiming policies account for the aperiodic bandwidth over the whole interval at
** once — 4e8 ticks times the utilisation, which does not fit in 32 bits. */
static void TestLull(void)
{
  long long duration = 3LL * 400000000 + 1000;
  char label[80];

  ToLull.Event = OSCreateEventDescriptor();
  CREATE_TASK(SignalerTask, 400000000, &ToLull);
  CREATE_SYNCHRONOUS_TASK(SignaledTask, 20000, ToLull.Event, NULL);

  StartKernel(NULL, NULL);
  RunAcross(duration);

  printf("\n%lld ticks of simulated time, one signal every 400 s\n\n", duration);
  snprintf(label, sizeof label, "  one wake-up per signal: %u for %u", SignaledRuns, ToLull.Runs);
  Check(label, ToLull.Runs == 4 && SignaledRuns + 1 >= ToLull.Runs &&
               SignaledRuns <= ToLull.Runs);
  Check("  no deadline missed", LateArrivals == 0);
  CheckSpeeds(FALSE);
}


/* TestSuspend: An event-driven task ends exactly at its deadline, or under deadline-
** monotonic scheduling at the end of its period, as a task set that uses the whole
** processor lets it, and a signal is then pending: its next instance may start at once.
** The soft timer interrupt raised by OSSuspendSynchronousTask is taken at once, as on the
** target, while the task has not yet left the ready queue. A periodic task signals it
** every 100 ticks, and every other instance signals itself: two instances of 20 ticks
** back to back. */
#define SUSPEND_LOAD 20
static Signaler ToSuspending;
static unsigned SuspendRuns;
static void SuspendingTask(void *event)
{
  SuspendRuns += 1;
  if (SuspendRuns % 2 == 1)
     OSScheduleSuspendedTask(event);
  HostAdvanceBy(SUSPEND_LOAD);
  HostSoftTimerHook = SoftTimerNow;
  OSSuspendSynchronousTask();
  HostSoftTimerHook = NULL;
}

static void TestSuspend(void)
{
  INT32 duration = 20000;
  char label[80];

  ToSuspending.Event = OSCreateEventDescriptor();
  CREATE_TASK(SignalerTask, 100, &ToSuspending);
  CreateTask(150);
  CREATE_SYNCHRONOUS_TASK(SuspendingTask, SUSPEND_LOAD, ToSuspending.Event, ToSuspending.Event);

  StartKernel(NULL, NULL);
  RunFor(duration);

  printf("\n%d ticks of simulated time, an event-driven task ending at its deadline\n\n",
         duration);
  CheckActivations(duration);
  snprintf(label, sizeof label, "  two instances back to back per signal: %u for %u",
           SuspendRuns, ToSuspending.Runs);
  Check(label, SuspendRuns + 2 >= 2 * ToSuspending.Runs && SuspendRuns <= 2 * ToSuspending.Runs);
  Check("  the ready queue stays whole", QueueBreaks == 0);
  Check("  no deadline missed", LateArrivals == 0);
}


/* TestSignalInside: An interrupt signals an event-driven task while it suspends
** itself, at each of the LLs of OSSuspendSynchronousTask in turn, one instance after
** another; the soft timer interrupt it raises is taken at once. Every other time the
** interrupt wakes instead an event-driven task of higher priority, which preempts the
** first and signals it itself. A task signaled before it had left the ready queue was
** inserted in it again, or had the context of another discarded in its place — that of
** the task preempting it: SoakPico hung under the soft kernel (2026-09-25). OSSuspend-
** SynchronousTask masks interrupts from its enqueue to its leaving the ready queue: an
** interrupt that comes in between is taken after, and must still wake the task once. */
static Signaler ToInside;
static void *HelperEvent;
static unsigned InsideRuns, HelperRuns, InsideDirect, InsideViaHelper, InsideAt, InsideCount;

static void Interrupt(void)
{
  HostUnmaskHook = NULL;
  if (InsideRuns % 2) {
     InsideDirect += 1;
     OSScheduleSuspendedTask(ToInside.Event);
  }
  else {
     InsideViaHelper += 1;
     OSScheduleSuspendedTask(HelperEvent);
  }
}

static BOOL SignalInside(void)
{
  if (++InsideCount != InsideAt)
     return FALSE;
  HostLLHook = NULL;
  if (HostMasked) {       // taken when interrupts are unmasked
     HostUnmaskHook = Interrupt;
     return FALSE;
  }
  Interrupt();
  return TRUE;
}

static void InsideTask(void *argument)
{
  (void)argument;
  InsideRuns += 1;
  InsideCount = 0;
  InsideAt = 1 + InsideRuns / 2 % 8;
  HostLLHook = SignalInside;
  HostSoftTimerHook = SoftTimerNow;
  OSSuspendSynchronousTask();
  HostLLHook = NULL;
  HostSoftTimerHook = NULL;
}

/* HelperTask: Of higher priority, it signals the task it preempted. */
static void HelperTask(void *argument)
{
  (void)argument;
  HelperRuns += 1;
  OSScheduleSuspendedTask(ToInside.Event);
  OSSuspendSynchronousTask();
}

static void TestSignalInside(void)
{
  INT32 duration = 20000;
  unsigned total;
  char label[80];

  ToInside.Event = OSCreateEventDescriptor();
  HelperEvent = OSCreateEventDescriptor();
  CREATE_TASK(SignalerTask, 100, &ToInside);
  CreateTask(150);
  CREATE_SYNCHRONOUS_TASK(InsideTask, 20, ToInside.Event, NULL);
  CREATE_SYNCHRONOUS_TASK(HelperTask, 10, HelperEvent, NULL);

  StartKernel(NULL, NULL);
  RunFor(duration);
  HostLLHook = NULL;
  HostSoftTimerHook = NULL;
  HostUnmaskHook = NULL;

  printf("\n%d ticks of simulated time, an event-driven task signaled as it suspends\n\n",
         duration);
  CheckActivations(duration);
  /* A signal sent while one is already pending does not add up (docs/api.md): where
  ** the interrupt falls before the task masks interrupts, as under DM_SLACK, whose slack
  ** takes LLs of its own first, two signals can make one run. */
  total = ToInside.Runs + InsideDirect + HelperRuns;
  snprintf(label, sizeof label, "  one run per signal: %u for %u + %u + %u", InsideRuns,
           ToInside.Runs, InsideDirect, HelperRuns);
  Check(label, InsideDirect > 0 && HelperRuns > 0 && InsideRuns <= total + 1 &&
               InsideRuns + total / 50 + 1 >= total);
  snprintf(label, sizeof label, "  the task of higher priority ran each time: %u for %u",
           HelperRuns, InsideViaHelper);
  Check(label, WithinOne(HelperRuns, InsideViaHelper));
  Check("  the ready queue stays whole", QueueBreaks == 0);
  Check("  no deadline missed", LateArrivals == 0);
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
**          down the second, which the third still follows, and DM_SLACK must use it
**   reuse  a task slowed down on the time left is preempted by one that may use it too:
**          what the first has used of it is no longer there to give
**   overrun a task takes six times its WCET: once past it, the kernel knows no longer
**          what it has left to do, and must run it at the fastest speed
**   minspeed a light load, two tasks released together with equal deadlines, the power-
**          aware kernel kept above its slowest speed */

typedef enum { TIMED_BUSY, TIMED_EARLY, TIMED_SLACK, TIMED_EXPIRY, TIMED_RECLAIM,
               TIMED_REUSE, TIMED_OVERRUN, TIMED_MINSPEED, TIMED_IDLE, TIMED_FIRMWAIT } TimedMode;

typedef struct TimedTask {
  INT32 WCET, Period, Deadline;
  INT32 Takes;                /* what each instance takes, 0 when the mode decides */
  int Only;                   /* the one instance that takes it, -1 for all of them */
  unsigned Instance;          /* the one running or next to run, numbered from 0 */
  INT32 Work;                 /* left to do by that instance, in 256ths of a tick */
  unsigned Misses, EarlyStarts;
  INT32 Slow;                 /* ticks run below the fastest speed */
  UINT8 Ran[32];              /* firmwait: the instances that ran, from the first */
  void *Event;                /* the event of an event-driven task, NULL if periodic */
  BOOL Each;                  /* trace: each instance takes what EachTakes says, its work
                              ** drawn when it is first elected (Work 0) */
} TimedTask;

#define TIMED_TASKS 8
static TimedTask Timed[TIMED_TASKS];
#define MAX_EACH 1024
static INT32 EachTakes[TIMED_TASKS][MAX_EACH];   /* by instance, from 0; 0 for its WCET */
/* trace: who runs, from when, for how long and at what speed, and when each instance
** ends, for tools/differential.py to check against the algorithms. */
static FILE *TraceOut = NULL;
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
  if (task->Each) {
     INT32 takes = task->Instance < MAX_EACH ? EachTakes[task - Timed][task->Instance] : 0;
     return (takes != 0 ? takes : task->WCET) * 256;
  }
  if (task->Takes != 0 && (task->Only < 0 || task->Instance == (unsigned)task->Only))
     return task->Takes * 256;
  if (TimedRun != TIMED_EARLY)
     return task->WCET * 256;
  TimedSeed = TimedSeed * 1103515245u + 12345u;
  return (quarter + (INT32)((TimedSeed >> 16) % (UINT32)(task->WCET - quarter + 1))) * 256;
}

/* The timed runs start at TimedPhase of the counter, zero but for timewrap, and count
** time from there: TimedNow. */
static INT32 TimedPhase, TimedStart;
static unsigned TimedStartWraps;
static INT32 TimedNow(void)
{
  return (INT32)((long long)(HostClockWraps - TimedStartWraps) * 0x40000000 +
                 HostClockNow() - TimedStart);
}
#define MAX_ENDS 64
static INT32 Ends[MAX_ENDS];   /* when the first instances ended, for timewrap */
static unsigned NbEnds;
static INT32 EventEnds[MAX_ENDS];   /* the same for event-driven tasks */
static unsigned NbEventEnds;

/* TimedTaskCode: Called once the work of the instance is done. */
static void TimedTaskCode(void *argument)
{
  TimedTask *task = (TimedTask *)argument;
  if (TraceOut != NULL)
     fprintf(TraceOut, "E %d %d\n", TimedNow(), (int)(task - Timed));
  if (task->Event != NULL) {
     if (NbEventEnds < MAX_ENDS)
        EventEnds[NbEventEnds++] = TimedNow();
     task->Instance += 1;
     task->Work = NextWork(task);
     OSSuspendSynchronousTask();
     return;
  }
  if (NbEnds < MAX_ENDS)
     Ends[NbEnds++] = TimedNow();
  if (TimedRun == TIMED_FIRMWAIT) {
     /* Instances of an (m,k)-firm task may not run: the one ending is known from its
     ** next arrival, which the kernel has already set. */
     task->Instance = (unsigned)(_OSActiveTask->NextArrivalTimeLow / task->Period) - 1;
     if (task->Instance < sizeof task->Ran)
        task->Ran[task->Instance] += 1;
  }
  if (TimedNow() > (INT32)task->Instance * task->Period + task->Deadline)
     task->Misses += 1;
  task->Instance += 1;
  task->Work = task->Each ? 0 : NextWork(task);
  OSEndTask();
}

/* The timer interrupt taken inside the kernel: at its compiler barriers (endinside), where
** TimedTrace sums up who ran when and how fast, to compare with a run that takes none; or
** at the wrap of the counter (timewrap), at the WrapPoint-th time read or barrier of a task
** ending one tick before it. */
static BOOL AtBarriers, AtWrap;
static unsigned WrapPoint, WrapPoints, WrapsTaken;
static UINT32 BarrierSeed = 1, TimedTrace;
static unsigned BarrierInterrupts, BarrierPreemptions, BarrierZombies;
static jmp_buf TimedFrame;
static void RunTimedUntil(INT32 duration, HostTCB *interrupted);

/* RunTimed: Gives the processor to the elected task from one timer event to the next. */
static void RunTimed(INT32 duration)
{
  if (TimedPhase != 0) {             /* the arrivals moved from time zero to the phase */
     HostTCB *task;
     for (task = _OSQueueHead->Next[1]; task != IdleTCB; task = task->Next[1])
        task->NextArrivalTimeLow = TimedPhase;
     HostSetClock(TimedPhase);
  }
  TimedStart = TimedPhase;
  TimedStartWraps = HostClockWraps;
  TimerHandler();        /* the first arrivals */
  RunTimedUntil(duration, NULL);
}

static void SetKernelHooks(void);

/* Signals of an event, sent at those times as an interrupt handler would (firmeventwait). */
#define MAX_SIGNALS 256
static INT32 SignalAt[MAX_SIGNALS];
static void *SignalTo[MAX_SIGNALS];    /* the event of each signal, SignalEvent if NULL */
static unsigned NbSignals, NextSignal;
static void *SignalEvent;

#if defined(ESCAPEMENT_VERSION_SOFT)
/* CountStillReady: Instances found still in the ready queue, not yet started, when their
** task arrives again (firmwait), those of optional instances after the idle task under
** EDF included. */
static unsigned StillReady;
static void CountStillReady(void)
{
  HostTCB *task = _OSQueueHead;
  unsigned steps;
  for (steps = 0; steps <= MAX_TASKS + 1 && (task = task->Next[0]) != NULL; steps += 1)
     if (task != IdleTCB && (task->TaskState & 0x0B) == 0 &&   /* INIT, periodic */
         task->NextArrivalTimeLow <= HostClockNow())
        StillReady += 1;
}

/* WholeReadyQueue: The ready queue, and under EDF the optional instances after it, lead
** to their end with no task twice. */
static BOOL WholeReadyQueue(void)
{
  HostTCB *seen[MAX_TASKS + 2], *task = _OSQueueHead;
  unsigned n = 0, i;
  while ((task = task->Next[0]) != NULL) {
     if (n == MAX_TASKS + 2)
        return FALSE;
     for (i = 0; i < n; i += 1)
        if (seen[i] == task)
           return FALSE;
     seen[n++] = task;
  }
  return TRUE;
}
#endif

/* TakeInterrupt: Takes the soft timer interrupt inside the kernel's code. A task found
** ending is gone, its context discarded; a task elected over the interrupted one runs
** first, until the interrupted one is at the head of the ready queue again, as on the
** single stack. The reservation of an LL is lost, as on the target. */
static void TakeInterrupt(void)
{
  HostTCB *interrupted = _OSActiveTask;
  jmp_buf frame;
  HostCompilerBarrierHook = NULL;
  HostTimeReadHook = NULL;
  HostLoseReservation();
  BarrierInterrupts += 1;
  Finalize();
  TimerHandler();
  if (_OSNoSaveContext) {
     BarrierZombies += 1;
     longjmp(TimedFrame, 1);
  }
  if (_OSActiveTask != interrupted) {
     BarrierPreemptions += 1;
     memcpy(frame, TimedFrame, sizeof frame);
     RunTimedUntil(INT32_MAX, interrupted);
     // cppcheck-suppress uninitvar ; set by the memcpy above
     memcpy(TimedFrame, frame, sizeof frame);
  }
  SetKernelHooks();
}

/* BarrierInterrupt: At no cost in time, one barrier in two picked at random. */
static void BarrierInterrupt(void)
{
  BarrierSeed = BarrierSeed * 1103515245u + 12345u;
  if (((BarrierSeed >> 16) & 1) == 0)
     TakeInterrupt();
}

/* WrapInterrupt: One tick before the wrap, the WrapPoint-th time read or barrier moves
** the clock past it and takes the interrupt. */
static void WrapInterrupt(void)
{
  if (HostClockNow() != 0x3FFFFFFF || WrapPoints++ != WrapPoint)
     return;
  WrapsTaken += 1;
  HostAdvanceBy(1);
  TakeInterrupt();
}

static void SetKernelHooks(void)
{
  HostCompilerBarrierHook = AtBarriers ? BarrierInterrupt : AtWrap ? WrapInterrupt : NULL;
  HostTimeReadHook = AtWrap ? WrapInterrupt : NULL;
}

/* RunTimedUntil: The loop of RunTimed, which an interrupt inside the kernel enters again
** to run the tasks it elected, until the one it interrupted is elected again. */
static void RunTimedUntil(INT32 duration, HostTCB *interrupted)
{
  while (TimedNow() < duration) {
     INT32 now, toEvent, step;
     HostTCB *active;
     ServeSoftTimer();
     if (interrupted != NULL && _OSActiveTask == interrupted)
        return;
     now = TimedNow();
     if (NextSignal < NbSignals && SignalAt[NextSignal] == now) {
        NextSignal += 1;
        OSScheduleSuspendedTask(SignalTo[NextSignal - 1] != NULL ? SignalTo[NextSignal - 1]
                                                                 : SignalEvent);
        continue;                      /* the soft timer interrupt it raises is served */
     }
     toEvent = HostTicksToNextEvent();
     step = toEvent < duration - now ? toEvent : duration - now;
     if (NextSignal < NbSignals && SignalAt[NextSignal] - now < step)
        step = SignalAt[NextSignal] - now;
     active = _OSActiveTask;
     if (active != IdleTCB) {
        TimedTask *task = (TimedTask *)active->Argument;
        ElectedNoZombie(active, _OSNoSaveContext);
        if (task->Each && task->Work == 0) {
           /* An (m,k)-firm task's instances may be dropped: the one elected is known
           ** from the time, every instance running within its period. */
           task->Instance = (unsigned)(now / task->Period);
           task->Work = NextWork(task);
           if (task->Instance < sizeof task->Ran)
              task->Ran[task->Instance] += 1;
        }
        UINT8 speed = Speed();
        INT32 rate = WorkPerTick(speed), toEnd = (task->Work + rate - 1) / rate;
        if (now < (INT32)task->Instance * task->Period)
           task->EarlyStarts += 1;
        if (toEnd < step)
           step = toEnd;
        TimedTrace = TimedTrace * 31u + (UINT32)now * 7u + (UINT32)(task - Timed) * 3u + speed;
        if (TraceOut != NULL)
           fprintf(TraceOut, "S %d %d %d %u\n", now, step, (int)(task - Timed), speed);
        task->Work -= step * rate;
        BusyAt[speed] += step;
        #if defined(ESCAPEMENT_VERSION_HARD_PA)
           if (speed != OS_MAX_SPEED)
              task->Slow += step;
        #endif
        HostAdvanceBy(step);
        if (task->Work <= 0) {
           if (setjmp(TimedFrame) == 0) {
              unsigned asked = HostContextSwitchesRequested;
              WrapPoints = 0;
              SetKernelHooks();
              active->TaskCodePtr(active->Argument);
              if (!_OSNoSaveContext || HostContextSwitchesRequested == asked)
                 EndBreaks += 1;
           }
           HostCompilerBarrierHook = NULL;
           HostTimeReadHook = NULL;
           _OSNoSaveContext = FALSE;   /* the context switch that follows clears it */
           if (TimedNow() != now + step)
              continue;                /* the clock moved inside: its event is served */
        }
     }
     else {
        if (TraceOut != NULL)
           fprintf(TraceOut, "S %d %d -1 0\n", now, step);
        IdleTime += step;
        HostAdvanceBy(step);
     }
     if (step == toEvent) {
        #if defined(ESCAPEMENT_VERSION_SOFT)
           if (TimedRun == TIMED_FIRMWAIT)
              CountStillReady();
        #endif
        if (TraceOut != NULL)
           fprintf(TraceOut, "I %d\n", TimedNow());
        TimerHandler();
        CheckSimQueue();
        #if defined(ESCAPEMENT_VERSION_SOFT)
           if (TimedRun == TIMED_FIRMWAIT && !WholeReadyQueue())
              QueueBreaks += 1;
        #endif
     }
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
                                     "their WCET but the first, which ends early",
                                     "their WCET but one, whose time left is shared",
                                     "their WCET but one, which takes six times it",
                                     "a quarter of their WCET to all of it, a light load",
                                     "their WCET, the processor idle before two arrive together"};
  INT32 duration;
  long long busy = 0;
  unsigned i, misses = 0, earlyStarts = 0;
  char label[80];

  TimedRun = mode;
  if (mode == TIMED_MINSPEED) {
     /* A load of 6 %, which every policy would run at 12 MHz; the first two tasks share
     ** a period, so that their deadlines are equal, which EDF* breaks by arrival, then by
     ** address. */
     TimedRun = TIMED_EARLY;
     duration = 240000;
     CreateTimedTask(20, 1000, 1000, 0, -1);
     CreateTimedTask(30, 1000, 1000, 0, -1);
     CreateTimedTask(40, 4000, 4000, 0, -1);
     #if defined(ESCAPEMENT_VERSION_HARD_PA)
        OSSetMinimalProcessorSpeed(OS_50MHZ_SPEED);
     #endif
  }
  else if (mode == TIMED_BUSY || mode == TIMED_EARLY) {
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
  else if (mode == TIMED_IDLE) {
     /* The first task ends at 1200, and the processor idles until the two others arrive
     ** together at 1500, the second of them with 300 ticks of work within 700: a slack
     ** the first task left would slow it down, and the first, back at 2000, would find
     ** it far from done. Worst response times 200, 500 and 600 ticks. */
     duration = 24000;
     CreateTimedTask(200, 1000, 500, 0, -1);
     CreateTimedTask(300, 1500, 700, 0, -1);
     CreateTimedTask(100, 1500, 1500, 0, -1);
  }
  else if (mode == TIMED_OVERRUN) {
     /* The second task, alone after the first, stretches its 100 ticks to the next
     ** arrival at 500 and runs at the middle speed, doing about 199 of the 600 it takes.
     ** Resumed after the first task at 501 with its WCET used up, it ran at the slowest
     ** speed, 4277 ticks for the 401 left, past its deadline at 2000. */
     duration = 20000;
     CreateTimedTask(1, 500, 500, 0, -1);
     CreateTimedTask(100, 2000, 2000, 600, -1);
  }
  else if (mode == TIMED_REUSE) {
     /* Worst response times 400, 441, 482 and 582 ticks under deadline-monotonic
     ** scheduling, against deadlines of 500, 550, 600 and 2000. The second instance of the
     ** first task ends at 2001 and leaves 399 ticks, which slow the third task down to
     ** 12 MHz. The second arrives at 2201 and may slow down on what is left of them; given
     ** the 399 ticks again, it ran at 12 MHz until 2639, and the third ended at 2662, past
     ** its deadline at 2600. */
     duration = 20000;
     CreateTimedTask(400, 2000, 500, 1, 1);
     CreateTimedTask(41, 2201, 550, 0, -1);
     CreateTimedTask(41, 2000, 600, 0, -1);
     CreateTimedTask(100, 2000, 2000, 0, -1);
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
     if (mode == TIMED_MINSPEED) {
        extern unsigned HostSpeedsUsed, HostInvalidSpeeds;
        Check("  every speed asked for is an operating point", HostInvalidSpeeds == 0);
        Check("  never below the minimal speed", (HostSpeedsUsed & 1u << OS_12MHZ_SPEED) == 0);
     }
     else if (mode == TIMED_SLACK || mode == TIMED_EXPIRY || mode == TIMED_RECLAIM ||
         mode == TIMED_REUSE || mode == TIMED_OVERRUN || mode == TIMED_IDLE) {
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


/* TestEndInside: A timed run with the soft timer interrupt taken at the compiler barriers
** of the kernel, where a task ending or electing the next leaves its stores in the order
** the handler relies on: the handler must complete what it finds half done. The time does
** not move there, so the run must be the one without those interrupts, down to who ran
** when and how fast, which a child process runs first. */
static void TestEndInside(TimedMode mode)
{
  int fds[2];
  UINT32 baseline = 0;
  pid_t child;
  char label[96];
  if (pipe(fds) != 0 || (child = fork()) < 0) {
     Check("  fork", FALSE);
     return;
  }
  if (child == 0) {
     FILE *quiet = freopen("/dev/null", "w", stdout);
     (void)quiet;
     TestTimed(mode);
     ChildExit(write(fds[1], &TimedTrace, sizeof TimedTrace) == sizeof TimedTrace ? 0 : 1);
  }
  close(fds[1]);
  AtBarriers = TRUE;
  TestTimed(mode);
  if (read(fds[0], &baseline, sizeof baseline) != sizeof baseline)
     Check("  the run without interrupts", FALSE);
  waitpid(child, NULL, 0);
  printf("  %u interrupts at the barriers: %u found a task ending, %u elected another\n",
         BarrierInterrupts, BarrierZombies, BarrierPreemptions);
  snprintf(label, sizeof label, "  the same run as without them: %08x against %08x",
           TimedTrace, baseline);
  Check(label, TimedTrace == baseline);
}

/* TestTimeWrap: A task ending one tick before the counter wraps, the interrupt of the
** wrap taken at each time read and barrier of its end in turn, where the kernel may hold
** a time read before the shift. A first run, away from the wrap, finds when the instances
** end; the same run is then made to start so that one of them ends at the last tick
** before the wrap, the schedule being the same at any phase of the counter. Each run is a
** child process, the kernel keeping its state in its own variables.
** Then each end at the very tick of the wrap, before the timer's handler has shifted the
** kernel's times: the task reads a time already wrapped, which the power-aware kernel
** once took 2^30 short (tools/differential.py across the wraparound, 2026-10-06). */
#define WRAP_ENDS   16
#define WRAP_POINTS 4
static void TestTimeWrap(TimedMode mode)
{
  INT32 ends[WRAP_ENDS];
  unsigned e, p, taken = 0, failed = 0;
  char label[96];
  int fds[2];
  pid_t child;
  if (pipe(fds) != 0 || (child = fork()) < 0) {
     Check("  fork", FALSE);
     return;
  }
  if (child == 0) {
     FILE *quiet = freopen("/dev/null", "w", stdout);
     (void)quiet;
     TimedPhase = 0x20000000;
     TestTimed(mode);
     ChildExit(write(fds[1], Ends, sizeof ends) == sizeof ends ? 0 : 1);
  }
  close(fds[1]);
  if (read(fds[0], ends, sizeof ends) != sizeof ends) {
     Check("  the run away from the wrap", FALSE);
     return;
  }
  waitpid(child, NULL, 0);
  for (e = 0; e < WRAP_ENDS; e += 1)
     for (p = 0; p < WRAP_POINTS; p += 1) {
        int status = 0;
        if (pipe(fds) != 0 || (child = fork()) < 0) {
           Check("  fork", FALSE);
           return;
        }
        if (child == 0) {
           FILE *quiet = freopen("/dev/null", "w", stdout);
           (void)quiet;
           TimedPhase = 0x3FFFFFFF - ends[e];
           WrapsTaken = 0;
           AtWrap = TRUE;
           WrapPoint = p;
           TestTimed(mode);
           ChildExit(write(fds[1], &WrapsTaken, sizeof WrapsTaken) == sizeof WrapsTaken &&
                 Failures == 0 ? 0 : 1);
        }
        close(fds[1]);
        if (read(fds[0], &WrapsTaken, sizeof WrapsTaken) != sizeof WrapsTaken)
           WrapsTaken = 0;
        close(fds[0]);
        waitpid(child, &status, 0);
        taken += WrapsTaken;
        if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
           failed += 1;
           printf("  the end at %d, point %u: FAILED\n", ends[e], p);
        }
     }
  for (e = 0; e < WRAP_ENDS; e += 1) {
     int status = 0;
     if ((child = fork()) < 0) {
        Check("  fork", FALSE);
        return;
     }
     if (child == 0) {
        FILE *quiet = freopen("/dev/null", "w", stdout);
        (void)quiet;
        TimedPhase = 0x40000000 - ends[e];
        TestTimed(mode);
        ChildExit(Failures == 0 ? 0 : 1);
     }
     waitpid(child, &status, 0);
     if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        failed += 1;
        printf("  the end at %d, at the wrap: FAILED\n", ends[e]);
     }
  }
  snprintf(label, sizeof label, "  %u runs, the wrap taken inside %u times, and %u ends at "
           "the wrap: every check held", (unsigned)(WRAP_ENDS * WRAP_POINTS), taken,
           (unsigned)WRAP_ENDS);
  Check(label, failed == 0 && taken >= WRAP_ENDS);
}

/* TestTrace: Runs the tasks read on the standard input, one per line, and prints the trace
** (TraceOut) for tools/differential.py, which checks it against the scheduling algorithm
** the build was made for:
**   P wcet period deadline takes   a periodic task, each instance taking "takes" ticks at
**                                  the fastest speed
**   E takes workload               an event-driven task: its instances take "takes" ticks,
**                                  and come a workload apart at least
**   S time task                    a signal of that event-driven task's event, the signals
**                                  given in the order of their times
**   F wcet period deadline takes m k   an (m,k)-firm task, soft kernel only, its pattern
**                                  starting at instance 0
**   D task instance takes          what that instance of a periodic or (m,k)-firm task
**                                  takes, the others of the task their WCET
** Tasks are numbered in the order of their lines, as the trace numbers them. The run
** starts at phase of the counter, 0 unless given: a phase short of 2^30 takes the run
** across the wraparound of the kernel clock, the trace counting time from the start. */
static void TestTrace(INT32 duration, INT32 phase)
{
  char kind[2];
  long a, b, c, d;
  #if defined(ESCAPEMENT_VERSION_SOFT)
     long m, k;
  #endif
  TraceOut = stdout;
  TimedRun = TIMED_BUSY;
  while (scanf("%1s", kind) == 1) {
     if (kind[0] != 'S' && NbTimed == TIMED_TASKS) {
        printf("FAILED: more than %d tasks\n", TIMED_TASKS);   /* not one left out unseen */
        exit(1);
     }
     if (kind[0] == 'P' && scanf("%ld %ld %ld %ld", &a, &b, &c, &d) == 4 &&
         NbTimed < TIMED_TASKS)
        CreateTimedTask((INT32)a, (INT32)b, (INT32)c, (INT32)d, -1);
     #if defined(ESCAPEMENT_VERSION_SOFT)
     else if (kind[0] == 'F' && scanf("%ld %ld %ld %ld %ld %ld", &a, &b, &c, &d, &m, &k) == 6 &&
              NbTimed < TIMED_TASKS) {
        TimedTask *task = &Timed[NbTimed++];
        task->WCET = (INT32)a;
        task->Period = (INT32)b;
        task->Deadline = (INT32)c;
        task->Takes = (INT32)d;
        task->Only = -1;
        task->Work = NextWork(task);
        OSCreateTask(TimedTaskCode, (INT32)a, 0, (INT32)b, (INT32)c, (UINT8)m, (UINT8)k, 0, task);
     }
     #endif
     else if (kind[0] == 'E' && scanf("%ld %ld", &a, &b) == 2 && NbTimed < TIMED_TASKS) {
        TimedTask *task = &Timed[NbTimed++];
        task->WCET = (INT32)a;
        task->Takes = 0;
        task->Only = -1;
        task->Work = task->WCET * 256;
        task->Event = OSCreateEventDescriptor();
        #if defined(ESCAPEMENT_VERSION_SOFT)
           OSCreateSynchronousTask(TimedTaskCode, (INT32)a, (INT32)b, 0, task->Event, task);
        #elif defined(ESCAPEMENT_VERSION_HARD_PA)
           OSCreateSynchronousTask(TimedTaskCode, (INT32)a, (INT32)b,
                                   (UINT8)((a * 256 + b - 1) / b), task->Event, task);
        #else
           OSCreateSynchronousTask(TimedTaskCode, (INT32)b, task->Event, task);
        #endif
     }
     else if (kind[0] == 'S' && scanf("%ld %ld", &a, &b) == 2 && NbSignals < MAX_SIGNALS &&
              b >= 0 && b < (long)NbTimed) {
        SignalAt[NbSignals] = (INT32)a;
        SignalTo[NbSignals++] = Timed[b].Event;
     }
     else if (kind[0] == 'D' && scanf("%ld %ld %ld", &a, &b, &c) == 3 && a >= 0 &&
              a < (long)NbTimed && Timed[a].Event == NULL && b >= 0 && b < MAX_EACH &&
              c > 0) {
        EachTakes[a][b] = (INT32)c;
        Timed[a].Each = TRUE;
        Timed[a].Work = 0;
     }
     else {
        printf("FAILED: a line not understood, at \"%s\"\n", kind);
        exit(1);
     }
  }
  StartKernel(NULL, NULL);
  TimedPhase = phase;
  RunTimed(duration);
  printf("T %d\n", duration);
}

/* TestEventSpacing: An event-driven task of workload 300, alone, signalled at 50, 150 and
** 400. The first signal releases it at once; the second waits in the arrival queue until
** 350, a workload after the first release, and the third until 650, a workload after the
** release from that queue. Each instance takes 10 ticks; at the fastest speed it ends 10
** after its release, and under the power-aware kernel, which may stretch it, by its
** deadline. */
static void TestEventSpacing(void)
{
  TimedTask *events = &Timed[0];
  static const INT32 release[3] = {50, 350, 650};
  unsigned i;
  char label[96];

  TimedRun = TIMED_BUSY;
  events->WCET = 10;
  events->Work = events->WCET * 256;
  events->Event = SignalEvent = OSCreateEventDescriptor();
  NbTimed = 1;
  #if defined(ESCAPEMENT_VERSION_SOFT)
     OSCreateSynchronousTask(TimedTaskCode, events->WCET, 300, 0, SignalEvent, events);
  #elif defined(ESCAPEMENT_VERSION_HARD_PA)
     OSCreateSynchronousTask(TimedTaskCode, events->WCET, 300, 9, SignalEvent, events);
  #else
     OSCreateSynchronousTask(TimedTaskCode, 300, SignalEvent, events);
  #endif
  SignalAt[0] = 50;
  SignalAt[1] = 150;
  SignalAt[2] = 400;
  NbSignals = 3;
  StartKernel(NULL, NULL);
  RunTimed(2000);

  printf("\n2000 ticks of simulated time, an event-driven task signalled sooner than its workload\n\n");
  snprintf(label, sizeof label, "  the task ran for each signal: %u of 3", NbEventEnds);
  Check(label, NbEventEnds == 3);
  for (i = 0; i < NbEventEnds && i < 3; i += 1) {
     #if defined(ESCAPEMENT_VERSION_HARD_PA)
        BOOL ok = EventEnds[i] >= release[i] + 10 && EventEnds[i] <= release[i] + 300;
     #else
        BOOL ok = EventEnds[i] == release[i] + 10;
     #endif
     snprintf(label, sizeof label, "  instance %u, released at %d, ended at %d", i, release[i],
              EventEnds[i]);
     Check(label, ok);
  }
}

/* TestSimStale: Under DRA and DR_OTE a task ending early leaves its entry in the simulation
** queue until the simulation, which runs every instance for its WCET, has used it up; and
** the simulation is brought up to date at an arrival only if the head of the ready queue
** changes. A task of 30 every 100 ends after 5, then a task of 140 every 1000, deadline
** 150, runs on: at 100 the first arrives behind it, its previous instance still in the
** simulation queue, which the insertion must take out rather than link it twice. Both
** tasks keep their deadlines under every kernel, and under DRA and DR_OTE the simulation
** queue stays whole. */
static void TestSimStale(void)
{
  char label[96];

  TimedRun = TIMED_BUSY;
  CreateTimedTask(30, 100, 100, 5, -1);
  CreateTimedTask(140, 1000, 150, 0, -1);
  StartKernel(NULL, NULL);
  RunTimed(3000);
  CheckSimQueue();

  printf("\n3000 ticks of simulated time, an arrival behind a task running on\n\n");
  snprintf(label, sizeof label, "  the short task: %u of 30 instances", Timed[0].Instance);
  Check(label, Timed[0].Instance == 30);
  snprintf(label, sizeof label, "  the long one: %u of 3", Timed[1].Instance);
  Check(label, Timed[1].Instance == 3);
  snprintf(label, sizeof label, "  no deadline missed: %u", Timed[0].Misses + Timed[1].Misses);
  Check(label, Timed[0].Misses + Timed[1].Misses == 0);
  Check("  the simulation queue stays whole", QueueBreaks == 0);
}

/* TestEventRelease: An event-driven task's next instance may be released a workload after
** the previous one was, whatever happened to that one since. The task, 100 ticks every
** 300 at least, is signalled at 151 and 451, beside a periodic task of 110 every 250 and
** higher priority under deadline-monotonic scheduling. Its first instance is preempted at
** 250 and elected again at 360. The soft kernel set the next release again at each
** election, 300 after 360: the second instance waited from 451 to 660, was preempted at
** 750, and ended at 870, 419 ticks after its signal (2026-10-04). Released at 451, it
** ends at 661 under either algorithm, its response by analysis 210. */
static void TestEventRelease(void)
{
  TimedTask *events = &Timed[1];
  unsigned i;
  char label[96];

  TimedRun = TIMED_BUSY;
  CreateTimedTask(110, 250, 250, 0, -1);
  events->WCET = 100;
  events->Work = events->WCET * 256;
  events->Event = SignalEvent = OSCreateEventDescriptor();
  NbTimed = 2;
  #if defined(ESCAPEMENT_VERSION_SOFT)
     OSCreateSynchronousTask(TimedTaskCode, events->WCET, 300, 0, SignalEvent, events);
  #elif defined(ESCAPEMENT_VERSION_HARD_PA)
     OSCreateSynchronousTask(TimedTaskCode, events->WCET, 300, 86, SignalEvent, events);
  #else
     OSCreateSynchronousTask(TimedTaskCode, 300, SignalEvent, events);
  #endif
  SignalAt[0] = 151;
  SignalAt[1] = 451;
  NbSignals = 2;
  StartKernel(NULL, NULL);
  RunTimed(2000);

  printf("\n2000 ticks of simulated time, an event-driven task signalled a workload apart\n\n");
  snprintf(label, sizeof label, "  the event-driven task ran for each signal: %u of %u",
           NbEventEnds, NbSignals);
  Check(label, NbEventEnds == NbSignals);
  for (i = 0; i < NbEventEnds && i < NbSignals; i += 1) {
     snprintf(label, sizeof label, "  instance %u ended %d after its signal, at most 300",
              i, EventEnds[i] - SignalAt[i]);
     Check(label, EventEnds[i] - SignalAt[i] <= 300);
  }
  snprintf(label, sizeof label, "  the periodic task missed no deadline: %u", Timed[0].Misses);
  Check(label, Timed[0].Misses == 0);
}

/* TestSignalZero: Under EDF, an event-driven task of WCET 200 and workload 300 signalled
** at time 0 gets the deadline 300, the server's first, which starts from 0 (mutant 0 of
** EscapementHard.c started it from 1). A periodic task of 1 every 151, deadline 150, runs
** first, then arrives again at 151 with the deadline 301: one tick behind the event-driven
** task, it must wait for its end rather than preempt it. */
static void TestSignalZero(void)
{
  TimedTask *events = &Timed[1];
  char label[96];

  #if SCHEDULER_REAL_TIME_MODE == DEADLINE_MONOTONIC_SCHEDULING
     printf("\nthe server's first deadline: under EDF only\n");
     return;
  #endif
  TimedRun = TIMED_BUSY;
  CreateTimedTask(1, 151, 150, 0, -1);
  events->WCET = 200;
  events->Work = events->WCET * 256;
  events->Event = SignalEvent = OSCreateEventDescriptor();
  NbTimed = 2;
  #if defined(ESCAPEMENT_VERSION_SOFT)
     OSCreateSynchronousTask(TimedTaskCode, events->WCET, 300, 0, SignalEvent, events);
  #elif defined(ESCAPEMENT_VERSION_HARD_PA)
     OSCreateSynchronousTask(TimedTaskCode, events->WCET, 300, 171, SignalEvent, events);
  #else
     OSCreateSynchronousTask(TimedTaskCode, 300, SignalEvent, events);
  #endif
  SignalAt[0] = 0;
  NbSignals = 1;
  StartKernel(NULL, NULL);
  RunTimed(300);

  printf("\n300 ticks of simulated time, an event-driven task signalled at time 0\n\n");
  snprintf(label, sizeof label, "  the event-driven task ran once: %u", NbEventEnds);
  Check(label, NbEventEnds == 1);
  snprintf(label, sizeof label, "  the periodic task ran twice: %u", NbEnds);
  Check(label, NbEnds == 2);
  if (NbEventEnds == 1 && NbEnds == 2) {
     snprintf(label, sizeof label, "  its instance of deadline 301 ended at %d, after the"
              " one of 300, at %d", Ends[1], EventEnds[0]);
     Check(label, Ends[1] > EventEnds[0]);
  }
}

/* TestWindowAlarm: The comparator's alarm in the few instructions between the timer
** handler arming it and its PENDSTCLR, as when the next arrival is a tick away: the clear
** drops the soft timer interrupt the alarm raised, and the handler must find the alarm in
** its flag and run again (mutant 130 of EscapementHard.c looked at the flag only with an
** event-driven task to reschedule). Missed, the arrival would wait for the next interrupt,
** the wraparound. Nor may the request stay pending once served, which would run the
** handler again for nothing (mutant 127 left the clear out). Two tasks of 5, every 100
** and every 101: at 100 the handler arms 101, and the alarm comes in the window. */
static BOOL WindowTaken;
static unsigned WindowRequest;
static void WindowAlarm(void)
{
  if (!WindowTaken && TimedNow() == 100 && HostTicksToNextEvent() == 1) {
     WindowTaken = TRUE;
     HostAdvanceBy(1);                 /* the comparator's flag rises */
     HostSoftTimerRequests += 1;       /* and its handler asks for the soft interrupt */
     WindowRequest = HostSoftTimerRequests;
  }
}

static void TestWindowAlarm(void)
{
  char label[96];

  TimedRun = TIMED_BUSY;
  CreateTimedTask(5, 100, 100, 0, -1);
  CreateTimedTask(5, 101, 101, 0, -1);
  StartKernel(NULL, NULL);
  HostSoftTimerClearHook = WindowAlarm;
  RunTimed(300);
  HostSoftTimerClearHook = NULL;

  printf("\n300 ticks of simulated time, the comparator's alarm before the handler's clear\n\n");
  Check("  the alarm came in the window", WindowTaken);
  snprintf(label, sizeof label, "  every instance ran: %u + %u of 3 + 3", Timed[0].Instance,
           Timed[1].Instance);
  Check(label, Timed[0].Instance == 3 && Timed[1].Instance == 3);
  snprintf(label, sizeof label, "  no deadline missed: %u + %u", Timed[0].Misses, Timed[1].Misses);
  Check(label, Timed[0].Misses == 0 && Timed[1].Misses == 0);
  Check("  the request the alarm raised cleared by the handler that served it",
        WindowTaken && (int)(HostSoftTimerCleared - WindowRequest) >= 0);
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

/* TestFirmLong: The schedulability test of an optional instance counts work in ticks,
** and under EDF scales it by 256 for the bandwidth of the event-driven tasks: with a
** WCET of 2^23 ticks, eight seconds at 1 MHz, and events whose deadlines run 2^25 ticks
** ahead, the products left 32 bits. A (1,2)-firm task of period 2^25 is tested at its
** second instance, while a task of period 2^24 keeps signalling an event-driven one. */
static void TestFirmLong(void)
{
  long long duration = 4LL << 25;
  void *event = OSCreateEventDescriptor();

  ToLull.Event = event;
  CreateFirmTask(&Firm[0], 1 << 23, 1 << 25, 1, 2);
  CREATE_TASK(SignalerTask, 1 << 24, &ToLull);
  CREATE_SYNCHRONOUS_TASK(SignaledTask, 1 << 25, event, NULL);

  StartKernel(NULL, NULL);
  RunAcross(duration);

  printf("\n%lld ticks of simulated time, an optional instance of 2^23 ticks\n\n", duration);
  Check("  every instance fits: all of them ran", WithinOne(Firm[0].Runs, 4));
  Check("  no deadline missed", LateArrivals == 0);
}

/* TestFirmEvents: (m,k)-firm tasks alongside an event-driven task, which the test of
** optional instances must count: under deadline-monotonic scheduling by its instances
** of higher priority, under EDF by the bandwidth set aside for it. The event-driven task
** gives no workload, so the kernel computes it from its WCET and that bandwidth. Each
** task also reads where it stands in its pattern with OSGetTaskInstance. */
static unsigned InstanceMismatches;
static void FirmInstanceTask(void *argument)
{
  FirmTask *task = (FirmTask *)argument;
  if (OSGetTaskInstance() != (HostClockNow() / task->Period) % task->K)
     InstanceMismatches += 1;
  FirmTaskCode(argument);
}

static void TestFirmEvents(void)
{
  INT32 duration = 30000;
  unsigned a = duration / 100, b = duration / 150;
  char label[80];

  ToSignaled.Event = OSCreateEventDescriptor();
  Firm[0].Period = 100; Firm[0].M = 1; Firm[0].K = 3;
  OSCreateTask(FirmInstanceTask, 40, 0, 100, 100, 1, 3, 0, &Firm[0]);
  Firm[1].Period = 150; Firm[1].M = 2; Firm[1].K = 3;
  OSCreateTask(FirmInstanceTask, 40, 0, 150, 150, 2, 3, 0, &Firm[1]);
  CREATE_TASK(SignalerTask, 300, &ToSignaled);
  #if BY_DEADLINE
     Check("  a WCET above its workload: refused",
           !OSCreateSynchronousTask(SignaledTask, 130, 120, 64, ToSignaled.Event, NULL));
     Check("  an event-driven task given a WCET and a bandwidth, no workload",
           OSCreateSynchronousTask(SignaledTask, 30, 0, 64, ToSignaled.Event, NULL));
  #else
     /* Deadline-monotonic scheduling has no bandwidth to compute a workload from. */
     Check("  no workload under deadline-monotonic scheduling: refused",
           !OSCreateSynchronousTask(SignaledTask, 30, 0, 64, ToSignaled.Event, NULL));
     OSCreateSynchronousTask(SignaledTask, 30, 120, 64, ToSignaled.Event, NULL);
  #endif

  StartKernel(NULL, NULL);
  RunFor(duration);

  printf("\n%d ticks of simulated time, (m,k)-firm tasks and an event-driven task\n\n",
         duration);
  snprintf(label, sizeof label, "  (1,3) task: %u of %u instances, 1 in every 3 at least", Firm[0].Runs, a);
  Check(label, FirmHolds(&Firm[0], a));
  snprintf(label, sizeof label, "  (2,3) task: %u of %u instances, 2 in every 3 at least", Firm[1].Runs, b);
  Check(label, FirmHolds(&Firm[1], b));
  snprintf(label, sizeof label, "  one wake-up per signal: %u for %u", SignaledRuns, ToSignaled.Runs);
  Check(label, WithinOne(SignaledRuns, ToSignaled.Runs));
  Check("  each task knows its place in its pattern", InstanceMismatches == 0);
  Check("  no deadline missed", LateArrivals == 0);
}

/* TestFirmWrap: Under EDF the soft kernel keeps optional instances in the ready queue
** after its tail sentinel, and their deadlines must follow the clock at the 2^30 wrap as
** the others do. Two tasks of period 2^29 - 100 arrive together 200 ticks short of each
** of the first wraparounds, and RunAcross calls them only after it: the hard one first,
** while the other waits behind the sentinel at instances 2 and 4 of its (1,3) pattern. */
#define FIRM_WRAP_PERIOD 536870812
/* TestFirmWait: Tasks taking time, the mandatory instances keeping the processor busy
** across a whole period of an optional one: the optional instance, never at the head of
** the ready queue, is still there when its task arrives again, and the kernel must take
** it out before it inserts the next. */
static BOOL Mandatory(unsigned j, unsigned m, unsigned k)
{
  j %= k;
  return j == (j * m + k - 1) / k * k / m;
}

static void TestFirmWait(void)
{
  INT32 duration = 24000;
  unsigned j, missing = 0, optional = 0, twice = 0, misses;
  char label[96];
  TimedTask *first = &Timed[0], *second = &Timed[1];

  TimedRun = TIMED_FIRMWAIT;
  /* The first task runs 1800 ticks of each 2000, the second 100 of each 1000 with
  ** instances 0 and 1 of each 3 mandatory: 96.7 % of the processor for the mandatory
  ** instances. From 2000 the first task keeps the processor to 3800 or later, under EDF
  ** as under deadline-monotonic scheduling, and the optional instance 2 of the second
  ** never reaches the head before 3000. */
  first->Period = first->Deadline = 2000;
  first->WCET = 1800;
  second->WCET = 100;
  second->Period = second->Deadline = 1000;
  NbTimed = 2;
  first->Work = first->WCET * 256;
  second->Work = second->WCET * 256;
  OSCreateTask(TimedTaskCode, first->WCET, 0, first->Period, first->Deadline, 1, 1, 0, first);
  OSCreateTask(TimedTaskCode, second->WCET, 0, second->Period, second->Deadline, 2, 3, 0, second);
  StartKernel(NULL, NULL);
  RunTimed(duration);

  printf("\n%d ticks of simulated time, an optional instance waiting past its period\n\n",
         duration);
  for (j = 0; j < (unsigned)(duration / second->Period); j += 1) {
     if (second->Ran[j] > 1)
        twice += 1;
     if (Mandatory(j, 2, 3) && second->Ran[j] == 0)
        missing += 1;
     if (!Mandatory(j, 2, 3) && second->Ran[j] == 0)
        optional += 1;
  }
  misses = first->Misses + second->Misses;
  snprintf(label, sizeof label, "  the first task: %u of %d instances", first->Instance,
           duration / first->Period);
  Check(label, first->Instance == (unsigned)(duration / first->Period));
  snprintf(label, sizeof label, "  the second: every mandatory instance ran, %u missing", missing);
  Check(label, missing == 0);
  snprintf(label, sizeof label, "  optional instances found still ready at their arrival: %u",
           StillReady);
  Check(label, StillReady > 0 && optional >= StillReady);
  Check("  no instance ran twice", twice == 0);
  Check("  the ready queue stays whole", QueueBreaks == 0);
  snprintf(label, sizeof label, "  no deadline missed: %u", misses);
  Check(label, misses == 0);
}

/* TestFirmOnce: The kernel tests an optional instance once, when it first reaches the
** head, and drops it for good if the test fails. An (m,k)-firm task of WCET 35 and period
** 100, (1,2), its mandatory instance 0 taking 1 tick, and a periodic task of WCET 50,
** period 60, taking 1. At 100 the optional instance 1 fails the test: its WCET and the
** periodic task's instances released at 120 and 180, counted up to 200, end past it
** (100 + 35 + 50 + 20 = 205). The periodic task then ends at 121 in 1 tick, and from there
** the instance would pass (121 + 35 + 20 = 176) and end by 156 at worst; it is not tested
** again. What this pins is what the kernel does, not what it should: tools/differential.py
** counts such instances (docs/method.md), and testing them again would change the kernel. */
static void TestFirmOnce(void)
{
  INT32 duration = 300;
  char label[96];
  TimedTask *firm = &Timed[0], *periodic = &Timed[1];

  firm->WCET = firm->Takes = 35;
  firm->Period = firm->Deadline = 100;
  firm->Each = TRUE;
  EachTakes[0][0] = 1;
  periodic->WCET = 50;
  periodic->Takes = 1;
  periodic->Period = periodic->Deadline = 60;
  periodic->Only = -1;
  periodic->Work = NextWork(periodic);
  NbTimed = 2;
  OSCreateTask(TimedTaskCode, firm->WCET, 0, firm->Period, firm->Deadline, 1, 2, 0, firm);
  OSCreateTask(TimedTaskCode, periodic->WCET, 0, periodic->Period, periodic->Deadline,
               1, 1, 0, periodic);
  StartKernel(NULL, NULL);
  RunTimed(duration);

  printf("\n%d ticks of simulated time, an optional instance tested once\n\n", duration);
  snprintf(label, sizeof label, "  mandatory instances 0 and 2 ran: %u, %u", firm->Ran[0],
           firm->Ran[2]);
  Check(label, firm->Ran[0] == 1 && firm->Ran[2] == 1);
  snprintf(label, sizeof label, "  optional instance 1, dropped at 100, never ran: %u",
           firm->Ran[1]);
  Check(label, firm->Ran[1] == 0);
  snprintf(label, sizeof label, "  the periodic task: %u of %d instances", periodic->Instance,
           duration / periodic->Period);
  Check(label, periodic->Instance == (unsigned)(duration / periodic->Period));
  snprintf(label, sizeof label, "  no deadline missed: %u", firm->Misses + periodic->Misses);
  Check(label, firm->Misses + periodic->Misses == 0);
}

/* TestFirmEventWait: An optional instance started, then delayed by an event-driven task
** whose load the schedulability test must count. Under EDF it declares its WCET and
** workload but no bandwidth: 700 ticks due within 750 of its signal, 94 % of the
** processor while it runs. */
static void TestFirmEventWait(void)
{
  INT32 duration = 8000;
  TimedTask *optional = &Timed[0], *events = &Timed[1];
  char label[96];

  TimedRun = TIMED_FIRMWAIT;
  optional->WCET = 400;
  optional->Period = optional->Deadline = 1000;
  optional->Work = optional->WCET * 256;
  events->WCET = 700;
  events->Work = events->WCET * 256;
  events->Event = SignalEvent = OSCreateEventDescriptor();
  NbTimed = 2;
  OSCreateTask(TimedTaskCode, optional->WCET, 0, optional->Period, optional->Deadline, 1, 2,
               0, optional);
  Check("  an event-driven task given a WCET and a workload, no bandwidth",
        OSCreateSynchronousTask(TimedTaskCode, events->WCET, 750, 0, SignalEvent, events));
  /* After the start of an optional instance, the odd ones, at 1000 and 3000. */
  SignalAt[0] = 1100;
  SignalAt[1] = 3100;
  NbSignals = 2;
  StartKernel(NULL, NULL);
  RunTimed(duration);

  printf("\n%d ticks of simulated time, an optional instance and an event\n\n", duration);
  snprintf(label, sizeof label, "  the event-driven task ran for each signal: %u of %u",
           events->Instance, NbSignals);
  Check(label, events->Instance == NbSignals);
  snprintf(label, sizeof label, "  no deadline missed: %u", optional->Misses);
  Check(label, optional->Misses == 0);
}

/* TestFirmEventQueued: An event-driven task signalled again while it runs waits in the
** arrival queue until it may be released, a workload after its release under DM, its
** deadline under EDF. The test of an optional instance walks that queue for the periodic
** tasks released before its deadline, and must step over the event-driven task, whose
** smaller control block has none of their fields; it counts it apart. The task, 100 ticks
** within 300, is signalled at 950, ends at 1050, and is signalled again at 1060: it waits
** in the arrival queue until 1250, and the optional instance of 1100-2200 is tested at
** its arrival with the task there. */
static void TestFirmEventQueued(void)
{
  INT32 duration = 4000;
  TimedTask *optional = &Timed[0], *events = &Timed[1];
  char label[96];

  TimedRun = TIMED_FIRMWAIT;
  optional->WCET = 400;
  optional->Period = optional->Deadline = 1100;
  optional->Work = optional->WCET * 256;
  events->WCET = 100;
  events->Work = events->WCET * 256;
  events->Event = SignalEvent = OSCreateEventDescriptor();
  NbTimed = 2;
  OSCreateTask(TimedTaskCode, optional->WCET, 0, optional->Period, optional->Deadline, 1, 2,
               0, optional);
  OSCreateSynchronousTask(TimedTaskCode, events->WCET, 300, 0, SignalEvent, events);
  SignalAt[0] = 950;
  SignalAt[1] = 1060;
  NbSignals = 2;
  StartKernel(NULL, NULL);
  RunTimed(duration);

  printf("\n%d ticks of simulated time, an optional instance tested with an event queued\n\n",
         duration);
  snprintf(label, sizeof label, "  the event-driven task ran for each signal: %u of %u",
           NbEventEnds, NbSignals);
  Check(label, NbEventEnds == NbSignals);
  snprintf(label, sizeof label, "  its second instance waited for 1250: ended at %d",
           NbEventEnds > 1 ? EventEnds[1] : 0);
  Check(label, NbEventEnds > 1 && EventEnds[1] >= 1350);
  snprintf(label, sizeof label, "  optional instance 1 (1100-2200) ran: %u", optional->Ran[1]);
  Check(label, optional->Ran[1] == 1);
  snprintf(label, sizeof label, "  no deadline missed: %u", optional->Misses);
  Check(label, optional->Misses == 0);
}

#if defined(ESCAPEMENT_VERSION_SOFT) && !BY_DEADLINE
/* TestFirmEventAhead: Under deadline-monotonic scheduling, an optional instance tested
** while an event-driven task of higher priority cannot be released again before its
** minimal inter-arrival time: its interference counts from that release, not from now, and
** the instances that do not fit whole count their WCET only. The event-driven task, WCET
** 100 every 300 at least, is signalled at 950 and ends at 1050, where the optional instance
** of 1000-2000, WCET 640, is tested. Releases at 1250, 1550 and 1850 take 2 * 100 + 100
** of the 750 ticks to 2000: 640 + 300 from 1050 ends at 1990, and the instance fits. Counted
** from 1050, or with the last 150 ticks counted whole, they take 350, and it is dropped.
** The signals then come at those releases, the worst case, and the instance ends at 1990. */
static void TestFirmEventAhead(void)
{
  INT32 duration = 4000;
  TimedTask *optional = &Timed[0], *events = &Timed[1];
  unsigned i, end = 0;
  char label[96];

  TimedRun = TIMED_FIRMWAIT;
  optional->WCET = 640;
  optional->Period = optional->Deadline = 1000;
  optional->Work = optional->WCET * 256;
  events->WCET = 100;
  events->Work = events->WCET * 256;
  events->Event = SignalEvent = OSCreateEventDescriptor();
  NbTimed = 2;
  OSCreateTask(TimedTaskCode, optional->WCET, 0, optional->Period, optional->Deadline, 1, 2,
               0, optional);
  OSCreateSynchronousTask(TimedTaskCode, events->WCET, 300, 0, SignalEvent, events);
  SignalAt[0] = 950;
  SignalAt[1] = 1250;
  SignalAt[2] = 1550;
  SignalAt[3] = 1850;
  NbSignals = 4;
  StartKernel(NULL, NULL);
  RunTimed(duration);

  for (i = 0; i < NbEnds; i += 1)
     if (Ends[i] > 1000 && Ends[i] <= 2000)
        end = (unsigned)Ends[i];
  printf("\n%d ticks of simulated time, an optional instance and an event that cannot come"
         " yet\n\n", duration);
  snprintf(label, sizeof label, "  the event-driven task ran for each signal: %u of %u",
           events->Instance, NbSignals);
  Check(label, events->Instance == NbSignals);
  snprintf(label, sizeof label, "  optional instance 1 (1000-2000) ran: %u", optional->Ran[1]);
  Check(label, optional->Ran[1] == 1);
  snprintf(label, sizeof label, "  and ended at 1990: %u", end);
  Check(label, end == 1990);
  snprintf(label, sizeof label, "  no deadline missed: %u", optional->Misses);
  Check(label, optional->Misses == 0);
}
#endif

#if defined(ESCAPEMENT_VERSION_SOFT) && BY_DEADLINE
/* TestFirmDiscount: An optional instance that cannot fit beside the mandatory instances
** released before its deadline, while an event-driven task, signalled earlier, holds a
** server deadline far beyond that of the optional instance. The event cannot interfere
** (its deadline is later), so the answer must come from the mandatory work alone: 400 of
** the optional instance plus 300 of the periodic task at 1500, 700 ticks in the 690 left
** at 1310: the instance must be dropped. */
static void TestFirmDiscount(INT32 workload, INT32 duration)
{
  TimedTask *periodic = &Timed[0], *optional = &Timed[1], *events = &Timed[2];
  unsigned j, twice = 0, missingMandatory = 0;
  char label[96];

  TimedRun = TIMED_FIRMWAIT;
  periodic->WCET = 300; periodic->Period = periodic->Deadline = 500;
  periodic->Takes = 300; periodic->Only = -1;
  periodic->Work = periodic->WCET * 256;
  optional->WCET = 400; optional->Period = optional->Deadline = 1000;
  optional->Takes = 400; optional->Only = -1;
  optional->Work = optional->WCET * 256;
  events->WCET = 10; events->Takes = 10; events->Only = -1;
  events->Work = events->WCET * 256;
  events->Event = SignalEvent = OSCreateEventDescriptor();
  NbTimed = 3;
  OSCreateTask(TimedTaskCode, periodic->WCET, 0, periodic->Period, periodic->Deadline, 1, 1, 0, periodic);
  OSCreateTask(TimedTaskCode, optional->WCET, 0, optional->Period, optional->Deadline, 1, 2, 0, optional);
  Check("  an event-driven task with a bandwidth of 128/256 and a long workload",
        OSCreateSynchronousTask(TimedTaskCode, events->WCET, workload, 128, SignalEvent, events));
  SignalAt[0] = 1000;
  NbSignals = 1;
  StartKernel(NULL, NULL);
  RunTimed(duration);

  printf("\n%d ticks of simulated time, an optional instance that cannot fit, an event due in %d\n\n",
         duration, workload);
  for (j = 0; j < (unsigned)(duration / optional->Period); j += 1) {
     if (optional->Ran[j] > 1)
        twice += 1;
     if (Mandatory(j, 1, 2) && optional->Ran[j] == 0)
        missingMandatory += 1;
  }
  snprintf(label, sizeof label, "  the periodic task: %u of %d instances", periodic->Instance,
           duration / periodic->Period);
  Check(label, periodic->Instance == (unsigned)(duration / periodic->Period));
  snprintf(label, sizeof label, "  the event-driven task ran once: %u", events->Instance);
  Check(label, events->Instance == 1);
  snprintf(label, sizeof label, "  every mandatory instance of the (1,2) task ran, %u missing", missingMandatory);
  Check(label, missingMandatory == 0);
  snprintf(label, sizeof label, "  optional instance 1 (1000-2000) not started: ran %u", optional->Ran[1]);
  Check(label, optional->Ran[1] == 0);
  Check("  no instance ran twice", twice == 0);
  Check("  the ready queue stays whole", QueueBreaks == 0);
  snprintf(label, sizeof label, "  no deadline missed: %u", periodic->Misses + optional->Misses);
  Check(label, periodic->Misses + optional->Misses == 0);
}
#endif

/* TestFirmWrapMandatory: An optional instance tested just before the wrap, its deadline
** beyond it, while a periodic task's next mandatory instance arrives beyond the wrap too:
** its arrival is then 2^30 plus its low part, which the test must take off before it
** compares it with the deadline. A (1,3)-firm task of period 2^29 - 500 and WCET 2^29 -
** 2000 has its optional instance 2 at 2^30 - 1000; a task of 1000 every 2^29 + 250 arrives
** at 2^30 + 500, once in the window. The instance fits by 499 ticks; counted 2^30 too
** early, that task's instances would take it over and drop it. */
#if BY_DEADLINE
static void TestFirmWrapEnd(void);
#endif
static void TestFirmWrapMandatory(void)
{
  long long duration = 0x40000000LL + 1000000;

  CreateFirmTask(&Firm[0], (1 << 29) - 2000, (1 << 29) - 500, 1, 3);
  CreateFirmTask(&Firm[1], 1000, (1 << 29) + 250, 1, 1);

  StartKernel(NULL, NULL);
  RunAcross(duration);

  printf("\n%lld ticks of simulated time, a mandatory arrival beyond the wrap\n\n", duration);
  Check("  the kernel clock wrapped", HostClockWraps == 1);
  /* Counted in all: after the wrap the instances are numbered from it. */
  Check("  optional instance 2 of the (1,3) task, at 2^30 - 1000, ran: 3 runs",
        Firm[0].Runs == 3);
  Check("  the other task: every instance ran", WithinOne(Firm[1].Runs, 3));
  Check("  no deadline missed", LateArrivals == 0);
}

#if BY_DEADLINE
/* TestFirmWrapEnd: A set of tools/differential.py (soft_edf, seed 2, set 785, its tasks
** taking less than their WCET) whose third task ends at the very tick of the wraparound,
** before the timer's handler has shifted the kernel's times. Reading a time already
** wrapped, the end saw every deadline some 2^30 ahead, admitted an optional instance that
** could not end in time, and the scheduler never returned (2026-10-06). Run as the trace
** of differential.py, in a child process, its input on the standard input. */
static void TestFirmWrapEnd(void)
{
  static const char input[] = "F 10 57 57 7 1 3\nF 6 33 33 4 1 1\nF 2 43 43 2 3 5\n"
                              "F 51 202 202 38 3 3\nF 76 177 177 50 2 4\n";
  int fds[2], status = 0;
  pid_t child;
  printf("\nAn instance ending at the tick of the wraparound\n\n");
  if (pipe(fds) != 0 || (child = fork()) < 0) {
     Check("  fork", FALSE);
     return;
  }
  if (child == 0) {
     FILE *quiet = freopen("/dev/null", "w", stdout);
     (void)quiet;
     close(fds[1]);
     dup2(fds[0], 0);
     TestTrace(6000, 0x40000000 - 5503);
     ChildExit(Failures == 0 ? 0 : 1);
  }
  close(fds[0]);
  Check("  the input written", write(fds[1], input, sizeof input - 1) == (ssize_t)(sizeof input - 1));
  close(fds[1]);
  waitpid(child, &status, 0);
  Check("  the run ended, every check held", WIFEXITED(status) && WEXITSTATUS(status) == 0);
}
#endif

/* TestFirmOverload: The test of an optional instance sums in 32 bits the work released
** before its deadline, and gives up once the sum passes 2^30: the instance cannot fit, and
** summing on would overflow. A (1,2)-firm task of period 2^29 is tested at its optional
** instance 1, while tasks of WCET up to 2^29 arrive 10, 20 and 30 ticks later. With three,
** the second takes the sum of their work past 2^30, and the third, added with the time,
** would take it past 2^31. Under EDF the event-driven tasks then take their share of
** what is left: at 200 / 256 that share alone, counted in 56ths, would pass 2^31; at
** 128 / 256, from 7/8 of 2^30, it passes 2^30, and the sum with the time 2^31. Each guard
** taken out shows in UndefinedBehaviorSanitizer. */
static void TestFirmOverload(int share)
{
  long long duration = 0x40000000LL + 1000000;
  unsigned tasks = share == 0 ? 3 : share == 200 ? 1 : 2;
  INT32 last = share == 128 ? 402653184 : 1 << 29;   /* 3/8 of 2^30 */
  char label[96];
  unsigned i;
  BOOL ran = TRUE;

  CreateFirmTask(&Firm[0], 100, 1 << 29, 1, 2);
  for (i = 1; i <= tasks; i += 1)
     CreateFirmTask(&Firm[i], i == tasks ? last : 1 << 29, (1 << 29) + 10 * (INT32)i, 1, 1);
  if (share != 0)
     OSCreateSynchronousTask(EventTask, 1, 1000, (UINT8)share, OSCreateEventDescriptor(), NULL);

  StartKernel(NULL, NULL);
  RunAcross(duration);

  printf("\n%lld ticks of simulated time, more than 2^30 ticks of work declared\n\n", duration);
  Check("  optional instance 1 dropped: the (1,2) task ran instances 0 and 2",
        Firm[0].Runs == 2);
  for (i = 1; i <= tasks; i += 1)
     ran = ran && Firm[i].Runs == 3;
  snprintf(label, sizeof label, "  every mandatory instance of the other %u ran", tasks);
  Check(label, ran);
}

/* TestFirmOverloadInstances: The same test, a task whose whole instances fall before the
** deadline: their work, instances times WCET, is checked against what is left of 2^30
** before it is added. The (1,2)-firm task declares 2^28, a task arriving 10 ticks after
** its optional instance almost 2^29, and a third task of 2^27 - 1000 every 2^27 - 1000
** has three whole instances before the deadline, where what is left of 2^30 holds two.
** Its period puts no arrival on the wrap, where RunAcross would hold the instance before.
** The sum stays below 2^31 even unchecked, and the check after it would drop the
** instance as well: that check alone does not show in any run. */
static void TestFirmOverloadInstances(void)
{
  long long duration = 0x40000000LL + 1000000;
  char label[96];

  CreateFirmTask(&Firm[0], 1 << 28, 1 << 29, 1, 2);
  CreateFirmTask(&Firm[1], 1 << 29, (1 << 29) + 10, 1, 1);
  CreateFirmTask(&Firm[2], (1 << 27) - 1000, (1 << 27) - 1000, 1, 1);

  StartKernel(NULL, NULL);
  RunAcross(duration);

  printf("\n%lld ticks of simulated time, whole instances past what 2^30 holds\n\n", duration);
  Check("  optional instance 1 dropped: the (1,2) task ran instances 0 and 2",
        Firm[0].Runs == 2);
  snprintf(label, sizeof label, "  every mandatory instance ran: %u of 3 and %u of 9",
           Firm[1].Runs, Firm[2].Runs);
  Check(label, Firm[1].Runs == 3 && Firm[2].Runs == 9);
}

static void TestFirmWrap(void)
{
  long long duration = 3LL * 0x40000000 + 1000000;
  unsigned expected = (unsigned)(duration / FIRM_WRAP_PERIOD);

  CreateFirmTask(&Firm[0], 1000, FIRM_WRAP_PERIOD, 1, 3);
  CreateFirmTask(&Firm[1], 1000, FIRM_WRAP_PERIOD, 1, 1);

  StartKernel(NULL, NULL);
  RunAcross(duration);

  printf("\n%lld ticks of simulated time, (m,k)-firm tasks across the wraparound\n\n", duration);
  Check("  the kernel clock wrapped three times", HostClockWraps == 3);
  Check("  every instance fits: all of them ran",
        WithinOne(Firm[0].Runs, expected) && WithinOne(Firm[1].Runs, expected));
  Check("  no deadline missed", LateArrivals == 0);
  Check("  every deadline shifted with the clock", DeadlinesOutOfReach == 0);
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
  else if (argc > 1 && strcmp(argv[1], "wrapinside") == 0)
     TestWrapInside();
  else if (argc > 1 && strcmp(argv[1], "priority") == 0)
     TestPriority();
  else if (argc > 1 && strcmp(argv[1], "create") == 0)
     TestCreate();
  else if (argc > 1 && strcmp(argv[1], "suspend") == 0)
     TestSuspend();
  else if (argc > 1 && strcmp(argv[1], "signalinside") == 0)
     TestSignalInside();
  else if (argc > 1 && strcmp(argv[1], "wrapsim") == 0)
     TestWrapSim();
  else if (argc > 1 && strcmp(argv[1], "wrapevents") == 0)
     TestWrapEvents();
  else if (argc > 1 && strcmp(argv[1], "events") == 0)
     TestEvents();
  else if (argc > 1 && strcmp(argv[1], "eventturns") == 0) {
     HostMallocFill = HOST_FILL_COUNT;
     TestEventTurns();
  }
  else if (argc > 1 && strcmp(argv[1], "lull") == 0)
     TestLull();
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
  else if (argc > 1 && strcmp(argv[1], "reuse") == 0)
     TestTimed(TIMED_REUSE);
  else if (argc > 1 && strcmp(argv[1], "overrun") == 0)
     TestTimed(TIMED_OVERRUN);
  else if (argc > 1 && strcmp(argv[1], "minspeed") == 0)
     TestTimed(TIMED_MINSPEED);
  else if (argc > 1 && strcmp(argv[1], "endinside") == 0)
     TestEndInside(TIMED_EARLY);
  else if (argc > 1 && strcmp(argv[1], "endinsidebusy") == 0)
     TestEndInside(TIMED_BUSY);
  else if (argc > 1 && strcmp(argv[1], "timewrap") == 0)
     TestTimeWrap(TIMED_EARLY);
  else if (argc > 1 && strcmp(argv[1], "timewrapbusy") == 0)
     TestTimeWrap(TIMED_BUSY);
  else if (argc > 1 && strcmp(argv[1], "signals") == 0)
     TestSignals();
  else if (argc > 1 && strcmp(argv[1], "notask") == 0)
     TestNoTask();
  else if (argc > 2 && strcmp(argv[1], "trace") == 0)
     TestTrace((INT32)strtol(argv[2], NULL, 10),
               argc > 3 ? (INT32)strtol(argv[3], NULL, 10) : 0);
  else if (argc > 1 && strcmp(argv[1], "createbounds") == 0)
     TestCreateBounds();
  else if (argc > 1 && strcmp(argv[1], "longperiod") == 0)
     TestLongPeriod();
  else if (argc > 1 && strcmp(argv[1], "twosignals") == 0)
     TestTwoSignals();
  else if (argc > 1 && strcmp(argv[1], "eventspacing") == 0)
     TestEventSpacing();
  else if (argc > 1 && strcmp(argv[1], "simstale") == 0)
     TestSimStale();
  else if (argc > 1 && strcmp(argv[1], "eventrelease") == 0)
     TestEventRelease();
  else if (argc > 1 && strcmp(argv[1], "signalzero") == 0)
     TestSignalZero();
  else if (argc > 1 && strcmp(argv[1], "windowalarm") == 0)
     TestWindowAlarm();
  else if (argc > 1 && strcmp(argv[1], "timewrapidle") == 0)
     TestTimeWrap(TIMED_IDLE);
  #if defined(ESCAPEMENT_VERSION_SOFT)
     else if (argc > 1 && strcmp(argv[1], "firm") == 0)
        TestFirm();
     else if (argc > 1 && strcmp(argv[1], "firmwrap") == 0)
        TestFirmWrap();
     else if (argc > 1 && strcmp(argv[1], "firmoverload") == 0)
        TestFirmOverload(0);
     else if (argc > 1 && strcmp(argv[1], "firmoverloadinstances") == 0)
        TestFirmOverloadInstances();
     #if BY_DEADLINE
        else if (argc > 1 && strcmp(argv[1], "firmoverloadshare") == 0)
           TestFirmOverload(200);
        else if (argc > 1 && strcmp(argv[1], "firmoverloadsum") == 0)
           TestFirmOverload(128);
     #endif
     else if (argc > 1 && strcmp(argv[1], "firmwrapmandatory") == 0)
        TestFirmWrapMandatory();
     #if BY_DEADLINE
        else if (argc > 1 && strcmp(argv[1], "firmwrapend") == 0)
           TestFirmWrapEnd();
     #endif
     else if (argc > 1 && strcmp(argv[1], "firmlong") == 0)
        TestFirmLong();
     else if (argc > 1 && strcmp(argv[1], "firmevents") == 0)
        TestFirmEvents();
     else if (argc > 1 && strcmp(argv[1], "firmwait") == 0)
        TestFirmWait();
     else if (argc > 1 && strcmp(argv[1], "firmonce") == 0)
        TestFirmOnce();
     else if (argc > 1 && strcmp(argv[1], "firmeventwait") == 0)
        TestFirmEventWait();
     else if (argc > 1 && strcmp(argv[1], "firmeventqueued") == 0)
        TestFirmEventQueued();
     #if !BY_DEADLINE
        else if (argc > 1 && strcmp(argv[1], "firmeventahead") == 0)
           TestFirmEventAhead();
     #endif
     #if BY_DEADLINE
        else if (argc > 1 && strcmp(argv[1], "firmdiscount") == 0)
           TestFirmDiscount(20000, 6000);
        else if (argc > 1 && strcmp(argv[1], "firmdiscountnear") == 0)
           TestFirmDiscount(1500, 6000);
     #endif
  #endif
  else
     TestTaskSet();

  if (EndBreaks != 0 || HandlerBreaks != 0) {
     char label[96];
     snprintf(label, sizeof label, "  ends without a switch or a context left unsaved: %u, "
              "handlers: %u", EndBreaks, HandlerBreaks);
     Check(label, FALSE);
  }
  printf("\n%s\n", Failures ? "FAILURES" : "all checks passed");
  return Failures ? 1 : 0;
}
