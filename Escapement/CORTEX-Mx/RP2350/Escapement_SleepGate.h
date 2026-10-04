/* Copyright (c) 2026 Bertrand Hurst. All rights reserved.
** Escapement - Lightweight Power-Aware Real-Time OS.
** Distributed under the terms of LICENSE at the root of this repository.
*/
/* File Escapement_SleepGate.h: The idle task of the RP2350 in SLEEP rather than a plain
** WFI, the clocks gated but those the kernel and the application keep, and PLL_SYS
** stopped through each sleep (RP2350 datasheet, 6.5.2, SLEEP_EN0 and SLEEP_EN1;
** docs/roadmap.md, item 5), transposed from the RP2040's.
** Platform version: RP2350 (Raspberry Pi Pico 2).
*/

#ifndef ESCAPEMENT_SLEEPGATE_H
#define ESCAPEMENT_SLEEPGATE_H

/* Clocks an application may keep through SLEEP, bits of SLEEP_EN0 and SLEEP_EN1: those of
** a UART that must receive while the idle task sleeps, for one. */
#define OS_SLEEP_EN1_UART0 ((1u << 23) | (1u << 22))  /* CLK_SYS_UART0, CLK_PERI_UART0 */
#define OS_SLEEP_EN1_UART1 ((1u << 25) | (1u << 24))  /* CLK_SYS_UART1, CLK_PERI_UART1 */

/* OSInitSleepGate: From main, after OSInitializeSystemClocks and before
** OSStartMultitasking. The chip enters SLEEP when both cores wait in WFI or WFE with deep
** sleep enabled on both (datasheet, 6.5.2): this enables it on core 0, where the idle
** task waits, and parks core 1 on it, so that an image calling it cannot run core 1 for
** itself. During SLEEP only the clocks of keep0 and keep1 run, and TIMER0 and its tick,
** always kept; an interrupt ends it. Each sleep also stops PLL_SYS, clk_sys on the
** crystal meanwhile, and locks it again before the interrupt is taken, which delays it
** by that lock (docs/roadmap.md, item 5). */
void OSInitSleepGate(UINT32 keep0, UINT32 keep1);

#endif /* ESCAPEMENT_SLEEPGATE_H */
