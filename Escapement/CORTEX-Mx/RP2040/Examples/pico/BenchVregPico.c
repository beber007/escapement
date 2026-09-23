/* Copyright (c) 2026 Bertrand Hurst. All rights reserved.
** Escapement - Lightweight Power-Aware Real-Time OS.
** Distributed under the terms of LICENSE at the root of this repository.
*/
/* File BenchVregPico.c: Times the settling of the core regulator of the RP2040 when its
** voltage is raised, without the kernel. Built with make KERNEL=PA bench.
**
** The only witness of the output the chip offers is the ROK bit of VREG, which says that
** the output has reached about 90 % of the voltage asked for: raising 1.05 V to 1.10 V,
** it never drops. Each step therefore starts lower, from 0.90, 0.95 or 1.00 V, below the
** 1.05 V the datasheet guarantees: the core runs at 12 MHz throughout, where 0.90 V has
** been reported to work, and the brown-out detector is lowered to 0.817 V first, as make
** UNDERVOLT=1 does. The chip restores 1.10 V and the default detector at the end.
**
** For each step, BENCH_REPS times: settle at the low voltage for 500 us, write the high
** one, and poll VREG until ROK has fallen and risen again, reading the 1 us counter of the
** timer at each turn; a pseudo-random wait first, as in BenchDVFSPico.c. What ROK gives is
** the time to reach 90 % of the target, not the full settling.
**
** BenchVreg[step][0..4]: sum of the times to ROK back, largest, repetitions where ROK fell
** and came back, repetitions where it never fell within 2000 us, repetitions where it
** fell and had not come back after 2000 us. BenchVregDone becomes 1 at the end.
** Platform version: RP2040.
*/

#include "Escapement.h"

#define BENCH_REPS 1000
#ifndef BENCH_SETTLE_US
   #define BENCH_SETTLE_US 500   /* time left at the low voltage before each step */
#endif

#define TIMER_TIMERAWL  *((volatile UINT32 *)(0x40054000 + 0x28))
#define TIMER_DBGPAUSE  *((volatile UINT32 *)(0x40054000 + 0x2C))

#define VREG            *((volatile UINT32 *)(0x40064000 + 0x00))
#define VREG_BOD        *((volatile UINT32 *)(0x40064000 + 0x04))
#define VREG_ROK        (1u << 12)
#define VREG_VSEL(v)    ((UINT32)(v) << 4)
#define VREG_EN         (1u << 0)
#define BOD_EN          (1u << 0)
#define BOD_VSEL_0_817V 0x8
#define BOD_VSEL_0_860V 0x9       /* the value at reset */

/* From, to: VSEL in 50 mV steps, 0.90 V at 0x7, 1.10 V at 0xB (RP2040 datasheet, VREG). */
static const UINT8 Steps[5][2] = {
  {0x7, 0xB},   /* 0.90 -> 1.10 V */
  {0x8, 0xB},   /* 0.95 -> 1.10 V */
  {0x9, 0xB},   /* 1.00 -> 1.10 V */
  {0xA, 0xB},   /* 1.05 -> 1.10 V, the step of the driver: ROK should not drop */
  {0x7, 0xA}    /* 0.90 -> 1.05 V */
};
volatile UINT32 BenchVreg[5][5];
volatile UINT32 BenchVregDone = 0;

#define VTOR *((volatile UINT32 *)0xE000ED08)


static void Wait(UINT32 us)
{
  UINT32 t = TIMER_TIMERAWL;
  while (TIMER_TIMERAWL - t < us);
} /* end of Wait */


static void Dither(void)
{
  static UINT32 lfsr = 0xACE1u;
  volatile UINT32 n;
  lfsr = (lfsr >> 1) ^ (-(lfsr & 1u) & 0xB400u);   /* 16-bit Galois LFSR */
  for (n = lfsr & 31; n > 0; n -= 1);
} /* end of Dither */


int main(void)
{
  extern void (* const CortexMxVectorTable[])(void);
  UINT32 i, k, t, now, dt, low;
  VTOR = (UINT32)CortexMxVectorTable;
  OSInitializeSystemClocks();
  OSInitProcessorSpeed();
  TIMER_DBGPAUSE = 0;   /* the loader leaves core 1 halted: let the counter run anyway */
  OSSetProcessorSpeed(OS_12MHZ_SPEED);
  VREG_BOD = VREG_VSEL(BOD_VSEL_0_817V) | BOD_EN;
  Wait(100);            /* the new threshold takes some 30 us to apply */

  for (k = 0; k < 5; k += 1) {
     for (i = 0; i < 5; i += 1)
        BenchVreg[k][i] = 0;
     for (i = 0; i < BENCH_REPS; i += 1) {
        VREG = VREG_VSEL(Steps[k][0]) | VREG_EN;
        Wait(BENCH_SETTLE_US);
        Dither();
        low = FALSE;
        t = TIMER_TIMERAWL;
        VREG = VREG_VSEL(Steps[k][1]) | VREG_EN;
        while (TRUE) {
           UINT32 v = VREG;
           now = TIMER_TIMERAWL;
           if ((v & VREG_ROK) == 0)
              low = TRUE;
           else if (low) {                            /* fell and came back */
              dt = now - t;
              BenchVreg[k][0] += dt;
              if (dt > BenchVreg[k][1])
                 BenchVreg[k][1] = dt;
              BenchVreg[k][2] += 1;
              break;
           }
           if (now - t > 2000) {
              BenchVreg[k][low ? 4 : 3] += 1;
              break;
           }
        }
     }
  }
  VREG = VREG_VSEL(0xB) | VREG_EN;                  /* 1.10 V */
  Wait(1000);
  VREG_BOD = VREG_VSEL(BOD_VSEL_0_860V) | BOD_EN;
  BenchVregDone = 1;
  while (TRUE)
     __asm("WFI");
} /* end of main */
