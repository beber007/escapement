/* Copyright (c) 2026 Bertrand Hurst. All rights reserved.
** Escapement - Lightweight Power-Aware Real-Time OS.
** Derived from prior work; see LICENSE and NOTICE at the root of this repository.
*/
/* File Escapement_Types.h: Common data types for the host test build.
**
** Same as the Cortex-Mx version except for the pointer-sized integer: the kernel stores
** pointers in UINTPTR when it links its queues, and a host pointer does not fit in 32
** bits. The load-linked and store-conditional pair on that type therefore gets its own
** definition instead of aliasing the 32-bit one.
*/
#ifndef _ESCAPEMENT_TYPES_H_
#define _ESCAPEMENT_TYPES_H_

#include <stdint.h>

typedef unsigned char BOOL;
typedef uint8_t  UINT8;
typedef int8_t   INT8;
typedef uint16_t UINT16;
typedef int16_t  INT16;
typedef uint32_t UINT32;
typedef int32_t  INT32;
#ifndef NULL
  #define NULL 0
#endif
#ifndef TRUE
  #define TRUE 1
#endif
#ifndef FALSE
  #define FALSE 0
#endif

typedef uintptr_t UINTPTR;

#endif /* _ESCAPEMENT_TYPE_H_ */
