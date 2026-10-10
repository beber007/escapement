/* Copyright (c) 2026 Bertrand Hurst. All rights reserved.
** Escapement - Lightweight Power-Aware Real-Time OS.
** Distributed under the terms of LICENSE at the root of this repository.
*/
/* File Escapement_SleepGate.c: The idle task of the RP2040 in SLEEP (Escapement_SleepGate.h).
** RP2040 datasheet (build of 2025-02-20), 2.11.2 SLEEP state, 2.11.5.1, and the CLOCKS
** registers of 2.15.7, as in Raspberry Pi's pico-sdk, hardware/regs/clocks.h.
**
** SLEEP gates the top-level clocks as SLEEP_EN0 and SLEEP_EN1 say instead of WAKE_EN0
** and WAKE_EN1, while the oscillators and the PLLs run on; a clock request handshake holds
** the cores off the bus until the clocks are back. The timer of the kernel counts the
** tick, which the watchdog's tick generator makes from clk_ref (4.7.2): CLK_SYS_TIMER
** alone is kept, so that the kernel's time goes on and its alarm wakes the core. The
** generator runs on clk_ref, which SLEEP does not gate, and not on CLK_SYS_WATCHDOG, the
** clock of the watchdog's bus interface: without it the timer kept the host's time
** through SLEEP on the board (2026-09-28, docs/rp2040.md). The WFI of the idle task
** enters SLEEP once both cores have deep sleep enabled.
**
** As on the RP2350, the idle task stops PLL_SYS for each sleep long enough: SLEEP drew
** 5.07 mA on the Pico's 3V3 rail with it running, 1.22 with it stopped and clk_sys on
** the crystal (SleepPico, 2026-10-10, docs/power-aware.md). clk_ref, and with it the
** tick, stays on the crystal, so the kernel's time is exact; the interrupt that ends the
** sleep is taken once PLL_SYS is locked again, some 55 us, interrupts masked meanwhile.
** The sleep therefore ends WAKE_US before the first alarm of the timer armed
** (_OSAlarmTime), on alarm 3, raises PLL_SYS, and waits for that alarm in SLEEP still,
** PLL_SYS running. A sleep shorter than STOP_MIN_US keeps PLL_SYS running throughout; an
** interrupt of another source is taken late by the lock. Alarm 3 is this file's: the
** timer events given it first, PLL_SYS keeps running, and given it after, they refuse it.
** Under the power-aware kernel the sleep keeps the operating point in effect, which
** _OSRaiseSystemClock restores.
** Platform version: RP2040 (Raspberry Pi Pico).
*/

#include "Escapement.h"
#include "Escapement_Core1.h"
#include "Escapement_Timer.h"
#include "Escapement_SleepGate.h"

#define CLOCKS_BASE          0x40008000
#define CLOCKS_SLEEP_EN0     *((volatile UINT32 *)(CLOCKS_BASE + 0xA8))
#define CLOCKS_SLEEP_EN1     *((volatile UINT32 *)(CLOCKS_BASE + 0xAC))
#define CLK_SYS_TIMER        (1u << 5)    /* in SLEEP_EN1 */

#define WATCHDOG_CTRL_CLR    *((volatile UINT32 *)(0x40058000 + 0x3000))
#define WATCHDOG_ENABLE      (1u << 30)

/* The timer's alarm 3, which ends the sleep early, and its interrupt, taken only through
** the WFI, masked. */
#define TIMER_BASE           0x40054000
#define TIMER_ALARM3         *((volatile UINT32 *)(TIMER_BASE + 0x1C))
#define TIMER_ARMED          *((volatile UINT32 *)(TIMER_BASE + 0x20))
#define TIMER_TIMERAWL       *((volatile UINT32 *)(TIMER_BASE + 0x28))
#define TIMER_INTR           *((volatile UINT32 *)(TIMER_BASE + 0x34))
#define TIMER_INTE_SET       *((volatile UINT32 *)(TIMER_BASE + 0x2000 + 0x38))
#define ALARM3_BIT           (1u << 3)
#define KERNEL_ALARMS        0x7u          /* alarms 0 to 2, the kernel's and the events' */
#define NVIC_ISER            *((volatile UINT32 *)0xE000E100)
#define NVIC_ICER            *((volatile UINT32 *)0xE000E180)
#define NVIC_ICPR            *((volatile UINT32 *)0xE000E280)

#define WAKE_US              150u          /* the lock's 55 us measured, and more */
#define STOP_MIN_US          (2u * WAKE_US)

#define SCB_SCR              *((volatile UINT32 *)0xE000ED10)
#define SCB_SCR_SLEEPDEEP    (1u << 2)

/* Core 1 needs no more than its entry below. */
static UINT32 Core1Stack[16] __attribute__((aligned(8)));

static void Core1Sleep(void);
static void GatedSleep(void);


void OSInitSleepGate(UINT32 keep0, UINT32 keep1)
{
  /* A firmware in flash may have armed the watchdog, which a reset of the processors
  ** leaves running and which pauses only while the debugger holds a core: once core 1
  ** runs, it rebooted the chip into the flash (2026-09-28), as for FourSlotCoresPico. */
  WATCHDOG_CTRL_CLR = WATCHDOG_ENABLE;
  /* Core 1 first: launching it, core 0 waits in WFE for the bootrom's answers on the
  ** inter-core FIFO, and on the RP2350, with deep sleep and the gates already set, the
  ** chip entered SLEEP there, the FIFO's clock gated, and the answer never came
  ** (2026-10-04). */
  OSLaunchCore1(Core1Sleep,&Core1Stack[16]);
  CLOCKS_SLEEP_EN0 = keep0;
  CLOCKS_SLEEP_EN1 = keep1 | CLK_SYS_TIMER;
  SCB_SCR |= SCB_SCR_SLEEPDEEP;
  /* Alarm 3 taken by the timer events already, PLL_SYS keeps running. */
  if (OSGetISRDescriptor(OS_IO_TIMER_3) != NULL)
     return;
  TIMER_ARMED = ALARM3_BIT;
  TIMER_INTR = ALARM3_BIT;
  TIMER_INTE_SET = ALARM3_BIT;
  _OSIdleHook = GatedSleep;
} /* end of OSInitSleepGate */


/* GatedSleep: One sleep of the idle task. Masked, WFI still wakes on the interrupt that
** becomes pending, which is taken once unmasked, PLL_SYS running again. The first alarm
** armed is found from TIMER_ARMED and _OSAlarmTime, interrupts masked, so that none is
** armed meanwhile; with none, the overflow alarm of the kernel being always armed, the
** sleep is as long as an interrupt allows. */
static void GatedSleep(void)
{
  UINT32 now, armed, n;
  INT32 left, first = 0x7FFFFFFF;
  _OSDisableInterrupts();
  now = TIMER_TIMERAWL;
  armed = TIMER_ARMED & KERNEL_ALARMS;
  for (n = 0; n < 3; n += 1)
     if (armed & (1u << n)) {
        left = (INT32)(_OSAlarmTime[n] - now);
        if (left < first)
           first = left;
     }
  if (first < (INT32)STOP_MIN_US) {
     __asm volatile ("DSB\n\tWFI\n\tISB" ::: "memory");
     _OSEnableInterrupts();
     return;
  }
  TIMER_ALARM3 = now + (UINT32)first - WAKE_US;
  NVIC_ICPR = 1u << OS_IO_TIMER_3;
  NVIC_ISER = 1u << OS_IO_TIMER_3;
  _OSLowerSystemClock();
  __asm volatile ("DSB\n\tWFI\n\tISB" ::: "memory");
  _OSRaiseSystemClock();
  TIMER_ARMED = ALARM3_BIT;              // woken by another interrupt, it is not wanted
  TIMER_INTR = ALARM3_BIT;
  NVIC_ICER = 1u << OS_IO_TIMER_3;
  NVIC_ICPR = 1u << OS_IO_TIMER_3;
  __asm volatile ("DSB\n\tWFI\n\tISB" ::: "memory");   // up to the alarm, at once if due
  _OSEnableInterrupts();
} /* end of GatedSleep */


/* Core1Sleep: Core 1 in deep sleep for good, its SCR being its own; no interrupt is
** enabled on it, and an event only brings it round the loop. */
static void Core1Sleep(void)
{
  SCB_SCR |= SCB_SCR_SLEEPDEEP;
  while (TRUE)
     __asm volatile ("WFE" ::: "memory");
} /* end of Core1Sleep */
