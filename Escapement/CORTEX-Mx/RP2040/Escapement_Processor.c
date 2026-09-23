/* Copyright (c) 2026 Bertrand Hurst. All rights reserved.
** Escapement - Lightweight Power-Aware Real-Time OS.
** Distributed under the terms of LICENSE at the root of this repository.
*/
/* File Escapement_Processor.c: Clock tree setup for the RP2040, and the DVFS driver of the
** power-aware variant.
**
** The bootrom leaves the chip running on its ring oscillator, whose frequency is neither
** precise nor known — around 6 MHz. Both the 1 us tick of the timer and the UART baud
** rate need a reference worthy of the name, so the crystal is started and the whole tree
** is switched onto it.
**
** The system clock is then taken to the nominal 125 MHz of the RP2040 through the PLL,
** because the per-activation cost of the kernel sets a floor on the achievable period: at
** 12 MHz a task of 1 ms already trips its overload guard.
**
** The reference and peripheral clocks stay on the crystal on purpose. The former keeps the
** microsecond tick exact whatever the core does — which is precisely what makes this chip
** a good target for the power-aware variant — and the latter keeps the UART dividing a
** frequency the driver knows.
**
** The power-aware variant moves the system clock between three operating points and sets
** the core voltage of each through the VREG register. The datasheet guarantees the core
** between 1.05 and 1.16 V only (table 634), so by default the voltage merely goes from
** 1.10 V at 125 MHz down to 1.05 V below it. ESCAPEMENT_RP2040_UNDERVOLT goes beyond that
** specification, for the measurement bench: see docs/power-aware.md before enabling it.
**
** Platform version: RP2040 (Raspberry Pi Pico).
*/

#include "Escapement.h"

#define XOSC_BASE            0x40024000
#define XOSC_CTRL            *((volatile UINT32 *)(XOSC_BASE + 0x00))
#define XOSC_STATUS          *((volatile UINT32 *)(XOSC_BASE + 0x04))
#define XOSC_STARTUP         *((volatile UINT32 *)(XOSC_BASE + 0x0C))
#define XOSC_FREQ_RANGE_1_15 0xAA0
#define XOSC_ENABLE          (0xFAB << 12)
#define XOSC_STABLE          (1u << 31)

#define CLOCKS_BASE          0x40008000
#define CLK_REF_CTRL         *((volatile UINT32 *)(CLOCKS_BASE + 0x30))
#define CLK_REF_SELECTED     *((volatile UINT32 *)(CLOCKS_BASE + 0x38))
#define CLK_SYS_CTRL         *((volatile UINT32 *)(CLOCKS_BASE + 0x3C))
#define CLK_SYS_SELECTED     *((volatile UINT32 *)(CLOCKS_BASE + 0x44))
#define CLK_PERI_CTRL        *((volatile UINT32 *)(CLOCKS_BASE + 0x48))
#define CLK_PERI_ENABLE      (1u << 11)

#define CLK_REF_SRC_XOSC     2
#define CLK_SYS_SRC_REF      0
#define CLK_SYS_SRC_AUX      1
#define CLK_SYS_AUXSRC_PLL   (0u << 5)
#define CLK_PERI_AUXSRC_XOSC (4u << 5)

#define RESETS_CLR           *((volatile UINT32 *)(0x4000C000 + 0x3000))
#define RESETS_DONE          *((volatile UINT32 *)(0x4000C000 + 0x08))
#define RESETS_PLL_SYS_BIT   (1u << 12)

#define PLL_SYS_BASE         0x40028000
#define PLL_CS               *((volatile UINT32 *)(PLL_SYS_BASE + 0x00))
#define PLL_PWR              *((volatile UINT32 *)(PLL_SYS_BASE + 0x04))
#define PLL_FBDIV_INT        *((volatile UINT32 *)(PLL_SYS_BASE + 0x08))
#define PLL_PRIM             *((volatile UINT32 *)(PLL_SYS_BASE + 0x0C))
#define PLL_CS_LOCK          (1u << 31)
#define PLL_PWR_PD           (1u << 0)
#define PLL_PWR_POSTDIVPD    (1u << 3)
#define PLL_PWR_VCOPD        (1u << 5)
#define PLL_PRIM_125MHZ      ((6u << 16) | (2u << 12))
#define PLL_PRIM_50MHZ       ((6u << 16) | (5u << 12))


/* OSInitializeSystemClocks: Switches the reference, system and peripheral clocks onto the
** 12 MHz crystal. Must be called before anything that depends on time, which includes
** _OSInitializeTimer and the UART. */
void OSInitializeSystemClocks(void)
{
  /* Start the crystal and wait for it to settle. The startup delay is counted in batches
  ** of 256 cycles; 47 gives the millisecond recommended for a 12 MHz crystal. */
  XOSC_STARTUP = 47;
  XOSC_CTRL = XOSC_FREQ_RANGE_1_15 | XOSC_ENABLE;
  while ((XOSC_STATUS & XOSC_STABLE) == 0);
  /* Reference clock onto the crystal, then system clock onto the reference. Each switch
  ** is acknowledged by a bit of the SELECTED register. */
  CLK_REF_CTRL = CLK_REF_SRC_XOSC;
  while ((CLK_REF_SELECTED & (1u << CLK_REF_SRC_XOSC)) == 0);
  CLK_SYS_CTRL = CLK_SYS_SRC_REF;
  while ((CLK_SYS_SELECTED & (1u << CLK_SYS_SRC_REF)) == 0);
  /* Peripheral clock straight onto the crystal, so the UART keeps dividing a known 12 MHz
  ** whatever the system clock does afterwards. */
  CLK_PERI_CTRL = CLK_PERI_ENABLE | CLK_PERI_AUXSRC_XOSC;
  /* System clock to 125 MHz: the 12 MHz crystal multiplied by 125 gives a 1500 MHz VCO,
  ** divided by 6 then by 2. */
  RESETS_CLR = RESETS_PLL_SYS_BIT;
  while ((RESETS_DONE & RESETS_PLL_SYS_BIT) == 0);
  PLL_CS = 1;                      /* REFDIV = 1 */
  PLL_FBDIV_INT = 125;
  PLL_PWR &= ~(PLL_PWR_PD | PLL_PWR_VCOPD);
  while ((PLL_CS & PLL_CS_LOCK) == 0);
  PLL_PRIM = PLL_PRIM_125MHZ;
  PLL_PWR &= ~PLL_PWR_POSTDIVPD;
  /* Switch glitchlessly: park on the reference, select the PLL as auxiliary source, then
  ** take the auxiliary. */
  CLK_SYS_CTRL = CLK_SYS_SRC_REF;
  while ((CLK_SYS_SELECTED & (1u << CLK_SYS_SRC_REF)) == 0);
  CLK_SYS_CTRL = CLK_SYS_AUXSRC_PLL | CLK_SYS_SRC_REF;
  CLK_SYS_CTRL = CLK_SYS_AUXSRC_PLL | CLK_SYS_SRC_AUX;
  while ((CLK_SYS_SELECTED & (1u << CLK_SYS_SRC_AUX)) == 0);
} /* end of OSInitializeSystemClocks */


#ifdef ESCAPEMENT_VERSION_HARD_PA

#define VREG_BASE            0x40064000
#define VREG                 *((volatile UINT32 *)(VREG_BASE + 0x00))
#define VREG_BOD             *((volatile UINT32 *)(VREG_BASE + 0x04))
#define VREG_ROK             (1u << 12)
#define VREG_VSEL(v)         ((UINT32)(v) << 4)
#define VREG_EN              (1u << 0)
#define BOD_EN               (1u << 0)

/* Values of the VSEL fields of VREG and BOD, in 50 mV and 43 mV steps respectively. */
#define VSEL_0_90V           0x7
#define VSEL_0_95V           0x8
#define VSEL_1_05V           0xA
#define VSEL_1_10V           0xB
#define BOD_VSEL_0_817V      0x8

#define TIMER_TIMERAWL       *((volatile UINT32 *)(0x40054000 + 0x28))

/* Work done at each operating point relative to the fastest, times 256, the fastest left
** out. Rounded down, so that the kernel never credits a task with more work than it did. */
const UINT8 _OSSlowdownRatios[] = {24,    /*  12 / 125 * 256 = 24.6  */
                                   102};  /*  50 / 125 * 256 = 102.4 */

#ifdef ESCAPEMENT_RP2040_UNDERVOLT
   /* Outside the specification. 0.90 V at low frequency is what has been reported to
   ** work, 0.85 V what has been reported not to; the regulator itself is within 3 %. */
   static const UINT8 CoreVoltage[] = {VSEL_0_90V, VSEL_0_95V, VSEL_1_10V};
   /* Time allowed for the regulator to settle after raising the voltage, before the clock
   ** follows. The datasheet gives no figure, and the SDK waits 1 ms when it raises the
   ** voltage at start-up, which would hold the interrupts far too long here. Measured on
   ** the board (BenchVregPico.c, docs/rp2040.md), ROK comes back 49 us at most after
   ** 0.90 -> 1.10 V; but ROK only reports some 90 % of the target, so the margin is kept. */
   #ifndef RP2040_VREG_SETTLING_US
      #define RP2040_VREG_SETTLING_US 100
   #endif
#else
   static const UINT8 CoreVoltage[] = {VSEL_1_05V, VSEL_1_05V, VSEL_1_10V};
#endif

static UINT8 CurrentSpeed = OS_MAX_SPEED;

static void SetCoreVoltage(UINT8 vsel, BOOL raising);
static void SetSystemClock(UINT8 speed);


/* OSInitProcessorSpeed: Sets the voltage of the operating point OSInitializeSystemClocks
** left the chip in. With ESCAPEMENT_RP2040_UNDERVOLT the brown-out detector, which resets
** the chip below about 0.86 V, is first lowered to 0.817 V: the first lower voltage only
** comes once the kernel runs, long after the 30 us the new threshold takes to apply. */
void OSInitProcessorSpeed(void)
{
  #ifdef ESCAPEMENT_RP2040_UNDERVOLT
     VREG_BOD = VREG_VSEL(BOD_VSEL_0_817V) | BOD_EN;
  #endif
  VREG = VREG_VSEL(CoreVoltage[OS_MAX_SPEED]) | VREG_EN;
  CurrentSpeed = OS_MAX_SPEED;
} /* end of OSInitProcessorSpeed */


UINT8 OSGetProcessorSpeed(void)
{
  return CurrentSpeed;
} /* end of OSGetProcessorSpeed */


/* OSSetProcessorSpeed: Moves to another operating point, raising the voltage before the
** frequency and lowering it after, so that the core never runs faster than its voltage
** allows. The kernel calls it from tasks and from the timer handler alike, so the whole
** change is done with interrupts masked; without a lock of the PLL to wait for, that lasts
** a few cycles of clk_ref, plus the settling time when undervolting. */
void OSSetProcessorSpeed(UINT8 speed)
{
  UINT32 primask;
  __asm volatile ("MRS %0, PRIMASK" : "=r" (primask) :: "memory");
  _OSDisableInterrupts();
  if (speed != CurrentSpeed && speed <= OS_MAX_SPEED) {
     OSTrace(OS_TRACE_SPEED,speed,CurrentSpeed);
     if (CoreVoltage[speed] > CoreVoltage[CurrentSpeed])
        SetCoreVoltage(CoreVoltage[speed],TRUE);
     SetSystemClock(speed);
     if (CoreVoltage[speed] < CoreVoltage[CurrentSpeed])
        SetCoreVoltage(CoreVoltage[speed],FALSE);
     CurrentSpeed = speed;
  }
  __asm volatile ("MSR PRIMASK, %0" :: "r" (primask) : "memory");
} /* end of OSSetProcessorSpeed */


/* SetCoreVoltage: Within the specification there is nothing to wait for: every voltage
** used is valid at every frequency, the lower one included. Undervolting, the clock must
** not rise before the voltage has: ROK only says the output is above 90 % of the target,
** so a fixed settling time comes first. */
static void SetCoreVoltage(UINT8 vsel, BOOL raising)
{
  VREG = VREG_VSEL(vsel) | VREG_EN;
  #ifdef ESCAPEMENT_RP2040_UNDERVOLT
     if (raising) {
        UINT32 start = TIMER_TIMERAWL;
        while (TIMER_TIMERAWL - start < RP2040_VREG_SETTLING_US);
        while ((VREG & VREG_ROK) == 0);
     }
  #else
     (void)raising;
  #endif
} /* end of SetCoreVoltage */


/* SetSystemClock: The PLL keeps running at 1500 MHz. clk_sys is parked on clk_ref, which
** already is the 12 MHz operating point, then for the others the post divider is changed
** while nothing uses it and the auxiliary source taken back, glitchlessly both ways. */
static void SetSystemClock(UINT8 speed)
{
  CLK_SYS_CTRL = CLK_SYS_AUXSRC_PLL | CLK_SYS_SRC_REF;
  while ((CLK_SYS_SELECTED & (1u << CLK_SYS_SRC_REF)) == 0);
  if (speed == OS_12MHZ_SPEED)
     return;
  PLL_PRIM = (speed == OS_50MHZ_SPEED) ? PLL_PRIM_50MHZ : PLL_PRIM_125MHZ;
  CLK_SYS_CTRL = CLK_SYS_AUXSRC_PLL | CLK_SYS_SRC_AUX;
  while ((CLK_SYS_SELECTED & (1u << CLK_SYS_SRC_AUX)) == 0);
} /* end of SetSystemClock */

#endif /* ESCAPEMENT_VERSION_HARD_PA */
