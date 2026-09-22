/* Copyright (c) 2026 Bertrand Hurst. All rights reserved.
** Escapement - Lightweight Power-Aware Real-Time OS.
** Derived from prior work; see LICENSE and NOTICE at the root of this repository.
*/
/* File Escapement_Timer.c: Hardware abstract timer layer for the RP2040.
**
** The RP2040 timer is a free running 64-bit counter fed by a fixed 1 us tick derived from
** clk_ref, and therefore *independent of the core clock*. Changing the core frequency, as
** the power-aware variant does, does not move the kernel's time base — unlike the STM32,
** where the timer clock follows the core clock through the APB prescaler.
**
** Escapement expects a counter that wraps at 2^30 and signals each wraparound, so that it
** can shift all its time values back. The RP2040 counter never wraps within any practical
** run, so the wraparound is reproduced with a second alarm armed on each 2^30 boundary:
**   ALARM0 carries the deadline set by _OSSetTimer;
**   ALARM1 marks the 2^30 boundary and plays the role of the overflow interrupt.
**
** Platform version: RP2040 (Raspberry Pi Pico).
*/

#include "Escapement.h"
#include "Escapement_Timer.h"

#define TIMER_BASE          0x40054000
#define TIMER_ALARM0        *((volatile UINT32 *)(TIMER_BASE + 0x10))
#define TIMER_ALARM1        *((volatile UINT32 *)(TIMER_BASE + 0x14))
#define TIMER_ARMED         *((volatile UINT32 *)(TIMER_BASE + 0x20))
#define TIMER_DBGPAUSE      *((volatile UINT32 *)(TIMER_BASE + 0x2C))
#define TIMER_TIMERAWL      *((volatile UINT32 *)(TIMER_BASE + 0x28))
#define TIMER_INTR          *((volatile UINT32 *)(TIMER_BASE + 0x34))
/* INTE and INTF are shared with the alarms of Escapement_TimerEvent.c: each side sets and
** clears its own bits through the atomic aliases of the register block, which a read-
** modify-write interrupted by the other side would not be. */
#define TIMER_INTE_SET      *((volatile UINT32 *)(TIMER_BASE + 0x2000 + 0x38))
#define TIMER_INTE_CLR      *((volatile UINT32 *)(TIMER_BASE + 0x3000 + 0x38))
#define TIMER_INTF_SET      *((volatile UINT32 *)(TIMER_BASE + 0x2000 + 0x3C))
#define TIMER_INTF_CLR      *((volatile UINT32 *)(TIMER_BASE + 0x3000 + 0x3C))

#define RESETS_BASE         0x4000C000
#define RESETS_RESET        *((volatile UINT32 *)(RESETS_BASE + 0x00))
#define RESETS_RESET_DONE   *((volatile UINT32 *)(RESETS_BASE + 0x08))
#define RESETS_TIMER_BIT    (1u << 21)

/* The 1 us tick of the timer is produced by the watchdog tick generator, which must be
** told how many clk_ref cycles make a microsecond. On a Raspberry Pi Pico clk_ref is the
** 12 MHz crystal. */
#define WATCHDOG_TICK       *((volatile UINT32 *)(0x40058000 + 0x2C))
#define WATCHDOG_TICK_ENABLE (1u << 9)

#define NVIC_ISER           *((volatile UINT32 *)0xE000E100)
#define NVIC_IPR            ((volatile UINT32 *)0xE000E400)

#define ALARM0_BIT          0x1
#define ALARM1_BIT          0x2
#define TIME_MASK           0x3FFFFFFFu   /* Escapement counts modulo 2^30 */

/* Interrupt cause marked by the ISR and processed by the lower priority handler
** _OSTimerInterruptHandler. */
/* Value of the counter when the kernel started. The RP2040 counter is free running since
** power-up and is never reset, whereas Escapement expects its clock to start near zero: on
** a board that has been running for a while the kernel would otherwise believe every
** deadline already missed. All kernel times are therefore counted from this origin. */
static UINT32 TimeOrigin = 0;

volatile BOOL _OSOverflowInterruptFlag = FALSE;
volatile BOOL _OSComparatorInterruptFlag = FALSE;

#ifdef ESCAPEMENT_MEASURE_SCHEDULING_COST
   /* Cost of a scheduling round, in microseconds: from the hardware timer interrupt that
   ** signals an arrival, to the moment the kernel arms the next deadline through
   ** _OSSetTimer. Covers the interrupt, the software timer handler, the transfer of
   ** arrivals to the ready queue and the election of the next task. Read over SWD. */
   volatile UINT32 _OSCostLast = 0;
   volatile UINT32 _OSCostMax = 0;
   volatile UINT32 _OSCostSum = 0;
   volatile UINT32 _OSCostCount = 0;
   static volatile UINT32 CostStart = 0;
   static volatile BOOL CostPending = FALSE;
#endif


/* Minimal descriptor retrieved by _OSIOHandler; its first field is the handler. */
typedef struct TIMER_ISR_DATA {
  void (*TimerIntHandler)(struct TIMER_ISR_DATA *);
} TIMER_ISR_DATA;

static TIMER_ISR_DATA Alarm0Descriptor;
static TIMER_ISR_DATA Alarm1Descriptor;

static void Alarm0Handler(struct TIMER_ISR_DATA *descriptor);
static void Alarm1Handler(struct TIMER_ISR_DATA *descriptor);


/* ArmOverflowAlarm: Arms ALARM1 on the next 2^30 boundary of the counter. */
static void ArmOverflowAlarm(void)
{
  UINT32 raw = TIMER_TIMERAWL;
  TIMER_ALARM1 = raw + ((TIME_MASK + 1) - ((raw - TimeOrigin) & TIME_MASK));
} /* end of ArmOverflowAlarm */


/* SetIRQPriority: Sets the priority of a peripheral interrupt. The Cortex-M0+ holds 4
** priority levels in the 2 most significant bits of each byte of the IPR words, which
** group 4 interrupts each. */
static void SetIRQPriority(UINT8 irq, UINT8 priority)
{
  UINT32 word = NVIC_IPR[irq >> 2];
  UINT8 shift = (irq & 0x3) << 3;
  word &= ~(0xFFu << shift);
  /* Only the 2 most significant bits of the byte are implemented; masking keeps a
  ** priority above 3 from spilling into the neighbouring interrupt. */
  word |= ((UINT32)((priority << 6) & 0xFF) << shift);
  NVIC_IPR[irq >> 2] = word;
} /* end of SetIRQPriority */


/* _OSInitializeTimer: Brings the timer out of reset and prepares its two alarms, without
** starting to count arrivals. The kernel calls _OSStartTimer later, from the idle task. */
void _OSInitializeTimer(void)
{
  /* Release the timer from reset and wait for it to answer. */
  RESETS_RESET &= ~RESETS_TIMER_BIT;
  while ((RESETS_RESET_DONE & RESETS_TIMER_BIT) == 0);
  /* Produce the 1 us tick from the 12 MHz reference clock. */
  WATCHDOG_TICK = WATCHDOG_TICK_ENABLE | 12;
  /* The RP2040 freezes its timer as soon as a core is halted by the debugger. Keeping that
  ** behaviour is deliberate: without it, every inspection lets the kernel's clock run on
  ** while the tasks are stopped, and on resume the kernel finds every deadline missed —
  ** which trips its overload guard as soon as a task has a short period. Debugging a
  ** real-time kernel requires its clock to stop with it.
  ** Measuring is the one case that wants the opposite. The probe has to hold a core
  ** halted while it loads the image, which freezes the clock; the kernel then arms a
  ** deadline computed on a stopped clock, and since an alarm of the RP2040 fires on
  ** equality, a deadline the counter has already passed once it resumes is never
  ** reached again. Letting the clock run from the start avoids that dead end. */
  #ifdef ESCAPEMENT_MEASURE_SCHEDULING_COST
     TIMER_DBGPAUSE = 0x0;   /* wall time, including across debugger halts */
  #else
     TIMER_DBGPAUSE = 0x7;
  #endif
  /* Disarm both alarms and clear any pending cause. */
  TIMER_INTE_CLR = ALARM0_BIT | ALARM1_BIT;
  TIMER_ARMED = ALARM0_BIT | ALARM1_BIT;
  TIMER_INTR = ALARM0_BIT | ALARM1_BIT;
  TIMER_INTF_CLR = ALARM0_BIT | ALARM1_BIT;
  /* Install the two handlers and enable their interrupts. */
  Alarm0Descriptor.TimerIntHandler = Alarm0Handler;
  Alarm1Descriptor.TimerIntHandler = Alarm1Handler;
  OSSetISRDescriptor(OS_IO_TIMER_0, &Alarm0Descriptor);
  OSSetISRDescriptor(OS_IO_TIMER_1, &Alarm1Descriptor);
  SetIRQPriority(OS_IO_TIMER_0, TIMER_PRIORITY);
  SetIRQPriority(OS_IO_TIMER_1, TIMER_PRIORITY);
  NVIC_ISER = (1u << OS_IO_TIMER_0) | (1u << OS_IO_TIMER_1);
} /* end of _OSInitializeTimer */


/* _OSStartTimer: Starts scheduling arrivals. The counter is already running, so this only
** arms the wraparound alarm and forces a first comparator interrupt, which gives the
** kernel the opportunity to compute its first deadline. */
void _OSStartTimer(void)
{
  TimeOrigin = TIMER_TIMERAWL;
  ArmOverflowAlarm();
  TIMER_INTE_SET = ALARM0_BIT | ALARM1_BIT;
  TIMER_INTF_SET = ALARM0_BIT;   // force the first comparator interrupt
} /* end of _OSStartTimer */


/* _OSGetActualTime: Returns the current time, counted modulo 2^30. */
INT32 _OSGetActualTime(void)
{
  return (INT32)((TIMER_TIMERAWL - TimeOrigin) & TIME_MASK);
} /* end of _OSGetActualTime */


/* _OSTimerIsOverflow: Tells whether the counter passed a 2^30 boundary since the last
** call, in which case the kernel shifts all its time values back by that amount. */
BOOL _OSTimerIsOverflow(INT32 shiftTimeLimit)
{
  if (_OSOverflowInterruptFlag) {
     _OSOverflowInterruptFlag = FALSE;
     return TRUE;
  }
  return FALSE;
} /* end of _OSTimerIsOverflow */


/* _OSSetTimer: Arms the comparator on the next arrival time.
** Returned value: TRUE when the deadline lies ahead, FALSE when it already passed, in
**   which case the caller processes it immediately. */
BOOL _OSSetTimer(INT32 nextArrivalTime)
{
  UINT32 raw = TIMER_TIMERAWL;
  UINT32 now = raw - TimeOrigin;
  UINT32 target = (UINT32)nextArrivalTime;
  #ifdef ESCAPEMENT_MEASURE_SCHEDULING_COST
     if (CostPending) {
        UINT32 cost = raw - CostStart;
        CostPending = FALSE;
        _OSCostLast = cost;
        _OSCostSum += cost;
        _OSCostCount += 1;
        if (cost > _OSCostMax)
           _OSCostMax = cost;
     }
  #endif
  if (target > (now & TIME_MASK)) {
     /* Keep the armed value: reading ALARM0 back does not return it, the register is
     ** cleared as soon as the alarm fires. */
     UINT32 deadline = raw + (target - (now & TIME_MASK));
     TIMER_ALARM0 = deadline;
     /* The counter may have moved past the deadline while it was being armed. */
     if ((INT32)(deadline - TIMER_TIMERAWL) > 0)
        return TRUE;
  }
  /* Disarm, then drop any cause the alarm may have raised while it was being set:
  ** the caller is told the deadline has passed and processes the arrival itself, so
  ** a pending interrupt would only buy a second, redundant scheduling round. */
  TIMER_ARMED = ALARM0_BIT;
  TIMER_INTR = ALARM0_BIT;
  return FALSE;
} /* end of _OSSetTimer */


/* Alarm0Handler: Comparator interrupt, a task arrival is due. */
static void Alarm0Handler(struct TIMER_ISR_DATA *descriptor)
{
  #ifdef ESCAPEMENT_MEASURE_SCHEDULING_COST
     CostStart = TIMER_TIMERAWL;
     CostPending = TRUE;
  #endif
  TIMER_INTF_CLR = ALARM0_BIT; // release a possibly forced interrupt
  TIMER_INTR = ALARM0_BIT;     // acknowledge
  _OSComparatorInterruptFlag = TRUE;
  _OSGenerateSoftTimerInterrupt();
} /* end of Alarm0Handler */


/* Alarm1Handler: The counter passed a 2^30 boundary; rearm for the next one. */
static void Alarm1Handler(struct TIMER_ISR_DATA *descriptor)
{
  TIMER_INTR = ALARM1_BIT;     // acknowledge
  ArmOverflowAlarm();
  _OSOverflowInterruptFlag = TRUE;
  _OSGenerateSoftTimerInterrupt();
} /* end of Alarm1Handler */
