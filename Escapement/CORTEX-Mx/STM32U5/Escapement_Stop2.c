/* Copyright (c) 2026 Bertrand Hurst. All rights reserved.
** Escapement - Lightweight Power-Aware Real-Time OS.
** Distributed under the terms of LICENSE at the root of this repository.
*/
/* File Escapement_Stop2.c: The idle task in Stop 2 (Escapement_Stop2.h). RM0456 rev. 7,
** chapters 10 (PWR), 11 (RCC) and 58 (LPTIM); ES0499 rev. 12 for the errata.
**
** TIM2 stays the kernel's clock while the chip runs. The idle task, finding the next
** event at least OS_STOP2_MIN_US off, the compare of TIM2 for the next arrival, its wrap
** at 2^30, or the next timer event of TIM5, arms the compare of LPTIM1 OS_STOP2_WAKE_US
** before it and enters Stop 2, where PLL1, the HSE, TIM2 and TIM5 stop and LPTIM1 counts
** on. On waking the chip runs on the MSIS in the range it had, 4, or 2 on a board without
** the HSE or once the HSE missed a wake-up (RM0456, 10.7.8, RCC_ICSCR1);
** _OSRaiseSystemClock takes it back to 160 MHz, waiting at most 1.95 ms for the HSE,
** and TIM2 and TIM5 are moved on by the time LPTIM1 counted
** (Escapement_TimerEvent.c). Both are stopped and started again on an edge of LPTIM1, so
** that the time between is whole ticks, 15625/512 us each, the fraction carried to the
** next sleep. The idle task then sleeps in Sleep until the event. Interrupts stay masked
** from the reading of TIM2 to its start again: one that comes meanwhile wakes the chip and
** is taken after, late by the start of the HSE should it have come during Stop 2.
**
** A byte received on LPUART1, or a wake-up from Stop 2 that LPTIM1 did not cause, keeps
** the idle task in Sleep for OS_STOP2_LINK_WINDOW_US after it, so that the bytes that
** follow are sampled on a running HSI16, at 115,200 baud: the first byte of a silence only
** wakes the chip (docs/roadmap.md, item 6). The window is counted on LPTIM1, whose compare
** wakes the chip at its end if nothing else does, for Stop 2 to resume then.
**
** LPTIM1 wakes the chip from Stop 2 with its clock enabled in Run, in Sleep and Stop (the
** reset value of RCC_APB3SMENR) and in autonomous mode (RCC_SRDAMR), and its interrupt
** enabled in the NVIC (RM0456, 10.7.8 and 11.4.24): enabled only across the WFI, since it
** has no handler. DBG_STOP and DBG_STANDBY are cleared, which a debugger may have set:
** Debian's OpenOCD 0.12.0 sets both at each connection (stm32x5x_common.cfg), and a
** system reset leaves them (RM0456, 75.12.4). With DBG_STANDBY set and DBG_STOP clear the
** NUCLEO-U575ZI-Q entered Stop 2 and never woke, nor did NRST reach it, until powered off
** (2026-10-02); with DBG_STOP set the clocks never stop, and a wake-up from an SRD
** peripheral may end in a HardFault (errata 2.2.19). PLL2, PLL3,
** HSI48 and SHSI are not started by the port (2.2.5), nor any SRAM powered down (2.2.22).
** ICACHE is disabled across the WFI and enabled again after it (2.2.11, revision X: the
** first fetch from the cache line last used before Stop 2 may read wrong after it), which
** an image run from SRAM through the S-bus, uncached, does not need and one run from the
** flash does (STM32U5_FLASH.ld).
** Platform version: STM32U585 (Arduino UNO Q), any STM32U5.
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

#define PWR_BASE             0x46020800
#define PWR_CR1              *((volatile UINT32 *)(PWR_BASE + 0x00))
#define PWR_SR               *((volatile UINT32 *)(PWR_BASE + 0x38))
#define PWR_CR1_LPMS_MASK    7u
#define PWR_CR1_LPMS_STOP2   2u
#define PWR_SR_CSSF          (1u << 0)
#define PWR_SR_STOPF         (1u << 1)

#define RCC_BASE             0x46020C00
#define RCC_CR               *((volatile UINT32 *)(RCC_BASE + 0x00))
#define RCC_CR_HSION         (1u << 8)
#define RCC_APB3SMENR        *((volatile UINT32 *)(RCC_BASE + 0xD0))
#define RCC_SRDAMR           *((volatile UINT32 *)(RCC_BASE + 0xD8))
#define RCC_LPTIM1           (1u << 11)    /* in APB3SMENR and SRDAMR */

#define SCB_SCR              *((volatile UINT32 *)0xE000ED10)
#define SCB_SCR_SLEEPDEEP    (1u << 2)
#define DBGMCU_CR            *((volatile UINT32 *)0xE0044004)
#define ICACHE_CR            *((volatile UINT32 *)0x40030400)
#define ICACHE_CR_EN         (1u << 0)
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
** domain of LPTIM1 some time after its write, which RM0456 does not bound (58.4.11), and
** one that arrives past the count never matches: a window nearer its end is taken as
** closed, and its end is read again once the compare is written (WindowSleep). */
#define WINDOW_TICKS         ((UINT32)OS_STOP2_LINK_WINDOW_US * 512u / 15625u)
#define WINDOW_MIN_TICKS     4u
#if OS_STOP2_LINK_WINDOW_US > 900000
   #error "OS_STOP2_LINK_WINDOW_US: LPTIM1 counts 2 s at most"
#endif


static OS_STOP2_COUNTS Counts;
static volatile BOOL Allowed = TRUE;
static UINT32 Fraction;                    // of a microsecond, in 512ths, carried over
static BOOL WindowOpen;                    // the window of LPUART1, and its end on LPTIM1
static UINT16 WindowEnd;
static BOOL WindowArmed;                   // the compare of LPTIM1 set at WindowEnd

static void Stop2Idle(void);


/* The UART driver says whether Stop 2 would lose its work, and the timer-event driver
** how far off its next event is, and stops and moves TIM5 on with TIM2; an image
** without them has nothing there to lose. */
__attribute__((weak)) BOOL _OSUARTIdle(void)
{
  return TRUE;
}

__attribute__((weak)) BOOL _OSUARTReceived(void)
{
  return FALSE;
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
  RCC_APB3SMENR |= RCC_LPTIM1;
  RCC_SRDAMR |= RCC_LPTIM1;
  DBGMCU_CR &= ~(DBGMCU_CR_DBG_STOP | DBGMCU_CR_DBG_STANDBY);
  PWR_CR1 = (PWR_CR1 & ~PWR_CR1_LPMS_MASK) | PWR_CR1_LPMS_STOP2;
  _OSIdleHook = Stop2Idle;
  return TRUE;
} /* end of OSInitStop2 */


/* OSGetStop2Counts: The counts at one instant, interrupts masked meanwhile and then left
** as the caller had them: from main before the kernel starts, or from an interrupt, they
** were unmasked on the way out before 2026-09-29. */
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


/* NextTick: Waits for the count of LPTIM1 to change, and returns the new one. */
static UINT16 NextTick(void)
{
  UINT16 count = OSGetLPTimer(), next;
  while ((next = OSGetLPTimer()) == count);
  return next;
} /* end of NextTick */


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
  UINT16 start, wake, end;
  BOOL holds, woken;
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
  micros = target - now - OS_STOP2_WAKE_US;
  if (micros > MAX_TICKS * 15625u / 512u)
     micros = MAX_TICKS * 15625u / 512u;
  ticks = micros * 512u / 15625u;
  start = OSGetLPTimer();
  OSSetLPTimerCompare((UINT16)(start + ticks));
  start = NextTick();
  TIM_CR1 &= ~TIM_CR1_CEN;
  _OSTimerEventHalt();
  now = TIM_CNT;
  /* A compare the write above may have met on its way clears, and the interrupt with it:
  ** only the one to come is to wake the chip. */
  (void)OSLPTimerCompared();
  NVIC_ICPR(OS_IO_LPTIM1) = NVIC_BIT(OS_IO_LPTIM1);
  NVIC_ISER(OS_IO_LPTIM1) = NVIC_BIT(OS_IO_LPTIM1);
  PWR_SR = PWR_SR_CSSF;
  hsi = RCC_CR & RCC_CR_HSION;           // cleared entering Stop (RM0456, RCC_CR)
  _OSSRAMBeforeStop2(TRUE);              // the SRAM's wait state for the wake-up, if any
  ICACHE_CR &= ~ICACHE_CR_EN;            // erratum 2.2.11; invalidated in the background
  SCB_SCR |= SCB_SCR_SLEEPDEEP;
  __asm volatile ("DSB\n\tWFI\n\tISB" ::: "memory");
  SCB_SCR &= ~SCB_SCR_SLEEPDEEP;
  ICACHE_CR |= ICACHE_CR_EN;
  NVIC_ICER(OS_IO_LPTIM1) = NVIC_BIT(OS_IO_LPTIM1);
  woken = !OSLPTimerCompared();
  NVIC_ICPR(OS_IO_LPTIM1) = NVIC_BIT(OS_IO_LPTIM1);
  /* An interrupt already pending leaves the WFI at once, the chip still on PLL1. */
  if (PWR_SR & PWR_SR_STOPF) {
     wake = OSGetLPTimer();
     if (_OSRaiseSystemClock(OSGetLPTimer))
        Counts.HSEMissed += 1;           // PLL1 on the MSIS until the next wake-up
     RCC_CR |= hsi;                      // HSI16 back for LPUART1 (Escapement_UART.c)
     if (woken)
        OpenWindow();                    // LPUART1 woke it, its byte read or not
     end = NextTick();
     Counts.Entries += 1;
     if ((UINT16)(end - wake) > Counts.WakeMaxTicks)
        Counts.WakeMaxTicks = (UINT16)(end - wake);
  }
  else {
     _OSSRAMBeforeStop2(FALSE);
     end = NextTick();
  }
  micros = (UINT16)(end - start) * 15625u + Fraction;
  Fraction = micros % 512u;
  micros /= 512u;
  TIM_CNT = now + micros >= limit ? limit - 1 : now + micros;
  TIM_CR1 |= TIM_CR1_CEN;
  if (!_OSTimerEventResume(micros) || now + micros >= target)
     Counts.Late += 1;
  _OSEnableInterrupts();
} /* end of Stop2Idle */
