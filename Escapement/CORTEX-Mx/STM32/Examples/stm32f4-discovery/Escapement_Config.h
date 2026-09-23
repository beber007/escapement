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
/* File Escapement_Config.h: Escapement configuration file.
** Platform version: STM32F4-Discovery (STM32F407VG).
** Version identifier: March 2012
*/

#ifndef ESCAPEMENT_CONFIG_H_
#define ESCAPEMENT_CONFIG_H_


/* Scheduling algorithm. EscapementHard.h defines the names before reading this choice and
** falls back to deadline-monotonic when an application says nothing. */
#ifndef SCHEDULER_REAL_TIME_MODE   /* make SCHEDULER=... builds the other one */
   #define SCHEDULER_REAL_TIME_MODE EARLIEST_DEADLINE_FIRST
#endif

/* Target device: the STM32F407VG of the STM32F4-Discovery board, the one STM32 kept as a
** regression target. The port still holds the branches of the other families it was
** written for; none of them is built or tested. */
#define STM32F407XX
#define STM32F4XXXX
#define CORTEX_M4


/* Select which Escapement version to use: the hard real-time kernel, unless the build asks
** for the soft one with make KERNEL=SOFT. */
#ifndef ESCAPEMENT_VERSION_SOFT
   #define ESCAPEMENT_VERSION_HARD
#endif


/* The following symbol defines the maximum size of permanent allocations performed by
** OSMalloc while main is in execution. This value can be increased if more than 512
** bytes are needed, and decreased if the run-time stack overflows before or when
** OSStartMultitasking is called. Note that there is no point optimizing this value as
** the run-time stack pointer is readjusted within OSStartMultitasking so that the stack
** can take all the remaining RAM memory not occupied by the dynamic memory allocations
** and the application's global variables.
** (Also see function OSMalloc in Escapement_CortexMx.c) */
#define OSMALLOC_INTERNAL_HEAP_SIZE  2048


/* The nested vector interrupt controller (NVIC) under Cortex-M3 or Cortex-M4 allows
** dynamic prioritization of interrupts with up to 256 levels that can be arranged
** into priority level groups where each group can be preempted. The NVIC is config-
** urable by the pair (A,B), where A denotes the number of bits used to set the number
** of groups (number of priorities), and B is the number of bits used for the number
** of subpriorities within a group. A + B = 8 and A = [2..7]. Note that Escapement re-
** quires 3 priority groups for itself. From highest to lowest priority, these are:
** one for the peripheral hardware timer, one for the software timer interrupt and a
** final one for the PendSV used to finalize a context switch.
** In the following, PRIGROUP determines the setting of the (A,B) pair, where PRIGROUP
** is a value in the range [0..5] yielding A = 7 - PRIGROUP and B = 8 - A.
** STM32 based Cortex-M3 and -M4 cores has 16 distinct priority levels (=2^4) and
** PRIGROUP must be in the range [3..5] which then gives A = 7 - PRIGROUP and B = 4
** - A. */
#define PRIGROUP  (UINT32)3


/* Internal interval timer used by Escapement */
/* You can choose the timer for Escapement from the following choices: OS_IO_TIM1 to
** OS_IO_TIM5 and OS_IO_TIM8 to OS_IO_TIM17. Basic timers 6 and 7 may not be used be-
** cause they do not have a comparator. If the selected timer is not implemented in the
** part number of the STM32 series used, a compiler error will indicate that the corres-
** ponding symbol does not exist.*/
#define ESCAPEMENT_TIMER  OS_IO_TIM2

/* Define the interval-timer prescaler */
#define ESCAPEMENT_TIMER_PRESCALER  81

/* Defines the priority group and level of the interval-timer. The timer's priority must
** be higher than that of SysTick. See _OSResetHandler(). */
#define TIMER_PRIORITY  (UINT8)0
#if defined(CORTEX_M3) || defined(CORTEX_M4)
   #define TIMER_SUB_PRIORITY  (UINT8)0
#endif

#endif /* ESCAPEMENT_CONFIG_H_ */
