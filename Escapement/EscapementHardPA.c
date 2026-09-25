/* Copyright (c) 2006-2012 MIS Institute of the HEIG-VD affiliated to the University of
** Applied Sciences of Western Switzerland. All rights reserved.
** Permission to use, copy, modify, and distribute this software and its documentation
** for any purpose, without fee, and without written agreement is hereby granted, pro-
** vided that the above copyright notice, the following three sentences and the authors
** appear in all copies of this software and in the software where it is used.
** IN NO EVENT SHALL THE MIS INSTITUTE NOR THE HEIG-VD NOR THE UNIVERSITY OF APPLIED
** SCIENCES OF WESTERN SWITZERLAND BE LIABLE TO ANY PARTY FOR DIRECT, INDIRECT, SPECIAL,
** INCIDENTAL, OR CONSEQUENTIAL DAMAGES ARISING OUT OF THE USE OF THIS SOFTWARE AND ITS
** DOCUMENTATION, EVEN IF THE MIS INSTITUTE OR THE HEIG-VD OR THE UNIVERSITY OF APPLIED
** SCIENCES OF WESTERN SWITZERLAND HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
** THE MIS INSTITUTE, THE HEIG-VD AND THE UNIVERSITY OF APPLIED SCIENCES OF WESTERN SWIT-
** ZERLAND SPECIFICALLY DISCLAIMS ANY WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
** IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE. THE SOFT-
** WARE PROVIDED HEREUNDER IS ON AN "AS IS" BASIS, AND THE MIS INSTITUTE NOR THE HEIG-VD
** AND NOR THE UNIVERSITY OF APPLIED SCIENCES OF WESTERN SWITZERLAND HAVE NO OBLIGATION
** TO PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR MODIFICATIONS.
** Authors: MIS-TIC
**
** Escapement - Lightweight Power-Aware Real-Time OS, derived from ZottaOS.
** Modifications Copyright (c) 2026 Bertrand Hurst, distributed under the same terms;
** see LICENSE and NOTICE at the root of this repository.
*/
/* File EscapementHardPA.c: Generic kernel hard real-time implementation with power-aware
**                       schemes.
** Version date: July 2012
*/
#include "Escapement.h"           /* Insert the user API with the specific kernel */
#include "Escapement_Processor.h" /* Architecture dependent defines */
#include "Escapement_Timer.h"     /* Timer HAL definitions */

#ifdef ESCAPEMENT_VERSION_HARD_PA


/* Table of speed slowdowns * 256 for all frequency settings except for the maximum one.
** These values are computed using the relation Speed/Max_Speed = A/256, where A is the
** number in the table. */
#if POWER_MANAGEMENT != NONE
   extern const UINT8 _OSSlowdownRatios[];
   /* ScaleBy256ths: A time times a ratio over 256, rounded down, as
   ** ((time) * (ratio)) >> 8 but without its overflow: multiplied whole, the product
   ** leaves 32 bits past 2^31 / ratio ticks, 21 s at 1 us for a ratio of 102. Split
   ** into the time's upper bits and its last eight, it cannot. */
   #define ScaleBy256ths(time,ratio) \
      (((time) >> 8) * (ratio) + ((((time) & 0xFF) * (ratio)) >> 8))
   /* The work done in a time at a speed below the fastest. */
   #define Slowdown(time,speed) ScaleBy256ths(time,_OSSlowdownRatios[speed])
#endif


/* TASK STATE BIT DEFINITION
**            STATE_INIT   <-  STATE_TERMINATED
**                 |                 ^
**                 V                 |
**            STATE_RUNNING  -> STATE_ZOMBIE
** In state INIT, a task is in the ready queue but hasn't yet started its execution.
** In the RUNNING state, the task is also in the ready queue and has begun its execution.
** The difference between these two states lies with the context switch. In the former
** state there are no registers to restore. This is not the case in the RUNNING state.
** When a task terminates its execution, it becomes a ZOMBIE and it needs to stay in the
** ready queue until it finishes its preparation because otherwise and when a timer in-
** terruption occurs, the task will never have the opportunity to finish. In ZOMBIE state,
** the task cannot begin its next period until it is removed from the ready queue. To
** circumvent this problem, a timer interrupt asserts that an active zombie task is no
** longer in the ready queue. A task with state ZOMBIE and no longer in the ready queue
** is considered to be in state TERMINATED, which is a fictitious state. */
/* STATE_INIT must be 0 and STATE_RUNNING bit 0: Escapement_CortexMx_a.S tests and sets
** them by value. */
#define STATE_INIT          0x00
#define STATE_RUNNING       0x01
#define STATE_ZOMBIE        0x02
#define STATE_TERMINATED    0x04
/* Because the task structure is different for event-driven tasks, we need to distinguish
** them. By default all tasks are periodic unless specified. */
#define TASKTYPE_BLOCKING   0x08


/* PERIODIC TASK CONTROL BLOCK */
typedef struct TCB {
  struct TCB *Next[2];           // Next TCB in the list where this task is located
                                 // [0]: ready queue link, [1]: arrival queue link
  UINT8 TaskState;               // Current state of the task
  #if SCHEDULER_REAL_TIME_MODE == DEADLINE_MONOTONIC_SCHEDULING
     UINT8 Priority;             // Current instance running priority
  #endif
  INT32 NextArrivalTimeLow;      // Next arrival time, modulo 2^30
  void (*TaskCodePtr)(void *);   // Pointer to the first instruction of the code task
                                 // (Needed to reinitialize a new instance execution)
  void *Argument;                // An instance specific pointer width value
  #if SCHEDULER_REAL_TIME_MODE == EARLIEST_DEADLINE_FIRST_STAR
     INT32 CurrentArrivalTimeLow; // Release time of the current instance, EDF* tie-break
  #endif
  #if POWER_MANAGEMENT == DRA || POWER_MANAGEMENT == DR_OTE
     /* The simulation-queue link is not a third entry of Next[]: the context switch reads
     ** the fields that follow Next[] at fixed offsets, and a third entry moved them all
     ** four bytes. */
     struct TCB *NextSim;        // Next TCB in the simulation queue
  #endif
  #ifdef STATIC_POWER_MANAGEMENT
     UINT8 FrequencyIndex;       // Static frequency setting
  #endif
  #if POWER_MANAGEMENT != NONE && POWER_MANAGEMENT != OTE && POWER_MANAGEMENT != DM_SLACK
     INT32 WCET;                 // Worst case execution time (user input)
     INT32 CompletionTime;       // Remaining execution time of this instance
  #endif
  #if POWER_MANAGEMENT == DM_SLACK
     INT32 WCET;                 // Worst case execution time (user input)
     INT32 RemainingWork;        // Amount of WCET yet to be done
  #endif
  INT32 PeriodLow;               // Interarrival period >= WCET (user input)
  #if SCHEDULER_REAL_TIME_MODE != DEADLINE_MONOTONIC_SCHEDULING || POWER_MANAGEMENT != NONE
     INT32 NextDeadline;         // Current instance deadline (offset==ETCB)
     INT32 Deadline;             // Task's deadline <= Period (user input)
  #endif
  #if POWER_MANAGEMENT == OTE
     INT32 WCET;                 // Worst case execution time (user input)
  #endif
  UINT16 PeriodHigh;             // Number of full 2^30 cycles of the interarrival period
  UINT16 NextArrivalTimeHigh;    // Number of full 2^30 cycles of the next arrival time
  #if POWER_MANAGEMENT == DRA || POWER_MANAGEMENT == DR_OTE || POWER_MANAGEMENT == OTE
     INT32 RemainingWork;        // Amount of WCET yet to be done by the task
  #endif
} TCB;

OSCheckTCBLayout();



/* EVENT-DRIVEN OR SYNCHRONOUS TASK CONTROL BLOCK */
struct FIFOQUEUE;
typedef struct ETCB {
  struct ETCB *Next[2];          // Next TCB in the list where this task is located
                                 // [0]: ready queue link, [1]: event queue link
  UINT8 TaskState;               // Current state of the task
  #if SCHEDULER_REAL_TIME_MODE == DEADLINE_MONOTONIC_SCHEDULING
     UINT8 Priority;             // Static task priority
  #endif
  INT32 NextArrivalTimeLow;      // Earliest time the next instance may start
  void (*TaskCodePtr)(void *);   // Pointer to the first instruction of the code task
                                 // (Needed to reinitialize a new instance execution)
  void *Argument;                // An instance specific pointer width value
  #if SCHEDULER_REAL_TIME_MODE == EARLIEST_DEADLINE_FIRST_STAR
     INT32 CurrentArrivalTimeLow; // Release time of the current instance, EDF* tie-break
  #endif
  #if POWER_MANAGEMENT == DRA || POWER_MANAGEMENT == DR_OTE
     struct ETCB *NextSim;       // At the same place as in the TCB
  #endif
  #ifdef STATIC_POWER_MANAGEMENT
     UINT8 FrequencyIndex;       // Static frequency setting
  #endif
  #if POWER_MANAGEMENT != NONE && POWER_MANAGEMENT != OTE && POWER_MANAGEMENT != DM_SLACK
     INT32 WCET;                 // Worst case execution time (user input)
     INT32 CompletionTime;       // Remaining execution time of this instance
  #endif
  #if POWER_MANAGEMENT == DM_SLACK
     INT32 WCET;                 // Worst case execution time (user input)
     INT32 RemainingWork;        // Amount of WCET yet to be done
  #endif
  #if SCHEDULER_REAL_TIME_MODE == DEADLINE_MONOTONIC_SCHEDULING
     INT32 PeriodLow;            // Interarrival period >= WCET (user input)
  #else
     INT32 WorkLoad;             // Equal to WCET / (available processor load)
     INT32 NextDeadline;         // Key criterion for EDF scheduling (offset==TCB)
  #endif
  struct FIFOQUEUE *EventQueue;  // Pointer to the event queue of this task
  struct ETCB *NextETCB;         // Link to the next created ETCB
} ETCB;


/* TASK SCHEDULING QUEUES */
/* All queues use the same head and tail sentinel blocks and there are 2 or 3 lists to
** manage the scheduling of the tasks:
** Ready queue: The first item of the list points to the currently active task instance
** (_OSActiveTask). */
#define READYQ    0
/* Queue of task instances that have yet to arrive: When a periodic task instance termi-
** nates, it places its TCB in the arrival queue which stays in the queue until its pe-
** riod expires at which time a new instance is created. */
#define ARRIVALQ  1
/* Simulation queue (linked through NextSim, DRA and DR_OTE only): Simulated list of run-
** ning tasks giving the task events when these tasks use their WCETs. This list is
** needed to extract the excess times with DRA. */
/* Queue of event-driven tasks that are blocked for an event: Event-driven tasks can be
** blocked, running or waiting. In the blocked state, the task is placed in the queue as-
** sociated with the event using the same link as ARRIVALQ. The task can also be in the
** arrival queue if it has expired its processor load with its last execution. */
#define BLOCKQ    ARRIVALQ

/* The sentinels of the queues, the tail being also the idle task. Both are whole TCBs,
** zeroed as the rest of .bss: the kernel reads task fields through the tail, and a
** sentinel sized to the few fields it uses would let those reads fall past it — into
** the next allocation or past the end of the RAM. */
static TCB QueueHeadSentinel, QueueTailSentinel;
TCB *_OSQueueHead = NULL;
static TCB *OSQueueTail = NULL;

#if SCHEDULER_REAL_TIME_MODE == DEADLINE_MONOTONIC_SCHEDULING
  /* EVENT-DRIVEN TASKING UNDER DEADLINE MONOTONIC SCHEDULING
  ** Event-driven tasks are characterized by a tuple (WCET,workload) where the workload
  ** is the equivalent of the task's deadline and period. Because of the priority assign-
  ** ment, the workload fixes the task's scheduling priority. However, a new instance may
  ** not begin before the end of the previous period to guarantee the task set is sched-
  ** ulable. Hence, when a task completes its current instance, it joins a blocking
  ** queue to wait for the next event; when the event occurs, it must first finish its
  ** period. This is done in the arrival queue of periodic tasks. Because the timer peri-
  ** odically resets the system time, the earliest starting time of each event-driven
  ** task must also be adjusted. To do this, we need to access them. */
  static ETCB *SynchronousTaskList = NULL;
#else
  /* APERIODIC SERVER SCHEDULING DEFINITIONS UNDER EDF
  ** Event-driven tasks are served by a total bandwidth server (M. Spuri and
  ** G. Buttazzo, Real-Time Systems 10(2), 1996): if U(hard) denotes the processor utili-
  ** zation of the periodic tasks, the sum of WCET_i/period_i, the remaining utilization
  ** 1-U(hard) can be given to the event-driven tasks. Each instance gets the deadline
  **           d(j) = max(d(j-1),currentTime) + WCET/U(tasks)
  ** where U(tasks) <= 1 - U(hard), and is scheduled by EDF with the periodic tasks. */
  static INT32 SynchronousTaskDeadlines = 0;  // denotes d(j) in the above
  /* SynchronousTaskList serves the same purpose as for DM scheduling. */
  static ETCB *SynchronousTaskList = NULL;
  #if POWER_MANAGEMENT == DRA || POWER_MANAGEMENT == DR_OTE
    static UINT8 AperiodicUtilization = 0; // aperiodic processor utilization
    static INT32 AperiodicExcess = 0;      // transferable excess not used by aperiodics
    static INT32 AperiodicExcessTime = 0;  // time of the last update
  #endif
#endif
/* RescheduleSynchronousTaskList is a temporary LIFO queue that is used to transfer event
** driven tasks that can be scheduled into the ready queue. Tasks in this queue are in-
** serted by a signaling task or by an event-driven task when this last task needs to re-
** start itself. The rationale behind this queue is to let the timer interrupt insert
** the tasks into the ready queue because these tasks may take precedence over the caller.
** Hence, the caller needs to save its context before releasing the processor. This is
** done by invoking a timer interrupt. */
static ETCB *RescheduleSynchronousTaskList = NULL;


/* With DRA or DR_OTE power management modes, we mimic the tasks of the ready queue running
** their WCETs. Because time-periods to update the simulation are defined when a task
** terminates or when a task arrives, the simulation clock, DRASimTime, may be ahead of
** the system wall clock. The difference between the current time and DRASimTime is the
** time spent executing the task at the head of the simulation queue. */
#if POWER_MANAGEMENT == DRA || POWER_MANAGEMENT == DR_OTE
  static INT32 DRASimTime = 0;
#endif
/* To determine the worst-case running time that is left for a periodic task, the elapsed
** time since the task got hold of the processor needs to be determined. The following
** global variable marks the time the active task was scheduled (the simulated time under
** DRA). */
#if POWER_MANAGEMENT != NONE
  static INT32 LastRemainingWorkUpdate = 0;
#endif
/* With deadline monotonic scheduling, tasks that complete before their forecasted WCET
** can give their remaining cycles (slack) to a lower priority task. However we need to
** monitor the slack and the slack owner's priority. */
#if POWER_MANAGEMENT == DM_SLACK
  static INT32 DMSlackAmount = 0;
  static UINT8 DMSlackPriority = 0;
#endif
/* Speeds are indices of the port's operating points, OS_xxMHZ_SPEED, slowest first (a
** frequency with its core voltage). The minimal one bounds the speeds GetProcessorSpeed
** may choose. */
#if POWER_MANAGEMENT != NONE
  static UINT8 MinimalProcessorSpeed = 0;  // Slowest processor speed by default
#endif


/* When a task instance is finishing and transfers the processor to another task, it must
** set the time parameters of the new instance in its TCB. If however the terminating
** task fails to accomplish this because of a timer interrupt, and because the termina-
** ting task never returns once preempted, the timer must complete the work started by
** this task. The timer handler finds such a task from _OSNoSaveContext: raised by the
** task once it is a zombie, or by FinalizeContextSwitchPreparation, which completes its
** removal, and cleared only by the context switch that follows. A flag of its own, raised
** before the task became a zombie and cleared by the handler, was cleared by an interrupt
** that found the task still running: a second one, after it had become a zombie, took the
** next task for one that had been running, charged it the time of the task ending and
** left it at that task's speed (test/host, endinside, 2026-09-25). */


/* TASK INSTANCE MANAGEMENT */
/* _OSActiveTask: Pointer to the current active task. This is the task that gets the pro-
** cessor when running the application tasks. */
TCB *_OSActiveTask = NULL;

/* _OSNoSaveContext: Indicates when the context of the interrupted task must not be saved
** because the task is marked for delete and will not resume. At the end of the execution
** of a task, the task tries to remove itself from the ready queue and then pass the pro-
** cessor to the next ready task while cleaning its stack. Once it has completed parts of
** these operations, there is no need for it to completely finish its termination as this
** can be done by the interrupt handler. In this case, the task never resumes and must
** never be saved. (see function OSEndTask()) */
BOOL _OSNoSaveContext = TRUE;

/* CompilerBarrier: Keeps the compiler from moving memory accesses across it, where an
** interrupt on this core may come in between and must find them in program order.
** _OSMemoryBarrier orders them for the other core too. */
#ifndef CompilerBarrier   /* the host test takes an interrupt there (test/host) */
   #define CompilerBarrier() __asm volatile ("" ::: "memory")
#endif


/* TASK EXECUTION STACK
** _OSStackBasePointer: Pointer to the stack base of currently running task: Local varia-
** bles used in the task are located between the SP register and the stack base, and when
** resuming the previous task, we need to start popping registers from the base.
** Before OSStartMultitasking, OSMalloc uses it as its allocation pointer. */
void *_OSStackBasePointer;


/* TIME KEEPING */
/* The timer counts modulo 2^30: each time it passes that boundary, every temporal vari-
** able is shifted back by ShiftTimeLimit. The value is tied to the 2^30 cycles of
** PeriodHigh and NextArrivalTimeHigh and to the 0x3FFFFFFF masks of the timer handler;
** it cannot be changed alone. */
static const INT32 ShiftTimeLimit = 0x40000000; // = 2^30
/* The while loop that empties the arrival queue uses a condition that simply depends
** upon the current time. To avoid crossing the tail sentinel, the arrival time of the
** sentinel must be unreachable (unattainable arrival time). The host build, which
** includes <stdint.h>, already has it. */
#ifndef INT32_MAX
   #define INT32_MAX 0x7FFFFFFF   /* 2^31 - 1 */
#endif


/* INTERNAL FUNCTION PROTOTYPES AND MACROS */
static BOOL Initialize(void);
static void IdleTask(void *);
static TCB *CreateTask(void task(void *), INT32, UINT16, INT32, INT32, void *);
static BOOL ValidTiming(UINT16 periodCycles, INT32 periodOffset, INT32 deadline);
#if SCHEDULER_REAL_TIME_MODE == DEADLINE_MONOTONIC_SCHEDULING
  static UINT8 GetTaskPriority(INT32 deadline);
#endif
typedef BOOL SEARCHFUNCTION(const TCB *, const TCB *);
static void InsertQueue(SEARCHFUNCTION TestKey, UINT8 offsetNext, TCB *newNode);
static BOOL ReadyQueueInsertTestKey(const TCB *searchKey, const TCB *node);
#define ReadyQueueInsert(node) InsertQueue(ReadyQueueInsertTestKey,READYQ,node)
static BOOL ArrivalQueueInsertTestKey(const TCB *searchKey, const TCB *node);
#define ArrivalQueueInsert(node) InsertQueue(ArrivalQueueInsertTestKey,ARRIVALQ,node)
void _OSTimerInterruptHandler(void);
#ifndef NonMaskableSoftwareTimer
   static void EnqueueRescheduleQueue(ETCB *etcb);
#else
   static void EnqueueRescheduleQueueBeforeBoot(ETCB *etcb);
   static void EnqueueRescheduleQueueAfterBoot(ETCB *etcb);
   static void (*EnqueueRescheduleQueue)(ETCB *etcb) = EnqueueRescheduleQueueBeforeBoot;
#endif
#if POWER_MANAGEMENT == DRA || POWER_MANAGEMENT == DR_OTE
  static BOOL EmptyRescheduleSynchronousTaskList(INT32 currentTime,
                                                 BOOL doSimUpdateElapseTime);
#else
  static void EmptyRescheduleSynchronousTaskList(INT32 currentTime);
#endif
#if SCHEDULER_REAL_TIME_MODE != DEADLINE_MONOTONIC_SCHEDULING
  static INT32 GetSuspendedSchedulingDeadline(ETCB *, INT32 currentTime);
#endif
#if POWER_MANAGEMENT == OTE
  static void UpdateRemainingWork(TCB *task, UINT8 currentSpeed, INT32 newTime);
#elif POWER_MANAGEMENT == DM_SLACK
  static void UpdateRemainingWork(TCB *task, UINT8 currentSpeed, INT32 newTime);
  static void DMSlackCalculateSlack(TCB *task, UINT8 currentSpeed, INT32 newTime);
  static BOOL DMSlackUpdateSlack(void);
#elif POWER_MANAGEMENT == DRA || POWER_MANAGEMENT == DR_OTE
  static void DRASimQueueInsert(TCB *newNode);
  static void UpdateRemainingWork(TCB *task, UINT8 currentSpeed, INT32 newTime);
  static void DRASimUpdateElapseTime(INT32 newTime);
  static void InterruptibleINT32CAS2(INT32 *, INT32, INT32, INT32 *, INT32, INT32, BOOL);
  static void InterruptibleMixCAS2(INT32 *, INT32, INT32, TCB **, TCB *, TCB *, BOOL);
#endif
#if POWER_MANAGEMENT != NONE
  static UINT8 GetProcessorSpeed(INT32 time);
#endif
#if POWER_MANAGEMENT == OTE || POWER_MANAGEMENT == DR_OTE || POWER_MANAGEMENT == DM_SLACK
  static INT32 GetEarliestAperiodicArrival(void);
#endif
#if POWER_MANAGEMENT == DR_OTE || POWER_MANAGEMENT == DRA
  static INT32 GetDRASlackTime(void);
#endif


/* Initialize: Initializes the internals of the OS. This function is called prior to
** creating the first task and sets up the needed queues.
** Returned value: (BOOL) TRUE on success and FALSE otherwise. */
BOOL Initialize(void)
{
  /* The sentinel heads and tails of the ready and arrival queues. */
  _OSQueueHead = &QueueHeadSentinel;
  OSQueueTail = &QueueTailSentinel;
  _OSQueueHead->Next[READYQ] = OSQueueTail;
  OSQueueTail->Next[READYQ] = NULL;
  /* Create the idle task as the tail of the ready queue. */
  OSQueueTail->TaskCodePtr = IdleTask;
  #if SCHEDULER_REAL_TIME_MODE == DEADLINE_MONOTONIC_SCHEDULING
     OSQueueTail->Priority = 0;
  #endif
  /* Make an empty arrival queue. */
  _OSQueueHead->Next[ARRIVALQ] = OSQueueTail;
  OSQueueTail->Next[ARRIVALQ] = NULL;
  OSQueueTail->TaskState = STATE_INIT | TASKTYPE_BLOCKING;
  OSQueueTail->NextArrivalTimeLow = INT32_MAX;
  #if POWER_MANAGEMENT == DRA || POWER_MANAGEMENT == DR_OTE
    _OSQueueHead->NextSim = OSQueueTail;
    OSQueueTail->NextSim = NULL;
  #endif
  return TRUE;
} /* end of Initialize */


/* IdleTask: Runs whenever no other task is ready. On its first run it starts the timer,
** which OSStartMultitasking only prepared, and it then sleeps until the next interrupt;
** once running, it is resumed, never restarted. The argument is undefined. */
void IdleTask(void *argument)
{
  if (_OSQueueHead->Next[ARRIVALQ] != OSQueueTail || SynchronousTaskList != NULL) {
     /* No interrupt in between: a task signaled once the switch below is made runs the
     ** timer handler, which must find the timer started and its origin set. */
     _OSDisableInterrupts();
     #ifdef NonMaskableSoftwareTimer
        /* Switch to normal processing of task signaling, see EnqueueRescheduleQueue-
        ** BeforeBoot defined below. */
        EnqueueRescheduleQueue = EnqueueRescheduleQueueAfterBoot;
     #endif
     _OSStartTimer();   // Start the interval timer
     _OSEnableInterrupts();
  }
  /* Enter in the lowest possible sleep mode */
  _OSSleep();
} /* end of IdleTask */


/* ValidTiming: Whether the kernel can count with a period and a deadline: a period that
** is not 0, made of fewer than 65535 turns of 2^30 ticks, since the arrival time takes one
** more when its remainder carries, and a remainder below 2^30; a deadline from one tick to
** the period, and below 2^30, since the absolute deadline, an arrival plus the deadline,
** must fit in an INT32. */
BOOL ValidTiming(UINT16 periodCycles, INT32 periodOffset, INT32 deadline)
{
  return periodCycles < 0xFFFF && periodOffset >= 0 && periodOffset < ShiftTimeLimit &&
         deadline > 0 && deadline < ShiftTimeLimit &&
         (periodCycles > 0 || deadline <= periodOffset);
} /* end of ValidTiming */


/* OSCreateTask: Creates a new task by allocating a new TCB to the task. */
BOOL OSCreateTask(void task(void *), INT32 wcet, UINT16 periodCycles, INT32 periodOffset,
                  INT32 deadline, void *argument)
{
  return CreateTask(task,wcet,periodCycles,periodOffset,deadline,argument) != NULL;
} /* end of OSCreateTask */


/* CreateTask: Allocates and initializes the TCB of a new periodic task, and returns it
** (NULL on failure) so that OSCreateTask and _OSCreateTask can complete it.
** The period and arrival time of periodic tasks are given respectively by the relation
**     period = PeriodHigh * 2^30 + PeriodLow
**     arrival time = NextArrivalTimeHigh * 2^30 + NextArrivalTimeLow */
TCB *CreateTask(void task(void *), INT32 wcet, UINT16 periodCycles, INT32 periodOffset,
                  INT32 deadline, void *argument)
{
  TCB *ptcb;
  if (!ValidTiming(periodCycles,periodOffset,deadline))
     return NULL;
  if (_OSQueueHead == NULL && !Initialize())
     return NULL;
  #if SCHEDULER_REAL_TIME_MODE == DEADLINE_MONOTONIC_SCHEDULING
     if (OSQueueTail->Priority == 0xFF)  // the priorities are counted in a byte
        return NULL;
  #endif
  /* Get a new TCB and initialize it. */
  if ((ptcb = (TCB*)OSMalloc(sizeof(TCB))) == NULL)
     return NULL;
  ptcb->Next[READYQ] = NULL;
  ptcb->TaskState = STATE_ZOMBIE;
  ptcb->TaskCodePtr = task;
  ptcb->PeriodLow = periodOffset;
  ptcb->PeriodHigh = periodCycles;
  ptcb->NextArrivalTimeHigh = 0;
  #ifdef STATIC_POWER_MANAGEMENT
     ptcb->FrequencyIndex = OS_MAX_SPEED; // Run the task at the highest frequency
  #endif
  #if POWER_MANAGEMENT != NONE
     ptcb->WCET = wcet;
  #endif
  ptcb->Argument = argument;
  #if SCHEDULER_REAL_TIME_MODE == DEADLINE_MONOTONIC_SCHEDULING
     /* Temporarily save the deadline until the user calls OSStartMultitasking(). */
     ptcb->NextArrivalTimeLow = deadline;
     #if POWER_MANAGEMENT != NONE
        ptcb->Deadline = deadline;
     #endif
     ptcb->Priority = GetTaskPriority(deadline);
  #else
     ptcb->NextArrivalTimeLow = 0;
     ptcb->Deadline = deadline;
  #endif
  ArrivalQueueInsert(ptcb);
  return ptcb;
} /* end of CreateTask */


#if SCHEDULER_REAL_TIME_MODE == DEADLINE_MONOTONIC_SCHEDULING
/* GetTaskPriority: Returns the priority for a task given its deadline. */
UINT8 GetTaskPriority(INT32 deadline)
{
  ETCB *nextETCB;
  TCB *nextTCB = (TCB *)_OSQueueHead;
  UINT8 nbTasks = 0;
  /* Periodic tasks are initially sorted according to their deadlines and temporarily in-
  ** serted in the arrival queue; the priority of these tasks can be found by traversing
  ** this queue. Note that the task arrival times are reset to their initial values when
  ** starting Escapement. Also note that after creating the last task, OSQueueTail->Priority
  ** holds the number of tasks in the application. */
  do {
     nextTCB = nextTCB->Next[ARRIVALQ];
     if (nextTCB->NextArrivalTimeLow > deadline)
        nextTCB->Priority++;
     else
        nbTasks++;
  } while (nextTCB != OSQueueTail);
  /* There may also be event-driven tasks that may have higher priority. Because these
  ** are not sorted, the whole list must be traversed. */
  for (nextETCB = SynchronousTaskList; nextETCB != NULL; nextETCB = nextETCB->NextETCB)
     if (nextETCB->PeriodLow > deadline)
        nextETCB->Priority++;
     else
        nbTasks++;
  return nbTasks;
} /* end of GetTaskPriority */
#endif


#ifdef STATIC_POWER_MANAGEMENT
/* _OSCreateTask: Same as OSCreateTask() but adds a frequency index to the list of para-
** meters. This frequency index is then used whenever the task is scheduled. */
BOOL _OSCreateTask(void task(void *), INT32 wcet, UINT16 periodCycles, INT32 periodOffset,
                   INT32 deadline, void *argument, UINT8 frequencyIndex)
{
  TCB *ptcb = CreateTask(task,wcet,periodCycles,periodOffset,deadline,argument);
  if (ptcb == NULL) return FALSE;
     ptcb->FrequencyIndex = frequencyIndex;
  return TRUE;
} /* end of _OSCreateTask */
#endif /* end of STATIC_POWER_MANAGEMENT */


/* OSEndTask: Called by a periodic task when it terminates its instance. The next in-
** stance of the task is already in the arrival queue. As soon as the task succeeds in
** positioning its state, it may be interrupted by any entity so long as this entity com-
** pletes the instructions done here. */
void OSEndTask(void)
{
  #if POWER_MANAGEMENT == DM_SLACK
     DMSlackCalculateSlack(_OSActiveTask,OSGetProcessorSpeed(),_OSGetActualTime());
  #endif
  /* Set the task to zombie to indicate that it is about to remove itself from the ready
  ** queue and that its context should not be saved. */
  _OSActiveTask->TaskState |= STATE_ZOMBIE;
  /* A zombie before its context is said not to be saved. GCC stored the flag first, and
  ** an interrupt in between found a task that was no zombie: it left the task in the
  ** ready queue, then wiped its stack in the task switch it made, and the task, still
  ** running as far as the kernel knew, was resumed later with the context of the one
  ** it had preempted (SoakPico2 under Renode, 2026-09-25). */
  CompilerBarrier();
  _OSNoSaveContext = TRUE; // Don't save the context of this task
  CompilerBarrier();       // a zombie before it leaves the ready queue
  #if POWER_MANAGEMENT == DRA || POWER_MANAGEMENT == DR_OTE
     /* Update the timing used up by this task in the simulation queue. */
     DRASimUpdateElapseTime(_OSGetActualTime());
  #endif
  /* Remove the task from the ready queue */
  _OSQueueHead->Next[READYQ] = _OSActiveTask->Next[READYQ];
  _OSActiveTask = _OSQueueHead->Next[READYQ];
  #if POWER_MANAGEMENT == DM_SLACK
     /* Set the available slack for a lower priority task. */
     DMSlackUpdateSlack();
  #endif
  #if POWER_MANAGEMENT != NONE
     /* Adjust the processor speed for power management. There is no point in changing the
     ** current speed when switching to the idle task. */
     if (_OSActiveTask != OSQueueTail) {
        #if POWER_MANAGEMENT == DRA || POWER_MANAGEMENT == DR_OTE
           LastRemainingWorkUpdate = DRASimTime;
           OSSetProcessorSpeed(GetProcessorSpeed(LastRemainingWorkUpdate));
        #elif POWER_MANAGEMENT == OTE
           LastRemainingWorkUpdate = _OSGetActualTime();
           OSSetProcessorSpeed(GetProcessorSpeed(LastRemainingWorkUpdate));
        #else /* POWER_MANAGEMENT == DM_SLACK */
           LastRemainingWorkUpdate = _OSGetActualTime();
           OSSetProcessorSpeed(GetProcessorSpeed(LastRemainingWorkUpdate));
        #endif
     }
  #elif defined(STATIC_POWER_MANAGEMENT)
     if (_OSActiveTask != OSQueueTail)
        OSSetProcessorSpeed(_OSActiveTask->FrequencyIndex);
  #endif
  _OSScheduleTask();
} /* end of OSEndTask */


/* SubOrZeroIfNeg: result = max(result - sub, 0), in 32-bit arithmetic, where sub > 0. */
#define SubOrZeroIfNeg(result,sub) \
{ \
  result -= sub;  \
  if (result < 0) \
     result = 0;  \
}


#if POWER_MANAGEMENT != NONE
   /* The very first timer interrupt of a burst saves the current processor speed so that
   ** the very last interrupt can restore the speed or determine a new speed. */
   static BOOL ResetProcessorSpeed = FALSE;  // Flag indicating whether speed was saved.
   static UINT8 SavedCurrentSpeed;
#endif
/* _OSTimerInterruptHandler: Software interrupt handler for the timer that manages task
** instance arrivals. Because the timer is a bit counter with a predefined number of bits,
** when a timer event occurs, it can be that there are no arrivals. In this case, we only
** need to check whether temporal variables must be shifted. */
void _OSTimerInterruptHandler(void)
{
  INT32 currentTime;
  ETCB *etcb;
  TCB *arrival;
  extern volatile BOOL _OSOverflowInterruptFlag;
  extern volatile BOOL _OSComparatorInterruptFlag;
  #if POWER_MANAGEMENT == DRA || POWER_MANAGEMENT == DR_OTE
     BOOL doSimUpdateElapseTime = TRUE;
  #endif
  #if POWER_MANAGEMENT != NONE
     if (!ResetProcessorSpeed) { // Keep the first processor speed if not already saved
        SavedCurrentSpeed = OSGetProcessorSpeed(); // Save the current speed before modifying it
        OSSetProcessorSpeed(OS_MAX_SPEED);
        ResetProcessorSpeed = TRUE;
     }
  #elif defined(STATIC_POWER_MANAGEMENT)
     OSSetProcessorSpeed(OS_MAX_SPEED);
  #endif
  #if POWER_MANAGEMENT == DRA || POWER_MANAGEMENT == DR_OTE
     /* Finish any pending CAS2 which may modify the simulation queue before we insert a
     ** new arrival. This needs to be done only once. */
     InterruptibleMixCAS2(NULL,0,0,NULL,NULL,NULL,TRUE);
     /* Do this also for probable pending DRASimTime */
     InterruptibleINT32CAS2(0,0,0,0,0L,0,TRUE);
  #endif
  do {
     if (_OSTimerIsOverflow(ShiftTimeLimit)) {   // Has timer overflowed?
        /* To avoid overflow of the timer's time, a time shift is done on all temporal
        ** variables. Because all these variables are signed, their relative values are pre-
        ** served. */
        #if SCHEDULER_REAL_TIME_MODE != DEADLINE_MONOTONIC_SCHEDULING || POWER_MANAGEMENT != NONE
           /* Time shift periodic tasks in the ready queue. Under deadline-monotonic
           ** scheduling too, the deadline bounding how far a task is slowed down. */
           for (arrival = _OSQueueHead; (arrival = arrival->Next[READYQ]) != OSQueueTail; )
              if ((arrival->TaskState & TASKTYPE_BLOCKING) == 0) {
                 arrival->NextDeadline -= ShiftTimeLimit;
                 #if SCHEDULER_REAL_TIME_MODE == EARLIEST_DEADLINE_FIRST_STAR
                    arrival->CurrentArrivalTimeLow -= ShiftTimeLimit;
                 #endif
              }
        #endif
        #if POWER_MANAGEMENT == DRA || POWER_MANAGEMENT == DR_OTE
           /* The simulation queue also holds periodic instances that ended before their
           ** WCET, zombies until their next arrival: sorted as the ready queue is, they
           ** shift with it, or each new instance would be inserted before them. */
           for (arrival = _OSQueueHead; (arrival = arrival->NextSim) != OSQueueTail; )
              if ((arrival->TaskState & (TASKTYPE_BLOCKING | STATE_ZOMBIE)) == STATE_ZOMBIE) {
                 arrival->NextDeadline -= ShiftTimeLimit;
                 arrival->CurrentArrivalTimeLow -= ShiftTimeLimit;
              }
        #endif
        /* Time shift all tasks in the arrival queue. */
        for (arrival = _OSQueueHead; (arrival = arrival->Next[ARRIVALQ]) != OSQueueTail; ) {
           if ((arrival->TaskState & TASKTYPE_BLOCKING) == 0) {
              if (arrival->NextArrivalTimeHigh > 0)
                 arrival->NextArrivalTimeHigh--;
              else
                 arrival->NextArrivalTimeLow -= ShiftTimeLimit;
           }
           #if SCHEDULER_REAL_TIME_MODE != DEADLINE_MONOTONIC_SCHEDULING
              else {
                 arrival->NextArrivalTimeLow -= ShiftTimeLimit;
                 #if SCHEDULER_REAL_TIME_MODE == EARLIEST_DEADLINE_FIRST_STAR
                    SubOrZeroIfNeg(arrival->CurrentArrivalTimeLow,ShiftTimeLimit);
                 #endif
              }
           #endif
        }
        #if POWER_MANAGEMENT != NONE
           LastRemainingWorkUpdate -= ShiftTimeLimit;
        #endif
        /* Time shift all event-driven tasks. */
        for (etcb = SynchronousTaskList; etcb != NULL; etcb = etcb->NextETCB) {
           #if SCHEDULER_REAL_TIME_MODE == DEADLINE_MONOTONIC_SCHEDULING
              SubOrZeroIfNeg(etcb->NextArrivalTimeLow,ShiftTimeLimit);
           #else
              SubOrZeroIfNeg(etcb->NextDeadline,ShiftTimeLimit);
           #endif
        }
        #if SCHEDULER_REAL_TIME_MODE != DEADLINE_MONOTONIC_SCHEDULING
           SubOrZeroIfNeg(SynchronousTaskDeadlines,ShiftTimeLimit);
        #endif
        #if POWER_MANAGEMENT == DRA || POWER_MANAGEMENT == DR_OTE
           DRASimTime -= ShiftTimeLimit;
           /* Only an event-driven task brings this time forward: without one, shifting it
           ** at every wraparound would overflow it. Moved to zero, it only loses excess. */
           SubOrZeroIfNeg(AperiodicExcessTime,ShiftTimeLimit);
        #endif
     }
     while (TRUE) {
        _OSComparatorInterruptFlag = FALSE;
        /* Transfer all new arrivals to the ready queue. */
        currentTime = _OSGetActualTime();
        arrival = _OSQueueHead->Next[ARRIVALQ];
        while (arrival->NextArrivalTimeLow <= currentTime &&
             ((arrival->TaskState & TASKTYPE_BLOCKING) || arrival->NextArrivalTimeHigh == 0)) {
           _OSQueueHead->Next[ARRIVALQ] = arrival->Next[ARRIVALQ];
           /* At this point an arriving periodic task should be in state STATE_ZOMBIE, but
           ** event-driven tasks in state STATE_ZOMBIE | TASKTYPE_BLOCKING. */
           #ifdef DEBUG_MODE
              if (!(arrival->TaskState & STATE_ZOMBIE)) { // Is task still in the ready queue?
                 /* An arriving task should not be in state STATE_RUNNING */
                 _OSDisableInterrupts();
                 while (TRUE); // If we get here, the processor utilization > 100%.
              }
           #endif
           /* Set task to INIT while keeping flag TASKTYPE_BLOCKING */
           arrival->TaskState &= TASKTYPE_BLOCKING;
           #if SCHEDULER_REAL_TIME_MODE != DEADLINE_MONOTONIC_SCHEDULING
              if ((arrival->TaskState & TASKTYPE_BLOCKING) == 0)
                 arrival->NextDeadline = arrival->NextArrivalTimeLow + arrival->Deadline;
              else
                 ((ETCB *)arrival)->NextDeadline =
                                    GetSuspendedSchedulingDeadline((ETCB *)arrival,currentTime);
           #elif POWER_MANAGEMENT != NONE 
              if ((arrival->TaskState & TASKTYPE_BLOCKING) == 0)
                 arrival->NextDeadline = arrival->NextArrivalTimeLow + arrival->Deadline;
           #endif
           #if SCHEDULER_REAL_TIME_MODE == EARLIEST_DEADLINE_FIRST_STAR
              arrival->CurrentArrivalTimeLow = arrival->NextArrivalTimeLow;
           #endif
           ReadyQueueInsert(arrival);
           /* Prepare the next instance arrival */
           if ((arrival->TaskState & TASKTYPE_BLOCKING) == 0) {
              arrival->NextArrivalTimeHigh = arrival->PeriodHigh;
              arrival->NextArrivalTimeLow += arrival->PeriodLow;
              if (arrival->NextArrivalTimeLow > 0x3FFFFFFF) {
                 arrival->NextArrivalTimeHigh += 1;
                 arrival->NextArrivalTimeLow &= 0x3FFFFFFF;
              }
              ArrivalQueueInsert(arrival);
           }
           #if SCHEDULER_REAL_TIME_MODE == DEADLINE_MONOTONIC_SCHEDULING
              else
                 arrival->NextArrivalTimeLow += ((ETCB *)arrival)->PeriodLow;
           #endif
           #if POWER_MANAGEMENT == DRA || POWER_MANAGEMENT == DR_OTE
              /* Before modifying the simulation queue, we need to update it when its head can
              ** change, otherwise the elapse time will be decremented for the wrong simulated
              ** task. */
              if (doSimUpdateElapseTime && _OSQueueHead->Next[READYQ] != _OSActiveTask) {
                 DRASimUpdateElapseTime(currentTime);
                 doSimUpdateElapseTime = FALSE;
              }
              arrival->CompletionTime = arrival->WCET;
              if (arrival->TaskState == STATE_INIT)  // For periodic tasks only
                 arrival->RemainingWork = arrival->WCET;
              /* DRASimQueueInsert() will always insert the new instance TCB after the previous
              ** one if it is still present, and in doing so, it will also remove it. */
              DRASimQueueInsert(arrival);
           #elif POWER_MANAGEMENT == DM_SLACK
              arrival->RemainingWork = arrival->WCET;
           #elif POWER_MANAGEMENT != NONE
              if (arrival->TaskState == STATE_INIT)  // For periodic tasks only
                 arrival->RemainingWork = arrival->WCET;
           #endif
           arrival = _OSQueueHead->Next[ARRIVALQ];
        }
        #if POWER_MANAGEMENT == DM_SLACK
           if (DMSlackUpdateSlack()) {
              /* The slack runs out with the time elapsed, whichever task ran: taken after
              ** UpdateRemainingWork, which moves LastRemainingWorkUpdate to now, it was 0,
              ** and a task preempting one slowed down on the slack got it whole again. */
              if ((DMSlackAmount -= currentTime - LastRemainingWorkUpdate) < 0)
                 DMSlackAmount = 0;
              if (_OSActiveTask != OSQueueTail)
                 UpdateRemainingWork(_OSActiveTask,SavedCurrentSpeed,currentTime);
              LastRemainingWorkUpdate = currentTime;
           }
        #elif POWER_MANAGEMENT != NONE
           /* Only a periodic task has work to update: testing TASKTYPE_BLOCKING leaves out
           ** both the event-driven tasks and the idle task, which carries the flag too. */
           if (!_OSNoSaveContext && (_OSActiveTask->TaskState & TASKTYPE_BLOCKING) == 0)
              UpdateRemainingWork(_OSActiveTask,SavedCurrentSpeed,currentTime);
        #endif
        /* Process pending event-driven tasks found in the RescheduleSynchronousTaskList. */
        #if POWER_MANAGEMENT == DRA || POWER_MANAGEMENT == DR_OTE
           doSimUpdateElapseTime = EmptyRescheduleSynchronousTaskList(currentTime,
                                                                     doSimUpdateElapseTime);
        #else
           EmptyRescheduleSynchronousTaskList(currentTime);
        #endif
        /* Set arrival timer to the next arrival time. */
        arrival = _OSQueueHead->Next[ARRIVALQ];
        if (arrival != OSQueueTail &&
               ((arrival->TaskState & TASKTYPE_BLOCKING) || arrival->NextArrivalTimeHigh == 0)) {
           /* Set the timer comparator to the next periodic task arrival time. */
           if (_OSSetTimer(arrival->NextArrivalTimeLow))
               break; // break if NextArrivalTimeLow > current time
        }
        else
           break;
     }
     _OSClearSoftTimerInterrupt();
  } while (_OSOverflowInterruptFlag || _OSComparatorInterruptFlag || RescheduleSynchronousTaskList != NULL);
  #if POWER_MANAGEMENT != NONE
     /* We may need to change the processor speed if a new task is scheduled. */
     arrival = _OSActiveTask;
     /* Return to the task with highest priority or start a new instance. */
     _OSActiveTask = _OSQueueHead->Next[READYQ];
     if (_OSActiveTask != OSQueueTail) {
        #if POWER_MANAGEMENT == DRA || POWER_MANAGEMENT == DR_OTE
           if (_OSActiveTask != arrival || _OSNoSaveContext) {
              if (doSimUpdateElapseTime)
                 DRASimUpdateElapseTime(currentTime);
              LastRemainingWorkUpdate = currentTime;
              SavedCurrentSpeed = GetProcessorSpeed(currentTime);
           }
        #elif POWER_MANAGEMENT == DM_SLACK
           if (_OSActiveTask != arrival || _OSNoSaveContext) {
              LastRemainingWorkUpdate = currentTime;
              SavedCurrentSpeed = GetProcessorSpeed(currentTime);
           }
        #else /*  POWER_MANAGEMENT == OTE */
           if (_OSActiveTask != arrival || _OSNoSaveContext) {
              LastRemainingWorkUpdate = currentTime;
              SavedCurrentSpeed = GetProcessorSpeed(currentTime);
           }
        #endif
        OSSetProcessorSpeed(SavedCurrentSpeed);
     }
     else {
        #if POWER_MANAGEMENT == DM_SLACK
           LastRemainingWorkUpdate = currentTime;
        #endif
     }
     ResetProcessorSpeed = FALSE;
  #elif defined(STATIC_POWER_MANAGEMENT)
     _OSActiveTask = _OSQueueHead->Next[READYQ];
     if (_OSActiveTask != OSQueueTail)
        OSSetProcessorSpeed(_OSActiveTask->FrequencyIndex);
  #else
     /* Return to the task with highest priority or start a new instance. */
     _OSActiveTask = _OSQueueHead->Next[READYQ];
  #endif
  _OSScheduleTask();
} /* end of _OSTimerInterruptHandler */


/* FUNCTIONS MANAGING THE QUEUES OF THE OS */
/* InsertQueue: Inserts a TCB into one of the queues of the OS. This can be the ready
** queue or the arrival queue.
** Parameters:
**  (1) (SEARCHFUNCTION) a selection function f(A,B) that returns TRUE when A should be
**      placed before B;
**  (2) (UINT8) offsetNext: one of READYQ or ARRIVALQ;
**  (3) (TCB *) the node to insert. */
void InsertQueue(SEARCHFUNCTION TestKey, UINT8 offsetNext, TCB *newNode)
{
  TCB *right, *left;
  /* Get left and right TCB neighbors */
  left = _OSQueueHead;
  while (TRUE) {
     /* Always insert before the tail or if the selection function succeeds. */
     if ((right = left->Next[offsetNext]) == OSQueueTail || TestKey(newNode,right))
        break; // found the right node
     left = right;
  }
  /* The loop always sets right, in its condition; cppcheck reads it as unset. */
  // cppcheck-suppress uninitvar
  newNode->Next[offsetNext] = right;
  left->Next[offsetNext] = newNode;
} /* end of InsertQueue */


/* ReadyQueueInsertTestKey: Search function used to insert a new TCB in the ready queue.
** Parameters:
**   (1) (const TCB *) new TCB to insert;
**   (2) (const TCB *) next TCB in the queue that will be scheduled after the new TCB.
** Returned value: (BOOL) TRUE if the node is before the next TCB and FALSE otherwise. */
BOOL ReadyQueueInsertTestKey(const TCB *insert, const TCB *next)
{
  #if SCHEDULER_REAL_TIME_MODE == DEADLINE_MONOTONIC_SCHEDULING
     return insert->Priority < next->Priority;
  #elif SCHEDULER_REAL_TIME_MODE == EARLIEST_DEADLINE_FIRST_STAR || SCHEDULER_REAL_TIME_MODE == EARLIEST_DEADLINE_FIRST
     INT32 insertDeadline, nextDeadline;
     if (insert->TaskState & TASKTYPE_BLOCKING)
        insertDeadline = ((ETCB *)insert)->NextDeadline;
     else
        insertDeadline = insert->NextDeadline;
     if (next->TaskState & TASKTYPE_BLOCKING)
        nextDeadline = ((ETCB *)next)->NextDeadline;
     else
        nextDeadline = next->NextDeadline;
  #endif
  #if SCHEDULER_REAL_TIME_MODE == EARLIEST_DEADLINE_FIRST
     return insertDeadline <= nextDeadline;
  #elif SCHEDULER_REAL_TIME_MODE == EARLIEST_DEADLINE_FIRST_STAR 
     return insertDeadline < nextDeadline ||
         (insertDeadline == nextDeadline &&
         // Same deadline: see EDF* in EscapementHardPA.h
         (next->CurrentArrivalTimeLow < insert->CurrentArrivalTimeLow ||
         (next->CurrentArrivalTimeLow == insert->CurrentArrivalTimeLow && next < insert)));
  #endif
} /* end of ReadyQueueInsertTestKey */


/* ArrivalQueueInsertTestKey: Search function used to insert a new TCB in the arrival
** queue.
** Parameters:
**   (1) (const TCB *) new TCB to insert;
**   (2) (const TCB *) next TCB in the queue that will arrive after the new TCB.
** Returned value: (BOOL) TRUE if the node is before the next TCB and FALSE otherwise. */
BOOL ArrivalQueueInsertTestKey(const TCB *insert, const TCB *next)
{
  /* A periodic task counts the wraparounds before its arrival apart, and keeps the rest
  ** below 2^30; an event-driven task keeps one time, the end of its period or its dead-
  ** line, which can lie beyond the next wraparound. Compared as it is, with no wraparound
  ** counted, such a time put the task before periodic tasks due earlier, which were then
  ** released late, or released again while still ready. */
  UINT16 insertHigh, nextHigh;
  INT32 insertLow = insert->NextArrivalTimeLow, nextLow = next->NextArrivalTimeLow;
  if ((insert->TaskState & TASKTYPE_BLOCKING) == 0)
     insertHigh = insert->NextArrivalTimeHigh;
  else if ((insertHigh = insertLow >= ShiftTimeLimit))
     insertLow -= ShiftTimeLimit;
  if ((next->TaskState & TASKTYPE_BLOCKING) == 0)
     nextHigh = next->NextArrivalTimeHigh;
  else if ((nextHigh = nextLow >= ShiftTimeLimit))
     nextLow -= ShiftTimeLimit;
  return insertHigh < nextHigh || (insertHigh == nextHigh && insertLow <= nextLow);
} /* end of ArrivalQueueInsertTestKey */


/* ARRAY-BASED DEFINITION OF A WAITFREE FIFO QUEUE USED TO STORE EVENT-DRIVEN TASKS */
typedef struct FIFOQUEUE {
  UINTPTR *Q;              // Array of queued items
  void *PendingOp;         // Posted operation for the queue (dequeue or enqueue op.)
  UINT16 Head, Tail;       // Current head and tail indices
  UINT16 MaxIndex;         // Value of Head or Tail at wrap-around
  UINT8 QueueLength;       // Circular array queue size
} FIFOQUEUE;

typedef struct DEQUEUE_DESCRIPTOR {
  UINTPTR SlotPropose, SlotReturn;
  UINT16 HeadPropose, Head;
  volatile BOOL Done;
} DEQUEUE_DESCRIPTOR;

typedef struct ENQUEUE_DESCRIPTOR {
  UINTPTR SlotPropose, SlotReturn;
  UINTPTR Item;
  UINT16 TailPropose, Tail;
  volatile BOOL Done;
} ENQUEUE_DESCRIPTOR;

static UINT16 GetFIFOArrayMaxIndex(UINT16 queueSize);
static UINTPTR FIFODequeue(FIFOQUEUE *queue, UINTPTR signal);
static void FIFODequeueHelper(FIFOQUEUE *queue, UINTPTR signal, DEQUEUE_DESCRIPTOR *des);
static BOOL FIFOEnqueue(FIFOQUEUE *queue, UINTPTR signal, UINTPTR item);
static void FIFOEnqueueHelper(FIFOQUEUE *queue, ENQUEUE_DESCRIPTOR *des);
static void IncrementFifoQueueIndex(UINT16 *index, UINT16 oldValue, UINT16 moduloBase);


/* The FIFO queue of suspended tasks associated with an event may also contain a special
** marker called SIGNAL. When a task signals another but there is no waiting event-driven
** task at that moment, SIGNAL is inserted into the queue where the next enqueue takes
** place. The next event-driven task that becomes blocked (and enqueues itself) can then
** immediately restart a new instance of itself. */
#define SIGNAL 0xFFFFu

/* EnqueueEventTask: Adds an event-driven task TCB into a FIFO list of tasks that are
** suspended for a particular event. If the slot where the task is to be stored holds a
** SIGNAL marker, the queue was empty when the event occurred; the task is not inserted
** as it is already signaled.
** Parameters:
**   (1) (FIFOQUEUE *) event descriptor with the queue of suspended tasks;
**   (2) (ETCB *) TCB to enqueue.
** Returned value: (BOOL) TRUE if the caller is not queued and should be rescheduled.
**    Otherwise the task is added at the end of the event queue. */
#define EnqueueEventTask(eventQueue,etcb) FIFOEnqueue(eventQueue,SIGNAL,etcb)

/* DequeueEventTask: Returns the first task in the FIFO list of tasks that is suspended
** and waiting for a particular event. If no such task exists, the event is marked so
** that the next time that a task suspends itself in the FIFO list it catches the event.
** This function is called by the task that generated an event.
** Parameter: (FIFOQUEUE *) event to signal and holding suspended tasks for it.
** Returned value: (ETCB *) Suspended task to schedule or NULL if none exist. */
#define DequeueEventTask(eventQueue) (ETCB *)FIFODequeue(eventQueue,SIGNAL)


/* OSCreateEventDescriptor: Creates and returns a descriptor with all the needed informa-
** tion to block (suspend) and wake-up an event-driven task instance. The implementation
** requires a circular list that can be created and completed once the number of tasks
** that can be blocked is known. */
void *OSCreateEventDescriptor(void)
{
  FIFOQUEUE *eventQueue;
  if ((eventQueue = (FIFOQUEUE *)OSMalloc(sizeof(FIFOQUEUE))) != NULL) {
     eventQueue->Head = eventQueue->Tail = eventQueue->QueueLength = 0;
     eventQueue->Q = NULL;
     eventQueue->PendingOp = NULL;
  }
  return eventQueue;
} /* end of OSCreateEventDescriptor */


/* FUNCTIONS FOR EVENT-DRIVEN TASKS */
/* OSCreateSynchronousTask: Like OSCreateTask() but applied to event-driven tasks. */
BOOL OSCreateSynchronousTask(void task(void *), INT32 wcet, INT32 workLoad,
                             UINT8 aperiodicUtilization, void *event, void *arg)
{
  ETCB *etcb;
  /* The workload is the deadline of each instance, and an event's queue counts its tasks
  ** in a byte. */
  if (event == NULL || ((FIFOQUEUE *)event)->QueueLength == 0xFF ||
      workLoad <= 0 || workLoad >= ShiftTimeLimit)
     return FALSE;
  if (_OSQueueHead == NULL && !Initialize())
     return FALSE;
  #if SCHEDULER_REAL_TIME_MODE == DEADLINE_MONOTONIC_SCHEDULING
     if (OSQueueTail->Priority == 0xFF)  // the priorities are counted in a byte
        return FALSE;
  #endif
  #if POWER_MANAGEMENT == DRA || POWER_MANAGEMENT == DR_OTE
     AperiodicUtilization = aperiodicUtilization;
  #endif
  /* Get a new TCB and initialize it. */
  if ((etcb = (ETCB *)OSMalloc(sizeof(ETCB))) == NULL)
     return FALSE;
  etcb->TaskState = STATE_ZOMBIE | TASKTYPE_BLOCKING;
  etcb->TaskCodePtr = task;
  etcb->Argument = arg;
  #ifdef STATIC_POWER_MANAGEMENT
     etcb->FrequencyIndex = OS_MAX_SPEED;
  #endif
  #if POWER_MANAGEMENT != NONE && POWER_MANAGEMENT != OTE
     etcb->WCET = wcet;
  #endif
  #if SCHEDULER_REAL_TIME_MODE == DEADLINE_MONOTONIC_SCHEDULING
     etcb->NextArrivalTimeLow = 0;
     etcb->Priority = GetTaskPriority(workLoad);
     etcb->PeriodLow = workLoad;
  #else
     etcb->WorkLoad = workLoad;
     etcb->NextDeadline = 0;
  #endif
  etcb->EventQueue = (FIFOQUEUE *)event;
  etcb->EventQueue->QueueLength++;
  /* Because FIFO enqueueing and dequeueing uses a table, the size of this table is only
  ** known after all the synchronous tasks have been created. Hence we need to tempora-
  ** rily save this task in a list, and enqueue the task later. */
  etcb->NextETCB = SynchronousTaskList;
  SynchronousTaskList = etcb;
  return TRUE;
} /* end of OSCreateSynchronousTask */


#ifdef STATIC_POWER_MANAGEMENT
/* _OSCreateSynchronousTask: Like OSCreateSynchronousTask() but adds a static frequency
** index to the list of parameters. This frequency index is then used whenever the task
** is scheduled. */
BOOL _OSCreateSynchronousTask(void task(void *), INT32 wcet, INT32 workLoad,
                              UINT8 aperiodicUtilization, void *event, void *arg,
                              UINT8 frequencyIndex)
{
  if (!OSCreateSynchronousTask(task,wcet,workLoad,aperiodicUtilization,event,arg))
     return FALSE;
  SynchronousTaskList->FrequencyIndex = frequencyIndex;
  return TRUE;
} /* end of _OSCreateSynchronousTask */
#endif /* end of STATIC_POWER_MANAGEMENT */


/* OSSuspendSynchronousTask: The very last instruction of an event-driven task. This
** function checks if a new task instance should restart (an event was signaled) or if it
** should suspend itself until the event is signaled by OSScheduleSuspendedTask(). */
void OSSuspendSynchronousTask(void)
{
  ETCB *task = (ETCB *)_OSActiveTask;
  FIFOQUEUE *eq = task->EventQueue;
  BOOL signaled;
  #if POWER_MANAGEMENT == DM_SLACK
     DMSlackCalculateSlack(_OSActiveTask,OSGetProcessorSpeed(),_OSGetActualTime());
  #endif
  /* Insert the task into the event queue so that a task signaling the event will de-
  ** tect this task. When EnqueueEventTask() returns TRUE, there is a pending event and
  ** the task is not appended to the queue: it is then rescheduled by the timer inter-
  ** rupt handler, since the information it needs can be updated concurrently by that
  ** handler. Interrupts are masked from here until the task has left the ready queue: a
  ** signal in between, from an interrupt or from a task of higher priority preempting
  ** this one, gave the handler a task not yet a zombie, still in the ready queue and
  ** maybe deep in the stack, which the handler inserted in the ready queue again, or,
  ** taking it for the task it had interrupted, whose context it discarded instead of
  ** the right one (SoakPico, 2026-09-25). Masked, the task is a zombie out of the ready
  ** queue by the time the handler sees it. */
  _OSDisableInterrupts();
  signaled = EnqueueEventTask(eq,(UINTPTR)task);
  /* Set task to zombie to indicate that the task is about to remove itself from the
  ** ready queue and that its context should not be saved. */
  _OSActiveTask->TaskState |= STATE_ZOMBIE;
  _OSNoSaveContext = TRUE;    // Don't save the context of this task
  CompilerBarrier();          // a zombie before it leaves the ready queue
  #if POWER_MANAGEMENT == DRA || POWER_MANAGEMENT == DR_OTE
     /* Update the timing used up by this task */
     DRASimUpdateElapseTime(_OSGetActualTime());
  #endif
  /* Remove the task from the ready queue */
  _OSQueueHead->Next[READYQ] = _OSActiveTask->Next[READYQ];
  _OSActiveTask = _OSQueueHead->Next[READYQ];
  if (signaled)
     EnqueueRescheduleQueue(task);
  _OSEnableInterrupts();
  #if POWER_MANAGEMENT == DM_SLACK
     /* Set the available slack for a lower priority task. */
     DMSlackUpdateSlack();
  #endif
  #if POWER_MANAGEMENT != NONE
     /* Adjust the processor speed for power management. There is no point in changing the
     ** current speed when switching to the idle task. */
     if (_OSActiveTask != OSQueueTail) {
        #if POWER_MANAGEMENT == DRA || POWER_MANAGEMENT == DR_OTE
           LastRemainingWorkUpdate = DRASimTime;
           OSSetProcessorSpeed(GetProcessorSpeed(LastRemainingWorkUpdate));
        #elif POWER_MANAGEMENT == OTE
           LastRemainingWorkUpdate = _OSGetActualTime();
           OSSetProcessorSpeed(GetProcessorSpeed(LastRemainingWorkUpdate));
        #else /* POWER_MANAGEMENT == DM_SLACK */
           LastRemainingWorkUpdate = _OSGetActualTime();
           OSSetProcessorSpeed(GetProcessorSpeed(LastRemainingWorkUpdate));
        #endif
     }
  #elif defined(STATIC_POWER_MANAGEMENT)
     if (_OSActiveTask != OSQueueTail)
        OSSetProcessorSpeed(_OSActiveTask->FrequencyIndex);
  #endif
  _OSScheduleTask();
} /* end of OSSuspendSynchronousTask */


/* OSScheduleSuspendedTask: Removes a blocked task that is waiting for an event and sche-
** dules it. */
void OSScheduleSuspendedTask(void *eq)
{
  ETCB *etcb;
  if ((etcb = DequeueEventTask((FIFOQUEUE *)eq)) != NULL)
     EnqueueRescheduleQueue(etcb);
} /* end of OSScheduleSuspendedTask */


/* NonMaskableSoftwareTimer, when a port needs it, comes from its Escapement_Timer.h. */
#ifndef NonMaskableSoftwareTimer
   /* EnqueueRescheduleQueue: Inserts an aperiodic ETCB into a list of work that needs to be
   ** processed by the timer handler (see EmptyRescheduleSynchronousTaskList). This indirect
   ** scheduling of aperiodic tasks is necessary because it is possible that the timer hand-
   ** ler receives an interrupt and performs a shift of all temporal variables while this
   ** scheduling is taking place. Note that the timer handler is invoked by forcing an in-
   ** terrupt. */
   void EnqueueRescheduleQueue(ETCB *etcb)
   {
     while (TRUE) {
        etcb->Next[BLOCKQ] = (ETCB *)OSUINTPTR_LL((UINTPTR *)&RescheduleSynchronousTaskList);
        if (OSUINTPTR_SC((UINTPTR *)&RescheduleSynchronousTaskList,(UINTPTR)etcb)) {
           _OSGenerateSoftTimerInterrupt(); // Generate a soft timer interrupt
           return;
        }
     }
   } /* end of EnqueueRescheduleQueue */
#else
   /* EnqueueRescheduleQueueBeforeBoot: Same as EnqueueRescheduleQueue but without gene-
   ** rating an interrupt to schedule the signaled task. This is needed so that the task
   ** is not executed when Escapement's timer has not yet started. This function is only
   ** called prior to starting the timer and inserts the task to be scheduled in the
   ** temporary list RescheduleSynchronousTaskList. As soon as the timer is started and
   ** enters its first processing, this list is emptied with the first timer interrupt
   ** generated by the Idle task. */
   void EnqueueRescheduleQueueBeforeBoot(ETCB *etcb)
   {
     while (TRUE) {
        etcb->Next[BLOCKQ] = (ETCB *)OSUINTPTR_LL((UINTPTR *)&RescheduleSynchronousTaskList);
        if (OSUINTPTR_SC((UINTPTR *)&RescheduleSynchronousTaskList,(UINTPTR)etcb))
           return;
     }
   } /* end of EnqueueRescheduleQueueBeforeBoot */

   /* EnqueueRescheduleQueueAfterBoot: Identical to EnqueueRescheduleQueue. This is the
   ** function that is called after having started Escapement's timer. This function is
   ** needed so that a function pointer can be defined to change between the initializa-
   ** tion phases while using the same global name. */
   void EnqueueRescheduleQueueAfterBoot(ETCB *etcb)
   {
     while (TRUE) {
        etcb->Next[BLOCKQ] = (ETCB *)OSUINTPTR_LL((UINTPTR *)&RescheduleSynchronousTaskList);
        if (OSUINTPTR_SC((UINTPTR *)&RescheduleSynchronousTaskList,(UINTPTR)etcb)) {
           _OSGenerateSoftTimerInterrupt(); // Generate a soft timer interrupt
           return;
        }
     }
   } /* end of EnqueueRescheduleQueueAfterBoot */
#endif


/* EmptyRescheduleSynchronousTaskList: This function schedules all aperiodic tasks that
** are in a temporary LIFO list by placing them either in the ready queue or in the arri-
** val queue if the task hasn't finished its previous load imposed on the processor. */
#if POWER_MANAGEMENT == DRA || POWER_MANAGEMENT == DR_OTE
BOOL EmptyRescheduleSynchronousTaskList(INT32 currentTime, BOOL doSimUpdateElapseTime)
#else
void EmptyRescheduleSynchronousTaskList(INT32 currentTime)
#endif
{
  BOOL wait;
  ETCB *etcb;
  do {
     while ((etcb = (ETCB *)OSUINTPTR_LL((UINTPTR *)&RescheduleSynchronousTaskList)) != NULL) {
        if (OSUINTPTR_SC((UINTPTR *)&RescheduleSynchronousTaskList,(UINTPTR)etcb->Next[BLOCKQ])) {
           /* A task not yet a zombie has not left the ready queue: OSSuspendSynchronous-
           ** Task asks for a task to be rescheduled only once it has, interrupts masked. */
           #ifdef DEBUG_MODE
              if ((etcb->TaskState & STATE_ZOMBIE) == 0)
                 while (TRUE);
           #endif
           #if SCHEDULER_REAL_TIME_MODE != DEADLINE_MONOTONIC_SCHEDULING
              /* Under EDF, the task that is to process the event cannot execute until its
              ** previous deadline has passed. */
              wait = currentTime < etcb->NextDeadline;
           #else
              /* Under deadline monotonic scheduling, the task that is to process the
              ** event cannot execute until it has finished its previous period. */
              wait = currentTime < etcb->NextArrivalTimeLow;
           #endif
           if (wait) {
              #if SCHEDULER_REAL_TIME_MODE != DEADLINE_MONOTONIC_SCHEDULING
                 etcb->NextArrivalTimeLow = etcb->NextDeadline;
              #endif
              ArrivalQueueInsert((TCB *)etcb);
           }
           else {
              etcb->TaskState = TASKTYPE_BLOCKING;
              #if SCHEDULER_REAL_TIME_MODE != DEADLINE_MONOTONIC_SCHEDULING
                 etcb->NextDeadline = GetSuspendedSchedulingDeadline(etcb,currentTime);
              #else
                 etcb->NextArrivalTimeLow = currentTime + etcb->PeriodLow;
              #endif
              #if SCHEDULER_REAL_TIME_MODE == EARLIEST_DEADLINE_FIRST_STAR
                 /* Released now, as the timer handler sets it for the tasks it releases
                 ** from the arrival queue: EDF* breaks ties between equal deadlines on it. */
                 etcb->CurrentArrivalTimeLow = currentTime;
              #endif
              ReadyQueueInsert((TCB *)etcb);
              #if POWER_MANAGEMENT == DRA || POWER_MANAGEMENT == DR_OTE
                 etcb->CompletionTime = etcb->WCET;
                 /* Before modifying the simulation queue, we need to update it when the head
                 ** of the queue can change, otherwise the elapse time will be decremented for
                 ** the wrong task. */
                 if (doSimUpdateElapseTime && _OSQueueHead->Next[READYQ] != _OSActiveTask) {
                    DRASimUpdateElapseTime(currentTime);
                    doSimUpdateElapseTime = FALSE;
                 }
                 DRASimQueueInsert((TCB *)etcb);
              #elif POWER_MANAGEMENT == DM_SLACK
                etcb->RemainingWork = etcb->WCET;
              #endif
           }
        }
     }
  /* The list was seen empty: the SC confirms that no task was pushed since that LL, else
  ** the loop runs again. */
  } while (!OSUINTPTR_SC((UINTPTR *)&RescheduleSynchronousTaskList,NULL));
  #if POWER_MANAGEMENT == DRA || POWER_MANAGEMENT == DR_OTE
     return doSimUpdateElapseTime;
  #endif
} /* end of EmptyRescheduleSynchronousTaskList */


#if POWER_MANAGEMENT != NONE
/* UpdateRemainingWork: Updates the work that is still to be done by a task when it is
** preempted by another task. This function should be called only when the currently ac-
** tive periodic task may resume its execution, i.e., when the timer function dispatches
** another task. Note that this function is not concurrent. */
void UpdateRemainingWork(TCB *task, UINT8 currentSpeed, INT32 newTime)
{
  INT32 completed = newTime - LastRemainingWorkUpdate;
  if (currentSpeed != OS_MAX_SPEED)
     completed = Slowdown(completed,currentSpeed);
  task->RemainingWork -= completed;
  LastRemainingWorkUpdate = newTime;
} /* end of UpdateRemainingWork */
#endif


#if POWER_MANAGEMENT == DM_SLACK
   /* When a task instance finishes its execution, the computation of the slack it leaves
   ** behind for lower priority tasks is done in two steps. The task computes and stores
   ** the results into temporary variables in a first step, and then later, in a second
   ** step, it or the timer ISR transforms these temporaries into the true variables. */
   static UINT8 DMSlackInterrupt = FALSE;     // True if temporaries should be transformed
   static INT32 DMTmpRemaindingWork;          // New slack value
   static UINT8 DMTmpPriority;                // Priority of the task that left the slack
   static INT32 DMTmpLastRemainingWorkUpdate; // New starting interval for updates
#endif


#if POWER_MANAGEMENT == DM_SLACK
/* DMSlackCalculateSlack: Calculates the amount of slack that is left by a terminating
** task. */
void DMSlackCalculateSlack(TCB *task, UINT8 currentSpeed, INT32 newTime)
{
  INT32 dmRemaindingWork;
  /* The LL/SC pair on DMSlackInterrupt makes the three stores one unit with the flag: a
  ** timer interrupt in between, which may consume them, makes the SC fail and the values
  ** are computed again. */
  do {
     OSUINT8_LL(&DMSlackInterrupt);
     dmRemaindingWork = newTime - LastRemainingWorkUpdate;
     if (currentSpeed != OS_MAX_SPEED)
        dmRemaindingWork = Slowdown(dmRemaindingWork,currentSpeed);
     if ((DMTmpRemaindingWork = task->RemainingWork - dmRemaindingWork) < 0)
        DMTmpRemaindingWork = 0;
     DMTmpLastRemainingWorkUpdate = newTime;
     DMTmpPriority = task->Priority;
  } while (!OSUINT8_SC(&DMSlackInterrupt,TRUE));
} /* end of DMSlackCalculateSlack */
#endif


#if POWER_MANAGEMENT == DM_SLACK
/* DMSlackUpdateSlack: Finalizes the values calculated in DMSlackCalculateSlack into the
** global variables they refer to.
** Returned value: (BOOL) FALSE when it installed a pending slack, TRUE when there was
** none. */
BOOL DMSlackUpdateSlack(void)
{
  if (DMSlackInterrupt) {
     DMSlackAmount = DMTmpRemaindingWork;
     DMSlackPriority = DMTmpPriority;
     LastRemainingWorkUpdate = DMTmpLastRemainingWorkUpdate;
     DMSlackInterrupt = FALSE;
     return FALSE;
  }
  return TRUE;
} /* end of DMSlackUpdateSlack */
#endif


#if SCHEDULER_REAL_TIME_MODE != DEADLINE_MONOTONIC_SCHEDULING
/* GetSuspendedSchedulingDeadline: Updates the next deadline of a synchronous task. This
** function is used only for EDF scheduling. Note that this function is not concurrent
** and is only called by the timer service routine. */
INT32 GetSuspendedSchedulingDeadline(ETCB *etcb, INT32 currentTime)
{
  if (SynchronousTaskDeadlines > currentTime)
     SynchronousTaskDeadlines += etcb->WorkLoad;
  else
     SynchronousTaskDeadlines = currentTime + etcb->WorkLoad;
  return SynchronousTaskDeadlines;
} /* end of GetSuspendedSchedulingDeadline */
#endif


#if POWER_MANAGEMENT == DRA || POWER_MANAGEMENT == DR_OTE
/* DRASimUpdateElapseTime: Updates the simulated tasks and the aperiodic excess time.
** This function should be called prior to setting the processor speed using dynamic recla-
** mation algorithm for power management.
** Parameter: (INT32) newTime: new update time, i.e. brings the values in the simulation
**            queue up to this time.
** Note: Before calling this function, there should be no pending interruptible CAS2
** operations. */


void DRASimUpdateElapseTime(INT32 newTime)
{
  INT32 oldTime, completed, newExcess, oldExcess;
  TCB *simTask;
  if (SynchronousTaskList != NULL) {
     /* Update the excess created by not having a scheduled aperiodic task. */
     oldTime = AperiodicExcessTime;             // Last aperiodic excess update time
     oldExcess = AperiodicExcess;
     if (newTime > SynchronousTaskDeadlines) {  // Any excess?
        if (oldTime < SynchronousTaskDeadlines)
           completed = newTime - SynchronousTaskDeadlines;
        else
           completed = newTime - oldTime;
        newExcess = ScaleBy256ths(completed,AperiodicUtilization) + oldExcess;
     }
     else
        newExcess = 0;
     InterruptibleINT32CAS2(&AperiodicExcessTime,oldTime,newTime,
                                             &AperiodicExcess,oldExcess,newExcess,FALSE);
     /* Advance DRASimTime by first decreasing the excess caused by aperiodic tasks. */
     oldTime = DRASimTime;         // Last excess update time
     completed = newTime - oldTime;
     if (newExcess > 0) {
        if (newExcess > completed) {
           oldExcess = newExcess - completed;
           InterruptibleINT32CAS2(&DRASimTime,oldTime,newTime,
                                             &AperiodicExcess,newExcess,oldExcess,FALSE);
           return;
        }
        else {
           INT32 tmp = oldTime + newExcess;
           InterruptibleINT32CAS2(&DRASimTime,oldTime,tmp,&AperiodicExcess,newExcess,0,FALSE);
           oldTime = tmp; 
           completed -= newExcess;
        }
     }
  }
  else {
     oldTime = DRASimTime;           // Last excess update time
     completed = newTime - oldTime;  // Time interval since last update
  }
  /* Get the amount of work done up to the actual time that needs to be deducted from the
  ** WCET of the tasks. */
  while (completed > 0) {
     if ((simTask = _OSQueueHead->NextSim) == OSQueueTail) {
        /* Nothing left to simulate: the simulation clock catches up with the wall clock,
        ** unless an interrupting call already moved it. A store-conditional can fail
        ** although DRASimTime did not change, so only a changed value gives up. */
        do {
           if (OSINT32_LL(&DRASimTime) != oldTime)
              return;
        } while (!OSINT32_SC(&DRASimTime,newTime));
        break;
     }
     if ((newExcess = simTask->CompletionTime) > completed) { // Got some remaining slack
        InterruptibleINT32CAS2(&DRASimTime,oldTime,newTime,
                           &simTask->CompletionTime,newExcess,newExcess-completed,FALSE);
        break;
     }
     completed -= newExcess;
     newExcess += oldTime;
     InterruptibleMixCAS2(&DRASimTime,oldTime,newExcess,
                          &_OSQueueHead->NextSim,simTask,simTask->NextSim,FALSE);
     oldTime = newExcess;
  }
} /* end of DRASimUpdateElapseTime */
#endif


#if POWER_MANAGEMENT == DRA || POWER_MANAGEMENT == DR_OTE
/* InterruptibleINT32CAS2: Two compare-and-stores, one after the other, whose operands and
** step reached are kept in static variables, so that code preempting the caller can
** complete a pending pair (check = TRUE) instead of undoing it: the caller, a timer
** interrupt or a task in OSEndTask or OSSuspendSynchronousTask, may never resume. The
** pair is not indivisible. Code that uses the variables of a pair must first complete
** any pending one. */
void InterruptibleINT32CAS2(INT32 *m1, INT32 e1, INT32 v1, INT32 *m2, INT32 e2, INT32 v2, BOOL check)
{
  static INT32 *memory[2];
  static INT32 expected[2];
  static INT32 newValue[2];
  static UINT8 phase = 2;
  if (phase == 2) {
     if (check) return;
     memory[0] = m1; memory[1] = m2;
     expected[0] = e1; expected[1] = e2;
     newValue[0] = v1; newValue[1] = v2;
     phase = 0;
  }
  for ( ; phase < 2; phase += 1)
     if (*memory[phase] == expected[phase])
        *memory[phase] = newValue[phase];
} /* end of InterruptibleINT32CAS2 */
#endif


#if POWER_MANAGEMENT == DRA || POWER_MANAGEMENT == DR_OTE
/* InterruptibleMixCAS2: Same as InterruptibleINT32CAS2 but with a CAS2 for an INT32 and
** an address. */
void InterruptibleMixCAS2(INT32 *m1, INT32 e1, INT32 v1, TCB **m2, TCB *e2, TCB *v2, BOOL check)
{
  static INT32 *memInt, expectedInt, newInt;
  static TCB **memTCB, *expectedTCB, *newTCB;
  static UINT8 phase = 2;
  if (phase == 2) {
     if (check) return;
     memInt = m1; memTCB = m2;
     expectedInt = e1; expectedTCB = e2;
     newInt = v1; newTCB = v2;
     phase = 0;
  }
  if (phase == 0) {
     if (*memInt == expectedInt)
        *memInt = newInt;
     phase = 1;
  }
  if (phase == 1) {
     if (*memTCB == expectedTCB)
        *memTCB = newTCB;
     phase = 2;
  }
} /* end of InterruptibleMixCAS2 */
#endif


#if POWER_MANAGEMENT != NONE || defined(STATIC_POWER_MANAGEMENT)
/* OSSetMinimalProcessorSpeed: Sets minimal processor speed.
** Parameter: (UINT8) one of the port's OS_xxMHZ_SPEED operating points, slowest first;
**            GetProcessorSpeed never chooses a slower one. */
void OSSetMinimalProcessorSpeed(UINT8 speed)
{
  #if POWER_MANAGEMENT != NONE
     MinimalProcessorSpeed = speed;
  #endif
} /* end of OSSetMinimalProcessorSpeed */ 
#endif


#if POWER_MANAGEMENT != NONE
/* GetProcessorSpeed: Returns the operating point (OS_xxMHZ_SPEED) to run the active task
** at, the slowest that still completes its remaining work in the time the policy grants
** it. Called whenever the speed must be adjusted dynamically. */
UINT8 GetProcessorSpeed(INT32 time)
{
  INT32 completionTime;
  #if POWER_MANAGEMENT != DRA
     INT32 tmp;
  #endif
  #if POWER_MANAGEMENT == DR_OTE
     INT32 oteCompletionTime;
  #endif
  INT8 speed;
  if ((_OSActiveTask->TaskState & TASKTYPE_BLOCKING) == 0) {
     #if POWER_MANAGEMENT == OTE
        /* OTE: when the active task is the only one ready, it may stretch its remaining
        ** work up to the earliest of the next periodic arrival, the next event-driven
        ** release and its own deadline. */
        if (_OSActiveTask->Next[READYQ] == OSQueueTail) {
           if (_OSQueueHead->Next[ARRIVALQ] != OSQueueTail)
              completionTime = _OSQueueHead->Next[ARRIVALQ]->NextArrivalTimeLow;
           else
              completionTime = ShiftTimeLimit - time;
           if (SynchronousTaskList != NULL && 
               (tmp = GetEarliestAperiodicArrival()) < completionTime) {
              if (tmp <= time)
                 #ifdef STATIC_POWER_MANAGEMENT
                    return _OSActiveTask->FrequencyIndex;
                 #else
                    return OS_MAX_SPEED;
                 #endif
              else
                 completionTime = tmp;
           }
           if (completionTime > _OSActiveTask->NextDeadline)
              completionTime = _OSActiveTask->NextDeadline;
           completionTime -= time;
           if (completionTime <= _OSActiveTask->RemainingWork)
              #ifdef STATIC_POWER_MANAGEMENT
                 return _OSActiveTask->FrequencyIndex;
              #else
                 return OS_MAX_SPEED;
              #endif
        }
        else
           #ifdef STATIC_POWER_MANAGEMENT
              return _OSActiveTask->FrequencyIndex;
           #else
              return OS_MAX_SPEED;
           #endif
     #elif POWER_MANAGEMENT == DR_OTE
        /* Check if we can apply OTE: Get next arrival time and correct for deadlines that
        ** are smaller than periods. */
        completionTime = GetDRASlackTime() + AperiodicExcess;
        if (_OSActiveTask->Next[READYQ] == OSQueueTail) {
           if (_OSQueueHead->Next[ARRIVALQ] != OSQueueTail)
              oteCompletionTime = _OSQueueHead->Next[ARRIVALQ]->NextArrivalTimeLow;
           else
              oteCompletionTime = ShiftTimeLimit - time;
           if (SynchronousTaskList != NULL && (tmp = GetEarliestAperiodicArrival()) < oteCompletionTime) {
              if (tmp <= time)
                 #ifdef STATIC_POWER_MANAGEMENT
                    return _OSActiveTask->FrequencyIndex;
                 #else
                    return OS_MAX_SPEED;
                 #endif
              else
                 oteCompletionTime = tmp;
           }           
           if (oteCompletionTime > _OSActiveTask->NextDeadline)
              oteCompletionTime = _OSActiveTask->NextDeadline;
           oteCompletionTime -= time;
           if (completionTime < oteCompletionTime)
              completionTime = oteCompletionTime;
           if (completionTime <= _OSActiveTask->RemainingWork)
              #ifdef STATIC_POWER_MANAGEMENT
                 return _OSActiveTask->FrequencyIndex;
              #else
                 return OS_MAX_SPEED;
              #endif
        }
     #elif POWER_MANAGEMENT == DRA
        completionTime = GetDRASlackTime() + AperiodicExcess;
     #else /* POWER_MANAGEMENT == DM_SLACK */
        /* Check if we can apply OTE: Get next arrival time and correct for deadlines that
        ** are smaller than periods. */
        if (_OSActiveTask->Next[READYQ] == OSQueueTail) {
           if (_OSQueueHead->Next[ARRIVALQ] != OSQueueTail)
              completionTime = _OSQueueHead->Next[ARRIVALQ]->NextArrivalTimeLow;
           else
              completionTime = ShiftTimeLimit - time;
           if (SynchronousTaskList != NULL && (tmp = GetEarliestAperiodicArrival()) < completionTime) {
              if (tmp <= time)
                 #ifdef STATIC_POWER_MANAGEMENT
                    return _OSActiveTask->FrequencyIndex;
                 #else
                    return OS_MAX_SPEED;
                 #endif
              else
                 completionTime = tmp;
           }
           if (completionTime > _OSActiveTask->NextDeadline)
              completionTime = _OSActiveTask->NextDeadline;
           completionTime -= time;
           if (completionTime <= _OSActiveTask->RemainingWork)
              #ifdef STATIC_POWER_MANAGEMENT
                 return _OSActiveTask->FrequencyIndex;
              #else
                 return OS_MAX_SPEED;
              #endif
        }
        /* The slack goes to tasks of lower priority than its owner, a larger number:
        ** only they counted the owner's WCET in their response time. A task of higher
        ** priority never did, and given the slack it could miss its deadline. */
        else if (_OSActiveTask->Priority > DMSlackPriority && DMSlackAmount > 0)
           completionTime = DMSlackAmount + _OSActiveTask->RemainingWork;
        else
           #ifdef STATIC_POWER_MANAGEMENT
              return _OSActiveTask->FrequencyIndex;
           #else
              return OS_MAX_SPEED;
           #endif
     #endif
     /* Find the speed to apply to the task: the slowest that does the work left in the
     ** time given. The work is an integer, so it exceeds the rounded-down product exactly
     ** when it exceeds the product itself: this is the comparison of RemainingWork << 8
     ** with the ratio times completionTime, without the overflow of that product. */
     #ifdef STATIC_POWER_MANAGEMENT
        speed = _OSActiveTask->FrequencyIndex;  // first frequency setting
     #else
        speed = OS_MAX_SPEED;                      // first frequency setting
     #endif
     /* A task past its WCET has no work left on record, which the slowest speed would
     ** do; the work it still has is unknown, and the fastest ends it soonest. */
     if (_OSActiveTask->RemainingWork <= 0)
        return speed;
     while ((speed -= 1) >= MinimalProcessorSpeed)
        if (_OSActiveTask->RemainingWork > Slowdown(completionTime,speed))
           return speed + 1;
     return MinimalProcessorSpeed;
  }
  else
     #ifdef STATIC_POWER_MANAGEMENT
        return _OSActiveTask->FrequencyIndex;
     #else
        return OS_MAX_SPEED;
     #endif
} /* end of GetProcessorSpeed */
#endif


#if POWER_MANAGEMENT == OTE || POWER_MANAGEMENT == DR_OTE || POWER_MANAGEMENT == DM_SLACK
/* GetEarliestAperiodicArrival: Returns the time that the next aperiodic task may begin
** its execution. */
INT32 GetEarliestAperiodicArrival(void)
{
  ETCB *etcb;
  INT32 minArrivalTime = INT32_MAX;
  for (etcb = SynchronousTaskList; etcb != NULL; etcb = etcb->NextETCB)
     #if SCHEDULER_REAL_TIME_MODE != DEADLINE_MONOTONIC_SCHEDULING
        if (etcb->NextDeadline < minArrivalTime)
           minArrivalTime = etcb->NextDeadline;
     #else
        if (etcb->NextArrivalTimeLow < minArrivalTime)
           minArrivalTime = etcb->NextArrivalTimeLow;
     #endif
  return minArrivalTime;
} /* end of GetEarliestAperiodicArrival */
#endif


#if POWER_MANAGEMENT == DRA || POWER_MANAGEMENT == DR_OTE
/* GetDRASlackTime: When power management uses the dynamic reclaiming algorithm, the
** ready queue is simulated so that the slack time produced by an excess of the task in-
** stance WCET can be given to other instances in the ready queue. Each instance TCB has
** a CompletionTime field that holds the remaining execution of the instance. When the
** simulated value is greater than zero and that instance has completed, this time excess
** can be given to other instances. Because the simulated ready queue is an exact mimic
** of the ready queue, when scheduling a task instance that is at the head of the ready
** queue, if that instance is not at the head of the simulated ready queue, then the sum
** of all completionTime fields before it constitutes the instance's slack time.
** GetDRASlackTime returns this value augmented by the instance completion time. In other
** words, this function returns the total time that may be allotted to complete the in-
** stance.
** An instance still running once the simulation has used up its WCET is no longer in the
** simulated queue: it overran its WCET, or ended with it at the instant a timer interrupt
** took it out. The search then stops at the tail rather than follow its null link: no
** time is left to give, and the task goes on at the fastest speed. */
INT32 GetDRASlackTime(void)
{
  TCB *task;
  INT32 completionTime = _OSActiveTask->CompletionTime;
  for (task = _OSQueueHead; (task = task->NextSim) != _OSActiveTask; ) {
     if (task == OSQueueTail)
        return 0;
     completionTime += task->CompletionTime;
  }
  return completionTime;
} /* end of GetDRASlackTime */


/* DRASimQueueInsert: Inserts a new instance into the simulation queue, in the order of
** the ready queue. Its TCB can still hold the previous instance, when the simulation has
** not yet used up the WCET of that one: the TCB is then taken out on the way, which it
** always is before the place of the new instance, whose deadline is later. Inserting it
** as it stood would link it twice and loop the queue. Only the timer interrupt calls
** this function. */
void DRASimQueueInsert(TCB *newNode)
{
  TCB *right, *left = _OSQueueHead;
  while (TRUE) {
     if ((right = left->NextSim) == newNode) {
        left->NextSim = newNode->NextSim;
        continue;
     }
     if (right == OSQueueTail || ReadyQueueInsertTestKey(newNode,right))
        break;
     left = right;
  }
  newNode->NextSim = right;
  left->NextSim = newNode;
} /* end of DRASimQueueInsert */
#endif


/* OSStartMultitasking: This function is called only once and after the application tasks
** have all been defined in the application. This function never returns and schedules
** all user tasks. When these terminate, an idle task runs with the lowest priority to
** keep the processor busy until the next task arrives. This function also creates a
** timer handler when there are tasks to be scheduled. */
BOOL OSStartMultitasking(void (*f)(void *), void *arg)
{
  ETCB *etcb;
  FIFOQUEUE *eq;
  UINT8 i;
  #if SCHEDULER_REAL_TIME_MODE == DEADLINE_MONOTONIC_SCHEDULING
     TCB *tcb;
  #endif
  /* Assert that Escapement begins with disabled maskable interruptions. */
  _OSDisableInterrupts();
  if (_OSQueueHead == NULL)
     Initialize();
  /* The first context switch starts the stack at its base, which the first call to
  ** OSMalloc sets: an application that allocated nothing would otherwise start it at 0. */
  (void)OSMalloc(0);
  /* Create the FIFO array of tasks for all created event descriptors. */
  for (etcb = SynchronousTaskList; etcb != NULL; etcb = etcb->NextETCB) {
     if (etcb->EventQueue->Q == NULL) {
        eq = etcb->EventQueue;
        if ((eq->Q = (UINTPTR *)OSMalloc(eq->QueueLength*sizeof(UINTPTR))) == NULL)
           return FALSE;
        for (i = 0; i < eq->QueueLength; i += 1)
           eq->Q[i] = NULL;
        eq->MaxIndex = GetFIFOArrayMaxIndex(eq->QueueLength);
     }
     EnqueueEventTask(etcb->EventQueue,(UINTPTR)etcb);
  }
  #if SCHEDULER_REAL_TIME_MODE == DEADLINE_MONOTONIC_SCHEDULING
     /* Reset the arrival time of each periodic task to its first arriving instance. */
     for (tcb = _OSQueueHead->Next[ARRIVALQ]; tcb != OSQueueTail; tcb = tcb->Next[ARRIVALQ])
        tcb->NextArrivalTimeLow = 0;
  #endif
  /* There are periodic tasks in the system when the arrival queue is not empty. Note
  ** that periodic tasks are initially inserted in this queue before starting Escapement. */
  if (_OSQueueHead->Next[ARRIVALQ] != OSQueueTail || SynchronousTaskList != NULL)
     /* Prepare the timer; it begins scheduling arrivals when the idle task first runs
     ** (_OSStartTimer). */
     _OSInitializeTimer();
  /* Start the first task in the ready queue, i.e. the Idle task. */
  _OSActiveTask = _OSQueueHead->Next[READYQ];
  /* Finalize the application initializations if any */
  if (f != NULL) f(arg);
  _OSScheduleTask();     // Start the Idle task
  _OSEnableInterrupts(); // _OSScheduleTask only pended PendSV: the switch to the idle
  return FALSE;          // task is taken here, and never returns
} /* end of OSStartMultitasking */


/* WAITFREE FIFO QUEUE IMPLEMENTATION THAT IS USED INTERNALLY */
/* Addresses in a waitfree or non-blocking algorithm can also contain a marker so that an
** atomic operation can be done. The following define macros to test, insert and remove a
** marker. Note that these macros could have been defined as in-line functions but macros
** are as simple.
** On Cortex-M the marker is the most significant bit (MARKEDBIT, Escapement_CortexMx.h):
** every marked address must lie below 0x80000000, as RAM does. */

/* IsMarkedReference: Determines whether an address is marked.
** Parameter: Address to test.
** Returned value: Non-zero for TRUE and 0 for FALSE. */
#define IsMarkedReference(node) ((UINTPTR)node & MARKEDBIT)

/* GetMarkedReference: Inserts a marker into an address and returns it.
** Parameter: Address to mark.
** Returned value: Marked address. */
#define GetMarkedReference(node) (UINTPTR)((UINTPTR)node | MARKEDBIT)

/* GetUnmarkedReference: Returns a valid address from a marked one. When using an address
** that can be marked, it is necessary to first remove the marker before referring to
** that address.
** Parameter: An address, usually a node in some data structure.
** Returned value: (UINTPTR) a valid address. */
#define GetUnmarkedReference(node) (UINTPTR)((UINTPTR)node & UNMARKEDBIT)


/* GetFIFOArrayMaxIndex: Returns the value at which Head and Tail wrap around: the smallest
** multiple of the array size S not below 0xFFFF - S. Being a multiple of S keeps index % S
** continuous across the wrap, and 0xFFFF itself is never reached: it marks an index not
** yet read in the operation descriptors (FIFODequeue, FIFOEnqueue). */
UINT16 GetFIFOArrayMaxIndex(UINT16 queueSize)
{
  UINT16 tmp, maxIndex = 0xFFFF - queueSize;
  if ((tmp = maxIndex % queueSize) != 0)
     maxIndex += queueSize - tmp;
  return maxIndex;
} /* end of GetFIFOArrayMaxIndex */


/* FIFODequeue: Returns and dequeues the first item of a FIFO queue. If there is no such
** item, i.e. the queue is empty at the time of the call, and the signal marker (used to
** implement event queues) is passed as parameter, the marker is inserted into the array
** entry that will be read by the next enqueue operation.
** Parameters:
**   (1) (FIFOQUEUE *) array-based queue descriptor;
**   (2) (UINTPTR) signal marker: can be NULL or SIGNAL.
** Return value: (UINTPTR) the oldest enqueued item or NULL if none exist. */
UINTPTR FIFODequeue(FIFOQUEUE *queue, UINTPTR signal)
{
  DEQUEUE_DESCRIPTOR des;
  void *op;
  if (queue == NULL || queue->Q == NULL) // Test if fifo is initialized
     return NULL;
  /* Initialize the fields of the descriptor to uninitialized markers. */
  des.HeadPropose = des.Head = 0xFFFF;
  /* The descriptor points to itself: cppcheck takes the address of a structure whose
  ** other fields are set on the lines around for a read of them. */
  // cppcheck-suppress uninitvar
  des.SlotPropose = des.SlotReturn = (UINTPTR)&des;
  des.Done = FALSE;
  /* Complete the pending operation before the current dequeue. */
  if ((op = queue->PendingOp) != NULL) { // Is an operation pending?
     if (IsMarkedReference(op))          // Is pending operation a dequeue?
        FIFODequeueHelper(queue,signal,(DEQUEUE_DESCRIPTOR *)GetUnmarkedReference(op));
     else
        FIFOEnqueueHelper(queue,(ENQUEUE_DESCRIPTOR *)op);
  }
  /* Post current dequeue operation and do it: an interrupt that helps it must find the
  ** descriptor written, and the queue is read only once the operation is posted. */
  CompilerBarrier();
  queue->PendingOp = (void *)GetMarkedReference(&des);
  CompilerBarrier();
  FIFODequeueHelper(queue,signal,&des);  // Do the operation
  CompilerBarrier();
  queue->PendingOp = NULL;               // Assert that no other task does this operation
  if (des.SlotReturn == (UINTPTR)SIGNAL) // Was there already a signal?
     return NULL;                        // Do not return the signal
  else
     return des.SlotReturn;              // Returns NULL or the dequeued item
} /* end of FIFODequeue */


/* FIFODequeueHelper: Performs a pending dequeue on the FIFO array. If there is no item
** to dequeue, a special marker (SIGNAL) can be inserted into the slot pointed by Tail
** (=Head) so that the next posted enqueue operation receives the signal and does not add
** the item it would otherwise enqueue. This feature is used in conjunction with events
** so that a dequeue operation posts an event or dequeues the blocked task waiting for
** the event.
** Parameters:
**   (1) (FIFOQUEUE *) array-based queue descriptor;
**   (2) (UINTPTR) signal marker: can be NULL or SIGNAL;
**   (3) (DEQUEUE_DESCRIPTOR *) parameters of pending dequeue operation.
** Return value: None, but the result of the operation is stored in field SlotReturn. */
void FIFODequeueHelper(FIFOQUEUE *queue, UINTPTR signal, DEQUEUE_DESCRIPTOR *des)
{
  UINT16 h;
  UINTPTR slot;
  if (des->HeadPropose == 0xFFFF)
     des->HeadPropose = queue->Head;
  if (des->Head == 0xFFFF)
     des->Head = des->HeadPropose;
  h = des->Head % queue->QueueLength;
  if (des->SlotPropose == (UINTPTR)des)
     des->SlotPropose = queue->Q[h];
  if (des->SlotReturn == (UINTPTR)des)
     des->SlotReturn = des->SlotPropose;
  if (des->SlotReturn != signal)
     while (TRUE) {
        slot = OSUINTPTR_LL(&queue->Q[h]);
        if (des->Done)
           break;
        else if (slot == SIGNAL) {
           /* The signal this dequeue left: mark it done, or a helper coming later, once
           ** an enqueue has taken the signal, would leave a second one
           ** (test/model/fifo.py). */
           des->Done = TRUE;
           break;
        }
        else if (slot == NULL)
           if (des->SlotReturn == NULL) {
              if (OSUINTPTR_SC(&queue->Q[h],SIGNAL)) {
                 des->Done = TRUE;
                 break;
              }
           }
           else {
              IncrementFifoQueueIndex(&queue->Head,des->Head,queue->MaxIndex);
              des->Done = TRUE;
              break;
           }
        else if (OSUINTPTR_SC(&queue->Q[h],NULL)) {
           IncrementFifoQueueIndex(&queue->Head,des->Head,queue->MaxIndex);
           des->Done = TRUE;
           break;
        }
     }
} /* end of FIFODequeueHelper */


/* FIFOEnqueue: Inserts an item into a FIFO array-based queue. If the array entry holds
** the optional SIGNAL marker, the queue was empty when a dequeue operation occurred and
** the item is not inserted into the queue. This feature is used together with events so
** that a task is not inserted into an event queue when it is already signaled.
** Parameters:
**   (1) (FIFOQUEUE *) array-based queue descriptor;
**   (2) (UINTPTR) signal marker: can be NULL or SIGNAL;
**   (3) (UINTPTR) item to enqueue.
** Returned value: (BOOL) TRUE when the tail slot held the marker passed as parameter:
**    with SIGNAL, the queue held a signal and the item was not inserted; with NULL, the
**    slot was free and the item was inserted. FALSE otherwise: with SIGNAL, the item was
**    inserted; with NULL, the queue was full. */
BOOL FIFOEnqueue(FIFOQUEUE *queue, UINTPTR signal, UINTPTR item)
{
  ENQUEUE_DESCRIPTOR des;
  void *op;
  /* Initialize all fields with uninitialized markers and insert the item to enqueue. */
  des.TailPropose = des.Tail = 0xFFFF;
  /* The descriptor points to itself: cppcheck takes the address of a structure whose
  ** other fields are set on the lines around for a read of them. */
  // cppcheck-suppress uninitvar
  des.SlotPropose = des.SlotReturn = (UINTPTR)&des;
  des.Item = item;
  des.Done = FALSE;
  /* Finish all pending operations before enqueueing the item. */
  if ((op = queue->PendingOp) != NULL) { // Is there any pending operations?
     if (IsMarkedReference(op))          // Is pending operation a dequeue?
        FIFODequeueHelper(queue,signal,(DEQUEUE_DESCRIPTOR *)GetUnmarkedReference(op));
     else
        FIFOEnqueueHelper(queue,(ENQUEUE_DESCRIPTOR *)op);
  }
  /* Post and do this enqueue operation, as FIFODequeue does. */
  CompilerBarrier();
  queue->PendingOp = &des;
  CompilerBarrier();
  FIFOEnqueueHelper(queue,&des);
  CompilerBarrier();
  queue->PendingOp = NULL;           // Assert that no other task does this operation
  return des.SlotReturn == signal;   // Extract the value to return.
} /* end of FIFOEnqueue */


/* FIFOEnqueueHelper: Performs a pending enqueue in the array-based FIFO queue.
** Parameters:
**   (1) (FIFOQUEUE *) array-based queue descriptor;
**   (2) (ENQUEUE_DESCRIPTOR *) parameters of pending enqueue operation.
** Return value: None, but the result of the operation is stored in field SlotReturn. */
void FIFOEnqueueHelper(FIFOQUEUE *queue, ENQUEUE_DESCRIPTOR *des)
{
  UINT16 t;
  UINTPTR slot;
  if (des->TailPropose == 0xFFFF)
     des->TailPropose = queue->Tail;
  if (des->Tail == 0xFFFF)
     des->Tail = des->TailPropose;
  t = des->Tail % queue->QueueLength;
  if (des->SlotPropose == (UINTPTR)des)
     des->SlotPropose = queue->Q[t];
  if (des->SlotReturn == (UINTPTR)des)
     des->SlotReturn = des->SlotPropose;
  if (des->SlotReturn == NULL || des->SlotReturn == SIGNAL)
     while (TRUE) {
        slot = OSUINTPTR_LL(&queue->Q[t]);
        if (des->Done)
           break;
        else if (slot == SIGNAL) {
           if (OSUINTPTR_SC(&queue->Q[t],NULL)) {
              des->Done = TRUE;
              break;
           }
        }
        else if (slot == NULL) {
           if (des->SlotReturn == SIGNAL) {
              des->Done = TRUE;
              break;
           }
           else if (OSUINTPTR_SC(&queue->Q[t],des->Item)) {
              IncrementFifoQueueIndex(&queue->Tail,des->Tail,queue->MaxIndex);
              des->Done = TRUE;
              break;
           }
        }
        else {
           IncrementFifoQueueIndex(&queue->Tail,des->Tail,queue->MaxIndex);
           des->Done = TRUE;
           break;
        }
     }
} /* end of FIFOEnqueueHelper */


/* IncrementFifoQueueIndex: Increments an index (Head or Tail) of a circular array-based
** FIFO queue.
** Parameters:
**   (1) (UINT16 *) address of the index to increment;
**   (2) (UINT16) current value of the index;
**   (3) (UINT16) value whereby the index wraps-around. */
void IncrementFifoQueueIndex(UINT16 *index, UINT16 oldValue, UINT16 moduloBase)
{
  UINT16 tmp = oldValue + 1;
  if (tmp == moduloBase)
     tmp = 0;
  while (OSUINT16_LL(index) == oldValue)
     if (OSUINT16_SC(index,tmp))
        break;
} /* end of IncrementFifoQueueIndex */


/* USER CONCURRENT FIFO QUEUE IMPLEMENTATION BASED ON THE INTERNAL WAITFREE QUEUE AND
** THAT CAN BE USED FOR INTER-TASK COMMUNICATIONS */
typedef struct NODE {      // User nodes stored into the FIFO queue
  UINT16 size;             // Encapsulated useful size of the node
  UINTPTR info;            // Starting field of the node's content
} NODE;
/* The application's part of a node starts at info, which the compiler aligns for a
** pointer: a word written there whole must not fault, as an unaligned one does on the
** Cortex-M0+. */
#define NODE_INFO_OFFSET __builtin_offsetof(NODE,info)

/* Its first fields are those of FIFOQUEUE, in the same order: OSEnqueueFIFO and
** OSDequeueFIFO hand it to FIFOEnqueue and FIFODequeue as one. */
typedef struct BUFFER_DESCRIPTOR_FIFO {
  NODE **Q;                // Array of queued items
  void *PendingOp;         // Posted operation for the queue (dequeue or enqueue op.)
  UINT16 Head, Tail;       // Current head and tail indices
  UINT16 MaxIndex;         // Value of Head or Tail at wrap-around
  UINT8 QueueLength;       // Circular array queue size
  FIFOQUEUE *FreeList;     // List of free blocks
} BUFFER_DESCRIPTOR_FIFO;


/* OSInitFIFOQueue: Creates a circular array-based FIFO queue that can be used for inter-
** task communication by the application. This function creates a FIFO descriptor with an
** initialized empty queue along with its initial pool of free buffers. */
void *OSInitFIFOQueue(UINT8 maxNodes, UINT8 maxNodeSize)
{
  UINT8 i;
  BUFFER_DESCRIPTOR_FIFO *desc;
  /* Initialize FIFO; a queue of no node would take every index modulo 0 */
  if (maxNodes == 0 ||
      (desc = (BUFFER_DESCRIPTOR_FIFO *)OSMalloc(sizeof(BUFFER_DESCRIPTOR_FIFO))) == NULL)
     return NULL;
  desc->Head = desc->Tail = 0;
  desc->QueueLength = maxNodes;
  desc->MaxIndex = GetFIFOArrayMaxIndex(maxNodes);
  desc->PendingOp = NULL;
  if ((desc->Q = (NODE **)OSMalloc(maxNodes * sizeof(NODE *))) == NULL)
     return NULL;
  for (i = 0; i < maxNodes; i++)
     desc->Q[i] = NULL;
  /* Create blocks and initialize the free list */
  if ((desc->FreeList = (FIFOQUEUE *)OSCreateEventDescriptor()) == NULL)
     return NULL;
  desc->FreeList->QueueLength = maxNodes;
  desc->FreeList->MaxIndex = GetFIFOArrayMaxIndex(maxNodes);
  if ((desc->FreeList->Q = (UINTPTR *)OSMalloc(maxNodes * sizeof(NODE *))) == NULL)
     return NULL;
  for (i = 0; i < maxNodes; i++)
     if ((desc->FreeList->Q[i] = (UINTPTR)OSMalloc(NODE_INFO_OFFSET + maxNodeSize)) == NULL)
        return NULL;
  return (void *)desc;
} /* OSInitFIFOQueue */


/* OSEnqueueFIFO: Inserts a node into a FIFO queue. The size parameter (user input) is
** stored into the node so that it can be transferred to the dequeuer task. */
BOOL OSEnqueueFIFO(void *queue, void *node, UINT16 size)
{
  NODE *tmpNode = (NODE *)(((UINTPTR)node) - NODE_INFO_OFFSET);
  tmpNode->size = size;
  return FIFOEnqueue((FIFOQUEUE *)queue,NULL,(UINTPTR)tmpNode);
} /* end of OSEnqueueFIFO */


/* OSDequeueFIFO: Retrieves a node from the FIFO queue. The size parameter (an output
** parameter) receives the value that was stored by the enqueue function. */
void *OSDequeueFIFO(void *queue, UINT16 *size)
{
  NODE *node;
  if ((node = (NODE *)FIFODequeue((FIFOQUEUE *)queue,NULL)) != NULL) {
     *size = node->size;
     node = (NODE *)&node->info;
  }
  return node;
} /* end of OSDequeueFIFO */


/* OSGetFreeNodeFIFO: Returns a node from the free pool of a FIFO queue. */
void *OSGetFreeNodeFIFO(void *descriptor)
{
  NODE *tmp = (NODE *)FIFODequeue(((BUFFER_DESCRIPTOR_FIFO *)descriptor)->FreeList,NULL);
  if (tmp == NULL)
     return NULL;
  else
     return &tmp->info;
} /* end of OSGetFreeNodeFIFO */


/* OSReleaseNodeFIFO: Inserts a node into the free pool of nodes associated with a FIFO
** queue. */
void OSReleaseNodeFIFO(void *descriptor, void *node)
{
  node = (void *)((UINTPTR)node - NODE_INFO_OFFSET);
  FIFOEnqueue(((BUFFER_DESCRIPTOR_FIFO *)descriptor)->FreeList,NULL,(UINTPTR)node);
} /* end OSReleaseNodeFIFO */


/* 3 and 4 SLOT BUFFER IMPLEMENTATIONS USED TO COMMUNICATE A WAITFREE VARIABLE THAT IS
** READ ACCORDING TO A LIFO POLICY. */
typedef struct {      // Defines one slot that can be used with either slot schemes
  UINT8 *Data;        // buffer that holds a data
  UINT8 BufferItems;  // number of bytes currently used in the buffer
  UINT32 Sequence;    // number of the slot among those written, from 1
} BUFFER_DATA;

typedef struct {              // 4-slotted buffer descriptor
  BUFFER_DATA *CurrentWriter; // current writer task's slot
  BUFFER_DATA Slot[2][2];     // slots used by the writer and reader tasks
  BOOL Reading, Latest;       // 4-slot selection specifics
  BOOL CurrentWriterPair, CurrentWriterIndex, Index[2];
} BUFFER_4_SLOT;

typedef struct {              // 3-slotted buffer descriptor
  BUFFER_DATA *CurrentWriter; // current writer task's slot
  BUFFER_DATA Slot[3];        // slots used by the writer and reader tasks
  UINT8 Reading, Latest;      // 3-slot selection specifics
  UINT8 CurrentWriterIndex;   // current slot index being filled by the writer task
} BUFFER_3_SLOT;

/* General structure suitable for both 3- or 4-slot reader-writer protocols. This struc-
** ture can also be used by an interrupt handler defining the I/O buffers */
typedef struct {
  void *Buffer;               // 3- or 4-slot mechanism descriptor
  UINT8 BufferSize;           // number of bytes composing a full data item
  UINT8 Status;               // one of {BUFFER_INIT,BUFFER_UNREAD}
  UINT8 BufferSlotType;       // one of {OS_BUFFER_TYPE_3_SLOT,OS_BUFFER_TYPE_4_SLOT}
  void *EventQueue;           // optional event associated with the buffer
  UINT32 Written;             // slots written so far, by the one writer
  UINT32 LastRead;            // Sequence of the slot last read with OS_READ_ONLY_ONCE
} BUFFER_DESCRIPTOR;

/* Internal slot-buffer function prototypes */
static BUFFER_DATA *GetReadyBuffer3Slot(BUFFER_DESCRIPTOR *descriptor);
static BUFFER_DATA *GetReadyBuffer4Slot(BUFFER_DESCRIPTOR *descriptor);


/* 3 or 4 Slot-buffer states:
** BUFFER_INIT indicates an unfilled buffer. This is the initial state of the buffer and
** its sole purpose is to distinguish an unfilled buffer from one holding a slot to read.
** It can no longer be re-entered once exited.
** BUFFER_UNREAD indicates that a full slot has been handed over to the reader.
** Whether that slot is new to a reader taking each slot once is told by its Sequence:
** a status set apart from the slot cannot tell which slot it speaks of. Set before the
** slot is handed over, a reader preempting the writer in between took the slot it had
** read as new and the new one then counted as read; set after, it took the new slot
** early, the status still saying unread from the slot before, and again once the
** writer had said it unread (SoakPico, 2026-09-25). */
#define BUFFER_INIT   0x1
#define BUFFER_UNREAD 0x2


/* OSInitBuffer: Creates a 3- or 4-slot buffer that can be used for I/Os and inter-task
** communications. Note that the scheme only applies to a single writer/reader task pair.
** Parameters:
**   (1) (UINT8) required buffer size;
**   (2) (UINT8) buffer slot type: this can either be OS_BUFFER_TYPE_3_SLOT or
**               OS_BUFFER_TYPE_4_SLOT;
**   (3) (void *) an optional event queue, which when specified can be used to signal a
**                task.
** Returned value: (void *) On success, the function returns a descriptor holding all
** state and buffer information that can be used for a device or for task communications.
** On memory allocation failure, the function returns NULL. */
void *OSInitBuffer(UINT8 bufferSize, UINT8 bufferSlotType, void *eventQueue)
{
  UINT8 i;
  BUFFER_DESCRIPTOR *descriptor;
  BUFFER_4_SLOT *buffer4;
  BUFFER_3_SLOT *buffer3;
  if ((bufferSlotType != OS_BUFFER_TYPE_4_SLOT && bufferSlotType != OS_BUFFER_TYPE_3_SLOT) ||
      (descriptor = (BUFFER_DESCRIPTOR *)OSMalloc(sizeof(BUFFER_DESCRIPTOR))) == NULL)
     return NULL;
  descriptor->Status = BUFFER_INIT;
  descriptor->Written = descriptor->LastRead = 0;
  descriptor->BufferSize = bufferSize;
  descriptor->BufferSlotType = bufferSlotType;
  descriptor->EventQueue = eventQueue;
  switch (bufferSlotType) {
     case OS_BUFFER_TYPE_4_SLOT:
        if ((descriptor->Buffer = OSMalloc(sizeof(BUFFER_4_SLOT))) == NULL)
           return NULL;
        buffer4 = (BUFFER_4_SLOT *)descriptor->Buffer;
        buffer4->Reading = 0;  buffer4->Latest = 0;
        /* OSMalloc does not clear what it hands out: an index read before the writer
        ** set it would name a slot outside the buffer. */
        buffer4->Index[0] = buffer4->Index[1] = 0;
        buffer4->CurrentWriterPair = 1;
        buffer4->CurrentWriterIndex = !(buffer4->Index[1]);
        buffer4->CurrentWriter = &buffer4->Slot[1][buffer4->CurrentWriterIndex];
        for (i = 0; i < 2; i++)  // Allocate the buffer of each slot
           for (bufferSize = 0; bufferSize < 2; bufferSize++) {
              buffer4->Slot[i][bufferSize].BufferItems = 0;
              buffer4->Slot[i][bufferSize].Sequence = 0;
              if ((buffer4->Slot[i][bufferSize].Data = (UINT8 *)OSMalloc(descriptor->BufferSize)) == NULL)
                 return NULL;
           }
        break;
     case OS_BUFFER_TYPE_3_SLOT:
        if ((descriptor->Buffer = OSMalloc(sizeof(BUFFER_3_SLOT))) == NULL)
           return NULL;
        buffer3 = (BUFFER_3_SLOT *)descriptor->Buffer;
        buffer3->Reading = 3;  buffer3->Latest = 0;
        buffer3->CurrentWriterIndex = 2;
        buffer3->CurrentWriter = &buffer3->Slot[2];
        for (i = 0; i < 3; i++) {  // Allocate the buffer of each slot
           buffer3->Slot[i].BufferItems = 0;
           buffer3->Slot[i].Sequence = 0;
           if ((buffer3->Slot[i].Data = (UINT8 *)OSMalloc(descriptor->BufferSize)) == NULL)
              return NULL;
        }
        break;
  }
  return descriptor;
} /* end of OSInitBuffer */


/* OSWriteBuffer: Copies the data to the current writer slot and truncates what exceeds
** the buffer size.
** Parameters:
**   (1) (void *) a buffer descriptor created by OSInitBuffer();
**   (2) (UINT8 *) data to copy into the writer buffer;
**   (3) (UINT8) number of data bytes to consider.
** Returned value: (UINT8 ) number of bytes accepted into the buffer. */
UINT8 OSWriteBuffer(void *descriptor, UINT8 *data, UINT8 size)
{
  /* next[Reading][Latest]: a slot that is neither the one being read nor the latest; in
  ** row 3, a reader still asking for a slot, only the latest is avoided (Chen and Burns,
  ** 1997). */
  static const UINT8 next [4][3] = {{1,2,1},{2,2,0},{1,0,0},{1,2,0}};
  BUFFER_DESCRIPTOR *descript = (BUFFER_DESCRIPTOR *)descriptor;
  BUFFER_DATA *element;
  UINT8 i = 0;
  if (descript != NULL) { // Check that the buffer was created
     // CurrentWriter comes first in both slot structures
     element = ((BUFFER_4_SLOT *)descript->Buffer)->CurrentWriter;
     for ( ; i < size && i < descript->BufferSize &&
                                      element->BufferItems != descript->BufferSize; i++)
        element->Data[element->BufferItems++] = data[i]; // copy the byte
     if (element->BufferItems == descript->BufferSize) {
        /* Number the full slot, with its data; hand it over, then get a new one for the
        ** next time the writer is invoked. */
        element->Sequence = ++descript->Written;
        if (descript->BufferSlotType == OS_BUFFER_TYPE_4_SLOT) {
           BOOL wpair;
           BUFFER_4_SLOT *buf = (BUFFER_4_SLOT*)((BUFFER_DESCRIPTOR*)descriptor)->Buffer;
           wpair = buf->CurrentWriterPair;      // Continue with the previous buffer pair
           /* A reader on the other core must see the slot filled before it is named, and
           ** its index before its pair (test/model/fourslot.py, explore_weak). */
           _OSMemoryBarrier();
           buf->Index[wpair] = buf->CurrentWriterIndex; // Writer indicates slot
           _OSMemoryBarrier();
           buf->Latest = wpair;                         // Writer indicates pair
           /* Prepare for the next time the writer gets a new byte. */
           buf->CurrentWriterPair = !buf->Reading;
           /* As in Simpson's writer, the slot of the chosen pair that is not its latest. */
           buf->CurrentWriterIndex = !buf->Index[buf->CurrentWriterPair];
           buf->CurrentWriter = &buf->Slot[buf->CurrentWriterPair][buf->CurrentWriterIndex];
           buf->CurrentWriter->BufferItems = 0;
        }
        else {
           UINT8 windex;
           BUFFER_3_SLOT *buffer;
           buffer = (BUFFER_3_SLOT*)((BUFFER_DESCRIPTOR*)descriptor)->Buffer;
           /* A reader on the other core must see the slot filled before it is named, and
           ** named before it is handed over (test/model/threeslot.py, explore_weak). */
           _OSMemoryBarrier();
           buffer->Latest = windex = buffer->CurrentWriterIndex;
           _OSMemoryBarrier();
           /* Hand the slot to a reader that asks for one (Reading == 3), retrying until
           ** the SC succeeds or the reader has taken a slot itself: an SC also fails when
           ** an interrupt merely came between it and its LL. On one core that interrupt
           ** also clears the reader's reservation, so one try would do; across two cores
           ** the reader's survives, its SC gives it a Latest older than this one, and the
           ** slot chosen below would be the one it reads (test/model/threeslot.py). */
           while (OSUINT8_LL(&buffer->Reading) == 3)
              if (OSUINT8_SC(&buffer->Reading,windex))
                 break;
           /* Prepare for the next time the writer gets a new byte. */
           buffer->CurrentWriterIndex = windex = next[buffer->Reading][buffer->Latest];
           buffer->CurrentWriter = &buffer->Slot[windex];
           buffer->CurrentWriter->BufferItems = 0;
        }
        /* A full slot is there to read from now on; on two cores, it must be seen handed
        ** over before this. */
        _OSMemoryBarrier();
        descript->Status = BUFFER_UNREAD;
        /* Unblock a task if there is an event associated with a full buffer */
        if (descript->EventQueue != NULL)
           OSScheduleSuspendedTask(descript->EventQueue);
     }
  }
  return i;
} /* end of OSWriteBuffer */


/* OSGetReferenceBuffer: Returns a pointer to the most recent data buffer held in the
** descriptor.
** Parameters:
**   (1) (void *) general buffer descriptor created and returned by OSInitBuffer();
**   (2) (UINT8) read mode: this can be OS_READ_ONLY_ONCE or OS_READ_MULTIPLE;
**   (3) (UINT8 **) pointer to the data buffer.
** Returned value: (UINT8) On success, the number of bytes that are available in the 3rd
** argument is returned. On failure or when there is nothing to read, 0 is returned and
** the data buffer is set to NULL. */
UINT8 OSGetReferenceBuffer(void *descriptor, UINT8 readMode, UINT8 **data)
{
  BUFFER_DESCRIPTOR *descript = (BUFFER_DESCRIPTOR *)descriptor;
  BUFFER_DATA *buffer;
  /* Check that the buffer was created and that there is something to read. */
  if (descript != NULL && descript->Status != BUFFER_INIT) {
     /* Get the slot holding the most recent written data. */
     if (descript->BufferSlotType == OS_BUFFER_TYPE_4_SLOT)
        buffer = GetReadyBuffer4Slot(descript);
     else
        buffer = GetReadyBuffer3Slot(descript);
     /* The latest slot, if not the one last read once: see its Sequence. */
     if (!readMode) {
        if (buffer->Sequence == descript->LastRead)
           goto fail;
        descript->LastRead = buffer->Sequence;
     }
     if (buffer->BufferItems > 0) {
        *data = buffer->Data;
        return buffer->BufferItems;
     }
  }
fail:
  *data = NULL;
  return 0;
} /* end of OSGetReferenceBuffer */


/* OSGetCopyBuffer: Returns a copy of the data held in the current buffer descriptor.
** Parameters:
**   (1) (void *) general buffer descriptor created and returned by OSInitBuffer();
**   (2) (UINT8) read mode: This can be OS_READ_ONLY_ONCE or OS_READ_MULTIPLE;
**   (3) (UINT8 *) data buffer where data is to be copied. This buffer should be greater
**          or equal to the buffer size specified when the general buffer descriptor was
**          created by OSInitBuffer().
** Returned value: (UINT8) Number of bytes copied into the specified data buffer. */
UINT8 OSGetCopyBuffer(void *descriptor, UINT8 readMode, UINT8 *data)
{
  UINT8 i;
  BUFFER_DESCRIPTOR *descript = (BUFFER_DESCRIPTOR *)descriptor;
  BUFFER_DATA *buffer;
  /* Check that the buffer was created and that there is something to read. */
  if (descript != NULL && descript->Status != BUFFER_INIT) {
     if (descript->BufferSlotType == OS_BUFFER_TYPE_4_SLOT)
        buffer = GetReadyBuffer4Slot(descript);
     else
        buffer = GetReadyBuffer3Slot(descript);
     if (!readMode) {   // as OSGetReferenceBuffer
        if (buffer->Sequence == descript->LastRead)
           goto fail;
        descript->LastRead = buffer->Sequence;
     }
     for(i = 0; i < buffer->BufferItems; i++)
        data[i] = buffer->Data[i];
     return buffer->BufferItems;
  }
fail:
  return 0;
} /* end of OSGetCopyBuffer */


/* GetReadyBuffer4Slot: Returns the next and most recent slot for the reader. This func-
** tion should be called each time an application task requests a copy or a reference to
** some shared data.
** Parameter: (void *) General buffer descriptor created and returned by OSInitBuffer().
** Returned value: Next available buffer slot to read from. */
BUFFER_DATA *GetReadyBuffer4Slot(BUFFER_DESCRIPTOR *descriptor)
{
  BOOL rpair, rindex;
  BUFFER_4_SLOT *buffer = (BUFFER_4_SLOT *)descriptor->Buffer;
  rpair = buffer->Latest;         // Reader chooses pair
  /* With the writer on the other core: the previous slot read before the new pair is
  ** announced, and the pair announced before its slot is chosen and read
  ** (test/model/fourslot.py, explore_weak). */
  _OSMemoryBarrier();
  buffer->Reading = rpair;        // Reader indicates pair
  _OSMemoryBarrier();
  rindex = buffer->Index[rpair];  // Reader chooses slot
  return &buffer->Slot[rpair][rindex];
} /* end of GetReadyBuffer4Slot */


/* GetReadyBuffer3Slot: Same as GetReadyBuffer4Slot() but applied to a 3-slot mechanism.
** Parameter: (void *) General buffer descriptor created and returned by OSInitBuffer().
** Returned value: Next available buffer slot to read from. */
BUFFER_DATA *GetReadyBuffer3Slot(BUFFER_DESCRIPTOR *descriptor)
{
  BUFFER_3_SLOT *buffer = (BUFFER_3_SLOT *)descriptor->Buffer;
  /* With the writer on the other core: the previous slot read before a new one is asked
  ** for, and the request seen before the slot is read (test/model/threeslot.py,
  ** explore_weak). */
  _OSMemoryBarrier();
  buffer->Reading = 3;
  _OSMemoryBarrier();
  /* Unlike the compare-and-swap of Chen and Burns (1997), an SC also fails when an
  ** interrupt merely came between it and its LL, with Reading still 3: try again until
  ** the SC succeeds or the writer has chosen for the reader (test/model/threeslot.py). */
  while (OSUINT8_LL(&buffer->Reading) == 3)
     if (OSUINT8_SC(&buffer->Reading,buffer->Latest))
        break;
  return &buffer->Slot[buffer->Reading];
} /* end of GetReadyBuffer3Slot */

#endif /* ESCAPEMENT_VERSION_HARD_PA */ 
