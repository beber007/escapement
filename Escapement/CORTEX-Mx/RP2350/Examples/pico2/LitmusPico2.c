/* Copyright (c) 2026 Bertrand Hurst. All rights reserved.
** Escapement - Lightweight Power-Aware Real-Time OS.
** Distributed under the terms of LICENSE at the root of this repository.
*/
/* File LitmusPico2.c: Litmus tests of the order in which each Cortex-M33 of the RP2350
** sees the other's accesses to SRAM, the question the barriers of the kernel between the
** cores rest on. The Armv8-M architecture lets Normal memory be weakly ordered (DDI0553,
** B7.9) and the SRAM is Normal and non-shareable in the default memory map; the models
** (explore_weak in test/model) assume every reordering the architecture allows, and the
** kernel pays for it with its DMBs. What the chip does is for it to say.
**
** Three classic tests, each with and without a DMB between its two accesses on each core
** (Alglave, Maranget, Sarkar and Sewell, "Litmus: running tests against hardware",
** TACAS 2011):
**   SB, store buffering:  core 0: X = 1; r0 = Y    core 1: Y = 1; r1 = X
**       r0 = r1 = 0 needs a load to pass the store before it;
**   MP, message passing:  core 0: D = 1; F = 1     core 1: r0 = F; r1 = D
**       r0 = 1, r1 = 0 needs the two stores, or the two loads, to be reordered;
**   LB, load buffering:   core 0: r0 = X; Y = 1    core 1: r1 = Y; X = 1
**       r0 = r1 = 1 needs a store to pass the load before it.
** The accesses are written in assembly, so that the compiler can neither reorder nor
** merge them. Each round, core 0 clears the variables, then hands core 1 the test and a
** pseudo-random wait through the SIO FIFO and waits its own, so that the two cores meet
** at every offset around each other, to the cycle (Wait); core 1 runs its half and
** sends its register back.
** The harness has its DMBs outside the window of the test: the clearing seen before the
** round starts, and core 1's stores done before it reports.
**
** Results holds, for each test, the rounds run and how many ended in each of the four
** outcomes, indexed r0 * 2 + r1; tools/pico2_check.py reads it over SWD, as does
** escapement_pico2.robot under Renode, whose cores keep program order.
**
** Built for the Hazard3 cores (make ARCH=riscv, issue #77), the same tests run with sw,
** lw and fence rw,rw for the DMB: RVWMO, RISC-V's memory model, allows the same
** reorderings as Armv8-M (RISC-V unprivileged ISA, "RVWMO Memory Consistency Model").
** The harness's barriers, between memory and the FIFO of the SIO, a device, are
** fence iorw,iorw, the fence rw,rw ordering memory alone.
** On the Pico 2 on 2026-11-06, 2.56 million rounds of each test, none ended in its weak
** outcome, with or without the fence: the Hazard3 cores kept program order, as the
** Cortex-M33 cores do. With the nops of Wait, on 2026-11-07, three boots of 3.56
** million rounds of each test, none weak; SB overlapped in some 156,600 rounds each
** time, where the loops alone had given from 0 to 46,694.
** Platform version: RP2350 (Raspberry Pi Pico 2).
*/

#include "Escapement.h"
#include "Escapement_Core1.h"

enum { SB, SB_DMB, MP, MP_DMB, LB, LB_DMB, TESTS };

typedef struct {
  UINT32 Rounds, Outcome[4];
} LITMUS;

volatile struct {
  UINT32 Marker;             /* 'LTMS' once the rounds have started */
  LITMUS Test[TESTS];
  #ifdef __riscv
     UINT32 Steps[2];        /* the cycles of 1 and 31 nops of Wait, less none's */
  #endif
} Results;

/* The variables of the tests, in two words of their own: consecutive words lie in
** different banks of the striped SRAM (RP2350 datasheet, 4.2), as two variables would
** in the kernel's structures. */
static volatile UINT32 Shared[2] __attribute__((aligned(32)));
#define X (&Shared[0])
#define Y (&Shared[1])

static UINT32 Core1Stack[256] __attribute__((aligned(8)));

static void Core1(void);
static UINT32 Core0Half(UINT32 test);
static UINT32 Core1Half(UINT32 test);
static void Wait(UINT32 random);
#ifdef __riscv
   static UINT32 Cycles(UINT32 random);
#endif
static void Push(UINT32 word);
static UINT32 Pop(void);

#define VTOR              *((volatile UINT32 *)0xE000ED08)
#define WATCHDOG_CTRL_CLR *((volatile UINT32 *)(0x400D8000 + 0x3000))
#define WATCHDOG_ENABLE   (1u << 30)
#define SIO_FIFO_ST       *((volatile UINT32 *)(0xD0000000 + 0x50))
#define SIO_FIFO_WR       *((volatile UINT32 *)(0xD0000000 + 0x54))
#define SIO_FIFO_RD       *((volatile UINT32 *)(0xD0000000 + 0x58))
#define SIO_FIFO_VLD      (1u << 0)
#define SIO_FIFO_RDY      (1u << 1)

/* The accesses of the tests, and their barrier, in the assembly of each architecture. */
#ifdef __riscv
   #define STR(r,a)  "sw " r ",0(" a ")\n"
   #define LDR(r,a)  "lw " r ",0(" a ")\n"
   #define DMB       "fence rw,rw\n"
   #define HARNESS_BARRIER() __asm volatile ("fence iorw,iorw" ::: "memory")
#else
   #define STR(r,a)  "str " r ",[" a "]\n"
   #define LDR(r,a)  "ldr " r ",[" a "]\n"
   #define DMB       "dmb\n"
   #define HARNESS_BARRIER() __asm volatile ("dmb" ::: "memory")
#endif


int main(void)
{
  #ifndef __riscv
     extern void (* const CortexMxVectorTable[])(void);
  #endif
  UINT32 seed = 1, round, test, r0, r1;
  #ifndef __riscv
     VTOR = (UINT32)CortexMxVectorTable;
  #endif
  /* As in FourSlotCoresPico2.c: a watchdog a firmware in flash armed. */
  WATCHDOG_CTRL_CLR = WATCHDOG_ENABLE;
  OSInitializeSystemClocks();
  OSLaunchCore1(Core1,&Core1Stack[sizeof Core1Stack / sizeof Core1Stack[0]]);
  #ifdef __riscv
     Results.Steps[0] = Cycles(1u << 5) - Cycles(0);
     Results.Steps[1] = Cycles(31u << 5) - Cycles(0);
  #endif
  Results.Marker = 0x4C544D53;
  for (round = 0; ; round += 1) {
     test = round % TESTS;
     *X = *Y = 0;
     HARNESS_BARRIER();
     seed = seed * 1103515245u + 12345u;
     Push(test | (seed >> 16 & 0xFFFF) << 8);
     seed = seed * 1103515245u + 12345u;
     Wait(seed >> 16);
     r0 = Core0Half(test);
     r1 = Pop();
     Results.Test[test].Rounds += 1;
     Results.Test[test].Outcome[r0 * 2 + r1] += 1;
  }
} /* end of main */


/* Core1: Core 1. Each round: the test and its delay, its half, and its register back. */
static void Core1(void)
{
  UINT32 word, r1;
  while (TRUE) {
     word = Pop();
     Wait(word >> 8);
     r1 = Core1Half(word & 0xFF);
     HARNESS_BARRIER();
     Push(r1);
  }
} /* end of Core1 */


/* Core0Half: Core 0's side of a test, and the value it read; in MP, 0. */
static UINT32 Core0Half(UINT32 test)
{
  UINT32 r = 0, one = 1;
  switch (test) {
     case SB:
        __asm volatile (STR("%1","%2") LDR("%0","%3") : "=&r"(r) : "r"(one), "r"(X), "r"(Y) : "memory");
        break;
     case SB_DMB:
        __asm volatile (STR("%1","%2") DMB LDR("%0","%3") : "=&r"(r) : "r"(one), "r"(X), "r"(Y) : "memory");
        break;
     case MP:    /* X the data, Y the flag */
        __asm volatile (STR("%0","%1") STR("%0","%2") : : "r"(one), "r"(X), "r"(Y) : "memory");
        break;
     case MP_DMB:
        __asm volatile (STR("%0","%1") DMB STR("%0","%2") : : "r"(one), "r"(X), "r"(Y) : "memory");
        break;
     case LB:
        __asm volatile (LDR("%0","%2") STR("%1","%3") : "=&r"(r) : "r"(one), "r"(X), "r"(Y) : "memory");
        break;
     case LB_DMB:
        __asm volatile (LDR("%0","%2") DMB STR("%1","%3") : "=&r"(r) : "r"(one), "r"(X), "r"(Y) : "memory");
        break;
  }
  return r;
} /* end of Core0Half */


/* Core1Half: Core 1's side. In MP both loads are core 1's: it returns the flag in bit 1
** and the data in bit 0, and core 0's half returns 0, so that the outcome index is still
** r0 * 2 + r1, with r0 the flag and r1 the data. */
static UINT32 Core1Half(UINT32 test)
{
  UINT32 r = 0, f = 0, one = 1;
  switch (test) {
     case SB:
        __asm volatile (STR("%1","%2") LDR("%0","%3") : "=&r"(r) : "r"(one), "r"(Y), "r"(X) : "memory");
        break;
     case SB_DMB:
        __asm volatile (STR("%1","%2") DMB LDR("%0","%3") : "=&r"(r) : "r"(one), "r"(Y), "r"(X) : "memory");
        break;
     case MP:
        __asm volatile (LDR("%0","%2") LDR("%1","%3") : "=&r"(f), "=&r"(r) : "r"(Y), "r"(X) : "memory");
        r |= f << 1;
        break;
     case MP_DMB:
        __asm volatile (LDR("%0","%2") DMB LDR("%1","%3") : "=&r"(f), "=&r"(r) : "r"(Y), "r"(X) : "memory");
        r |= f << 1;
        break;
     case LB:
        __asm volatile (LDR("%0","%2") STR("%1","%3") : "=&r"(r) : "r"(one), "r"(Y), "r"(X) : "memory");
        break;
     case LB_DMB:
        __asm volatile (LDR("%0","%2") DMB STR("%1","%3") : "=&r"(r) : "r"(one), "r"(Y), "r"(X) : "memory");
        break;
  }
  return r;
} /* end of Core1Half */


/* Wait: 0 to 31 turns of a loop of two instructions, then 0 to 31 of one of three,
** each count taken from 5 bits of random. With a single loop of the same length on both
** cores, their offset moved by whole turns only and some offsets were never met: in 2.6
** million rounds of LB, not one had both loads before both stores (2026-09-28). Two
** lengths whose difference is one cycle reach every offset.
** On the Hazard3 the two loops took 2 and 4 cycles a turn, and their costs moved with
** where the code sat: every wait moved the offset by an even count, which kept the
** parity it had at boot. SB's overlap then came in some boots and never in others, 0
** in 2.58 million rounds on 2026-11-07 (#80). So the second count there is of nops of
** 4 bytes, run in a row from a computed jump. main measures 1 and 31 of them, on its
** own copy of Wait, and tools/pico2_check.py checks that 1 takes an odd count. */
static void Wait(UINT32 random)
{
  UINT32 n = random & 31, m = random >> 5 & 31, scratch = 0;
  #ifdef __riscv
     __asm volatile ("1: addi %0,%0,-1\n bgez %0,1b" : "+r"(n));
     __asm volatile (".option push\n .option norvc\n"
                     " la %0,2f\n slli %1,%1,2\n sub %0,%0,%1\n jr %0\n"
                     " .rept 31\n nop\n .endr\n2:\n .option pop"
                     : "=&r"(scratch), "+r"(m));
  #else
     __asm volatile ("1: subs %0,#1\n bpl 1b" : "+r"(n) : : "cc");
     __asm volatile ("1: subs %0,#1\n add.w %1,%1,#1\n bpl 1b" : "+r"(m), "+r"(scratch) : : "cc");
  #endif
} /* end of Wait */


#ifdef __riscv
/* Cycles: The fewest cycles of three waits of random, on mcycle. */
static UINT32 Cycles(UINT32 random)
{
  UINT32 i, start, end, least = 0xFFFFFFFF;
  for (i = 0; i < 3; i += 1) {
     __asm volatile ("csrr %0, mcycle" : "=r"(start) :: "memory");
     Wait(random);
     __asm volatile ("csrr %0, mcycle" : "=r"(end) :: "memory");
     if (end - start < least)
        least = end - start;
  }
  return least;
} /* end of Cycles */
#endif


static void Push(UINT32 word)
{
  while ((SIO_FIFO_ST & SIO_FIFO_RDY) == 0);
  SIO_FIFO_WR = word;
} /* end of Push */


static UINT32 Pop(void)
{
  while ((SIO_FIFO_ST & SIO_FIFO_VLD) == 0);
  return SIO_FIFO_RD;
} /* end of Pop */
