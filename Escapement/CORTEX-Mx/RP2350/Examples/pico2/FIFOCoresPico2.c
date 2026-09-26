/* Copyright (c) 2026 Bertrand Hurst. All rights reserved.
** Escapement - Lightweight Power-Aware Real-Time OS.
** Distributed under the terms of LICENSE at the root of this repository.
*/
/* File FIFOCoresPico2.c: The queue of Evéquoz between the two cores of the RP2350
** (Escapement_CoreQueue.c), each core a producer and a consumer of the same queues.
**
** A pool of nodes goes round. Each core takes a free node from one queue, fills it with a
** record of eight words, its own number then seven times its own counter, rising by one
** from each record to the next, and appends it to a second queue; and each takes records
** from the second queue, whichever core wrote them, and gives each node back to the
** first. Bare code does both on core 1, a task on core 0 each millisecond. So each queue
** has two producers and two consumers, one per core, as its model has
** (test/model/fifo_mp.py), their operations crossing in both directions.
**
** A queue must deliver every record once, in the order of its producer. Each consumer
** counts the records torn (words that differ) and those of a producer that come no later
** than the one it took from that producer before; and it sums, per producer, the records
** it took, their counters and their squares. Each producer stops after LIMIT records:
** once both queues are still, the two consumers' sums for a producer are those of 1 to
** LIMIT, each counter taken once, whoever took it.
**
** The counts sit in Results, to be read over SWD, on the board with an OpenOCD that
** knows the RP2350; escapement_pico2.robot reads them under Renode, with the exclusive
** monitor of the RP2350 played.
** Platform version: RP2350 (Raspberry Pi Pico 2).
*/

#include "Escapement.h"
#include "Escapement_Core1.h"
#include "Escapement_CoreQueue.h"

#define WORDS 8                  /* words per record: its producer, then its counter */
#define NODES 32                 /* nodes going round */
#define LIMIT 500                /* records each core writes */
#define READS_PER_RUN 32
#define WRITES_PER_RUN 16

typedef struct {
  UINT32 Word[WORDS];
} RECORD;

/* Per consumer (core 0, core 1), and per producer where [2]: what it took and found. */
typedef struct {
  UINT32 Taken[2], Sum[2], Squares[2], Torn, OutOfOrder;
} CONSUMER;

volatile struct {
  UINT32 Written[2];
  CONSUMER Consumer[2];
} Results;

static void *Free, *Full;
static UINT32 Core1Stack[256] __attribute__((aligned(8)));

static BOOL Produce(UINT32 core);
static BOOL Consume(UINT32 core);
static void Core1(void);
static void Core0Task(void *argument);

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
     OSCreateTask(Core0Task,0,0,1000,1000,1,1,0,NULL);
  #else
     OSCreateTask(Core0Task,0,1000,1000,NULL);
  #endif
  OSLaunchCore1(Core1,&Core1Stack[sizeof Core1Stack / sizeof Core1Stack[0]]);
  return OSStartMultitasking(NULL,NULL);
} /* end of main */


/* Produce: A record of this core's into a free node, and the node into Full; FALSE if
** no node was free or the core has written its LIMIT. */
static BOOL Produce(UINT32 core)
{
  RECORD *node;
  UINT32 counter = Results.Written[core] + 1, i;
  if (counter > LIMIT || (node = (RECORD *)OSDequeueCoreQueue(Free)) == NULL)
     return FALSE;
  node->Word[0] = core;
  for (i = 1; i < WORDS; i += 1)
     node->Word[i] = counter;
  while (!OSEnqueueCoreQueue(Full,node));   // never full: NODES places for NODES nodes
  Results.Written[core] = counter;
  return TRUE;
} /* end of Produce */


/* Consume: A record from Full, checked and counted, its node given back to Free; FALSE
** if Full was empty. */
static BOOL Consume(UINT32 core)
{
  static UINT32 last[2][2];      /* per consumer, the counter last taken of each producer */
  volatile CONSUMER *me = &Results.Consumer[core];
  RECORD *node;
  UINT32 producer, counter, i;
  if ((node = (RECORD *)OSDequeueCoreQueue(Full)) == NULL)
     return FALSE;
  producer = node->Word[0] & 1;
  counter = node->Word[1];
  for (i = 2; i < WORDS; i += 1)
     if (node->Word[i] != counter || node->Word[0] > 1) {
        me->Torn += 1;
        break;
     }
  if (counter <= last[core][producer])
     me->OutOfOrder += 1;
  last[core][producer] = counter;
  me->Taken[producer] += 1;
  me->Sum[producer] += counter;
  me->Squares[producer] += counter * counter;
  OSEnqueueCoreQueue(Free,node);
  return TRUE;
} /* end of Consume */


/* Core1: Bare code on core 1, a producer and a consumer by turns. */
static void Core1(void)
{
  while (TRUE) {
     Produce(1);
     Consume(1);
  }
} /* end of Core1 */


/* Core0Task: Core 0, each millisecond: a few records written, the waiting ones taken. */
static void Core0Task(void *argument)
{
  UINT32 n;
  (void)argument;
  for (n = 0; n < WRITES_PER_RUN && Produce(0); n += 1);
  for (n = 0; n < READS_PER_RUN && Consume(0); n += 1);
  OSEndTask();
} /* end of Core0Task */
