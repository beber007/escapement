/* Copyright (c) 2026 Bertrand Hurst. All rights reserved.
** Escapement - Lightweight Power-Aware Real-Time OS.
** Derived from prior work; see LICENSE and NOTICE at the root of this repository.
*/
/* File Escapement_Types.h: Contains common data types that are compiler specific.
** Platform version: All MSP430 and CC430 microcontrollers.
** Version identifier: March 2012
*/
#ifndef _ESCAPEMENT_TYPES_H_
#define _ESCAPEMENT_TYPES_H_

typedef unsigned char BOOL;
typedef unsigned char UINT8;
typedef signed char INT8;
typedef unsigned int UINT16;
typedef signed int INT16;
typedef unsigned long UINT32;
typedef signed long INT32;
#ifndef NULL
  #define NULL 0
#endif
#ifndef TRUE
  #define TRUE 1
#endif
#ifndef FALSE
  #define FALSE 0
#endif

/* Equivalence between address and unsigned to allow arithmetic and bitwise operations */
typedef UINT16 UINTPTR;
#define OSUINTPTR_LL OSUINT16_LL
#define OSUINTPTR_SC OSUINT16_SC

#endif /* _ESCAPEMENT_TYPE_H_ */
