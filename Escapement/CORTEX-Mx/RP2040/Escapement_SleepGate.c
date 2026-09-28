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
** tick, which the watchdog's tick generator makes from clk_ref (4.7.2): CLK_SYS_TIMER,
** and CLK_SYS_WATCHDOG for the generator, are kept, so that the kernel's time goes on and
** its alarm wakes the core; whether the generator needs CLK_SYS_WATCHDOG the datasheet
** does not say. The idle task itself is unchanged: its WFI enters SLEEP once both cores
** have deep sleep enabled.
** Platform version: RP2040 (Raspberry Pi Pico).
*/

#include "Escapement.h"
#include "Escapement_Core1.h"
#include "Escapement_SleepGate.h"

#define CLOCKS_BASE          0x40008000
#define CLOCKS_SLEEP_EN0     *((volatile UINT32 *)(CLOCKS_BASE + 0xA8))
#define CLOCKS_SLEEP_EN1     *((volatile UINT32 *)(CLOCKS_BASE + 0xAC))
#define CLK_SYS_TIMER        (1u << 5)    /* in SLEEP_EN1 */
#define CLK_SYS_WATCHDOG     (1u << 12)   /* in SLEEP_EN1 */

#define WATCHDOG_CTRL_CLR    *((volatile UINT32 *)(0x40058000 + 0x3000))
#define WATCHDOG_ENABLE      (1u << 30)

#define SCB_SCR              *((volatile UINT32 *)0xE000ED10)
#define SCB_SCR_SLEEPDEEP    (1u << 2)

/* Core 1 needs no more than its entry below. */
static UINT32 Core1Stack[16] __attribute__((aligned(8)));

static void Core1Sleep(void);


void OSInitSleepGate(UINT32 keep0, UINT32 keep1)
{
  /* A firmware in flash may have armed the watchdog, which a reset of the processors
  ** leaves running and which pauses only while the debugger holds a core: once core 1
  ** runs, it rebooted the chip into the flash (2026-09-28), as for FourSlotCoresPico. */
  WATCHDOG_CTRL_CLR = WATCHDOG_ENABLE;
  CLOCKS_SLEEP_EN0 = keep0;
  CLOCKS_SLEEP_EN1 = keep1 | CLK_SYS_TIMER | CLK_SYS_WATCHDOG;
  SCB_SCR |= SCB_SCR_SLEEPDEEP;
  OSLaunchCore1(Core1Sleep,&Core1Stack[16]);
} /* end of OSInitSleepGate */


/* Core1Sleep: Core 1 in deep sleep for good, its SCR being its own; no interrupt is
** enabled on it, and an event only brings it round the loop. */
static void Core1Sleep(void)
{
  SCB_SCR |= SCB_SCR_SLEEPDEEP;
  while (TRUE)
     __asm volatile ("WFE" ::: "memory");
} /* end of Core1Sleep */
