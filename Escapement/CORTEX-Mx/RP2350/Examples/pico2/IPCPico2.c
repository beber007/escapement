/* Copyright (c) 2026 Bertrand Hurst. All rights reserved.
** Escapement - Lightweight Power-Aware Real-Time OS.
** Distributed under the terms of LICENSE at the root of this repository.
*/
/* File IPCPico2.c: The FIFO queue and a slot buffer used by tasks that preempt one
** another in the middle of their operations, on one core.
**
** A long task, Filler, of period 5 ms, spends 3 ms of each instance putting records in
** a FIFO queue and writing them to a 3-slot buffer, as fast as it can. A short task, Poker, of
** period 1 ms and higher priority, preempts it wherever it stands: it puts a record of
** its own in the queue and takes the latest record from the buffer with OS_READ_ONLY_ONCE
** through OSGetReferenceBuffer. An event-driven task, Drainer, which both signal with
** OSScheduleSuspendedTask and whose priority is higher still, empties the queue.
**
** A task preempted inside OSEnqueueFIFO or OSDequeueFIFO leaves its operation posted,
** and the task that preempts it completes that operation before its own: the code that
** does so runs only under preemption, which the host test reaches by hand
** (test/host/test_ipc.c) and this reaches as a task set does. The Drainer checks that
** the records of each producer come out once and in order; the Poker, that the buffer
** never gives it a record twice nor a torn one. Each task also leaves registers R8-R11
** changed as it ends, which a context switch must not hand to the task it resumes: the
** Filler checks them around a delay (CheckHighRegisters). The counts sit in Results, which the
** Renode suite reads (escapement_pico2.robot) and SWD reads on the board.
** Platform version: RP2350 (Raspberry Pi Pico 2).
*/

#include "Escapement.h"
#include "Escapement_Timer.h"   /* _OSGetActualTime, to bound the Filler's work in time */

#define NODES      8             /* places in the queue, and nodes in its pool */
#define FILL_TIME  3000          /* ticks of 1 us the Filler works per instance, so that
                                ** the Poker's arrivals fall inside its operations */

typedef struct {
  UINT32 Producer, Sequence;
} RECORD;

/* What the Filler writes to the buffer: a counter and its complement, which a slot made
** of two writes would not match. */
typedef struct {
  UINT32 Counter, Complement;
} SLOT;

/* Per producer, 0 the Filler and 1 the Poker: records put and taken; then what went
** wrong, and how often the Filler found the queue full. */
volatile struct {
  UINT32 Put[2], Taken[2], OutOfOrder, Full;
  UINT32 SlotReads, SlotRepeats, SlotTorn;
  UINT32 Registers;
} Results;

static void *Queue, *Slots, *Drain;

static void FillerTask(void *argument);
static void PokerTask(void *argument);
static void DrainerTask(void *argument);
static UINT32 CheckHighRegisters(UINT32 seed);
static void ClobberHighRegisters(void);

#define VTOR              *((volatile UINT32 *)0xE000ED08)


int main(void)
{
  extern void (* const CortexMxVectorTable[])(void);
  VTOR = (UINT32)CortexMxVectorTable;
  OSInitializeSystemClocks();
  Queue = OSInitFIFOQueue(NODES,sizeof(RECORD));
  Slots = OSInitBuffer(sizeof(SLOT),OS_BUFFER_TYPE_3_SLOT,NULL);
  Drain = OSCreateEventDescriptor();
  /* Deadlines, and under deadline-monotonic scheduling priorities: the Drainer first,
  ** then the Poker, then the Filler. */
  #if defined(ESCAPEMENT_VERSION_SOFT)
     OSCreateSynchronousTask(DrainerTask,50,400,64,Drain,NULL);
     OSCreateTask(PokerTask,50,0,1000,1000,1,1,0,NULL);
     OSCreateTask(FillerTask,3000,0,5000,5000,1,1,0,NULL);
  #else
     OSCreateSynchronousTask(DrainerTask,400,Drain,NULL);
     OSCreateTask(PokerTask,0,1000,1000,NULL);
     OSCreateTask(FillerTask,0,5000,5000,NULL);
  #endif
  return OSStartMultitasking(NULL,NULL);
} /* end of main */


/* Put: Puts the next record of a producer in the queue, if a node is free and a place
** too; returns whether it did. */
static BOOL Put(UINT32 producer)
{
  static UINT32 next[2] = {1, 1};
  RECORD *node;
  if ((node = (RECORD *)OSGetFreeNodeFIFO(Queue)) == NULL)
     return FALSE;
  node->Producer = producer;
  node->Sequence = next[producer];
  if (!OSEnqueueFIFO(Queue,node,sizeof(RECORD))) {
     OSReleaseNodeFIFO(Queue,node);
     return FALSE;
  }
  next[producer] += 1;
  Results.Put[producer] += 1;
  return TRUE;
} /* end of Put */


/* FillerTask: The long task, preempted in the middle of its operations. */
static void FillerTask(void *argument)
{
  static SLOT written = {0, ~0u};
  INT32 start = _OSGetActualTime();
  (void)argument;
  while (_OSGetActualTime() - start < FILL_TIME) {
     if (!Put(0))
        Results.Full += 1;
     OSScheduleSuspendedTask(Drain);
     written.Counter += 1;
     written.Complement = ~written.Counter;
     OSWriteBuffer(Slots,(UINT8 *)&written,sizeof written);
     Results.Registers += CheckHighRegisters(written.Counter);
  }
  OSEndTask();
} /* end of FillerTask */


/* PokerTask: The short task. A slot read with OS_READ_ONLY_ONCE must be newer than the
** last one read: a slot delivered twice gives it again. */
static void PokerTask(void *argument)
{
  static UINT32 last = 0;
  SLOT *slot;
  (void)argument;
  Put(1);
  OSScheduleSuspendedTask(Drain);
  if (OSGetReferenceBuffer(Slots,OS_READ_ONLY_ONCE,(UINT8 **)&slot) == sizeof(SLOT)) {
     Results.SlotReads += 1;
     if (slot->Complement != ~slot->Counter)
        Results.SlotTorn += 1;
     else if (slot->Counter <= last)
        Results.SlotRepeats += 1;
     last = slot->Counter;
  }
  ClobberHighRegisters();
  OSEndTask();
} /* end of PokerTask */


/* DrainerTask: Takes every record queued, each producer's in the order put. */
static void DrainerTask(void *argument)
{
  static UINT32 last[2] = {0, 0};
  RECORD *node;
  UINT16 size;
  (void)argument;
  while ((node = (RECORD *)OSDequeueFIFO(Queue,&size)) != NULL) {
     if (size != sizeof(RECORD) || node->Producer > 1 ||
         node->Sequence != last[node->Producer] + 1)
        Results.OutOfOrder += 1;
     else {
        last[node->Producer] = node->Sequence;
        Results.Taken[node->Producer] += 1;
     }
     OSReleaseNodeFIFO(Queue,node);
  }
  ClobberHighRegisters();
  OSSuspendSynchronousTask();
} /* end of DrainerTask */


/* CheckHighRegisters: Sets R8-R11 from a seed, waits long enough for the Poker and the
** Drainer to preempt the Filler there now and then, and returns how many of them changed
** meanwhile. On the Cortex-M0+, which reaches R8-R11 only with MOV, the context switch
** once saved R4-R7 alone (Escapement_CortexMx_a.S). */
static UINT32 CheckHighRegisters(UINT32 seed)
{
  UINT32 changed;
  __asm volatile (
     "  .syntax unified       \n"   /* GCC hands Thumb-1 inline assembly over divided */
     "  mov  r4, %[seed]      \n"
     "  mov  r8, r4           \n"
     "  adds r4, #1           \n"
     "  mov  r9, r4           \n"
     "  adds r4, #1           \n"
     "  mov  r10, r4          \n"
     "  adds r4, #1           \n"
     "  mov  r11, r4          \n"
     "  movs r5, #200         \n"
     "1: subs r5, #1          \n"
     "  bne  1b               \n"
     "  movs %[changed], #0   \n"
     "  mov  r4, %[seed]      \n"
     "  cmp  r8, r4           \n"
     "  beq  2f               \n"
     "  adds %[changed], #1   \n"
     "2: adds r4, #1          \n"
     "  cmp  r9, r4           \n"
     "  beq  3f               \n"
     "  adds %[changed], #1   \n"
     "3: adds r4, #1          \n"
     "  cmp  r10, r4          \n"
     "  beq  4f               \n"
     "  adds %[changed], #1   \n"
     "4: adds r4, #1          \n"
     "  cmp  r11, r4          \n"
     "  beq  5f               \n"
     "  adds %[changed], #1   \n"
     "5:                      \n"
     : [changed] "=&l" (changed)
     : [seed] "l" (seed)
     : "r4", "r5", "r8", "r9", "r10", "r11", "cc");
  return changed;
} /* end of CheckHighRegisters */


/* ClobberHighRegisters: Leaves R8-R11 changed; the task calling it ends without
** restoring them, as a task ending in the middle of a function that uses them does. */
static void __attribute__((noinline, naked)) ClobberHighRegisters(void)
{
  __asm volatile (
     "  .syntax unified      \n"
     "  movs r0, #0          \n"
     "  mvns r0, r0          \n"
     "  mov  r8, r0          \n"
     "  mov  r9, r0          \n"
     "  mov  r10, r0         \n"
     "  mov  r11, r0         \n"
     "  bx   lr              \n");
} /* end of ClobberHighRegisters */
