/* Copyright (c) 2026 Bertrand Hurst. All rights reserved.
** Escapement - Lightweight Power-Aware Real-Time OS.
** Distributed under the terms of LICENSE at the root of this repository.
*/
/* File Escapement_Config.h: Kernel configuration for the host test build.
**
** No CORTEX_Mx is declared here: this build has no assembler context switch, wider
** pointers and a different task control block layout, so the checks that tie the two
** together do not apply.
*/

#ifndef ESCAPEMENT_CONFIG_H_
#define ESCAPEMENT_CONFIG_H_

#define ESCAPEMENT_HOST

/* Kernel variant: HOST_KERNEL_SOFT selects the soft real-time kernel, with its (m,k)-firm
** tasks, HOST_KERNEL_PA the power-aware one, and the hard real-time kernel is built
** otherwise. */
#if defined(HOST_KERNEL_SOFT)
   #define ESCAPEMENT_VERSION_SOFT
#elif defined(HOST_KERNEL_PA)
   #define ESCAPEMENT_VERSION_HARD_PA
#else
   #define ESCAPEMENT_VERSION_HARD
#endif

/* Scheduling algorithm. The names come from the kernel header, which defines them before
** reading this choice; HOST_SCHEDULER lets the test build both ways. */
#ifndef HOST_SCHEDULER
   #define HOST_SCHEDULER 1
#endif
#define SCHEDULER_REAL_TIME_MODE HOST_SCHEDULER

#define OSMALLOC_INTERNAL_HEAP_SIZE 65536

#endif /* ESCAPEMENT_CONFIG_H_ */
