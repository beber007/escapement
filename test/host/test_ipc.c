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


/* TestCoreQueueInterleaved: The other core's operation run between an LL and its SC of
** this one, at each LL in turn: it finds a place taken but Tail or Head not yet moved,
** and moves it on itself (E12-E13, D12-D13). The queue must then hold what one of the two
** orders leaves, and each operation return what that order gives it. The interleavings
** the host reaches are those at an LL; test/model/fifo_mp.py explores them all. */
static void *CoreQ;
static int CoreItems[16], CoreInner, CoreInnerDone;
static void *CoreInnerGot;
static unsigned CoreAt, CoreCount;

static BOOL OtherCore(void)
{
  if (++CoreCount != CoreAt)
     return FALSE;
  HostLLHook = NULL;
  if (CoreInner)
     CoreInnerDone = OSEnqueueCoreQueue(CoreQ, &CoreItems[9]);
  else
     CoreInnerGot = OSDequeueCoreQueue(CoreQ);
  HostLLHook = OtherCore;
  return TRUE;
}

static void TestCoreQueueInterleaved(void)
{
  unsigned outer, inner, filled, at, i, reached, cases = 0, bad = 0;
  char label[80];
  printf("\nqueue between the cores, the other core's operation at each LL\n\n");
  for (outer = 0; outer < 2; outer += 1)
     for (inner = 0; inner < 2; inner += 1)
        for (filled = 0; filled <= CORE_LENGTH; filled += 1)
           for (at = 1, reached = 1; reached; at += 1) {
              void *got[CORE_LENGTH + 3], *outerGot = NULL;
              int outerDone = -1, k, ok[2];
              unsigned n;
              CoreQ = OSInitCoreQueue(CORE_LENGTH);
              for (i = 0; i < filled; i += 1)
                 OSEnqueueCoreQueue(CoreQ, &CoreItems[i]);
              CoreInner = (int)inner; CoreInnerDone = -1; CoreInnerGot = NULL;
              CoreAt = at; CoreCount = 0;
              HostLLHook = OtherCore;
              if (outer)
                 outerDone = OSEnqueueCoreQueue(CoreQ, &CoreItems[8]);
              else
                 outerGot = OSDequeueCoreQueue(CoreQ);
              HostLLHook = NULL;
              if (!(reached = CoreCount >= at))
                 break;
              cases += 1;
              for (n = 0; (got[n] = OSDequeueCoreQueue(CoreQ)) != NULL; n += 1);
              for (k = 0; k < 2; k += 1) {       /* k = 0: this core's operation first */
                 void *q[CORE_LENGTH + 2], *took[2] = {NULL, NULL};
                 int done[2] = {0, 0}, j;
                 unsigned m = 0;
                 for (i = 0; i < filled; i += 1)
                    q[m++] = &CoreItems[i];
                 for (j = 0; j < 2; j += 1) {
                    int mine = (k == 0) == (j == 0), isEnq = mine ? (int)outer : (int)inner;
                    if (isEnq && m < CORE_LENGTH) {
                       q[m++] = &CoreItems[mine ? 8 : 9];
                       done[mine] = 1;
                    }
                    else if (!isEnq && m > 0) {
                       took[mine] = q[0];
                       memmove(q, q + 1, (m - 1) * sizeof q[0]);
                       m -= 1;
                    }
                 }
                 ok[k] = m == n && (outer ? outerDone == done[1] : outerGot == took[1]) &&
                         (inner ? CoreInnerDone == done[0] : CoreInnerGot == took[0]);
                 for (i = 0; ok[k] && i < m; i += 1)
                    ok[k] = got[i] == q[i];
              }
              if (!ok[0] && !ok[1] && ++bad <= 5)
                 printf("  this core %s, the other %s, %u queued, at LL %u: wrong\n",
                        outer ? "enqueues" : "dequeues", inner ? "enqueues" : "dequeues", filled, at);
           }
  snprintf(label, sizeof label, "  every interleaving leaves one of the two orders: %u cases", cases);
  Check(label, bad == 0 && cases > 0);
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

  /* The indices started 50,000 below the value they wrap at, the queue being empty, so
  ** that the rounds take them across it: 2^32 operations would take too long. The first
  ** fields of the queue's descriptor, as the kernels lay them out (FIFOQUEUE). */
  {
     struct { void *Q, *PendingOp; UINT32 Head, Tail, MaxIndex; } *fifo = queue;
     fifo->Head = fifo->Tail = fifo->MaxIndex - 50000;
  }
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


/* TestFIFOPreempted: An operation interrupted between an LL and its SC by another on the
** same queue, as an interrupt handler preempting a task does: the interrupting one finds
** the first posted in PendingOp and completes it before its own, and the interrupted one,
** its SC failed, finds its work done. Each pair of operations, enqueue or dequeue, is
** interrupted at each of its LLs in turn; the queue must then hold what one of the two
** orders of the operations leaves, and each must return what that order gives it. */
#define PRE_NODES 4
static void *PreQueue, *PreNode[PRE_NODES + 2];
static int PreInner, PreInnerSize, PreInnerDone;      /* the interrupting operation */
static unsigned PreAt, PreCount;
static void *PreGot;
static UINT16 PreGotSize;

static BOOL Interrupt(void)
{
  if (++PreCount != PreAt)
     return FALSE;
  HostLLHook = NULL;
  if (PreInner)
     PreInnerDone = OSEnqueueFIFO(PreQueue, PreNode[PRE_NODES + 1], (UINT16)PreInnerSize);
  else
     PreGot = OSDequeueFIFO(PreQueue, &PreGotSize);
  HostLLHook = Interrupt;
  return TRUE;
}

/* Drain: What the queue holds, as sizes, oldest first; -1 ends. */
static void Drain(int *sizes)
{
  UINT8 *node;
  UINT16 size;
  int n = 0;
  while ((node = OSDequeueFIFO(PreQueue, &size)) != NULL) {
     sizes[n++] = size;
     OSReleaseNodeFIFO(PreQueue, node);
  }
  sizes[n] = -1;
}

static void TestFIFOPreempted(void)
{
  unsigned outer, inner, filled, at, i, reached, cases = 0, bad = 0;
  printf("\nFIFO queue, an operation interrupted by another at each of its LLs\n\n");
  for (outer = 0; outer < 2; outer += 1)            /* 1: enqueue, 0: dequeue */
     for (inner = 0; inner < 2; inner += 1)
        for (filled = 0; filled <= PRE_NODES; filled += 1)
           for (at = 1, reached = 1; reached; at += 1) {
              int got[PRE_NODES + 4], model1[PRE_NODES + 4], model2[PRE_NODES + 4];
              int q[PRE_NODES + 4], n = 0, k, outerDone = 0, ok1, ok2;
              void *outerGot = NULL;
              UINT16 outerSize = 0;
              PreQueue = OSInitFIFOQueue(PRE_NODES, 8);
              for (i = 0; i < PRE_NODES; i += 1)
                 PreNode[i] = OSGetFreeNodeFIFO(PreQueue);
              for (i = 0; i < filled; i += 1)
                 OSEnqueueFIFO(PreQueue, PreNode[i], (UINT16)(10 + i));
              /* Nodes for the operations: from another queue, if this one is empty. */
              PreNode[PRE_NODES] = filled < PRE_NODES ? PreNode[filled] : OSGetFreeNodeFIFO(OSInitFIFOQueue(2, 8));
              PreNode[PRE_NODES + 1] = OSGetFreeNodeFIFO(OSInitFIFOQueue(2, 8));
              PreInner = (int)inner; PreInnerSize = 99; PreInnerDone = -1; PreGot = NULL;
              PreAt = at; PreCount = 0;
              HostLLHook = Interrupt;
              if (outer)
                 outerDone = OSEnqueueFIFO(PreQueue, PreNode[PRE_NODES], 50);
              else
                 outerGot = OSDequeueFIFO(PreQueue, &outerSize);
              HostLLHook = NULL;
              reached = PreCount >= at;
              if (!reached)
                 break;
              cases += 1;
              Drain(got);
              /* The two orders, on a model of the queue: sizes 10.. then the operations. */
              for (k = 0; k < 2; k += 1) {
                 int *m = k ? model2 : model1, e1 = 0, d1 = -1, e2 = 0, d2 = -1, j;
                 n = 0;
                 for (i = 0; i < filled; i += 1)
                    q[n++] = 10 + (int)i;
                 for (j = 0; j < 2; j += 1) {
                    int first = (k == 0) == (j == 0);     /* k=0: outer first */
                    int isEnq = first ? (int)outer : (int)inner, size = first ? 50 : 99;
                    int done = 0, took = -1;
                    if (isEnq) {
                       if (n < PRE_NODES) { q[n++] = size; done = 1; }
                    }
                    else if (n > 0) {
                       took = q[0];
                       memmove(q, q + 1, (size_t)(n - 1) * sizeof q[0]);
                       n -= 1;
                    }
                    if (first) { e1 = done; d1 = took; } else { e2 = done; d2 = took; }
                 }
                 for (i = 0; i < (unsigned)n; i += 1)
                    m[i] = q[i];
                 m[n] = -1;
                 /* What each operation returned must match this order. */
                 m[PRE_NODES + 2] = (outer ? outerDone == e1 : (d1 < 0 ? outerGot == NULL : outerGot != NULL && outerSize == d1)) &&
                                    (inner ? PreInnerDone == e2 : (d2 < 0 ? PreGot == NULL : PreGot != NULL && PreGotSize == d2));
              }
              for (ok1 = model1[PRE_NODES + 2], i = 0; ok1 && (i == 0 || got[i - 1] != -1); i += 1)
                 ok1 = got[i] == model1[i];
              for (ok2 = model2[PRE_NODES + 2], i = 0; ok2 && (i == 0 || got[i - 1] != -1); i += 1)
                 ok2 = got[i] == model2[i];
              if (!ok1 && !ok2) {
                 bad += 1;
                 if (bad <= 5)
                    printf("  outer %s, inner %s, %u queued, interrupted at LL %u: wrong\n",
                           outer ? "enqueue" : "dequeue", inner ? "enqueue" : "dequeue", filled, at);
              }
           }
  char label[80];
  snprintf(label, sizeof label, "  every interruption leaves one of the two orders: %u cases", cases);
  Check(label, bad == 0 && cases > 0);
}


/* TestFIFONested: Three operations on one queue, each interrupting the one before at
** one of its LLs, as a task preempted by another that an interrupt preempts in turn:
** every pair of places, every kind of each operation. The middle one may find, while it
** helps the first, that the third has done that work already. The queue must hold what
** one of the six orders of the three operations leaves, and each must return what that
** order gives it. */
#define NEST_NODES 3
static void *NestQueue, *NestNode[4];
static int NestKind[3], NestDone[3];
static void *NestGot[3];
static UINT16 NestSize[3];
static unsigned NestAt[3], NestCount[3], NestLevel;

static void NestOperation(int op);
static BOOL NestInterrupt(void)
{
  int level = (int)NestLevel;
  if (level >= 2 || ++NestCount[level] != NestAt[level])
     return FALSE;
  NestLevel += 1;
  NestOperation(level + 1);
  NestLevel -= 1;
  return TRUE;
}

static void NestOperation(int op)
{
  if (NestKind[op])
     NestDone[op] = OSEnqueueFIFO(NestQueue, NestNode[op], (UINT16)(50 + op));
  else
     NestGot[op] = OSDequeueFIFO(NestQueue, &NestSize[op]);
}

static void TestFIFONested(void)
{
  static const int orders[6][3] = {{0,1,2},{0,2,1},{1,0,2},{1,2,0},{2,0,1},{2,1,0}};
  unsigned kinds, filled, a, b, i, cases = 0, bad = 0, reached;
  char label[80];
  printf("\nFIFO queue, three operations each interrupting the one before\n\n");
  for (kinds = 0; kinds < 8; kinds += 1)
     for (filled = 0; filled <= NEST_NODES; filled += 1)
        for (a = 1; a < 40; a += 1)
           for (b = 1, reached = 1; reached && b < 40; b += 1) {
              int got[NEST_NODES + 4], n, o, ok = 0;
              void *pool[NEST_NODES];
              NestQueue = OSInitFIFOQueue(NEST_NODES, 8);
              for (i = 0; i < NEST_NODES; i += 1)
                 pool[i] = OSGetFreeNodeFIFO(NestQueue);
              for (i = 0; i < filled; i += 1)
                 OSEnqueueFIFO(NestQueue, pool[i], (UINT16)(10 + i));
              for (i = 0; i < 3; i += 1) {
                 NestKind[i] = (int)(kinds >> i) & 1;
                 NestDone[i] = -1; NestGot[i] = NULL; NestSize[i] = 0; NestCount[i] = 0;
                 NestNode[i] = OSGetFreeNodeFIFO(OSInitFIFOQueue(1, 8));
              }
              NestAt[0] = a; NestAt[1] = b; NestLevel = 0;
              HostLLHook = NestInterrupt;
              NestOperation(0);
              HostLLHook = NULL;
              if (NestCount[0] < a) { reached = 0; if (b == 1) a = 40; continue; }
              reached = NestCount[1] >= b;
              if (!reached)
                 continue;
              cases += 1;
              for (n = 0; n < NEST_NODES + 3; n += 1) {
                 UINT16 size;
                 void *node = OSDequeueFIFO(NestQueue, &size);
                 if (node == NULL) break;
                 got[n] = size;
              }
              got[n] = -1;
              for (o = 0; o < 6 && !ok; o += 1) {
                 int q[NEST_NODES + 4], m = 0, j, good = 1;
                 for (i = 0; i < filled; i += 1)
                    q[m++] = 10 + (int)i;
                 for (j = 0; j < 3; j += 1) {
                    int op = orders[o][j];
                    if (NestKind[op]) {
                       int done = m < NEST_NODES;
                       if (done) q[m++] = 50 + op;
                       good = good && NestDone[op] == done;
                    }
                    else if (m > 0) {
                       good = good && NestGot[op] != NULL && NestSize[op] == q[0];
                       memmove(q, q + 1, (size_t)(m - 1) * sizeof q[0]);
                       m -= 1;
                    }
                    else
                       good = good && NestGot[op] == NULL;
                 }
                 for (i = 0; good && (int)i <= m; i += 1)
                    good = got[i] == ((int)i < m ? q[i] : -1);
                 ok = good;
              }
              if (!ok && ++bad <= 5)
                 printf("  kinds %u, %u queued, at LL %u then %u: no order fits\n",
                        kinds, filled, a, b);
           }
  snprintf(label, sizeof label, "  every nesting leaves one of the six orders: %u cases", cases);
  Check(label, bad == 0 && cases > 0);
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
static unsigned RaceBarrier, RaceAt;
static void ReaderInWindow(void)
{
  if (++RaceBarrier < RaceAt)
     return;
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
  RaceBarrier = 0; RaceAt = 1;
  HostBarrierHook = ReaderInWindow;
  OSWriteBuffer(Race, a, SLOT);
  ok = RaceGot == 0 || (RaceGot == SLOT && memcmp(RaceCopy, a, SLOT) == 0);
  Check("  in the first write, it gets nothing or that slot", ok);
  ok = RaceGot == SLOT ||
       (OSGetCopyBuffer(Race, OS_READ_ONLY_ONCE, copy) == SLOT && memcmp(copy, a, SLOT) == 0);
  Check("  and the slot is delivered", ok);

  OSGetCopyBuffer(Race, OS_READ_ONLY_ONCE, copy);
  RaceBarrier = 0; RaceAt = 1;
  HostBarrierHook = ReaderInWindow;
  OSWriteBuffer(Race, b, SLOT);
  ok = RaceGot == 0 || (RaceGot == SLOT && memcmp(RaceCopy, b, SLOT) == 0);
  Check("  later, it never gets the slot it has read again", ok);
  ok = RaceGot == SLOT ||
       (OSGetCopyBuffer(Race, OS_READ_ONLY_ONCE, copy) == SLOT && memcmp(copy, b, SLOT) == 0);
  Check("  and the new slot is delivered once", ok &&
        OSGetCopyBuffer(Race, OS_READ_ONLY_ONCE, copy) == 0);
  /* The slot before is left unread; the reader comes at the writer's last barrier, the
  ** new slot handed over and the buffer about to be said unread again. A status set
  ** apart from the slot let it take the new slot there, and again once the writer had
  ** marked it unread: SoakPico found the same record twice on the board (2026-09-25). */
  OSWriteBuffer(Race, a, SLOT);
  RaceBarrier = 0; RaceAt = 3;
  RaceGot = 0;
  HostBarrierHook = ReaderInWindow;
  OSWriteBuffer(Race, b, SLOT);
  ok = RaceGot == SLOT && memcmp(RaceCopy, b, SLOT) == 0 &&
       OSGetCopyBuffer(Race, OS_READ_ONLY_ONCE, copy) == 0;
  Check("  a slot taken as the writer ends is not taken again", ok);
  HostBarrierHook = NULL;
  Check("  a slot type that does not exist is refused", OSInitBuffer(SLOT, 2, NULL) == NULL);
}


/* TestOutOfMemory: Each allocation of each creation made to fail in turn, as OSMalloc
** does when its heap is used up: the creation must say so, not crash nor hand out a
** structure missing a part. The budget of allocations grows until the creation succeeds. */
static void TestOutOfMemory(void)
{
  static const char *const what[] = {"a FIFO queue", "a 3-slot buffer", "a 4-slot buffer",
                                     "a queue between the cores"};
  unsigned kind;
  printf("\ncreations when memory runs out\n\n");
  for (kind = 0; kind < 4; kind += 1) {
     int budget, failures = 0;
     void *made = NULL;
     char label[80];
     for (budget = 0; made == NULL && budget < 40; budget += 1) {
        HostMallocBudget = budget;
        made = kind == 0 ? OSInitFIFOQueue(3, 8) :
               kind == 1 ? OSInitBuffer(3, OS_BUFFER_TYPE_3_SLOT, NULL) :
               kind == 2 ? OSInitBuffer(3, OS_BUFFER_TYPE_4_SLOT, NULL) : OSInitCoreQueue(4);
        failures += made == NULL;
     }
     HostMallocBudget = -1;
     snprintf(label, sizeof label, "  %s: refused %d times, then made", what[kind], failures);
     Check(label, made != NULL && failures > 0);
  }
}


/* The wait-free queue retries in loops that a broken index never leaves. */
static void Timeout(int signal)
{
  static const char message[] = "\nFAILED: the kernel did not return within 10 s\n";
  (void)signal;
  write(STDOUT_FILENO, message, sizeof message - 1);
  _exit(1);
}

/* TestFIFOIndexWrap: A dequeue preempted at each of its LLs in turn, while the operations
** that preempt it complete it, then run enqueue and dequeue in pairs until Head has gone
** round the 16 bits indices once had: an operation resumed after that many took Head for
** the value it had read and moved it again, one place past Tail, and the queue lost what
** was enqueued next. Indices of 32 bits come round only after 2^32 operations. */
#define WRAP_PAIRS 65531          /* for 4 nodes, 16 bits: GetFIFOArrayMaxIndex(4) - 1 */
static void *WrapQueue;
static unsigned WrapAt, WrapCount;

static BOOL RoundTheIndices(void)
{
  unsigned i;
  UINT16 size;
  if (++WrapCount != WrapAt)
     return FALSE;
  HostLLHook = NULL;
  for (i = 0; i < WRAP_PAIRS; i += 1) {
     void *node = OSGetFreeNodeFIFO(WrapQueue);
     OSEnqueueFIFO(WrapQueue, node, 7);
     OSReleaseNodeFIFO(WrapQueue, OSDequeueFIFO(WrapQueue, &size));
  }
  return TRUE;
}

static void TestFIFOIndexWrap(void)
{
  unsigned at, reached, cases = 0, bad = 0;
  char label[96];
  printf("\nFIFO queue, a dequeue preempted while its indices go round\n\n");
  for (at = 1, reached = 1; reached; at += 1) {
     UINT16 size = 0;
     void *first, *got, *next;
     WrapQueue = OSInitFIFOQueue(NODES, NODE_SIZE);
     first = OSGetFreeNodeFIFO(WrapQueue);
     OSEnqueueFIFO(WrapQueue, first, 1);
     WrapAt = at; WrapCount = 0;
     HostLLHook = RoundTheIndices;
     got = OSDequeueFIFO(WrapQueue, &size);
     HostLLHook = NULL;
     if (!(reached = WrapCount >= at))
        break;
     cases += 1;
     OSReleaseNodeFIFO(WrapQueue, got);
     next = OSGetFreeNodeFIFO(WrapQueue);
     OSEnqueueFIFO(WrapQueue, next, 2);
     if (got != first || OSDequeueFIFO(WrapQueue, &size) != next || size != 2 ||
         OSDequeueFIFO(WrapQueue, &size) != NULL)
        bad += 1;
  }
  snprintf(label, sizeof label, "  the queue keeps what it is given, preempted at each of %u LLs",
           cases);
  Check(label, bad == 0 && cases > 0);
}

int main(void)
{
  setvbuf(stdout, NULL, _IOLBF, 0);   /* keep what was printed if the kernel crashes */
  signal(SIGALRM, Timeout);
  alarm(10);
  TestFIFO();
  TestFIFOPreempted();
  TestFIFONested();
  TestFIFOIndexWrap();
  TestCoreQueue();
  TestCoreQueueInterleaved();
  TestBuffer(OS_BUFFER_TYPE_3_SLOT, "3-slot");
  TestBuffer(OS_BUFFER_TYPE_4_SLOT, "4-slot");
  TestPublication(OS_BUFFER_TYPE_3_SLOT, "3-slot");
  TestPublication(OS_BUFFER_TYPE_4_SLOT, "4-slot");
  TestOutOfMemory();
  printf("\n%s\n", Failures ? "FAILURES" : "all checks passed");
  return Failures ? 1 : 0;
}
