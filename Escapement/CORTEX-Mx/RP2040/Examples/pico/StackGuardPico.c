/* Copyright (c) 2026 Bertrand Hurst. All rights reserved.
** Escapement - Lightweight Power-Aware Real-Time OS.
** Distributed under the terms of LICENSE at the root of this repository.
*/
/* File StackGuardPico.c: A task that overflows the one stack, on purpose.
**
** The stack grows down toward the globals. _OSResetHandler puts a region of the MPU, no
** access, over the 1 KB the linker script keeps between them (Escapement_CortexMx.c), so
** the overflow must fault there and end in the HardFault handler, whose stacking the MPU
** lets into the region: Sentinel, the last of the globals the linker places, keeps its
** value, and Depth says how deep the task went. As StackGuardPico2, by the MPU rather
** than MSPLIM. On the Pico on 2026-10-05: the handler, Depth 507, Sentinel kept.
** Platform version: RP2040 (Raspberry Pi Pico).
*/

#include "Escapement.h"

#define VTOR *((volatile UINT32 *)0xE000ED08)

volatile UINT32 Depth;
volatile UINT32 Sentinel;

static void OverflowTask(void *argument);
static UINT32 Recurse(UINT32 level) __attribute__((noinline));

int main(void)
{
  extern void (* const CortexMxVectorTable[])(void);
  VTOR = (UINT32)CortexMxVectorTable;
  OSInitializeSystemClocks();
  #if defined(ESCAPEMENT_VERSION_HARD_PA)
     OSInitProcessorSpeed();
  #endif
  Sentinel = 0xA5A5A5A5u;
  /* A period of 100 s: the kernel's overload check would stop it at the next arrival of
  ** a task still running, long before the stack runs out. */
  #if defined(ESCAPEMENT_VERSION_SOFT)
     OSCreateTask(OverflowTask,0,0,100000000,100000000,1,1,0,NULL);
  #elif defined(ESCAPEMENT_VERSION_HARD_PA)
     OSCreateTask(OverflowTask,100000000,0,100000000,100000000,NULL);
  #else
     OSCreateTask(OverflowTask,0,100000000,100000000,NULL);
  #endif
  return OSStartMultitasking(NULL,NULL);
} /* end of main */


/* Recurse: Takes 512 bytes of stack a level, every byte of it written, so that a stack run
** past its end writes over whatever lies below, Sentinel included; and never returns. A
** frame under the guard's 1 KB cannot step over it. Not inlined: GCC put two levels in
** one frame of 1,536 bytes, which stepped over the guard and wrote over Sentinel in the
** deadline-monotonic build (2026-10-05). */
static UINT32 Recurse(UINT32 level)
{
  volatile UINT8 frame[512];
  UINT32 i;
  for (i = 0; i < sizeof frame; i += 1)
     frame[i] = (UINT8)level;
  Depth = level;
  /* Bounded only so that GCC sees an end: the stack, some 260 KB, is gone long before. */
  return (level < 100000u ? Recurse(level + 1) : 0) + frame[0] + frame[sizeof frame - 1];
} /* end of Recurse */


static void OverflowTask(void *argument)
{
  (void)argument;
  Recurse(1);
  OSEndTask();
} /* end of OverflowTask */
