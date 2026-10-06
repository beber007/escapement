/* Copyright (c) 2026 Bertrand Hurst. All rights reserved.
** Escapement - Lightweight Power-Aware Real-Time OS.
** Distributed under the terms of LICENSE at the root of this repository.
*/
/* File Escapement_LPTimer.c: LPTIM1 of the STM32U385 on the LSE (Escapement_LPTimer.h),
** the driver of the STM32U5 port. RM0487 rev. 3, chapter 42 (LPTIM) and 10 (RCC); the
** addresses, offsets and bits checked against STMicroelectronics, cmsis-device-u3,
** stm32u385xx.h: LPTIM1 at 0x40044400 (memory map, p. 113), its interrupt 67 (table 134,
** p. 630), its registers at the U5's offsets (42.7, p. 1922-1945).
**
** The order of the writes is the manual's: CFGR only with the timer disabled (42.4.13,
** p. 1909), ARR, CCR1 and DIER only with it enabled, each then waited for through its
** flag, ARROK, CMP1OK or DIEROK, before the next write to it, any earlier one leading to
** "unpredictable results" (42.4.11, p. 1908; LPTIMx_DIER, p. 1933). LPTIM1 is never
** disabled by its ENABLE bit, which may leave its interrupt stuck and keep the chip out of
** Stop (ES0626 rev. 3, 2.11.1, both revisions): it is reset through the RCC instead, as
** the erratum's workaround says. Writing DIER clears the flag it enables (2.11.3, no
** workaround): DIER is written once, before any compare.
** Platform version: STM32U385 (NUCLEO-U385RG-Q), any STM32U375/385.
*/

#include "Escapement.h"
#include "Escapement_LPTimer.h"

#define LPTIM1_BASE          0x40044400
#define LPTIM_ISR            *((volatile UINT32 *)(LPTIM1_BASE + 0x00))
#define LPTIM_ICR            *((volatile UINT32 *)(LPTIM1_BASE + 0x04))
#define LPTIM_DIER           *((volatile UINT32 *)(LPTIM1_BASE + 0x08))
#define LPTIM_CFGR           *((volatile UINT32 *)(LPTIM1_BASE + 0x0C))
#define LPTIM_CR             *((volatile UINT32 *)(LPTIM1_BASE + 0x10))
#define LPTIM_CCR1           *((volatile UINT32 *)(LPTIM1_BASE + 0x14))
#define LPTIM_ARR            *((volatile UINT32 *)(LPTIM1_BASE + 0x18))
#define LPTIM_CNT            *((volatile UINT32 *)(LPTIM1_BASE + 0x1C))
/* LPTIMx_ISR and LPTIMx_ICR in output compare mode (p. 1923-1925, 1928), DIER (p. 1932). */
#define LPTIM_CC1IF          (1u << 0)
#define LPTIM_CMP1OK         (1u << 3)
#define LPTIM_ARROK          (1u << 4)
#define LPTIM_DIEROK         (1u << 24)
#define LPTIM_CC1IE          (1u << 0)
#define LPTIM_CR_ENABLE      (1u << 0)    /* LPTIM_CR, p. 1938 */
#define LPTIM_CR_CNTSTRT     (1u << 2)

#define RCC_BASE             0x40030C00
#define RCC_APB3RSTR         *((volatile UINT32 *)(RCC_BASE + 0x080))   /* p. 440 */
#define RCC_APB3ENR          *((volatile UINT32 *)(RCC_BASE + 0x0A8))   /* p. 449 */
#define RCC_CCIPR3           *((volatile UINT32 *)(RCC_BASE + 0x108))   /* p. 470-471 */
#define RCC_BDCR             *((volatile UINT32 *)(RCC_BASE + 0x110))   /* p. 471-474 */
#define RCC_BDCR_LSERDY      (1u << 1)
#define RCC_BDCR_LSESYSRDY   (1u << 11)
#define RCC_LPTIM1           (1u << 11)    /* in APB3RSTR and APB3ENR */
#define LPTIM1SEL_MASK       (3u << 10)
#define LPTIM1SEL_LSE        (3u << 10)


BOOL OSInitLPTimer(void)
{
  /* The LSE reaches a peripheral other than the RTC only through LSESYSEN (10.2.3,
  ** p. 404-405), which OSInitializeSystemClocks sets for the MSI's PLL mode: both ready,
  ** or LPTIM1 would count nothing. */
  if ((RCC_BDCR & (RCC_BDCR_LSERDY | RCC_BDCR_LSESYSRDY)) !=
      (RCC_BDCR_LSERDY | RCC_BDCR_LSESYSRDY))
     return FALSE;
  /* The LSE as its kernel clock, one that runs in Stop 2 (RCC_CCIPR3, p. 471). */
  RCC_CCIPR3 = (RCC_CCIPR3 & ~LPTIM1SEL_MASK) | LPTIM1SEL_LSE;
  RCC_APB3ENR |= RCC_LPTIM1;
  (void)RCC_APB3ENR;                      // 2 cycles of the bus before the timer (p. 410)
  RCC_APB3RSTR |= RCC_LPTIM1;             // from its reset state, however it was left
  RCC_APB3RSTR &= ~RCC_LPTIM1;
  LPTIM_CFGR = 0;                         // internal clock, no prescaler, no trigger
  /* ARR is written once the timer is enabled; its write completes, ARROK, only once the
  ** two cycles of the counter that ENABLE takes have passed (p. 1909). */
  LPTIM_CR = LPTIM_CR_ENABLE;
  LPTIM_ARR = 0xFFFF;
  while ((LPTIM_ISR & LPTIM_ARROK) == 0);
  LPTIM_ICR = LPTIM_ARROK;
  /* The compare interrupt enabled once, its flag clear: writing DIER clears the flag it
  ** enables (ES0626, 2.11.3), and an enable written after its flag rose would not assert
  ** the interrupt (RM0487, 42.6, p. 1920). Only the NVIC decides whether it is taken. */
  LPTIM_DIER = LPTIM_CC1IE;
  while ((LPTIM_ISR & LPTIM_DIEROK) == 0);
  LPTIM_ICR = LPTIM_DIEROK;
  LPTIM_CR = LPTIM_CR_ENABLE | LPTIM_CR_CNTSTRT;   // continuous
  return TRUE;
} /* end of OSInitLPTimer */


UINT16 OSGetLPTimer(void)
{
  UINT32 first, second = LPTIM_CNT;
  do {
     first = second;
     second = LPTIM_CNT;
  } while (first != second);
  return (UINT16)first;
} /* end of OSGetLPTimer */


void OSSetLPTimerCompare(UINT16 count)
{
  /* ARR, 0xFFFF, must stay strictly above the compare (LPTIM_ARR, p. 1940): at 0xFFFF it
  ** comes one tick sooner, which a caller asking for a time ahead can bear. */
  if (count == 0xFFFFu)
     count = 0xFFFEu;
  LPTIM_ICR = LPTIM_CMP1OK | LPTIM_CC1IF;
  LPTIM_CCR1 = count;
  while ((LPTIM_ISR & LPTIM_CMP1OK) == 0);
  LPTIM_ICR = LPTIM_CMP1OK;
} /* end of OSSetLPTimerCompare */


BOOL OSLPTimerCompared(void)
{
  if ((LPTIM_ISR & LPTIM_CC1IF) == 0)
     return FALSE;
  LPTIM_ICR = LPTIM_CC1IF;
  return TRUE;
} /* end of OSLPTimerCompared */
