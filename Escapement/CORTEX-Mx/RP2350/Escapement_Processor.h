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
/* File Escapement_Processor.h: Clock set-up of the RP2350. As on the RP2040, the timer is
** fed by a tick independent of the core clock: changing the frequency does not move the
** kernel's time base. The power-aware kernel is not ported yet: its DVFS driver needs the
** switching core regulator of this chip, which the RP2040 driver does not drive.
** Platform version: RP2350 (Raspberry Pi Pico 2).
*/

#ifndef ESCAPEMENT_PROCESSOR_H
#define ESCAPEMENT_PROCESSOR_H

#include "Escapement_Config.h"
#include "Escapement_CortexMx.h"

#ifdef ESCAPEMENT_VERSION_HARD_PA
   #error "the power-aware kernel is not ported to the RP2350 yet (docs/roadmap.md)"
#endif

/* Switches the clock tree onto the 12 MHz crystal and the system clock to 150 MHz. To be
** called first, before the timer and before any peripheral whose rate depends on the
** clock. */
void OSInitializeSystemClocks(void);

#endif /* ESCAPEMENT_PROCESSOR_H */
