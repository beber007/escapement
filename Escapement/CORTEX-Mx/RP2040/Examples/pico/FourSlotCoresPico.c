/* Copyright (c) 2026 Bertrand Hurst. All rights reserved.
** Escapement - Lightweight Power-Aware Real-Time OS.
** Distributed under the terms of LICENSE at the root of this repository.
*/
/* File FourSlotCoresPico.c: The 4-slot buffer of the kernel between the two cores.
**
** Simpson designed his four slots for a writer and a reader that run at the same time,
** on processors of their own. The kernel uses them between an interrupt handler and a
** task, on one core; here they cross from one core to the other, as the algorithm meant.
** Core 1, bare, writes records of eight words, all eight the same counter, rising by one
** from each record to the next, as fast as it can, into two places: a 4-slot buffer and
** a plain array. A task on core 0 reads both, 32 times each millisecond — every 2 ms
** under the power-aware kernel, which then changes speed under both cores — and counts
** the reads that mix two records (torn) and those that go back to an older one: the two
** properties Rushby model-checked for Simpson's algorithm, and test/model/fourslot.py
** for the kernel's, on one core and on two. The plain array is the negative control: the
** same writer and the same reader, without the mechanism, must tear.
**
** Only OS_READ_MULTIPLE is used. OS_READ_ONLY_ONCE marks the slot read with an LL/SC
** pair, which the Cortex-M0+ emulates with a reservation bit that interrupts clear: a
** store from the other core would leave it set, so it holds on one core only
** (Escapement_Atomic.c).
**
** The counts sit in Results, which tools/fourslot_cores.sh reads over SWD while both
** cores run, loading the image without ever stopping core 1 (docs/rp2040.md).
** Platform version: RP2040.
*/

#include "Escapement.h"
#include "Escapement_Core1.h"

#define WORDS 8                  /* words per record */
#define READS_PER_RUN 32

typedef struct {
  UINT32 Reads, Torn, Backwards, Last;
} READ_COUNTS;

/* What core 1 has written, what the reader found in each place, and the longest run of
** the reader in microseconds. */
volatile struct {
  UINT32 Written;
  READ_COUNTS FourSlot, Plain;
  UINT32 LongestRun;
} Results;

static void *Buffer;
static volatile UINT32 Plain[WORDS];
static UINT32 Core1Stack[256] __attribute__((aligned(8)));

static void Writer(void);
static void ReaderTask(void *argument);
static void Check(volatile READ_COUNTS *counts, const UINT32 *record);
static void Dither(void);

#define VTOR              *((volatile UINT32 *)0xE000ED08)
#define WATCHDOG_CTRL_CLR *((volatile UINT32 *)(0x40058000 + 0x3000))
#define WATCHDOG_ENABLE   (1u << 30)
#define TIMER_TIMERAWL    *((volatile UINT32 *)(0x40054000 + 0x28))


int main(void)
{
  extern void (* const CortexMxVectorTable[])(void);
  VTOR = (UINT32)CortexMxVectorTable;
  /* A firmware in flash may have armed the watchdog, which a reset of the processors
  ** leaves running. It pauses while the debugger holds either core, and the probe used
  ** to hold core 1 for good; now that core 1 runs, the watchdog fired within a second
  ** and the chip rebooted into the flash. */
  WATCHDOG_CTRL_CLR = WATCHDOG_ENABLE;
  OSInitializeSystemClocks();
  #if defined(ESCAPEMENT_VERSION_HARD_PA)
     OSInitProcessorSpeed();
  #endif
  Buffer = OSInitBuffer(WORDS * sizeof(UINT32),OS_BUFFER_TYPE_4_SLOT,NULL);
  #if defined(ESCAPEMENT_VERSION_SOFT)
     OSCreateTask(ReaderTask,0,0,1000,1000,1,1,0,NULL);
  #elif defined(ESCAPEMENT_VERSION_HARD_PA)
     /* 32 reads of each place took 314 us at most at 125 MHz, core 1 contending for the
     ** memory (Results.LongestRun, hard kernel, 2026-09-24). Every millisecond, those
     ** 400 us left no lower speed; every 2 ms, the kernel runs the reader at 50 MHz and
     ** the idle task at 125, and core 1, on the same clock, writes through both. */
     OSCreateTask(ReaderTask,400,0,2000,2000,NULL);
  #else
     OSCreateTask(ReaderTask,0,1000,1000,NULL);
  #endif
  OSLaunchCore1(Writer,&Core1Stack[sizeof Core1Stack / sizeof Core1Stack[0]]);
  return OSStartMultitasking(NULL,NULL);
} /* end of main */


/* Writer: Core 1. Every record in both places, for ever. */
static void Writer(void)
{
  UINT32 record[WORDS], counter = 0, i;
  while (TRUE) {
     counter += 1;
     for (i = 0; i < WORDS; i += 1)
        record[i] = counter;
     OSWriteBuffer(Buffer,(UINT8 *)record,sizeof record);
     for (i = 0; i < WORDS; i += 1)
        Plain[i] = counter;
     Results.Written = counter;
  }
} /* end of Writer */


/* ReaderTask: Core 0. Reads each place in turn and checks what it got. */
static void ReaderTask(void *argument)
{
  UINT32 record[WORDS], i, n, start = TIMER_TIMERAWL;
  for (n = 0; n < READS_PER_RUN; n += 1) {
     Dither();
     if (OSGetCopyBuffer(Buffer,OS_READ_MULTIPLE,(UINT8 *)record) == sizeof record)
        Check(&Results.FourSlot,record);
     Dither();
     for (i = 0; i < WORDS; i += 1)
        record[i] = Plain[i];
     if (record[0] != 0)
        Check(&Results.Plain,record);
  }
  if ((n = TIMER_TIMERAWL - start) > Results.LongestRun)
     Results.LongestRun = n;
  OSEndTask();
} /* end of ReaderTask */


/* Check: A record is torn when its words differ, and goes backwards when it is older than
** the last whole one read from the same place. */
static void Check(volatile READ_COUNTS *counts, const UINT32 *record)
{
  UINT32 i;
  counts->Reads += 1;
  for (i = 1; i < WORDS; i += 1)
     if (record[i] != record[0]) {
        counts->Torn += 1;
        return;
     }
  if (record[0] < counts->Last)
     counts->Backwards += 1;
  counts->Last = record[0];
} /* end of Check */


/* Dither: Waits a pseudo-random handful of cycles. Both cores run off the same clock and
** loop without end: left alone, the reader met the writer at the same point every time,
** and 6528 reads of the plain array, 64 per millisecond, missed every one of its writes
** (2026-09-24). */
static void Dither(void)
{
  static UINT32 seed = 1;
  volatile UINT32 i;
  seed = seed * 1103515245u + 12345u;
  for (i = (seed >> 16) & 31; i > 0; i -= 1);
} /* end of Dither */
