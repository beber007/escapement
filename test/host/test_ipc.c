/* Copyright (c) 2026 Bertrand Hurst. All rights reserved.
** Escapement - Lightweight Power-Aware Real-Time OS.
** Distributed under the terms of LICENSE at the root of this repository.
*/
/* File test_ipc.c: Exercises the inter-task communication of the kernel on the host: the
** concurrent FIFO queue, the queue between the cores of the RP2350, and the 3- and 4-slot
** buffers.
**
** Both are data structures that need no scheduling, so they are driven directly. What
** this cannot show is their behaviour under preemption: the host runs single threaded.
** It can make store-conditionals fail, as an interrupt between an LL and its SC does on
** the target (see host_port.c); test/model explores the interleavings themselves.
*/

#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "Escapement.h"
#include "../../Escapement/CORTEX-Mx/RP2350/Escapement_CoreQueue.h"

static unsigned Failures = 0;

static void Check(const char *what, int ok)
{
  printf("%-58s %s\n", what, ok ? "ok" : "FAILED");
  if (!ok) Failures += 1;
}


/* QUEUE BETWEEN THE CORES (RP2350) ------------------------------------------------------ */
/* One thread here: the order of items, the full and the empty queue, the indices going
** round, and the retries of every SC — test/model/fifo_mp.py explores the two cores. */
#define CORE_LENGTH 4

static void TestCoreQueue(void)
{
  void *queue = OSInitCoreQueue(CORE_LENGTH);
  static int items[CORE_LENGTH + 1];
  int model[CORE_LENGTH], head = 0, count = 0;
  unsigned i, round, n, ok;

  printf("\nqueue between the cores, %d places\n\n", CORE_LENGTH);
  Check("  a length that is not a power of 2 is refused", OSInitCoreQueue(3) == NULL);
  Check("  an empty queue dequeues nothing, NULL is refused",
        OSDequeueCoreQueue(queue) == NULL && !OSEnqueueCoreQueue(queue, NULL));
  for (i = 0, ok = 1; i < CORE_LENGTH; i += 1)
     ok = ok && OSEnqueueCoreQueue(queue, &items[i]);
  Check("  it takes as many items as it has places, then refuses",
        ok && !OSEnqueueCoreQueue(queue, &items[CORE_LENGTH]));
  for (i = 0, ok = 1; i < CORE_LENGTH; i += 1)
     ok = ok && OSDequeueCoreQueue(queue) == &items[i];
  Check("  and gives them back in order", ok && OSDequeueCoreQueue(queue) == NULL);

  /* Rounds of a few enqueues and dequeues, checked against a plain ring, the indices
  ** going round the array many times. */
  for (round = 0, ok = 1; round < 1000 && ok; round += 1) {
     for (n = round % 3 + 1; n > 0; n -= 1) {
        int accepted = OSEnqueueCoreQueue(queue, &items[round % (CORE_LENGTH + 1)]);
        ok = ok && accepted == (count < CORE_LENGTH);
        if (accepted)
           model[(head + count++) % CORE_LENGTH] = round % (CORE_LENGTH + 1);
     }
     for (n = round % 4; n > 0; n -= 1) {
        void *item = OSDequeueCoreQueue(queue);
        ok = ok && (count ? item == &items[model[head]] : item == NULL);
        if (count) { head = (head + 1) % CORE_LENGTH; count -= 1; }
     }
  }
  Check("  a thousand rounds against a plain queue", ok);
  while (OSDequeueCoreQueue(queue) != NULL);

  /* The SC that publishes an item goes through; the one that advances Tail fails, twice:
  ** an enqueue that tried it once would leave Tail behind, and the item unseen. */
  HostPassingSC = 1; HostFailingSC = 2;
  ok = OSEnqueueCoreQueue(queue, &items[0]);
  Check("  Tail advanced although its SC failed twice",
        ok && OSDequeueCoreQueue(queue) == &items[0] && OSDequeueCoreQueue(queue) == NULL);
  OSEnqueueCoreQueue(queue, &items[1]);
  HostPassingSC = 1; HostFailingSC = 2;
  ok = OSDequeueCoreQueue(queue) == &items[1];
  Check("  Head advanced although its SC failed twice",
        ok && OSDequeueCoreQueue(queue) == NULL && OSEnqueueCoreQueue(queue, &items[2]) &&
        OSDequeueCoreQueue(queue) == &items[2]);
  HostPassingSC = 0; HostFailingSC = 3;
  ok = OSEnqueueCoreQueue(queue, &items[3]) && OSDequeueCoreQueue(queue) == &items[3];
  Check("  a failed SC on a place is tried again", ok && HostFailingSC == 0);
}


/* FIFO QUEUE ---------------------------------------------------------------------------- */
#define NODES     4
#define NODE_SIZE 8

static void TestFIFO(void)
{
  void *queue = OSInitFIFOQueue(NODES, NODE_SIZE), *other = OSInitFIFOQueue(1, NODE_SIZE);
  void *nodes[NODES + 1];
  UINT8 *node;
  UINT16 size;
  unsigned i, j, round, n, ok;

  printf("\nFIFO queue of %d nodes of %d bytes\n\n", NODES, NODE_SIZE);

  for (i = 0; i <= NODES; i += 1)
     nodes[i] = OSGetFreeNodeFIFO(queue);
  ok = nodes[NODES] == NULL;
  for (i = 0; i < NODES; i += 1)
     for (j = 0; j < i; j += 1)
        if (nodes[i] == NULL || nodes[i] == nodes[j])
           ok = 0;
  Check("  the free pool holds exactly the nodes asked for", ok);
  /* A node must take a word written whole: the Cortex-M0+ faults on an unaligned one. */
  for (i = 0, ok = 1; i < NODES; i += 1)
     if ((UINTPTR)nodes[i] % sizeof(UINTPTR) != 0)
        ok = 0;
  Check("  every node is aligned for a pointer-sized access", ok);
  for (i = 0; i < NODES; i += 1)
     OSReleaseNodeFIFO(queue, nodes[i]);

  Check("  an empty queue dequeues nothing", OSDequeueFIFO(queue, &size) == NULL);
  /* A length of 0 would make every index modulo 0. */
  Check("  a queue of no node is refused", OSInitFIFOQueue(0, NODE_SIZE) == NULL);

  /* A full queue refuses a node, here one borrowed from another queue. */
  for (i = 0; i < NODES; i += 1)
     OSEnqueueFIFO(queue, OSGetFreeNodeFIFO(queue), (UINT16)i);
  node = OSGetFreeNodeFIFO(other);
  Check("  a full queue refuses one more node", !OSEnqueueFIFO(queue, node, 99));
  OSReleaseNodeFIFO(other, node);
  ok = 1;
  for (i = 0; i < NODES; i += 1) {
     if ((node = OSDequeueFIFO(queue, &size)) == NULL || size != i)
        ok = 0;
     else
        OSReleaseNodeFIFO(queue, node);
  }
  Check("  and still holds its nodes, in order, and nothing else",
        ok && OSDequeueFIFO(queue, &size) == NULL);

  /* Enough rounds for the head and tail indices to wrap around their 16-bit range. */
  ok = 1;
  for (round = 0; round < 40000; round += 1) {
     n = 1 + round % NODES;
     for (i = 0; i < n; i += 1) {
        if ((node = OSGetFreeNodeFIFO(queue)) == NULL) {
           ok = 0;
           break;
        }
        memset(node, (UINT8)(round + i), NODE_SIZE);
        if (!OSEnqueueFIFO(queue, node, (UINT16)(round + i)))
           ok = 0;
     }
     for (i = 0; i < n; i += 1) {
        node = OSDequeueFIFO(queue, &size);
        if (node == NULL || size != (UINT16)(round + i) ||
            node[0] != (UINT8)(round + i) || node[NODE_SIZE - 1] != (UINT8)(round + i))
           ok = 0;
        if (node != NULL)
           OSReleaseNodeFIFO(queue, node);
     }
     if (OSDequeueFIFO(queue, &size) != NULL)
        ok = 0;
  }
  Check("  nodes come out in order, with their data and size", ok);
}


/* SLOT BUFFERS -------------------------------------------------------------------------- */
#define SLOT 3

/* TestBuffer: One writer, one reader, taken through the states a slot buffer can be in. */
static void TestBuffer(UINT8 type, const char *name)
{
  void *buffer = OSInitBuffer(SLOT, type, NULL);
  UINT8 bytes[2 * SLOT + 1], copy[SLOT], *ref;
  unsigned i, round, ok;

  printf("\n%s buffer of %d bytes\n\n", name, SLOT);
  for (i = 0; i < sizeof bytes; i += 1)
     bytes[i] = (UINT8)(i + 1);

  Check("  nothing to read before the first full slot",
        OSGetCopyBuffer(buffer, OS_READ_MULTIPLE, copy) == 0 &&
        OSGetReferenceBuffer(buffer, OS_READ_MULTIPLE, &ref) == 0 && ref == NULL);

  ok = OSWriteBuffer(buffer, bytes, SLOT - 1) == SLOT - 1;
  ok = ok && OSGetCopyBuffer(buffer, OS_READ_ONLY_ONCE, copy) == 0;
  Check("  a slot filled in part cannot be read", ok);

  ok = OSWriteBuffer(buffer, bytes + SLOT - 1, 4) == 1;
  Check("  a write stops at the end of the slot", ok);

  ok = OSGetCopyBuffer(buffer, OS_READ_ONLY_ONCE, copy) == SLOT && memcmp(copy, bytes, SLOT) == 0;
  Check("  the full slot is read once", ok && OSGetCopyBuffer(buffer, OS_READ_ONLY_ONCE, copy) == 0);

  ok = OSGetReferenceBuffer(buffer, OS_READ_MULTIPLE, &ref) == SLOT && memcmp(ref, bytes, SLOT) == 0;
  ok = ok && OSGetCopyBuffer(buffer, OS_READ_MULTIPLE, copy) == SLOT;
  Check("  and again as often as asked with OS_READ_MULTIPLE", ok);

  /* Two slots written before the reader looks: it gets the latest only. */
  OSWriteBuffer(buffer, bytes + SLOT, SLOT);
  OSWriteBuffer(buffer, bytes, SLOT);
  ok = OSGetReferenceBuffer(buffer, OS_READ_ONLY_ONCE, &ref) == SLOT && memcmp(ref, bytes, SLOT) == 0;
  Check("  the reader gets the most recent slot", ok);

  /* An interrupt between an LL and its SC makes the SC fail although nothing changed:
  ** the reader must try again, not give up or take a slot that does not exist. */
  OSWriteBuffer(buffer, bytes + SLOT, SLOT);
  HostFailingSC = 1;
  ok = OSGetCopyBuffer(buffer, OS_READ_ONLY_ONCE, copy) == SLOT && memcmp(copy, bytes + SLOT, SLOT) == 0;
  HostFailingSC = 1;
  ok = ok && OSGetCopyBuffer(buffer, OS_READ_MULTIPLE, copy) == SLOT &&
       memcmp(copy, bytes + SLOT, SLOT) == 0;
  HostFailingSC = 2;
  ok = ok && OSGetReferenceBuffer(buffer, OS_READ_MULTIPLE, &ref) == SLOT &&
       memcmp(ref, bytes + SLOT, SLOT) == 0;
  HostFailingSC = 0;
  Check("  a store-conditional that fails is tried again", ok);

  /* Reads and writes interleaved so that the slots rotate through every combination: the
  ** writer must never write into the slot the reader holds, however many slots it fills
  ** meanwhile. */
  ok = 1;
  for (round = 0; round < 1000; round += 1) {
     UINT8 slot[SLOT];
     for (i = 0; i < SLOT; i += 1)
        slot[i] = (UINT8)(round * SLOT + i);
     if (OSWriteBuffer(buffer, slot, SLOT) != SLOT)
        ok = 0;
     if (round % 3 != 2) {
        if (OSGetReferenceBuffer(buffer, OS_READ_ONLY_ONCE, &ref) != SLOT)
           ok = 0;
        else {
           /* The reader holds this slot while the writer fills the next three. */
           for (i = 0; i < 3; i += 1)
              OSWriteBuffer(buffer, bytes + i, SLOT);
           if (memcmp(ref, slot, SLOT) != 0)
              ok = 0;
        }
     }
  }
  Check("  the writer never touches the slot being read", ok);
}


/* TestPublication: The reader comes in the middle of a write, as a higher-priority task
** or an interrupt preempting the writer does: at the writer's first memory barrier,
** after the slot is full and before it is handed over. The reader must then get nothing
** new, and the slot must still reach it once the writer is done. Blocks are filled with
** 0xA5 rather than zeros, as SRAM is on the target: a reader that took a slot never
** written would find a length there. */
static void *Race;
static UINT8 RaceCopy[256], RaceGot;
static void ReaderInWindow(void)
{
  HostBarrierHook = NULL;
  RaceGot = OSGetCopyBuffer(Race, OS_READ_ONLY_ONCE, RaceCopy);
}

static void TestPublication(UINT8 type, const char *name)
{
  UINT8 a[SLOT] = {1, 2, 3}, b[SLOT] = {4, 5, 6}, copy[256];
  unsigned ok;

  printf("\n%s buffer, a reader preempting the writer\n\n", name);
  HostMallocFill = 0xA5;
  Race = OSInitBuffer(SLOT, type, NULL);
  HostMallocFill = -1;
  HostBarrierHook = ReaderInWindow;
  OSWriteBuffer(Race, a, SLOT);
  ok = RaceGot == 0 || (RaceGot == SLOT && memcmp(RaceCopy, a, SLOT) == 0);
  Check("  in the first write, it gets nothing or that slot", ok);
  ok = RaceGot == SLOT ||
       (OSGetCopyBuffer(Race, OS_READ_ONLY_ONCE, copy) == SLOT && memcmp(copy, a, SLOT) == 0);
  Check("  and the slot is delivered", ok);

  OSGetCopyBuffer(Race, OS_READ_ONLY_ONCE, copy);
  HostBarrierHook = ReaderInWindow;
  OSWriteBuffer(Race, b, SLOT);
  ok = RaceGot == 0 || (RaceGot == SLOT && memcmp(RaceCopy, b, SLOT) == 0);
  Check("  later, it never gets the slot it has read again", ok);
  ok = RaceGot == SLOT ||
       (OSGetCopyBuffer(Race, OS_READ_ONLY_ONCE, copy) == SLOT && memcmp(copy, b, SLOT) == 0);
  Check("  and the new slot is delivered once", ok &&
        OSGetCopyBuffer(Race, OS_READ_ONLY_ONCE, copy) == 0);
  HostBarrierHook = NULL;
  Check("  a slot type that does not exist is refused", OSInitBuffer(SLOT, 2, NULL) == NULL);
}


/* The wait-free queue retries in loops that a broken index never leaves. */
static void Timeout(int signal)
{
  static const char message[] = "\nFAILED: the kernel did not return within 10 s\n";
  (void)signal;
  write(STDOUT_FILENO, message, sizeof message - 1);
  _exit(1);
}

int main(void)
{
  setvbuf(stdout, NULL, _IOLBF, 0);   /* keep what was printed if the kernel crashes */
  signal(SIGALRM, Timeout);
  alarm(10);
  TestFIFO();
  TestCoreQueue();
  TestBuffer(OS_BUFFER_TYPE_3_SLOT, "3-slot");
  TestBuffer(OS_BUFFER_TYPE_4_SLOT, "4-slot");
  TestPublication(OS_BUFFER_TYPE_3_SLOT, "3-slot");
  TestPublication(OS_BUFFER_TYPE_4_SLOT, "4-slot");
  printf("\n%s\n", Failures ? "FAILURES" : "all checks passed");
  return Failures ? 1 : 0;
}
