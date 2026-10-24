/* Copyright (c) 2026 Bertrand Hurst. All rights reserved.
** Escapement - Lightweight Power-Aware Real-Time OS.
** Distributed under the terms of LICENSE at the root of this repository.
*/
/* File BenchSlotsPico.c: The two measurements of issue #40's first step, on the RP2040 at
** 125 MHz, hard kernel under EDF.
**
** 1. What a slot released for nothing costs. A task of 100 ms spins 50 ms of each
**    instance on the 1 us counter, counting its turns; a slot task of SLOT_PERIOD us
**    (none when 0) preempts it. The turns lost against a run without the slot, at the
**    time of a turn measured without it, are the time the slot's releases took, the
**    kernel's entry, the switches and the instance itself: divided by the releases in
**    the window, the cost of one. With SLOT_FORTH the slot runs, as #40's would, the
**    word its switch points to, an empty one; without, it only ends.
**
** 2. What a minimal Forth word costs called from a task. An indirect-threaded inner
**    interpreter in C, the switch (defer) of a task pointing to : blink led toggle ;
**    against the same toggle called in C through a pointer, each timed in cycles by
**    SysTick, interrupts masked, the cost of timing nothing taken off.
**
** Slots: 0 runs, 1 instances of the counting task, 2 their turns summed, 3 the turns of
** the instance with the fewest, 4 the releases of the slot inside the windows summed,
** 5 the slot's instances. Forth: 0 cycles of an empty timing, 1 and 2 the fewest and the
** sum for the Forth word, 3 and 4 for the C call, 5 the timings made of each.
** Platform version: RP2040.
*/

#include "Escapement.h"

#if defined(ESCAPEMENT_VERSION_SOFT) || defined(ESCAPEMENT_VERSION_HARD_PA) || \
    SCHEDULER_REAL_TIME_MODE == DEADLINE_MONOTONIC_SCHEDULING
   #error "the hard kernel under EDF"
#endif
#ifndef SLOT_PERIOD
   #define SLOT_PERIOD 0
#endif

#define TIMER_TIMERAWL  *((volatile UINT32 *)(0x40054000 + 0x28))
#define TIMER_DBGPAUSE  *((volatile UINT32 *)(0x40054000 + 0x2C))
#define SYST_CSR        *((volatile UINT32 *)0xE000E010)
#define SYST_RVR        *((volatile UINT32 *)0xE000E014)
#define SYST_CVR        *((volatile UINT32 *)0xE000E018)
#define SIO_GPIO_OUT_XOR *((volatile UINT32 *)(0xD0000000 + 0x1C))
#define VTOR            *((volatile UINT32 *)0xE000ED08)

#define COUNT_PERIOD 100000
#define COUNT_WINDOW 50000
#define TIMINGS      1000

volatile UINT32 Slots[6];
volatile UINT32 Forth[6];

/* ---- The inner interpreter: each word a code field, then its body or its value. */
typedef struct Word Word;
struct Word {
   void (*Code)(const Word *);
   const Word *const *Body;      /* a colon definition's cells */
   UINT32 Value;                 /* a constant's value, or the word a defer runs */
};

static const Word *const *IP;
static UINT32 DataStack[16], *DSP = DataStack;
static const Word *const *ReturnStack[16];
static const Word *const **RSP = ReturnStack;

static void DoColon(const Word *w) { *RSP++ = IP; IP = w->Body; }
static void DoConstant(const Word *w) { *DSP++ = w->Value; }
static void DoDefer(const Word *w) { const Word *t = (const Word *)w->Value; t->Code(t); }
static void DoExit(const Word *w) { (void)w; IP = *--RSP; }
static void DoToggle(const Word *w) { (void)w; SIO_GPIO_OUT_XOR = *--DSP; }

/* Execute: runs a word from C, as a slot task does, until its body returns. */
static void Execute(const Word *w)
{
  const Word *const *outer = IP;
  IP = NULL;
  w->Code(w);
  while (IP != NULL) {
     const Word *next = *IP++;
     next->Code(next);
  }
  IP = outer;
} /* end of Execute */

static const Word Exit = {DoExit, NULL, 0};
static const Word Led = {DoConstant, NULL, 1u << 25};
static const Word Toggle = {DoToggle, NULL, 0};
static const Word *const BlinkBody[] = {&Led, &Toggle, &Exit};
static const Word Blink = {DoColon, BlinkBody, 0};
#ifdef SLOT_FORTH
   static const Word *const NoopBody[] = {&Exit};
   static const Word Noop = {DoColon, NoopBody, 0};
#endif
static Word BlinkTask = {DoDefer, NULL, (UINT32)&Blink};
#ifdef SLOT_FORTH
   static Word SlotTask = {DoDefer, NULL, (UINT32)&Noop};
#endif

static void __attribute__((noinline)) BlinkC(void)
{
  SIO_GPIO_OUT_XOR = 1u << 25;
} /* end of BlinkC */

static void ExecuteBlink(void) { Execute(&BlinkTask); }
static void Nothing(void) { }

/* Not inlined, so that each is called through a pointer, as a task's code is. */
static UINT32 __attribute__((noinline)) Time(void f(void))
{
  UINT32 before, after;
  __asm volatile ("cpsid i" ::: "memory");
  before = SYST_CVR;
  f();
  after = SYST_CVR;
  __asm volatile ("cpsie i" ::: "memory");
  return (before - after) & 0xFFFFFF;   /* SysTick counts down, on 24 bits */
} /* end of Time */

static void TimeForth(void)
{
  void (*volatile c)(void) = BlinkC;
  UINT32 i, t, least = 0xFFFFFF;
  for (i = 0; i < 64; i += 1)
     if ((t = Time(Nothing)) < least)
        least = t;
  Forth[0] = least;
  Forth[1] = Forth[3] = 0xFFFFFFFF;
  for (i = 0; i < TIMINGS; i += 1) {
     t = Time(ExecuteBlink) - least;
     Forth[2] += t;
     if (t < Forth[1])
        Forth[1] = t;
     t = Time(c) - least;
     Forth[4] += t;
     if (t < Forth[3])
        Forth[3] = t;
  }
  Forth[5] = TIMINGS;
} /* end of TimeForth */

/* ---- The slot and the task that counts. */
#if SLOT_PERIOD > 0
static void Slot(void *argument)
{
  (void)argument;
  Slots[5] += 1;
  #ifdef SLOT_FORTH
     Execute(&SlotTask);
  #endif
  OSEndTask();
} /* end of Slot */
#endif

static void CountingTask(void *argument)
{
  UINT32 start, turns = 0, releases;
  (void)argument;
  /* The loader leaves core 1 halted, which pauses the counter the kernel set to pause
  ** with it: let it run, as no core is halted from here on. */
  TIMER_DBGPAUSE = 0;
  releases = Slots[5];
  start = TIMER_TIMERAWL;
  while (TIMER_TIMERAWL - start < COUNT_WINDOW)
     turns += 1;
  releases = Slots[5] - releases;
  /* The first instance starts with the slot's first release: left out. */
  if (Slots[0]++ > 0) {
     Slots[1] += 1;
     Slots[2] += turns;
     if (Slots[3] == 0 || turns < Slots[3])
        Slots[3] = turns;
     Slots[4] += releases;
  }
  OSEndTask();
} /* end of CountingTask */


int main(void)
{
  extern void (* const CortexMxVectorTable[])(void);
  VTOR = (UINT32)CortexMxVectorTable;
  OSInitializeSystemClocks();
  SYST_RVR = 0xFFFFFF;
  SYST_CVR = 0;
  SYST_CSR = 0x5;        /* the processor's clock, enabled, no interrupt */
  TimeForth();
  OSCreateTask(CountingTask,0,COUNT_PERIOD,COUNT_PERIOD,NULL);
  #if SLOT_PERIOD > 0
     OSCreateTask(Slot,0,SLOT_PERIOD,SLOT_PERIOD,NULL);
  #endif
  return OSStartMultitasking(NULL,NULL);
} /* end of main */
