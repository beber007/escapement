/* Copyright (c) 2026 Bertrand Hurst. All rights reserved.
** Escapement - Lightweight Power-Aware Real-Time OS.
** Distributed under the terms of LICENSE at the root of this repository.
*/
/* File test_ipc.c: Exercises the inter-task communication of the kernel on the host: the
** concurrent FIFO queue and the 3- and 4-slot buffers.
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

static unsigned Failures = 0;

static void Check(const char *what, int ok)
{
  printf("%-58s %s\n", what, ok ? "ok" : "FAILED");
  if (!ok) Failures += 1;
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
  for (i = 0; i < NODES; i += 1)
     OSReleaseNodeFIFO(queue, nodes[i]);

  Check("  an empty queue dequeues nothing", OSDequeueFIFO(queue, &size) == NULL);

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
  TestBuffer(OS_BUFFER_TYPE_3_SLOT, "3-slot");
  TestBuffer(OS_BUFFER_TYPE_4_SLOT, "4-slot");
  printf("\n%s\n", Failures ? "FAILURES" : "all checks passed");
  return Failures ? 1 : 0;
}
