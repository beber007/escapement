/* Copyright (c) 2026 Bertrand Hurst. All rights reserved.
** Escapement - Lightweight Power-Aware Real-Time OS.
** Distributed under the terms of LICENSE at the root of this repository.
*/
/* File FourSlotCoresPico2.c: FourSlotCoresPico.c on the Raspberry Pi Pico 2, the 4-slot
** buffer of the kernel between the two Cortex-M33 of the RP2350. The figures quoted in
** the original were measured on the RP2040; none has been on the RP2350 yet.
**
** Simpson designed his four slots for a writer and a reader that run at the same time,
** on processors of their own. The kernel uses them between an interrupt handler and a
** task, on one core; here they cross from one core to the other, as the algorithm meant.
** Core 1, bare, writes records of eight words, all eight the same counter, rising by one
** from each record to the next, as fast as it can, into two places: a 4-slot buffer and
** a plain array. A task on core 0 reads both, 32 times each millisecond, and counts
** the reads that mix two records (torn) and those that go back to an older one: the two
** properties Rushby model-checked for Simpson's algorithm, and test/model/fourslot.py
** for the kernel's, on one core and on two. The plain array is the negative control: the
** same writer and the same reader, without the mechanism, must tear.
**
** Only OS_READ_MULTIPLE is used. OS_READ_ONLY_ONCE marks the slot read with an LL/SC
** pair: the Cortex-M33 has the exclusive instructions, but their monitors are local to
** each core until ACTLR.EXTEXCLALL makes them see the stores of the other one (RP2350
** datasheet), which the port does not set yet.
**
** The counts sit in Results, to be read over SWD while both cores run, as
** tools/fourslot_cores.sh does on the Pico; on the Pico 2 it needs an OpenOCD that knows
** the RP2350.
** Platform version: RP2350 (Raspberry Pi Pico 2).
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
#define WATCHDOG_CTRL_CLR *((volatile UINT32 *)(0x400D8000 + 0x3000))
#define WATCHDOG_ENABLE   (1u << 30)
#define TIMER_TIMERAWL    *((volatile UINT32 *)(0x400B0000 + 0x28))   /* TIMER0 */


int main(void)
{
  extern void (* const CortexMxVectorTable[])(void);
  VTOR = (UINT32)CortexMxVectorTable;
  /* A firmware in flash may have armed the watchdog, which a reset of the processors
  ** leaves running and which pauses only while the debugger holds a core: on the Pico,
  ** once core 1 ran, it fired within a second and the chip rebooted into the flash. */
  WATCHDOG_CTRL_CLR = WATCHDOG_ENABLE;
  OSInitializeSystemClocks();
  Buffer = OSInitBuffer(WORDS * sizeof(UINT32),OS_BUFFER_TYPE_4_SLOT,NULL);
  #if defined(ESCAPEMENT_VERSION_SOFT)
     OSCreateTask(ReaderTask,0,0,1000,1000,1,1,0,NULL);
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
