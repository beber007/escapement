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
*/
/* File ZottaOS.h: User API for all ZottaOS kernels. The kernel selection is imported from
**                 ZottaOS_Config.h file.
** Platform version: All CORTEX-Mx microcontrollers.
** Version date: March 2012
** Authors: MIS-TIC
*/

#ifndef _ZOTTAOS_H_
#define _ZOTTAOS_H_

#include "ZottaOS_Config.h"     /* Import the kernel version */

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

#include "ZottaOS_Types.h"      /* Type definitions */
#include "ZottaOS_Interrupts.h" /* Interrupt priorities and ISR index definitions */
#include "ZottaOS_Processor.h"

#endif /* _ASM_ */

#if defined(ZOTTAOS_VERSION_HARD)
   #include "ZottaOSHard.h"
#elif defined(ZOTTAOS_VERSION_SOFT)
   #include "ZottaOSSoft.h"
#else
   #error ZOTTAOS_VERSION undefined!
#endif

#endif /* _ZOTTAOS_H_ */
