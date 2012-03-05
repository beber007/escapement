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
** Version identifier: March 2012
** Authors: MIS-TIC
*/
#include "msp430.h"        /* Hardware specifics */
#include "ZottaOS_Types.h" /* Type definitions */
#include "ZottaOS.h"       /* Needed for the definitions of OSUINT16_LL and OSUINT16_SC */
#include "ZottaOS_Timer.h"

/* The kernel keeps track of the relative time in a variable called _OSTime. This time
** serves as the basis to guarantee all temporal constraints. This variable is shared
** with the hardware timer ISR which updates it whenever it triggers. */
INT32 _OSTime = 0; /* System wall clock */

/* Although the MSP430 and CC430 provides several 16-bit timers/counters with multiple
** modes and features, only one timer is used to keep track of the task arrivals. This
** timer is configured in up mode with interrupts and continuously counts up until it
** equals to the value specified in its compare register at which time an interrupt is
** generated and the counter restarts counting from zero. The timer never stops once it
** has begun counting. According to the MSP430 Family User's Guides (e.g. SLAU208), when
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
  /* _OSEnableSoftTimerInterrupt re-enables software timer interrupt. This function is
  ** called when the current software timer ISR is complete and may be re-invoked. This
  ** function is defined in the generated assembler file because the port pin can be
  ** chosen by the user. */
  extern void _OSEnableSoftTimerInterrupt(void);
  // Assure that the first interrupt sets _OSTime to CC register+1=0
  OSTimerCompareRegister = 0xFFFF;
  // Create an interrupt as soon as the timer runs
  OSTimerCounter = 0xFFFE;
  // Select timer clock source and divider, and enable timer interrupts
  OSTimerControlRegister |= OSTimerSourceEnable;
  #ifdef OSTimerSourceDivider
     OSTimerSourceDivider;
  #endif
  _OSEnableSoftTimerInterrupt();
} /* end of _OSInitializeTimer */


/* _OSStartTimer: Starts the interval timer by setting it to up mode. This function is
** called only once when the kernel is ready to schedule the first application task. */
void _OSStartTimer(void)
{
  OSTimerControlRegister |= MC_1;
} /* end of _OSStartTimer */


/* To force a timer interrupt, setting the comparator to TAR does not immediately gene-
** rate the expected interrupt. To do so, we need to set the comparator TAR + TARDELAY,
** where TARDELAY is a delay that depends on the speed ratio between the timer clock and
** the core clock.
** Because the timer continues ticking, when we wish set a new value for the timer compa-
** rator, the difference in time between the new value and the previous must be such that
** when the assignment is done, the timer has not passed the comparator value. This is
** guaranteed by TAROFFSET. */
#define TARDELAY   2
#define TAROFFSET  2
/* The comparator register only has 16 bits, so the maximum value that is can hold is
** 0x0000FFFF. */
#define INFINITY32 0x0000FFFE

/* _OSSetTimer: Sets the timer comparator to the next time event interval. This function
 * is called by the software timer interrupt handler when it finishes processing the cur-
 * rent interrupt and prepares its next interrupt. */
void _OSSetTimer(INT32 nextTimeInterval)
{
  UINT16 newTime;
  if (nextTimeInterval < INFINITY32) {
     newTime = (UINT16)nextTimeInterval;
     while (TRUE) {
        OSUINT16_LL((UINT16 *)&OSTimerCompareRegister);
        if (newTime > OSTimerCounter + TAROFFSET)
           newTime -= 1;
        else
           newTime = OSTimerCounter + TARDELAY;
        if (OSUINT16_SC((UINT16 *)&OSTimerCompareRegister,newTime))
           break;
        /* If the timer gets here, there must have been an interrupt other than from a
        ** nested timer source. Before calling this function, the software timer inter-
        ** rupt sets the _OSNoSaveContext flag to indicate that it cannot return from a
        ** nested call and that the new software timer interrupt will restart from its
        ** beginning. The new software timer interrupt effectively wipes out the current
        ** handler. All other interrupt sources save the context of the caller, and when
        ** the caller resumes, it is possible that TAR has changed. The SC instruction
        ** fails in this case. */
     }
  }
} /* end of _OSSetTimer */


/* _OSGenerateSoftTimerInterrupt: Generates an internal software-initiated interrupt. */
void _OSGenerateSoftTimerInterrupt(void)
{
  do {
     OSUINT16_LL((UINT16 *)&OSTimerCompareRegister);
     if (OSTimerCounter >= 0xFFFC) // 0xFFFE - TARDELAY
        break;
  } while (!OSUINT16_SC((UINT16 *)&OSTimerCompareRegister,OSTimerCounter + TAROFFSET));
} /* end of _OSGenerateSoftTimerInterrupt */


/* OSGetActualTime: Returns the current value of the wall clock. Combines the 16 bits of
** the timer counter with the global variable Time to yield the current time.*/
INT32 OSGetActualTime(void)
{
  INT32 currentTime, tmp;
  do {
     currentTime = _OSTime;
     tmp = currentTime + OSTimerCounter;
  } while (currentTime != _OSTime);
  return tmp;
} /* end of OSGetActualTime */


/* _OSTimerShift: Shifts the global Time variable. This is a value greater than 2^16. */
void _OSTimerShift(INT32 shiftTimeLimit)
{
  _OSTime -= shiftTimeLimit;
} /* end of _OSTimerShift */
