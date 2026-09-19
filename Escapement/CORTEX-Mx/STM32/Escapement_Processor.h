/* Copyright (c) 2026 Bertrand Hurst. All rights reserved.
** Escapement - Lightweight Power-Aware Real-Time OS.
** Derived from prior work; see LICENSE and NOTICE at the root of this repository.
*/
/* File Escapement_Processor.h: Defined architecture specifics for Escapement kernels. Note that
**                           these definitions are not intended for the user.
** Platform version: All STM32 microcontrollers.
** Version identifier: March 2012
*/

#ifndef ESCAPEMENT_PROCESSOR_H_
#define ESCAPEMENT_PROCESSOR_H_

#include "Escapement_Config.h"
#include "Escapement_CortexMx.h"

#ifdef ESCAPEMENT_VERSION_HARD_PA

#define OS_4MHZ_SPEED  0
#define OS_16MHZ_SPEED 1
#define OS_32MHZ_SPEED 2

#define OS_MAX_SPEED 2

/* OSInitProcessorSpeed: */
void OSInitProcessorSpeed(void);

/* OSGetCurrentSpeed: */
UINT8 OSGetProcessorSpeed(void);

#endif /* ESCAPEMENT_VERSION_HARD_PA */

#endif /* ESCAPEMENT_PROCESSOR_H_ */
