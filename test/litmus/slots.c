/* Copyright (c) 2026 Bertrand Hurst. All rights reserved.
** Escapement - Lightweight Power-Aware Real-Time OS.
** Distributed under the terms of LICENSE at the root of this repository.
*/
/* File slots.c: The kernel's slot buffers between two threads of a weakly ordered
** Armv8-A host, as litmus.c runs the queue between the cores: the 3-slot buffer
** (OSWriteBuffer and GetReadyBuffer3Slot, in EscapementHard.c, after Chen and Burns, 1997)
** and, with -4, the 4-slot one (GetReadyBuffer4Slot, after Simpson, 1990, with no LL or
** SC: each side writes its own words only). The kernel is compiled as
** it stands, with the host port of test/host built for the bench (HOST_LITMUS): its DMBs
** are DMB ISH, its LL and SC LDXRB and STXRB.
**
** The writer writes items of 16 bytes, a number counting up and a check word that depends
** on it, each item with one call to OSWriteBuffer, which copies it byte by byte into its
** slot. The reader copies the latest item out with OSGetCopyBuffer, byte by byte too. A
** slot the writer wrote while it was read comes out torn, its check wrong or its length
** short; a slot named before it was filled comes out with an older number or a torn one;
** and the numbers read must never go backwards. These are the three properties
** test/model/threeslot.py and fourslot.py check in every interleaving.
**
**   slots [-4] [-p PAIRS] [-n ITEMS]
*/

#include <pthread.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#ifdef __APPLE__
   #include <pthread/qos.h>
#endif

#include "../host/Escapement.h"   /* the kernel's, not the queue's of this directory */

#ifndef SKIP_BARRIER
   #define SKIP_BARRIER 0
#endif

#define SIZE 16
#define CHECK(n) ((n) * 0x9E3779B97F4A7C15ull)

typedef struct {
  void *Buffer;
  uint64_t Items;
  _Atomic uint64_t Written;            /* items the writer has written */
  _Atomic int Done;
  unsigned long Reads, Errors;
} PAIR;

static void FastCore(void)
{
#ifdef __APPLE__
  pthread_set_qos_class_self_np(QOS_CLASS_USER_INTERACTIVE,0);
#endif
}

static double Now(void)
{
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC,&ts);
  return ts.tv_sec + ts.tv_nsec * 1e-9;
}

static void *Writer(void *arg)
{
  PAIR *p = arg;
  UINT8 data[SIZE];
  FastCore();
  for (uint64_t n = 1; n <= p->Items; n++) {
     uint64_t check = CHECK(n);
     memcpy(data,&n,8);
     memcpy(data + 8,&check,8);
     if (OSWriteBuffer(p->Buffer,data,SIZE) != SIZE)
        abort();
     atomic_store_explicit(&p->Written,n,memory_order_relaxed);
  }
  atomic_store(&p->Done,1);
  return NULL;
}

static void Report(PAIR *p, const char *what, uint64_t a, uint64_t b)
{
  if (p->Errors++ < 5)
     fprintf(stderr,"  %s (%llu, %llu)\n",what,(unsigned long long)a,(unsigned long long)b);
}

static void *Reader(void *arg)
{
  PAIR *p = arg;
  UINT8 data[SIZE];
  uint64_t last = 0, n, check;
  FastCore();
  while (!atomic_load_explicit(&p->Done,memory_order_relaxed)) {
     UINT8 got = OSGetCopyBuffer(p->Buffer,OS_READ_MULTIPLE,data);
     if (got == 0)
        continue;                      /* nothing written yet */
     p->Reads += 1;
     memcpy(&n,data,8);
     memcpy(&check,data + 8,8);
     if (got != SIZE)
        Report(p,"a slot read short (its length, its number)",got,n);
     else if (check != CHECK(n))
        Report(p,"a torn slot (its number, its check)",n,check);
     else if (n < last)
        Report(p,"the numbers read went backwards",last,n);
     else
        last = n;
  }
  return NULL;
}

int main(int argc, char **argv)
{
  int pairs = 2, slots = 3, opt;
  uint64_t items = 20000000;
  while ((opt = getopt(argc,argv,"4p:n:")) != -1)
     switch (opt) {
     case '4': slots = 4; break;
     case 'p': pairs = atoi(optarg); break;
     case 'n': items = strtoull(optarg,NULL,0); break;
     default:
        fprintf(stderr,"usage: %s [-4] [-p PAIRS] [-n ITEMS]\n",argv[0]);
        return 2;
     }
  PAIR *p = calloc(pairs,sizeof *p);
  pthread_t *t = calloc(2 * pairs,sizeof *t);
  double start = Now();
  for (int i = 0; i < pairs; i++) {
     p[i].Buffer = OSInitBuffer(SIZE,slots == 3 ? OS_BUFFER_TYPE_3_SLOT
                                                : OS_BUFFER_TYPE_4_SLOT,NULL);
     p[i].Items = items;
     pthread_create(&t[2 * i],NULL,Writer,&p[i]);
     pthread_create(&t[2 * i + 1],NULL,Reader,&p[i]);
  }
  /* A watchdog: a writer that makes no progress for 10 s is stuck in its SC loop. */
  uint64_t *last = calloc(pairs,sizeof *last);
  double *still = calloc(pairs,sizeof *still);
  for (int i = 0; i < pairs; i++)
     still[i] = Now();
  for (int done = 0; !done; ) {
     usleep(200000);
     done = 1;
     for (int i = 0; i < pairs; i++) {
        if (atomic_load(&p[i].Done))
           continue;
        done = 0;
        uint64_t w = atomic_load(&p[i].Written);
        if (w != last[i]) {
           last[i] = w;
           still[i] = Now();
        }
        else if (Now() - still[i] > 10) {
           printf("  %d-slot buffer, pair %d stuck at item %llu: no progress for 10 s\n",slots,i,
                  (unsigned long long)w);
           fflush(stdout);
           _exit(1);
        }
     }
  }
  unsigned long errors = 0, reads = 0;
  for (int i = 0; i < 2 * pairs; i++)
     pthread_join(t[i],NULL);
  for (int i = 0; i < pairs; i++) {
     errors += p[i].Errors;
     reads += p[i].Reads;
  }
  printf("  %d-slot buffer (SKIP_BARRIER=%d), %d pairs of threads, %llu items written each, "
         "%lu read: %lu errors (%.0f s)\n",slots,SKIP_BARRIER,pairs,(unsigned long long)items,reads,
         errors,Now() - start);
  return errors != 0;
}
