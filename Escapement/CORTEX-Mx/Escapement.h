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
/* File Escapement.h: User API for all Escapement kernels. The kernel selection is imported from
**                 Escapement_Config.h file.
** Platform version: All Cortex-Mx based microcontrollers.
** Version date: March 2012
*/

#ifndef _ESCAPEMENT_H_
#define _ESCAPEMENT_H_

#include "Escapement_Config.h"     /* Import the kernel version */

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

#ifndef _ASM_

#include "Escapement_Types.h"      /* Type definitions */
#include "Escapement_Interrupts.h" /* Interrupt priorities and ISR index definitions */
#include "Escapement_Processor.h"
#include "Escapement_Trace.h"        /* Trace points, empty unless ESCAPEMENT_TRACE */

#endif /* _ASM_ */

#if defined(ESCAPEMENT_VERSION_HARD)
   #include "EscapementHard.h"
#elif defined(ESCAPEMENT_VERSION_SOFT)
   #include "EscapementSoft.h"
#elif defined(ESCAPEMENT_VERSION_HARD_PA)
   #include "EscapementHardPA.h"
#else
   #error ESCAPEMENT_VERSION undefined!
#endif

#endif /* _ESCAPEMENT_H_ */
