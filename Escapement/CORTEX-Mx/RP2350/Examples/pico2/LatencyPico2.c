/* Copyright (c) 2026 Bertrand Hurst. All rights reserved.
** Escapement - Lightweight Power-Aware Real-Time OS.
** Distributed under the terms of LICENSE at the root of this repository.
*/
/* File LatencyPico2.c: The cycles from the alarm of an arrival to the first instruction
** of the task it releases, to compare the Cortex-M33 and the Hazard3 cores of the RP2350
** (issue #77). It covers the entry of the interrupt, the round of the kernel and the
** context switch.
**
** The idle task does not sleep: it stores the cycle counter into IdleStamp without end,
** a loop of three instructions on each architecture. The alarm interrupts that loop. The
** task, alone, of period 1 ms, reads the counter as it starts: the difference is the
** latency, to within one turn of the loop. The cycle counter is the DWT's CYCCNT on the
** Cortex-M33, which main starts, and mcycle on the Hazard3, which the RISC-V layer
** starts.
**
** Results holds the instances, then the sum, the least and the most of their latencies,
** the first ten left out, and the last one. tools/pico2_latency.py reads it over SWD.
** On the Pico 2 on 2026-11-06, at 150 MHz under the hard kernel and EDF: 371 cycles on
** the Cortex-M33, 474 on the Hazard3, within 3 cycles from one instance to the next.
** Platform version: RP2350 (Raspberry Pi Pico 2).
*/

#include "Escapement.h"

volatile struct {
  UINT32 Count, Sum, Min, Max, Last;
} Results = {0, 0, 0xFFFFFFFF, 0, 0};

static volatile UINT32 IdleStamp;

static void Stamp(void);
static void LatencyTask(void *argument);

#ifdef __riscv
   #define CYCLES() __extension__ ({ UINT32 c; \
              __asm volatile ("csrr %0, mcycle" : "=r"(c) :: "memory"); c; })
#else
   #define VTOR              *((volatile UINT32 *)0xE000ED08)
   #define DEMCR             *((volatile UINT32 *)0xE000EDFC)
   #define DEMCR_TRCENA      (1u << 24)
   #define DWT_CTRL          *((volatile UINT32 *)0xE0001000)
   #define DWT_CTRL_CYCCNTENA (1u << 0)
   #define DWT_CYCCNT        *((volatile UINT32 *)0xE0001004)
   #define CYCLES()          DWT_CYCCNT
#endif
#define WATCHDOG_CTRL_CLR *((volatile UINT32 *)(0x400D8000 + 0x3000))
#define WATCHDOG_ENABLE   (1u << 30)
#define SKIPPED           10   /* the first instances, before the loop settles */


int main(void)
{
  #ifndef __riscv
     extern void (* const CortexMxVectorTable[])(void);
     VTOR = (UINT32)CortexMxVectorTable;
     DEMCR |= DEMCR_TRCENA;
     DWT_CTRL |= DWT_CTRL_CYCCNTENA;
  #endif
  /* A firmware in flash may have armed the watchdog, which a reset of the processors
  ** leaves running. */
  WATCHDOG_CTRL_CLR = WATCHDOG_ENABLE;
  OSInitializeSystemClocks();
  #if defined(ESCAPEMENT_VERSION_SOFT)
     OSCreateTask(LatencyTask,0,0,1000,1000,1,1,0,NULL);
  #else
     OSCreateTask(LatencyTask,0,1000,1000,NULL);
  #endif
  _OSIdleHook = Stamp;
  return OSStartMultitasking(NULL,NULL);
} /* end of main */


/* Stamp: The idle task's loop, in place of its sleep. */
static void Stamp(void)
{
  while (TRUE)
     IdleStamp = CYCLES();
} /* end of Stamp */


/* LatencyTask: The cycles since the idle task's last stamp, counted. */
static void LatencyTask(void *argument)
{
  UINT32 latency = CYCLES() - IdleStamp;
  (void)argument;
  Results.Last = latency;
  Results.Count += 1;
  if (Results.Count > SKIPPED) {
     Results.Sum += latency;
     if (latency < Results.Min)
        Results.Min = latency;
     if (latency > Results.Max)
        Results.Max = latency;
  }
  OSEndTask();
} /* end of LatencyTask */
