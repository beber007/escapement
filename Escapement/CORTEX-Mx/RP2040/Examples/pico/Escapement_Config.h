/* Copyright (c) 2026 Bertrand Hurst. All rights reserved.
** Escapement - Lightweight Power-Aware Real-Time OS.
** Derived from prior work; see LICENSE and NOTICE at the root of this repository.
*/
/* File Escapement_Config.h: Escapement configuration for a Raspberry Pi Pico.
** Platform version: RP2040.
*/

#ifndef ESCAPEMENT_CONFIG_H_
#define ESCAPEMENT_CONFIG_H_

#define RP2040
#define CORTEX_M0


/* Measures the cost of a scheduling round, read back over SWD by
** tools/measure_cost.sh. Off by default: it adds work to the critical path of
** the kernel, and it clears TIMER_DBGPAUSE so that the clock keeps running
** through debugger halts — which is what a measurement needs and the opposite
** of what debugging needs. */
//#define ESCAPEMENT_MEASURE_SCHEDULING_COST


/* Select which Escapement version to use. */
#define ESCAPEMENT_VERSION_HARD
//#define ESCAPEMENT_VERSION_SOFT
//#define ESCAPEMENT_VERSION_HARD_PA


/* Maximum size of the permanent allocations performed by OSMalloc while main is running.
** Also caps the run-time stack, which takes all the remaining RAM.
** (See function OSMalloc in Escapement_CortexMx.c) */
#define OSMALLOC_INTERNAL_HEAP_SIZE  2048


/* The Cortex-M0+ of the RP2040 offers 4 priority levels, 0 being the highest. Escapement
** reserves two of them:
**   PendSV  — context switching, must sit at the lowest level (3);
**   SysTick — software timer interrupt that schedules the tasks, just above (2).
** The hardware timer takes level 0 or 1, leaving the other to the application. */
#define LOWEST_PRIORITY_LEVEL  3

/* Priority of the hardware timer, which must be higher than that of SysTick. */
#define TIMER_PRIORITY  (UINT8)0

#endif /* ESCAPEMENT_CONFIG_H_ */
