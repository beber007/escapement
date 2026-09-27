/* Copyright (c) 2026 Bertrand Hurst. All rights reserved.
** Escapement - Lightweight Power-Aware Real-Time OS.
** Distributed under the terms of LICENSE at the root of this repository.
*/
/* File Escapement_LPTimer.c: LPTIM1 of the STM32U585 on the LSE (Escapement_LPTimer.h).
** RM0456 rev. 7, chapter 58 and RCC; registers and bits as in STMicroelectronics,
** cmsis-device-u5, stm32u575xx.h.
**
** The order of the writes is the manual's: CFGR only with the timer disabled, ARR, CCR1
** and DIER only with it enabled, each then waited for through its flag, ARROK, CMP1OK or
** DIEROK, before the next write to it. LPTIM1 is never disabled by its ENABLE bit, which
** may leave its interrupt stuck and keep the chip from Stop (ES0499, 2.17.1): it is reset
** through the RCC instead.
** Platform version: STM32U585 (Arduino UNO Q), any STM32U5.
*/

#include "Escapement.h"
#include "Escapement_LPTimer.h"

#define LPTIM1_BASE          0x46004400
#define LPTIM_ISR            *((volatile UINT32 *)(LPTIM1_BASE + 0x00))
#define LPTIM_ICR            *((volatile UINT32 *)(LPTIM1_BASE + 0x04))
#define LPTIM_CFGR           *((volatile UINT32 *)(LPTIM1_BASE + 0x0C))
#define LPTIM_CR             *((volatile UINT32 *)(LPTIM1_BASE + 0x10))
#define LPTIM_CCR1           *((volatile UINT32 *)(LPTIM1_BASE + 0x14))
#define LPTIM_ARR            *((volatile UINT32 *)(LPTIM1_BASE + 0x18))
#define LPTIM_CNT            *((volatile UINT32 *)(LPTIM1_BASE + 0x1C))
#define LPTIM_CC1IF          (1u << 0)
#define LPTIM_CMP1OK         (1u << 3)
#define LPTIM_ARROK          (1u << 4)
#define LPTIM_CR_ENABLE      (1u << 0)
#define LPTIM_CR_CNTSTRT     (1u << 2)

#define RCC_BASE             0x46020C00
#define RCC_BDCR             *((volatile UINT32 *)(RCC_BASE + 0xF0))
#define RCC_APB3RSTR         *((volatile UINT32 *)(RCC_BASE + 0x80))
#define RCC_APB3ENR          *((volatile UINT32 *)(RCC_BASE + 0xA8))
#define RCC_CCIPR3           *((volatile UINT32 *)(RCC_BASE + 0xE8))
#define RCC_BDCR_LSERDY      (1u << 1)
#define RCC_LPTIM1           (1u << 11)    /* in APB3RSTR and APB3ENR */
#define LPTIM1SEL_MASK       (3u << 10)
#define LPTIM1SEL_LSE        (3u << 10)


BOOL OSInitLPTimer(void)
{
  if ((RCC_BDCR & RCC_BDCR_LSERDY) == 0)
     return FALSE;
  RCC_CCIPR3 = (RCC_CCIPR3 & ~LPTIM1SEL_MASK) | LPTIM1SEL_LSE;
  RCC_APB3ENR |= RCC_LPTIM1;
  (void)RCC_APB3ENR;                      // the clock runs before the timer is written
  RCC_APB3RSTR |= RCC_LPTIM1;             // from its reset state, however it was left
  RCC_APB3RSTR &= ~RCC_LPTIM1;
  LPTIM_CFGR = 0;                         // internal clock, no prescaler, no trigger
  LPTIM_CR = LPTIM_CR_ENABLE;
  LPTIM_ARR = 0xFFFF;
  while ((LPTIM_ISR & LPTIM_ARROK) == 0);
  LPTIM_ICR = LPTIM_ARROK;
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
