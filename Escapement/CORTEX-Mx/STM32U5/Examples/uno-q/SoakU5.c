/* Copyright (c) 2026 Bertrand Hurst. All rights reserved.
** Escapement - Lightweight Power-Aware Real-Time OS.
** Distributed under the terms of LICENSE at the root of this repository.
*/
/* File SoakU5.c: Everything the kernel offers, running at once for as long as the board
** stays on, each part checking itself: the endurance test, SoakPico2.c on the STM32U575.
** One core only: the part the Pico 2 runs between its cores is here a 4-slot buffer
** written by the interrupt and read by a task it preempts.
**
**   0  Pulse      a task of period 1 ms measures how late it starts on the kernel's
**                 clock; past its deadline, or before its arrival, is an error. It also
**                 counts the wraps of the kernel clock, every 2^30 us, some 18 minutes.
**   1  Queue      a long task fills a FIFO queue, a short one preempts it to put its own
**                 records, an interrupt does too, and an event-driven task empties it:
**                 each producer's records must come out once and in order (IPCPico).
**   2  Buffer     the long task writes a 3-slot buffer, the short one reads it once each
**                 time: never a slot twice, never a torn one.
**   3  Events     a task schedules a timer event every 5 ms; the event-driven task it
**                 wakes must run within 1 ms of the alarm.
**   4  Buffer4    the interrupt writes a record to a 4-slot buffer at each call, a task
**                 reads it every 2 ms: never torn, never older than the last.
**   5  Heartbeat  a task of period 1 s feeds the watchdog and checks that every part
**                 did something during the second; one that did not is an error.
**   6  Interrupt  TIM3, at pseudo-random intervals of 200 to 700 us,
**                 puts records in the queue and signals the task that empties it, from
**                 wherever it interrupts the tasks — inside their queue operations too.
**   7  Memory     the heartbeat finds how much of the stack was never used, and
**                 checks the guard words around the structures allocated: less than
**                 512 bytes free, or a guard overwritten, is an error.
**
** The long task works 500 to 1500 us of each 10 ms, then for 20 s of every minute 6 ms:
** the load goes from about a fifth of the processor to three quarters and back.
**
** Results, in words from its start, laid out as SoakPico's and SoakPico2's, which the
** Renode suite reads; tools/soak.sh drives a Pico only for now (docs/stm32u5.md):
**    0 marker   1 seconds run   2 wraps crossed   3-10 activity of the parts
**   11-18 errors of the parts   19 worst lateness of the pulse   20 of the timer events,
**   in us   21 bytes of the stack never used   22 0, no second core   23 load phase
**   24-55 lateness of the pulse by 10 us, the last for 310 us or more   56-87 the same
**   for the timer events.
** The counts only grow: a probe reading them twice and finding them smaller, or the
** marker gone, has seen the board restart. The independent watchdog restarts it within
** 3 s of the heartbeat stopping, which is how a kernel that hangs shows; the image being
** in SRAM, the board comes back to Arduino's firmware in its flash, and the marker is
** gone. tools/unoq_load.sh freezes the watchdog and the timers while a debugger halts
** the core, so that a halt to read the counts neither restarts the board nor makes the
** tasks late.
** Platform version: STM32U585 (Arduino UNO Q).
*/

#include "Escapement.h"
#include "Escapement_TimerEvent.h"
#include "Escapement_Timer.h"   /* _OSGetActualTime, for the wraps and the queue's work */

#define PARTS       8
#define MARKER      0x534F414Bu        /* "SOAK" */
#define NODES       8
#define WORDS       8
#define BINS        32
#define FILL_TIME   1000               /* per instance of the Filler, without a seed */
#define FILL_HIGH   6000               /* in the phases of high load */
#define EVENT_DELAY 1000               /* of the timer events, without a seed */
#define STACK_FILL  0x5AC05AC0u        /* what unused stack holds */
#define GUARD       0x6A7D6A7Du
#define GUARDS      5
#define EVENT_TIMER_INDEX OS_IO_TIM5
#define ISR_TIMER_INDEX   OS_IO_TIM3

enum { PULSE, QUEUE, BUFFER, EVENTS, BUFFER4, HEARTBEAT, INTERRUPT, MEMORY };

volatile struct {
  UINT32 Marker, Seconds, Wraps;
  UINT32 Activity[PARTS], Errors[PARTS];
  UINT32 PulseLateMax, EventLateMax, Stack0Free, Stack1Free, HighLoad;
  UINT32 PulseLate[BINS], EventLate[BINS];
} Results;

typedef struct {
  UINT32 Producer, Sequence;
} RECORD;

typedef struct {
  UINT32 Counter, Complement;
} SLOT;

/* The descriptor of the interrupt of TIM3: its handler first, as the kernel's dispatcher
** expects (_OSIOHandler). */
typedef struct {
  void (*Handler)(void *);
} ISR_DESCRIPTOR;

/* Set by an emulator running several instances, for each its own conditions: from it,
** the Filler works 500 to 1500 us per instance and the timer events come 500 to 1500 us
** after they are scheduled. Unset on the board. The image runs from SRAM, .data in place:
** an emulator writes it into the image once loaded. */
volatile UINT32 SoakSeed = 0xFFFFFFFF;
static UINT32 FillTime = FILL_TIME, EventDelay = EVENT_DELAY;

static void *Queue, *Slots, *Drain, *Records, *Tick;
static UINT32 *Guard[GUARDS];
static UINT32 EventScheduledAt;
static ISR_DESCRIPTOR AlarmISR;

static void PulseTask(void *argument);
static void FillerTask(void *argument);
static void PokerTask(void *argument);
static void DrainerTask(void *argument);
static void EventSourceTask(void *argument);
static void EventTask(void *argument);
static void ReaderTask(void *argument);
static void HeartbeatTask(void *argument);
static void AlarmHandler(void *descriptor);
static UINT32 *NewGuard(void);
static void PaintStack(void);
static UINT32 Unused(const UINT32 *bottom, const UINT32 *top);
static void CountLate(volatile UINT32 *bins, UINT32 late);

/* The raw clock, in microseconds: the counter of TIM5, free on 32 bits once the event
** manager has started it. */
#define RAW_US            *((volatile UINT32 *)(0x40000C00 + 0x24))
#define TIM3_BASE         0x40000400
#define TIM3_CR1          *((volatile UINT32 *)(TIM3_BASE + 0x00))
#define TIM3_DIER         *((volatile UINT32 *)(TIM3_BASE + 0x0C))
#define TIM3_SR           *((volatile UINT32 *)(TIM3_BASE + 0x10))
#define TIM3_EGR          *((volatile UINT32 *)(TIM3_BASE + 0x14))
#define TIM3_PSC          *((volatile UINT32 *)(TIM3_BASE + 0x28))
#define TIM3_ARR          *((volatile UINT32 *)(TIM3_BASE + 0x2C))
#define UPDATE_BIT        (1u << 0)
#define RCC_APB1ENR1      *((volatile UINT32 *)(0x46020C00 + 0x9C))
#define RCC_APB1ENR1_TIM3EN (1u << 1)
#define NVIC_ISER(irq)    ((volatile UINT32 *)0xE000E100)[(irq) >> 5]
#define NVIC_BIT(irq)     (1u << ((irq) & 0x1F))
#define NVIC_IPR          ((volatile UINT8 *)0xE000E400)
/* The independent watchdog runs on the 32 kHz LSI: divided by 64 (PR = 4), 1500 counts
** are 3 s. Once started it cannot be stopped; the key 0xAAAA reloads it (RM0456, IWDG). */
#define IWDG_KR           *((volatile UINT32 *)(0x40003000 + 0x00))
#define IWDG_PR           *((volatile UINT32 *)(0x40003000 + 0x04))
#define IWDG_RLR          *((volatile UINT32 *)(0x40003000 + 0x08))
#define IWDG_SR           *((volatile UINT32 *)(0x40003000 + 0x0C))
#define IWDG_START        0xCCCCu
#define IWDG_UNLOCK       0x5555u
#define IWDG_RELOAD       0xAAAAu
/* Priority of TIM3, in the 4 most significant bits of its byte: that of the timer events,
** above SysTick and PendSV. */
#define ISR_PRIORITY      (1u << 4)

/* Task creation for each kernel: (1,1)-firm, that is hard, tasks for the soft one. The
** power-aware kernel is not ported to the STM32U5. */
#if defined(ESCAPEMENT_VERSION_SOFT)
   #define PERIODIC(task,wcet,period) OSCreateTask(task,wcet,0,period,period,1,1,0,NULL)
   #define EVENT_DRIVEN(task,wcet,workload,event) \
              OSCreateSynchronousTask(task,wcet,workload,32,event,NULL)
#else
   #define PERIODIC(task,wcet,period) OSCreateTask(task,0,period,period,NULL)
   #define EVENT_DRIVEN(task,wcet,workload,event) \
              OSCreateSynchronousTask(task,workload,event,NULL)
#endif


int main(void)
{
  PaintStack();
  OSInitializeSystemClocks();
  Results.Marker = MARKER;
  if (SoakSeed != 0xFFFFFFFF) {
     FillTime = 500 + SoakSeed % 1001;
     EventDelay = 500 + SoakSeed / 1001 % 1001;
  }
  /* Each structure between two guard words, the heap growing down from them. */
  Guard[0] = NewGuard();
  Queue = OSInitFIFOQueue(NODES,sizeof(RECORD));
  Guard[1] = NewGuard();
  Slots = OSInitBuffer(sizeof(SLOT),OS_BUFFER_TYPE_3_SLOT,NULL);
  Guard[2] = NewGuard();
  Records = OSInitBuffer(WORDS * sizeof(UINT32),OS_BUFFER_TYPE_4_SLOT,NULL);
  Guard[3] = NewGuard();
  Drain = OSCreateEventDescriptor();
  Tick = OSCreateEventDescriptor();
  Guard[4] = NewGuard();
  OSInitTimerEvent(2,1,EVENT_TIMER_INDEX);
  PERIODIC(PulseTask,20,1000);
  PERIODIC(PokerTask,50,2000);
  PERIODIC(ReaderTask,400,2000);
  PERIODIC(EventSourceTask,20,5000);
  PERIODIC(FillerTask,7000,10000);
  PERIODIC(HeartbeatTask,50,1000000);
  EVENT_DRIVEN(DrainerTask,100,1000,Drain);
  EVENT_DRIVEN(EventTask,20,2000,Tick);
  /* TIM3 counts microseconds and interrupts at each update, 1 ms after the start and
  ** then at the interval its handler sets. */
  AlarmISR.Handler = AlarmHandler;
  OSSetISRDescriptor(ISR_TIMER_INDEX,&AlarmISR);
  RCC_APB1ENR1 |= RCC_APB1ENR1_TIM3EN;
  (void)RCC_APB1ENR1;
  TIM3_PSC = OS_SYSTEM_CLOCK_HZ / 1000000u - 1u;
  TIM3_ARR = 1000 - 1;
  TIM3_EGR = UPDATE_BIT;                // load the prescaler
  TIM3_SR = 0;
  TIM3_DIER = UPDATE_BIT;
  TIM3_CR1 = 1;
  NVIC_IPR[ISR_TIMER_INDEX] = ISR_PRIORITY;
  NVIC_ISER(ISR_TIMER_INDEX) = NVIC_BIT(ISR_TIMER_INDEX);
  return OSStartMultitasking(NULL,NULL);
} /* end of main */


/* PulseTask: How late each instance starts after its arrival, on the kernel's clock,
** where the task arrives at every multiple of its period from 0: the arrival goes back
** by 2^30 with the clock at each wrap, as the kernel shifts its own. Before the arrival
** is an error too. */
static void PulseTask(void *argument)
{
  static INT32 arrival = 0, lastTime = 0;
  INT32 time = _OSGetActualTime(), late;
  (void)argument;
  if (time < lastTime) {   // the kernel clock went round
     Results.Wraps += 1;
     arrival -= 0x40000000;
  }
  lastTime = time;
  late = time - arrival;
  arrival += 1000;
  if (late < 0 || late >= 1000)
     Results.Errors[PULSE] += 1;
  if (late >= 0) {
     if ((UINT32)late > Results.PulseLateMax)
        Results.PulseLateMax = late;
     CountLate(Results.PulseLate,late);
  }
  Results.Activity[PULSE] += 1;
  OSEndTask();
} /* end of PulseTask */


/* Put: Puts the next record of a producer in the queue, if a node and a place are
** free: the Filler (0), the Poker (1) and the interrupt of TIM3 (2). */
static void Put(UINT32 producer)
{
  static UINT32 next[3] = {1, 1, 1};
  RECORD *node;
  if ((node = (RECORD *)OSGetFreeNodeFIFO(Queue)) == NULL)
     return;
  node->Producer = producer;
  node->Sequence = next[producer];
  if (!OSEnqueueFIFO(Queue,node,sizeof(RECORD)))
     OSReleaseNodeFIFO(Queue,node);
  else
     next[producer] += 1;
} /* end of Put */


/* FillerTask: Puts records and writes slots for the time of its phase, preempted
** wherever it stands. */
static void FillerTask(void *argument)
{
  static SLOT written = {0, ~0u};
  INT32 start = _OSGetActualTime();
  UINT32 work = Results.HighLoad ? FILL_HIGH : FillTime;
  (void)argument;
  while ((UINT32)(_OSGetActualTime() - start) < work) {
     Put(0);
     OSScheduleSuspendedTask(Drain);
     written.Counter += 1;
     written.Complement = ~written.Counter;
     OSWriteBuffer(Slots,(UINT8 *)&written,sizeof written);
  }
  OSEndTask();
} /* end of FillerTask */


/* PokerTask: Puts a record of its own, and reads the latest slot once. */
static void PokerTask(void *argument)
{
  static UINT32 last = 0;
  SLOT *slot;
  (void)argument;
  Put(1);
  OSScheduleSuspendedTask(Drain);
  if (OSGetReferenceBuffer(Slots,OS_READ_ONLY_ONCE,(UINT8 **)&slot) == sizeof(SLOT)) {
     /* Newer than the last, counted modulo 2^32, as the counters go round. */
     if (slot->Complement != ~slot->Counter || (INT32)(slot->Counter - last) <= 0)
        Results.Errors[BUFFER] += 1;
     last = slot->Counter;
     Results.Activity[BUFFER] += 1;
  }
  OSEndTask();
} /* end of PokerTask */


/* AlarmHandler: TIM3 puts a record in the queue and signals the Drainer, writes the next
** record of the 4-slot buffer, and comes back 200 to 700 us later: the counter restarts
** from 0 at each update, and the new limit counts from there. */
static void AlarmHandler(void *descriptor)
{
  static UINT32 seed = 1, counter = 0;
  UINT32 record[WORDS], i;
  (void)descriptor;
  TIM3_SR = ~UPDATE_BIT;     // acknowledge (rc_w0)
  Put(2);
  OSScheduleSuspendedTask(Drain);
  counter += 1;
  for (i = 0; i < WORDS; i += 1)
     record[i] = counter;
  OSWriteBuffer(Records,(UINT8 *)record,sizeof record);
  seed = seed * 1103515245u + 12345u;
  TIM3_ARR = 200 + (seed >> 16) % 501 - 1;
  Results.Activity[INTERRUPT] += 1;
} /* end of AlarmHandler */


/* DrainerTask: Every record queued, each producer's in the order put. */
static void DrainerTask(void *argument)
{
  static UINT32 last[3] = {0, 0, 0};
  RECORD *node;
  UINT16 size;
  (void)argument;
  while ((node = (RECORD *)OSDequeueFIFO(Queue,&size)) != NULL) {
     if (size != sizeof(RECORD) || node->Producer > 2 ||
         node->Sequence != last[node->Producer] + 1)
        Results.Errors[QUEUE] += 1;
     else
        last[node->Producer] = node->Sequence;
     Results.Activity[QUEUE] += 1;
     OSReleaseNodeFIFO(Queue,node);
  }
  OSSuspendSynchronousTask();
} /* end of DrainerTask */


/* EventSourceTask: A timer event EventDelay ahead. */
static void EventSourceTask(void *argument)
{
  (void)argument;
  EventScheduledAt = RAW_US;
  OSScheduleTimerEvent(Tick,EventDelay,EVENT_TIMER_INDEX);
  OSEndTask();
} /* end of EventSourceTask */


/* EventTask: Woken by the timer event; how long after the alarm it runs. */
static void EventTask(void *argument)
{
  UINT32 late = RAW_US - EventScheduledAt - EventDelay;
  (void)argument;
  if (late > Results.EventLateMax)
     Results.EventLateMax = late;
  if (late >= 1000)
     Results.Errors[EVENTS] += 1;
  CountLate(Results.EventLate,late);
  Results.Activity[EVENTS] += 1;
  OSSuspendSynchronousTask();
} /* end of EventTask */


/* ReaderTask: The latest record of the interrupt, whole and not older than the last,
** copied while the interrupt may preempt the copy. */
static void ReaderTask(void *argument)
{
  static UINT32 last = 0;
  UINT32 record[WORDS], i;
  (void)argument;
  if (OSGetCopyBuffer(Records,OS_READ_MULTIPLE,(UINT8 *)record) == sizeof record) {
     for (i = 1; i < WORDS && record[i] == record[0]; i += 1);
     /* Not older than the last, modulo 2^32: the writer may go round its counter within
     ** hours, where a comparison as plain numbers takes the next record for an error. The
     ** one error of a 12-hour run of SoakPico on the board came at 8.6 hours, when a
     ** writer of 139,000 records a second would have gone round (2026-09-26). */
     if (i < WORDS || (INT32)(record[0] - last) < 0)
        Results.Errors[BUFFER4] += 1;
     last = record[0];
     Results.Activity[BUFFER4] += 1;
  }
  OSEndTask();
} /* end of ReaderTask */


/* HeartbeatTask: Once a second, feeds the watchdog, checks that every part moved and
** that memory holds, and switches the load between its phases. */
static void HeartbeatTask(void *argument)
{
  extern UINT32 _ebss;
  extern void *_OSStackBasePointer;
  static UINT32 seen[PARTS];
  UINT32 i;
  (void)argument;
  if (Results.Seconds == 0) {       // started, then set: its prescaler and reload
     IWDG_KR = IWDG_START;          // registers are written only while it runs
     IWDG_KR = IWDG_UNLOCK;
     IWDG_PR = 4;
     IWDG_RLR = 1500;
     while (IWDG_SR & 0xFu);        // PVU, RVU, WVU, EWU: every update taken
  }
  IWDG_KR = IWDG_RELOAD;
  if (Results.Seconds > 0)
     for (i = 0; i < PARTS; i += 1)
        if (i != HEARTBEAT && i != MEMORY && Results.Activity[i] == seen[i])
           Results.Errors[HEARTBEAT] += 1;
  for (i = 0; i < PARTS; i += 1)
     seen[i] = Results.Activity[i];
  /* The stack runs down from the heap towards the end of .bss. */
  Results.Stack0Free = Unused(&_ebss,(UINT32 *)_OSStackBasePointer);
  if (Results.Stack0Free < 512)
     Results.Errors[MEMORY] += 1;
  for (i = 0; i < GUARDS; i += 1)
     if (*Guard[i] != GUARD)
        Results.Errors[MEMORY] += 1;
  Results.Activity[MEMORY] += 1;
  Results.Seconds += 1;
  Results.HighLoad = Results.Seconds % 60 >= 40;
  Results.Activity[HEARTBEAT] += 1;
  OSEndTask();
} /* end of HeartbeatTask */


/* NewGuard: A guard word, allocated between two structures. */
static UINT32 *NewGuard(void)
{
  UINT32 *guard = (UINT32 *)OSMalloc(sizeof(UINT32));
  *guard = GUARD;
  return guard;
} /* end of NewGuard */


/* PaintStack: Fills what the stack may use, from the end of .bss to a little below this
** frame, with a pattern, before anything runs on it. */
static void PaintStack(void)
{
  extern UINT32 _ebss;
  UINT32 *word, sp;
  __asm volatile ("MOV %0, SP" : "=r" (sp));
  for (word = &_ebss; (UINTPTR)word < sp - 128; word += 1)
     *word = STACK_FILL;
} /* end of PaintStack */


/* Unused: The bytes at the bottom of a stack that still hold the pattern. */
static UINT32 Unused(const UINT32 *bottom, const UINT32 *top)
{
  const UINT32 *word = bottom;
  while (word < top && *word == STACK_FILL)
     word += 1;
  return (UINT32)(word - bottom) * sizeof(UINT32);
} /* end of Unused */


/* CountLate: One lateness in its bin of 10 us, the last for all beyond. */
static void CountLate(volatile UINT32 *bins, UINT32 late)
{
  bins[late / 10 < BINS - 1 ? late / 10 : BINS - 1] += 1;
} /* end of CountLate */
