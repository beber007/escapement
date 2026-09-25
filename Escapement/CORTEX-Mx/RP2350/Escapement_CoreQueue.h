/* Copyright (c) 2026 Bertrand Hurst. All rights reserved.
** Escapement - Lightweight Power-Aware Real-Time OS.
** Distributed under the terms of LICENSE at the root of this repository.
*/
/* File Escapement_CoreQueue.h: A FIFO queue of pointers between the two cores.
**
** The kernel's FIFO queue announces its operation for a preempting caller to complete,
** which holds only while preemptions nest, on one core. This one is the array-based
** queue of C. Evéquoz (ICPP 2008, Figure 3) adapted to the RP2350: lock-free, not
** wait-free, for any number of producers and consumers on either core, task, interrupt
** handler or bare code on core 1. It needs the LL/SC pairs of both cores to go through
** the global monitor (ACTLR.EXTEXCLALL, which the port sets), and so is for the RP2350
** alone: the Cortex-M0+ of the RP2040 emulates LL/SC for one core.
**
** It carries pointers, never NULL; what they point to is the caller's: a free list of
** nodes is another such queue. It signals no event: a task on core 0 polls it.
**
** Platform version: RP2350 (Raspberry Pi Pico 2).
*/

#ifndef ESCAPEMENT_COREQUEUE_H
#define ESCAPEMENT_COREQUEUE_H

/* OSInitCoreQueue: Creates an empty queue. To be called from main, before
** OSStartMultitasking(), since it allocates with OSMalloc.
** Parameter: (UINT32) the number of places, a power of 2.
** Returned value: (void *) the queue, or NULL when memory runs out or the length is not
** a power of 2. */
void *OSInitCoreQueue(UINT32 length);

/* OSEnqueueCoreQueue: Appends an item.
** Parameters:
**   (1) (void *) a queue created by OSInitCoreQueue();
**   (2) (void *) the item, not NULL. What it points to is written before the item is
**       published, and seen by the core that dequeues it.
** Returned value: (BOOL) FALSE when the queue is full or the item NULL. */
BOOL OSEnqueueCoreQueue(void *queue, void *item);

/* OSDequeueCoreQueue: Removes the oldest item.
** Parameter: (void *) a queue created by OSInitCoreQueue().
** Returned value: (void *) the item, or NULL when the queue is empty. */
void *OSDequeueCoreQueue(void *queue);

#endif /* ESCAPEMENT_COREQUEUE_H */
