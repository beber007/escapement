/* Copyright (c) 2026 Bertrand Hurst. All rights reserved.
** Escapement - Lightweight Power-Aware Real-Time OS.
** Derived from prior work; see LICENSE and NOTICE at the root of this repository.
*/
/* File VCORE.h: .
** Version identifier: April 2010
*/
/* (c)2009 by Texas Instruments Incorporated, All Rights Reserved. */

#ifndef VCORE_H_
#define VCORE_H_

void SetVCore(UINT8 level);

inline UINT8 GetVCore(void)
{
  return (PMMCTL0 & PMMCOREV_3); // Get actuel VCore
}

#endif /*VCORE_H_*/
