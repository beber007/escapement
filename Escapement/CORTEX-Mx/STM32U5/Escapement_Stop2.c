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
** on. On waking the chip runs on the MSIS in range 4 (RM0456, 10.7.8); _OSRaiseSystemClock
** takes it back to 160 MHz, and TIM2 and TIM5 are moved on by the time LPTIM1 counted
** (Escapement_TimerEvent.c). Both are stopped and started again on an edge of LPTIM1, so
** that the time between is whole ticks, 15625/512 us each, the fraction carried to the
** next sleep. The idle task then sleeps in Sleep until the event. Interrupts stay masked
** from the reading of TIM2 to its start again: one that comes meanwhile wakes the chip and
** is taken after, late by the start of the HSE should it have come during Stop 2.
**
** LPTIM1 wakes the chip from Stop 2 with its clock enabled in Run, in Sleep and Stop (the
** reset value of RCC_APB3SMENR) and in autonomous mode (RCC_SRDAMR), and its interrupt
** enabled in the NVIC (RM0456, 10.7.8 and 11.4.24): enabled only across the WFI, since it
** has no handler. Errata: DBG_STOP is cleared, which the debugger may have set, since a
** wake-up from an SRD peripheral with it set may end in a HardFault (2.2.19); PLL2, PLL3,
** HSI48 and SHSI are not started by the port (2.2.5), nor any SRAM powered down (2.2.22).
** The image runs from SRAM through the S-bus, which ICACHE does not cache (2.2.11).
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
#define RCC_APB3SMENR        *((volatile UINT32 *)(RCC_BASE + 0xD0))
#define RCC_SRDAMR           *((volatile UINT32 *)(RCC_BASE + 0xD8))
#define RCC_LPTIM1           (1u << 11)    /* in APB3SMENR and SRDAMR */

#define SCB_SCR              *((volatile UINT32 *)0xE000ED10)
#define SCB_SCR_SLEEPDEEP    (1u << 2)
#define DBGMCU_CR            *((volatile UINT32 *)0xE0044004)
#define DBGMCU_CR_DBG_STOP   (1u << 1)

#define NVIC_ISER(irq)       ((volatile UINT32 *)0xE000E100)[(irq) >> 5]
#define NVIC_ICER(irq)       ((volatile UINT32 *)0xE000E180)[(irq) >> 5]
#define NVIC_ICPR(irq)       ((volatile UINT32 *)0xE000E280)[(irq) >> 5]
#define NVIC_BIT(irq)        (1u << ((irq) & 0x1F))

/* The longest sleep, in ticks, short of the 2^16 of the counter of LPTIM1: 1.83 s. */
#define MAX_TICKS            60000u


static OS_STOP2_COUNTS Counts;
static volatile BOOL Allowed = TRUE;
static UINT32 Fraction;                    // of a microsecond, in 512ths, carried over

static void Stop2Idle(void);


/* The UART driver says whether Stop 2 would lose its work, and the timer-event driver
** how far off its next event is, and stops and moves TIM5 on with TIM2; an image
** without them has nothing there to lose. */
__attribute__((weak)) BOOL _OSUARTIdle(void)
{
  return TRUE;
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
  DBGMCU_CR &= ~DBGMCU_CR_DBG_STOP;
  PWR_CR1 = (PWR_CR1 & ~PWR_CR1_LPMS_MASK) | PWR_CR1_LPMS_STOP2;
  _OSIdleHook = Stop2Idle;
  return TRUE;
} /* end of OSInitStop2 */


void OSGetStop2Counts(OS_STOP2_COUNTS *counts)
{
  _OSDisableInterrupts();
  *counts = Counts;
  _OSEnableInterrupts();
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


/* Stop2Idle: One sleep of the idle task, in Stop 2 if the next event is far enough off and
** nothing would be lost, in Sleep otherwise. */
static void Stop2Idle(void)
{
  UINT32 now, target, compare, ticks, micros, event;
  UINT16 start, wake, end;
  _OSDisableInterrupts();
  now = TIM_CNT;
  compare = TIM_CCR1;                      // 0 when disarmed (Escapement_Timer.c)
  target = compare != 0 && compare > now ? compare : TIMER_WRAP;
  if (_OSTimerEventNext(&event) && event < target - now)
     target = now + event;               // the kernel's time of the event, near enough
  if (!Allowed || target - now < OS_STOP2_MIN_US || !_OSUARTIdle()) {
     __asm volatile ("WFI" ::: "memory");
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
  SCB_SCR |= SCB_SCR_SLEEPDEEP;
  __asm volatile ("DSB\n\tWFI\n\tISB" ::: "memory");
  SCB_SCR &= ~SCB_SCR_SLEEPDEEP;
  NVIC_ICER(OS_IO_LPTIM1) = NVIC_BIT(OS_IO_LPTIM1);
  (void)OSLPTimerCompared();
  NVIC_ICPR(OS_IO_LPTIM1) = NVIC_BIT(OS_IO_LPTIM1);
  /* An interrupt already pending leaves the WFI at once, the chip still on PLL1. */
  if (PWR_SR & PWR_SR_STOPF) {
     wake = OSGetLPTimer();
     _OSRaiseSystemClock();
     end = NextTick();
     Counts.Entries += 1;
     if ((UINT16)(end - wake) > Counts.WakeMaxTicks)
        Counts.WakeMaxTicks = (UINT16)(end - wake);
  }
  else
     end = NextTick();
  micros = (UINT16)(end - start) * 15625u + Fraction;
  Fraction = micros % 512u;
  micros /= 512u;
  TIM_CNT = now + micros >= compare && compare > now ? compare - 1 :
            now + micros >= TIMER_WRAP ? TIMER_WRAP - 1 : now + micros;
  TIM_CR1 |= TIM_CR1_CEN;
  if (!_OSTimerEventResume(micros) || now + micros >= target)
     Counts.Late += 1;
  _OSEnableInterrupts();
} /* end of Stop2Idle */
