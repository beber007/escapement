/* Copyright (c) 2026 Bertrand Hurst. All rights reserved.
** Escapement - Lightweight Power-Aware Real-Time OS.
** Distributed under the terms of LICENSE at the root of this repository.
*/
/* File litmus.c: Runs on a weakly ordered Armv8-A host, two threads on two cores, first
** the three classic litmus tests, then the queue between the cores of the RP2350
** (Escapement_CoreQueue.c) under stress.
**
** The Pico 2 keeps its accesses in order (LitmusPico2.c found none reordered), so the
** board shows that the queue runs, not that its seven DMBs are needed nor that they are
** enough. An Apple M-series core does reorder: the same source, its barriers made DMB
** ISH, is tried there, then once per barrier left out (SKIP_BARRIER, Escapement.h). It
** stands for a processor that uses the freedom Armv8-M also gives Normal memory (Armv8-M
** ARM, DDI0553B.y, B7), not for a Cortex-M, none of which is known to reorder as far. A
** mutant this bench catches is a barrier some processor needs; one it does not catch
** proves nothing — test/model/fifo_mp.py is the proof.
**
** The litmus tests (Alglave, Maranget, Sarkar and Sewell, "Litmus: running tests against
** hardware", TACAS 2011), each round the two threads meeting on a counter, then running
** K instances side by side, as litmus7 does (a meeting per instance left none overlap):
**   SB  thread 0: X = 1; r0 = Y    thread 1: Y = 1; r1 = X     relaxed: r0 = r1 = 0
**   MP  thread 0: D = 1; F = 1     thread 1: r0 = F; r1 = D    relaxed: r0 = 1, r1 = 0
**   LB  thread 0: r0 = X; Y = 1    thread 1: r1 = Y; X = 1     relaxed: r0 = r1 = 1
** each without and with a DMB between its two accesses; the relaxed outcome must vanish
** with it. They show the bench can see what it looks for.
**
** The queue: each of two threads enqueues items of its own, numbered, and dequeues, in
** turns; an item is a node whose number and check word the enqueuer writes just before
** enqueuing it. Every dequeued item must carry the number and check written, items of one
** enqueuer must reach each dequeuer in the order they were enqueued, and at the end, the
** queue drained, every item must have been dequeued once. A thread that makes no progress
** for some seconds is a queue stuck, an item lost behind Tail or Head.
**
**   litmus [-p PAIRS] [-n ITEMS] [-l LENGTH] [-r ROUNDS] [-q]   (-q: the queue only)
*/

#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#ifdef __APPLE__
   #include <pthread/qos.h>
#endif

#include "Escapement.h"
#include "Escapement_CoreQueue.h"

#define DMB() __asm volatile ("dmb ish" ::: "memory")

static void FastCore(void)
{
#ifdef __APPLE__
  /* The performance cores, as far as macOS lets a thread ask: it cannot be pinned. */
  pthread_set_qos_class_self_np(QOS_CLASS_USER_INTERACTIVE,0);
#endif
}

static double Now(void)
{
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC,&ts);
  return ts.tv_sec + ts.tv_nsec * 1e-9;
}

/* ---- The litmus tests ------------------------------------------------------------- */

enum { SB, SB_DMB, MP, MP_DMB, LB, LB_DMB, TESTS };
static const char *TestName[TESTS] = { "SB", "SB+dmb", "MP", "MP+dmb", "LB", "LB+dmb" };
/* The outcome, r0 * 2 + r1, that only a reordering gives. */
static const int Relaxed[TESTS] = { 0, 0, 2, 2, 3, 3 };

/* Each round runs K instances of the test, each pair of variables on cache lines of its
** own (Apple's lines are of 128 bytes), the two threads walking them side by side after
** meeting, as litmus7 does: the instances where the two halves overlap closely enough are
** those that can show a reordering, and a sync per instance would leave none. */
#define K 1024
typedef struct { volatile UINT32 v; char pad[124]; } __attribute__((aligned(128))) LINE;
static LINE VarX[K], VarY[K];
static UINT32 R0[K], R1[K];
static _Atomic unsigned long Go[2];       /* the round each thread reached */
static unsigned long LitmusRounds;

/* The halves are in assembly: the compiler must neither reorder nor merge the accesses.
** For thread 0, a is X and b is Y; for thread 1 the other way round, but in MP. */
static UINT32 Half(int test, int thread, volatile UINT32 *x, volatile UINT32 *y)
{
  UINT32 r = 0, s = 0, one = 1;
  volatile UINT32 *a = thread == 0 ? x : y, *b = thread == 0 ? y : x;
  switch (test) {
  case SB: __asm volatile ("str %w2, [%1]\n\tldr %w0, [%3]" : "=&r" (r)
                           : "r" (a), "r" (one), "r" (b) : "memory"); break;
  case SB_DMB: __asm volatile ("str %w2, [%1]\n\tdmb ish\n\tldr %w0, [%3]" : "=&r" (r)
                               : "r" (a), "r" (one), "r" (b) : "memory"); break;
  case LB: __asm volatile ("ldr %w0, [%1]\n\tstr %w2, [%3]" : "=&r" (r)
                           : "r" (b), "r" (one), "r" (a) : "memory"); break;
  case LB_DMB: __asm volatile ("ldr %w0, [%1]\n\tdmb ish\n\tstr %w2, [%3]" : "=&r" (r)
                               : "r" (b), "r" (one), "r" (a) : "memory"); break;
  /* MP: thread 0 writes the data X, then the flag Y; thread 1 reads Y, then X, and
  ** returns Y * 2 + X. */
  case MP: case MP_DMB:
     if (thread == 0 && test == MP)
        __asm volatile ("str %w0, [%1]\n\tstr %w0, [%2]" :
                        : "r" (one), "r" (x), "r" (y) : "memory");
     else if (thread == 0)
        __asm volatile ("str %w0, [%1]\n\tdmb ish\n\tstr %w0, [%2]" :
                        : "r" (one), "r" (x), "r" (y) : "memory");
     else if (test == MP)
        __asm volatile ("ldr %w0, [%2]\n\tldr %w1, [%3]" : "=&r" (r), "=&r" (s)
                        : "r" (y), "r" (x) : "memory");
     else
        __asm volatile ("ldr %w0, [%2]\n\tdmb ish\n\tldr %w1, [%3]" : "=&r" (r), "=&r" (s)
                        : "r" (y), "r" (x) : "memory");
     r = r * 2 + s;
     break;
  }
  return r;
}

static void Meet(int thread, unsigned long round)
{
  atomic_store_explicit(&Go[thread],round,memory_order_release);
  while (atomic_load_explicit(&Go[1 - thread],memory_order_acquire) < round)
     ;
}

/* Each thread waits a pseudo-random few spins after meeting, so that the instances of
** the two cross at every offset around each other. */
static UINT32 Spin(UINT32 seed)
{
  UINT32 n = (seed >> 16) % 32;
  while (n--)
     __asm volatile ("" ::: "memory");
  return seed * 1103515245u + 12345u;
}

static void *Litmus1(void *arg)
{
  int test = *(int *)arg;
  UINT32 seed = 99;
  FastCore();
  for (unsigned long round = 1; round <= LitmusRounds / K; round++) {
     Meet(1,2 * round);
     seed = Spin(seed);
     for (int i = 0; i < K; i++)
        R1[i] = Half(test,1,&VarX[i].v,&VarY[i].v);
     DMB();
     Meet(1,2 * round + 1);
  }
  return NULL;
}

static unsigned long RunLitmus(int test, unsigned long outcome[4])
{
  pthread_t t;
  UINT32 seed = 1;
  memset(outcome,0,4 * sizeof outcome[0]);
  atomic_store(&Go[0],0);
  atomic_store(&Go[1],0);
  pthread_create(&t,NULL,Litmus1,&test);
  FastCore();
  for (unsigned long round = 1; round <= LitmusRounds / K; round++) {
     for (int i = 0; i < K; i++)
        VarX[i].v = VarY[i].v = 0;
     DMB();
     Meet(0,2 * round);
     seed = Spin(seed);
     for (int i = 0; i < K; i++)
        R0[i] = Half(test,0,&VarX[i].v,&VarY[i].v);
     Meet(0,2 * round + 1);
     DMB();
     for (int i = 0; i < K; i++)
        if (test == MP || test == MP_DMB)
           outcome[R1[i]] += 1;           /* only thread 1 reads in MP */
        else
           outcome[R0[i] * 2 + R1[i]] += 1;
  }
  pthread_join(t,NULL);
  return outcome[Relaxed[test]];
}

/* ---- The queue -------------------------------------------------------------------- */

#define RING 65536                     /* nodes an enqueuer cycles through */
#define CHECK(n) ((n) * 0x9E3779B97F4A7C15ull)

typedef struct {
  volatile uint64_t Number, Check;
  char pad[112];
} NODE;

typedef struct PAIR PAIR;
typedef struct {
  PAIR *Pair;
  int Id;
  NODE *Nodes;                         /* RING of them, this thread's items */
  uint64_t Enqueued, Last[2];          /* last number dequeued from each enqueuer */
  uint8_t *Seen;                       /* times each item of each enqueuer was dequeued */
  _Atomic uint64_t Progress;
  unsigned long Errors;
} WORKER;

struct PAIR {
  void *Queue;
  WORKER W[2];
  uint64_t Items;
  _Atomic int Stop;
};

static int Verbose = 1;

static void Report(WORKER *w, const char *what, uint64_t a, uint64_t b)
{
  if (w->Errors++ < 5)
     fprintf(stderr,"  thread %d: %s (%llu, %llu)\n",w->Id,what,
             (unsigned long long)a,(unsigned long long)b);
}

/* Checks an item dequeued by w: whose it is, its contents, its order. */
static void Check(WORKER *w, NODE *node)
{
  PAIR *p = w->Pair;
  int from;
  for (from = 0; from < 2; from++)
     if (node >= p->W[from].Nodes && node < p->W[from].Nodes + RING)
        break;
  if (from == 2) {
     Report(w,"an item nobody enqueued",(uint64_t)(UINTPTR)node,0);
     return;
  }
  uint64_t n = node->Number, c = node->Check;
  if (c != CHECK(n) || (uint64_t)(node - p->W[from].Nodes) != n % RING)
     Report(w,"an item whose contents are not those written",n,c);
  else if (n <= w->Last[from] && w->Last[from] != 0)
     Report(w,"an enqueuer's items out of order",w->Last[from],n);
  else {
     w->Last[from] = n;
     if (n == 0 || n > p->Items)
        Report(w,"a number never enqueued",n,0);
     else
        __atomic_fetch_add(&p->W[from].Seen[n],1,__ATOMIC_RELAXED);
  }
}

static void *Worker(void *arg)
{
  WORKER *w = arg;
  PAIR *p = w->Pair;
  NODE *node;
  FastCore();
  while (w->Enqueued < p->Items && !atomic_load_explicit(&p->Stop,memory_order_relaxed)) {
     uint64_t n = w->Enqueued + 1;
     node = &w->Nodes[n % RING];
     node->Number = n;
     node->Check = CHECK(n);
     /* The node's contents are seen before the item is: the queue's first barrier,
     ** after the read of Tail, orders them. */
     while (!OSEnqueueCoreQueue(p->Queue,node)) {
        if ((node = OSDequeueCoreQueue(p->Queue)) != NULL)
           Check(w,node);
        node = &w->Nodes[n % RING];
        if (atomic_load_explicit(&p->Stop,memory_order_relaxed))
           return NULL;
     }
     w->Enqueued = n;
     if ((node = OSDequeueCoreQueue(p->Queue)) != NULL)
        Check(w,node);
     atomic_store_explicit(&w->Progress,n,memory_order_relaxed);
  }
  return NULL;
}

static unsigned long RunQueue(int pairs, uint64_t items, UINT32 length)
{
  PAIR *p = calloc(pairs,sizeof *p);
  pthread_t *t = calloc(2 * pairs,sizeof *t);
  unsigned long errors = 0;
  double start = Now();
  for (int i = 0; i < pairs; i++) {
     p[i].Queue = OSInitCoreQueue(length);
     p[i].Items = items;
     for (int k = 0; k < 2; k++) {
        WORKER *w = &p[i].W[k];
        w->Pair = &p[i];
        w->Id = k;
        if (posix_memalign((void **)&w->Nodes,128,RING * sizeof(NODE)) != 0)
           abort();
        memset(w->Nodes,0,RING * sizeof(NODE));
        w->Seen = calloc(items + 1,1);
     }
  }
  for (int i = 0; i < pairs; i++)
     for (int k = 0; k < 2; k++)
        pthread_create(&t[2 * i + k],NULL,Worker,&p[i].W[k]);
  /* A watchdog: a thread that has not finished and makes no progress for 10 s is stuck
  ** on its queue, an item lost behind Tail or Head, or two threads livelocked. */
  uint64_t *last = calloc(2 * pairs,sizeof *last);
  double *still = calloc(2 * pairs,sizeof *still);
  for (int i = 0; i < 2 * pairs; i++)
     still[i] = Now();
  int stuck = 0;
  while (!stuck) {
     usleep(200000);
     int done = 1;
     uint64_t sum = 0;
     for (int i = 0; i < 2 * pairs; i++) {
        uint64_t g = atomic_load(&p[i / 2].W[i % 2].Progress);
        sum += g;
        if (g >= items)
           continue;
        done = 0;
        if (g != last[i]) {
           last[i] = g;
           still[i] = Now();
        }
        else if (Now() - still[i] > 10) {
           fprintf(stderr,"  pair %d, thread %d stuck at item %llu: no progress for 10 s\n",
                   i / 2,i % 2,(unsigned long long)g);
           stuck = 1;
        }
     }
     if (done)
        break;
     if (Verbose > 1)
        fprintf(stderr,"\r  %.0f%%",100.0 * sum / (2.0 * pairs * items));
  }
  /* A thread stuck inside the queue's loops never comes back to be joined. */
  if (stuck) {
     printf("  queue of %u places, %d pairs of threads: stuck\n",length,pairs);
     fflush(stdout);
     _exit(1);
  }
  for (int i = 0; i < 2 * pairs; i++)
     pthread_join(t[i],NULL);
  /* Drain, then every item once. */
  for (int i = 0; i < pairs; i++) {
     NODE *node;
     while ((node = OSDequeueCoreQueue(p[i].Queue)) != NULL)
        Check(&p[i].W[0],node);
     for (int k = 0; k < 2; k++) {
        WORKER *w = &p[i].W[k];
        errors += w->Errors;
        if (atomic_load(&p[i].Stop))
           continue;
        unsigned long lost = 0, twice = 0;
        for (uint64_t n = 1; n <= items; n++)
           if (w->Seen[n] == 0) lost++;
           else if (w->Seen[n] > 1) twice++;
        if (lost || twice) {
           fprintf(stderr,"  pair %d, thread %d's items: %lu lost, %lu dequeued twice\n",
                   i,k,lost,twice);
           errors += lost + twice;
        }
     }
  }
  double s = Now() - start;
  printf("  queue of %u places, %d pairs of threads, %llu items each way: %.1f M "
         "operations/s, %lu errors (%.0f s)\n",length,pairs,(unsigned long long)items,
         4.0 * pairs * items / s / 1e6,errors,s);
  return errors;
}

int main(int argc, char **argv)
{
  int pairs = 4, queueOnly = 0, opt;
  uint64_t items = 20000000;
  UINT32 length = 2;
  LitmusRounds = 20000000;
  while ((opt = getopt(argc,argv,"p:n:l:r:qv")) != -1)
     switch (opt) {
     case 'p': pairs = atoi(optarg); break;
     case 'n': items = strtoull(optarg,NULL,0); break;
     case 'l': length = (UINT32)atoi(optarg); break;
     case 'r': LitmusRounds = strtoul(optarg,NULL,0); break;
     case 'q': queueOnly = 1; break;
     case 'v': Verbose = 2; break;
     default:
        fprintf(stderr,"usage: %s [-p PAIRS] [-n ITEMS] [-l LENGTH] [-r ROUNDS] [-q]\n",
                argv[0]);
        return 2;
     }
  unsigned long failures = 0;
  if (!queueOnly) {
     printf("litmus tests, %lu instances each:\n",LitmusRounds / K * K);
     for (int test = 0; test < TESTS; test++) {
        unsigned long outcome[4];
        unsigned long relaxed = RunLitmus(test,outcome);
        printf("  %-7s %lu %lu %lu %lu   relaxed outcome: %lu\n",TestName[test],
               outcome[0],outcome[1],outcome[2],outcome[3],relaxed);
        if (test % 2 == 1 && relaxed != 0)
           failures += 1;            /* a DMB that did not order */
     }
  }
  printf("queue (SKIP_BARRIER=%d):\n",SKIP_BARRIER);
  failures += RunQueue(pairs,items,length);
  return failures != 0;
}
