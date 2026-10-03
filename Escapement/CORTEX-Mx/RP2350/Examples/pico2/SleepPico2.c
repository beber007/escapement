/* Copyright (c) 2026 Bertrand Hurst. All rights reserved.
** Escapement - Lightweight Power-Aware Real-Time OS.
** Distributed under the terms of LICENSE at the root of this repository.
*/
/* File SleepPico2.c: The three sleeps of the RP2350, for the PPK2 to measure (docs/roadmap.md,
** item 5): no kernel, the clock set-up of the port alone, and the same load in each,
** some 100 us of work every 100 ms on core 0, core 1 held off by the loader.
**
**  - WFI: the core waits for TIMER0's alarm, every clock running, as the idle task does;
**  - SLEEP: the same with deep sleep and every top-level clock gated but the tick and
**    TIMER0, which keep the time (SLEEP_EN0 and SLEEP_EN1, RP2350 datasheet, 6.5.2);
**  - DORMANT: the system clock back on the crystal, PLL_SYS stopped and the crystal itself
**    put dormant (6.5.3), until a falling edge on GP1, then the crystal's start, its
**    STARTUP delay, and PLL_SYS locked again.
**
** The edge comes from outside: GP1 is the line the probe's UART drives, and the UNO Q
** sends it a byte 0x00 every 100 ms while it measures (tools/pico2_sleep_load.sh), the
** start bit and the eight zeros one low pulse. The always-on timer cannot wake this
** DORMANT: its count runs on LPOSC, but its alarm is not compared while the crystal is
** stopped, the power manager's own clock following clk_ref while the core is powered
** (POWMAN, DBG_POW_STATE_SWCORE, USING_FAST_POWCK). On the board on 2026-10-03 the alarm
** fired awake and never in DORMANT, where an edge on GP1 woke the chip each time; the
** pico-extras run clk_ref from LPOSC for that reason (pico_sleep, sleep.c). A kernel would
** do the same, or wake on an external 32.768 kHz clock (docs/roadmap.md, item 5).
**
** Each phase lasts PHASE_PERIODS periods, then the next, round and round: GP16 and GP17,
** the PPK2's D0 and D1, give the phase as a number, 0 WFI, 1 SLEEP, 2 DORMANT (tools/
** ppk2_nucleo.py). The ring oscillator, which the bootrom leaves on and nothing uses once
** the port runs on the crystal, is stopped first, and PLL_USB and the clocks of USB, the
** ADC and HSTX with it, for all three: their current would only add to each.
**
** The debugger reads nothing while the chip is in SLEEP or DORMANT: each change of phase
** sends a line on UART0, GP0, to the probe's UART at 115,200 baud, "SLEEP2" and the words
** 1 to 5 of Results in hexadecimal. The LED, GP25, stays off: on half the time it drew
** some 1.1 mA at 5 V in every phase, nearly DORMANT's whole mean (2026-10-03).
** Results, in words: 0 marker, 1 the phase, 2 to 5 the periods of each phase.
**
** Built with make DEEP=1, the three phases are the cheaper sleeps a kernel could take:
** 0 SLEEP as above, PLL_SYS running; 1 the same with clk_sys on the crystal and PLL_SYS
** stopped through each sleep, locked again on waking, TIMER0 still counting the
** crystal's microseconds; 2 DORMANT with the crystal's STARTUP delay at the millisecond
** the datasheet asks for, not the port's six (Escapement_Processor.c), to tell its
** wake-ups' part of DORMANT's mean.
**
** Built with make RUN=1, the core computes instead, without a pause, at four operating
** points of 30 s each: 150 MHz at 1.10 V, then clk_sys divided by 2 and by 4, 75 and
** 37.5 MHz, PLL_SYS still at 1.5 GHz, and the crystal's 12 MHz with PLL_SYS stopped,
** these three at RUN_VSEL, 1.10 V by default and 1.00 V with RUN_VSEL=9: the current of
** the core running, for DVFS (docs/roadmap.md, item 3). The datasheet guarantees 1.1 V
** only (6.3.2), and its brown-out detector resets the chip under some 0.95 V (6.6.2):
** below 1.10 V is for the bench alone, as UNDERVOLT is on the RP2040.
** Platform version: RP2350 (Raspberry Pi Pico 2).
*/

#include "Escapement.h"

#define MARKER            0x534C5032u   /* "SLP2" */
#define PERIOD_US         100000u
#ifndef PHASE_PERIODS
   #define PHASE_PERIODS  300u          /* 30 s */
#endif
#define WORK              4000u         /* loop turns, some 100 us at 150 MHz */
#define WAKE_PIN          1u            /* the probe's UART TX */
#define PHASE_PIN0        16u           /* PPK2 D0 */
#define PHASE_PIN1        17u           /* PPK2 D1 */
#define UART_TX_PIN       0u

#define RESETS_CLR        *((volatile UINT32 *)(0x40020000 + 0x3000))
#define RESETS_RESET_DONE *((volatile UINT32 *)0x40020008)
#define RESETS_IO_BANK0   (1u << 6)
#define RESETS_PADS_BANK0 (1u << 9)
#define RESETS_TIMER0     (1u << 23)
#define RESETS_UART0      (1u << 26)
#define RESETS_USED       (RESETS_IO_BANK0 | RESETS_PADS_BANK0 | RESETS_TIMER0 | RESETS_UART0)

/* IO_BANK0, PADS_BANK0 and the SIO, offsets of the pico-sdk's hardware/regs. */
#define IO_BANK0_CTRL(p)  *((volatile UINT32 *)(0x40028000 + 0x04 + 8 * (p)))
#define IO_BANK0_INTR0    *((volatile UINT32 *)(0x40028000 + 0x230))
#define IO_BANK0_DORMANT_WAKE_INTE0 *((volatile UINT32 *)(0x40028000 + 0x2D8))
#define GPIO_EDGE_LOW(p)  (1u << (4 * (p) + 2))   /* in INTR0 and DORMANT_WAKE_INTE0 */
#define PADS_BANK0_GPIO(p) *((volatile UINT32 *)(0x40038000 + 0x04 + 4 * (p)))
#define PADS_ISO_BIT      (1u << 8)
#define PADS_IE_BIT       (1u << 6)
#define FUNCSEL_UART      2u
#define FUNCSEL_SIO       5u
#define SIO_GPIO_OUT_SET  *((volatile UINT32 *)(0xD0000000 + 0x18))
#define SIO_GPIO_OUT_CLR  *((volatile UINT32 *)(0xD0000000 + 0x20))
#define SIO_GPIO_OE_SET   *((volatile UINT32 *)(0xD0000000 + 0x38))

/* UART0, a PL011 on clk_peri, the crystal's 12 MHz: 12e6 / (16 x 115,200) = 6 + 33/64. */
#define UART0_BASE        0x40070000
#define UART0_DR          *((volatile UINT32 *)(UART0_BASE + 0x00))
#define UART0_FR          *((volatile UINT32 *)(UART0_BASE + 0x18))
#define UART0_IBRD        *((volatile UINT32 *)(UART0_BASE + 0x24))
#define UART0_FBRD        *((volatile UINT32 *)(UART0_BASE + 0x28))
#define UART0_LCR_H       *((volatile UINT32 *)(UART0_BASE + 0x2C))
#define UART0_CR          *((volatile UINT32 *)(UART0_BASE + 0x30))
#define UART_FR_TXFF      (1u << 5)
#define UART_FR_BUSY      (1u << 3)

/* CLOCKS, offsets of the pico-sdk's hardware/regs/clocks.h. */
#define CLOCKS_BASE       0x40010000
#define CLK_SYS_CTRL      *((volatile UINT32 *)(CLOCKS_BASE + 0x3C))
#define CLK_SYS_SELECTED  *((volatile UINT32 *)(CLOCKS_BASE + 0x44))
#define CLK_HSTX_CTRL     *((volatile UINT32 *)(CLOCKS_BASE + 0x54))
#define CLK_USB_CTRL      *((volatile UINT32 *)(CLOCKS_BASE + 0x60))
#define CLK_ADC_CTRL      *((volatile UINT32 *)(CLOCKS_BASE + 0x6C))
#define CLOCKS_SLEEP_EN0  *((volatile UINT32 *)(CLOCKS_BASE + 0xB4))
#define CLOCKS_SLEEP_EN1  *((volatile UINT32 *)(CLOCKS_BASE + 0xB8))
#define CLK_ENABLE        (1u << 11)
#define CLK_SYS_SRC_AUX   1u
#define CLK_REF_TICKS     (1u << 17)    /* in SLEEP_EN1 */
#define CLK_SYS_TIMER0    (1u << 19)    /* in SLEEP_EN1 */

#define CLK_SYS_DIV       *((volatile UINT32 *)(CLOCKS_BASE + 0x40))
#define PLL_SYS_PWR       *((volatile UINT32 *)(0x40050000 + 0x04))
#define PLL_USB_PWR       *((volatile UINT32 *)(0x40058000 + 0x04))
#define XOSC_DORMANT      *((volatile UINT32 *)(0x40048000 + 0x08))
#define XOSC_STARTUP      *((volatile UINT32 *)(0x40048000 + 0x0C))
#define XOSC_STARTUP_1MS  47u           /* batches of 256 cycles of 12 MHz */
#define XOSC_DORMANT_WORD 0x636F6D61u   /* "coma" */
#define ROSC_CTRL         *((volatile UINT32 *)(0x400E8000 + 0x00))
#define ROSC_DISABLE      (0xD1Eu << 12)

/* TIMER0 and its tick, as Escapement_Timer.c has them; alarm 2, which the kernel's
** timer does not use. */
#define TIMER_BASE        0x400B0000
#define TIMER_ALARM2      *((volatile UINT32 *)(TIMER_BASE + 0x18))
#define TIMER_TIMERAWL    *((volatile UINT32 *)(TIMER_BASE + 0x28))
#define TIMER_DBGPAUSE    *((volatile UINT32 *)(TIMER_BASE + 0x2C))
#define TIMER_INTR        *((volatile UINT32 *)(TIMER_BASE + 0x3C))
#define TIMER_INTE_SET    *((volatile UINT32 *)(TIMER_BASE + 0x2000 + 0x40))
#define ALARM2_BIT        (1u << 2)
#define TIMER0_IRQ_2      2u
#define TICKS_TIMER0_CTRL *((volatile UINT32 *)(0x40108000 + 0x18))
#define TICKS_TIMER0_CYCLES *((volatile UINT32 *)(0x40108000 + 0x1C))

/* The core's regulator in POWMAN: VSEL in 50 mV steps from 0.55 V, 11 for 1.10 V. */
#define POWMAN_VREG_CTRL_SET *((volatile UINT32 *)(0x40100000 + 0x2000 + 0x04))
#define POWMAN_VREG       *((volatile UINT32 *)(0x40100000 + 0x0C))
#define POWMAN_KEY        0x5AFE0000u
#define VREG_UNLOCK       (1u << 13)
#define VREG_UPDATING     (1u << 15)
#define VREG_VSEL_MASK    (0x1Fu << 4)
#define VSEL_1_10         11u
#ifndef RUN_VSEL
   #define RUN_VSEL       VSEL_1_10
#endif
#define RUN_PHASE_US      30000000u

#define SCB_SCR           *((volatile UINT32 *)0xE000ED10)
#define SCB_SCR_SLEEPDEEP (1u << 2)
#define NVIC_ISER0        *((volatile UINT32 *)0xE000E100)
#define NVIC_ICPR0        *((volatile UINT32 *)0xE000E280)
#define VTOR              *((volatile UINT32 *)0xE000ED08)

#ifdef SLEEP_DEEP
   enum { PHASE_SLEEP, PHASE_SLEEP_XOSC, PHASE_DORMANT, PHASES };
   #define PHASE_WFI      PHASE_SLEEP
   #define GATED(phase)   ((phase) != PHASE_DORMANT)
#else
   enum { PHASE_WFI, PHASE_SLEEP, PHASE_DORMANT, PHASES };
   #define GATED(phase)   ((phase) == PHASE_SLEEP)
#endif
#define RUN_POINTS        4u

volatile struct {
  UINT32 Marker, Phase, Periods[RUN_POINTS];
} Results;

static void InitializePin(UINT32 pin);
static void SetPhase(UINT32 phase);
static void TimerSleep(UINT32 *due);
static void DormantSleep(void);
static void Report(void);
#ifdef SLEEP_RUN
   static void RunPoints(void);
#endif


int main(void)
{
  extern void (* const CortexMxVectorTable[])(void);
  UINT32 due, period;
  volatile UINT32 i;
  VTOR = (UINT32)CortexMxVectorTable;
  _OSDisableInterrupts();               // the WFIs wake on the NVIC's pending alarm alone
  OSInitializeSystemClocks();
  /* What the three phases leave running but do not use (above). */
  ROSC_CTRL = ROSC_DISABLE;
  CLK_USB_CTRL &= ~CLK_ENABLE;
  CLK_ADC_CTRL &= ~CLK_ENABLE;
  CLK_HSTX_CTRL &= ~CLK_ENABLE;
  PLL_USB_PWR = 0xFFFFFFFFu;
  RESETS_CLR = RESETS_USED;
  while ((RESETS_RESET_DONE & RESETS_USED) != RESETS_USED);
  InitializePin(PHASE_PIN0);
  InitializePin(PHASE_PIN1);
  UART0_IBRD = 6;
  UART0_FBRD = 33;
  UART0_LCR_H = 0x70;                   // 8 bits, FIFO on; written after the divisors
  UART0_CR = 0x101;                     // UARTEN, TXE
  IO_BANK0_CTRL(UART_TX_PIN) = FUNCSEL_UART;
  PADS_BANK0_GPIO(UART_TX_PIN) &= ~PADS_ISO_BIT;
  /* GP1 an input whose falling edge wakes the chip from DORMANT, and only from it. */
  IO_BANK0_CTRL(WAKE_PIN) = FUNCSEL_SIO;
  PADS_BANK0_GPIO(WAKE_PIN) = (PADS_BANK0_GPIO(WAKE_PIN) & ~PADS_ISO_BIT) | PADS_IE_BIT;
  IO_BANK0_DORMANT_WAKE_INTE0 = GPIO_EDGE_LOW(WAKE_PIN);
  /* TIMER0 counts microseconds of the crystal, and on while the debugger holds a core. */
  TICKS_TIMER0_CTRL = 0;
  TICKS_TIMER0_CYCLES = 12;
  TICKS_TIMER0_CTRL = 1;
  TIMER_DBGPAUSE = 0;
  TIMER_INTR = ALARM2_BIT;
  TIMER_INTE_SET = ALARM2_BIT;
  NVIC_ISER0 = 1u << TIMER0_IRQ_2;
  Results.Marker = MARKER;
  #ifdef SLEEP_RUN
     RunPoints();
  #endif
  SetPhase(PHASE_WFI);
  due = TIMER_TIMERAWL + PERIOD_US;
  for (period = 0; ; period += 1) {
     if (period == PHASE_PERIODS) {
        period = 0;
        Report();
        SetPhase((Results.Phase + 1) % PHASES);
        due = TIMER_TIMERAWL + PERIOD_US;
     }
     for (i = 0; i < WORK; i += 1);
     Results.Periods[Results.Phase] += 1;
     if (Results.Phase == PHASE_DORMANT)
        DormantSleep();
     #ifdef SLEEP_DEEP
     else if (Results.Phase == PHASE_SLEEP_XOSC) {
        CLK_SYS_CTRL &= ~CLK_SYS_SRC_AUX;
        while ((CLK_SYS_SELECTED & 1u) == 0);
        PLL_SYS_PWR = 0xFFFFFFFFu;
        TimerSleep(&due);
        OSInitializeSystemClocks();
     }
     #endif
     else
        TimerSleep(&due);
  }
} /* end of main */


/* InitializePin: An output of the SIO, low. */
static void InitializePin(UINT32 pin)
{
  SIO_GPIO_OUT_CLR = 1u << pin;
  SIO_GPIO_OE_SET = 1u << pin;
  IO_BANK0_CTRL(pin) = FUNCSEL_SIO;
  PADS_BANK0_GPIO(pin) &= ~PADS_ISO_BIT;
} /* end of InitializePin */


/* SetPhase: The phase on the two pins, and the clock gates and deep sleep of SLEEP for
** that phase only. */
static void SetPhase(UINT32 phase)
{
  Results.Phase = phase;
  if (phase & 1u)
     SIO_GPIO_OUT_SET = 1u << PHASE_PIN0;
  else
     SIO_GPIO_OUT_CLR = 1u << PHASE_PIN0;
  if (phase & 2u)
     SIO_GPIO_OUT_SET = 1u << PHASE_PIN1;
  else
     SIO_GPIO_OUT_CLR = 1u << PHASE_PIN1;
  if (GATED(phase)) {
     CLOCKS_SLEEP_EN0 = 0;
     CLOCKS_SLEEP_EN1 = CLK_REF_TICKS | CLK_SYS_TIMER0;
     SCB_SCR |= SCB_SCR_SLEEPDEEP;
  }
  else {
     CLOCKS_SLEEP_EN0 = 0xFFFFFFFFu;
     CLOCKS_SLEEP_EN1 = 0xFFFFFFFFu;
     SCB_SCR &= ~SCB_SCR_SLEEPDEEP;
  }
} /* end of SetPhase */


/* TimerSleep: WFI until TIMER0 reaches due, which then moves one period on. Interrupts
** are masked: the pending alarm ends the WFI, and is cleared here. */
static void TimerSleep(UINT32 *due)
{
  TIMER_ALARM2 = *due;
  __asm volatile ("DSB\n\tWFI\n\tISB" ::: "memory");
  while ((TIMER_INTR & ALARM2_BIT) == 0);   // an earlier wake-up, none expected
  TIMER_INTR = ALARM2_BIT;
  NVIC_ICPR0 = 1u << TIMER0_IRQ_2;
  *due += PERIOD_US;
} /* end of TimerSleep */


/* DormantSleep: DORMANT until the next falling edge on GP1. The system clock goes to
** clk_ref, the crystal, PLL_SYS is stopped (6.5.3, "DORMANT does not halt PLLs"), the
** edge flags cleared, and the crystal put dormant, which stops the core. The edge
** restarts the crystal; OSInitializeSystemClocks then locks PLL_SYS again, as at start. */
static void DormantSleep(void)
{
  CLK_SYS_CTRL &= ~CLK_SYS_SRC_AUX;
  while ((CLK_SYS_SELECTED & 1u) == 0);
  PLL_SYS_PWR = 0xFFFFFFFFu;
  IO_BANK0_INTR0 = GPIO_EDGE_LOW(WAKE_PIN);
  #ifdef SLEEP_DEEP
     XOSC_STARTUP = XOSC_STARTUP_1MS;   // OSInitializeSystemClocks sets the port's back
  #endif
  XOSC_DORMANT = XOSC_DORMANT_WORD;
  __asm volatile ("DSB\n\tISB" ::: "memory");
  IO_BANK0_INTR0 = GPIO_EDGE_LOW(WAKE_PIN);
  OSInitializeSystemClocks();
} /* end of DormantSleep */


#ifdef SLEEP_RUN
/* SetVoltage: The core's regulator at vsel, once any change before has ended. */
static void SetVoltage(UINT32 vsel)
{
  POWMAN_VREG_CTRL_SET = POWMAN_KEY | VREG_UNLOCK;
  while (POWMAN_VREG & VREG_UPDATING);
  POWMAN_VREG = POWMAN_KEY | (POWMAN_VREG & ~VREG_VSEL_MASK & 0xFFFFu) | (vsel << 4);
  while (POWMAN_VREG & VREG_UPDATING);
} /* end of SetVoltage */


/* RunPoints: The four operating points in turn, for good, the core computing on each for
** RUN_PHASE_US of TIMER0, which counts the crystal's microseconds whatever clk_sys is.
** The voltage rises before the clock and falls after it. */
static void RunPoints(void)
{
  static const UINT32 divisor[RUN_POINTS] = { 1, 2, 4, 0 };   // 0: the crystal itself
  UINT32 point = 0, start;
  volatile UINT32 i;
  while (TRUE) {
     Results.Phase = point;
     if (point & 1u)
        SIO_GPIO_OUT_SET = 1u << PHASE_PIN0;
     else
        SIO_GPIO_OUT_CLR = 1u << PHASE_PIN0;
     if (point & 2u)
        SIO_GPIO_OUT_SET = 1u << PHASE_PIN1;
     else
        SIO_GPIO_OUT_CLR = 1u << PHASE_PIN1;
     start = TIMER_TIMERAWL;
     while (TIMER_TIMERAWL - start < RUN_PHASE_US) {
        for (i = 0; i < 1000; i += 1);
        Results.Periods[point] += 1;
     }
     Report();
     point = (point + 1) % RUN_POINTS;
     if (point == 0) {                  // back to 150 MHz at 1.10 V from the crystal
        SetVoltage(VSEL_1_10);
        OSInitializeSystemClocks();
     }
     else if (divisor[point] != 0) {
        CLK_SYS_DIV = divisor[point] << 16;
        SetVoltage(RUN_VSEL);
     }
     else {
        CLK_SYS_CTRL &= ~CLK_SYS_SRC_AUX;
        while ((CLK_SYS_SELECTED & 1u) == 0);
        PLL_SYS_PWR = 0xFFFFFFFFu;
        CLK_SYS_DIV = 1u << 16;
     }
  }
} /* end of RunPoints */
#endif


/* Report: "SLEEP2" and the words 1 to 5 of Results in hexadecimal, sent before the
** phase changes and left to drain: SLEEP gates the UART's clocks, DORMANT every clock. */
static void Report(void)
{
  static const char digits[] = "0123456789ABCDEF";
  const char *text = "SLEEP2";
  UINT32 w, k, value;
  while (*text != '\0') {
     while (UART0_FR & UART_FR_TXFF);
     UART0_DR = (UINT32)*text++;
  }
  for (w = 1; w <= 5; w += 1) {
     value = ((volatile UINT32 *)&Results)[w];
     while (UART0_FR & UART_FR_TXFF);
     UART0_DR = ' ';
     for (k = 0; k < 8; k += 1) {
        while (UART0_FR & UART_FR_TXFF);
        UART0_DR = (UINT32)digits[(value >> (28 - 4 * k)) & 0xF];
     }
  }
  while (UART0_FR & UART_FR_TXFF);
  UART0_DR = '\n';
  while (UART0_FR & UART_FR_BUSY);
} /* end of Report */
