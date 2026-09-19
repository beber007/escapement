/* Copyright (c) 2026 Bertrand Hurst. All rights reserved.
** Escapement - Lightweight Power-Aware Real-Time OS.
** Derived from prior work; see LICENSE and NOTICE at the root of this repository.
*/
/* File Escapement.h: User API for all Escapement kernels. The kernel selection is imported from
**                 Escapement_msp430XXX.h or Escapement_cc430XXX.h generated file.
** Platform version: All MSP430 and CC430 microcontrollers.
** Version date: March 2012
*/

#ifndef _ESCAPEMENT_H_
#define _ESCAPEMENT_H_

#include "Escapement_Types.h"  /* Type definitions */
#include "msp430.h"         /* Hardware specifics */
#include "Escapement_msp430.h" /* Application configuration */

/* DEBUGGING HELP -------------------------------------------------------------------- */
/* There are 3 error sources that are not trivial to locate when developing an applica-
** tion. The first is detecting when an application runs out of memory with function
** OSMalloc, and the second is when a (periodic or event-driven) task overruns its period
** (under Deadline Monotonic Scheduling) or its allotted load (under EDF) and is immedi-
** ately rescheduled. This last case is caused when the instantaneous load of a processor
** is higher than 100% and can be corrected by increasing the task's period. The third
** and final error source is caused when an interrupt for a peripheral device occurs for
** which no ISR handler was configured.
** To help the developer, these errors go into an infinite loop if DEBUG_MODE is defined;
** the developer may then tap into the error source with a JTAG. */
#define DEBUG_MODE

#if defined(ESCAPEMENT_VERSION_HARD)
   #include "../EscapementHard.h"
#elif defined(ESCAPEMENT_VERSION_SOFT)
   #include "../EscapementSoft.h"
#elif defined(ESCAPEMENT_VERSION_HARD_PA)
   #include "../EscapementHardPA.h"
#elif defined(ESCAPEMENT_VERSION_SOFT_PA)
   #include "../EscapementSoftPA.h"
#else
   #error ESCAPEMENT_VERSION undefined!
#endif

#endif /* _ESCAPEMENT_H_ */
