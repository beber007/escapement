/* Copyright (c) 2006-2012 MIS Institute of the HEIG-VD affiliated to the University of
** Applied Sciences of Western Switzerland. All rights reserved.
** Permission to use, copy, modify, and distribute this software and its documentation
** for any purpose, without fee, and without written agreement is hereby granted, pro-
** vided that the above copyright notice, the following three sentences and the authors
** appear in all copies of this software and in the software where it is used.
** IN NO EVENT SHALL THE MIS INSTITUTE NOR THE HEIG-VD NOR THE UNIVERSITY OF APPLIED
** SCIENCES OF WESTERN SWITZERLAND BE LIABLE TO ANY PARTY FOR DIRECT, INDIRECT, SPECIAL,
** INCIDENTAL, OR CONSEQUENTIAL DAMAGES ARISING OUT OF THE USE OF THIS SOFTWARE AND ITS
** DOCUMENTATION, EVEN IF THE MIS INSTITUTE OR THE HEIG-VD OR THE UNIVERSITY OF APPLIED
** SCIENCES OF WESTERN SWITZERLAND HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
** THE MIS INSTITUTE, THE HEIG-VD AND THE UNIVERSITY OF APPLIED SCIENCES OF WESTERN SWIT-
** ZERLAND SPECIFICALLY DISCLAIMS ANY WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
** IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE. THE SOFT-
** WARE PROVIDED HEREUNDER IS ON AN "AS IS" BASIS, AND THE MIS INSTITUTE NOR THE HEIG-VD
** AND NOR THE UNIVERSITY OF APPLIED SCIENCES OF WESTERN SWITZERLAND HAVE NO OBLIGATION
** TO PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR MODIFICATIONS.
** Authors: MIS-TIC
**
** Escapement - Lightweight Power-Aware Real-Time OS, derived from ZottaOS.
** Modifications Copyright (c) 2026 Bertrand Hurst, distributed under the same terms;
** see LICENSE and NOTICE at the root of this repository.
*/
/* File Escapement_Config.h: Escapement configuration for a Raspberry Pi Pico 2.
** Platform version: RP2350.
*/

#ifndef ESCAPEMENT_CONFIG_H_
#define ESCAPEMENT_CONFIG_H_


/* Scheduling algorithm. EscapementHard.h defines the names before reading this choice and
** falls back to deadline-monotonic when an application says nothing. */
#ifndef SCHEDULER_REAL_TIME_MODE   /* make SCHEDULER=... builds the other one */
   #define SCHEDULER_REAL_TIME_MODE EARLIEST_DEADLINE_FIRST
#endif

#define RP2350
#define CORTEX_M33


/* Measures the cost of a scheduling round, read back over SWD by
** tools/measure_cost.sh. Off by default: it adds work to the critical path of
** the kernel, and it clears TIMER_DBGPAUSE so that the clock keeps running
** through debugger halts — which is what a measurement needs and the opposite
** of what debugging needs. */
//#define ESCAPEMENT_MEASURE_SCHEDULING_COST


/* Select which Escapement version to use: the hard real-time kernel, unless the build asks
** for the soft one with make KERNEL=SOFT. The power-aware kernel is not ported yet. */
#if !defined(ESCAPEMENT_VERSION_SOFT) && !defined(ESCAPEMENT_VERSION_HARD_PA)
   #define ESCAPEMENT_VERSION_HARD
#endif


/* Maximum size of the permanent allocations performed by OSMalloc while main is running.
** Also caps the run-time stack, which takes all the remaining RAM.
** (See function OSMalloc in Escapement_CortexMx.c) */
#define OSMALLOC_INTERNAL_HEAP_SIZE  2048


/* The Cortex-M33 of the RP2350 implements 4 bits of interrupt priority, 16 levels (pico-
** sdk, hardware/irq.h). PRIGROUP 3 gives all 4 to preemption, as on the STM32F4; the
** generic layer then puts SysTick, the software timer interrupt that schedules the tasks,
** at level 14 and PendSV, the context switch, at 15, the lowest. */
#define PRIGROUP  (UINT32)3

/* Level of the hardware timer, which must preempt SysTick: 0, the highest. */
#define TIMER_PRIORITY  (UINT8)0

#endif /* ESCAPEMENT_CONFIG_H_ */
