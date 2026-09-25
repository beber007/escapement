/* Copyright (c) 2026 Bertrand Hurst. All rights reserved.
** Escapement - Lightweight Power-Aware Real-Time OS.
** Distributed under the terms of LICENSE at the root of this repository.
*/
/* File Escapement_Core1.c: Starts the second core of the RP2350, as the RP2040 port does:
** the bootrom of core 1 waits on the inter-core FIFO of the SIO for the sequence 0, 0, 1,
** vector table, stack pointer, entry point, and echoes each word back; core 1 is first
** forced off and on again through the power-on state machine. The pico-sdk runs the same
** code on both chips (multicore_reset_core1 and multicore_launch_core1_raw, in
** pico_multicore/multicore.c); only the address of the PSM and the bit of core 1 in its
** FRCE_OFF register differ (hardware/regs/psm.h, addressmap.h).
**
** Core 1 does not start on the entry it is given but on Core1Start, which first sets its
** ACTLR.EXTEXCLALL, as Escapement_RamEntry.S does for core 0: each core has its own ACTLR,
** and the exclusive loads and stores of both must go through the global monitor for the
** LL/SC pairs of the kernel to hold between them.
**
** Platform version: RP2350 (Raspberry Pi Pico 2).
*/

#include "Escapement.h"
#include "Escapement_Core1.h"

#define PSM_FRCE_OFF      *((volatile UINT32 *)(0x40018000 + 0x04))
#define PSM_FRCE_OFF_SET  *((volatile UINT32 *)(0x40018000 + 0x2000 + 0x04))
#define PSM_FRCE_OFF_CLR  *((volatile UINT32 *)(0x40018000 + 0x3000 + 0x04))
#define PSM_PROC1         (1u << 24)

#define SIO_FIFO_ST       *((volatile UINT32 *)(0xD0000000 + 0x50))
#define SIO_FIFO_WR       *((volatile UINT32 *)(0xD0000000 + 0x54))
#define SIO_FIFO_RD       *((volatile UINT32 *)(0xD0000000 + 0x58))
#define SIO_FIFO_VLD      (1u << 0)   /* something to read */
#define SIO_FIFO_RDY      (1u << 1)   /* room to write */

#define ACTLR             *((volatile UINT32 *)0xE000E008)
#define ACTLR_EXTEXCLALL  (1u << 29)

static void Push(UINT32 word);
static UINT32 Pop(void);
static void Core1Start(void);

/* The function core 1 runs, once Core1Start has set it up. */
static void (*volatile Core1Entry)(void);


/* OSLaunchCore1: The bootrom's handshake. */
void OSLaunchCore1(void (*entry)(void), UINT32 *stackTop)
{
  extern void (* const CortexMxVectorTable[])(void);
  const UINT32 sequence[] = {0, 0, 1, (UINT32)CortexMxVectorTable, (UINT32)stackTop,
                             (UINT32)Core1Start};
  UINT32 i = 0;
  Core1Entry = entry;
  PSM_FRCE_OFF_SET = PSM_PROC1;
  while ((PSM_FRCE_OFF & PSM_PROC1) == 0);
  PSM_FRCE_OFF_CLR = PSM_PROC1;
  /* Once out of reset, the bootrom of core 1 announces itself with a 0. */
  Pop();
  while (i < sizeof sequence / sizeof sequence[0]) {
     if (sequence[i] == 0) {
        /* Before each 0, drop what core 1 may have left in the FIFO. */
        while (SIO_FIFO_ST & SIO_FIFO_VLD)
           (void)SIO_FIFO_RD;
        __asm volatile ("sev");
     }
     Push(sequence[i]);
     i = Pop() == sequence[i] ? i + 1 : 0;
  }
} /* end of OSLaunchCore1 */


/* Core1Start: Where core 1 starts: its ACTLR, then the function it was given. */
static void Core1Start(void)
{
  ACTLR |= ACTLR_EXTEXCLALL;
  __asm volatile ("dsb" ::: "memory");
  __asm volatile ("isb" ::: "memory");
  Core1Entry();
} /* end of Core1Start */


/* Push: Writes a word to core 1, waking it if it waits for one. */
static void Push(UINT32 word)
{
  while ((SIO_FIFO_ST & SIO_FIFO_RDY) == 0);
  SIO_FIFO_WR = word;
  __asm volatile ("sev");
} /* end of Push */


/* Pop: Waits for a word from core 1, which signals each one it writes with an event. */
static UINT32 Pop(void)
{
  while ((SIO_FIFO_ST & SIO_FIFO_VLD) == 0)
     __asm volatile ("wfe");
  return SIO_FIFO_RD;
} /* end of Pop */
