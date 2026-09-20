/* Copyright (c) 2026 Bertrand Hurst. All rights reserved.
** Escapement - Lightweight Power-Aware Real-Time OS.
** Derived from prior work; see LICENSE and NOTICE at the root of this repository.
*/
/* File Escapement_Interrupts.h: Interrupt table of the host test build. There is no
** hardware here, so the table exists only to satisfy the kernel.
*/

#ifndef ESCAPEMENT_INTERRUPTS_H
#define ESCAPEMENT_INTERRUPTS_H

#define OS_IO_HOST_TIMER  0
#define OS_IO_NB_ENTRIES  1

void OSSetISRDescriptor(UINT16 entry, void *descriptor);
void *OSGetISRDescriptor(UINT16 entry);

#endif /* ESCAPEMENT_INTERRUPTS_H */
