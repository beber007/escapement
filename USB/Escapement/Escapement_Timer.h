/* Copyright (c) 2026 Bertrand Hurst. All rights reserved.
** Escapement - Lightweight Power-Aware Real-Time OS.
** Derived from prior work; see LICENSE and NOTICE at the root of this repository.
*/
/* File Escapement_Timer.h:
** Version identifier: August 2009
*/

/* Claude : Comentaires à revoir dans tout le fichier */

#ifndef ESCAPEMENT_TIMER_H
#define ESCAPEMENT_TIMER_H

#include "Escapement.h"

/* _OSGenerateSoftTimerInterrupt: */
void _OSGenerateSoftTimerInterrupt(void);

/* _OSEnableSoftTimerInterrupt: */
void _OSEnableSoftTimerInterrupt(void);

/* _OSInitializeTimer: */
void _OSInitializeTimer(void);

/* _OSStartTimer: */
void _OSStartTimer(void);

/* _OSUpdateTime: */
void _OSUpdateTime(void);

/* _OSGetTime: */
INT32 _OSGetTime(void);

/* _OSShiftTime: 
** Parameter: (INT32) 
*/
void _OSShiftTime(INT32 shiftTimeLimit);

/* _OSSetTimer:
** Parameter: (INT32)
*/ 
void _OSSetTimer(INT32 nextArrival);

/* _OSSleep: . */
void _OSSleep(void);

#endif /* ESCAPEMENT_TIMER_H */
