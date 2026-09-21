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
