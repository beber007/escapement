/* Copyright (c) 2006-2009 MIS Institute of the HEIG affiliated to the University of
** Applied Sciences of Western Switzerland. All rights reserved.
** IN NO EVENT SHALL THE MIS INSTITUTE NOR THE HEIG NOR THE UNIVERSITY OF APPLIED
** SCIENCES OF WESTERN SWITZERLAND BE LIABLE TO ANY PARTY FOR DIRECT, INDIRECT, SPECIAL,
** INCIDENTAL, OR CONSEQUENTIAL DAMAGES ARISING OUT OF THE USE OF THIS SOFTWARE AND ITS
** DOCUMENTATION, EVEN IF THE MIS INSTITUTE OR THE HEIG OR THE UNIVERSITY OF APPLIED
** SCIENCES OF WESTERN SWITZERLAND HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
** THE MIS INSTITUTE, THE HEIG AND THE UNIVERSITY OF APPLIED SCIENCES OF WESTERN SWIT-
** ZERLAND SPECIFICALLY DISCLAIMS ANY WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
** IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE. THE SOFT-
** WARE PROVIDED HEREUNDER IS ON AN "AS IS" BASIS, AND THE MIS INSTITUTE NOR THE HEIG
** AND NOR THE UNIVERSITY OF APPLIED SCIENCES OF WESTERN SWITZERLAND HAVE NO OBLIGATION
** TO PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR MODIFICATIONS.
*/
/* File ZottaOS_Timer.c:
** Version identifier: August 2009
** Authors: MIS-TIC
*/
#include "ZottaOS_msp430.h"
#include "ZottaOS_Timer.h"

#ifdef USB_DRIVER
   #include "USB\USB.h"
#endif

/* Claude : Comentaires à revoir dans tout le fichier */

/* TASK INSTANCE INTER-ARRIVAL TIMER
** Although the MSP430F5xx provides three 16 bits timers/counters with multiple modes and
** features, the only timer used to keep track of the task arrivals is TIMER_A3 configured
** in up mode with interrupts. This timer continuously counts up until it equals to the
** value specified in its compare register at which time an interrupt is generated and
** the counter restarts counting from zero. The timer never stops the once it has begun
** counting. According to the MSP430F5xx Family User's Guide (SLAU208), when setting a
** new value in the comparator register and that value is greater of equal to the current
** counter count, the timer continues to count until it reaches the new comparator value.
** However, if the new value is smaller, the timer restarts counting from zero, and an
** additional count may occur before the count begins. */
/* The MSP430F2xx behaves the same way but with a different peripheral. */


/* The kernel keeps track of the relative time within a hyper-period in a global variable
** called Time. Because everything restarts afresh at the end of a hyper-period, there is
** no need to keep another time base. Time is updated when new task instances arrive at
** the beginning of their periods (release time) or when the timer wraps around. */
static INT32 Time = 0;  /* System wall clock */
UINT16 _OSBackupCCR;    /* Timer comparator value when a timer-interrupt occurred. */


/* To force a timer interrupt, setting the comparator to TAR does not immediately gene-
** rate the expected interrupt. To do so, we need to set the comparator TAR + TARDelay,
** where TARDelay is a delay that depends on the speed ratio between the timer clock and
** the core clock.
** Because the timer continues ticking, when we wish set a new value for the timer compa-
** rator, the difference in time between the new value and the previous must be such that
** when the assignment is done, the timer has not passed the comparator value. This is
** guaranteed by TAROffset. */
static const UINT16 TARDelay = 2;
static const UINT16 TAROffset = 2;

/* Because the compare register only has 16 bits, instance arrivals are scheduled by
** steps of modulo 0x0000FFFF. But because the timer triggers with comparator-1, to get
** 0xFFFF, we need to set the comparator to 0xFFFE. */
#define INFINITY32 0x0000FFFE
#define INFINITY16 0xFFFE


/* _OSGenerateSoftTimerInterrupt: Enable I/O Port 2 pin 7 interrupts */
void _OSGenerateSoftTimerInterrupt(void)
{
  P2IFG |= 0x80;
} /* end of _OSGenerateSoftTimerInterrupt */


/* _OSEnableSoftTimerInterrupt: Enable I/O Port 2 pin 7 interrupts */
void _OSEnableSoftTimerInterrupt(void)
{
  P2IE |= 0x80;
} /* end of _OSEnableSoftTimerInterrupt */


/* Initialize the timer which starts counting as soon as the idle task begins. At
** this point, the timer's input divider is selected but it is halted. */
void _OSInitializeTimer(void)
{
  TACCR0 = 0xFFFF;           
  TAR = 0xFFFE;           
  TACTL |= TASSEL0 | TAIE; // Select timer clock source and enable timer A interrupts
  P2IE |= 0x80;            // Enable I/O Port 2 pin 7 interrupts
}


/* _OSStartTimer: Start the interval timer by setting it to up mode */
void _OSStartTimer(void)
{
  TACTL |= MC_1;
} /* end of _OSStartTimer */


/* _OSSetTimer: Set the timer comparator to the next periodic task arrival time. */
void _OSSetTimer(INT32 nextArrival)
{
  INT32 time;
  UINT16 newTime;
  if ((time = nextArrival - Time) < INFINITY32) {
	 newTime = (UINT16)time;
     while (TRUE) {
        OSUINT16_LL((UINT16 *)&TACCR0);
        if (newTime > TAR + TAROffset)
           newTime -= 1;
        else
           newTime = TAR + TARDelay;
        if (OSUINT16_SC((UINT16 *)&TACCR0,newTime))
           break;
        /* If the timer gets here, there must have been an interrupt other than from a
        ** nested timer source. */
     }
  }
} /* end of _OSSetTimer */


/* _OSGetTime: */
INT32 _OSGetTime(void)
{
  return Time;	
} /* end of _OSGetTime */


/* _OSShiftTime: */
void _OSShiftTime(INT32 shiftTimeLimit)
{
  Time -= shiftTimeLimit;
} /* end of _OSShiftTime */


/* _OSUpdateTime: Update the wall clock with the timer comparator that was saved when the
** timer generated an interrupt. */
void _OSUpdateTime(void)
{
  TACTL &= ~TAIE;           // Disable Timer A interrupt so that _OSBackupCCR cannot be 
  Time += _OSBackupCCR + 1; // modified by the hard timer interrupt
  _OSBackupCCR = 0;
  TACTL |= TAIE;            // Re-enable Timer A interrupt
} /* end of _OSUpdateTime */
