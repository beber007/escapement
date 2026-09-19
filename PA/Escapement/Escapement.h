/* Copyright (c) 2026 Bertrand Hurst. All rights reserved.
** Escapement - Lightweight Power-Aware Real-Time OS.
** Derived from prior work; see LICENSE and NOTICE at the root of this repository.
*/
/* File Escapement.h: .
** Version date: April 2010
*/
#ifndef ESCAPEMENT_H_
#define ESCAPEMENT_H_

#include "msp430.h"
#include "Escapement_msp430.h"

#if ESCAPEMENT_VERSION == ESCAPEMENT_VERSION_HARD
   #include "EscapementHard.h"
#elif ESCAPEMENT_VERSION == ESCAPEMENT_VERSION_SOFT
   #include "EscapementSoft.h"
#elif ESCAPEMENT_VERSION == ESCAPEMENT_VERSION_HARD_PA
   #include "EscapementHardPA.h"
#elif ESCAPEMENT_VERSION == ESCAPEMENT_VERSION_SOFT_PA
   #include "EscapementSoftPA.h"
#else
   #error ESCAPEMENT_VERSION undefined!
#endif

#endif /*ESCAPEMENT_H_*/
