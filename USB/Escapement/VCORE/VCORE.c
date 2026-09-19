/* Copyright (c) 2026 Bertrand Hurst. All rights reserved.
** Escapement - Lightweight Power-Aware Real-Time OS.
** Derived from prior work; see LICENSE and NOTICE at the root of this repository.
*/
/* File VCORE.c: .
** Version identifier: April 2010
*/
/* (c)2009 by Texas Instruments Incorporated, All Rights Reserved. */

#include "..\Escapement_msp430.h"
#include "VCORE.h"

static void SetVCoreUp(UINT8 level);
static void SetVCoreDown (UINT8 level);


void SetVCoreUp (UINT8 level)
{
  // Open PMM registers for write access
  PMMCTL0_H = 0xA5;
  // Set SVS/SVM high side new level
  SVSMHCTL = SVSHE + SVSHRVL0 * level + SVMHE + SVSMHRRL0 * level;
  // Set SVM low side to new level
  SVSMLCTL = SVMLFP + SVSLE + SVMLE + SVSMLRRL0 * level;
  // Wait till SVM is settled
  while ((PMMIFG & SVSMLDLYIFG) == 0);
  // Clear already set flags
  PMMIFG &= ~(SVMLVLRIFG + SVMLIFG);
  // Set VCore to new level
  PMMCTL0_L = PMMCOREV0 * level;
  // Wait till new level reached
  if (PMMIFG & SVMLIFG)
  while ((PMMIFG & SVMLVLRIFG) == 0);
  // Set SVS/SVM low side to new level
  SVSMLCTL = SVSLE + SVSLRVL0 * level + SVMLE + SVSMLRRL0 * level;
  // Lock PMM registers for write access
  PMMCTL0_H = 0x00;
}


void SetVCoreDown (UINT8 level)
{
  PMMCTL0_H = 0xA5;                         // Open PMM module registers for write access
  SVSMLCTL = SVSLE + SVSLRVL0 * level + SVMLE + SVSMLRRL0 * level;
  // Set SVS/SVM high side new level
  SVSMHCTL = SVSHE + SVSHRVL0 * level + SVMHE + SVSMHRRL0 * level;
  while ((PMMIFG & SVSMLDLYIFG) == 0);      // Wait till SVM is settled (Delay)
  PMMCTL0 = 0xA500 | (level * PMMCOREV0);   // Set VCore to requested level
  while (PMMIFG & SVMLIFG)
    PMMIFG &= ~(SVMLIFG);                   // Wait till SVM will not be set anymore
  PMMCTL0_H = 0x00;                         // Lock PMM module registers for write access
}


void SetVCore(UINT8 level)
{
  UINT16 actlevel;
  level &= PMMCOREV_3;                       // Set Mask for Max. level
  actlevel = (PMMCTL0 & PMMCOREV_3);         // Get actuel VCore
  while (level != actlevel)
    if (level > actlevel)
      SetVCoreUp(++actlevel);
    else
      SetVCoreDown(--actlevel);
}


