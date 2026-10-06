/* Copyright (c) 2026 Bertrand Hurst. All rights reserved.
** Escapement - Lightweight Power-Aware Real-Time OS.
** Distributed under the terms of LICENSE at the root of this repository.
*/
/* File DormantPico2.c: DORMANT woken by the always-on timer, and how well that timer keeps
** the time asleep (docs/roadmap.md, item 5, step 3). LposcPico2 measured LPOSC awake;
** asleep, its supply and its load differ, and only a clock outside the chip can tell.
**
** The always-on timer compares its alarm only while its clock, clk_ref when the switched
** core is powered, runs (RP2350 datasheet, 8.1, and SleepPico2.c, where clk_ref on the
** crystal never woke): clk_ref goes to LPOSC, which never stops, and clk_sys to the crystal
** directly, which DORMANT stops with the processor (6.5.3). The alarm restarts the crystal.
**
** First LPOSC is calibrated awake for CAL_S seconds against TIMER0 on the crystal, and the
** always-on timer given the frequency found (POWMAN_LPOSC_FREQ_KHZ_INT and _FRAC). Then
** CYCLES sleeps follow, each until an alarm SLEEP_MS after the one before, on the timer's
** own milliseconds. At each wake a line goes out on UART0, GP0, to the probe's UART at
** 115,200 baud: "WAKE", the cycle, and the microseconds Send took in the cycle before,
** its bytes left in the UART's FIFO (2 or 3 on the board): not the time awake; on the
** line of cycle 0, the frequency the calibration found, in 1/65536 kHz. The
** UNO Q stamps each line on its own clock: between the lines of cycles 1 and CYCLES,
** (CYCLES - 1) * SLEEP_MS of the always-on timer, each after a wake-up of the same delay;
** the line of cycle 0 follows none, and would count one delay, a millisecond or more.
**
** Built with STAY_AWAKE, the crystal is not put dormant: the core waits for the alarm
** awake, the clocks as asleep, the control of what DORMANT itself adds. Built with
** SLEEP_ON_ROSC, clk_sys goes to the ring oscillator for the sleep, which DORMANT stops
** instead of the crystal, left running: whether the crystal stopping is what moves LPOSC.
**
** Results, in words: 0 marker, 1 cycles done, 2 LPOSC's frequency found, in 1/65536 kHz.
** Platform version: RP2350 (Raspberry Pi Pico 2).
*/

#include "Escapement.h"

#define MARKER            0x444F524Du   /* "DORM" */
#ifndef CAL_S
   #define CAL_S          60u
#endif
#ifndef SLEEP_MS
   #define SLEEP_MS       10000u
#endif
#ifndef CYCLES
   #define CYCLES         60u
#endif
#define UART_TX_PIN       0u

#define VTOR              *((volatile UINT32 *)0xE000ED08)
#define RESETS_CLR        *((volatile UINT32 *)(0x40020000 + 0x3000))
#define RESETS_RESET_DONE *((volatile UINT32 *)0x40020008)
#define RESETS_IO_BANK0   (1u << 6)
#define RESETS_PADS_BANK0 (1u << 9)
#define RESETS_TIMER0     (1u << 23)
#define RESETS_UART0      (1u << 26)
#define RESETS_USED       (RESETS_IO_BANK0 | RESETS_PADS_BANK0 | RESETS_TIMER0 | RESETS_UART0)
#define IO_BANK0_CTRL(p)  *((volatile UINT32 *)(0x40028000 + 0x04 + 8 * (p)))
#define PADS_BANK0_GPIO(p) *((volatile UINT32 *)(0x40038000 + 0x04 + 4 * (p)))
#define PADS_ISO_BIT      (1u << 8)
#define FUNCSEL_UART      2u
#define UART0_BASE        0x40070000
#define UART0_DR          *((volatile UINT32 *)(UART0_BASE + 0x00))
#define UART0_FR          *((volatile UINT32 *)(UART0_BASE + 0x18))
#define UART0_IBRD        *((volatile UINT32 *)(UART0_BASE + 0x24))
#define UART0_FBRD        *((volatile UINT32 *)(UART0_BASE + 0x28))
#define UART0_LCR_H       *((volatile UINT32 *)(UART0_BASE + 0x2C))
#define UART0_CR          *((volatile UINT32 *)(UART0_BASE + 0x30))
#define UART_FR_TXFF      (1u << 5)
#define UART_FR_BUSY      (1u << 3)
#define TIMER0_TIMERAWL   *((volatile UINT32 *)(0x400B0000 + 0x28))
#define TICKS_TIMER0_CTRL *((volatile UINT32 *)(0x40108000 + 0x18))
#define TICKS_TIMER0_CYCLES *((volatile UINT32 *)(0x40108000 + 0x1C))

/* The clock muxes (pico-sdk, hardware/regs/clocks.h). */
#define CLOCKS_BASE       0x40010000
#define CLK_REF_CTRL      *((volatile UINT32 *)(CLOCKS_BASE + 0x30))
#define CLK_REF_SELECTED  *((volatile UINT32 *)(CLOCKS_BASE + 0x38))
#define CLK_SYS_CTRL      *((volatile UINT32 *)(CLOCKS_BASE + 0x3C))
#define CLK_SYS_SELECTED  *((volatile UINT32 *)(CLOCKS_BASE + 0x44))
#define CLK_REF_SRC_XOSC  2u
#define CLK_REF_SRC_LPOSC 3u
#define CLK_SYS_SRC_REF   0u
#define CLK_SYS_SRC_AUX   1u
#define CLK_SYS_AUX_XOSC  (3u << 5)
#define CLK_SYS_AUX_PLL   (0u << 5)
#define XOSC_DORMANT      *((volatile UINT32 *)(0x40048000 + 0x08))
#define XOSC_DORMANT_WORD 0x636F6D61u   /* "coma" */
#define CLK_SYS_AUX_ROSC  (2u << 5)
#define ROSC_DORMANT      *((volatile UINT32 *)(0x400E8000 + 0x10))   /* pico-sdk, rosc.h */
#define ROSC_STATUS       *((volatile UINT32 *)(0x400E8000 + 0x1C))
#define ROSC_STABLE       (1u << 31)

/* POWMAN, every write with its password in the upper half (pico-sdk, hardware/regs/
** powman.h). */
#define POWMAN_BASE       0x40100000
#define POWMAN_KEY        0x5AFE0000u
#define POWMAN_LPOSC_KHZ_INT  *((volatile UINT32 *)(POWMAN_BASE + 0x50))
#define POWMAN_LPOSC_KHZ_FRAC *((volatile UINT32 *)(POWMAN_BASE + 0x54))
#define POWMAN_SET_TIME(n)    *((volatile UINT32 *)(POWMAN_BASE + 0x60 + 4 * (n)))  /* 63to48 first */
#define POWMAN_READ_TIME_LOWER *((volatile UINT32 *)(POWMAN_BASE + 0x74))
#define POWMAN_ALARM_TIME(n)  *((volatile UINT32 *)(POWMAN_BASE + 0x78 + 4 * (n))) /* 63to48 first */
#define POWMAN_TIMER      *((volatile UINT32 *)(POWMAN_BASE + 0x88))
#define TIMER_RUN         (1u << 1)
#define TIMER_ALARM_ENAB  (1u << 4)
#define TIMER_ALARM       (1u << 6)
#define TIMER_USE_LPOSC   (1u << 8)
#define TIMER_USING_LPOSC (1u << 17)

volatile UINT32 Results[3];

static UINT32 Calibrate(void);
static void ClocksAsleep(void);
static void ClocksAwake(void);
static void StartTimer(UINT32 ms);
static void SleepUntil(UINT32 ms);
static void Send(UINT32 cycle, UINT32 awake);

int main(void)
{
  extern void (* const CortexMxVectorTable[])(void);
  UINT32 cycle, alarm, woke = 0, awake;
  VTOR = (UINT32)CortexMxVectorTable;
  _OSDisableInterrupts();               // DORMANT ends without an interrupt
  OSInitializeSystemClocks();
  RESETS_CLR = RESETS_USED;
  while ((RESETS_RESET_DONE & RESETS_USED) != RESETS_USED);
  UART0_IBRD = 6;                       // 115,200 baud from clk_peri, the crystal's 12 MHz
  UART0_FBRD = 33;
  UART0_LCR_H = 0x70;                   // 8 bits, FIFO on; written after the divisors
  UART0_CR = 0x101;                     // UARTEN, TXE
  IO_BANK0_CTRL(UART_TX_PIN) = FUNCSEL_UART;
  PADS_BANK0_GPIO(UART_TX_PIN) &= ~PADS_ISO_BIT;
  /* TIMER0 counts the crystal's microseconds, without the kernel that starts its tick. */
  TICKS_TIMER0_CTRL = 0;
  TICKS_TIMER0_CYCLES = 12;
  TICKS_TIMER0_CTRL = 1;
  Results[0] = MARKER;
  Results[1] = 0;
  /* The nominal 32.768 kHz Calibrate measures against, whatever a previous image left. */
  POWMAN_TIMER = POWMAN_KEY | TIMER_USE_LPOSC;
  POWMAN_LPOSC_KHZ_INT = POWMAN_KEY | 32u;
  POWMAN_LPOSC_KHZ_FRAC = POWMAN_KEY | 0xC49Cu;
  StartTimer(0);
  Results[2] = Calibrate();
  /* The frequency is written with the timer stopped (powman.h), its count kept. */
  POWMAN_TIMER = POWMAN_KEY | TIMER_USE_LPOSC;
  alarm = POWMAN_READ_TIME_LOWER;
  POWMAN_LPOSC_KHZ_INT = POWMAN_KEY | (Results[2] >> 16);
  POWMAN_LPOSC_KHZ_FRAC = POWMAN_KEY | (Results[2] & 0xFFFFu);
  StartTimer(alarm);
  awake = Results[2];                   // the first line gives the frequency found
  for (cycle = 0; cycle <= CYCLES; cycle += 1) {
     Send(cycle, awake);
     alarm += SLEEP_MS;
     awake = TIMER0_TIMERAWL - woke;
     SleepUntil(alarm);
     woke = TIMER0_TIMERAWL;
     Results[1] = cycle + 1;
  }
  while (TRUE);
} /* end of main */


/* StartTimer: The always-on timer counting from LPOSC, from ms on. */
static void StartTimer(UINT32 ms)
{
  POWMAN_TIMER = POWMAN_KEY | TIMER_USE_LPOSC;
  POWMAN_SET_TIME(0) = POWMAN_KEY;
  POWMAN_SET_TIME(1) = POWMAN_KEY;
  POWMAN_SET_TIME(2) = POWMAN_KEY | (ms >> 16);
  POWMAN_SET_TIME(3) = POWMAN_KEY | (ms & 0xFFFFu);
  POWMAN_TIMER = POWMAN_KEY | TIMER_USE_LPOSC | TIMER_RUN;
  while ((POWMAN_TIMER & TIMER_USING_LPOSC) == 0);
} /* end of StartTimer */


/* Calibrate: LPOSC's frequency in 1/65536 kHz, from the timer's milliseconds at its
** nominal 32.768 kHz against TIMER0's microseconds, between two steps of the millisecond
** CAL_S seconds apart (LposcPico2.c).
** Built with CAL_ASLEEP_CLOCKS, the clocks are set as SleepUntil sets them before DORMANT,
** clk_ref on LPOSC, which then feeds the clock tree of clk_ref too: whether that load is
** what moves LPOSC asleep (docs/roadmap.md, item 5). TIMER0 ticks from clk_ref and no
** longer counts microseconds then: the cycles of clk_sys, the crystal's 12 MHz, are
** counted instead, by the DWT's cycle counter. */
#define DEMCR             *((volatile UINT32 *)0xE000EDFC)
#define DEMCR_TRCENA      (1u << 24)
#define DWT_CTRL          *((volatile UINT32 *)0xE0001000)
#define DWT_CYCCNT        *((volatile UINT32 *)0xE0001004)
static UINT32 Calibrate(void)
{
  UINT32 before, ms0, ms1, us0, us1;
  #ifdef CAL_ASLEEP_CLOCKS
     UINT32 c0, c1;
     ClocksAsleep();
     DEMCR |= DEMCR_TRCENA;
     DWT_CYCCNT = 0;
     DWT_CTRL |= 1u;                    // CYCCNTENA
     before = POWMAN_READ_TIME_LOWER;
     while ((ms0 = POWMAN_READ_TIME_LOWER) == before);
     c0 = DWT_CYCCNT;
     while (DWT_CYCCNT - c0 < CAL_S * 12000000u);
     before = POWMAN_READ_TIME_LOWER;
     while ((ms1 = POWMAN_READ_TIME_LOWER) == before);
     c1 = DWT_CYCCNT;
     ClocksAwake();
     (void)us0;
     (void)us1;
     return (UINT32)(((unsigned long long)32768u * 65536u * 12u * (ms1 - ms0)) / (c1 - c0));
  #else
     before = POWMAN_READ_TIME_LOWER;
     while ((ms0 = POWMAN_READ_TIME_LOWER) == before);
     us0 = TIMER0_TIMERAWL;
     while (TIMER0_TIMERAWL - us0 < CAL_S * 1000000u);
     before = POWMAN_READ_TIME_LOWER;
     while ((ms1 = POWMAN_READ_TIME_LOWER) == before);
     us1 = TIMER0_TIMERAWL;
     /* 32.768 kHz times the milliseconds counted per millisecond elapsed, in 1/65536 kHz:
     ** 32768 * 65536 * ms / us. */
     return (UINT32)(((unsigned long long)32768u * 65536u * (ms1 - ms0)) / (us1 - us0));
  #endif
} /* end of Calibrate */


/* ClocksAsleep: clk_sys on the crystal itself and clk_ref on LPOSC, PLL_SYS stopped. */
static void ClocksAsleep(void)
{
  _OSLowerSystemClock();                // clk_sys on clk_ref, the crystal; PLL_SYS off
  CLK_SYS_CTRL = CLK_SYS_AUX_XOSC | CLK_SYS_SRC_REF;
  CLK_SYS_CTRL = CLK_SYS_AUX_XOSC | CLK_SYS_SRC_AUX;
  while ((CLK_SYS_SELECTED & (1u << CLK_SYS_SRC_AUX)) == 0);
  CLK_REF_CTRL = CLK_REF_SRC_LPOSC;
  while ((CLK_REF_SELECTED & (1u << CLK_REF_SRC_LPOSC)) == 0);
} /* end of ClocksAsleep */


/* ClocksAwake: clk_ref and clk_sys back as the port has them. */
static void ClocksAwake(void)
{
  CLK_REF_CTRL = CLK_REF_SRC_XOSC;
  while ((CLK_REF_SELECTED & (1u << CLK_REF_SRC_XOSC)) == 0);
  CLK_SYS_CTRL = CLK_SYS_AUX_XOSC | CLK_SYS_SRC_REF;
  while ((CLK_SYS_SELECTED & (1u << CLK_SYS_SRC_REF)) == 0);
  /* The auxiliary source back on the PLL while clk_sys is on clk_ref: _OSRaiseSystemClock
  ** takes the auxiliary in the write that would otherwise change its source too, a
  ** glitch the datasheet forbids (8.1.3.2; a review, 2026-10-06). */
  CLK_SYS_CTRL = CLK_SYS_AUX_PLL | CLK_SYS_SRC_REF;
  _OSRaiseSystemClock();
} /* end of ClocksAwake */


/* SleepUntil: DORMANT until the timer reaches ms. clk_sys goes to the crystal itself and
** clk_ref to LPOSC, PLL_SYS stopped; the crystal put dormant stops the processor, and the
** alarm restarts it. Then clk_ref and clk_sys back as the port has them. */
static void SleepUntil(UINT32 ms)
{
  while (UART0_FR & UART_FR_BUSY);
  ClocksAsleep();
  /* USE_LPOSC is a request to switch, which may cost up to two LPOSC periods (12.10.5.1):
  ** the timer already runs on it, and these writes leave it out. */
  POWMAN_TIMER = POWMAN_KEY | TIMER_RUN | TIMER_ALARM;
  POWMAN_ALARM_TIME(0) = POWMAN_KEY;
  POWMAN_ALARM_TIME(1) = POWMAN_KEY;
  POWMAN_ALARM_TIME(2) = POWMAN_KEY | (ms >> 16);
  POWMAN_ALARM_TIME(3) = POWMAN_KEY | (ms & 0xFFFFu);
  POWMAN_TIMER = POWMAN_KEY | TIMER_RUN | TIMER_ALARM_ENAB;
  /* Only the comparison armed when the crystal stops wakes the chip, not the ALARM status
  ** (6.5.3.1): the write read back first, in POWMAN's clock, now LPOSC's. */
  while ((POWMAN_TIMER & TIMER_ALARM_ENAB) == 0);
#if defined(SLEEP_ON_ROSC)
  /* clk_sys onto the ROSC through clk_ref, the auxiliary changed while not selected. */
  CLK_SYS_CTRL = CLK_SYS_AUX_XOSC | CLK_SYS_SRC_REF;
  while ((CLK_SYS_SELECTED & (1u << CLK_SYS_SRC_REF)) == 0);
  CLK_SYS_CTRL = CLK_SYS_AUX_ROSC | CLK_SYS_SRC_REF;
  CLK_SYS_CTRL = CLK_SYS_AUX_ROSC | CLK_SYS_SRC_AUX;
  while ((CLK_SYS_SELECTED & (1u << CLK_SYS_SRC_AUX)) == 0);
  ROSC_DORMANT = XOSC_DORMANT_WORD;     // the same keyword (rosc.h)
#elif !defined(STAY_AWAKE)
  XOSC_DORMANT = XOSC_DORMANT_WORD;
#endif
  __asm volatile ("DSB\n\tISB" ::: "memory");
  /* The core may run a few instructions before the crystal's output stops: none may disarm
  ** the alarm before it has fired. */
  while ((POWMAN_TIMER & TIMER_ALARM) == 0);
  POWMAN_TIMER = POWMAN_KEY | TIMER_RUN | TIMER_ALARM;
#if defined(SLEEP_ON_ROSC)
  while ((ROSC_STATUS & ROSC_STABLE) == 0);
  CLK_SYS_CTRL = CLK_SYS_AUX_ROSC | CLK_SYS_SRC_REF;
  while ((CLK_SYS_SELECTED & (1u << CLK_SYS_SRC_REF)) == 0);
#endif
  ClocksAwake();
} /* end of SleepUntil */


/* Send: "WAKE", the cycle and the microseconds awake before, in hexadecimal. */
static void Send(UINT32 cycle, UINT32 awake)
{
  static const char digits[] = "0123456789ABCDEF";
  const char *text = "WAKE";
  UINT32 w, k, value;
  while (*text != '\0') {
     while (UART0_FR & UART_FR_TXFF);
     UART0_DR = (UINT32)*text++;
  }
  for (w = 0; w < 2; w += 1) {
     value = w == 0 ? cycle : awake;
     while (UART0_FR & UART_FR_TXFF);
     UART0_DR = ' ';
     for (k = 0; k < 8; k += 1) {
        while (UART0_FR & UART_FR_TXFF);
        UART0_DR = (UINT32)digits[(value >> (28 - 4 * k)) & 0xF];
     }
  }
  while (UART0_FR & UART_FR_TXFF);
  UART0_DR = '\n';
} /* end of Send */
