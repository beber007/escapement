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
#include "ZottaOS.h"       /* Needed for the definitions of OSUINT16_LL and OSUINT16_SC */
#include "ZottaOS_Timer.h"

/* System wall clock. This variable stores the most-significant 16 bits of the current
** time. The lower 16 bits are directly taken from the timer counter register. To get
** the current time, use OSGetActualTime. */
volatile INT16 _OSTime = 0;


/* Claude */
volatile BOOL _OSOverflowInterruptFlag = FALSE;
volatile BOOL _OSComparatorInterruptFlag = FALSE;


/* Claude à revoir */
/* Although the MSP430 and CC430 provides several 16-bit timers/counters with multiple
** modes and features, only one timer is used to keep track of the task arrivals. This
** timer is configured in continuous mode with interrupts and continuously counts up until
** it equals to the value of 2^16 - 1 at which time an interrupt is generated and the
** counter restarts counting from zero. The timer never stops once it has begun counting.



** According to the MSP430 Family User's Guides (e.g. SLAU208), when
** setting a new value in the comparator register and if that value is greater or equal
** to the current counter count, the timer continues to count until it reaches the new
** comparator value. However, if the new value is smaller, the timer restarts counting
** from zero, and an additional timer increment may occur before the count begins.
** Also, the hardware timer triggers when its comparator register CCR is equal to its
** counter at which time it rolls back to 0, but the actual number of timer ticks that
** has elapsed is equal to CCR + 1. */


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
  /* _OSEnableSoftTimerInterrupt enables software timer interrupts. This function is
  ** defined in the generated assembler file because the port pin can be chosen by the
  ** user. */
  // Select timer clock source and divider, and enable timer interrupts
  OSTimerControlRegister |= OSTimerSourceEnable;
  // Enable comparator timer interrupt
  OSTimerCompareControlRegister |= OSTimerCompareInterruptEnable;
  #ifdef OSTimerSourceDivider
     OSTimerSourceDivider;
  #endif
  _OSEnableSoftTimerInterrupt();
} /* end of _OSInitializeTimer */


/* _OSStartTimer: Starts the interval timer by setting it to up mode. This function is
** called only once when the kernel is ready to schedule the first application task. */
void _OSStartTimer(void)
{
  OSTimerControlRegister |= MC_2; // Start timer in continuous mode
  _OSGenerateSoftTimerInterrupt();
} /* end of _OSStartTimer */


/* _OSSetTimer: Sets the timer comparator to the next time event interval. This function
 * is called by the software timer interrupt handler when it finishes processing the cur-
 * rent interrupt and prepares its next interrupt. */
BOOL _OSSetTimer(INT32 nextArrivalTime)
{
  if (nextArrivalTime >> 16 == _OSTime) {
     OSTimerCompareRegister = (UINT16)nextArrivalTime;
     return OSTimerCompareRegister > OSTimerCounter;
  }
  else
     return TRUE;
} /* end of _OSSetTimer */


/* _OSTimerIsOverflow: return true if a timer overflow occurs. */
BOOL _OSTimerIsOverflow(INT32 shiftTimeLimit)
{
  INT16 tmp;
  _OSOverflowInterruptFlag = FALSE;
  if ((tmp = shiftTimeLimit >> 16) < _OSTime) {
     _OSTime -= tmp; // No need LL/SC because word subtraction is one assembly instruction
     return TRUE;
  }
  else
     return FALSE;
} /* end of _OSTimerIsOverflow */


/* OSGetActualTime: Returns the current value of the wall clock. Combines the 16 bits of
** the timer counter with the global variable Time to yield the current time.*/
INT32 OSGetActualTime(void)
{
  INT16 currentTime;
  INT32 tmp;
  do {
     currentTime = _OSTime;
     tmp = (INT32)_OSTime << 16 | OSTimerCounter;
  } while (currentTime != _OSTime);
  return tmp;
} /* end of OSGetActualTime */
