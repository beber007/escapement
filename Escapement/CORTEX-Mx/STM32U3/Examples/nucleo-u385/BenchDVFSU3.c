/* Copyright (c) 2026 Bertrand Hurst. All rights reserved.
** Escapement - Lightweight Power-Aware Real-Time OS.
** Distributed under the terms of LICENSE at the root of this repository.
*/
/* File BenchDVFSU3.c: The DVFS driver of the STM32U385 on the board, without the kernel:
** how long its changes of operating point take, and whether the kernel's clock, TIM2,
** keeps its time across them. Built with make KERNEL=PA. Interrupts stay masked.
**
** TIM2 counts the MSIK under the power-aware kernel, not HCLK, whose frequency the
** changes halve and double (Escapement_Processor.h): its microsecond should not move when
** the speed does. The bench counts TIM2 against LPTIM1, which counts the LSE, over two
** windows of BENCH_WINDOW_S seconds each, both begun and ended on an edge of LPTIM1: the
** first at 96 MHz without a change, the second changing speed as fast as the driver can,
** some two million times a minute. The MSIK is locked on the same LSE, so both windows
** should read the same ratio, 107 ppm above 1 (Escapement_Processor.c); the difference
** between them is what the changes cost the kernel's clock, within a tick of LPTIM1 at
** each end, half a ppm over a minute. A change that lost a few hundredths of a microsecond
** would show as hundreds of ppm.
**
** Then each change, BENCH_REPS times, on TIM2, the sum giving the mean below the
** microsecond: Dither() waits a pseudo-random 0 to 31 turns of a loop before each, so
** that the phase of the counter is unrelated to it, as BenchDVFSPico does. Two reads of
** TIM2 with nothing between them, at each speed, to be subtracted; and the longest of
** each change, which is how long the change holds the interrupts.
**
** A line goes out on USART1, at 115,200 baud from HSI16, to the virtual COM port of the
** ST-LINK, at the end of each round, and the rounds go on: "DVFSB" and, in hexadecimal,
** the words 1 to 15 of Results. Results, in words: 0 marker, 1 rounds, 2 TIM2's
** microseconds of the still window, 3 its ticks of LPTIM1, 4 those of the window of
** changes, 5 its ticks, 6 the changes made in it, 7 the sum of 48 -> 96 MHz, 8 of
** 96 -> 48, 9 of two reads at 48 MHz, 10 at 96, 11 of _OSRaiseSpeedOnWake from 48 MHz,
** 12 the longest 48 -> 96, 13 the longest 96 -> 48, in microseconds, 14 BENCH_REPS,
** 15 1 if the LSE did not start, the bench then without its reference.
** Platform version: STM32U385 (NUCLEO-U385RG-Q).
*/

#include "Escapement.h"
#include "Escapement_Timer.h"
#include "Escapement_LPTimer.h"
#include "Escapement_UART.h"

#define MARKER         0x44564642u   /* "DVFB" */
#ifndef BENCH_WINDOW_S
   #define BENCH_WINDOW_S 60u
#endif
#ifndef BENCH_WINDOW_TICKS            /* BenchDVFSU3Short: some ms, for Renode */
   #define BENCH_WINDOW_TICKS (BENCH_WINDOW_S * OS_LPTIMER_HZ)
#endif
#ifndef BENCH_REPS
   #define BENCH_REPS  10000u
#endif
#define WORDS          15u
#define TIMER_MASK     0x3FFFFFFFu   /* TIM2 wraps at 2^30 (Escapement_Timer.c) */

#define USART1_ISR     *((volatile UINT32 *)(0x40013800 + 0x1C))
#define USART1_TDR     *((volatile UINT32 *)(0x40013800 + 0x28))
#define ISR_TC         (1u << 6)
#define ISR_TXE        (1u << 7)

volatile UINT32 Results[WORDS + 1];

static void Window(BOOL changing, UINT32 *micros, UINT32 *ticks, UINT32 *changes);
static void TimeChange(UINT8 from, UINT8 to, UINT32 index, UINT32 maxIndex);
static void Dither(void);
static UINT16 NextTick(void);
static void Report(void);


int main(void)
{
  UINT32 i, k, t, sum;
  _OSDisableInterrupts();
  OSInitializeSystemClocks();
  OSInitProcessorSpeed();
  Results[0] = MARKER;
  Results[14] = BENCH_REPS;
  /* USART1 as the port sets it, on HSI16, written to here without its interrupt. */
  OSInitUART(1,1,NULL,OS_IO_USART1);
  /* TIM2 as the kernel sets and starts it, its interrupts left pending, never taken. */
  _OSInitializeTimer();
  _OSStartTimer();
  Results[15] = !OSInitLPTimer();
  while (TRUE) {
     if (!Results[15]) {
        Window(FALSE,(UINT32 *)&Results[2],(UINT32 *)&Results[3],NULL);
        Window(TRUE,(UINT32 *)&Results[4],(UINT32 *)&Results[5],(UINT32 *)&Results[6]);
     }
     TimeChange(OS_48MHZ_SPEED,OS_96MHZ_SPEED,7,12);
     TimeChange(OS_96MHZ_SPEED,OS_48MHZ_SPEED,8,13);
     for (k = 0; k < 2; k += 1) {
        OSSetProcessorSpeed(k == 0 ? OS_48MHZ_SPEED : OS_96MHZ_SPEED);
        sum = 0;
        for (i = 0; i < BENCH_REPS; i += 1) {
           Dither();
           t = (UINT32)_OSGetActualTime();
           sum += ((UINT32)_OSGetActualTime() - t) & TIMER_MASK;
        }
        Results[9 + k] = sum;
     }
     sum = 0;
     for (i = 0; i < BENCH_REPS; i += 1) {
        OSSetProcessorSpeed(OS_48MHZ_SPEED);
        _OSIdleAsleep = TRUE;
        Dither();
        t = (UINT32)_OSGetActualTime();
        _OSRaiseSpeedOnWake();
        sum += ((UINT32)_OSGetActualTime() - t) & TIMER_MASK;
     }
     Results[11] = sum;
     Results[1] += 1;
     Report();
  }
} /* end of main */


/* Window: TIM2's microseconds and LPTIM1's ticks over BENCH_WINDOW_TICKS of LPTIM1, from
** an edge of it to another, at 96 MHz, or changing speed at each turn. Both counters are
** read at each turn, often enough that neither wraps between two reads. */
static void Window(BOOL changing, UINT32 *micros, UINT32 *ticks, UINT32 *changes)
{
  UINT32 us = 0, counted = 0, n = 0, time, lastTime;
  UINT16 tick, lastTick;
  UINT8 speed = OS_96MHZ_SPEED;
  OSSetProcessorSpeed(speed);
  lastTick = NextTick();
  lastTime = (UINT32)_OSGetActualTime();
  while (counted < BENCH_WINDOW_TICKS) {
     if (changing) {
        speed = speed == OS_96MHZ_SPEED ? OS_48MHZ_SPEED : OS_96MHZ_SPEED;
        OSSetProcessorSpeed(speed);
        n += 1;
     }
     tick = OSGetLPTimer();
     counted += (UINT16)(tick - lastTick);
     lastTick = tick;
     time = (UINT32)_OSGetActualTime();
     us += (time - lastTime) & TIMER_MASK;
     lastTime = time;
  }
  OSSetProcessorSpeed(OS_96MHZ_SPEED);
  tick = NextTick();
  time = (UINT32)_OSGetActualTime();
  counted += (UINT16)(tick - lastTick);
  us += (time - lastTime) & TIMER_MASK;
  *micros = us;
  *ticks = counted;
  if (changes != NULL)
     *changes = n;
} /* end of Window */


/* TimeChange: One change BENCH_REPS times, its sum in Results[index], its longest in
** Results[maxIndex]; the change back untimed. */
static void TimeChange(UINT8 from, UINT8 to, UINT32 index, UINT32 maxIndex)
{
  UINT32 i, t, d, sum = 0, max = 0;
  for (i = 0; i < BENCH_REPS; i += 1) {
     OSSetProcessorSpeed(from);
     Dither();
     t = (UINT32)_OSGetActualTime();
     OSSetProcessorSpeed(to);
     d = ((UINT32)_OSGetActualTime() - t) & TIMER_MASK;
     sum += d;
     if (d > max)
        max = d;
  }
  Results[index] = sum;
  Results[maxIndex] = max;
} /* end of TimeChange */


static void Dither(void)
{
  static UINT32 lfsr = 0xACE1u;
  volatile UINT32 n;
  lfsr = (lfsr >> 1) ^ (-(lfsr & 1u) & 0xB400u);   /* 16-bit Galois LFSR */
  for (n = lfsr & 31; n > 0; n -= 1);
} /* end of Dither */


/* NextTick: Waits for the count of LPTIM1 to change, and returns the new one. */
static UINT16 NextTick(void)
{
  UINT16 count = OSGetLPTimer(), next;
  while ((next = OSGetLPTimer()) == count);
  return next;
} /* end of NextTick */


/* Report: "DVFSB" and the words 1 to 15 of Results in hexadecimal, left to drain. */
static void Report(void)
{
  static const char digits[] = "0123456789ABCDEF";
  const char *text = "DVFSB";
  UINT32 w, k, value;
  while (*text != '\0') {
     while ((USART1_ISR & ISR_TXE) == 0);
     USART1_TDR = (UINT32)*text++;
  }
  for (w = 1; w <= WORDS; w += 1) {
     value = Results[w];
     while ((USART1_ISR & ISR_TXE) == 0);
     USART1_TDR = ' ';
     for (k = 0; k < 8; k += 1) {
        while ((USART1_ISR & ISR_TXE) == 0);
        USART1_TDR = (UINT32)digits[(value >> (28 - 4 * k)) & 0xF];
     }
  }
  while ((USART1_ISR & ISR_TXE) == 0);
  USART1_TDR = '\n';
  while ((USART1_ISR & ISR_TC) == 0);
} /* end of Report */
