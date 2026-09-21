/* Copyright (c) 2026 Bertrand Hurst. All rights reserved.
** Escapement - Lightweight Power-Aware Real-Time OS.
** Derived from prior work; see LICENSE and NOTICE at the root of this repository.
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
  /* The device header names the APB2 bits DBGMCU_APB1_FZ_*, hence the plain values. */
  else if (timer == OS_IO_TIM1)  DBGMCU->APB2FZ |= 1u << 0;
  else if (timer == OS_IO_TIM8)  DBGMCU->APB2FZ |= 1u << 1;
  else if (timer == OS_IO_TIM9)  DBGMCU->APB2FZ |= 1u << 16;
  else if (timer == OS_IO_TIM10) DBGMCU->APB2FZ |= 1u << 17;
  else if (timer == OS_IO_TIM11) DBGMCU->APB2FZ |= 1u << 18;
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
