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
*/
/* File ZottaOS_Timer.c: Hardware abstract timer layer. This file holds all timer
**          related functions needed by the ZottaOS family of kernels so that these can
**          easily be ported from one MSP to another and also to other microcontrollers.
** Version date: March 2012
** Authors: MIS-TIC
*/

/* TODO: offrir la possibilité de chager de timer. OK pour les Timer 2, 3 ,4 */
/* TODO: optimiser et documenter _OSInitializeTimer */

#include "ZottaOS_CortexM3.h"
#include "ZottaOS.h"
#include "ZottaOS_Timer.h"

#define TIMER_PRESCALER 71

//#define TIM OS_IO_TIM2
//#define TIM OS_IO_TIM3
#define TIM OS_IO_TIM4


/* Because the timer continues ticking, when we wish set a new value for the timer compa-
** rator, the difference in time between the new value and the previous must be such that
** when the assignment is done, the timer has not passed the comparator value. This is
** guaranteed by TIMER_OFFSET. */
/* The number of core cycles to considered in order to obtain TIMER_OFFSET are given in
** function _OSSetTimerComparator, and comprised between markers START_TIMER_OFFSET and
** END_TIMER_OFFSET. */
#define TIMER_OFFSET 5

#define INFINITY32 0x0000FFFF
#define INFINITY16 0xFFFF
#define INFINITY32_OFFSET 0x0000FFFA // = INFINITY32 - TIMER_OFFSET
#define INFINITY16_OFFSET 0xFFFA     // = INFINITY16 - TIMER_OFFSET


/* STM-32 hardware registers */
#define ACPB1CLKENABLE       *((UINT32 *)0x4002101C)
#define AIRCR                *((UINT32 *)0xE000ED0C)
#define INT_SET_ENABLE       *((UINT32 *)0xE000E100)
#define INT_PRIORITY_LEVEL   *((UINT32 *)0xE000E470)

#if TIM == OS_IO_TIM2
  #define TIME_BASE 0x40000000
#elif TIM == OS_IO_TIM3
  #define TIME_BASE 0x40000400
#elif TIM == OS_IO_TIM4
  #define TIME_BASE 0x40000800
#elif TIM == OS_IO_TIM5
  #define TIME_BASE 0x40000C00
#endif

#define TIM_CONTROL1         *((UINT16 *)(TIME_BASE + 0x00))
#define TIM_INT_ENABLE       *((UINT16 *)(TIME_BASE + 0x0C))
#define TIM_STATUS           *((UINT16 *)(TIME_BASE + 0x10))
#define TIM_EVENT_GENERATION *((UINT16 *)(TIME_BASE + 0x14))
#define TIM_COUNTER          *((UINT16 *)(TIME_BASE + 0x24))
#define TIM_PRESCALER        *((UINT16 *)(TIME_BASE + 0x28))
#define TIM_AUTORELOAD       *((UINT16 *)(TIME_BASE + 0x2C))
#define TIM_COMPARATOR       *((UINT16 *)(TIME_BASE + 0x34))


/* System wall clock. This variable stores the most-significant 16 bits of the current
** time. The lower 16 bits are directly taken from the timer counter register. To get the
** current time, use _OSTimerGet. */
static volatile INT32 Time;


/* _OSInitializeTimer: Initializes the timer which starts counting as soon as ZottaOS is
** ready to process the first arrival. When the kernel, i.e. when OSStartMultitasking()
** is called, the last operation that is done is to set the timer handler and then start
** the idle task which is the only ready task in the system at that time. The first time
** the idle task executes, it calls _OSStartTimer().
** After _OSInitializeTimer() is called the timer's input divider is selected but it is
** halted.
** Note that the timer ISR updates the wall clock _OSTime with the current value of its
** comparator register CCR and adds 1, and at the very first interrupt, _OSTime must be
** set 0. This is done by initializing CCR with 0 - 1 = 0xFFFF. */
void _OSInitializeTimer(void)
{
  UINT32 tmppriority, tmppre, tmpsub = 0x0F;
  /* Enable clock for timer TIM2 */
  ACPB1CLKENABLE |= 1 << (TIM - OS_IO_TIM2);
  /* Set the autoreload value */
  TIM_AUTORELOAD = 65535;
  /* Set the compare register value */
  TIM_COMPARATOR = 1;
  /* Set the prescaler value */
  TIM_PRESCALER = TIMER_PRESCALER;
  /* Generate an update event to reload the prescaler and the Repetition counter values
  ** immediately */
  TIM_EVENT_GENERATION = 1;
  /* Enable update and compare 1 interrupt */
  TIM_INT_ENABLE |= 3;
  TIM_STATUS = (UINT16)~3; // Clear interrupt flag
  /* Compute the corresponding IRQ priority */
  tmppriority = (0x700 - (AIRCR & (UINT32)0x700))>> 0x08;
  tmppre = (0x4 - tmppriority);
  tmpsub = tmpsub >> tmppriority;
  tmppriority = (UINT32)TIMER_PRIORITY << tmppre;
  tmppriority |=  TIMER_SUB_PRIORITY & tmpsub;
  INT_PRIORITY_LEVEL = tmppriority << 0x04;
  /* Enable the selected IRQ channels */
  INT_SET_ENABLE = (UINT32)(0x01 << TIM);
} /* end of _OSInitializeTimer */


/* _OSStartTimer: Starts the interval timer by setting it to up mode. This function is
** called only once when the kernel is ready to schedule the first application task. */
void _OSStartTimer(void)
{
  /* Enable the TIM Counter */
  TIM_CONTROL1 |= 1;
} /* end of _OSStartTimer */


/* _OSTimerShift: Shifts the global Time variable. This is a value greater than 2^16. */
void _OSTimerShift(INT32 shiftTimeLimit)
{
  Time -= shiftTimeLimit;
} /* end of _OSTimerShift */


/* OSGetActualTime: Retrieve the current time. Combines the 16 bits of the timer counter
** with the global variable Time to yield the current time. */
INT32 OSGetActualTime(void)
{
  INT32 currentTime, tmp;
  do {
     currentTime = Time;
     tmp = currentTime | TIM_COUNTER;
  } while (currentTime != Time);
  return tmp;
} /* end of OSGetActualTime */


/* _OSTimerHandler: Catches a STM-32 Timer 2 interrupt and generates a software timer
** interrupt which is than carried out at a lower priority.
** Note: This function could have been written in assembler to reduce interrupt latencies.
*/
void _OSTimerHandler(void)
{
  /* Disable timer comparator*/
  TIM_COMPARATOR = 0;
  /* Test interrupt source */
  if (TIM_STATUS & 2)    // Is comparator interrupt pending?
     TIM_STATUS &= ~2;   // Clear interrupt flag
  if (TIM_STATUS & 1) {  // Is timer overflow interrupt?
     TIM_STATUS &= ~1;   // Clear interrupt flag
     Time += 0x10000;    // Increment most significant word of Time
  }
  _OSGenerateSoftTimerInterrupt();
} /* end of _OSTimerHandler */


/* _OSSetTimer: Sets the timer comparator to the next time event interval. This function
 * is called by the software timer interrupt handler when it finishes processing the cur-
 * rent interrupt and prepares its next interrupt. */
void _OSSetTimer(INT32 nextArrival)
{
  INT32 time32;
  UINT16 time16;
  while (TRUE) {
     OSUINT16_LL(&TIM_COMPARATOR);
     if ((time32 =  nextArrival - Time) < INFINITY32_OFFSET) {
         if (time32 > 0) {
           time16 = (UINT16)time32;
           if (time16 > TIMER_OFFSET) {
              if (TIM_COUNTER > time16 - TIMER_OFFSET) {       // START_TIMER_OFFSET
                 if (time16 > INFINITY16 - TIMER_OFFSET)
                    break;
                 else
                    time16 = TIM_COUNTER + TIMER_OFFSET;
              }
           }
           else
              time16 = TIM_COUNTER + TIMER_OFFSET;
        }
        else
           break;
        if (OSUINT16_SC(&TIM_COMPARATOR,time16))              // END_TIMER_OFFSET
           break;
        /* If we get here, the task has been interrupted by a source different than
        ** the interval timer. */
     }
     else
        break;
  }
} /* end of _OSSetTimer */

