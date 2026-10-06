/* Copyright (c) 2026 Bertrand Hurst. All rights reserved.
** Escapement - Lightweight Power-Aware Real-Time OS.
** Distributed under the terms of LICENSE at the root of this repository.
*/
/* File Escapement_LPTimer.h: LPTIM1 of the STM32U385 counting the 32.768 kHz crystal of
** the board, the LSE, a clock that runs on through Stop 2 where TIM2, the kernel's, stops
** (RM0487 rev. 3, RCC_CCIPR3, p. 471; LPTIM, table 453, p. 1897): a counter of 16 bits,
** wrapping every 2 s, in steps of 30.5 us, and a compare that raises a flag, which wakes
** the chip from Stop 2. The driver of the STM32U5 port at the addresses of this chip.
** Platform version: STM32U385 (NUCLEO-U385RG-Q), any STM32U375/385.
*/

#ifndef ESCAPEMENT_LPTIMER_H
#define ESCAPEMENT_LPTIMER_H

#define OS_LPTIMER_HZ 32768u

/* OSInitLPTimer: Starts LPTIM1 counting the LSE, from 0 to 0xFFFF and round again. The
** LSE must run and reach the peripherals, which OSInitializeSystemClocks sees to; returns
** FALSE if it does not. */
BOOL OSInitLPTimer(void);

/* OSGetLPTimer: The count of LPTIM1. It runs on a clock of its own: two reads that agree
** are taken for it (RM0487, LPTIM_CNT, p. 1940). */
UINT16 OSGetLPTimer(void);

/* OSSetLPTimerCompare: The count at which the compare flag of LPTIM1 rises, 0xFFFF taken
** as 0xFFFE; the write takes some cycles of the LSE to reach the counter, which this
** waits for. */
void OSSetLPTimerCompare(UINT16 count);

/* OSLPTimerCompared: TRUE once the count has reached the compare set last, the flag then
** cleared. */
BOOL OSLPTimerCompared(void);

#endif /* ESCAPEMENT_LPTIMER_H */
