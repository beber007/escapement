/* Copyright (c) 2026 Bertrand Hurst. All rights reserved.
** Escapement - Lightweight Power-Aware Real-Time OS.
** Distributed under the terms of LICENSE at the root of this repository.
*/
/* File BoardF4.h: The register accesses every STM32F4-Discovery example repeats, written
** against the CMSIS device header rather than the ST Standard Peripheral Library, of
** which the examples used about fifteen functions. References are to RM0090.
*/

#ifndef BOARD_F4_H_
#define BOARD_F4_H_

#include "Escapement.h"
#include "stm32f4xx.h"

#define PIN(n) ((UINT16)(1u << (n)))

/* BoardInitClock: Runs the core at 168 MHz from the 8 MHz crystal of the board, through
** the main PLL (M = 8, N = 336, P = 2, Q = 7), with AHB at 168 MHz and both APB buses at
** 42 MHz. ST's current SystemInit() leaves the clocks alone, and the timer prescalers in
** Escapement_Config.h assume these frequencies. Should the crystal not start, the core
** stays on the 16 MHz internal oscillator. */
static inline void BoardInitClock(void)
{
  UINT32 timeout;
  /* Back to the reset configuration, in case the debugger restarted without a reset */
  RCC->CR |= RCC_CR_HSION;
  RCC->CFGR = 0;
  RCC->CR &= ~(RCC_CR_HSEON | RCC_CR_CSSON | RCC_CR_PLLON);
  RCC->PLLCFGR = 0x24003010;
  RCC->CR &= ~RCC_CR_HSEBYP;
  RCC->CIR = 0;
  /* Start the crystal oscillator */
  RCC->CR |= RCC_CR_HSEON;
  for (timeout = 0x500; (RCC->CR & RCC_CR_HSERDY) == 0 && timeout > 0; timeout -= 1);
  if (RCC->CR & RCC_CR_HSERDY) {
     /* Regulator in scale 1 mode, required up to 168 MHz */
     RCC->APB1ENR |= RCC_APB1ENR_PWREN;
     (void)RCC->APB1ENR;  // the clock reaches PWR a few cycles later (ES0182 rev 19, 2.2.13)
     PWR->CR |= PWR_CR_VOS;
     RCC->CFGR |= RCC_CFGR_HPRE_DIV1 | RCC_CFGR_PPRE2_DIV4 | RCC_CFGR_PPRE1_DIV4;
     RCC->PLLCFGR = 8 | (336 << RCC_PLLCFGR_PLLN_Pos) | (((2 >> 1) - 1) << RCC_PLLCFGR_PLLP_Pos) |
                    RCC_PLLCFGR_PLLSRC_HSE | (7 << RCC_PLLCFGR_PLLQ_Pos);
     RCC->CR |= RCC_CR_PLLON;
     while ((RCC->CR & RCC_CR_PLLRDY) == 0);
     /* Instruction and data caches, 5 wait states */
     FLASH->ACR = FLASH_ACR_ICEN | FLASH_ACR_DCEN | FLASH_ACR_LATENCY_5WS;
     RCC->CFGR = (RCC->CFGR & ~RCC_CFGR_SW) | RCC_CFGR_SW_PLL;
     while ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_PLL);
  }
  SystemCoreClockUpdate();
}

/* BoardStopTimerInDebug: Freezes a timer while the debugger halts the core, so that the
** kernel does not see time pass at a breakpoint (DBGMCU_APB1_FZ and DBGMCU_APB2_FZ). */
static inline void BoardStopTimerInDebug(UINT32 timer)
{
  if (timer == OS_IO_TIM2)       DBGMCU->APB1FZ |= DBGMCU_APB1_FZ_DBG_TIM2_STOP;
  else if (timer == OS_IO_TIM3)  DBGMCU->APB1FZ |= DBGMCU_APB1_FZ_DBG_TIM3_STOP;
  else if (timer == OS_IO_TIM4)  DBGMCU->APB1FZ |= DBGMCU_APB1_FZ_DBG_TIM4_STOP;
  else if (timer == OS_IO_TIM5)  DBGMCU->APB1FZ |= DBGMCU_APB1_FZ_DBG_TIM5_STOP;
  else if (timer == OS_IO_TIM12) DBGMCU->APB1FZ |= DBGMCU_APB1_FZ_DBG_TIM12_STOP;
  else if (timer == OS_IO_TIM13) DBGMCU->APB1FZ |= DBGMCU_APB1_FZ_DBG_TIM13_STOP;
  else if (timer == OS_IO_TIM14) DBGMCU->APB1FZ |= DBGMCU_APB1_FZ_DBG_TIM14_STOP;
  else if (timer == OS_IO_TIM1)  DBGMCU->APB2FZ |= DBGMCU_APB2_FZ_DBG_TIM1_STOP;
  else if (timer == OS_IO_TIM8)  DBGMCU->APB2FZ |= DBGMCU_APB2_FZ_DBG_TIM8_STOP;
  else if (timer == OS_IO_TIM9)  DBGMCU->APB2FZ |= DBGMCU_APB2_FZ_DBG_TIM9_STOP;
  else if (timer == OS_IO_TIM10) DBGMCU->APB2FZ |= DBGMCU_APB2_FZ_DBG_TIM10_STOP;
  else if (timer == OS_IO_TIM11) DBGMCU->APB2FZ |= DBGMCU_APB2_FZ_DBG_TIM11_STOP;
}

/* BoardKeepDebugInSleep: Keeps the debugger connected while the core sleeps. */
static inline void BoardKeepDebugInSleep(void)
{
  DBGMCU->CR |= DBGMCU_CR_DBG_SLEEP;
}

/* BoardEnablePort: Clocks a GPIO port. The ports sit 0x400 apart from GPIOA and their
** enable bits follow in the same order in RCC_AHB1ENR. */
static inline void BoardEnablePort(GPIO_TypeDef *port)
{
  RCC->AHB1ENR |= 1u << (((UINT32)port - GPIOA_BASE) / 0x400);
  (void)RCC->AHB1ENR;     // the port clocked before it is written (ES0182 rev 19, 2.2.13)
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

#endif /* BOARD_F4_H_ */
