/* Copyright (c) 2026 Bertrand Hurst. All rights reserved.
** Escapement - Lightweight Power-Aware Real-Time OS.
** Derived from prior work; see LICENSE and NOTICE at the root of this repository.
*/
/* File DMA.h: .
** Version identifier: April 2010
*/
/* (c)2009 by Texas Instruments Incorporated, All Rights Reserved. */

#ifndef DMA_H_
#define DMA_H_

//this function init a DMA channel
void DMAInit(UINT8 channel);

// this functions starts DMA transfer to/from USB memory into/from RAM
// Using DMA0
// Support only for data in <64k memory area.
void * DMAMemcpy0(void * dest, const void *  source, UINT8 count);

// this functions starts DMA transfer to/from USB memory into/from RAM
// Using DMA1
// Support only for data in <64k memory area.
void * DMAMemcpy1(void * dest, const void * source, UINT8 count);

// this functions starts DMA transfer to/from USB memory into/from RAM
// Using DMA2
// Support only for data in <64k memory area.
void * DMAMemcpy2(void * dest, const void * source, UINT8 count);

#endif /*DMA_H_*/
