/* Copyright (c) 2026 Bertrand Hurst. All rights reserved.
** Escapement - Lightweight Power-Aware Real-Time OS.
** Distributed under the terms of LICENSE at the root of this repository.
*/
/* File Escapement_Trace.h: A trace of what the port does, kept in RAM and read back over
** SWD after the fact.
**
** Watching a running board from the debugger disturbs it: stopping a core to read its
** registers makes the tasks miss their deadlines, and each read of memory takes about a
** millisecond, so a series of them is no snapshot. The trace records instead, with the
** time of the timer, the events that matter to the scheduling — alarms, rounds of the
** software timer, deadlines armed, changes of speed — in a ring buffer of the last
** OS_TRACE_SIZE of them, which the debugger reads once the run is over or stuck.
**
** The RP2040 and RP2350 ports implement it, only with ESCAPEMENT_TRACE defined (make
** TRACE=1): otherwise every trace point compiles to nothing.
** Platform version: All Cortex-Mx based microcontrollers.
*/

#ifndef ESCAPEMENT_TRACE_H
#define ESCAPEMENT_TRACE_H

/* Events. */
#define OS_TRACE_ALARM0      1   /* comparator alarm fired */
#define OS_TRACE_ALARM1      2   /* wrap alarm fired */
#define OS_TRACE_SOFT_ENTER  3   /* software timer handler entered */
#define OS_TRACE_SOFT_LEAVE  4   /* software timer handler left */
#define OS_TRACE_SET_TIMER   5   /* deadline armed: arg 1 ahead, 0 passed; extra: ticks ahead */
#define OS_TRACE_SPEED       6   /* speed changed: arg new, extra old */
#define OS_TRACE_MARK        7   /* left by the application: arg and extra its own */
#define OS_TRACE_EVENT       8   /* timer event delivered: arg its alarm bit; extra us late */

#if defined(ESCAPEMENT_TRACE) && !defined(_ASM_)
   #define OS_TRACE_SIZE 256     /* a power of two */

   typedef struct {
     UINT32 Time;                /* lower 32 bits of the counter of the timer */
     UINT8 Event;
     UINT8 Arg;
     UINT16 Extra;
   } OS_TRACE_ENTRY;

   extern volatile OS_TRACE_ENTRY _OSTrace[OS_TRACE_SIZE];
   extern volatile UINT32 _OSTraceCount;   /* entries written since start, wrapping */
   extern volatile UINT32 _OSTraceFrozen;  /* set by the debugger to read a still buffer */

   void _OSTraceEvent(UINT8 event, UINT8 arg, UINT16 extra);
   #define OSTrace(event,arg,extra) _OSTraceEvent(event,arg,extra)
#else
   #define OSTrace(event,arg,extra) ((void)0)
#endif

#endif /* ESCAPEMENT_TRACE_H */
