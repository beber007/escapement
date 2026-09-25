/* Copyright (c) 2026 Bertrand Hurst. All rights reserved.
** Escapement - Lightweight Power-Aware Real-Time OS.
** Distributed under the terms of LICENSE at the root of this repository.
*/
/* File ThreeSlotCoresPico2.c: FourSlotCoresPico2.c with the 3-slot buffer of the kernel,
** the one whose writer and reader hand a slot over with LL/SC on its Reading byte. On the
** RP2350 these are LDREXB/STREXB, and across the cores they hold only because the port
** sets ACTLR.EXTEXCLALL on both (Escapement_RamEntry.S, Escapement_Core1.c).
**
** Core 1, bare, writes records of eight equal words, rising, into the buffer and into a
** plain array; a task on core 0 reads both 32 times a millisecond and counts the reads
** that mix two records or go back to an older one. The plain array must tear.
**
** What a run can show depends on the failures of the SCs (test/model/threeslot.py, two
** cores). The reader's fail when core 0 switches context between its LL and its SC, the
** kernel clearing the monitor there (CLREX, Escapement_CortexMx_a.S). The writer's never
** fail without a reason: core 1 takes no interrupt. So a writer that tries its SC only
** once, as the kernels did until 6a73672, does not tear here: the model needs that SC to
** fail for no visible reason. A writer that takes the slot being read does, and so does
** a reader that tries its SC once. Under Renode (1.17), whose SC compares the value its
** LL read instead of watching the other core's stores, the model says the same; but
** there the run slows down a thousandfold or stalls (docs/emulation.md), so the demo is
** not in escapement_pico2.robot.
**
** The counts sit in Results, laid out as in FourSlotCoresPico2.c, to be read over SWD
** on the board, with an OpenOCD that knows the RP2350.
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
  READ_COUNTS ThreeSlot, Plain;
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
  /* As in FourSlotCoresPico2.c: a watchdog armed by a firmware in flash would reboot the
  ** chip once core 1 runs. */
  WATCHDOG_CTRL_CLR = WATCHDOG_ENABLE;
  OSInitializeSystemClocks();
  Buffer = OSInitBuffer(WORDS * sizeof(UINT32),OS_BUFFER_TYPE_3_SLOT,NULL);
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
        Check(&Results.ThreeSlot,record);
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


/* Dither: Waits a pseudo-random handful of cycles, so that the two cores, off the same
** clock, do not meet at the same point every time (FourSlotCoresPico2.c). */
static void Dither(void)
{
  static UINT32 seed = 1;
  volatile UINT32 i;
  seed = seed * 1103515245u + 12345u;
  for (i = (seed >> 16) & 31; i > 0; i -= 1);
} /* end of Dither */
