/* Copyright (c) 2026 Bertrand Hurst. All rights reserved.
** Escapement - Lightweight Power-Aware Real-Time OS.
** Distributed under the terms of LICENSE at the root of this repository.
*/
/* File RestartPico.c: Issue #40's second step on the RP2040: core 1 restarts core 0, and
** with it the kernel, when a task loops.
**
** Core 0 runs the kernel: a heartbeat of 1 ms, and a task of 10 ms whose 50th instance
** after each start loops for ever, interrupts left on after an even number of restarts,
** masked after an odd one. Core 1 runs no kernel: it watches the heartbeat on the 1 us
** counter and, after DETECT us without a beat, restarts core 0.
**
** - Soft: core 1 writes a word into the SIO FIFO, whose interrupt on core 0 rewrites the
**   PC of the frame it stacked so that its return lands in RestartEntry, in Thread mode.
**   That sets the stack pointer back to the top of the one stack and goes through
**   _OSResetHandler and main again. A loop with interrupts masked never takes it.
** - Hard, if no beat comes back within SOFT_WAIT us: core 1 forces the interrupt of the
**   timer's alarm 2, which the kernel leaves alone (TIMER_INTF), and core 0 takes it as its
**   NMI (SYSCFG_PROC0_NMI_MASK), masked interrupts or not. RestartNMI writes the
**   watchdog's SCRATCH4 to 7, the bootrom's vector to RAM (RP2040 datasheet 2.8.1.1;
**   pico-sdk, watchdog_reboot: magic 0xb007c0d3, pc ^ -magic, sp, pc), and resets its core
**   (AIRCR.SYSRESETREQ). Core 0's bootrom finds the vector and jumps to RestartEntry with
**   the stack given. Forcing core 0 off from core 1 through the PSM, the way first tried,
**   left the debugger unable to reach either core until a rescue reset (2026-10-24): as if
**   the PSM had taken core 1, after core 0 in its sequence, down too, before it could
**   release core 0; likely, not checked in the datasheet.
**
** Either way the peripherals keep their state: RestartCore0 disarms the timer's alarms and
** clears their flags, clears the NVIC, and puts back .data, which an image in SRAM does
** not reload; the clocks, set at the first start, are left as they are. What core 1 keeps
** sits in .noinit, which neither the loader nor _OSResetHandler touches.
**
** Restart: 0 restarts asked, 1 soft that brought the beat back, 2 hard made, 3 hard that
** brought it back, 4 and 5 the most and the sum of the real time lost by a soft one, from
** the last beat to the next, in us, 6 and 7 the same for a hard one, 8 restarts that did
** not bring it back within HARD_WAIT us, 9 1 once RESTARTS are done, 10 the beats seen,
** 11 the windows of 100 ms between restarts that missed a beat, 12 core 0's starts.
** Platform version: RP2040.
*/

#include "Escapement.h"
#include "Escapement_Core1.h"

#if defined(ESCAPEMENT_VERSION_SOFT) || defined(ESCAPEMENT_VERSION_HARD_PA)
   #error "the hard kernel"
#endif

#define RESTARTS   20
#define DETECT     3000
#define SOFT_WAIT  5000
#define HARD_WAIT  50000

#define TIMER_TIMERAWL    *((volatile UINT32 *)(0x40054000 + 0x28))
#define TIMER_DBGPAUSE    *((volatile UINT32 *)(0x40054000 + 0x2C))
#define TIMER_ARMED       *((volatile UINT32 *)(0x40054000 + 0x20))
#define TIMER_INTR        *((volatile UINT32 *)(0x40054000 + 0x34))
#define WATCHDOG_CTRL_CLR *((volatile UINT32 *)(0x40058000 + 0x3000))
#define WATCHDOG_ENABLE   (1u << 30)
#define WATCHDOG_SCRATCH  ((volatile UINT32 *)(0x40058000 + 0x0C))
#define TIMER_INTF        *((volatile UINT32 *)(0x40054000 + 0x3C))
#define ALARM2_BIT        (1u << 2)
#define SYSCFG_PROC0_NMI_MASK *((volatile UINT32 *)0x40004000)
#define AIRCR             *((volatile UINT32 *)0xE000ED0C)
#define RESETS_CLR        *((volatile UINT32 *)(0x4000C000 + 0x3000))
#define RESETS_RESET_DONE *((volatile UINT32 *)(0x4000C000 + 0x08))
#define RESETS_SYSCFG_BIT (1u << 18)
#define SIO_FIFO_ST       *((volatile UINT32 *)(0xD0000000 + 0x50))
#define SIO_FIFO_WR       *((volatile UINT32 *)(0xD0000000 + 0x54))
#define SIO_FIFO_RD       *((volatile UINT32 *)(0xD0000000 + 0x58))
#define SIO_FIFO_VLD      (1u << 0)
#define SIO_FIFO_RDY      (1u << 1)
#define NVIC_ISER         *((volatile UINT32 *)0xE000E100)
#define NVIC_ICER         *((volatile UINT32 *)0xE000E180)
#define NVIC_ICPR         *((volatile UINT32 *)0xE000E280)
#define NVIC_IPR          ((volatile UINT32 *)0xE000E400)
#define ICSR              *((volatile UINT32 *)0xE000ED04)
#define VTOR              *((volatile UINT32 *)0xE000ED08)
#define SIO_IRQ_PROC0     15
#define RESTART_WORD      0x52535452u   /* "RSTR" */
#define RESTARTING        0x52535452u
#define VECTORED_BOOT     0xb007c0d3u

/* ---- What a restart of core 0 leaves alone. */
#define NOINIT __attribute__((section(".noinit")))
volatile UINT32 Restart[13] NOINIT;
static volatile UINT32 Beat NOINIT;
static volatile UINT32 Restarting NOINIT;
static UINT32 SavedData[64] NOINIT;     /* .data as the image had it */
static UINT32 Core1Stack[256] NOINIT __attribute__((aligned(8)));

/* ---- Core 0. */
extern void (* const CortexMxVectorTable[])(void);
static UINT32 RamTable[64] __attribute__((aligned(256)));
static UINT32 Instances;

void _OSResetHandler(void);
void RestartEntry(void);
void RestartIRQ(void);

/* RestartCore0: From RestartEntry, the stack set back, interrupts masked. */
void __attribute__((noreturn, used)) RestartCore0(void)
{
  extern UINT32 _data, _edata;
  UINT32 *p, i = 0;
  NVIC_ICER = 0xFFFFFFFF;
  NVIC_ICPR = 0xFFFFFFFF;
  ICSR = (1u << 27) | (1u << 25);    /* PENDSVCLR, PENDSTCLR */
  TIMER_ARMED = 0xF;                 /* written 1, an alarm is disarmed */
  TIMER_INTR = 0xF;
  TIMER_INTF = 0;
  while (SIO_FIFO_ST & SIO_FIFO_VLD)
     (void)SIO_FIFO_RD;
  SIO_FIFO_ST = 0xFF;                /* clears the error flags */
  // cppcheck-suppress comparePointers
  for (p = &_data; p < &_edata; p += 1)
     *p = SavedData[i++];
  Restarting = RESTARTING;
  _OSResetHandler();
  for (;;);
} /* end of RestartCore0 */

/* RestartEntry: Where both restarts land, in Thread mode: the stack at its top. */
__asm__(
"  .syntax unified\n"
"  .section .text.RestartEntry,\"ax\",%progbits\n"
"  .global RestartEntry\n"
"  .thumb_func\n"
"RestartEntry:\n"
"  cpsid i\n"
"  ldr  r0, =CortexMxVectorTable\n"
"  ldr  r1, [r0]\n"
"  msr  msp, r1\n"
"  ldr  r2, =0xE000ED08\n"
"  str  r0, [r2]\n"
"  bl   RestartCore0\n"
"  .ltorg\n"
/* RestartIRQ: The SIO FIFO's interrupt. Interrupting Thread mode (EXC_RETURN 0xFFFFFFF9),
** it rewrites the PC and xPSR of the frame stacked on the one stack, and returns into
** RestartEntry; interrupting a handler, it does nothing, and the hard restart follows. */
"  .section .text.RestartIRQ,\"ax\",%progbits\n"
"  .global RestartIRQ\n"
"  .thumb_func\n"
"RestartIRQ:\n"
/* The word taken first, and only if there is one, then the error flags cleared: read last,
** the line had not fallen when the handler returned, the interrupt came again, read an
** empty FIFO, and the flag of that read (ROE) held it for ever (on the Pico, 2026-10-24). */
"  ldr  r2, =0xD0000050\n"
"  ldr  r1, [r2]\n"               /* FIFO_ST */
"  lsls r1, r1, #31\n"
"  beq  2f\n"
"  ldr  r1, [r2, #8]\n"           /* FIFO_RD */
"2:\n"
"  movs r1, #0xFF\n"
"  str  r1, [r2]\n"               /* clears WOF and ROE */
"  dsb\n"
"  ldr  r1, =0xFFFFFFF9\n"
"  cmp  lr, r1\n"
"  bne  1f\n"
"  mrs  r0, msp\n"
"  ldr  r1, =RestartEntry\n"
"  movs r2, #1\n"
"  bics r1, r2\n"                 /* a stacked PC has bit 0 clear */
"  str  r1, [r0, #24]\n"
"  ldr  r1, =0x01000000\n"
"  str  r1, [r0, #28]\n"
"1:\n"
"  bx   lr\n"
"  .ltorg\n"
);

/* RestartNMI: Core 0's NMI, the hard restart: the vector to RAM, then a reset of the core. */
void RestartNMI(void)
{
  UINT32 pc = (UINT32)RestartEntry | 1;
  TIMER_INTF = 0;
  WATCHDOG_SCRATCH[4] = VECTORED_BOOT;
  WATCHDOG_SCRATCH[5] = pc ^ -VECTORED_BOOT;
  WATCHDOG_SCRATCH[6] = (UINT32)CortexMxVectorTable[0];
  WATCHDOG_SCRATCH[7] = pc;
  __asm volatile ("dsb" ::: "memory");
  AIRCR = (0x05FAu << 16) | (1u << 2);    /* VECTKEY, SYSRESETREQ */
  for (;;);
} /* end of RestartNMI */

static void Heartbeat(void *argument)
{
  (void)argument;
  Beat += 1;
  OSEndTask();
} /* end of Heartbeat */

static void FaultyTask(void *argument)
{
  (void)argument;
  if (++Instances == 50 && Restart[9] == 0) {
     if (Restart[0] & 1)
        __asm volatile ("cpsid i" ::: "memory");
     for (;;)
        __asm volatile ("" ::: "memory");
  }
  OSEndTask();
} /* end of FaultyTask */

/* Continue: An overrun lets the instance go on. Stopping, the default, halts the kernel
** with interrupts masked, which only the hard restart gets out of; the loop that keeps
** them on is the one the soft restart is for. Its deadline passed, the instance comes
** first under EDF and the heartbeat stops. */
static UINT8 Continue(void *argument)
{
  (void)argument;
  return OS_OVERRUN_CONTINUE;
} /* end of Continue */

/* ---- Core 1: no kernel, a loop on the 1 us counter. */
static void Record(UINT32 most, UINT32 sum, UINT32 lost)
{
  if (lost > Restart[most])
     Restart[most] = lost;
  Restart[sum] += lost;
} /* end of Record */

static void HardRestart(void)
{
  TIMER_INTF = ALARM2_BIT;
} /* end of HardRestart */

static void Watcher(void)
{
  enum { WATCHING, SOFT, HARD, GAVE_UP } state = WATCHING;
  UINT32 last = Beat, lastAt = TIMER_TIMERAWL, askedAt = 0, windowAt = lastAt, inWindow = 0;
  for (;;) {
     UINT32 now = TIMER_TIMERAWL, beat = Beat;
     if (beat != last) {
        if (state == SOFT || state == HARD) {
           Record(state == SOFT ? 4 : 6, state == SOFT ? 5 : 7, now - lastAt);
           Restart[state == SOFT ? 1 : 3] += 1;
           if (Restart[0] >= RESTARTS)
              Restart[9] = 1;
           state = WATCHING;
           windowAt = now, inWindow = 0;
        }
        Restart[10] += beat - last;
        inWindow += beat - last;
        last = beat, lastAt = now;
     }
     if (state == WATCHING && now - windowAt >= 100000) {
        if (inWindow < 99)
           Restart[11] += 1;
        windowAt = now, inWindow = 0;
     }
     if (state == WATCHING && now - lastAt > DETECT) {
        Restart[0] += 1;
        while ((SIO_FIFO_ST & SIO_FIFO_RDY) == 0);
        SIO_FIFO_WR = RESTART_WORD;
        state = SOFT, askedAt = now;
     }
     else if (state == SOFT && now - askedAt > SOFT_WAIT) {
        Restart[2] += 1;
        HardRestart();
        state = HARD, askedAt = TIMER_TIMERAWL;
     }
     else if (state == HARD && now - askedAt > HARD_WAIT) {
        Restart[8] += 1;
        state = GAVE_UP;
     }
  }
} /* end of Watcher */


int main(void)
{
  extern UINT32 _data, _edata;
  UINT32 i, *p;
  BOOL restarted = Restarting == RESTARTING;
  Restarting = 0;
  if (!restarted) {
     VTOR = (UINT32)CortexMxVectorTable;
     WATCHDOG_CTRL_CLR = WATCHDOG_ENABLE;
     OSInitializeSystemClocks();
     /* .data as loaded, for the restarts to put back. */
     i = 0;
     // cppcheck-suppress comparePointers
     for (p = &_data; p < &_edata && i < 64; p += 1)
        SavedData[i++] = *p;
     for (i = 0; i < sizeof Restart / sizeof Restart[0]; i += 1)
        Restart[i] = 0;
     Beat = 0;
  }
  Restart[12] += 1;
  /* The vector table in RAM, the SIO FIFO's interrupt ours, of the highest priority. */
  for (i = 0; i < 64; i += 1)
     RamTable[i] = (UINT32)CortexMxVectorTable[i];
  RamTable[16 + SIO_IRQ_PROC0] = (UINT32)RestartIRQ;
  RamTable[2] = (UINT32)RestartNMI;
  /* SYSCFG leaves reset here, or the mask written is lost and no NMI comes (2026-10-24). */
  RESETS_CLR = RESETS_SYSCFG_BIT;
  while ((RESETS_RESET_DONE & RESETS_SYSCFG_BIT) == 0);
  SYSCFG_PROC0_NMI_MASK = ALARM2_BIT;
  VTOR = (UINT32)RamTable;
  /* The loader leaves core 1 halted, which pauses the counter the kernel set to pause
  ** with it; core 1 runs from here on, and no core is halted. */
  TIMER_DBGPAUSE = 0;
  if (!restarted)
     OSLaunchCore1(Watcher,&Core1Stack[sizeof Core1Stack / sizeof Core1Stack[0]]);
  while (SIO_FIFO_ST & SIO_FIFO_VLD)
     (void)SIO_FIFO_RD;
  NVIC_IPR[SIO_IRQ_PROC0 >> 2] &= ~(0xFFu << (8 * (SIO_IRQ_PROC0 & 3)));
  NVIC_ISER = 1u << SIO_IRQ_PROC0;
  Instances = 0;
  OSCreateTask(Heartbeat,0,1000,1000,NULL);
  OSCreateTask(FaultyTask,0,10000,10000,NULL);
  OSSetOverrunHook(Continue);
  return OSStartMultitasking(NULL,NULL);
} /* end of main */
