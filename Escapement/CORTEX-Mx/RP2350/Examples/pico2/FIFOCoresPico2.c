/* Copyright (c) 2026 Bertrand Hurst. All rights reserved.
** Escapement - Lightweight Power-Aware Real-Time OS.
** Distributed under the terms of LICENSE at the root of this repository.
*/
/* File FIFOCoresPico2.c: The queue of Evéquoz between the two cores of the RP2350
** (Escapement_CoreQueue.c), in both directions.
**
** A pool of nodes goes round: bare code on core 1 takes a free node from one queue, fills
** it with a record of eight words, all eight the same counter, rising by one from each
** record to the next, and appends it to a second queue; a task on core 0 takes the
** records from the second queue, 32 at most each millisecond, and gives each node back
** to the first. Unlike the slot buffers, which give the reader the latest record, a
** queue must deliver every record, once, in order: the task counts the records torn
** (words that differ), lost or repeated (a counter other than the last plus one), and
** those it finds while core 1 is still writing them, since a node is only enqueued once
** filled. test/model/fifo_mp.py explores every interleaving of two cores running the
** queue; this runs it, under Renode with the exclusive monitor of the RP2350 played
** (escapement_pico2.robot), and on the board.
**
** The counts sit in Results, to be read over SWD, on the board with an OpenOCD that
** knows the RP2350.
** Platform version: RP2350 (Raspberry Pi Pico 2).
*/

#include "Escapement.h"
#include "Escapement_Core1.h"
#include "Escapement_CoreQueue.h"

#define WORDS 8                  /* words per record */
#define NODES 32                 /* nodes going round */
#define READS_PER_RUN 32

typedef struct {
  UINT32 Word[WORDS];
} RECORD;

/* What core 1 has written, what the reader has taken, and what it found wrong. */
volatile struct {
  UINT32 Written, Taken, Torn, OutOfOrder, EmptyRuns;
} Results;

static void *Free, *Full;
static UINT32 Core1Stack[256] __attribute__((aligned(8)));

static void Writer(void);
static void ReaderTask(void *argument);

#define VTOR              *((volatile UINT32 *)0xE000ED08)
#define WATCHDOG_CTRL_CLR *((volatile UINT32 *)(0x400D8000 + 0x3000))
#define WATCHDOG_ENABLE   (1u << 30)


int main(void)
{
  extern void (* const CortexMxVectorTable[])(void);
  UINT32 i;
  VTOR = (UINT32)CortexMxVectorTable;
  /* A firmware in flash may have armed the watchdog, which a reset of the processors
  ** leaves running and which pauses only while the debugger holds a core. */
  WATCHDOG_CTRL_CLR = WATCHDOG_ENABLE;
  OSInitializeSystemClocks();
  Free = OSInitCoreQueue(NODES);
  Full = OSInitCoreQueue(NODES);
  for (i = 0; i < NODES; i += 1)
     OSEnqueueCoreQueue(Free,OSMalloc(sizeof(RECORD)));
  #if defined(ESCAPEMENT_VERSION_SOFT)
     OSCreateTask(ReaderTask,0,0,1000,1000,1,1,0,NULL);
  #else
     OSCreateTask(ReaderTask,0,1000,1000,NULL);
  #endif
  OSLaunchCore1(Writer,&Core1Stack[sizeof Core1Stack / sizeof Core1Stack[0]]);
  return OSStartMultitasking(NULL,NULL);
} /* end of main */


/* Writer: Core 1. Fills every free node with the next record and passes it on. */
static void Writer(void)
{
  RECORD *node;
  UINT32 counter = 0, i;
  while (TRUE) {
     if ((node = (RECORD *)OSDequeueCoreQueue(Free)) == NULL)
        continue;               // all nodes on their way: wait for core 0
     counter += 1;
     for (i = 0; i < WORDS; i += 1)
        node->Word[i] = counter;
     while (!OSEnqueueCoreQueue(Full,node));   // never full: NODES places for NODES nodes
     Results.Written = counter;
  }
} /* end of Writer */


/* ReaderTask: Core 0. Takes the records in order and gives their nodes back. */
static void ReaderTask(void *argument)
{
  static UINT32 last = 0;
  RECORD *node;
  UINT32 n, i;
  for (n = 0; n < READS_PER_RUN; n += 1) {
     if ((node = (RECORD *)OSDequeueCoreQueue(Full)) == NULL) {
        Results.EmptyRuns += n == 0;
        break;
     }
     Results.Taken += 1;
     for (i = 1; i < WORDS; i += 1)
        if (node->Word[i] != node->Word[0]) {
           Results.Torn += 1;
           break;
        }
     if (node->Word[0] != last + 1)
        Results.OutOfOrder += 1;
     last = node->Word[0];
     OSEnqueueCoreQueue(Free,node);
  }
  OSEndTask();
} /* end of ReaderTask */
