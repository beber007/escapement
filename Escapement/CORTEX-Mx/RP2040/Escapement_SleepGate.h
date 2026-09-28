/* Copyright (c) 2026 Bertrand Hurst. All rights reserved.
** Escapement - Lightweight Power-Aware Real-Time OS.
** Distributed under the terms of LICENSE at the root of this repository.
*/
/* File Escapement_SleepGate.h: The idle task of the RP2040 in SLEEP rather than a plain
** WFI, the clocks gated but those the kernel and the application keep (RP2040 datasheet,
** 2.11.2 and 2.15.7, SLEEP_EN0 and SLEEP_EN1; docs/roadmap.md, item 1).
** Platform version: RP2040 (Raspberry Pi Pico).
*/

#ifndef ESCAPEMENT_SLEEPGATE_H
#define ESCAPEMENT_SLEEPGATE_H

/* Clocks an application may keep through SLEEP, bits of SLEEP_EN0 and SLEEP_EN1: those of
** a UART that must receive while the idle task sleeps, for one. */
#define OS_SLEEP_EN1_UART0 ((1u << 7) | (1u << 6))   /* CLK_SYS_UART0, CLK_PERI_UART0 */
#define OS_SLEEP_EN1_UART1 ((1u << 9) | (1u << 8))   /* CLK_SYS_UART1, CLK_PERI_UART1 */

/* OSInitSleepGate: From main, after OSInitializeSystemClocks and before
** OSStartMultitasking. The chip enters SLEEP when both cores wait in WFI or WFE with deep
** sleep enabled on both (datasheet, 2.11.5.1): this enables it on core 0, where the idle
** task waits, and parks core 1 on it, so that an image calling it cannot run core 1 for
** itself. During SLEEP only the clocks of keep0 and keep1 run, and the timer of the
** kernel, always kept, whose tick SLEEP leaves alone; an interrupt ends it. */
void OSInitSleepGate(UINT32 keep0, UINT32 keep1);

#endif /* ESCAPEMENT_SLEEPGATE_H */
