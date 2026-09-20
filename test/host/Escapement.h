/* Copyright (c) 2026 Bertrand Hurst. All rights reserved.
** Escapement - Lightweight Power-Aware Real-Time OS.
** Derived from prior work; see LICENSE and NOTICE at the root of this repository.
*/
/* File Escapement.h: Entry header of the host test build.
**
** The Cortex-Mx header of the same name cannot be reused: it includes its neighbours with
** quotes, which resolve in its own directory first, so the types and the processor layer
** it picks up are always the ones of the target. This is the same header for the host.
*/
#ifndef _ESCAPEMENT_H_
#define _ESCAPEMENT_H_

#include "Escapement_Config.h"
#define DEBUG_MODE
#include "Escapement_Types.h"
#include "Escapement_Interrupts.h"
#include "Escapement_Processor.h"
#include "Escapement_Timer.h"

#if defined(ESCAPEMENT_VERSION_HARD)
   #include "EscapementHard.h"
#else
   #error the host test build only covers the hard real-time kernel
#endif

#endif /* _ESCAPEMENT_H_ */
