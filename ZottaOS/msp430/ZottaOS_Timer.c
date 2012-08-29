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
/* File ZottaOS_Timer.c: Hardware abstract timer layer. This file holds all timer related
**          functions needed by ZottaOS so that these can easily be ported from one MSP to
**          another and also to other microcontrollers.
** Platform version: All MSP430 and CC430 microcontrollers.
** Version identifier: June 2012
** Authors: MIS-TIC
*/
#include "ZottaOS.h"
#include "ZottaOS_Timer.h"

/* Although the MSP430 and CC430 provides several 16-bit timers/counters with multiple
** modes and features, only one timer is used to keep track of the task arrivals. This
** timer is configured in continuous mode with interrupts and continuously counts up until
** it equals to the value of 2^16 - 1 at which time an interrupt is generated and the
** counter restarts counting from zero. The timer never stops once it has begun counting.
** Task arrivals are programmed with comparator register CCR1 that sets an interrupt when-
** ever the timer's internal counter register matches CCR1. According to the MSP430 Family
** User's Guides (e.g. SLAU208), an interrupt is generated only if the comparator register
** holds a value that is greater than the current timer's internal counter. Hence, when
** setting a new comparator, care must be exerted to avoid setting a value smaller than or
** equal to the current counter count.

** System wall clock. This variable stores the most-significant 16 bits of the current
** time, and it is incremented every time the timer overflows (transits from 2^16 - 1 to
** 0). To get the current time, use _OSGetActualTime, which concatenates the lower 16 bits
** taken from the timer counter register with the number of overflows. */
volatile INT16 _OSTime = 0;

/* Global interrupt source flags positioned by the timer ISR. These are defined here so
** that they may be (1) correctly typed, and (2) architecturally dependent. */
volatile BOOL _OSOverflowInterruptFlag = FALSE;
volatile BOOL _OSComparatorInterruptFlag = FALSE;


/* _OSInitializeTimer: Initializes the timer which starts counting as soon as ZottaOS is
** ready to process the first arrival. When the kernel, i.e. when OSStartMultitasking()
** is called, the last operation that is done is to set the timer handler and then start
** the idle task which is the only ready task in the system at that time, and the first
** time the idle task executes, it calls _OSStartTimer().
** After _OSInitializeTimer() is called the timer's input divider is selected but it is
** halted. */
void _OSInitializeTimer(void)
{
  // Select timer clock source and divider, and enable timer interrupts
  OSTimerControlRegister |= OSTimerSourceEnable;
  // Enable comparator timer interrupt
  OSTimerCompareControlRegister |= OSTimerCompareInterruptEnable;
  #ifdef OSTimerSourceDivider
     OSTimerSourceDivider;
  #endif
} /* end of _OSInitializeTimer */


/* _OSStartTimer: Starts the interval timer by setting it to up mode. This function is
** called only once when the kernel is ready to schedule the first application task. */
void _OSStartTimer(void)
{
  OSTimerControlRegister |= MC_2;  // Start timer in continuous mode
  _OSGenerateSoftTimerInterrupt(); // Schedule all periodic tasks arriving at time 0
  // _OSEnableSoftTimerInterrupt enables software timer interrupts. This function is de-
  // fined in the generated assembler file because the port pin can be chosen by the user.
  // For PA kernel version, the software timer interrupt is enable in _OSSleep function
  // after idle task stack pointer backup operation.
  #if !defined(ZOTTAOS_VERSION_HARD_PA) && !defined(ZOTTAOS_VERSION_SOFT_PA)
     _OSEnableSoftTimerInterrupt();
  #endif
} /* end of _OSStartTimer */


/* _OSSetTimer: Sets the timer comparator to the next time event, i.e. to the next task
** arrival time. This function is called by the software timer interrupt handler when it
** finishes processing the current interrupt and prepares its next interrupt.
** Returned value: False when the event has been missed and should be processed before
** calling this function one more time but with the next task arrival time; otherwise the
** function returns true. */
BOOL _OSSetTimer(INT32 nextArrivalTime)
{ /* Check if the interrupt time is in the range of the timer's counter; otherwise, wait
  ** until the next timer overflow. (The comparator is set to 0 in the ISR.) */
  if (nextArrivalTime >> 16 == _OSTime) {
     OSTimerCompareRegister = (UINT16)nextArrivalTime;
     if (OSTimerCompareRegister > OSTimerCounter) // Missed the interrupt?
        return TRUE;
     OSTimerCompareRegister = 0;
     return FALSE;
 }
  else
     return TRUE;
} /* end of _OSSetTimer */


/* _OSTimerIsOverflow: Returns true if the increment to internal system wall clock over-
** flows and all time related values should be shifted. */
BOOL _OSTimerIsOverflow(INT32 shiftTimeLimit)
{
  INT16 tmp;
  _OSOverflowInterruptFlag = FALSE;
  if ((tmp = shiftTimeLimit >> 16) < _OSTime) {
     _OSTime -= tmp; // No need for LL/SC because word subtraction is one assembly instruction
     return TRUE;
  }
  else
     return FALSE;
} /* end of _OSTimerIsOverflow */


/* _OSGetActualTime: Returns the current value of the wall clock. Combines the 16 bits of
** the timer counter with the global variable Time to yield the current time. This func-
** tion should never be called when interrupts are disabled. */
INT32 _OSGetActualTime(void)
{
  INT16 currentTime;
  INT32 tmp;
  do {
     currentTime = _OSTime;
     tmp = (INT32)_OSTime << 16 | OSTimerCounter;
  } while (currentTime != _OSTime);
  return tmp;
} /* end of _OSGetActualTime */
