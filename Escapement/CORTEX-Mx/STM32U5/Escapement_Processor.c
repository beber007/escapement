/* Copyright (c) 2026 Bertrand Hurst. All rights reserved.
** Escapement - Lightweight Power-Aware Real-Time OS.
** Distributed under the terms of LICENSE at the root of this repository.
*/
/* File Escapement_Processor.c: Clock set-up of the STM32U575 (RM0456, the reference
** manual of the STM32U5: RCC, PWR, FLASH and ICACHE; registers and bits as in
** STMicroelectronics, cmsis-device-u5, stm32u575xx.h).
**
** Reset leaves the core on the MSIS at 4 MHz, in voltage range 4. The system clock goes
** to 160 MHz, the maximum of the chip, from PLL1 fed by the MSIS: 4 MHz x 80 = 320 MHz at
** the VCO, divided by 2. That needs voltage range 1 with the EPOD booster, whose clock is
** the input of PLL1 and must be selected before the booster is enabled, then 4 wait
** states on the flash, and a first step through an AHB prescaler of 2, which ST's
** library takes above 80 MHz to soften the jump in current. The instruction cache hides
** the wait states.
**
** The MSIS runs free here, within about 1 % of its frequency; locked on the 32.768 kHz
** crystal of the board (MSIPLLEN) it would be far closer, which the board's timings will
** want and which has yet to be measured.
**
** Platform version: STM32U575 (NUCLEO-U575ZI-Q).
*/

#include "Escapement.h"

#define RCC_BASE             0x46020C00
#define RCC_CR               *((volatile UINT32 *)(RCC_BASE + 0x00))
#define RCC_CFGR1            *((volatile UINT32 *)(RCC_BASE + 0x1C))
#define RCC_CFGR2            *((volatile UINT32 *)(RCC_BASE + 0x20))
#define RCC_PLL1CFGR         *((volatile UINT32 *)(RCC_BASE + 0x28))
#define RCC_PLL1DIVR         *((volatile UINT32 *)(RCC_BASE + 0x34))
#define RCC_AHB3ENR          *((volatile UINT32 *)(RCC_BASE + 0x94))

#define RCC_CR_PLL1ON        (1u << 24)
#define RCC_CR_PLL1RDY       (1u << 25)
#define RCC_CFGR1_SW_PLL1    3u
#define RCC_CFGR1_SWS_MASK   (3u << 2)
#define RCC_CFGR1_SWS_PLL1   (3u << 2)
#define RCC_CFGR2_HPRE_MASK  0xFu
#define RCC_CFGR2_HPRE_DIV2  0x8u
#define RCC_AHB3ENR_PWREN    (1u << 2)

/* PLL1CFGR: source MSIS, input range 4 to 8 MHz, M = 1, booster prescaler 1, output R
** enabled. */
#define PLL1SRC_MSIS         (1u << 0)
#define PLL1RGE_4_8MHZ       (0u << 2)
#define PLL1M(m)             (((m) - 1u) << 8)
#define PLL1MBOOST_DIV1      (0u << 12)
#define PLL1REN              (1u << 18)
#define PLL1CFGR_FIELDS      (0x3u | (0x3u << 2) | (0xFu << 8) | (0xFu << 12) | (1u << 18))

/* PLL1DIVR: N and R as their value less one; P and Q are left as reset sets them. */
#define PLL1N(n)             ((n) - 1u)
#define PLL1R(r)             (((r) - 1u) << 24)
#define PLL1DIVR_FIELDS      (0x1FFu | (0x7Fu << 24))

#define PWR_BASE             0x46020800
#define PWR_VOSR             *((volatile UINT32 *)(PWR_BASE + 0x0C))
#define PWR_VOSR_VOS_RANGE1  (3u << 16)
#define PWR_VOSR_BOOSTEN     (1u << 18)
#define PWR_VOSR_VOSRDY      (1u << 15)
#define PWR_VOSR_BOOSTRDY    (1u << 14)

#define FLASH_ACR            *((volatile UINT32 *)(0x40022000 + 0x00))
#define FLASH_ACR_LATENCY_MASK 0xFu
#define FLASH_ACR_PRFTEN     (1u << 8)
#define FLASH_WAIT_STATES    4u   /* range 1, 128 to 160 MHz */

#define ICACHE_CR            *((volatile UINT32 *)(0x40030400 + 0x00))
#define ICACHE_CR_EN         (1u << 0)


void OSInitializeSystemClocks(void)
{
  volatile UINT32 i;
  RCC_AHB3ENR |= RCC_AHB3ENR_PWREN;
  (void)RCC_AHB3ENR;                       // the enable takes effect before PWR is written
  /* The input of PLL1, which is also the booster's clock, before the booster. */
  RCC_PLL1CFGR = (RCC_PLL1CFGR & ~PLL1CFGR_FIELDS) |
                 PLL1SRC_MSIS | PLL1RGE_4_8MHZ | PLL1M(1) | PLL1MBOOST_DIV1 | PLL1REN;
  /* Range 1 and the booster, then wait for both. */
  PWR_VOSR = (PWR_VOSR & ~(3u << 16)) | PWR_VOSR_VOS_RANGE1 | PWR_VOSR_BOOSTEN;
  while ((PWR_VOSR & (PWR_VOSR_VOSRDY | PWR_VOSR_BOOSTRDY)) !=
         (PWR_VOSR_VOSRDY | PWR_VOSR_BOOSTRDY));
  /* Wait states before the clock rises; read back until they hold. */
  FLASH_ACR = (FLASH_ACR & ~FLASH_ACR_LATENCY_MASK) | FLASH_WAIT_STATES | FLASH_ACR_PRFTEN;
  while ((FLASH_ACR & FLASH_ACR_LATENCY_MASK) != FLASH_WAIT_STATES);
  /* 4 MHz x 80 / 2 = 160 MHz. */
  RCC_PLL1DIVR = (RCC_PLL1DIVR & ~PLL1DIVR_FIELDS) | PLL1N(80) | PLL1R(2);
  RCC_CR |= RCC_CR_PLL1ON;
  while ((RCC_CR & RCC_CR_PLL1RDY) == 0);
  /* Onto PLL1 through an AHB prescaler of 2 first, then 1. */
  RCC_CFGR2 = (RCC_CFGR2 & ~RCC_CFGR2_HPRE_MASK) | RCC_CFGR2_HPRE_DIV2;
  RCC_CFGR1 = (RCC_CFGR1 & ~3u) | RCC_CFGR1_SW_PLL1;
  while ((RCC_CFGR1 & RCC_CFGR1_SWS_MASK) != RCC_CFGR1_SWS_PLL1);
  for (i = 0; i < 100; i += 1);            // some microseconds at 80 MHz
  RCC_CFGR2 &= ~RCC_CFGR2_HPRE_MASK;
  ICACHE_CR |= ICACHE_CR_EN;
} /* end of OSInitializeSystemClocks */
