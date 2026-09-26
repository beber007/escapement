/* Copyright (c) 2006-2012 MIS Institute of the HEIG-VD affiliated to the University of
** Applied Sciences of Western Switzerland. All rights reserved.
** Permission to use, copy, modify, and distribute this software and its documentation
** for any purpose, without fee, and without written agreement is hereby granted, pro-
** vided that the above copyright notice, the following three sentences and the authors
** appear in all copies of this software and in the software where it is used.
** IN NO EVENT SHALL THE MIS INSTITUTE NOR THE HEIG-VD NOR THE UNIVERSITY OF APPLIED
** SCIENCES OF WESTERN SWITZERLAND BE LIABLE TO ANY PARTY FOR DIRECT, INDIRECT, SPECIAL,
** INCIDENTAL, OR CONSEQUENTIAL DAMAGES ARISING OUT OF THE USE OF THIS SOFTWARE AND ITS
** DOCUMENTATION, EVEN IF THE MIS INSTITUTE OR THE HEIG-VD OR THE UNIVERSITY OF APPLIED
** SCIENCES OF WESTERN SWITZERLAND HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
** THE MIS INSTITUTE, THE HEIG-VD AND THE UNIVERSITY OF APPLIED SCIENCES OF WESTERN SWIT-
** ZERLAND SPECIFICALLY DISCLAIMS ANY WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
** IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE. THE SOFT-
** WARE PROVIDED HEREUNDER IS ON AN "AS IS" BASIS, AND THE MIS INSTITUTE NOR THE HEIG-VD
** AND NOR THE UNIVERSITY OF APPLIED SCIENCES OF WESTERN SWITZERLAND HAVE NO OBLIGATION
** TO PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR MODIFICATIONS.
** Authors: MIS-TIC
**
** Escapement - Lightweight Power-Aware Real-Time OS, derived from ZottaOS.
** Modifications Copyright (c) 2026 Bertrand Hurst, distributed under the same terms;
** see LICENSE and NOTICE at the root of this repository.
*/
/* File Escapement_Timer.c: Hardware dependent timer layer of the STM32U575, the 32-bit
** path of the STM32 port on TIM2 of this chip (RM0456, general-purpose timers TIM2 to
** TIM5, and RCC; addresses as in STMicroelectronics, cmsis-device-u5, stm32u575xx.h).
**
** TIM2 counts microseconds, through a prescaler set for the system clock of
** OSInitializeSystemClocks, and wraps at 2^30, where the kernel shifts its times: the
** update interrupt marks the wrap, the comparator of channel 1 the next arrival. Both
** mark their cause and defer the work to the software timer interrupt, of lower priority.
** Platform version: STM32U575 (NUCLEO-U575ZI-Q).
*/

#include "Escapement.h"
#include "Escapement_Timer.h"

#define TIM2_BASE            0x40000000
#define TIM_CR1              *((volatile UINT32 *)(TIM2_BASE + 0x00))
#define TIM_DIER             *((volatile UINT32 *)(TIM2_BASE + 0x0C))
#define TIM_SR               *((volatile UINT32 *)(TIM2_BASE + 0x10))
#define TIM_EGR              *((volatile UINT32 *)(TIM2_BASE + 0x14))
#define TIM_CNT              *((volatile UINT32 *)(TIM2_BASE + 0x24))
#define TIM_PSC              *((volatile UINT32 *)(TIM2_BASE + 0x28))
#define TIM_ARR              *((volatile UINT32 *)(TIM2_BASE + 0x2C))
#define TIM_CCR1             *((volatile UINT32 *)(TIM2_BASE + 0x34))

#define TIM_CR1_CEN          (1u << 0)
#define TIM_CR1_URS          (1u << 2)
#define UPDATE_INT_BIT       (1u << 0)   /* UIF, UIE, UG */
#define COMPARATOR_INT_BIT   (1u << 1)   /* CC1IF, CC1IE, CC1G */

#define RCC_APB1ENR1         *((volatile UINT32 *)(0x46020C00 + 0x9C))
#define RCC_APB1ENR1_TIM2EN  (1u << 0)

#define NVIC_ISER(irq)       ((volatile UINT32 *)0xE000E100)[(irq) >> 5]
#define NVIC_BIT(irq)        (1u << ((irq) & 0x1F))
#define NVIC_IPR             ((volatile UINT8 *)0xE000E400)

/* TIM2 is clocked by PCLK1, the system clock with the APB prescaler at 1. */
#define TIMER_PRESCALER      (OS_SYSTEM_CLOCK_HZ / 1000000u - 1u)


/* Timer interrupt cause marked by the ISR and that is used by the lower priority handler
** (_OSTimerInterruptHandler) to process the interrupt. */
volatile BOOL _OSOverflowInterruptFlag = FALSE;
volatile BOOL _OSComparatorInterruptFlag = FALSE;


#ifdef ESCAPEMENT_TRACE
   volatile OS_TRACE_ENTRY _OSTrace[OS_TRACE_SIZE];
   volatile UINT32 _OSTraceCount = 0;
   volatile UINT32 _OSTraceFrozen = 0;

   /* _OSTraceEvent: Appends an event to the ring buffer, interrupts masked for the few
   ** instructions it takes, since interrupts of every priority leave events too. */
   void _OSTraceEvent(UINT8 event, UINT8 arg, UINT16 extra)
   {
     UINT32 primask;
     volatile OS_TRACE_ENTRY *entry;
     __asm volatile ("MRS %0, PRIMASK" : "=r" (primask) :: "memory");
     _OSDisableInterrupts();
     if (_OSTraceFrozen) {
        __asm volatile ("MSR PRIMASK, %0" :: "r" (primask) : "memory");
        return;
     }
     entry = &_OSTrace[_OSTraceCount & (OS_TRACE_SIZE - 1)];
     entry->Time = TIM_CNT;
     entry->Event = event;
     entry->Arg = arg;
     entry->Extra = extra;
     _OSTraceCount += 1;
     __asm volatile ("MSR PRIMASK, %0" :: "r" (primask) : "memory");
   } /* end of _OSTraceEvent */
#endif


/* Minimal descriptor retrieved by _OSIOHandler; its first field is the handler. */
typedef struct TIMER_ISR_DATA {
  void (*TimerIntHandler)(struct TIMER_ISR_DATA *);
} TIMER_ISR_DATA;

static TIMER_ISR_DATA TimerDescriptor;

static void TimerHandler(struct TIMER_ISR_DATA *descriptor);


/* _OSInitializeTimer: Prepares TIM2, halted, and its interrupt. The first time the idle
** task runs, it calls _OSStartTimer. */
void _OSInitializeTimer(void)
{
  RCC_APB1ENR1 |= RCC_APB1ENR1_TIM2EN;
  (void)RCC_APB1ENR1;                    // the clock runs before the timer is written
  /* The update event that loads the prescaler must not raise UIF: the timer sets it a
  ** few of its cycles after the write, past a clear that follows it at once, and the
  ** overflow it stood for then reached the kernel before the timer had started, which
  ** stopped on its overload check (the UNO Q, 2026-09-26; Renode sets it at once). With
  ** URS only a wrap of the counter raises it (RM0456, TIMx_CR1 and TIMx_SR). */
  TIM_CR1 = TIM_CR1_URS;
  TIM_ARR = 0x3FFFFFFF;                  // wraps at 2^30
  TIM_PSC = TIMER_PRESCALER;
  TIM_EGR = UPDATE_INT_BIT;              // load the prescaler
  TIM_SR = 0;
  TIM_DIER = UPDATE_INT_BIT | COMPARATOR_INT_BIT;
  /* Priority in the 4 most significant bits of the byte of the interrupt. */
  NVIC_IPR[OS_IO_TIM2] = (UINT8)(TIMER_PRIORITY << 4);
  TimerDescriptor.TimerIntHandler = TimerHandler;
  OSSetISRDescriptor(OS_IO_TIM2,&TimerDescriptor);
  NVIC_ISER(OS_IO_TIM2) = NVIC_BIT(OS_IO_TIM2);
} /* end of _OSInitializeTimer */


/* _OSStartTimer: Starts counting from zero, the origin the kernel computes its first
** arrivals from (see the STM32 port), and raises a first comparator interrupt. */
void _OSStartTimer(void)
{
  TIM_CNT = 0;
  TIM_CR1 |= TIM_CR1_CEN;
  TIM_EGR = COMPARATOR_INT_BIT;
} /* end of _OSStartTimer */


/* TimerHandler: Marks the cause of a TIM2 interrupt and raises the software timer
** interrupt, which does the work at a lower priority. */
static void TimerHandler(struct TIMER_ISR_DATA *descriptor)
{
  UINT32 status = TIM_SR;
  (void)descriptor;
  if (status & UPDATE_INT_BIT) {         // wrap of the counter
     TIM_SR = ~UPDATE_INT_BIT;           // rc_w0: writing 1 leaves the other flags
     _OSOverflowInterruptFlag = TRUE;
     _OSGenerateSoftTimerInterrupt();
  }
  if (status & COMPARATOR_INT_BIT) {     // next arrival
     TIM_SR = ~COMPARATOR_INT_BIT;
     TIM_CCR1 = 0;                       // disarmed until _OSSetTimer
     _OSComparatorInterruptFlag = TRUE;
     _OSGenerateSoftTimerInterrupt();
  }
} /* end of TimerHandler */


/* _OSSetTimer: Arms the comparator on the next arrival; returns FALSE, the comparator
** left disarmed, when that time has already come. */
BOOL _OSSetTimer(INT32 nextArrival)
{
  TIM_CCR1 = nextArrival;
  if (TIM_CCR1 > TIM_CNT)
     return TRUE;
  TIM_CCR1 = 0;
  return FALSE;
} /* end of _OSSetTimer */


/* _OSTimerIsOverflow: Returns TRUE once after each wrap of the counter. */
BOOL _OSTimerIsOverflow(INT32 shiftTimeLimit)
{
  (void)shiftTimeLimit;
  if (_OSOverflowInterruptFlag) {
     _OSOverflowInterruptFlag = FALSE;
     return TRUE;
  }
  return FALSE;
} /* end of _OSTimerIsOverflow */


/* _OSGetActualTime: The current time, in microseconds modulo 2^30. */
INT32 _OSGetActualTime(void)
{
  return TIM_CNT;
} /* end of _OSGetActualTime */
