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
** On the Hazard3 cores (issue #77) the handshake is the same, with three differences, as
** in the pico-sdk (multicore_launch_core1_with_stack, hardware/hazard3/instructions.h):
** the word of the vector table is the value of mtvec, the table in vectored mode; sev
** and wfe are h3.unblock and h3.block, two hints of the Hazard3 written as slt to x0;
** and core 1 sets nothing first, there being no ACTLR.
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

#ifdef __riscv
   #define SEV() __asm volatile ("slt x0, x0, x1" ::: "memory")   /* h3.unblock */
   #define WFE() __asm volatile ("slt x0, x0, x0" ::: "memory")   /* h3.block */
#else
   #define SEV() __asm volatile ("sev" ::: "memory")
   #define WFE() __asm volatile ("wfe" ::: "memory")
   #define ACTLR             *((volatile UINT32 *)0xE000E008)
   #define ACTLR_EXTEXCLALL  (1u << 29)
#endif

static void Push(UINT32 word);
static UINT32 Pop(void);
static void Core1Start(void);

/* The function core 1 runs, once Core1Start has set it up. */
static void (*volatile Core1Entry)(void);


/* OSLaunchCore1: The bootrom's handshake. */
void OSLaunchCore1(void (*entry)(void), UINT32 *stackTop)
{
  #ifdef __riscv
     /* The RISC-V psABI keeps sp aligned to 16 bytes, where the AAPCS asks for 8: the top
     ** is rounded down to it, whatever the caller's array. */
     extern void _OSVectorTable(void);
     const UINT32 sequence[] = {0, 0, 1, (UINT32)_OSVectorTable | 1u,
                                (UINT32)stackTop & ~15u, (UINT32)Core1Start};
  #else
     extern void (* const CortexMxVectorTable[])(void);
     const UINT32 sequence[] = {0, 0, 1, (UINT32)CortexMxVectorTable, (UINT32)stackTop,
                                (UINT32)Core1Start};
  #endif
  UINT32 i = 0;
  Core1Entry = entry;
  /* What core 0 stored for core 1, the entry and the structures it will use, before the
  ** FIFO's writes that start it: Armv8-M does not order a store to memory before a later
  ** one to a device without a barrier. On RISC-V, the fence of the slot buffers, rw,rw,
  ** orders memory alone; i and o add the devices (RISC-V unprivileged ISA, "FENCE"). */
  #ifdef __riscv
     __asm volatile ("fence iorw,iorw" ::: "memory");
  #else
     _OSMemoryBarrier();
  #endif
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
        SEV();
     }
     Push(sequence[i]);
     i = Pop() == sequence[i] ? i + 1 : 0;
  }
} /* end of OSLaunchCore1 */


/* Core1Start: Where core 1 starts: its ACTLR on the Cortex-M33, then the function it was
** given. */
static void Core1Start(void)
{
  #ifndef __riscv
     ACTLR |= ACTLR_EXTEXCLALL;
     __asm volatile ("dsb" ::: "memory");
     __asm volatile ("isb" ::: "memory");
  #endif
  Core1Entry();
} /* end of Core1Start */


/* Push: Writes a word to core 1, waking it if it waits for one. */
static void Push(UINT32 word)
{
  while ((SIO_FIFO_ST & SIO_FIFO_RDY) == 0);
  SIO_FIFO_WR = word;
  SEV();
} /* end of Push */


/* Pop: Waits for a word from core 1, which signals each one it writes with an event. */
static UINT32 Pop(void)
{
  while ((SIO_FIFO_ST & SIO_FIFO_VLD) == 0)
     WFE();
  return SIO_FIFO_RD;
} /* end of Pop */
