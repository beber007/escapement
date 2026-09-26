/* Copyright (c) 2026 Bertrand Hurst. All rights reserved.
** Escapement - Lightweight Power-Aware Real-Time OS.
** Distributed under the terms of LICENSE at the root of this repository.
*/
/* File SoakPico.c: Everything the kernel offers, running at once for as long as the board
** stays on, each part checking itself: the endurance test (tools/soak.py).
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
**   4  Cores      core 1 writes records to a 4-slot buffer without end, a task on core 0
**                 reads them every 2 ms: never torn, never older than the last.
**   5  Heartbeat  a task of period 1 s feeds the watchdog and checks that every part
**                 did something during the second; one that did not is an error.
**   6  Interrupt  alarm 3 of the timer, at pseudo-random intervals of 200 to 700 us,
**                 puts records in the queue and signals the task that empties it, from
**                 wherever it interrupts the tasks — inside their queue operations too.
**   7  Memory     the heartbeat finds how much of each core's stack was never used, and
**                 checks the guard words around the structures allocated: less than
**                 512 bytes free, or a guard overwritten, is an error.
**
** The load comes in phases of 5 to 60 s, drawn at random, each with a work of its own for
** the long task, from its own 500 to 1500 us of each 10 ms to 6 ms, drawn at random too:
** the processor from about a fifth loaded to three quarters, in steps of no set length.
**
** Results, in words from its start, which tools/soak.py and the Renode suites read:
**    0 marker   1 seconds run   2 wraps crossed   3-10 activity of the parts
**   11-18 errors of the parts   19 worst lateness of the pulse   20 of the timer events,
**   in us   21 bytes of core 0's stack never used   22 of core 1's   23 work of the long task in its phase, in us
**   24-55 lateness of the pulse by 10 us, the last for 310 us or more   56-87 the same
**   for the timer events.
** The counts only grow: a probe reading them twice and finding them smaller, or the
** marker gone, has seen the board restart. The watchdog restarts it within 3 s of the
** heartbeat stopping, into the firmware in flash, which is how a kernel that hangs shows.
** Platform version: RP2040.
*/

#include "Escapement.h"
#include "Escapement_Core1.h"
#include "Escapement_TimerEvent.h"
#include "Escapement_Timer.h"   /* _OSGetActualTime, for the wraps and the queue's work */

#define PARTS       8
#define MARKER      0x534F414Bu        /* "SOAK" */
#define NODES       8
#define WORDS       8
#define BINS        32
#define FILL_TIME   1000               /* per instance of the Filler, without a seed */
#define FILL_HIGH   6000               /* the most a phase of load gives it */
#define EVENT_DELAY 1000               /* of the timer events, without a seed */
#define STACK_FILL  0x5AC05AC0u        /* what unused stack holds */
#define GUARD       0x6A7D6A7Du
#define GUARDS      5
#define EVENT_TIMER_INDEX OS_IO_TIMER_2
#define ISR_TIMER_INDEX   OS_IO_TIMER_3

enum { PULSE, QUEUE, BUFFER, EVENTS, CORES, HEARTBEAT, INTERRUPT, MEMORY };

volatile struct {
  UINT32 Marker, Seconds, Wraps;
  UINT32 Activity[PARTS], Errors[PARTS];
  UINT32 PulseLateMax, EventLateMax, Stack0Free, Stack1Free, Load;
  UINT32 PulseLate[BINS], EventLate[BINS];
} Results;

typedef struct {
  UINT32 Producer, Sequence;
} RECORD;

typedef struct {
  UINT32 Counter, Complement;
} SLOT;

/* The descriptor of the interrupt of alarm 3: its handler first, as the kernel's
** dispatcher expects (_OSIOHandler). */
typedef struct {
  void (*Handler)(void *);
} ISR_DESCRIPTOR;

/* Cleared by an emulator whose core 1 cannot be launched (escapement_pico.robot): the
** part between the cores is then skipped, and counted as done. Initialised, so that it
** sits in .data, which an image linked in SRAM copies onto itself at startup: a value
** written before the start survives, where .bss is cleared. */
volatile UINT32 SoakLaunchCore1 = 1;
/* Set by an emulator running several instances (tools/soak_emulated.sh), for each its
** own conditions: from it, the Filler works 500 to 1500 us per instance and the timer
** events come 500 to 1500 us after they are scheduled. Unset on the board. In .data for
** the same reason. */
volatile UINT32 SoakSeed = 0xFFFFFFFF;
static UINT32 FillTime = FILL_TIME, EventDelay = EVENT_DELAY;

static void *Queue, *Slots, *Drain, *Cores, *Tick;
static UINT32 *Guard[GUARDS];
static UINT32 EventScheduledAt;
static UINT32 Core1Stack[256] __attribute__((aligned(8)));
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
static void Writer(void);
static UINT32 *NewGuard(void);
static void PaintStack(void);
static UINT32 Unused(const UINT32 *bottom, const UINT32 *top);
static void CountLate(volatile UINT32 *bins, UINT32 late);

#define VTOR              *((volatile UINT32 *)0xE000ED08)
#define TIMER_BASE        0x40054000
#define TIMER_ALARM3      *((volatile UINT32 *)(TIMER_BASE + 0x1C))
#define TIMER_TIMERAWL    *((volatile UINT32 *)(TIMER_BASE + 0x28))
#define TIMER_INTR        *((volatile UINT32 *)(TIMER_BASE + 0x34))
#define TIMER_INTE_SET    *((volatile UINT32 *)(TIMER_BASE + 0x2000 + 0x38))
#define ALARM3_BIT        (1u << 3)
#define NVIC_ISER         *((volatile UINT32 *)0xE000E100)
#define NVIC_IPR          ((volatile UINT32 *)0xE000E400)
#define WATCHDOG_CTRL     *((volatile UINT32 *)(0x40058000 + 0x00))
#define WATCHDOG_LOAD     *((volatile UINT32 *)(0x40058000 + 0x04))
#define WATCHDOG_ENABLE   (1u << 30)
/* The RP2040 decrements the watchdog twice per tick of 1 us (erratum RP2040-E1): 3 s. */
#define WATCHDOG_TICKS    (2u * 3000000u)
/* Priority of alarm 3, in the 2 most significant bits of its byte: that of the timer
** events, above SysTick and PendSV. */
#define ISR_PRIORITY      (1u << 6)

/* Task creation for each kernel: execution times at 125 MHz for the power-aware one,
** generous, the Filler's that of its phases of high load; (1,1)-firm, that is hard,
** tasks for the soft one. */
#if defined(ESCAPEMENT_VERSION_SOFT)
   #define PERIODIC(task,wcet,period) OSCreateTask(task,wcet,0,period,period,1,1,0,NULL)
   #define EVENT_DRIVEN(task,wcet,workload,event) \
              OSCreateSynchronousTask(task,wcet,workload,32,event,NULL)
#elif defined(ESCAPEMENT_VERSION_HARD_PA)
   #define PERIODIC(task,wcet,period) OSCreateTask(task,wcet,0,period,period,NULL)
   #define EVENT_DRIVEN(task,wcet,workload,event) \
              OSCreateSynchronousTask(task,wcet,workload,32,event,NULL)
#else
   #define PERIODIC(task,wcet,period) OSCreateTask(task,0,period,period,NULL)
   #define EVENT_DRIVEN(task,wcet,workload,event) \
              OSCreateSynchronousTask(task,workload,event,NULL)
#endif


int main(void)
{
  extern void (* const CortexMxVectorTable[])(void);
  UINT32 i;
  VTOR = (UINT32)CortexMxVectorTable;
  /* The watchdog a firmware in flash may have armed is disarmed until the heartbeat
  ** takes it over: core 1 runs, and nothing pauses it any more. */
  WATCHDOG_CTRL &= ~WATCHDOG_ENABLE;
  PaintStack();
  OSInitializeSystemClocks();
  #if defined(ESCAPEMENT_VERSION_HARD_PA)
     OSInitProcessorSpeed();
  #endif
  Results.Marker = MARKER;
  if (SoakSeed != 0xFFFFFFFF) {
     FillTime = 500 + SoakSeed % 1001;
     EventDelay = 500 + SoakSeed / 1001 % 1001;
  }
  Results.Load = FillTime;
  /* Each structure between two guard words, the heap growing down from them. */
  Guard[0] = NewGuard();
  Queue = OSInitFIFOQueue(NODES,sizeof(RECORD));
  Guard[1] = NewGuard();
  Slots = OSInitBuffer(sizeof(SLOT),OS_BUFFER_TYPE_3_SLOT,NULL);
  Guard[2] = NewGuard();
  Cores = OSInitBuffer(WORDS * sizeof(UINT32),OS_BUFFER_TYPE_4_SLOT,NULL);
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
  AlarmISR.Handler = AlarmHandler;
  OSSetISRDescriptor(ISR_TIMER_INDEX,&AlarmISR);
  NVIC_IPR[ISR_TIMER_INDEX >> 2] |= ISR_PRIORITY << ((ISR_TIMER_INDEX & 3) << 3);
  TIMER_INTE_SET = ALARM3_BIT;
  NVIC_ISER = 1u << ISR_TIMER_INDEX;
  TIMER_ALARM3 = TIMER_TIMERAWL + 1000;
  if (SoakLaunchCore1) {
     for (i = 0; i < sizeof Core1Stack / sizeof Core1Stack[0]; i += 1)
        Core1Stack[i] = STACK_FILL;
     OSLaunchCore1(Writer,&Core1Stack[sizeof Core1Stack / sizeof Core1Stack[0]]);
  }
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
** free: the Filler (0), the Poker (1) and the interrupt of alarm 3 (2). */
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
  UINT32 work = Results.Load;
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


/* AlarmHandler: Alarm 3 puts a record and signals the Drainer, then comes back 200 to
** 700 us later. */
static void AlarmHandler(void *descriptor)
{
  static UINT32 seed = 1;
  (void)descriptor;
  TIMER_INTR = ALARM3_BIT;   // acknowledge
  Put(2);
  OSScheduleSuspendedTask(Drain);
  seed = seed * 1103515245u + 12345u;
  TIMER_ALARM3 = TIMER_TIMERAWL + 200 + (seed >> 16) % 501;
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
  EventScheduledAt = TIMER_TIMERAWL;
  OSScheduleTimerEvent(Tick,EventDelay,EVENT_TIMER_INDEX);
  OSEndTask();
} /* end of EventSourceTask */


/* EventTask: Woken by the timer event; how long after the alarm it runs. */
static void EventTask(void *argument)
{
  UINT32 late = TIMER_TIMERAWL - EventScheduledAt - EventDelay;
  (void)argument;
  if (late > Results.EventLateMax)
     Results.EventLateMax = late;
  if (late >= 1000)
     Results.Errors[EVENTS] += 1;
  CountLate(Results.EventLate,late);
  Results.Activity[EVENTS] += 1;
  OSSuspendSynchronousTask();
} /* end of EventTask */


/* Writer: Core 1. Records of eight equal words, rising, for ever. */
static void Writer(void)
{
  UINT32 record[WORDS], counter = 0, i;
  while (TRUE) {
     counter += 1;
     for (i = 0; i < WORDS; i += 1)
        record[i] = counter;
     OSWriteBuffer(Cores,(UINT8 *)record,sizeof record);
  }
} /* end of Writer */


/* ReaderTask: The latest record of core 1, whole and not older than the last. */
static void ReaderTask(void *argument)
{
  static UINT32 last = 0;
  UINT32 record[WORDS], i;
  (void)argument;
  if (!SoakLaunchCore1)
     Results.Activity[CORES] += 1;
  else if (OSGetCopyBuffer(Cores,OS_READ_MULTIPLE,(UINT8 *)record) == sizeof record) {
     for (i = 1; i < WORDS && record[i] == record[0]; i += 1);
     /* Not older than the last, modulo 2^32: the writer may go round its counter within
     ** hours, where a comparison as plain numbers takes the next record for an error. The
     ** one error of a 12-hour run of SoakPico on the board came at 8.6 hours, when a
     ** writer of 139,000 records a second would have gone round (2026-09-26). */
     if (i < WORDS || (INT32)(record[0] - last) < 0)
        Results.Errors[CORES] += 1;
     last = record[0];
     Results.Activity[CORES] += 1;
  }
  OSEndTask();
} /* end of ReaderTask */


/* HeartbeatTask: Once a second, feeds the watchdog, checks that every part moved and
** that memory holds, and switches the load between its phases. */
static void HeartbeatTask(void *argument)
{
  extern UINT32 _ebss;
  extern void *_OSStackBasePointer;
  static UINT32 seen[PARTS], phaseEnd = 0, draw;
  UINT32 i;
  (void)argument;
  WATCHDOG_LOAD = WATCHDOG_TICKS;   // loaded before it is enabled: at 0, it fires at once
  if (Results.Seconds == 0)
     WATCHDOG_CTRL |= WATCHDOG_ENABLE;
  if (Results.Seconds > 0)
     for (i = 0; i < PARTS; i += 1)
        if (i != HEARTBEAT && i != MEMORY && Results.Activity[i] == seen[i])
           Results.Errors[HEARTBEAT] += 1;
  for (i = 0; i < PARTS; i += 1)
     seen[i] = Results.Activity[i];
  /* The stack of core 0 runs down from the heap towards the end of .bss. */
  Results.Stack0Free = Unused(&_ebss,(UINT32 *)_OSStackBasePointer);
  if (SoakLaunchCore1)
     Results.Stack1Free = Unused(Core1Stack,
                                 &Core1Stack[sizeof Core1Stack / sizeof Core1Stack[0]]);
  if (Results.Stack0Free < 512 || (SoakLaunchCore1 && Results.Stack1Free < 256))
     Results.Errors[MEMORY] += 1;
  for (i = 0; i < GUARDS; i += 1)
     if (*Guard[i] != GUARD)
        Results.Errors[MEMORY] += 1;
  Results.Activity[MEMORY] += 1;
  Results.Seconds += 1;
  /* A new phase at a random time, of a random load: 5 to 60 s long, the long task working
  ** from its own time to FILL_HIGH of each 10 ms. The draws start from the clock, or
  ** from the seed an emulator gives, so that two runs go through other phases. */
  if (Results.Seconds >= phaseEnd) {
     if (phaseEnd == 0)
        draw = SoakSeed != 0xFFFFFFFF ? SoakSeed : TIMER_TIMERAWL;
     draw = draw * 1103515245u + 12345u;
     phaseEnd = Results.Seconds + 5 + (draw >> 16) % 56;
     draw = draw * 1103515245u + 12345u;
     Results.Load = FillTime + (draw >> 16) % (FILL_HIGH - FillTime + 1);
  }
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


/* PaintStack: Fills what the stack of core 0 may use, from the end of .bss to a little
** below this frame, with a pattern, before anything runs on it. */
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
