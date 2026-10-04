/* Copyright (c) 2026 Bertrand Hurst. All rights reserved.
** Escapement - Lightweight Power-Aware Real-Time OS.
** Distributed under the terms of LICENSE at the root of this repository.
*/
/* File StackGuardPico2.c: A task that overflows the one stack, on purpose.
**
** The stack grows down toward the globals. _OSResetHandler sets MSPLIM at their end
** (Escapement_CortexMx.c), so the overflow must fault there, a HardFault with STKOF set
** in the CFSR, before any global is written: Sentinel, the last of them the linker
** places, keeps its value, and Depth says how deep the task went. Without the limit the
** task wrote over the globals, the kernel's among them.
** Platform version: RP2350 (Raspberry Pi Pico 2).
*/

#include "Escapement.h"

#define VTOR *((volatile UINT32 *)0xE000ED08)

volatile UINT32 Depth;
volatile UINT32 Sentinel;

static void OverflowTask(void *argument);
static UINT32 Recurse(UINT32 level);

int main(void)
{
  extern void (* const CortexMxVectorTable[])(void);
  VTOR = (UINT32)CortexMxVectorTable;
  OSInitializeSystemClocks();
  Sentinel = 0xA5A5A5A5u;
  /* A period of 100 s: the kernel's overload check would stop it at the next arrival of
  ** a task still running, long before the stack runs out. */
  #if defined(ESCAPEMENT_VERSION_SOFT)
     OSCreateTask(OverflowTask,0,0,100000000,100000000,1,1,0,NULL);
  #else
     OSCreateTask(OverflowTask,0,100000000,100000000,NULL);
  #endif
  return OSStartMultitasking(NULL,NULL);
} /* end of main */


/* Recurse: Takes 1 KB of stack a level, every byte of it written, so that a stack run
** past its end writes over whatever lies below, Sentinel included; and never returns. */
static UINT32 Recurse(UINT32 level)
{
  volatile UINT8 frame[1024];
  UINT32 i;
  for (i = 0; i < sizeof frame; i += 1)
     frame[i] = (UINT8)level;
  Depth = level;
  /* Bounded only so that GCC sees an end: the stack, 512 KB, is gone long before. */
  return (level < 100000u ? Recurse(level + 1) : 0) + frame[0] + frame[sizeof frame - 1];
} /* end of Recurse */


static void OverflowTask(void *argument)
{
  (void)argument;
  Recurse(1);
  OSEndTask();
} /* end of OverflowTask */
