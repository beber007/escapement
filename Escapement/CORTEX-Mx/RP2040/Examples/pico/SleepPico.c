/* Copyright (c) 2026 Bertrand Hurst. All rights reserved.
** Escapement - Lightweight Power-Aware Real-Time OS.
** Distributed under the terms of LICENSE at the root of this repository.
*/
/* File SleepPico.c: The sleeps and the operating points of the RP2040, for the PPK2 to
** measure on the 3V3 rail of a plain Pico (docs/roadmap.md, task 9; docs/power-aware.md,
** "Measuring the RP2040"): no kernel, the port's clock set-up and its DVFS driver alone,
** built with make KERNEL=PA sleep. As SleepPico2.c on the RP2350, the same load in each
** phase, some 100 us of work at 125 MHz every 100 ms, on core 0; core 1 sleeps for good,
** in deep sleep, since SLEEP needs both cores asleep (RP2040 datasheet, 2.11.2).
**
**  - 0, WFI at 125 MHz: the idle task as it is;
**  - 1, WFI at 12 MHz, PLL_SYS still locked, the work back at 125 MHz on waking: the
**    power-aware kernel's idle task built with SLEEP_SPEED=0;
**  - 2, SLEEP at 125 MHz, every clock gated but the timer's (SLEEP_EN0 and SLEEP_EN1,
**    2.15.7), as make SLEEP_GATE=1 has it;
**  - 3, SLEEP with clk_sys on the crystal and PLL_SYS powered down, locked again on
**    waking, its lock timed on the 1 us counter: whether a long sleep pays for stopping
**    it, the datasheet giving no lock time (2.18).
**
** Each phase lasts PHASE_PERIODS periods, then the next, round and round: GP16 and GP17,
** the PPK2's D0 and D1, give the phase as a number (tools/ppk2_nucleo.py, PPK2_PHASES).
** The ring oscillator, which the bootrom leaves on and nothing uses once the port runs on
** the crystal, is stopped first, and PLL_USB and the clocks of USB, the ADC and the RTC
** with it: their current would only add to each phase.
**
** The debugger reads nothing while the chip is in SLEEP: each change of phase sends a
** line on UART0, GP0, to the probe's UART at 115,200 baud, "SLEEP1" and the words 1 to 8
** of Results in hexadecimal; clk_peri stays on the crystal (Escapement_Processor.c), and
** the line goes out before the gates of the next phase. The LED, GP25, stays off.
** Results, in words: 0 marker, 1 the phase or the point, 2 to 5 the periods of each phase
** or the work of each point, 6 the longest lock of PLL_SYS in us, 7 the sum of the locks,
** 8 the wrong results of the computation (below).
**
** Built with make KERNEL=PA RUN=1, the core computes instead, without a pause, 30 s at
** each operating point of the port in turn, 125, 50 and 12 MHz, at the voltages of its
** DVFS driver: 1.10, 1.05 and 1.05 V, or with UNDERVOLT=1 1.10, 0.95 and 0.90 V, below the
** 1.05 V the datasheet guarantees (2.10). The work is a CRC-32 of a buffer, checked each
** time against the one computed first at 125 MHz and 1.10 V: a core undervolted too far
** may compute wrong without a fault, which word 8 counts (docs/power-aware.md).
** Platform version: RP2040 (Raspberry Pi Pico).
*/

#include "Escapement.h"
#include "Escapement_Core1.h"

#define MARKER            0x534C5031u   /* "SLP1" */
#define PERIOD_US         100000u
#ifndef PHASE_PERIODS
   #define PHASE_PERIODS  300u          /* 30 s */
#endif
#define WORK              2500u         /* loop turns, some 100 us at 125 MHz */
#define PHASE_PIN0        16u           /* PPK2 D0 */
#define PHASE_PIN1        17u           /* PPK2 D1 */
#define UART_TX_PIN       0u

#define RESETS_CLR        *((volatile UINT32 *)(0x4000C000 + 0x3000))
#define RESETS_DONE       *((volatile UINT32 *)(0x4000C000 + 0x08))
#define RESETS_IO_BANK0   (1u << 5)
#define RESETS_PADS_BANK0 (1u << 8)
#define RESETS_TIMER      (1u << 21)
#define RESETS_UART0      (1u << 22)
#define RESETS_USED       (RESETS_IO_BANK0 | RESETS_PADS_BANK0 | RESETS_TIMER | RESETS_UART0)

#define IO_BANK0_CTRL(p)  *((volatile UINT32 *)(0x40014000 + 0x04 + 8 * (p)))
#define FUNCSEL_UART      2u
#define FUNCSEL_SIO       5u
#define SIO_GPIO_OUT_SET  *((volatile UINT32 *)(0xD0000000 + 0x14))
#define SIO_GPIO_OUT_CLR  *((volatile UINT32 *)(0xD0000000 + 0x18))
#define SIO_GPIO_OE_SET   *((volatile UINT32 *)(0xD0000000 + 0x24))

/* UART0, a PL011 on clk_peri, the crystal's 12 MHz: 12e6 / (16 x 115,200) = 6 + 33/64. */
#define UART0_BASE        0x40034000
#define UART0_DR          *((volatile UINT32 *)(UART0_BASE + 0x00))
#define UART0_FR          *((volatile UINT32 *)(UART0_BASE + 0x18))
#define UART0_IBRD        *((volatile UINT32 *)(UART0_BASE + 0x24))
#define UART0_FBRD        *((volatile UINT32 *)(UART0_BASE + 0x28))
#define UART0_LCR_H       *((volatile UINT32 *)(UART0_BASE + 0x2C))
#define UART0_CR          *((volatile UINT32 *)(UART0_BASE + 0x30))
#define UART_FR_TXFF      (1u << 5)
#define UART_FR_BUSY      (1u << 3)

/* CLOCKS, offsets of the pico-sdk's hardware/regs/clocks.h for the RP2040. */
#define CLOCKS_BASE       0x40008000
#define CLK_SYS_CTRL      *((volatile UINT32 *)(CLOCKS_BASE + 0x3C))
#define CLK_USB_CTRL      *((volatile UINT32 *)(CLOCKS_BASE + 0x54))
#define CLK_ADC_CTRL      *((volatile UINT32 *)(CLOCKS_BASE + 0x60))
#define CLK_RTC_CTRL      *((volatile UINT32 *)(CLOCKS_BASE + 0x6C))
#define CLOCKS_SLEEP_EN0  *((volatile UINT32 *)(CLOCKS_BASE + 0xA8))
#define CLOCKS_SLEEP_EN1  *((volatile UINT32 *)(CLOCKS_BASE + 0xAC))
#define CLK_ENABLE        (1u << 11)
#define CLK_SYS_TIMER     (1u << 5)     /* in SLEEP_EN1 */
#define CLK_SYS_AUXSRC_XOSC (3u << 5)

#define PLL_SYS_CS        *((volatile UINT32 *)(0x40028000 + 0x00))
#define PLL_SYS_PWR       *((volatile UINT32 *)(0x40028000 + 0x04))
#define PLL_USB_PWR       *((volatile UINT32 *)(0x4002C000 + 0x04))
#define PLL_CS_LOCK       (1u << 31)
#define PLL_PWR_PD        (1u << 0)
#define PLL_PWR_DSMPD     (1u << 2)     /* kept at 1, the PLL in integer mode, as the port */
#define PLL_PWR_POSTDIVPD (1u << 3)
#define PLL_PWR_VCOPD     (1u << 5)
#define ROSC_CTRL         *((volatile UINT32 *)(0x40060000 + 0x00))
#define ROSC_DISABLE      (0xD1Eu << 12)

/* The timer and its tick, as Escapement_Timer.c has them; alarm 2, which the kernel's
** timer does not use. */
#define TIMER_BASE        0x40054000
#define TIMER_ALARM2      *((volatile UINT32 *)(TIMER_BASE + 0x18))
#define TIMER_TIMERAWL    *((volatile UINT32 *)(TIMER_BASE + 0x28))
#define TIMER_DBGPAUSE    *((volatile UINT32 *)(TIMER_BASE + 0x2C))
#define TIMER_INTR        *((volatile UINT32 *)(TIMER_BASE + 0x34))
#define TIMER_INTE        *((volatile UINT32 *)(TIMER_BASE + 0x38))
#define ALARM2_BIT        (1u << 2)
#define TIMER_IRQ_2       2u
#define WATCHDOG_TICK     *((volatile UINT32 *)(0x40058000 + 0x2C))
#define WATCHDOG_TICK_ENABLE (1u << 9)
#define WATCHDOG_CTRL_CLR *((volatile UINT32 *)(0x40058000 + 0x3000))
#define WATCHDOG_ENABLE   (1u << 30)
#define RUN_PHASE_US      30000000u

#define SCB_SCR           *((volatile UINT32 *)0xE000ED10)
#define SCB_SCR_SLEEPDEEP (1u << 2)
#define NVIC_ISER         *((volatile UINT32 *)0xE000E100)
#define NVIC_ICPR         *((volatile UINT32 *)0xE000E280)
#define VTOR              *((volatile UINT32 *)0xE000ED08)

enum { PHASE_WFI, PHASE_WFI_12MHZ, PHASE_SLEEP, PHASE_SLEEP_PLL_OFF, PHASES };
#define RUN_POINTS        3u

volatile struct {
  UINT32 Marker, Phase, Periods[4], LockMax, LockSum, Wrong;
} Results;

static UINT32 Core1Stack[16] __attribute__((aligned(8)));
static void Core1Sleep(void);
static void InitializePin(UINT32 pin);
static void SetPhase(UINT32 phase);
static void TimerSleep(UINT32 *due);
static void StopPll(void);
static void StartPll(void);
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
  OSInitProcessorSpeed();
  /* A firmware in flash may have armed the watchdog, which pauses only while the debugger
  ** holds a core: once core 1 runs, it rebooted the chip (Escapement_SleepGate.c). */
  WATCHDOG_CTRL_CLR = WATCHDOG_ENABLE;
  OSLaunchCore1(Core1Sleep,&Core1Stack[16]);
  /* What the four phases leave running but do not use (above). */
  ROSC_CTRL = ROSC_DISABLE;
  CLK_USB_CTRL &= ~CLK_ENABLE;
  CLK_ADC_CTRL &= ~CLK_ENABLE;
  CLK_RTC_CTRL &= ~CLK_ENABLE;
  PLL_USB_PWR = 0xFFFFFFFFu;
  RESETS_CLR = RESETS_USED;
  while ((RESETS_DONE & RESETS_USED) != RESETS_USED);
  InitializePin(PHASE_PIN0);
  InitializePin(PHASE_PIN1);
  UART0_IBRD = 6;
  UART0_FBRD = 33;
  UART0_LCR_H = 0x70;                   // 8 bits, FIFO on; written after the divisors
  UART0_CR = 0x101;                     // UARTEN, TXE
  IO_BANK0_CTRL(UART_TX_PIN) = FUNCSEL_UART;
  /* The timer counts microseconds of the crystal, and on while the debugger holds a core. */
  WATCHDOG_TICK = WATCHDOG_TICK_ENABLE | 12;
  TIMER_DBGPAUSE = 0;
  TIMER_INTR = ALARM2_BIT;
  TIMER_INTE |= ALARM2_BIT;
  NVIC_ISER = 1u << TIMER_IRQ_2;
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
     if (Results.Phase == PHASE_WFI_12MHZ) {
        OSSetProcessorSpeed(OS_12MHZ_SPEED);
        TimerSleep(&due);
        OSSetProcessorSpeed(OS_125MHZ_SPEED);
     }
     else if (Results.Phase == PHASE_SLEEP_PLL_OFF) {
        StopPll();
        TimerSleep(&due);
        StartPll();
     }
     else
        TimerSleep(&due);
  }
} /* end of main */


/* Core1Sleep: Core 1 in deep sleep for good, its SCR being its own; no interrupt is
** enabled on it, and an event only brings it round the loop. */
static void Core1Sleep(void)
{
  SCB_SCR |= SCB_SCR_SLEEPDEEP;
  while (TRUE)
     __asm volatile ("WFE" ::: "memory");
} /* end of Core1Sleep */


/* InitializePin: An output of the SIO, low. */
static void InitializePin(UINT32 pin)
{
  SIO_GPIO_OUT_CLR = 1u << pin;
  SIO_GPIO_OE_SET = 1u << pin;
  IO_BANK0_CTRL(pin) = FUNCSEL_SIO;
} /* end of InitializePin */


/* SetPhase: The phase on the two pins, and the clock gates and deep sleep of SLEEP for
** the phases that sleep so. */
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
  if (phase == PHASE_SLEEP || phase == PHASE_SLEEP_PLL_OFF) {
     CLOCKS_SLEEP_EN0 = 0;
     CLOCKS_SLEEP_EN1 = CLK_SYS_TIMER;
     SCB_SCR |= SCB_SCR_SLEEPDEEP;
  }
  else {
     CLOCKS_SLEEP_EN0 = 0xFFFFFFFFu;
     CLOCKS_SLEEP_EN1 = 0x7FFFu;
     SCB_SCR &= ~SCB_SCR_SLEEPDEEP;
  }
} /* end of SetPhase */


/* TimerSleep: WFI until the timer reaches due, which then moves one period on. Interrupts
** are masked: the pending alarm ends the WFI, and is cleared here. */
static void TimerSleep(UINT32 *due)
{
  TIMER_ALARM2 = *due;
  __asm volatile ("DSB\n\tWFI\n\tISB" ::: "memory");
  while ((TIMER_INTR & ALARM2_BIT) == 0);   // an earlier wake-up, none expected
  TIMER_INTR = ALARM2_BIT;
  NVIC_ICPR = 1u << TIMER_IRQ_2;
  *due += PERIOD_US;
} /* end of TimerSleep */


/* StopPll: clk_sys parked on clk_ref, the crystal, by the DVFS driver's 12 MHz point, its
** auxiliary source moved from PLL_SYS to the crystal, then PLL_SYS powered down, its
** dividers left as they are. With the auxiliary source still PLL_SYS, the chip took a
** HardFault on waking from SLEEP, the core's stacked PC just past its WFI, though clk_sys
** ran from clk_ref; awake, or in SLEEP with PLL_SYS running, it did not (the board,
** 2026-10-10). The DVFS driver's 125 MHz point takes PLL_SYS back as the source. */
static void StopPll(void)
{
  OSSetProcessorSpeed(OS_12MHZ_SPEED);
  CLK_SYS_CTRL = CLK_SYS_AUXSRC_XOSC;   // the reference still the source, glitchless
  PLL_SYS_PWR = PLL_PWR_PD | PLL_PWR_DSMPD | PLL_PWR_POSTDIVPD | PLL_PWR_VCOPD;
} /* end of StopPll */


/* StartPll: PLL_SYS powered up and locked again, the lock timed, then 125 MHz. */
static void StartPll(void)
{
  UINT32 start = TIMER_TIMERAWL, lock;
  PLL_SYS_PWR = PLL_PWR_DSMPD | PLL_PWR_POSTDIVPD;   // the VCO first, the post divider
  while ((PLL_SYS_CS & PLL_CS_LOCK) == 0);           // once locked
  lock = TIMER_TIMERAWL - start;
  PLL_SYS_PWR = PLL_PWR_DSMPD;
  Results.LockSum += lock;
  if (lock > Results.LockMax)
     Results.LockMax = lock;
  OSSetProcessorSpeed(OS_125MHZ_SPEED);
} /* end of StartPll */


#ifdef SLEEP_RUN
/* Crc32: The CRC-32 of the buffer, bit by bit, a computation whose result says whether
** the core computed it right. */
static UINT32 Crc32(const UINT8 *data, UINT32 length)
{
  UINT32 crc = 0xFFFFFFFFu, k;
  while (length-- > 0) {
     crc ^= *data++;
     for (k = 0; k < 8; k += 1)
        crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
  }
  return ~crc;
} /* end of Crc32 */


/* RunPoints: The three operating points in turn, for good, the core computing on each for
** RUN_PHASE_US of the timer, which counts the crystal's microseconds whatever clk_sys is;
** the DVFS driver raises the voltage before the clock and lowers it after. */
static void RunPoints(void)
{
  static const UINT8 speed[RUN_POINTS] = {OS_125MHZ_SPEED, OS_50MHZ_SPEED, OS_12MHZ_SPEED};
  static UINT8 buffer[256];
  UINT32 point = 0, start, expected, k;
  for (k = 0; k < sizeof buffer; k += 1)
     buffer[k] = (UINT8)(k * 37u + 11u);
  expected = Crc32(buffer,sizeof buffer);
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
     OSSetProcessorSpeed(speed[point]);
     start = TIMER_TIMERAWL;
     while (TIMER_TIMERAWL - start < RUN_PHASE_US) {
        if (Crc32(buffer,sizeof buffer) != expected)
           Results.Wrong += 1;
        Results.Periods[point] += 1;
     }
     Report();
     point = (point + 1) % RUN_POINTS;
  }
} /* end of RunPoints */
#endif


/* Report: "SLEEP1" and the words 1 to 8 of Results in hexadecimal, sent before the phase
** changes and left to drain: SLEEP gates the UART's clocks. */
static void Report(void)
{
  static const char digits[] = "0123456789ABCDEF";
  const char *text = "SLEEP1";
  UINT32 w, k, value;
  while (*text != '\0') {
     while (UART0_FR & UART_FR_TXFF);
     UART0_DR = (UINT32)*text++;
  }
  for (w = 1; w <= 8; w += 1) {
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
