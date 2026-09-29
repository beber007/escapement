/* Copyright (c) 2026 Bertrand Hurst. All rights reserved.
** Escapement - Lightweight Power-Aware Real-Time OS.
** Distributed under the terms of LICENSE at the root of this repository.
*/
/* File Escapement_CoreQueue.c: The FIFO queue of C. Evéquoz, "Non-Blocking Concurrent
** FIFO Queues with Single Word Synchronization Primitives", ICPP 2008, Figure 3, between
** the two cores of the RP2350. test/model/fifo_mp.py explores every interleaving of two
** cores running it, with the monitor of the chip, and checks that every run is
** linearizable.
**
** Figure 3 rests on LL/SC as the paper's Figure 2 defines them, an SC failing only when
** another thread's SC on the same word succeeded; the paper notes that real ones give
** less and offers another algorithm for them (its Section 5 and Figure 5). The queue is
** Figure 3 adapted to the RP2350 in two ways, both from that model:
**   - once its item is in, an enqueuer advances Tail, and a dequeuer Head, again while
**     the SC fails and the LL still finds the old index. Figure 3 tries once, which is
**     enough when a failed SC means another thread advanced the index; on the RP2350 an
**     SC also fails when the other core wrote anywhere in the granule of 16 bytes, or for
**     no visible reason, and the index would stay behind an operation that returned;
**   - DMB barriers, which Armv8-M needs for each core's accesses to reach the other in
**     an order the queue can bear (DDI0553B.y, B7): eight of them, where a first version
**     had one between any two accesses to different words, fifteen. The model of weakly
**     ordered cores (explore_weak) finds seven needed, each caught when left out: after
**     E5, E9, E15, D5, D6, D9 and D15. With those and the one before a dequeue returns,
**     every run is linearizable and every dequeuer finds its item's contents, the model
**     free to reorder any run of accesses between two barriers (test/model/fifo_mp.py;
**     in the CI's window only a part). The ones after E15 and D15 came on 2026-09-29:
**     nothing in Armv8-M orders an SC's write before later writes elsewhere (B7.2.3), and
**     without them Tail could be seen advanced before its item was in place, a dequeuer
**     then helping Head past the empty place and the item lost. The model had assumed
**     that order until an audit of the port questioned it. The proof holds within the
**     model's bounds: a queue of two places, two operations a core, two rounds of each
**     loop.
**
** Head and Tail count up without end and wrap at 2^32, which the length, a power of 2,
** divides: the place of an index is its remainder, and Tail - Head the items held.
**
** Platform version: RP2350 (Raspberry Pi Pico 2).
*/

#include "Escapement.h"
#include "Escapement_CoreQueue.h"

typedef struct {
  volatile UINTPTR *Q;        // the places, NULL when free
  volatile UINT32 Head;       // the first place that may hold an item
  volatile UINT32 Tail;       // the next place to insert into
  UINT32 Mask;                // length - 1
} CORE_QUEUE;


/* OSInitCoreQueue: Creates an empty queue of length places, a power of 2. */
void *OSInitCoreQueue(UINT32 length)
{
  CORE_QUEUE *queue;
  UINT32 i;
  if (length == 0 || (length & (length - 1)) != 0)
     return NULL;
  if ((queue = (CORE_QUEUE *)OSMalloc(sizeof(CORE_QUEUE))) == NULL ||
      (queue->Q = (volatile UINTPTR *)OSMalloc(length * sizeof(UINTPTR))) == NULL)
     return NULL;
  for (i = 0; i < length; i++)
     queue->Q[i] = (UINTPTR)NULL;
  queue->Head = queue->Tail = 0;
  queue->Mask = length - 1;
  return queue;
} /* end of OSInitCoreQueue */


/* OSEnqueueCoreQueue: Enqueue of Figure 3; the paper's line numbers are in the comments.
** Three barriers, after the read of Tail, after the LL of the place and after the SC that
** puts the item in: the first also puts what the item points to before the item is seen
** in the queue. */
BOOL OSEnqueueCoreQueue(void *queue, void *item)
{
  CORE_QUEUE *q = (CORE_QUEUE *)queue;
  UINTPTR *place, slot;
  UINT32 t;
  if (item == NULL)
     return FALSE;
  while (TRUE) {
     t = q->Tail;                                              // E5
     _OSMemoryBarrier();
     if (t == q->Head + q->Mask + 1)                           // E6
        return FALSE;
     place = (UINTPTR *)&q->Q[t & q->Mask];
     slot = OSUINTPTR_LL(place);                               // E9
     _OSMemoryBarrier();
     if (t != q->Tail)                                         // E10: the place is Tail's
        continue;
     if (slot != (UINTPTR)NULL) {                              // E11: Tail lags behind an
        if (OSUINT32_LL((UINT32 *)&q->Tail) == t)              // E12  insertion; help it on
           OSUINT32_SC((UINT32 *)&q->Tail,t + 1);              // E13
     }
     else if (OSUINTPTR_SC(place,(UINTPTR)item)) {             // E15
        _OSMemoryBarrier();       // the item seen in its place before Tail moved past it
        while (OSUINT32_LL((UINT32 *)&q->Tail) == t)           // E16
           if (OSUINT32_SC((UINT32 *)&q->Tail,t + 1))          // E17, again if it fails
              break;
        return TRUE;                                           // E18
     }
  }
} /* end of OSEnqueueCoreQueue */


/* OSDequeueCoreQueue: Dequeue of Figure 3. Five barriers, after the reads of Head and of
** Tail, after the LL of the place, after the SC that takes the item out, and before
** returning, which puts the item's contents read by the caller after the item was
** taken. */
void *OSDequeueCoreQueue(void *queue)
{
  CORE_QUEUE *q = (CORE_QUEUE *)queue;
  UINTPTR *place, slot;
  UINT32 h;
  while (TRUE) {
     h = q->Head;                                              // D5
     _OSMemoryBarrier();
     if (h == q->Tail)                                         // D6
        return NULL;
     place = (UINTPTR *)&q->Q[h & q->Mask];
     _OSMemoryBarrier();
     slot = OSUINTPTR_LL(place);                               // D9
     _OSMemoryBarrier();
     if (h != q->Head)                                         // D10: still the oldest
        continue;
     if (slot == (UINTPTR)NULL) {                              // D11: Head lags behind a
        if (OSUINT32_LL((UINT32 *)&q->Head) == h)              // D12  removal; help it on
           OSUINT32_SC((UINT32 *)&q->Head,h + 1);              // D13
     }
     else if (OSUINTPTR_SC(place,(UINTPTR)NULL)) {             // D15
        _OSMemoryBarrier();       // the place seen free before Head moved past it
        while (OSUINT32_LL((UINT32 *)&q->Head) == h)           // D16
           if (OSUINT32_SC((UINT32 *)&q->Head,h + 1))          // D17, again if it fails
              break;
        _OSMemoryBarrier();
        return (void *)slot;                                   // D18
     }
  }
} /* end of OSDequeueCoreQueue */
