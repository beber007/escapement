/* Copyright (c) 2026 Bertrand Hurst. All rights reserved.
** Escapement - Lightweight Power-Aware Real-Time OS.
** Distributed under the terms of LICENSE at the root of this repository.
*/
/* File DVFSU3.c: The operating points of the STM32U385, for a PPK2 in place of JP4 to
** measure, before a DVFS driver is written for it (docs/stm32u3.md, "Measuring DVFS"):
** whether a cycle costs less at a lower frequency and voltage, as the datasheet has it,
** or least at full speed, as on the STM32U5, the RP2350 and the RP2040
** (docs/power-aware.md). No kernel: the port's clock set-up at 12 MHz in range 2
** (make MHZ=12), then the points set here, each by the sequences of RM0487 rev. 3
** (9.3.3, p. 333-334).
**
** The core computes without a pause, a CRC-32 of a buffer checked each time against the
** first, DVFS_PHASE_S seconds of LPTIM1, which counts the LSE whatever the system clock,
** at each point in turn:
**
**  - 0, 96 MHz, range 1 (0.9 V typical), the booster on, as the port runs;
**  - 1, 48 MHz, range 1, the booster on: 2, its voltage alone, against
**  - 2, 48 MHz, range 2 (0.75 V typical), the booster on, as the port runs at 48;
**  - 3, 24 MHz, range 1, the booster off;
**  - 4, 24 MHz, range 2, the booster off, as the port runs at 24;
**  - 5, 24 MHz, range 2, the booster on: what the booster draws by itself, which a
**    driver that keeps it on between points would pay;
**  - 6, 12 MHz, range 1;
**  - 7, 12 MHz, range 2, as the port runs at 12.
**
** The eight points on the SMPS, then the same eight on the LDO (PWR_CR3.REGSEL, which may
** change in any range, p. 333), and round again. D7, D8 and D12, the PPK2's D0, D1 and
** D2, give the point as a number (tools/ppk2_nucleo.py, PPK2_PHASES); the regulator is
** told by the order, the SMPS first, and by the line of each phase. D13 is left alone: it
** drives LD2, whose current JP4 counts.
**
** At the end of each phase a line goes out on USART1, to the virtual COM port of the
** ST-LINK, at 115,200 baud from the clock of the point: "DVFS3" and, in hexadecimal, the
** words 1 to 9 of Results. Results, in words: 0 marker, 1 the regulator (1 the SMPS),
** 2 the point, 3 the CRCs computed in the phase, 4 those wrong, 5 the cycles the change
** of range waited for R1RDY or R2RDY, 6 the cycles the booster took to BOOSTRDY, 7 the
** cycles the MSIS took to MSISRDY at its new divider, 8 the cycles REGS took to follow
** REGSEL, 9 1 if it never did (no SMPS on the package: REGSEL is then reserved, p. 367).
** Each count of cycles is of the DWT, at the frequency in effect while it waited: that of
** the point before for a raise, which changes the range before the clock, that of the
** point for a fall. 0 when the change did not take place.
**
** Interrupts stay masked: the MSI, should it leave its PLL mode, is not put back in it,
** and runs free within about 1 % until the next reset; the work counted says so.
** Linked into the flash (DVFSU3Flash.elf), the image survives the power cycle that the
** PPK2's figures need, as SleepU3Flash.
** Platform version: STM32U385 (NUCLEO-U385RG-Q).
*/

#include "Escapement.h"
#include "Escapement_LPTimer.h"
#include "BoardU3.h"

#define MARKER            0x44564633u   /* "DVF3" */
#ifndef DVFS_PHASE_S
   #define DVFS_PHASE_S   20u
#endif
#ifndef DVFS_PHASE_TICKS                /* DVFSU3Short: 1 ms a point, for Renode */
   #define DVFS_PHASE_TICKS (DVFS_PHASE_S * OS_LPTIMER_HZ)
#endif
#define PHASE_TICKS       DVFS_PHASE_TICKS
#define PHASE_PIN0        FLAG1_PIN     /* D7, PPK2 D0 */
#define PHASE_PIN1        FLAG2_PIN     /* D8, PPK2 D1 */
#define PHASE_PIN2        PROBE_PIN     /* D12, PPK2 D2 */
#define POINTS            8u
#define TURNS             2000000u      /* the bound on a wait for REGS */

#define RCC_BASE          0x40030C00
#define RCC_CR            *((volatile UINT32 *)(RCC_BASE + 0x000))
#define RCC_ICSCR1        *((volatile UINT32 *)(RCC_BASE + 0x008))
#define RCC_CFGR4         *((volatile UINT32 *)(RCC_BASE + 0x028))
#define RCC_APB2ENR       *((volatile UINT32 *)(RCC_BASE + 0x0A4))
#define RCC_CR_MSISRDY    (1u << 2)
#define RCC_ICSCR1_MSISDIV_MASK (3u << 29)
#define RCC_ICSCR1_MSISDIV(d) ((UINT32)(d) << 29)
#define RCC_CFGR4_BOOSTSEL_MASK 3u
#define RCC_CFGR4_BOOSTSEL_MSIS 1u      /* 00 when the booster is off, to save power (p. 429) */
#define RCC_APB2ENR_USART1EN (1u << 14)

#define PWR_BASE          0x40030800
#define PWR_CR3           *((volatile UINT32 *)(PWR_BASE + 0x08))
#define PWR_VOSR          *((volatile UINT32 *)(PWR_BASE + 0x0C))
#define PWR_SVMSR         *((volatile UINT32 *)(PWR_BASE + 0x3C))
#define PWR_CR3_REGSEL    (1u << 1)     /* p. 367 */
#define PWR_SVMSR_REGS    (1u << 1)     /* p. 376 */
#define PWR_VOSR_R1EN     (1u << 0)
#define PWR_VOSR_R2EN     (1u << 1)
#define PWR_VOSR_BOOSTEN  (1u << 8)
#define PWR_VOSR_R1RDY    (1u << 16)
#define PWR_VOSR_R2RDY    (1u << 17)
#define PWR_VOSR_BOOSTRDY (1u << 24)

#define FLASH_ACR         *((volatile UINT32 *)(0x40022000 + 0x00))
#define FLASH_ACR_LATENCY_MASK 0xFu

/* USART1 on PA9, alternate function 7, to the virtual COM port; clocked by PCLK2, the
** system clock here, so its divider follows the point (Escapement_UART.c). */
#define USART1_BASE       0x40013800
#define USART1_CR1        *((volatile UINT32 *)(USART1_BASE + 0x00))
#define USART1_BRR        *((volatile UINT32 *)(USART1_BASE + 0x0C))
#define USART1_ISR        *((volatile UINT32 *)(USART1_BASE + 0x1C))
#define USART1_TDR        *((volatile UINT32 *)(USART1_BASE + 0x28))
#define CR1_UE            (1u << 0)
#define CR1_TE            (1u << 3)
#define ISR_TC            (1u << 6)
#define ISR_TXE           (1u << 7)
#define BAUD_RATE         115200u
#define GPIOA_MODER       *((volatile UINT32 *)(0x42020000 + 0x00))
#define GPIOA_AFRH        *((volatile UINT32 *)(0x42020000 + 0x24))
#define TX_PIN            9u
#define AF_USART1         7u

/* The DWT's cycle counter, which TRCENA in DEMCR powers (ARMv8-M, C1.5, D1.2). */
#define DEMCR             *((volatile UINT32 *)0xE000EDFC)
#define DEMCR_TRCENA      (1u << 24)
#define DWT_CTRL          *((volatile UINT32 *)0xE0001000)
#define DWT_CYCCNT        *((volatile UINT32 *)0xE0001004)

typedef struct {
  UINT8 Mhz, Range, Boost, Div;         /* Div: MSISDIV, MSIRC0's 96 MHz divided by 2^Div */
} POINT;

static const POINT Points[POINTS] = {
  {96, 1, 1, 0}, {48, 1, 1, 1}, {48, 2, 1, 1}, {24, 1, 0, 2},
  {24, 2, 0, 2}, {24, 2, 1, 2}, {12, 1, 0, 3}, {12, 2, 0, 3}
};

volatile struct {
  UINT32 Marker, Smps, Point, Crcs, Wrong, RangeCycles, BoostCycles, MsisCycles,
         RegCycles, NoSmps;
} Results;

static UINT8 Buffer[256];

static void SetPhasePins(UINT32 point);
static void SetRegulator(UINT32 smps);
static void SetPoint(const POINT *p);
static UINT32 WaitStates(const POINT *p);
static void SetWaitStates(UINT32 ws);
static UINT32 Crc32(const UINT8 *data, UINT32 length);
static void Report(void);


int main(void)
{
  UINT32 smps = 1, point = 0, expected, elapsed, k;
  UINT16 last, now;
  _OSDisableInterrupts();
  OSInitializeSystemClocks();           // 12 MHz, range 2, the MSI locked on the LSE
  InitializeFlag(PHASE_PIN0);
  InitializeFlag(PHASE_PIN1);
  InitializeFlag(PHASE_PIN2);
  RCC_APB2ENR |= RCC_APB2ENR_USART1EN;
  (void)RCC_APB2ENR;
  GPIOA_AFRH = (GPIOA_AFRH & ~(0xFu << 4 * (TX_PIN - 8))) | AF_USART1 << 4 * (TX_PIN - 8);
  GPIOA_MODER = (GPIOA_MODER & ~(3u << 2 * TX_PIN)) | 2u << 2 * TX_PIN;
  DEMCR |= DEMCR_TRCENA;
  DWT_CTRL |= 1;
  Results.Marker = MARKER;
  if (!OSInitLPTimer())                 // no LSE: no time to measure the phases by
     while (TRUE);
  for (k = 0; k < sizeof Buffer; k += 1)
     Buffer[k] = (UINT8)(k * 37u + 11u);
  expected = Crc32(Buffer,sizeof Buffer);
  while (TRUE) {
     Results.Smps = smps;
     Results.Point = point;
     Results.Crcs = Results.Wrong = 0;
     Results.RangeCycles = Results.BoostCycles = Results.MsisCycles = Results.RegCycles = 0;
     SetPhasePins(point);
     SetRegulator(smps);
     SetPoint(&Points[point]);
     USART1_CR1 = 0;
     USART1_BRR = (Points[point].Mhz * 1000000u + BAUD_RATE / 2) / BAUD_RATE;
     USART1_CR1 = CR1_UE | CR1_TE;
     last = OSGetLPTimer();
     for (elapsed = 0; elapsed < PHASE_TICKS; last = now) {
        if (Crc32(Buffer,sizeof Buffer) != expected)
           Results.Wrong += 1;
        Results.Crcs += 1;
        now = OSGetLPTimer();
        elapsed += (UINT16)(now - last);
     }
     Report();
     point += 1;
     if (point == POINTS) {
        point = 0;
        smps ^= 1;
     }
  }
} /* end of main */


/* SetPhasePins: The point as a number on D7, D8 and D12. */
static void SetPhasePins(UINT32 point)
{
  if (point & 1u) SetPin(PHASE_PIN0); else ClearPin(PHASE_PIN0);
  if (point & 2u) SetPin(PHASE_PIN1); else ClearPin(PHASE_PIN1);
  if (point & 4u) SetPin(PHASE_PIN2); else ClearPin(PHASE_PIN2);
} /* end of SetPhasePins */


/* SetRegulator: The SMPS or the LDO, REGS awaited, bounded: on a package without the SMPS
** REGSEL is reserved and REGS never follows, which word 9 then says, the LDO kept. */
static void SetRegulator(UINT32 smps)
{
  UINT32 start, turns;
  if (((PWR_CR3 & PWR_CR3_REGSEL) != 0) == (smps != 0) || (smps && Results.NoSmps))
     return;
  start = DWT_CYCCNT;
  PWR_CR3 = smps ? PWR_CR3 | PWR_CR3_REGSEL : PWR_CR3 & ~PWR_CR3_REGSEL;
  for (turns = 0; ((PWR_SVMSR & PWR_SVMSR_REGS) != 0) != (smps != 0) && turns < TURNS;
       turns += 1);
  Results.RegCycles = DWT_CYCCNT - start;
  if (turns == TURNS) {
     Results.NoSmps = 1;
     PWR_CR3 &= ~PWR_CR3_REGSEL;
  }
} /* end of SetRegulator */


/* SetPoint: From whatever point the chip is at to p, by the two sequences of 9.3.3. Up:
** range 1, R1RDY, the booster, BOOSTRDY, the wait states, the frequency. Down: the
** frequency, the wait states, the booster off at 24 MHz or less, range 2. R1EN and R2EN
** change only while the range they select is ready (p. 369), and the booster's clock, the
** MSIS, is chosen before it starts and left only once it has stopped (p. 334, 429). The
** wait states are the larger of the two points' while the clock changes. */
static void SetPoint(const POINT *p)
{
  UINT32 start, ws = WaitStates(p);
  if (p->Range == 1 && (PWR_VOSR & PWR_VOSR_R1EN) == 0) {
     while ((PWR_VOSR & (PWR_VOSR_R1RDY | PWR_VOSR_R2RDY)) != PWR_VOSR_R2RDY);
     start = DWT_CYCCNT;
     PWR_VOSR = (PWR_VOSR & ~(PWR_VOSR_R1EN | PWR_VOSR_R2EN)) | PWR_VOSR_R1EN;
     while ((PWR_VOSR & PWR_VOSR_R1RDY) == 0);
     Results.RangeCycles = DWT_CYCCNT - start;
  }
  if (p->Boost && (PWR_VOSR & PWR_VOSR_BOOSTEN) == 0) {
     RCC_CFGR4 = (RCC_CFGR4 & ~RCC_CFGR4_BOOSTSEL_MASK) | RCC_CFGR4_BOOSTSEL_MSIS;
     start = DWT_CYCCNT;
     PWR_VOSR |= PWR_VOSR_BOOSTEN;
     while ((PWR_VOSR & PWR_VOSR_BOOSTRDY) == 0);
     Results.BoostCycles = DWT_CYCCNT - start;
  }
  if (ws > (FLASH_ACR & FLASH_ACR_LATENCY_MASK))
     SetWaitStates(ws);
  if ((RCC_ICSCR1 & RCC_ICSCR1_MSISDIV_MASK) != RCC_ICSCR1_MSISDIV(p->Div)) {
     while ((RCC_CR & RCC_CR_MSISRDY) == 0);
     RCC_ICSCR1 = (RCC_ICSCR1 & ~RCC_ICSCR1_MSISDIV_MASK) | RCC_ICSCR1_MSISDIV(p->Div);
     start = DWT_CYCCNT;
     while ((RCC_CR & RCC_CR_MSISRDY) == 0);
     Results.MsisCycles = DWT_CYCCNT - start;
  }
  if (ws < (FLASH_ACR & FLASH_ACR_LATENCY_MASK))
     SetWaitStates(ws);
  if (!p->Boost && (PWR_VOSR & PWR_VOSR_BOOSTEN) != 0) {
     PWR_VOSR &= ~PWR_VOSR_BOOSTEN;
     while ((PWR_VOSR & PWR_VOSR_BOOSTRDY) != 0);
     RCC_CFGR4 &= ~RCC_CFGR4_BOOSTSEL_MASK;
  }
  if (p->Range == 2 && (PWR_VOSR & PWR_VOSR_R1EN) != 0) {
     while ((PWR_VOSR & PWR_VOSR_R1RDY) == 0);
     start = DWT_CYCCNT;
     PWR_VOSR = (PWR_VOSR & ~(PWR_VOSR_R1EN | PWR_VOSR_R2EN)) | PWR_VOSR_R2EN;
     while ((PWR_VOSR & PWR_VOSR_R2RDY) == 0);
     Results.RangeCycles = DWT_CYCCNT - start;
  }
} /* end of SetPoint */


/* WaitStates: The flash's wait states of a point, table 43 for the STM32U375/385 (p. 227):
** 0 up to 32 MHz in range 1, 16 in range 2, one more for each step of that size. */
static UINT32 WaitStates(const POINT *p)
{
  UINT32 step = p->Range == 1 ? 32u : 16u;
  return (p->Mhz - 1u) / step;
} /* end of WaitStates */


/* SetWaitStates: LATENCY written and read back until it holds (7.3.3, p. 228). */
static void SetWaitStates(UINT32 ws)
{
  FLASH_ACR = (FLASH_ACR & ~FLASH_ACR_LATENCY_MASK) | ws;
  while ((FLASH_ACR & FLASH_ACR_LATENCY_MASK) != ws);
} /* end of SetWaitStates */


/* Crc32: The CRC-32 of the buffer, bit by bit, a computation whose result says whether
** the core computed it right (SleepPico.c). */
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


/* Report: "DVFS3" and the words 1 to 9 of Results in hexadecimal, sent at the point's
** clock and left to drain before the next change. */
static void Report(void)
{
  static const char digits[] = "0123456789ABCDEF";
  const char *text = "DVFS3";
  UINT32 w, k, value;
  while (*text != '\0') {
     while ((USART1_ISR & ISR_TXE) == 0);
     USART1_TDR = (UINT32)*text++;
  }
  for (w = 1; w <= 9; w += 1) {
     value = ((volatile UINT32 *)&Results)[w];
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
