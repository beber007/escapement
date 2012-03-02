/* Copyright (c) 2006-2012 MIS Institute of the HEIG affiliated to the University of
** Applied Sciences of Western Switzerland. All rights reserved.
** Permission to use, copy, modify, and distribute this software and its documentation
** for any purpose, without fee, and without written agreement is hereby granted, pro-
** vided that the above copyright notice, the following three sentences and the authors
** appear in all copies of this software and in the software where it is used.
** IN NO EVENT SHALL THE MIS INSTITUTE NOR THE HEIG NOR THE UNIVERSITY OF APPLIED
** SCIENCES OF WESTERN SWITZERLAND BE LIABLE TO ANY PARTY FOR DIRECT, INDIRECT, SPECIAL,
** INCIDENTAL, OR CONSEQUENTIAL DAMAGES ARISING OUT OF THE USE OF THIS SOFTWARE AND ITS
** DOCUMENTATION, EVEN IF THE MIS INSTITUTE OR THE HEIG OR THE UNIVERSITY OF APPLIED
** SCIENCES OF WESTERN SWITZERLAND HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
** THE MIS INSTITUTE, THE HEIG AND THE UNIVERSITY OF APPLIED SCIENCES OF WESTERN SWITZER-
** LAND SPECIFICALLY DISCLAIMS ANY WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
** WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE. THE SOFTWARE PRO-
** VIDED HEREUNDER IS ON AN "AS IS" BASIS, AND THE MIS INSTITUTE NOR THE HEIG AND NOR THE
** UNIVERSITY OF APPLIED SCIENCES OF WESTERN SWITZERLAND HAVE NO OBLIGATION TO PROVIDE
** MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR MODIFICATIONS.
*/
/* File ZottaOSHard.c: Generic kernel implementation.
** Version date: February 2012
** Authors: MIS-TIC
*/

/* TODO: Les commentaires doivent être revu pour supprimer les references au msp430 */

#include "ZottaOS_Types.h"
#include "ZottaOS_Processor.h"
#include "ZottaOS_Timer.h"
#include "ZottaOSHard.h"

#ifdef ZOTTAOS_VERSION_HARD


/* TASK STATE BIT DEFINITION
**            STATE_INIT   <-  STATE_TERMINATED
**                 |                 ^
**                 V                 |
**            STATE_RUNNING  -> STATE_ZOMBIE
** In state INIT, a task is in the ready queue but hasn't yet started it's execution.
** In the RUNNING state, the task is also in the ready queue and has begun its execution.
** The difference between these two states lies with the context switch. In the former
** state there are no registers to restore. This is not the case in the READY state.
** When a task terminates its execution, it becomes a ZOMBIE and it needs to stay in the
** ready queue until it finishes its preparation because otherwise and when a timer in-
** terruption occurs, the task will never have the opportunity to finish. In ZOMBIE state,
** the task cannot begin its next period until it is removed from the ready queue. To
** circumvent this problem, a timer interrupt asserts that an active zombie task is no
** longer in the ready queue. A task with state ZOMBIE and no longer in the ready queue
** is considered to be in state TERMINATED, which is a fictitious state. */
#define STATE_INIT          0x00 /* Must be equal to zero */
#define STATE_RUNNING       0x01 /* These values are also used in ZottaOS_msp430XXX.asm */
#define STATE_ZOMBIE        0x02 /* or in ZottaOS_cc430XXX.asm */
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
  INT32 NextArrivalTimeLow;      // Remaining time before reappearing
  void (*TaskCodePtr)(void *);   // Pointer to the first instruction of the code task
                                 // (Needed to reinitialize a new instance execution)
  void *Argument;                // An instance specific 32-bit value
  INT32 PeriodLow;               // Interarrival period >= WCET (user input)
  #if SCHEDULER_REAL_TIME_MODE != DEADLINE_MONOTONIC_SCHEDULING
     INT32 NextDeadline;         // Current instance deadline (offset==ETCB)
     INT32 Deadline;             // Task's deadline <= Period (user input)
  #endif
  UINT16 PeriodHigh;             // Number of full 2^30 cycles of the interarrival period
  UINT16 NextArrivalTimeHigh;    // Number of full 2^30 cycles of the next arrival time
} TCB;

/* List head sentinel */
typedef struct {
  TCB *Next[2];                  // [0]: first TCB in the ready queue link Next
                                 // [1]: first TCB in the arrival queue link
} TCB_HEAD;

/* List tail sentinel, which also corresponds to the idle task */
typedef struct {
  TCB *Next[2];                  // [0] and [1] are always equal to NULL
  UINT8 TaskState;               // Current state of the idle task
  #if SCHEDULER_REAL_TIME_MODE == DEADLINE_MONOTONIC_SCHEDULING
     UINT8 Priority;             // Task priority. Must be the lowest in the system.
  #endif
  INT32 NextArrivalTimeLow;      // Set to INT32_MAX
  void (*TaskCodePtr)(void *);   // Pointer to the first instruction of the idle task
} TCB_TAIL;


/* EVENT-DRIVEN OR SYNCHRONOUS TASK CONTROL BLOCK */
struct FIFOQUEUE;
typedef struct ETCB {
  struct ETCB *Next[2];          // Next TCB in the list where this task is located
                                 // [0]: ready queue link, [1]: event queue link
  UINT8 TaskState;               // Current state of the task
  #if SCHEDULER_REAL_TIME_MODE == DEADLINE_MONOTONIC_SCHEDULING
     UINT8 Priority;             // Static task priority
  #endif
  INT32 NextArrivalTimeLow;      // Remaining time before it can reappear
  void (*TaskCodePtr)(void *);   // Pointer to the first instruction of the code task
                                 // (Needed to reinitialize a new instance execution)
  void *Argument;                // An instance specific 32-bit value
  #if SCHEDULER_REAL_TIME_MODE == DEADLINE_MONOTONIC_SCHEDULING
     INT32 PeriodLow;            // Interarrival period >= WCET (user input)
  #else
     INT32 WorkLoad;             // Equal to WCET / (available processor load)
     INT32 NextDeadline;         // Key criteria for EDF scheduling (offset==TCB)
  #endif
  struct FIFOQUEUE *EventQueue;  // Pointer to the event queue of this task
  struct ETCB *NextETCB;         // Link to the next created ETCB
} ETCB;


/* TASK SCHEDULING QUEUES
** The ready and arrival queues are implemented as wait-free concurrent linked-lists of
** TCBs. Compared to lock-free or non-blocking algorithms, the implemented queueing oper-
** ations are wait-free algorithms that take a bounded amount of time. Before performing
** an operation, enqueuers and dequeuers first check for any pending operation under way
** for the queue. If one exists, that operation is first completed. After this step, the
** enqueuer or dequeuer posts the operation it intends to do before actually starting the
** operation. If a higher priority enqueuer or dequeuer interrupts the queueing opera-
** tion, this task will complete the operation on behalf of the interrupted task. The
** point to observe and that meets the requirements of wait-free algorithms is that a
** task may need to complete the operation posted from a lower priority task at most once
** during its execution. The operations are also done in such a way that when completing
** the pending operation of another task, the operation starts where the interrupted task
** left off. */
/* All queues use the same head and tail sentinel blocks and there are 2 or 3 lists to
** manage the scheduling of the tasks:
** Ready queue: The first item of the list points to the currently active task instance
** (_OSActiveTask). */
#define READYQ    0
/* Queue of task instances that have yet to arrive: When a periodic task instance termi-
** nates, it places its TCB in the arrival queue which stays in the queue until its pe-
** riod expires at which time a new instance is created. */
#define ARRIVALQ  1
/* Queue of event-driven tasks that are blocked for an event: Event-driven tasks can be
** blocked, running or waiting. In the blocked state, the task is placed in the queue as-
** sociated with the event using the same link as ARRIVALQ. The task can also be in the
** arrival queue if it has expired its processor load with it's last execution. */
#define BLOCKQ    ARRIVALQ

TCB *_OSQueueHead = NULL;
static TCB_TAIL *OSQueueTail = NULL;

#if SCHEDULER_REAL_TIME_MODE == DEADLINE_MONOTONIC_SCHEDULING
  /* EVENT-DRIVEN TASKING UNDER DEADLINE MONOTONIC SCHEDULING
  ** Event-driven tasks are characterized by a tuple (WCET,workload) where the workload
  ** is the equivalent of the task's deadline and period. Because of the priority assign-
  ** ment, the workload fixes the task's scheduling priority. However, a new instance may
  ** not begin before the end of the previous period to guarantee the task set is sched-
  ** ulable. Hence, when a task completes it's current instance, it joins a blocking
  ** queue to wait for the next event; when the event occurs, it must first finish it's
  ** period. This is done in the arrival queue of periodic tasks. Because the timer peri-
  ** odically resets the system time, the earliest starting time of each event-driven
  ** task must also be adjusted. To do this, we need to access them. */
  static ETCB *SynchronousTaskList = NULL;
#else
  /* APERIODIC SERVER SCHEDULING DEFINITIONS UNDER EDF
  ** Event-driven tasks are scheduled according to a sporadic server model, i.e., (1)
  ** these tasks only run during the slack time left by the periodic tasks, (2) if
  ** U(hard) denotes the processor utilization of all the hard periodic tasks, i.e. the
  ** sum of WCET_i/period_i of all tasks i, then the remaining utilization 1-U(hard) can
  ** be given to the synchronous tasks. The computed deadline for the task is
  **           d(j) = max(d(j-1),currentTime) + WCET/U(task)
  ** where U(task) < 1 - U(hard). */
  static INT32 SynchronousTaskDeadlines = 0;  // denotes d(j) in the above
  /* SynchronousTaskList serves the same purpose as for DM scheduling. */
  static ETCB *SynchronousTaskList = NULL;
#endif
/* RescheduleSynchronousTaskList is a temporary LIFO queue that is used to transfer event
** driven tasks that can be scheduled into the ready queue. Tasks in this queue are in-
** serted by a signaling task or by an event-driven task when this last task needs to re-
** start itself. The rational behind this queue is to allow the timer interrupt insert
** the tasks into ready queue because these tasks may take precedence over the caller.
** Hence, the caller needs to save its context before releasing the processor. This is
** done by invoking a timer interrupt. */
static ETCB *RescheduleSynchronousTaskList = NULL;


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


/* TASK EXECUTION STACK
** _OSStackBasePointer: Pointer to the stack base of currently running task: Local varia-
** bles used in the task are located between the SP register and the stack base, and when
** resuming the previous task, we need to start popping registers from the base. */
void *_OSStackBasePointer;


/* TIME KEEPING */
/* To avoid overflow of temporal variables, periodically these variables are shifted once
** _OSTime is greater or equal to a given value. This value must be equal to 2^(16+i)
** where i > 0 (2^16 = 65536). The recommended value is i = 14. */
static const INT32 ShiftTimeLimit = 0x40000000; // = 2^30 (i = 14)
/* The while loop that empties the arrival uses a condition that simply depends upon the
** current time. To avoid crossing the tail sentinel, the arrival time of the sentinel
** must be unreachable (unattainable arrival time). */
#define INT32_MAX 0x7FFFFFFF   /* 2^31 - 1 */


/* INTERNAL FUNCTION PROTOTYPES AND MACROS */
/* Addresses in a waitfree or non-blocking algorithm can also contain a marker so that an
** atomic operation can be done. The following define macros to test, insert and remove a
** marker. Note that these macros could have been defined as in-line functions but macros
** are as simple.
** The marker value is processor dependent. When using the MSB of an address, all these
** addresses must be in the lower half memory locations. For markers using the LSB, all
** addresses must be aligned on modulo 2 word boundaries. */

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

static BOOL Initialize(void);
static void IdleTask(void *);
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
void _OSSleep(void);
static void EnqueueRescheduleQueue(ETCB *etcb);
static void EmptyRescheduleSynchronousTaskList(INT32 currentTime);
#if SCHEDULER_REAL_TIME_MODE != DEADLINE_MONOTONIC_SCHEDULING
  static INT32 GetSuspendedSchedulingDeadline(ETCB *, INT32 currentTime);
#endif


/* Initialize: Initializes the internals of the OS. This function is called prior to
** creating the first task and sets up the needed queues.
** Returned value: (BOOL) TRUE on success and FALSE otherwise. */
BOOL Initialize(void)
{
  /* Allocate TCB blocks to hold the sentinel heads and tails of the ready and arrival
  ** queues. */
  if ((_OSQueueHead = (TCB *)OSMalloc(sizeof(TCB_HEAD))) == NULL)
     return FALSE;
  if ((OSQueueTail = (TCB_TAIL *)OSMalloc(sizeof(TCB_TAIL))) == NULL)
     return FALSE;
  _OSQueueHead->Next[READYQ] = (TCB *)OSQueueTail;
  OSQueueTail->Next[READYQ] = NULL;
  /* Create the idle task as the tail of the ready queue. */
  OSQueueTail->TaskCodePtr = IdleTask;
  #if SCHEDULER_REAL_TIME_MODE == DEADLINE_MONOTONIC_SCHEDULING
     OSQueueTail->Priority = 0;
  #endif
  /* Make an empty arrival queue. */
  _OSQueueHead->Next[ARRIVALQ] = (TCB *)OSQueueTail;
  OSQueueTail->Next[ARRIVALQ] = NULL;
  OSQueueTail->TaskState = STATE_INIT | TASKTYPE_BLOCKING;
  OSQueueTail->NextArrivalTimeLow = INT32_MAX;
  return TRUE;
} /* end of Initialize */


/* IdleTask: This task executes whenever there is no other task in the ready queue. Its
** sole purpose is to keep the processor busy until the next task arrival time.
** The argument parameter to the idle task is undefined. */
void IdleTask(void *argument)
{
  if (_OSQueueHead->Next[ARRIVALQ] != (TCB *)OSQueueTail || SynchronousTaskList != NULL)
     _OSStartTimer();   // Start the interval timer
  /* set bits CPUOFF, SCG0 and SCG1 into the SR register to enter in LPM3 sleep mode */
  _OSSleep();
} /* end of IdleTask */


/* OSCreateTask: Creates a new periodic task by allocating a new TCB to the task.
** The period and arrival time of periodic tasks are given respectively by the relation
**     period = PeriodHigh * 2^30 + PeriodLow
**     arrival time = NextArrivalTimeHigh * 2^30 + NextArrivalTimeLow */
BOOL OSCreateTask(void task(void *), UINT16 periodCycles, INT32 periodOffset,
                  INT32 deadline, void *argument)
{
  TCB *ptcb;
  if (_OSQueueHead == NULL && !Initialize())
     return FALSE;
  /* Get a new TCB and initialize it. */
  if ((ptcb = (TCB*)OSMalloc(sizeof(TCB))) == NULL)
     return FALSE;
  ptcb->Next[READYQ] = NULL;
  ptcb->TaskState = STATE_ZOMBIE;
  ptcb->TaskCodePtr = task;
  ptcb->PeriodLow = periodOffset;
  ptcb->PeriodHigh = periodCycles;
  ptcb->NextArrivalTimeHigh = 0;
  ptcb->Argument = argument;
  #if SCHEDULER_REAL_TIME_MODE == DEADLINE_MONOTONIC_SCHEDULING
     /* Temporarily save the deadline until the user calls OSStartMultitasking(). */
     ptcb->NextArrivalTimeLow = deadline;
     ptcb->Priority = GetTaskPriority(deadline);
  #else
     ptcb->NextArrivalTimeLow = 0;
     ptcb->Deadline = deadline;
  #endif
  ArrivalQueueInsert(ptcb);
  return TRUE;
} /* end of OSCreateTask */


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
  ** starting ZottaOS. Note that after creating the last task, OSQueueTail->Priority
  ** holds the number of tasks in the application. */
  do {
     nextTCB = nextTCB->Next[ARRIVALQ];
     if (nextTCB->NextArrivalTimeLow > deadline)
        nextTCB->Priority++;
     else
        nbTasks++;
  } while (nextTCB != (TCB *)OSQueueTail);
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


/* OSEndTask: Called by a periodic task when it terminates its instance. The next in-
** stance of the task is already in the arrival queue. As soon as the task succeeds in
** positioning its state, it may be interrupted by any entity so long as this entity com-
** pletes the instructions done here. */
void OSEndTask(void)
{
  /* Set the task to zombie to indicate that it is about to remove itself from the ready
  ** queue and that its context should not be saved. */
  _OSActiveTask->TaskState |= STATE_ZOMBIE;
  _OSNoSaveContext = TRUE; // Don't save the context of this task
  /* Remove the task from the ready queue */
  _OSQueueHead->Next[READYQ] = _OSActiveTask->Next[READYQ];
  _OSActiveTask = _OSQueueHead->Next[READYQ];
  _OSScheduleTask();
} /* end of OSEndTask */


/* Macro definition of result=min(result-sub,0) done on 32-bit integer arithmetic where
** sub > 0. */
#define SubOrZeroIfNeg(result,sub) \
{ \
  result -= sub;  \
  if (result < 0) \
     result = 0;  \
}


#ifdef NESTED_TIMER_INTERRUPT
   /* _OSEnableSoftTimerInterrupt: Re-enables software timer interrupt. This function is
   ** called when the current software timer ISR is complete and may be re-invoked. This
   ** function is defined in the generated assembler file. */
   void _OSEnableSoftTimerInterrupt(void);
#endif

/* _OSTimerInterruptHandler: Software interrupt handler for the timer that manages task
** instance arrivals. Because the timer is a bit counter with a predefined number of bits,
** when a timer event occurs, it can be that there are no arrivals. In this case, we only
** need check if we need to shift temporal variables. */
void _OSTimerInterruptHandler(void)
{
  INT32 currentTime;
  ETCB *etcb;
  TCB *arrival;
  #ifdef NESTED_TIMER_INTERRUPT
     /* At this point there can only be one current timer interrupt under way. */
     #ifdef DEBUG_MODE
        static UINT8 nesting = 0;
        if (++nesting > 1) {
           _OSDisableInterrupts();
           while (TRUE); // If you get here, call us!
        }
     #endif
     /* Mark that a software timer interrupt is under way so that new interrupts will not be
     ** generated (see EnqueueRescheduleQueue). */
     while (!_OSUINTPTR_SC((UINTPTR *)&RescheduleSynchronousTaskList,
          GetMarkedReference(_OSUINTPTR_LL((UINTPTR *)&RescheduleSynchronousTaskList))));
  #endif
  currentTime = OSGetActualTime();
  /* Transfer all new arrivals to the ready queue. */
  arrival = _OSQueueHead->Next[ARRIVALQ];
  while (arrival->NextArrivalTimeLow <= currentTime &&
       ((arrival->TaskState & TASKTYPE_BLOCKING) || arrival->NextArrivalTimeHigh == 0)) {
     _OSQueueHead->Next[ARRIVALQ] = arrival->Next[ARRIVALQ];
     /* At this point an arriving periodic task should be state STATE_ZOMBIE, but event-
     ** driven tasks should be in state STATE_ZOMBIE | TASKTYPE_BLOCKING. */
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
     #endif
     ReadyQueueInsert(arrival);
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
     arrival = _OSQueueHead->Next[ARRIVALQ];
  }
  /* To avoid overflow of the wall clock _OSTime, a time shift is done on all temporal
  ** variables. Because all these variables are signed, their relative values are pre-
  ** served. */
  if (currentTime >= ShiftTimeLimit) {
     #if SCHEDULER_REAL_TIME_MODE != DEADLINE_MONOTONIC_SCHEDULING
        /* Time shift periodic tasks in the ready queue. */
        for (arrival = _OSQueueHead; (arrival = arrival->Next[READYQ]) != (TCB *)OSQueueTail; )
           if ((arrival->TaskState & TASKTYPE_BLOCKING) == 0)
              arrival->NextDeadline -= ShiftTimeLimit;
     #endif
     /* Time shift all tasks in the arrival queue. */
     for (arrival = _OSQueueHead; (arrival = arrival->Next[ARRIVALQ]) != (TCB *)OSQueueTail; ) {
        if ((arrival->TaskState & TASKTYPE_BLOCKING) == 0) {
           if (arrival->NextArrivalTimeHigh > 0)
              arrival->NextArrivalTimeHigh--;
           else
              arrival->NextArrivalTimeLow -= ShiftTimeLimit;
        }
        #if SCHEDULER_REAL_TIME_MODE != DEADLINE_MONOTONIC_SCHEDULING
           else
              arrival->NextArrivalTimeLow -= ShiftTimeLimit;
        #endif
     }
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
     /* Finally shift the wall clock. */
     _OSTimerShift(ShiftTimeLimit);
     currentTime -= ShiftTimeLimit;
  }
  /* Process pending event-driven tasks found in the RescheduleSynchronousTaskList. */
  EmptyRescheduleSynchronousTaskList(currentTime);
  #ifdef NESTED_TIMER_INTERRUPT
     /* If we get a timer interrupt, there's no point saving the context of the current ISR
     ** since we will have to restart it anyways. */
     _OSNoSaveContext = TRUE;
     #ifdef DEBUG_MODE
        --nesting;
     #endif
     _OSEnableSoftTimerInterrupt();
  #endif
  /* At this point another software timer can preempt and not save the current context as
  ** this interrupt restarts from the beginning. */
  /* Set arrival timer to the next arrival time. */
  arrival = _OSQueueHead->Next[ARRIVALQ];
  if (arrival != (TCB *)OSQueueTail &&
         ((arrival->TaskState & TASKTYPE_BLOCKING) || arrival->NextArrivalTimeHigh == 0))
     /* Set the timer comparator to the next periodic task arrival time. */
     _OSSetTimer(arrival->NextArrivalTimeLow);
  /* Return to the task with highest priority or start a new instance. */
  _OSActiveTask = _OSQueueHead->Next[READYQ];
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
     if ((right = left->Next[offsetNext]) == (TCB *)OSQueueTail || TestKey(newNode,right))
        break; // found the right node
     left = right;
  }
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
  #elif SCHEDULER_REAL_TIME_MODE == EARLIEST_DEADLINE_FIRST
     return insert->NextDeadline <= next->NextDeadline;
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
  UINT16 insertHigh, nextHigh;
  insertHigh =
        ((insert->TaskState & TASKTYPE_BLOCKING) == 0) ? insert->NextArrivalTimeHigh : 0;
  nextHigh =
            ((next->TaskState & TASKTYPE_BLOCKING) == 0) ? next->NextArrivalTimeHigh : 0;
  return insertHigh < nextHigh || (insertHigh == nextHigh &&
                                 insert->NextArrivalTimeLow <= next->NextArrivalTimeLow);
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


/* OSCreateEventDescriptor: Creates and returns a descriptor with all the need informa-
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
BOOL OSCreateSynchronousTask(void task(void *), INT32 workLoad, void *event, void *arg)
{
  ETCB *etcb;
  if (_OSQueueHead == NULL && !Initialize())
     return FALSE;
  /* Get a new TCB and initialize it. */
  if ((etcb = (ETCB *)OSMalloc(sizeof(ETCB))) == NULL)
     return FALSE;
  etcb->TaskState = STATE_ZOMBIE | TASKTYPE_BLOCKING;
  etcb->TaskCodePtr = task;
  etcb->Argument = arg;
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


/* OSSuspendSynchronousTask: The very last instruction of an event-driven task. This
** function checks if a new task instance should restart (an event was signaled) or if it
** should suspend itself until the event is signaled by OSScheduleSuspendedTask(). */
void OSSuspendSynchronousTask(void)
{
  FIFOQUEUE *eq = ((ETCB *)_OSActiveTask)->EventQueue;
  /* Insert the task into the event queue as soon as possible so that a task signaling
  ** an event will detect this task. When EnqueueEventTask() returns TRUE, there is a
  ** pending event and the task is not appended to the queue. */
  if (EnqueueEventTask(eq,(UINTPTR)_OSActiveTask)) {
     /* Now we need to reschedule this task, i.e. we need to resort the ready queue with
     ** new deadline or priority parameters. However because the needed information can
     ** be updated concurrently by the timer interrupt handler, it is simpler to resche-
     ** dule the task by the timer interrupt handler. */
     EnqueueRescheduleQueue((ETCB *)_OSActiveTask);
  }
  /* Set task to zombie to indicate that the task it is about to remove itself from the
  ** ready queue and that its context should not be saved. */
  _OSActiveTask->TaskState |= STATE_ZOMBIE;
  _OSNoSaveContext = TRUE;    // Don't save the context of this task
  /* Remove the task from the ready queue */
  _OSQueueHead->Next[READYQ] = _OSActiveTask->Next[READYQ];
  _OSActiveTask = _OSQueueHead->Next[READYQ];
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


/* EnqueueRescheduleQueue: Inserts an aperiodic ETCB into a list of work that needs to be
** processed by the timer handler (see EmptyRescheduleSynchronousTaskList). This indirect
** scheduling of aperiodic tasks is necessary because it is possible that the timer hand-
** ler receives an interrupt and performs a shift of all temporal variables while this
** scheduling is taking place. Note that the timer handler is invoked by forcing an in-
** terrupt. */
void EnqueueRescheduleQueue(ETCB *etcb)
{
#ifdef NESTED_TIMER_INTERRUPT
     UINTPTR tmp;
  #endif
  while (TRUE) {
     #ifdef NESTED_TIMER_INTERRUPT
        tmp = _OSUINTPTR_LL((UINTPTR *)&RescheduleSynchronousTaskList);
        if (IsMarkedReference(tmp)) {
           etcb->Next[BLOCKQ] = (ETCB *)GetUnmarkedReference(tmp);
           if (_OSUINTPTR_SC((UINTPTR *)&RescheduleSynchronousTaskList,GetMarkedReference(etcb)))
              return;
        }
        else {
           etcb->Next[BLOCKQ] = (ETCB *)tmp;
           if (_OSUINTPTR_SC((UINTPTR *)&RescheduleSynchronousTaskList,(UINTPTR)etcb)) {
              _OSGenerateSoftTimerInterrupt(); // Generate a soft timer interrupt
              return;
           }
        }
     #else
        etcb->Next[BLOCKQ] = (ETCB *)_OSUINTPTR_LL((UINTPTR *)&RescheduleSynchronousTaskList);
        if (_OSUINTPTR_SC((UINTPTR *)&RescheduleSynchronousTaskList,(UINTPTR)etcb)) {
           _OSGenerateSoftTimerInterrupt(); // Generate a soft timer interrupt
           return;
        }
     #endif
  }
} /* end of EnqueueRescheduleQueue */


/* EmptyRescheduleSynchronousTaskList: This function schedules all aperiodic tasks that
** are in a temporary LIFO list by placing them either in the ready queue or in the arri-
** val queue if the task hasn't finished its previous load imposed on the processor. */
void EmptyRescheduleSynchronousTaskList(INT32 currentTime)
{
  BOOL wait;
  ETCB *etcb;
  do {
     #ifdef NESTED_TIMER_INTERRUPT
        while ((etcb = (ETCB *)GetUnmarkedReference(_OSUINTPTR_LL((UINTPTR *)&RescheduleSynchronousTaskList))) != NULL) {
           if (_OSUINTPTR_SC((UINTPTR *)&RescheduleSynchronousTaskList,GetMarkedReference(etcb->Next[BLOCKQ]))) {
     #else
        while ((etcb = (ETCB *)_OSUINTPTR_LL((UINTPTR *)&RescheduleSynchronousTaskList)) != NULL) {
           if (_OSUINTPTR_SC((UINTPTR *)&RescheduleSynchronousTaskList,(UINTPTR)etcb->Next[BLOCKQ])) {
     #endif
           #if SCHEDULER_REAL_TIME_MODE != DEADLINE_MONOTONIC_SCHEDULING
              /* Under EDF, the task that is to process the event cannot execute until it
              ** has finished its previous deadline. */
              wait = currentTime < etcb->NextDeadline;
           #else
              /* Under deadline monotonic scheduling, the task that is to process the
              ** event cannot execute until it has finished its previous period. */
              if (etcb->TaskState & STATE_ZOMBIE)
                 wait = currentTime < etcb->NextArrivalTimeLow;
              else {
                 #ifdef DEBUG_MODE
                    if (currentTime >= etcb->NextArrivalTimeLow)
                       while (TRUE); // Event-driven task WCET overrun: increase task's period
                 #endif
                 for (wait = TRUE; currentTime >= etcb->NextArrivalTimeLow; )
                    etcb->NextArrivalTimeLow += etcb->PeriodLow;
              }
           #endif
           if (wait) {
              #if SCHEDULER_REAL_TIME_MODE != DEADLINE_MONOTONIC_SCHEDULING
                 etcb->NextArrivalTimeLow = etcb->NextDeadline;
              #endif
              ArrivalQueueInsert((TCB *)etcb);
           }
           else {
              #ifdef DEBUG_MODE
                 if ((etcb->TaskState & STATE_ZOMBIE) == 0)
                    while (TRUE); // Event-driven task WCET overrun: increase task's period
              #endif
              etcb->TaskState = TASKTYPE_BLOCKING;
              #if SCHEDULER_REAL_TIME_MODE != DEADLINE_MONOTONIC_SCHEDULING
                 etcb->NextDeadline = GetSuspendedSchedulingDeadline(etcb,currentTime);
              #else
                 etcb->NextArrivalTimeLow = currentTime + etcb->PeriodLow;
              #endif
              ReadyQueueInsert((TCB *)etcb);
           }
        }
     }
  } while (!_OSUINTPTR_SC((UINTPTR *)&RescheduleSynchronousTaskList,NULL));
} /* end of EmptyRescheduleSynchronousTaskList */


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


/* OSStartMultitasking: This function is called only once and after the application tasks
** have all been defined in the application. This function never returns and schedules
** all user tasks. When these terminate, an idle task runs with the lowest priority to
** keep the processor busy until the next task arrives. This function also creates a
** timer handler when there are tasks to be scheduled. */
BOOL OSStartMultitasking(void)
{
  ETCB *etcb;
  FIFOQUEUE *eq;
  UINT8 i;
  #if SCHEDULER_REAL_TIME_MODE == DEADLINE_MONOTONIC_SCHEDULING
     TCB *tcb;
  #endif
  /* Assert that ZottaOS begins with disabled maskable interruptions. */
  _OSDisableInterrupts();
  if (_OSQueueHead == NULL)
     Initialize();
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
     for (tcb = _OSQueueHead->Next[ARRIVALQ]; tcb != (TCB *)OSQueueTail; tcb = tcb->Next[ARRIVALQ])
        tcb->NextArrivalTimeLow = 0;
  #endif
  /* There are periodic tasks in the system when the arrival queue is not empty. Note
  ** that periodic tasks are initially inserted in this queue before starting ZottaOS. */
  if (_OSQueueHead->Next[ARRIVALQ] != (TCB *)OSQueueTail || SynchronousTaskList != NULL)
     /* Initialize the timer which starts counting as soon as the idle task begins. At
     ** this point, the timer's input divider is selected but it is halted. */
     _OSInitializeTimer();
  /* Start the first task in the ready queue, i.e. the Idle task. */
  _OSActiveTask = _OSQueueHead->Next[READYQ];
  _OSScheduleTask();     // Start the Idle task
  _OSEnableInterrupts();
  return FALSE;
} /* end of OSStartMultitasking */


/* INTERRUPT PROCESSING */
/* Global interrupt vector with one entry per source which is defined in either
** ZottaOS_msp430xxx.asm or ZottaOS_cc430xxx.asm */
extern void *_OSTabDevice[];


/* OSSetISRDescriptor: Associates an ISR descriptor with an _OSTabDevice entry.
** Parameters:
**   (1) (UINT8) index of the _OSTabDevice entry;
**   (2) (void *) ISR descriptor for the specified interrupt.
** Returned value: none. */
void OSSetISRDescriptor(UINT8 entry, void *descriptor)
{
  _OSTabDevice[entry] = descriptor;
} /* end of OSSetISRDescriptor */


/* OSGetISRDescriptor: Returns the ISR descriptor associated with an _OSTabDevice entry.
** Parameter: (UINT8) index of _OSTabDevice where the ISR descriptor is held.
** Returned value: (void *) The requested ISR descriptor is returned. If no previous
**    OSSetIODescriptor was previously made for the specified entry, the returned value
**    is undefined. */
void *OSGetISRDescriptor(UINT8 entry)
{
  return _OSTabDevice[entry];
} /* end of OSGetISRDescriptor */


/* WAITFREE FIFO QUEUE IMPLEMENTATION THAT IS USED INTERNALLY */
/* GetFIFOArrayMaxIndex: Given the size, say S, of a FIFO array, this function returns
** the largest natural number A such that A mod S = 0 and A != 2^n - 1 for any n > 0. */
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
  /* Initialize of fields of the descriptor to uninitialized markers. */
  des.HeadPropose = des.Head = 0xFFFF;
  des.SlotPropose = des.SlotReturn = (UINTPTR)&des;
  des.Done = FALSE;
  /* Do all pending operation before the current dequeue. */
  if ((op = queue->PendingOp) != NULL) { // Is there any pending operations?
     if (IsMarkedReference(op))          // Is pending operation a dequeue?
        FIFODequeueHelper(queue,signal,(DEQUEUE_DESCRIPTOR *)GetUnmarkedReference(op));
     else
        FIFOEnqueueHelper(queue,(ENQUEUE_DESCRIPTOR *)op);
  }
  /* Post current dequeue operation and do it. */
  queue->PendingOp = (void *)GetMarkedReference(&des);
  FIFODequeueHelper(queue,signal,&des);  // Do the operation
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
        slot = _OSUINTPTR_LL(&queue->Q[h]);
        if (des->Done || slot == SIGNAL)
           break;
        else if (slot == NULL)
           if (des->SlotReturn == NULL) {
              if (_OSUINTPTR_SC(&queue->Q[h],SIGNAL)) {
                 des->Done = TRUE;
                 break;
              }
           }
           else {
              IncrementFifoQueueIndex(&queue->Head,des->Head,queue->MaxIndex);
              des->Done = TRUE;
              break;
           }
        else if (_OSUINTPTR_SC(&queue->Q[h],NULL)) {
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
** Returned value: (BOOL) TRUE if the item is successfully enqueued or if the queue holds
**    the SIGNAL marker at the time of the call. The function returns FALSE if the queue
**    is full or if the queue does not contain the SIGNAL marker and the item (task) is
**    successfully inserted. */
BOOL FIFOEnqueue(FIFOQUEUE *queue, UINTPTR signal, UINTPTR item)
{
  ENQUEUE_DESCRIPTOR des;
  void *op;
  /* Initialize all fields with uninitialized markers and insert the item to enqueue. */
  des.TailPropose = des.Tail = 0xFFFF;
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
  /* Post and do this enqueue operation. */
  queue->PendingOp = &des;
  FIFOEnqueueHelper(queue,&des);
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
        slot = _OSUINTPTR_LL(&queue->Q[t]);
        if (des->Done)
           break;
        else if (slot == SIGNAL) {
           if (_OSUINTPTR_SC(&queue->Q[t],NULL)) {
              des->Done = TRUE;
              break;
           }
        }
        else if (slot == NULL) {
           if (des->SlotReturn == SIGNAL) {
              des->Done = TRUE;
              break;
           }
           else if (_OSUINTPTR_SC(&queue->Q[t],des->Item)) {
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
  UINT16 info;             // Starting field of the node's content
} NODE;

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
  /* Initialize FIFO */
  if ((desc = (BUFFER_DESCRIPTOR_FIFO *)OSMalloc(sizeof(BUFFER_DESCRIPTOR_FIFO))) == NULL)
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
  desc->FreeList = (FIFOQUEUE *)OSCreateEventDescriptor();
  desc->FreeList->QueueLength = maxNodes;
  desc->FreeList->MaxIndex = GetFIFOArrayMaxIndex(maxNodes);
  if ((desc->FreeList->Q = (UINTPTR *)OSMalloc(maxNodes * sizeof(NODE *))) == NULL)
     return NULL;
  for (i = 0; i < maxNodes; i++)
     if ((desc->FreeList->Q[i] = (UINTPTR)OSMalloc(sizeof(NODE) - sizeof(UINT16) + maxNodeSize)) == NULL)
        return NULL;
  return (void *)desc;
} /* OSInitFIFOQueue */


/* OSEnqueueFIFO: Inserts a node into a FIFO queue. The size parameter (user input) is
** stored into the node so that it can be transfered to the dequeuer task. */
BOOL OSEnqueueFIFO(void *queue, void *node, UINT16 size)
{
  NODE *tmpNode = (NODE *)(((UINTPTR)node) - sizeof(UINT16));
  tmpNode->size = size;
  return FIFOEnqueue((FIFOQUEUE *)queue,NULL,(UINTPTR)tmpNode);
} /* end of OSEnqueueFIFO */


/* OSDequeueFIFO: Retrieves a node from the FIFO queue. The size parameter (output para-
** meter corresponds to the value that was stored by the enqueue function. */
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
  node = (void *)((UINTPTR)node - sizeof(UINT16));
  FIFOEnqueue(((BUFFER_DESCRIPTOR_FIFO *)descriptor)->FreeList,NULL,(UINTPTR)node);
} /* end OSReleaseNodeFIFO */


/* 3 and 4 SLOT BUFFER IMPLEMENTATIONS USED TO COMMUNICATE A WAITFREE VARIABLE THAT IS
** READ ACCORDING TO A LIFO POLICY. */
typedef struct {      // Defines one slot that can be used with either slot schemes
  UINT8 *Data;        // buffer that holds a data
  UINT8 BufferItems;  // number of bytes currently used in the buffer
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
** ture can also be used by an interrupt hander defining the I/O buffers */
typedef struct {
  void *Buffer;               // 3- or 4-slot mechanism descriptor
  UINT8 BufferSize;           // number of bytes composing a full data item
  UINT8 Status;               // one of {BUFFER_INIT,BUFFER_UNREAD,BUFFER_READ}
  UINT8 BufferSlotType;       // one of {OS_BUFFER_TYPE_3_SLOT,OS_BUFFER_TYPE_4_SLOT}
  void *EventQueue;           // optional event associated the defined buffer
} BUFFER_DESCRIPTOR;

/* Internal slot-buffer function prototypes */
static BUFFER_DATA *GetReadyBuffer3Slot(BUFFER_DESCRIPTOR *descriptor);
static BUFFER_DATA *GetReadyBuffer4Slot(BUFFER_DESCRIPTOR *descriptor);


/* 3 or 4 Slot-buffer states:
** BUFFER_INIT indicates an unfilled and unread buffer. This is the initial state of the
** buffer and its sole purpose is to distinguish an unfilled buffer from a valid read or
** unread buffer. This is a transient state that can no longer be re-entered once exited.
** BUFFER_UNREAD indicates that there is an available and unread data buffer. The state
** of the buffer is set to BUFFER_READ only when an application task calling one of the
** OSGetBuffer() functions and specifies OS_READ_ONLY_ONCE.
** BUFFER_READ indicates that the latest and most recent available data buffer has al-
** ready been read. */
#define BUFFER_INIT   0x1
#define BUFFER_UNREAD 0x2
#define BUFFER_READ   0x4


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
  if ((descriptor = (BUFFER_DESCRIPTOR *)OSMalloc(sizeof(BUFFER_DESCRIPTOR))) == NULL)
     return NULL;
  descriptor->Status = BUFFER_INIT;
  descriptor->BufferSize = bufferSize;
  descriptor->BufferSlotType = bufferSlotType;
  descriptor->EventQueue = eventQueue;
  switch (bufferSlotType) {
     case OS_BUFFER_TYPE_4_SLOT:
        if ((descriptor->Buffer = OSMalloc(sizeof(BUFFER_4_SLOT))) == NULL)
           return NULL;
        buffer4 = (BUFFER_4_SLOT *)descriptor->Buffer;
        buffer4->Reading = 0;  buffer4->Latest = 0;
        buffer4->CurrentWriterPair = 1;
        buffer4->CurrentWriterIndex = !(buffer4->Index[1]);
        buffer4->CurrentWriter = &buffer4->Slot[1][buffer4->CurrentWriterIndex];
        buffer4->CurrentWriter->BufferItems = 0;
        for (i = 0; i < 2; i++)  // Allocate the buffer of each slot
           for (bufferSize = 0; bufferSize < 2; bufferSize++)
              if ((buffer4->Slot[i][bufferSize].Data = (UINT8 *)OSMalloc(descriptor->BufferSize)) == NULL)
                 return NULL;
        break;
     case OS_BUFFER_TYPE_3_SLOT:
        if ((descriptor->Buffer = OSMalloc(sizeof(BUFFER_3_SLOT))) == NULL)
           return NULL;
        buffer3 = (BUFFER_3_SLOT *)descriptor->Buffer;
        buffer3->Reading = 3;  buffer3->Latest = 0;
        buffer3->CurrentWriterIndex = 2;
        buffer3->CurrentWriter = &buffer3->Slot[2];
        buffer3->CurrentWriter->BufferItems = 0;
        for (i = 0; i < 3; i++)  // Allocate the buffer of each slot
           if ((buffer3->Slot[i].Data = (UINT8 *)OSMalloc(descriptor->BufferSize)) == NULL)
              return NULL;
     default: break;
  }
  return descriptor;
} /* end of OSInitBuffer */


/* OSWriteBuffer: Copies the data to the current writer buffer and truncates those that
** exceeds the buffer size.
** Parameters:
**   (1) (void *) a buffer descriptor created by OSInitBuffer();
**   (2) (UINT8 *) data to copy into the writer buffer;
**   (3) (UINT8) number of data bytes to consider.
** Returned value: (UINT8 ) number of bytes accepted into the buffer. */
UINT8 OSWriteBuffer(void *descriptor, UINT8 *data, UINT8 size)
{
  static const UINT8 next [4][3] = {{1,2,1},{2,2,0},{1,0,0},{1,2,0}};
  BUFFER_DESCRIPTOR *descript = (BUFFER_DESCRIPTOR *)descriptor;
  BUFFER_DATA *element;
  UINT8 i = 0;
  if (descript != NULL) { // Check that the port was initialized
     element = ((BUFFER_4_SLOT *)descript->Buffer)->CurrentWriter;
     for ( ; i < size && i < descript->BufferSize &&
                                      element->BufferItems != descript->BufferSize; i++)
        element->Data[element->BufferItems++] = data[i]; // copy the byte
     if (element->BufferItems == descript->BufferSize) {
        /* Indicate to the reader that a full buffer is now ready for reading */
        descript->Status = BUFFER_UNREAD;
        /* Get a new slot for the next time the writer is invoked. */
        if (descript->BufferSlotType == OS_BUFFER_TYPE_4_SLOT) {
           BOOL wpair;
           BUFFER_4_SLOT *buf = (BUFFER_4_SLOT*)((BUFFER_DESCRIPTOR*)descriptor)->Buffer;
           wpair = buf->CurrentWriterPair;      // Continue with the previous buffer pair
           buf->Index[wpair] = buf->CurrentWriterIndex; // Writer indicates slot
           buf->Latest = wpair;                         // Writer indicates pair
           /* Prepare for the next time the writer gets a new byte. */
           buf->CurrentWriterPair = !buf->Reading;
           buf->CurrentWriterIndex = !buf->Index[wpair];
           buf->CurrentWriter = &buf->Slot[buf->CurrentWriterPair][buf->CurrentWriterIndex];
           buf->CurrentWriter->BufferItems = 0;
        }
        else {
           UINT8 windex;
           BUFFER_3_SLOT *buffer;
           buffer = (BUFFER_3_SLOT*)((BUFFER_DESCRIPTOR*)descriptor)->Buffer;
           buffer->Latest = windex = buffer->CurrentWriterIndex;
           if (OSUINT8_LL(&buffer->Reading) == 3)
              OSUINT8_SC(&buffer->Reading,windex);
           /* Prepare for the next time the writer gets a new byte. */
           buffer->CurrentWriterIndex = windex = next[buffer->Reading][buffer->Latest];
           buffer->CurrentWriter = &buffer->Slot[windex];
           buffer->CurrentWriter->BufferItems = 0;
        }
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
  /* Check that the port was allocated and that there is something to read. */
  if (descript != NULL && descript->Status != BUFFER_INIT) {
     if (!readMode) {
        if (OSUINT8_LL(&descript->Status) != BUFFER_UNREAD)
           goto fail;
        if (!OSUINT8_SC(&descript->Status,BUFFER_READ))
           goto fail;
     }
     /* Get the slot holding the most recent written data. */
     if (descript->BufferSlotType == OS_BUFFER_TYPE_4_SLOT)
        buffer = GetReadyBuffer4Slot(descript);
     else
        buffer = GetReadyBuffer3Slot(descript);
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
  /* Check that the port was allocated and that there is something to read. */
  if (descript != NULL && descript->Status != BUFFER_INIT) {
     if (!readMode) {
        if (OSUINT8_LL(&descript->Status) != BUFFER_UNREAD)
           goto fail;
        if (!OSUINT8_SC(&descript->Status,BUFFER_READ))
           goto fail;
     }
     if (descript->BufferSlotType == OS_BUFFER_TYPE_4_SLOT)
        buffer = GetReadyBuffer4Slot(descript);
     else
        buffer = GetReadyBuffer3Slot(descript);
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
  buffer->Reading = rpair;        // Reader indicates pair
  rindex = buffer->Index[rpair];  // Reader chooses slot
  return &buffer->Slot[rpair][rindex];
} /* end of GetReadyBuffer4Slot */


/* GetReadyBuffer3Slot: Same as GetReadyBuffer4Slot() but applied to a 3-slot mechanism.
** Parameter: (void *) General buffer descriptor created and returned by OSInitBuffer().
** Returned value: Next available buffer slot to read from. */
BUFFER_DATA *GetReadyBuffer3Slot(BUFFER_DESCRIPTOR *descriptor)
{
  BUFFER_3_SLOT *buffer = (BUFFER_3_SLOT *)descriptor->Buffer;
  buffer->Reading = 3;
  if (OSUINT8_LL(&buffer->Reading) == 3)
     OSUINT8_SC(&buffer->Reading,buffer->Latest);
  return &buffer->Slot[buffer->Reading];
} /* end of GetReadyBuffer3Slot */

#endif /* ZOTTAOS_VERSION_HARD */
