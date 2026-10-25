/* Copyright (c) 2026 Bertrand Hurst. All rights reserved.
** Escapement - Lightweight Power-Aware Real-Time OS.
** Distributed under the terms of LICENSE at the root of this repository.
*/
/* File LiveTasksPico.c: Tasks created and deleted while the kernel runs (make LIVE=4,
** EscapementHard.h, "LIVE TASKS"), over and over.
**
** A manager of 2 ms, made by OSCreateTask, turns round four slots: at each instance it
** deletes the worker of the next slot if it has one, and creates one otherwise, of a
** period of its own, 500, 700, 1100 or 1300 us. The worker of slot 3 is never deleted by
** the manager: it deletes itself at its fifth instance, inside it, and its block comes
** back at its next release, 1.3 ms later, before the manager comes round to the slot
** again: Refused stays 0 (on the Pico W and under Renode, 2026-10-24). Slot 0's first
** worker is created in main, before the kernel starts.
** Whenever the four slots are full, the manager asks for a fifth worker, which the pool of
** four must refuse.
**
** Each worker checks its own instances against the time it was created at: an instance
** due at the creation plus a whole number of periods, started within LATE_US of it. One
** due after its deletion returned is a ghost. Results counts them, for escapement_pico.robot
** or a debugger to read: Refused, Late, Ghosts and Errors must stay 0.
** Platform version: RP2040 (Raspberry Pi Pico).
*/

#include "Escapement.h"
#include "Escapement_Timer.h"   /* _OSGetActualTime */

#define VTOR *((volatile UINT32 *)0xE000ED08)

#ifndef ESCAPEMENT_LIVE_TASKS
   #error "LiveTasksPico is built with make LIVE=4"
#endif
/* A pool of four, which the checks of Results count on; tested in C, not in #if, which
** cppcheck takes with the macro defined to other values. */
typedef char LivePoolOfFour[ESCAPEMENT_LIVE_TASKS == 4 ? 1 : -1];

#define SLOTS     4
#define MANAGER_US 2000
#define LATE_US   200    /* the workers take a few us; the manager, the longest, some 20 */

typedef struct SLOT {
  void *Task;              /* the worker, NULL when the slot is empty */
  INT32 Period;
  volatile INT32 Due;      /* the worker's next instance, modulo 2^30 */
  volatile INT32 DeletedAt; /* read once the deletion returned, and the handler served it */
  volatile BOOL Deleting;
  volatile UINT32 Runs;    /* instances of the worker under way */
} SLOT;

static SLOT Slots[SLOTS];
static const INT32 Periods[SLOTS] = {500, 700, 1100, 1300};

volatile struct {
  UINT32 Cycles;           /* instances of the manager */
  UINT32 Created;
  UINT32 Deleted;          /* by the manager or by a worker itself */
  UINT32 Releases;         /* instances of the workers */
  UINT32 Refused;          /* a creation finding the pool empty */
  UINT32 Late;             /* an instance started early, or LATE_US past its due time */
  UINT32 Ghosts;           /* an instance due after its task's deletion was asked for */
  UINT32 Errors;           /* an answer of the kernel other than expected */
  UINT32 Blocks;           /* distinct blocks handed out, at most the pool's four */
  UINT32 Fifths;           /* fifth workers asked for, the four slots full */
} Results;

static void *Seen[SLOTS + 1];

static void ManagerTask(void *argument);
static void WorkerTask(void *argument);


/* Delete: Deletes the worker of a slot. */
static void Delete(SLOT *s)
{
  if (OSDeleteTaskLive(s->Task))
     Results.Deleted += 1;
  else
     Results.Errors += 1;
  s->DeletedAt = _OSGetActualTime();
  s->Deleting = TRUE;
  s->Task = NULL;
} /* end of Delete */


/* Since: a - b in the 30 bits the kernel counts time in, signed. */
static INT32 Since(INT32 a, INT32 b)
{
  return (INT32)((UINT32)(a - b) << 2) >> 2;
} /* end of Since */


/* Create: Creates the worker of a slot, due at now. */
static BOOL Create(UINT32 slot, INT32 now)
{
  SLOT *s = &Slots[slot];
  UINT32 i;
  s->Period = Periods[slot];
  s->Due = now;
  s->Deleting = FALSE;
  s->Runs = 0;
  if ((s->Task = OSCreateTaskLive(WorkerTask,0,s->Period,s->Period,s)) == NULL)
     return FALSE;
  Results.Created += 1;
  for (i = 0; i < Results.Blocks && Seen[i] != s->Task; i += 1);
  if (i == Results.Blocks) {
     if (i == SLOTS)
        Results.Errors += 1;   /* a fifth block */
     else
        Seen[Results.Blocks++] = s->Task;
  }
  return TRUE;
} /* end of Create */


int main(void)
{
  extern void (* const CortexMxVectorTable[])(void);
  VTOR = (UINT32)CortexMxVectorTable;
  OSInitializeSystemClocks();
  OSCreateTask(ManagerTask,0,MANAGER_US,MANAGER_US,NULL);
  /* Before the start: released at the first time the timer's handler reads. */
  if (!Create(0,0))
     Results.Errors += 1;
  return OSStartMultitasking(NULL,NULL);
} /* end of main */


/* ManagerTask: Deletes or creates the worker of the next slot. */
static void ManagerTask(void *argument)
{
  static UINT32 next = 1;
  SLOT *s = &Slots[next];
  UINT32 i, full;
  INT32 now = _OSGetActualTime();
  Results.Cycles += 1;
  if (s->Task != NULL) {
     if (next != 3)
        Delete(s);
  }
  else if (!Create(next,now))
     Results.Refused += 1;
  /* With the four slots full, a fifth worker must be refused. */
  for (i = 0, full = 0; i < SLOTS; i += 1)
     full += Slots[i].Task != NULL;
  if (full == SLOTS) {
     void *fifth = OSCreateTaskLive(WorkerTask,0,1000,1000,NULL);
     Results.Fifths += 1;
     if (fifth != NULL) {
        Results.Errors += 1;
        (void)OSDeleteTaskLive(fifth);
     }
  }
  next = (next + 1) % SLOTS;
  OSEndTask();
} /* end of ManagerTask */


/* WorkerTask: Checks that its instance is due when it should be, and that it is not one
** after its deletion. */
static void WorkerTask(void *argument)
{
  SLOT *s = (SLOT *)argument;
  INT32 now = _OSGetActualTime(), late;
  if (s != NULL) {             /* NULL: the fifth, which should not exist */
     Results.Releases += 1;
     late = Since(now, s->Due);
     if (late < 0 || late > LATE_US)
        Results.Late += 1;
     if (s->Deleting && Since(s->Due, s->DeletedAt) > 0)
        Results.Ghosts += 1;
     s->Due = (s->Due + s->Period) & 0x3FFFFFFF;
     s->Runs += 1;
     if (s == &Slots[3] && s->Runs == 5)
        Delete(s);
  }
  OSEndTask();
} /* end of WorkerTask */
