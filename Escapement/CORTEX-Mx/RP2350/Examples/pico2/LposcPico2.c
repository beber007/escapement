/* Copyright (c) 2026 Bertrand Hurst. All rights reserved.
** Escapement - Lightweight Power-Aware Real-Time OS.
** Distributed under the terms of LICENSE at the root of this repository.
*/
/* File LposcPico2.c: How well LPOSC keeps time, against the crystal (docs/roadmap.md, item
** 5, step 3). A kernel sleeping in DORMANT would count the time asleep on the always-on
** timer, which runs on LPOSC, an RC oscillator; calibrated against the crystal before each
** sleep, its error is what is left between a calibration and the sleep that follows.
**
** No kernel: the clock set-up of the port alone, both timers running awake. The always-on
** timer of POWMAN counts milliseconds from LPOSC at its nominal 32.768 kHz, and TIMER0 the
** crystal's microseconds. Each window, about WINDOW_S seconds long, starts and ends on a
** step of the always-on timer's millisecond, polled: its milliseconds against TIMER0's
** microseconds give LPOSC's frequency to some 18 ppm a window of 60 s, the timer counting
** whole milliseconds. The frequency counter FC0 measures LPOSC as well, to 1/32 kHz,
** some 1,000 ppm of it, for comparison.
**
** Results, in words: 0 marker, 1 windows done, then for each window three words: the
** always-on timer's milliseconds, TIMER0's microseconds, FC0's result (kHz << 5 | 1/32).
** Platform version: RP2350 (Raspberry Pi Pico 2).
*/

#include "Escapement.h"

#define MARKER            0x4C504F53u   /* "LPOS" */
#ifndef WINDOW_S
   #define WINDOW_S       60u
#endif
#define WINDOWS           120u

#define RESETS_CLR        *((volatile UINT32 *)(0x40020000 + 0x3000))
#define RESETS_RESET_DONE *((volatile UINT32 *)0x40020008)
#define RESETS_TIMER0     (1u << 23)
#define TIMER0_TIMERAWL   *((volatile UINT32 *)(0x400B0000 + 0x28))

/* POWMAN, every write with its password in the upper half (RP2350 datasheet, 6.4). */
#define POWMAN_BASE       0x40100000
#define POWMAN_KEY        0x5AFE0000u
#define POWMAN_SET_TIME_63TO48 *((volatile UINT32 *)(POWMAN_BASE + 0x60))
#define POWMAN_SET_TIME_47TO32 *((volatile UINT32 *)(POWMAN_BASE + 0x64))
#define POWMAN_SET_TIME_31TO16 *((volatile UINT32 *)(POWMAN_BASE + 0x68))
#define POWMAN_SET_TIME_15TO0  *((volatile UINT32 *)(POWMAN_BASE + 0x6C))
#define POWMAN_READ_TIME_LOWER *((volatile UINT32 *)(POWMAN_BASE + 0x74))
#define POWMAN_TIMER      *((volatile UINT32 *)(POWMAN_BASE + 0x88))
#define TIMER_RUN         (1u << 1)
#define TIMER_USE_LPOSC   (1u << 8)
#define TIMER_USING_LPOSC (1u << 17)

/* The frequency counter (6.1.17), its reference clk_ref, the crystal's 12 MHz. */
#define CLOCKS_BASE       0x40010000
#define FC0_REF_KHZ       *((volatile UINT32 *)(CLOCKS_BASE + 0x8C))
#define FC0_MIN_KHZ       *((volatile UINT32 *)(CLOCKS_BASE + 0x90))
#define FC0_MAX_KHZ       *((volatile UINT32 *)(CLOCKS_BASE + 0x94))
#define FC0_DELAY         *((volatile UINT32 *)(CLOCKS_BASE + 0x98))
#define FC0_INTERVAL      *((volatile UINT32 *)(CLOCKS_BASE + 0x9C))
#define FC0_SRC           *((volatile UINT32 *)(CLOCKS_BASE + 0xA0))
#define FC0_STATUS        *((volatile UINT32 *)(CLOCKS_BASE + 0xA4))
#define FC0_RESULT        *((volatile UINT32 *)(CLOCKS_BASE + 0xA8))
#define FC0_SRC_LPOSC     0x0Eu
#define FC0_STATUS_DONE   (1u << 4)
#define FC0_STATUS_RUNNING (1u << 8)

volatile UINT32 Results[2 + 3 * WINDOWS];

static UINT32 MeasureLposc(void);
static void WaitStep(UINT32 *ms, UINT32 *us);

int main(void)
{
  UINT32 i, ms, us;
  OSInitializeSystemClocks();
  RESETS_CLR = RESETS_TIMER0;
  while ((RESETS_RESET_DONE & RESETS_TIMER0) == 0);
  Results[0] = MARKER;
  Results[1] = 0;
  /* Stopped, set to 0, then run from LPOSC at the nominal frequency of its registers. */
  POWMAN_TIMER = POWMAN_KEY;
  POWMAN_SET_TIME_63TO48 = POWMAN_KEY;
  POWMAN_SET_TIME_47TO32 = POWMAN_KEY;
  POWMAN_SET_TIME_31TO16 = POWMAN_KEY;
  POWMAN_SET_TIME_15TO0 = POWMAN_KEY;
  POWMAN_TIMER = POWMAN_KEY | TIMER_USE_LPOSC | TIMER_RUN;
  while ((POWMAN_TIMER & TIMER_USING_LPOSC) == 0);
  for (i = 0; i < WINDOWS; i += 1) {
     WaitStep(&ms, &us);
     Results[2 + 3 * i] = ms;
     Results[3 + 3 * i] = us;
     Results[4 + 3 * i] = MeasureLposc();
     Results[1] = i + 1;
     while (TIMER0_TIMERAWL - us < WINDOW_S * 1000000u);
  }
  while (TRUE);
} /* end of main */


/* WaitStep: The always-on timer's millisecond at its next step, and TIMER0 then. */
static void WaitStep(UINT32 *ms, UINT32 *us)
{
  UINT32 before = POWMAN_READ_TIME_LOWER, now;
  while ((now = POWMAN_READ_TIME_LOWER) == before);
  *us = TIMER0_TIMERAWL;
  *ms = now;
} /* end of WaitStep */


/* MeasureLposc: FC0's reading of LPOSC, kHz << 5 | 1/32 kHz, over its longest interval. */
static UINT32 MeasureLposc(void)
{
  while (FC0_STATUS & FC0_STATUS_RUNNING);
  FC0_REF_KHZ = 12000;
  FC0_MIN_KHZ = 0;
  FC0_MAX_KHZ = 0x1FFFFFF;
  FC0_DELAY = 1;
  FC0_INTERVAL = 15;
  FC0_SRC = FC0_SRC_LPOSC;
  while ((FC0_STATUS & FC0_STATUS_DONE) == 0);
  return FC0_RESULT;
} /* end of MeasureLposc */
