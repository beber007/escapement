/* Copyright (c) 2026 Bertrand Hurst. All rights reserved.
** Escapement - Lightweight Power-Aware Real-Time OS.
** Distributed under the terms of LICENSE at the root of this repository.
*/
/* File Escapement_Core1.h: Starts the second core of the RP2350 on a function of its own.
**
** Escapement schedules its tasks on core 0 alone. Core 1 runs bare code beside it, with
** no task, no interrupt and no call into the scheduler: what it shares with the tasks is
** memory, through a slot buffer: the 4-slot buffer, or the 3-slot one, whose LL/SC pairs
** see both cores once each sets ACTLR.EXTEXCLALL. The FIFO queue does not hold between
** them.
**
** Platform version: RP2350 (Raspberry Pi Pico 2).
*/

#ifndef ESCAPEMENT_CORE1_H
#define ESCAPEMENT_CORE1_H

/* OSLaunchCore1: Resets core 1 and has the bootrom start it on entry, after setting its
** ACTLR.EXTEXCLALL, with the stack whose top is given. Called from main on core 0, before
** OSStartMultitasking(); entry must never return.
** Parameters:
**   (1) (void (*)(void)) the function core 1 runs;
**   (2) (UINT32 *) the top of its stack, 8-byte aligned. */
void OSLaunchCore1(void (*entry)(void), UINT32 *stackTop);

#endif /* ESCAPEMENT_CORE1_H */
