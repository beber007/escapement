/* Copyright (c) 2026 Bertrand Hurst. All rights reserved.
** Escapement - Lightweight Power-Aware Real-Time OS.
** Derived from prior work; see LICENSE and NOTICE at the root of this repository.
*/
/* File Escapement_Processor.h: Processor layer of the host test build.
**
** The kernel expects its port to provide the means of raising a context switch, of
** masking interrupts and of going to sleep. Here there is no processor to speak of: the
** test drives the kernel by hand, so raising a context switch records that one was asked
** for and returns. What the test then checks is which task the kernel elected, not that
** it ran.
*/

#ifndef ESCAPEMENT_PROCESSOR_H
#define ESCAPEMENT_PROCESSOR_H

#include "Escapement_Config.h"

/* Non-blocking algorithms mark a pointer with a bit the address space never uses. */
#define MARKEDBIT    0x8000000000000000u
#define UNMARKEDBIT  0x7FFFFFFFFFFFFFFFu

extern unsigned HostContextSwitchesRequested;
extern unsigned HostSoftTimerRequests;
extern unsigned HostFailingSC;       /* store-conditionals to fail, see host_port.c */

#define _OSScheduleTask()               (HostContextSwitchesRequested += 1)
#define _OSGenerateSoftTimerInterrupt() (HostSoftTimerRequests += 1)
#define _OSClearSoftTimerInterrupt()    ((void)0)
#define _OSEnableInterrupts()           ((void)0)
#define _OSDisableInterrupts()          ((void)0)
#define _OSSleep()                      ((void)0)

/* The assembler context switch, and the offsets it assumes, do not exist here. */
#define OSCheckTCBLayout() struct OSCheckTCBLayoutNotApplicable

/* Load-linked / store-conditional pairs. The kernel builds its queues with them; on the
** host, nothing preempts, so a reservation is never lost. */
UINT8  OSUINT8_LL(UINT8 *addr);
BOOL   OSUINT8_SC(UINT8 *addr, UINT8 value);
UINT16 OSUINT16_LL(UINT16 *addr);
BOOL   OSUINT16_SC(UINT16 *addr, UINT16 value);
INT16  OSINT16_LL(INT16 *addr);
BOOL   OSINT16_SC(INT16 *addr, INT16 value);
UINT32 OSUINT32_LL(UINT32 *addr);
BOOL   OSUINT32_SC(UINT32 *addr, UINT32 value);
INT32  OSINT32_LL(INT32 *addr);
BOOL   OSINT32_SC(INT32 *addr, INT32 value);
UINTPTR OSUINTPTR_LL(UINTPTR *addr);
BOOL   OSUINTPTR_SC(UINTPTR *addr, UINTPTR value);

/* Operating points of the power-aware kernel, those of the RP2040: the kernel picks one
** from the work left and the time until the next arrival, and the host only records it,
** tasks taking no time here whatever the speed. */
#ifdef ESCAPEMENT_VERSION_HARD_PA
   #define OS_12MHZ_SPEED  0
   #define OS_50MHZ_SPEED  1
   #define OS_125MHZ_SPEED 2
   #define OS_MAX_SPEED    OS_125MHZ_SPEED
   UINT8 OSGetProcessorSpeed(void);
   void OSSetProcessorSpeed(UINT8 speed);
#endif

void _OSIOHandler(void);
void *OSMalloc(UINT16 size);

#endif /* ESCAPEMENT_PROCESSOR_H */
