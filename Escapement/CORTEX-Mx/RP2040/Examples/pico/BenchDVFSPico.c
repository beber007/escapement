/* Copyright (c) 2026 Bertrand Hurst. All rights reserved.
** Escapement - Lightweight Power-Aware Real-Time OS.
** Distributed under the terms of LICENSE at the root of this repository.
*/
/* File BenchDVFSPico.c: Times the changes of operating point of the DVFS driver of the
** RP2040, without the kernel. Built with make KERNEL=PA bench.
**
** Each measurement repeats one change BENCH_REPS times, reading the 1 us counter of the
** timer before and after it, and adds up the differences: the sum over many repetitions
** gives the mean well below the microsecond, provided the phase of the counter is
** unrelated to the change. It is not by itself: at 12 MHz the core and the counter run
** off the same crystal, and a loop of fixed length meets the counter at the same phase
** every time. Dither() therefore waits a pseudo-random 0 to 31 turns of a loop before
** each measurement, more than a microsecond at every speed. Two reads of the counter with
** nothing between them are timed too, at each speed, to be subtracted.
**
** BenchSum holds the sums, in the order listed above its definition, over BENCH_REPS
** repetitions each; BenchDone becomes 1 at the end. tools/dvfs_bench.py loads the image,
** waits for it and prints the means, without stopping a core.
** Platform version: RP2040.
*/

#include "Escapement.h"

#define BENCH_REPS 10000

#define TIMER_TIMERAWL  *((volatile UINT32 *)(0x40054000 + 0x28))
#define TIMER_DBGPAUSE  *((volatile UINT32 *)(0x40054000 + 0x2C))

/* The registers the driver writes, to time its steps one by one (Escapement_Processor.c). */
#define VREG            *((volatile UINT32 *)(0x40064000 + 0x00))
#define VREG_VSEL(v)    ((UINT32)(v) << 4)
#define VREG_EN         (1u << 0)
#define VSEL_1_05V      0xA
#define VSEL_1_10V      0xB
#define CLK_SYS_CTRL     *((volatile UINT32 *)(0x40008000 + 0x3C))
#define CLK_SYS_SELECTED *((volatile UINT32 *)(0x40008000 + 0x44))
#define CLK_SYS_SRC_REF  0
#define CLK_SYS_SRC_AUX  1
#define PLL_PRIM         *((volatile UINT32 *)(0x40028000 + 0x0C))
#define PLL_PRIM_125MHZ  ((6u << 16) | (2u << 12))

/*  0- 5: OSSetProcessorSpeed from one operating point to another
**  6- 8: two reads of the counter, at 12, 50 and 125 MHz
**  9-12: the steps of 12 -> 125 MHz, at 12 MHz: VREG, clk_sys onto the reference (already
**        there), the post-divider of the PLL, clk_sys onto the PLL
** 13-14: _OSRaiseSpeedOnWake from 12 and from 50 MHz, the wake-up path of a slow idle task */
volatile UINT32 BenchSum[15];
volatile UINT32 BenchDone = 0;

static const UINT8 Pairs[6][2] = {{0,2},{2,0},{1,2},{2,1},{0,1},{1,0}};

static void Dither(void)
{
  static UINT32 lfsr = 0xACE1u;
  volatile UINT32 n;
  lfsr = (lfsr >> 1) ^ (-(lfsr & 1u) & 0xB400u);   /* 16-bit Galois LFSR */
  for (n = lfsr & 31; n > 0; n -= 1);
} /* end of Dither */

#define VTOR *((volatile UINT32 *)0xE000ED08)


int main(void)
{
  extern void (* const CortexMxVectorTable[])(void);
  UINT32 i, k, t, sum;
  VTOR = (UINT32)CortexMxVectorTable;
  OSInitializeSystemClocks();
  OSInitProcessorSpeed();
  TIMER_DBGPAUSE = 0;   /* the loader leaves core 1 halted: let the counter run anyway */

  for (k = 0; k < 6; k += 1) {
     sum = 0;
     for (i = 0; i < BENCH_REPS; i += 1) {
        OSSetProcessorSpeed(Pairs[k][0]);
        Dither();
        t = TIMER_TIMERAWL;
        OSSetProcessorSpeed(Pairs[k][1]);
        sum += TIMER_TIMERAWL - t;
     }
     BenchSum[k] = sum;
  }

  for (k = 0; k < 3; k += 1) {
     OSSetProcessorSpeed(k);
     sum = 0;
     for (i = 0; i < BENCH_REPS; i += 1) {
        Dither();
        t = TIMER_TIMERAWL;
        sum += TIMER_TIMERAWL - t;
     }
     BenchSum[6 + k] = sum;
  }

  /* The steps of 12 -> 125 MHz, each timed alone at 12 MHz and undone untimed. */
  OSSetProcessorSpeed(0);
  for (k = 9; k < 13; k += 1)
     BenchSum[k] = 0;
  for (i = 0; i < BENCH_REPS; i += 1) {
     Dither();
     t = TIMER_TIMERAWL;
     VREG = VREG_VSEL(VSEL_1_10V) | VREG_EN;
     BenchSum[9] += TIMER_TIMERAWL - t;
     Dither();
     t = TIMER_TIMERAWL;
     CLK_SYS_CTRL = CLK_SYS_SRC_REF;
     while ((CLK_SYS_SELECTED & (1u << CLK_SYS_SRC_REF)) == 0);
     BenchSum[10] += TIMER_TIMERAWL - t;
     Dither();
     t = TIMER_TIMERAWL;
     PLL_PRIM = PLL_PRIM_125MHZ;
     BenchSum[11] += TIMER_TIMERAWL - t;
     Dither();
     t = TIMER_TIMERAWL;
     CLK_SYS_CTRL = CLK_SYS_SRC_AUX;
     while ((CLK_SYS_SELECTED & (1u << CLK_SYS_SRC_AUX)) == 0);
     BenchSum[12] += TIMER_TIMERAWL - t;
     /* back to 12 MHz and 1.05 V, as the driver leaves them */
     CLK_SYS_CTRL = CLK_SYS_SRC_REF;
     while ((CLK_SYS_SELECTED & (1u << CLK_SYS_SRC_REF)) == 0);
     VREG = VREG_VSEL(VSEL_1_05V) | VREG_EN;
  }
  for (k = 0; k < 2; k += 1) {
     sum = 0;
     for (i = 0; i < BENCH_REPS; i += 1) {
        OSSetProcessorSpeed(k);
        _OSIdleAsleep = TRUE;
        Dither();
        t = TIMER_TIMERAWL;
        _OSRaiseSpeedOnWake();
        sum += TIMER_TIMERAWL - t;
     }
     BenchSum[13 + k] = sum;
  }
  BenchDone = 1;
  while (TRUE)
     __asm("WFI");
} /* end of main */
