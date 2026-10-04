/* Copyright (c) 2026 Bertrand Hurst. All rights reserved.
** Escapement - Lightweight Power-Aware Real-Time OS.
** Distributed under the terms of LICENSE at the root of this repository.
*/
/* File Escapement.h: What Escapement_CoreQueue.c needs of the kernel, on an Armv8-A host
** whose cores are weakly ordered, an Apple M-series chip or any AArch64 Linux machine.
**
** The queue is compiled as it stands; only its primitives change. LL and SC become the
** exclusive load and store of AArch64 (LDXR, STXR), which keep the meaning the queue
** relies on: one reservation a core, cleared by the other core's write to the granule,
** and an SC that may fail for no visible reason. The DMB becomes DMB ISH, all the cores
** of the host being in the inner shareable domain.
**
** SKIP_BARRIER, set to the line of Escapement_CoreQueue.c that holds a barrier, leaves
** that one barrier out of the processor and keeps it in the compiler: the mutant then
** asks the hardware alone whether the barrier is needed. It must add no access to
** memory: between an LDXR and its STXR, an exclusive access of its own clears the
** reservation, and the STXR then fails every time (run.sh checks the line instead).
*/

#ifndef ESCAPEMENT_H
#define ESCAPEMENT_H

#include <stdint.h>
#include <stdlib.h>

#if !defined(__aarch64__)
   #error "the litmus bench needs an Armv8-A host: x86 keeps stores in order (TSO)"
#endif

typedef uint8_t   UINT8;
typedef uint16_t  UINT16;
typedef uint32_t  UINT32;
typedef uintptr_t UINTPTR;
typedef int       BOOL;
#define TRUE  1
#define FALSE 0

#ifndef SKIP_BARRIER
   #define SKIP_BARRIER 0
#endif

#define _OSMemoryBarrier() do { \
          if (__LINE__ != SKIP_BARRIER) \
             __asm volatile ("dmb ish" ::: "memory"); \
          else \
             __asm volatile ("" ::: "memory"); \
        } while (0)

static inline void *OSMalloc(UINT16 size)
{
  return calloc(1,size);
}

static inline UINTPTR OSUINTPTR_LL(UINTPTR *addr)
{
  UINTPTR value;
  __asm volatile ("ldxr %0, [%1]" : "=r" (value) : "r" (addr) : "memory");
  return value;
}

static inline BOOL OSUINTPTR_SC(UINTPTR *addr, UINTPTR value)
{
  UINT32 failed;
  __asm volatile ("stxr %w0, %2, [%1]" : "=&r" (failed) : "r" (addr), "r" (value)
                  : "memory");
  return failed == 0;
}

static inline UINT32 OSUINT32_LL(UINT32 *addr)
{
  UINT32 value;
  __asm volatile ("ldxr %w0, [%1]" : "=r" (value) : "r" (addr) : "memory");
  return value;
}

static inline BOOL OSUINT32_SC(UINT32 *addr, UINT32 value)
{
  UINT32 failed;
  __asm volatile ("stxr %w0, %w2, [%1]" : "=&r" (failed) : "r" (addr), "r" (value)
                  : "memory");
  return failed == 0;
}

#endif /* ESCAPEMENT_H */
