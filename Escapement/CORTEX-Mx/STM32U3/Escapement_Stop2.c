/* Copyright (c) 2026 Bertrand Hurst. All rights reserved.
** Escapement - Lightweight Power-Aware Real-Time OS.
** Distributed under the terms of LICENSE at the root of this repository.
*/
/* File Escapement_Stop2.c: The idle task in Stop 2 (Escapement_Stop2.h), the logic of the
** STM32U5 port on the STM32U385. RM0487 rev. 3, chapters 9 (PWR), 10 (RCC) and 42
** (LPTIM); ES0626 rev. 3 for the errata; addresses and bits checked against
** STMicroelectronics, cmsis-device-u3, stm32u385xx.h. Written before the board came:
** only Renode has run it (docs/stm32u3.md).
**
** TIM2 stays the kernel's clock while the chip runs. The idle task, finding the next
** event at least OS_STOP2_MIN_US off, the compare of TIM2 for the next arrival, its wrap
** at 2^30, or the next timer event of TIM4, arms the compare of LPTIM1 OS_STOP2_WAKE_US
** before it and enters Stop 2 (PWR_CR1.LPMS = 010, SLEEPDEEP, WFI: table 91, p. 351),
** where the MSI, TIM2 and TIM4 stop and LPTIM1 counts on. Both timers are stopped by
** their counter enable before, as RM0487 asks of every peripheral that does not run in
** Stop 2 (caution, p. 351), and USART1 is disabled (Escapement_UART.c), which the U5 port
** never did; LPTIM1 and LPUART1, autonomous in Stop 2 (9.3.5, p. 344), run on.
**
** The chip wakes on the MSIS, STOPWUCK and STOPKERWUCK being 0 (RCC_CFGR1, p. 426), in
** voltage range 2 (p. 351): at the frequency it had, or at 48 MHz if it had more, the
** hardware writing MSISDIV so (10.3, p. 415; RCC_ICSCR1, p. 421). The booster, on at
** 96 and 48 MHz with the MSIS as its clock, stays on through Stop 2, as a wake-up at
** 48 MHz requires (10.2.3, p. 405). The MSI's PLL mode lost its lock with the MSI and
** locks again (MSIPLL0RDY, p. 418): _OSWaitMSILock waits for it, at most
** OS_STOP2_LOCK_TICKS of LPTIM1, unless MSIPLL0FAST kept it (make FAST=1); the rest of
** the margin is slept in Sleep on that clock (SlowSleep). Then _OSRaiseSystemClock reads PWR_VOSR, RCC_ICSCR1 and RCC_CFGR4 and replays the raise of
** OSInitializeSystemClocks from there, range 1 and its ready flag before 96 MHz
** (Escapement_Processor.c). TIM2 and TIM4 are moved on by the time LPTIM1 counted
** (Escapement_TimerEvent.c). Both are stopped and started again on an edge of LPTIM1, so
** that the time between is whole ticks, 15625/512 us each, the fraction carried to the
** next sleep. Interrupts stay masked from the reading of TIM2 to its start again: one that
** comes meanwhile wakes the chip and is taken after, late by the wake-up.
**
** Stop 2 is entered only with no interrupt pending, and with every flag that may wake
** the chip clear, or the entry is ignored and the WFI falls through (table 91, p. 351):
** the compare flag of LPTIM1 is cleared before, and PWR_SR.STOPF, cleared by CSSF
** (p. 375), tells a Stop 2 from a WFI that fell through.
**
** A byte received on LPUART1, or a wake-up from Stop 2 that LPTIM1 did not cause, keeps
** the idle task in Sleep for OS_STOP2_LINK_WINDOW_US after it, so that the bytes that
** follow are sampled on a running HSI16: the first byte of a silence only wakes the chip
** (docs/roadmap.md, item 6). The window is counted on LPTIM1, whose compare wakes the chip
** at its end if nothing else does, for Stop 2 to resume then.
**
** LPTIM1 wakes the chip from Stop 2 with its clock enabled in Run, in Sleep and in Stop
** (LPTIM1EN, LPTIM1SLPEN, LPTIM1STPEN: table 91, p. 352; RCC_APB3SLPENR and
** RCC_APB3STPENR, p. 458 and 465, both all ones at reset), and its interrupt enabled in
** the NVIC (p. 352): enabled only across the WFI, since it has no handler. DBG_STOP and
** DBG_STANDBY are cleared, which a debugger may have set and a system reset leaves
** (DBGMCU_CR, p. 2848-2849): on the U5, Debian's OpenOCD set both at each connection, and
** a NUCLEO with DBG_STANDBY alone set never woke from Stop 2 (docs/stm32u5.md). No SRAM is
** powered down (PWR_CR2 left at 0), which keeps the chip clear of errata 2.2.7 to 2.2.12
** and 2.2.17 (ES0626). ICACHE is left on: RM0487 asks nothing of it before Stop (8.5,
** p. 319), and ES0626 has nothing like the U5's erratum 2.2.11.
** Platform version: STM32U385 (NUCLEO-U385RG-Q), any STM32U375/385.
*/

#include "Escapement.h"
#include "Escapement_LPTimer.h"
#include "Escapement_Stop2.h"

#define TIM2_BASE            0x40000000
#define TIM_CR1              *((volatile UINT32 *)(TIM2_BASE + 0x00))
#define TIM_CNT              *((volatile UINT32 *)(TIM2_BASE + 0x24))
#define TIM_CCR1             *((volatile UINT32 *)(TIM2_BASE + 0x34))
#define TIM_CR1_CEN          (1u << 0)
#define TIMER_WRAP           0x40000000u   /* TIM2 wraps at 2^30 (Escapement_Timer.c) */

#define PWR_BASE             0x40030800
#define PWR_CR1              *((volatile UINT32 *)(PWR_BASE + 0x00))   /* p. 363-365 */
#define PWR_SR               *((volatile UINT32 *)(PWR_BASE + 0x38))   /* p. 375-376 */
#define PWR_CR1_LPMS_MASK    7u
#define PWR_CR1_LPMS_STOP2   2u
#define PWR_SR_CSSF          (1u << 0)
#define PWR_SR_STOPF         (1u << 1)

#define RCC_BASE             0x40030C00
#define RCC_CR               *((volatile UINT32 *)(RCC_BASE + 0x000))
#define RCC_CR_HSION         (1u << 11)    /* RCC_CR, p. 418 */
#define RCC_CFGR1            *((volatile UINT32 *)(RCC_BASE + 0x01C))
#define RCC_CFGR1_STOPWUCK   (1u << 4)     /* RCC_CFGR1, p. 426 */
#define RCC_CFGR1_STOPKERWUCK (1u << 5)
#define RCC_APB3SLPENR       *((volatile UINT32 *)(RCC_BASE + 0x0D0))
#define RCC_APB3STPENR       *((volatile UINT32 *)(RCC_BASE + 0x0F8))
#define RCC_LPTIM1           (1u << 11)    /* in APB3SLPENR and APB3STPENR */

#define SCB_SCR              *((volatile UINT32 *)0xE000ED10)
#define SCB_SCR_SLEEPDEEP    (1u << 2)
#define DBGMCU_CR            *((volatile UINT32 *)0xE0044004)
#define DBGMCU_CR_DBG_STOP   (1u << 1)
#define DBGMCU_CR_DBG_STANDBY (1u << 2)

#define NVIC_ISER(irq)       ((volatile UINT32 *)0xE000E100)[(irq) >> 5]
#define NVIC_ICER(irq)       ((volatile UINT32 *)0xE000E180)[(irq) >> 5]
#define NVIC_ICPR(irq)       ((volatile UINT32 *)0xE000E280)[(irq) >> 5]
#define NVIC_BIT(irq)        (1u << ((irq) & 0x1F))

/* The longest sleep, in ticks, short of the 2^16 of the counter of LPTIM1: 1.83 s. */
#define MAX_TICKS            60000u

/* The window in ticks of LPTIM1, under half its counter to tell a window open from one
** long closed; and the ticks it must still have to be armed. The compare reaches the clock
** domain of LPTIM1 some time after its write, which RM0487 does not bound (42.4.11,
** p. 1908), and one that arrives past the count never matches: a window nearer its end is
** taken as closed, and its end is read again once the compare is written (WindowSleep). */
#define WINDOW_TICKS         ((UINT32)OS_STOP2_LINK_WINDOW_US * 512u / 15625u)
#define WINDOW_MIN_TICKS     4u
#if OS_STOP2_LINK_WINDOW_US > 900000
   #error "OS_STOP2_LINK_WINDOW_US: LPTIM1 counts 2 s at most"
#endif


static OS_STOP2_COUNTS Counts;
static volatile BOOL Allowed = TRUE;
static volatile UINT32 WakeUs = OS_STOP2_WAKE_US;  // how early LPTIM1 wakes the chip
static UINT32 Fraction;                    // of a microsecond, in 512ths, carried over
/* TIM2 stops a few instructions after the tick that starts the sleep, and starts again some
** 30 cycles after the one that ends it, those of the code between: as many cycles lost per
** wake-up, given back in 512ths of a microsecond. The 30 were measured on the U5 at 160 MHz
** (docs/stm32u5.md); this chip's are the board's to measure. */
#define RESTART_CYCLES       30u
#define RESTART_FRACTION     (RESTART_CYCLES * 512u / (OS_SYSTEM_CLOCK_HZ / 1000000u))
static BOOL WindowOpen;                    // the window of LPUART1, and its end on LPTIM1
static UINT16 WindowEnd;
static BOOL WindowArmed;                   // the compare of LPTIM1 set at WindowEnd

static void Stop2Idle(void);

/* Ticks of LPTIM1 the margin of the wake-up leaves to sleep in Sleep on the clock the chip
** woke on, once the MSI is locked: the margin less OS_STOP2_RAISE_US, kept for the raise
** of the clock. */
#define SLOW_TICKS(wake)     ((wake) > OS_STOP2_RAISE_US ? \
                              ((wake) - OS_STOP2_RAISE_US) * 512u / 15625u : 0u)


/* The UART driver says whether Stop 2 would lose its work and disables USART1 across it,
** and the timer-event driver how far off its next event is, and stops and moves TIM4 on
** with TIM2; an image without them has nothing there to lose. */
__attribute__((weak)) BOOL _OSUARTIdle(void)
{
  return TRUE;
}

__attribute__((weak)) BOOL _OSUARTReceived(void)
{
  return FALSE;
}

__attribute__((weak)) void _OSUARTStop2(BOOL stopping)
{
  (void)stopping;
}

__attribute__((weak)) BOOL _OSTimerEventNext(UINT32 *delay)
{
  (void)delay;
  return FALSE;
}

__attribute__((weak)) void _OSTimerEventHalt(void)
{
}

__attribute__((weak)) BOOL _OSTimerEventResume(UINT32 micros)
{
  (void)micros;
  return TRUE;
}


BOOL OSInitStop2(void)
{
  if (!OSInitLPTimer())
     return FALSE;
  RCC_APB3SLPENR |= RCC_LPTIM1;
  RCC_APB3STPENR |= RCC_LPTIM1;
  /* The MSIS as the clock of the wake-up, the MSIK with it, the same value in both as
  ** RCC_CFGR1 asks (p. 426), as reset leaves them: _OSRaiseSystemClock takes the MSIS on
  ** from there, and waits for both MSIS and MSIK ready before it writes their dividers. */
  RCC_CFGR1 &= ~(RCC_CFGR1_STOPWUCK | RCC_CFGR1_STOPKERWUCK);
  DBGMCU_CR &= ~(DBGMCU_CR_DBG_STOP | DBGMCU_CR_DBG_STANDBY);
  PWR_CR1 = (PWR_CR1 & ~PWR_CR1_LPMS_MASK) | PWR_CR1_LPMS_STOP2;
  _OSIdleHook = Stop2Idle;
  return TRUE;
} /* end of OSInitStop2 */


/* OSGetStop2Counts: The counts at one instant, interrupts masked meanwhile and then left
** as the caller had them. */
void OSGetStop2Counts(OS_STOP2_COUNTS *counts)
{
  UINT32 primask;
  __asm volatile ("MRS %0, PRIMASK" : "=r" (primask) :: "memory");
  _OSDisableInterrupts();
  *counts = Counts;
  __asm volatile ("MSR PRIMASK, %0" :: "r" (primask) : "memory");
} /* end of OSGetStop2Counts */


void OSAllowStop2(BOOL allowed)
{
  Allowed = allowed;
} /* end of OSAllowStop2 */


void OSSetStop2Wake(UINT32 micros)
{
  WakeUs = micros;
} /* end of OSSetStop2Wake */


/* NextTick: Waits for the count of LPTIM1 to change, and returns the new one. */
static UINT16 NextTick(void)
{
  UINT16 count = OSGetLPTimer(), next;
  while ((next = OSGetLPTimer()) == count);
  return next;
} /* end of NextTick */


/* SlowSleep: Sleep on the clock the chip woke from Stop 2 on, at most 48 MHz in range 2,
** until LPTIM1 counts until or another interrupt comes; the ticks slept. The margin of
** the wake-up would otherwise be spent at the system clock, which on the U5 drew 3 to 4
** times the current of its slower clock in Sleep (docs/stm32u5.md, "Slower clocks,
** measured"); on this chip it halves the clock at 96 MHz only, and saves the wait for
** range 1. An until less than two ticks off is not slept: the compare written takes some
** ticks of LPTIM1 to hold, and one already passed would never come. */
static UINT16 SlowSleep(UINT16 until)
{
  UINT16 from = OSGetLPTimer();
  if ((UINT16)(until - from) < 4u || (UINT16)(until - from) > MAX_TICKS)
     return 0;
  OSSetLPTimerCompare(until);
  if ((UINT16)(until - OSGetLPTimer()) < 2u || (UINT16)(until - OSGetLPTimer()) > MAX_TICKS)
     return (UINT16)(OSGetLPTimer() - from);
  NVIC_ICPR(OS_IO_LPTIM1) = NVIC_BIT(OS_IO_LPTIM1);
  NVIC_ISER(OS_IO_LPTIM1) = NVIC_BIT(OS_IO_LPTIM1);
  __asm volatile ("DSB\n\tWFI\n\tISB" ::: "memory");
  NVIC_ICER(OS_IO_LPTIM1) = NVIC_BIT(OS_IO_LPTIM1);
  (void)OSLPTimerCompared();
  NVIC_ICPR(OS_IO_LPTIM1) = NVIC_BIT(OS_IO_LPTIM1);
  return (UINT16)(OSGetLPTimer() - from);
} /* end of SlowSleep */


/* OpenWindow: The window of LPUART1 from now on. A compare already armed for an earlier
** end stays: it ends that sleep, and the next arms the new end. */
static void OpenWindow(void)
{
  WindowOpen = TRUE;
  WindowEnd = (UINT16)(OSGetLPTimer() + WINDOW_TICKS);
} /* end of OpenWindow */


/* WindowHolds: TRUE while the window of LPUART1 is open, its end not passed as long as
** the ticks left are fewer than the window. The idle task kept from running for 2 s, the
** counter of LPTIM1 gone round, may take a closed window for open: a Sleep for nothing,
** no more. Closed, the compare is free for Stop 2, which sets its own. */
static BOOL WindowHolds(void)
{
  UINT16 left;
  if (_OSUARTReceived())
     OpenWindow();
  if (WindowOpen) {
     left = (UINT16)(WindowEnd - OSGetLPTimer());
     if (left < WINDOW_MIN_TICKS || left > WINDOW_TICKS) {
        WindowOpen = FALSE;
        WindowArmed = FALSE;
     }
  }
  return WindowOpen;
} /* end of WindowHolds */


/* WindowSleep: One sleep in Sleep while the window holds, Stop 2 otherwise due: LPTIM1
** ends it at the window's end, armed once a window, since its write waits some two ticks,
** interrupts masked, and the bytes of a message come every 87 us at 115,200 baud. */
static void WindowSleep(void)
{
  UINT16 left;
  if (!WindowArmed) {
     OSSetLPTimerCompare(WindowEnd);
     WindowArmed = TRUE;
     /* The end passed while the compare was written: it would match only 2 s on, the idle
     ** task in Sleep meanwhile. The window closes instead, Stop 2 due at the next turn. */
     left = (UINT16)(WindowEnd - OSGetLPTimer());
     if (left == 0 || left > WINDOW_TICKS) {
        WindowOpen = FALSE;
        WindowArmed = FALSE;
        return;
     }
  }
  NVIC_ICPR(OS_IO_LPTIM1) = NVIC_BIT(OS_IO_LPTIM1);
  NVIC_ISER(OS_IO_LPTIM1) = NVIC_BIT(OS_IO_LPTIM1);
  __asm volatile ("DSB\n\tWFI\n\tISB" ::: "memory");
  NVIC_ICER(OS_IO_LPTIM1) = NVIC_BIT(OS_IO_LPTIM1);
  if (OSLPTimerCompared())
     WindowArmed = FALSE;                // ended, or a window opened since: armed anew
  NVIC_ICPR(OS_IO_LPTIM1) = NVIC_BIT(OS_IO_LPTIM1);
  Counts.LinkHeld += 1;
} /* end of WindowSleep */


/* Stop2Idle: One sleep of the idle task, in Stop 2 if the next event is far enough off and
** nothing would be lost, in Sleep otherwise. */
static void Stop2Idle(void)
{
  UINT32 now, target, compare, limit, ticks, micros, event, hsi;
  UINT16 start, alarm, wake, end, slept;
  BOOL holds, woken, whole;
  _OSDisableInterrupts();
  holds = WindowHolds();
  now = TIM_CNT;
  compare = TIM_CCR1;                      // 0 when disarmed (Escapement_Timer.c)
  /* The kernel arms the compare with an event-driven task's arrival even beyond the wrap,
  ** where it never matches (ArrivalQueueInsertTestKey, EscapementHard.c): the wrap comes
  ** first then. Slept past it, TIM2 was set back to just before it and the kernel's clock
  ** lost the rest (Stop2EventWrapU5, 2026-09-29). */
  limit = compare != 0 && compare > now && compare < TIMER_WRAP ? compare : TIMER_WRAP;
  target = limit;
  if (_OSTimerEventNext(&event) && event < target - now)
     target = now + event;               // the kernel's time of the event, near enough
  if (!Allowed || target - now < OS_STOP2_MIN_US || !_OSUARTIdle()) {
     __asm volatile ("WFI" ::: "memory");
     _OSEnableInterrupts();
     return;
  }
  if (holds) {
     WindowSleep();
     _OSEnableInterrupts();
     return;
  }
  micros = target - now - WakeUs;
  whole = micros <= MAX_TICKS * 15625u / 512u;   // the alarm WakeUs before the event
  if (!whole)
     micros = MAX_TICKS * 15625u / 512u;
  ticks = micros * 512u / 15625u;
  start = OSGetLPTimer();
  alarm = (UINT16)(start + ticks);
  OSSetLPTimerCompare(alarm);
  start = NextTick();
  TIM_CR1 &= ~TIM_CR1_CEN;
  _OSTimerEventHalt();
  now = TIM_CNT;
  /* A compare the write above may have met on its way clears, and the interrupt with it:
  ** only the one to come is to wake the chip. */
  (void)OSLPTimerCompared();
  NVIC_ICPR(OS_IO_LPTIM1) = NVIC_BIT(OS_IO_LPTIM1);
  NVIC_ISER(OS_IO_LPTIM1) = NVIC_BIT(OS_IO_LPTIM1);
  /* HSI16 is cleared entering Stop (RCC_CR, p. 418) and set again after it for LPUART1
  ** (Escapement_UART.c): read before Stop 2, not after. */
  hsi = RCC_CR & RCC_CR_HSION;
  _OSUARTStop2(TRUE);
  PWR_SR = PWR_SR_CSSF;
  SCB_SCR |= SCB_SCR_SLEEPDEEP;
  __asm volatile ("DSB\n\tWFI\n\tISB" ::: "memory");
  SCB_SCR &= ~SCB_SCR_SLEEPDEEP;
  NVIC_ICER(OS_IO_LPTIM1) = NVIC_BIT(OS_IO_LPTIM1);
  woken = !OSLPTimerCompared();
  NVIC_ICPR(OS_IO_LPTIM1) = NVIC_BIT(OS_IO_LPTIM1);
  /* An interrupt already pending leaves the WFI at once, the chip still at its clock. */
  if (PWR_SR & PWR_SR_STOPF) {
     wake = OSGetLPTimer();
     /* The lock first, as the U5 waits for its HSE first: a lock that misses its bound
     ** then eats into the sleep below, not into the margin kept for the raise. */
     if (_OSWaitMSILock(OSGetLPTimer,OS_STOP2_LOCK_TICKS))
        Counts.LockMissed += 1;          // the MSIS free, within some 1 %, until it locks
     /* The rest of the margin in Sleep, OS_STOP2_RAISE_US kept for the raise; a byte on
     ** LPUART1 has the clock raised at once. */
     slept = woken || !whole ? 0 : SlowSleep((UINT16)(alarm + SLOW_TICKS(WakeUs)));
     if (slept != 0)
        Counts.Slow += 1;
     _OSRaiseSystemClock();
     RCC_CR |= hsi;                      // HSI16 back for LPUART1 (Escapement_UART.c)
     _OSUARTStop2(FALSE);
     if (woken)
        OpenWindow();                    // LPUART1 woke it, its byte read or not
     end = NextTick();
     Counts.Entries += 1;
     if ((UINT16)(end - wake - slept) > Counts.WakeMaxTicks)
        Counts.WakeMaxTicks = (UINT16)(end - wake - slept);
  }
  else {
     _OSUARTStop2(FALSE);
     end = NextTick();
  }
  micros = (UINT16)(end - start) * 15625u + Fraction + RESTART_FRACTION;
  Fraction = micros % 512u;
  micros /= 512u;
  TIM_CNT = now + micros >= limit ? limit - 1 : now + micros;
  TIM_CR1 |= TIM_CR1_CEN;
  if (!_OSTimerEventResume(micros) || now + micros >= target)
     Counts.Late += 1;
  _OSEnableInterrupts();
} /* end of Stop2Idle */
