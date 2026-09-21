/* Copyright (c) 2026 Bertrand Hurst. All rights reserved.
** Escapement - Lightweight Power-Aware Real-Time OS.
** Derived from prior work; see LICENSE and NOTICE at the root of this repository.
*/
/* File BoardL1.h: The register accesses every STM32L-Discovery example repeats, written
** against the CMSIS device header rather than the ST Standard Peripheral Library, of
** which the examples used about fifteen functions. References are to RM0038.
*/

#ifndef BOARD_L1_H_
#define BOARD_L1_H_

#include "Escapement.h"
#include "stm32l1xx.h"

#define PIN(n) ((UINT16)(1u << (n)))

/* BoardInitClock: Runs the core at 32 MHz from the 16 MHz internal oscillator, through
** the PLL (x4, /2), with AHB and both APB buses at 32 MHz. ST's current SystemInit()
** leaves the clocks alone, and the timer prescalers in Escapement_Config.h assume this
** frequency. */
static inline void BoardInitClock(void)
{
  /* Back to the reset configuration, in case the debugger restarted without a reset */
  RCC->CR |= RCC_CR_MSION;
  RCC->CFGR &= 0x88FFC00C;   /* SW, HPRE, PPRE1, PPRE2, MCOSEL and MCOPRE */
  RCC->CR &= ~(RCC_CR_HSION | RCC_CR_HSEON | RCC_CR_CSSON | RCC_CR_PLLON);
  RCC->CR &= ~RCC_CR_HSEBYP;
  RCC->CFGR &= ~(RCC_CFGR_PLLSRC | RCC_CFGR_PLLMUL | RCC_CFGR_PLLDIV);
  RCC->CIR = 0;
  /* Start the internal oscillator */
  RCC->CR |= RCC_CR_HSION;
  while ((RCC->CR & RCC_CR_HSIRDY) == 0);
  /* 64-bit flash access, prefetch, one wait state */
  FLASH->ACR |= FLASH_ACR_ACC64;
  FLASH->ACR |= FLASH_ACR_PRFTEN;
  FLASH->ACR |= FLASH_ACR_LATENCY;
  /* Voltage range 1 (1.8 V), required above 16 MHz */
  RCC->APB1ENR |= RCC_APB1ENR_PWREN;
  PWR->CR = PWR_CR_VOS_0;
  while (PWR->CSR & PWR_CSR_VOSF);
  RCC->CFGR |= RCC_CFGR_HPRE_DIV1 | RCC_CFGR_PPRE2_DIV1 | RCC_CFGR_PPRE1_DIV1;
  RCC->CFGR |= RCC_CFGR_PLLSRC_HSI | RCC_CFGR_PLLMUL4 | RCC_CFGR_PLLDIV2;
  RCC->CR |= RCC_CR_PLLON;
  while ((RCC->CR & RCC_CR_PLLRDY) == 0);
  RCC->CFGR = (RCC->CFGR & ~RCC_CFGR_SW) | RCC_CFGR_SW_PLL;
  while ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_PLL);
  SystemCoreClockUpdate();
}

/* BoardStopTimerInDebug: Freezes a timer while the debugger halts the core, so that the
** kernel does not see time pass at a breakpoint (DBGMCU_APB1_FZ and DBGMCU_APB2_FZ). */
static inline void BoardStopTimerInDebug(UINT32 timer)
{
  if (timer == OS_IO_TIM2)       DBGMCU->APB1FZ |= DBGMCU_APB1_FZ_DBG_TIM2_STOP;
  else if (timer == OS_IO_TIM3)  DBGMCU->APB1FZ |= DBGMCU_APB1_FZ_DBG_TIM3_STOP;
  else if (timer == OS_IO_TIM4)  DBGMCU->APB1FZ |= DBGMCU_APB1_FZ_DBG_TIM4_STOP;
  #ifdef OS_IO_TIM5   /* only on the parts that have it */
     else if (timer == OS_IO_TIM5) DBGMCU->APB1FZ |= DBGMCU_APB1_FZ_DBG_TIM5_STOP;
  #endif
  else if (timer == OS_IO_TIM9)  DBGMCU->APB2FZ |= DBGMCU_APB2_FZ_DBG_TIM9_STOP;
  else if (timer == OS_IO_TIM10) DBGMCU->APB2FZ |= DBGMCU_APB2_FZ_DBG_TIM10_STOP;
  else if (timer == OS_IO_TIM11) DBGMCU->APB2FZ |= DBGMCU_APB2_FZ_DBG_TIM11_STOP;
}

/* BoardKeepDebugInSleep: Keeps the debugger connected while the core sleeps. */
static inline void BoardKeepDebugInSleep(void)
{
  DBGMCU->CR |= DBGMCU_CR_DBG_SLEEP;
}

/* BoardEnablePort: Clocks a GPIO port. Ports A to E and H sit 0x400 apart from GPIOA and
** their enable bits follow in the same order in RCC_AHBENR. */
static inline void BoardEnablePort(GPIO_TypeDef *port)
{
  RCC->AHBENR |= 1u << (((UINT32)port - GPIOA_BASE) / 0x400);
}

/* BoardInitOutputs: Makes the given pins push-pull outputs at the highest speed, without
** pull resistor. */
static inline void BoardInitOutputs(GPIO_TypeDef *port, UINT16 pins)
{
  UINT32 i;
  BoardEnablePort(port);
  for (i = 0; i < 16; i += 1)
     if (pins & PIN(i)) {
        port->MODER = (port->MODER & ~(3u << (2 * i))) | (1u << (2 * i));
        port->OTYPER &= ~PIN(i);
        port->OSPEEDR |= 3u << (2 * i);
        port->PUPDR &= ~(3u << (2 * i));
     }
}

#endif /* BOARD_L1_H_ */
