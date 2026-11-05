/* Copyright (c) 2026 Bertrand Hurst. All rights reserved.
** Escapement - Lightweight Power-Aware Real-Time OS.
** Distributed under the terms of LICENSE at the root of this repository.
*/
/* File Escapement_Atomic.c: The LL/SC pairs of the kernels' API (EscapementHard.h) on the
** RISC-V "A" extension, the counterpart of CORTEX-Mx/Escapement_Atomic.c.
**
** lr.w reserves a word and sc.w stores into it only if the reservation holds. The
** Hazard3 drops it as it takes a trap and as it returns from one (hazard3_core.v): an
** interrupt between the two makes the SC fail, as CLREX does on the Cortex-M33. Checked
** on the Pico 2 on 2026-11-05. A store of the other core to the word is to drop it too,
** through the RP2350's global exclusive monitor: still to be checked with core 1 (#77,
** step 4).
**
** The "A" extension has no load-reserved of a byte or of a halfword. Those of 8 and 16
** bits reserve the word that holds them. Their SC reads that word again, sets its byte
** or halfword, and stores it back with sc.w: what the same code wrote into the word
** between the LL and the SC is kept, read with it, and a store of an interrupt or of
** the other core in between makes the SC fail. The kernels use the 8-bit pair alone, on
** the Reading field of a 3-slot buffer, whose word holds Latest, and on the power-aware
** kernel's DMSlackInterrupt.
** Platform version: Hazard3 (RV32IMAC), RP2350.
*/

#include "Escapement.h"

/* The word holding memAddr, and the shift of its byte or halfword in it: the RISC-V is
** little-endian. */
#define WORD_OF(memAddr)  ((UINT32 *)((UINTPTR)(memAddr) & ~3u))
#define SHIFT_OF(memAddr) (((UINTPTR)(memAddr) & 3u) << 3)

static inline UINT32 LoadReserved(UINT32 *memAddr)
{
  UINT32 tmp;
  __asm volatile ("lr.w %0, (%1)" : "=r"(tmp) : "r"(memAddr) : "memory");
  return tmp;
} /* end of LoadReserved */

/* StoreConditional: sc.w writes 0 on success; TRUE is returned then, as on the Cortex-M. */
static inline BOOL StoreConditional(UINT32 *memAddr, UINT32 newVal)
{
  UINT32 failed;
  __asm volatile ("sc.w %0, %2, (%1)" : "=&r"(failed) : "r"(memAddr),"r"(newVal) : "memory");
  return failed == 0;
} /* end of StoreConditional */

/* StoreConditionalPart: The SC of a byte or a halfword, of mask its width, into the word
** reserved by the LL before. */
static inline BOOL StoreConditionalPart(const void *memAddr, UINT32 mask, UINT32 newVal)
{
  UINT32 *word = WORD_OF(memAddr);
  UINT32 shift = SHIFT_OF(memAddr);
  UINT32 value = (*(volatile UINT32 *)word & ~(mask << shift)) | ((newVal & mask) << shift);
  return StoreConditional(word, value);
} /* end of StoreConditionalPart */


inline UINT8 OSUINT8_LL(UINT8 *memAddr)
{
  return (UINT8)(LoadReserved(WORD_OF(memAddr)) >> SHIFT_OF(memAddr));
} /* end of OSUINT8_LL */

inline BOOL OSUINT8_SC(UINT8 *memAddr, register UINT8 newVal)
{
  return StoreConditionalPart(memAddr, 0xFFu, newVal);
} /* end of OSUINT8_SC */

inline UINT16 OSUINT16_LL(UINT16 *memAddr)
{
  return (UINT16)(LoadReserved(WORD_OF(memAddr)) >> SHIFT_OF(memAddr));
} /* end of OSUINT16_LL */

inline BOOL OSUINT16_SC(UINT16 *memAddr, register UINT16 newVal)
{
  return StoreConditionalPart(memAddr, 0xFFFFu, newVal);
} /* end of OSUINT16_SC */

inline INT16 OSINT16_LL(INT16 *memAddr)
{
  return (INT16)(LoadReserved(WORD_OF(memAddr)) >> SHIFT_OF(memAddr));
} /* end of OSINT16_LL */

inline BOOL OSINT16_SC(INT16 *memAddr, register INT16 newVal)
{
  return StoreConditionalPart(memAddr, 0xFFFFu, (UINT16)newVal);
} /* end of OSINT16_SC */

inline UINT32 OSUINT32_LL(UINT32 *memAddr)
{
  return LoadReserved(memAddr);
} /* end of OSUINT32_LL */

inline BOOL OSUINT32_SC(UINT32 *memAddr, register UINT32 newVal)
{
  return StoreConditional(memAddr, newVal);
} /* end of OSUINT32_SC */

inline INT32 OSINT32_LL(INT32 *memAddr)
{
  return (INT32)LoadReserved((UINT32 *)memAddr);
} /* end of OSINT32_LL */

inline BOOL OSINT32_SC(INT32 *memAddr, register INT32 newVal)
{
  return StoreConditional((UINT32 *)memAddr, (UINT32)newVal);
} /* end of OSINT32_SC */
