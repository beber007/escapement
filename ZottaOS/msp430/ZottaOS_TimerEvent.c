/* Copyright (c) 2012 MIS Institute of the HEIG-VD affiliated to the University of
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
/* File TimerEvent.c: Implements an event queue that is associated with a timer device.
** Events that are inserted into the queue are specified along with a delay, and as soon
** as this delay has expired, the event is scheduled.
** Platform version: All MSP430 and CC430 microcontrollers.
** Version identifier: May 2012
** Authors: MIS-TIC */

#include "ZottaOS.h"
#include "ZottaOS_TimerEvent.h"

/* Because events are sorted by the time at which they occur, and that this time is mon-
** otonically increasing, the relative time reference wraparounds. We therefore need to
** periodically shift the occurrence times. This is done every SHIFT_TIME_LIMIT ticks. */
#define SHIFT_TIME_LIMIT     0x40000000 // = 2^30
#define SHIFT_TIME_LIMIT_16  0x4000     // = 2^30 >> 16

typedef struct TIMER_EVENT_NODE {   // Blocks that are in the event queue
  struct TIMER_EVENT_NODE *Next;
  void *Event;
  INT32 Time;
} TIMER_EVENT_NODE;

typedef struct SOFTWARE_TIMER_ISR_DATA {
  void (*TimerIntHandler)(struct SOFTWARE_TIMER_ISR_DATA *);
  BOOL OverflowInterruptFlag;
  BOOL ComparatorInterruptFlag;
  UINT16 *Counter;
  UINT16 *Compare;
  INT16 Time;                       // 16-bit MSB
  void *PendingQueueOperation;      // Interrupted operations that are not complete
  TIMER_EVENT_NODE *EventQueue;     // List of events sorted by their time of occurrence
  TIMER_EVENT_NODE *FreeNodes;      // Pool of free nodes used to link events
  UINT8 *PortIFG;                   // Needed for software generation IO port interrupt
  UINT8 *PortIE;                    // Needed to re-enable IO port interrupt
  UINT8 PortPinBit;
} SOFTWARE_TIMER_ISR_DATA;

typedef struct TIMER_ISR_DATA {
  void (*TimerIntHandler)(struct TIMER_ISR_DATA *);
  BOOL *InterruptFlag;              // Pointer to interrupt flag field of SOFTWARE_TIMER_ISR_DATA structure.
  UINT16 *TimerControl;             // Needed to re-enable timer interrupt
  UINT16 TimerEnableBit;
  UINT8 *PortIFG;                   // Needed for software generation IO port interrupt
  UINT8 PortPinBit;
} TIMER_ISR_DATA;


typedef enum {InsertEventOp,DeleteEventOp} EVENTOP;

typedef struct {
  EVENTOP EventOp;
  volatile BOOL Done;
  TIMER_EVENT_NODE *Left;
  TIMER_EVENT_NODE *Node;
  volatile BOOL GeneratedInterrupt;
  INT32 Time;                       // Interrupt time of the event
  INT16 Version;                    // Backup of 16-bit MSB
} INSERTQUEUE_OP;

typedef struct {
  EVENTOP EventOp;
  volatile BOOL Done;
  TIMER_EVENT_NODE *Left;
  TIMER_EVENT_NODE *Node;
  volatile BOOL GenerateInterrupt;
  void *Event;
} DELETEQUEUE_OP;


static SOFTWARE_TIMER_ISR_DATA *SetSoftwareInterrupt(UINT8 softwareInterruptIndex);
static BOOL SetTimerInterrupts(SOFTWARE_TIMER_ISR_DATA *softwareDevice, UINT8 overflowInterruptIndex);
static void InsertQueueHelper(INSERTQUEUE_OP *des, SOFTWARE_TIMER_ISR_DATA *device, BOOL genInt);
static void DeleteQueueHelper(DELETEQUEUE_OP *des);
static TIMER_EVENT_NODE *GetFreeNode(SOFTWARE_TIMER_ISR_DATA *device);
static void ReleaseNode(SOFTWARE_TIMER_ISR_DATA *device, TIMER_EVENT_NODE *node);
static void SoftwareTimerIntHandler(SOFTWARE_TIMER_ISR_DATA *device);
static void TimerIntHandler(TIMER_ISR_DATA *device);


/* OSInitTimerEvent: Creates all the interrupt handlers needed to manage an event queue
** and their structures: a timer overflow and compare match and a software generated timer
** handler. The first 2 handlers are associated with a timer device and propagate their
** processing to the software generated timer. In all 3 interrupt handlers should be de-
** fined in ZottaOSconf.exe. This function also starts the timer device. */
BOOL OSInitTimerEvent(UINT8 nbNode, UINT8 softwareInterruptIndex, UINT8 overflowInterruptIndex)
{
  UINT8 i;
  SOFTWARE_TIMER_ISR_DATA *softwareDevice;
  if ((softwareDevice = SetSoftwareInterrupt(softwareInterruptIndex)) != NULL)
     if (SetTimerInterrupts(softwareDevice,overflowInterruptIndex)) {
        softwareDevice->Time = 0;
        softwareDevice->PendingQueueOperation = NULL;
        /* Initialize event queue */
        softwareDevice->EventQueue = NULL;
        /* Create a pool of free event nodes */
        softwareDevice->FreeNodes = (TIMER_EVENT_NODE *)OSMalloc(nbNode * sizeof(TIMER_EVENT_NODE));
        for (i = 0; i < nbNode - 1; i += 1)
           softwareDevice->FreeNodes[i].Next = &softwareDevice->FreeNodes[i+1];
        softwareDevice->FreeNodes[i].Next = NULL;
        /* Allow software timer interrupts */
        *softwareDevice->PortIE |= softwareDevice->PortPinBit;
        return TRUE;
     }
  return FALSE;
} /* end of OSInitTimerEvent */


/* SetSoftwareInterrupt: Creates and registers SoftwareTimerIntHandler that acts as the
** timer handler so that interrupts can be re-enabled as soon as possible. */
SOFTWARE_TIMER_ISR_DATA *SetSoftwareInterrupt(UINT8 softwareInterruptIndex)
{
  SOFTWARE_TIMER_ISR_DATA *softwareDevice;
  softwareDevice = (SOFTWARE_TIMER_ISR_DATA *)OSMalloc(sizeof(SOFTWARE_TIMER_ISR_DATA));
  softwareDevice->TimerIntHandler = SoftwareTimerIntHandler;
  softwareDevice->OverflowInterruptFlag = FALSE;
  softwareDevice->ComparatorInterruptFlag = FALSE;
  switch (softwareInterruptIndex) {  // Specific Port device configuration
     #ifdef OS_IO_PORT1_0
        case OS_IO_PORT1_0:
           softwareDevice->PortIFG = (UINT8 *)&P1IFG;
           softwareDevice->PortIE = (UINT8 *)&P1IE;
           softwareDevice->PortPinBit = BIT0;
           break;
     #endif
     #ifdef OS_IO_PORT1_1
        case OS_IO_PORT1_1:
           softwareDevice->PortIFG = (UINT8 *)&P1IFG;
           softwareDevice->PortIE = (UINT8 *)&P1IE;
           softwareDevice->PortPinBit = BIT1;
           break;
     #endif
     #ifdef OS_IO_PORT1_2
        case OS_IO_PORT1_2:
           softwareDevice->PortIFG = (UINT8 *)&P1IFG;
           softwareDevice->PortIE = (UINT8 *)&P1IE;
           softwareDevice->PortPinBit = BIT2;
           break;
     #endif
     #ifdef OS_IO_PORT1_3
        case OS_IO_PORT1_3:
           softwareDevice->PortIFG = (UINT8 *)&P1IFG;
           softwareDevice->PortIE = (UINT8 *)&P1IE;
           softwareDevice->PortPinBit = BIT3;
           break;
     #endif
     #ifdef OS_IO_PORT1_4
        case OS_IO_PORT1_4:
           softwareDevice->PortIFG = (UINT8 *)&P1IFG;
           softwareDevice->PortIE = (UINT8 *)&P1IE;
           softwareDevice->PortPinBit = BIT4;
           break;
     #endif
     #ifdef OS_IO_PORT1_5
        case OS_IO_PORT1_5:
           softwareDevice->PortIFG = (UINT8 *)&P1IFG;
           softwareDevice->PortIE = (UINT8 *)&P1IE;
           softwareDevice->PortPinBit = BIT5;
           break;
     #endif
     #ifdef OS_IO_PORT1_6
        case OS_IO_PORT1_6:
           softwareDevice->PortIFG = (UINT8 *)&P1IFG;
           softwareDevice->PortIE = (UINT8 *)&P1IE;
           softwareDevice->PortPinBit = BIT6;
           break;
     #endif
     #ifdef OS_IO_PORT1_7
        case OS_IO_PORT1_7:
           softwareDevice->PortIFG = (UINT8 *)&P1IFG;
           softwareDevice->PortIE = (UINT8 *)&P1IE;
           softwareDevice->PortPinBit = BIT7;
           break;
     #endif
     #ifdef OS_IO_PORT2_0
        case OS_IO_PORT2_0:
           softwareDevice->PortIFG = (UINT8 *)&P2IFG;
           softwareDevice->PortIE = (UINT8 *)&P2IE;
           softwareDevice->PortPinBit = BIT0;
           break;
     #endif
     #ifdef OS_IO_PORT2_1
        case OS_IO_PORT2_1:
           softwareDevice->PortIFG = (UINT8 *)&P2IFG;
           softwareDevice->PortIE = (UINT8 *)&P2IE;
           softwareDevice->PortPinBit = BIT1;
           break;
     #endif
     #ifdef OS_IO_PORT2_2
        case OS_IO_PORT2_2:
           softwareDevice->PortIFG = (UINT8 *)&P2IFG;
           softwareDevice->PortIE = (UINT8 *)&P2IE;
           softwareDevice->PortPinBit = BIT2;
           break;
     #endif
     #ifdef OS_IO_PORT2_3
        case OS_IO_PORT2_3:
           softwareDevice->PortIFG = (UINT8 *)&P2IFG;
           softwareDevice->PortIE = (UINT8 *)&P2IE;
           softwareDevice->PortPinBit = BIT3;
           break;
     #endif
     #ifdef OS_IO_PORT2_4
        case OS_IO_PORT2_4:
           softwareDevice->PortIFG = (UINT8 *)&P2IFG;
           softwareDevice->PortIE = (UINT8 *)&P2IE;
           softwareDevice->PortPinBit = BIT4;
           break;
     #endif
     #ifdef OS_IO_PORT2_5
        case OS_IO_PORT2_5:
           softwareDevice->PortIFG = (UINT8 *)&P2IFG;
           softwareDevice->PortIE = (UINT8 *)&P2IE;
           softwareDevice->PortPinBit = BIT5;
           break;
     #endif
     #ifdef OS_IO_PORT2_6
        case OS_IO_PORT2_6:
           softwareDevice->PortIFG = (UINT8 *)&P2IFG;
           softwareDevice->PortIE = (UINT8 *)&P2IE;
           softwareDevice->PortPinBit = BIT6;
           break;
     #endif
     #ifdef OS_IO_PORT2_7
        case OS_IO_PORT2_7:
           softwareDevice->PortIFG = (UINT8 *)&P2IFG;
           softwareDevice->PortIE = (UINT8 *)&P2IE;
           softwareDevice->PortPinBit = BIT7;
           break;
     #endif
     #ifdef OS_IO_PORT3_0
        case OS_IO_PORT3_0:
           softwareDevice->PortIFG = (UINT8 *)&P3IFG;
           softwareDevice->PortIE = (UINT8 *)&P3IE;
           softwareDevice->PortPinBit = BIT0;
           break;
     #endif
     #ifdef OS_IO_PORT3_1
        case OS_IO_PORT3_1:
           softwareDevice->PortIFG = (UINT8 *)&P3IFG;
           softwareDevice->PortIE = (UINT8 *)&P3IE;
           softwareDevice->PortPinBit = BIT1;
           break;
     #endif
     #ifdef OS_IO_PORT3_2
        case OS_IO_PORT3_2:
           softwareDevice->PortIFG = (UINT8 *)&P3IFG;
           softwareDevice->PortIE = (UINT8 *)&P3IE;
           softwareDevice->PortPinBit = BIT2;
           break;
     #endif
     #ifdef OS_IO_PORT3_3
        case OS_IO_PORT3_3:
           softwareDevice->PortIFG = (UINT8 *)&P3IFG;
           softwareDevice->PortIE = (UINT8 *)&P3IE;
           softwareDevice->PortPinBit = BIT3;
           break;
     #endif
     #ifdef OS_IO_PORT3_4
        case OS_IO_PORT3_4:
           softwareDevice->PortIFG = (UINT8 *)&P3IFG;
           softwareDevice->PortIE = (UINT8 *)&P3IE;
           softwareDevice->PortPinBit = BIT4;
           break;
     #endif
     #ifdef OS_IO_PORT3_5
        case OS_IO_PORT3_5:
           softwareDevice->PortIFG = (UINT8 *)&P3IFG;
           softwareDevice->PortIE = (UINT8 *)&P3IE;
           softwareDevice->PortPinBit = BIT5;
           break;
     #endif
     #ifdef OS_IO_PORT3_6
        case OS_IO_PORT3_6:
           softwareDevice->PortIFG = (UINT8 *)&P3IFG;
           softwareDevice->PortIE = (UINT8 *)&P3IE;
           softwareDevice->PortPinBit = BIT6;
           break;
     #endif
     #ifdef OS_IO_PORT3_7
        case OS_IO_PORT3_7:
           softwareDevice->PortIFG = (UINT8 *)&P3IFG;
           softwareDevice->PortIE = (UINT8 *)&P3IE;
           softwareDevice->PortPinBit = BIT7;
           break;
     #endif
     #ifdef OS_IO_PORT4_0
        case OS_IO_PORT4_0:
           softwareDevice->PortIFG = (UINT8 *)&P4IFG;
           softwareDevice->PortIE = (UINT8 *)&P4IE;
           softwareDevice->PortPinBit = BIT0;
           break;
     #endif
     #ifdef OS_IO_PORT4_1
        case OS_IO_PORT4_1:
           softwareDevice->PortIFG = (UINT8 *)&P4IFG;
           softwareDevice->PortIE = (UINT8 *)&P4IE;
           softwareDevice->PortPinBit = BIT1;
           break;
     #endif
     #ifdef OS_IO_PORT4_2
        case OS_IO_PORT4_2:
           softwareDevice->PortIFG = (UINT8 *)&P4IFG;
           softwareDevice->PortIE = (UINT8 *)&P4IE;
           softwareDevice->PortPinBit = BIT2;
           break;
     #endif
     #ifdef OS_IO_PORT4_3
        case OS_IO_PORT4_3:
           softwareDevice->PortIFG = (UINT8 *)&P4IFG;
           softwareDevice->PortIE = (UINT8 *)&P4IE;
           softwareDevice->PortPinBit = BIT3;
           break;
     #endif
     #ifdef OS_IO_PORT4_4
        case OS_IO_PORT4_4:
           softwareDevice->PortIFG = (UINT8 *)&P4IFG;
           softwareDevice->PortIE = (UINT8 *)&P4IE;
           softwareDevice->PortPinBit = BIT4;
           break;
     #endif
     #ifdef OS_IO_PORT4_5
        case OS_IO_PORT4_5:
           softwareDevice->PortIFG = (UINT8 *)&P4IFG;
           softwareDevice->PortIE = (UINT8 *)&P4IE;
           softwareDevice->PortPinBit = BIT5;
           break;
     #endif
     #ifdef OS_IO_PORT4_6
        case OS_IO_PORT4_6:
           softwareDevice->PortIFG = (UINT8 *)&P4IFG;
           softwareDevice->PortIE = (UINT8 *)&P4IE;
           softwareDevice->PortPinBit = BIT6;
           break;
     #endif
     #ifdef OS_IO_PORT4_7
        case OS_IO_PORT4_7:
           softwareDevice->PortIFG = (UINT8 *)&P4IFG;
           softwareDevice->PortIE = (UINT8 *)&P4IE;
           softwareDevice->PortPinBit = BIT7;
           break;
     #endif
        default:
           return NULL;
  }
  OSSetISRDescriptor(softwareInterruptIndex,softwareDevice);
  return softwareDevice;
} /* end of SetSoftwareInterrupt */


/* SetTimerInterrupts: Creates and registers TimerIntHandler to catch timer overflow and
** comparator match interrupts. These interrupts simply propagate the interrupt to the
** software timer. Note that the CC1 is used as comparator. */
BOOL SetTimerInterrupts(SOFTWARE_TIMER_ISR_DATA *softwareDevice, UINT8 overflowInterruptIndex)
{
  UINT16 clockSourceSelect;
  TIMER_ISR_DATA *comparatorDevice;
  TIMER_ISR_DATA *overflowDevice = (TIMER_ISR_DATA *)OSMalloc(sizeof(TIMER_ISR_DATA));
  overflowDevice->TimerIntHandler = TimerIntHandler;
  overflowDevice->InterruptFlag = &softwareDevice->OverflowInterruptFlag;
  overflowDevice->PortIFG = softwareDevice->PortIFG;
  overflowDevice->PortPinBit = softwareDevice->PortPinBit;
  comparatorDevice = (TIMER_ISR_DATA *)OSMalloc(sizeof(TIMER_ISR_DATA));
  comparatorDevice->TimerIntHandler = TimerIntHandler;
  comparatorDevice->InterruptFlag = &softwareDevice->ComparatorInterruptFlag;
  comparatorDevice->PortIFG = softwareDevice->PortIFG;
  comparatorDevice->PortPinBit = softwareDevice->PortPinBit;
  switch (overflowInterruptIndex) {  // Specific timer device configuration
     #if defined(OS_IO_TIMER0_A1_TA) && defined(OS_IO_TIMER0_A1_CC1)
        case OS_IO_TIMER0_A1_TA:
           overflowDevice->TimerControl = (UINT16 *)&TA0CTL;
           overflowDevice->TimerEnableBit = TAIE;
           softwareDevice->Counter = (UINT16 *)&TA0R;
           softwareDevice->Compare = (UINT16 *)&TA0CCR1;
           comparatorDevice->TimerControl = (UINT16 *)&TA0CCTL1;
           comparatorDevice->TimerEnableBit = CCIE;
           OSSetISRDescriptor(OS_IO_TIMER0_A1_CC1,comparatorDevice);
           clockSourceSelect = TASSEL_1;
           break;
     #endif
     #if defined(OS_IO_TIMER1_A1_TA) && defined(OS_IO_TIMER1_A1_CC1)
        case OS_IO_TIMER1_A1_TA:
           overflowDevice->TimerControl = (UINT16 *)&TA1CTL;
           overflowDevice->TimerEnableBit = TAIE;
           softwareDevice->Counter = (UINT16 *)&TA1R;
           softwareDevice->Compare = (UINT16 *)&TA1CCR1;
           comparatorDevice->TimerControl = (UINT16 *)&TA1CCTL1;
           comparatorDevice->TimerEnableBit = CCIE;
           OSSetISRDescriptor(OS_IO_TIMER1_A1_CC1,comparatorDevice);
           clockSourceSelect = TASSEL_1;
           break;
     #endif
     #if defined(OS_IO_TIMER2_A1_TA) && defined(OS_IO_TIMER2_A1_CC1)
        case OS_IO_TIMER2_A1_TA:
           overflowDevice->TimerControl = (UINT16 *)&TA2CTL;
           overflowDevice->TimerEnableBit = TAIE;
           softwareDevice->Counter = (UINT16 *)&TA2R;
           softwareDevice->Compare = (UINT16 *)&TA2CCR1;
           comparatorDevice->TimerControl = (UINT16 *)&TA2CCTL1;
           comparatorDevice->TimerEnableBit = CCIE;
           OSSetISRDescriptor(OS_IO_TIMER2_A1_CC1,comparatorDevice);
           clockSourceSelect = TASSEL_1;
           break;
     #endif
     #if defined(OS_IO_TIMER3_A1_TA) && defined(OS_IO_TIMER3_A1_CC1)
        case OS_IO_TIMER3_A1_TA:
           overflowDevice->TimerControl = (UINT16 *)&TA3CTL;
           overflowDevice->TimerEnableBit = TAIE;
           softwareDevice->Counter = (UINT16 *)&TA3R;
           softwareDevice->Compare = (UINT16 *)&TA3CCR1;
           comparatorDevice->TimerControl = (UINT16 *)&TA3CCTL1;
           comparatorDevice->TimerEnableBit = CCIE;
           OSSetISRDescriptor(OS_IO_TIMER3_A1_CC1,comparatorDevice);
           clockSourceSelect = TASSEL_1;
           break;
     #endif
     #if defined(OS_IO_TIMERA1_TA) && defined(OS_IO_TIMERA1_CC1)
        case OS_IO_TIMERA1_TA:
           overflowDevice->TimerControl = (UINT16 *)&TACTL;
           overflowDevice->TimerEnableBit = TAIE;
           softwareDevice->Counter = (UINT16 *)&TAR;
           softwareDevice->Compare = (UINT16 *)&TACCR1;
           comparatorDevice->TimerControl = (UINT16 *)&TACCTL1;
           comparatorDevice->TimerEnableBit = CCIE;
           OSSetISRDescriptor(OS_IO_TIMERA1_CC1,comparatorDevice);
           clockSourceSelect = TASSEL_1;
           break;
     #endif
     #if defined(OS_IO_TIMER0_B1_TB) && defined(OS_IO_TIMER0_B1_CC1)
        case OS_IO_TIMER0_B1_TB:
           overflowDevice->TimerControl = (UINT16 *)&TB0CTL;
           overflowDevice->TimerEnableBit = TBIE;
           softwareDevice->Counter = (UINT16 *)&TB0R;
           softwareDevice->Compare = (UINT16 *)&TB0CCR1;
           comparatorDevice->TimerControl = (UINT16 *)&TB0CCTL1;
           comparatorDevice->TimerEnableBit = CCIE;
           OSSetISRDescriptor(OS_IO_TIMER0_B1_CC1,comparatorDevice);
           clockSourceSelect = TBSSEL_1;
           break;
     #endif
     #if defined(OS_IO_TIMER1_B1_TB) && defined(OS_IO_TIMER1_B1_CC1)
        case OS_IO_TIMER1_B1_TB:
           overflowDevice->TimerControl = (UINT16 *)&TB1CTL;
           overflowDevice->TimerEnableBit = TBIE;
           softwareDevice->Counter = (UINT16 *)&TB1R;
           softwareDevice->Compare = (UINT16 *)&TB1CCR1;
           comparatorDevice->TimerControl = (UINT16 *)&TB1CCTL1;
           comparatorDevice->TimerEnableBit = CCIE;
           OSSetISRDescriptor(OS_IO_TIMER1_B1_CC1,comparatorDevice);
           clockSourceSelect = TBSSEL_1;
           break;
     #endif
     #if defined(OS_IO_TIMER2_B1_TB) && defined(OS_IO_TIMER2_B1_CC1)
        case OS_IO_TIMER2_B1_TB:
           overflowDevice->TimerControl = (UINT16 *)&TB2CTL;
           overflowDevice->TimerEnableBit = TBIE;
           softwareDevice->Counter = (UINT16 *)&TB2R;
           softwareDevice->Compare = (UINT16 *)&TB2CCR1;
           comparatorDevice->TimerControl = (UINT16 *)&TB2CCTL1;
           comparatorDevice->TimerEnableBit = CCIE;
           OSSetISRDescriptor(OS_IO_TIMER2_B1_CC1,comparatorDevice);
           clockSourceSelect = TBSSEL_1;
           break;
     #endif
     #if defined(OS_IO_TIMERB1_TB) && defined(OS_IO_TIMERB1_CC1)
        case OS_IO_TIMERB1_TB:
           overflowDevice->TimerControl = (UINT16 *)&TBCTL;
           overflowDevice->TimerEnableBit = TBIE;
           softwareDevice->Counter = (UINT16 *)&TBR;
           softwareDevice->Compare = (UINT16 *)&TBCCR1;
           comparatorDevice->TimerControl = (UINT16 *)&TBCCTL1;
           comparatorDevice->TimerEnableBit = CCIE;
           OSSetISRDescriptor(OS_IO_TIMERB1_CC1,comparatorDevice);
           clockSourceSelect = TBSSEL_1;
           break;
     #endif
     #if defined(OS_IO_TIMER0_D1_TD) && defined(OS_IO_TIMER0_D1_CC1)
        case OS_IO_TIMER0_D1_TD:
           overflowDevice->TimerControl = (UINT16 *)&TD0CTL0;
           overflowDevice->TimerEnableBit = TDIE;
           softwareDevice->Counter = (UINT16 *)&TD0R;
           softwareDevice->Compare = (UINT16 *)&TD0CCR1;
           comparatorDevice->TimerControl = (UINT16 *)&TD0CCTL1;
           comparatorDevice->TimerEnableBit = CCIE;
           OSSetISRDescriptor(OS_IO_TIMER0_D1_CC1,comparatorDevice);
           clockSourceSelect = TDSSEL_1;
           break;
     #endif
     #if defined(OS_IO_TIMER1_D1_TD) && defined(OS_IO_TIMER1_D1_CC1)
        case OS_IO_TIMER1_D1_TD:
           overflowDevice->TimerControl = (UINT16 *)&TD1CTL0;
           overflowDevice->TimerEnableBit = TDIE;
           softwareDevice->Counter = (UINT16 *)&TD1R;
           softwareDevice->Compare = (UINT16 *)&TD1CCR1;
           comparatorDevice->TimerControl = (UINT16 *)&TD1CCTL1;
           comparatorDevice->TimerEnableBit = CCIE;
           OSSetISRDescriptor(OS_IO_TIMER1_D1_CC1,comparatorDevice);
           clockSourceSelect = TDSSEL_1;
           break;
     #endif
        default:
           return FALSE;
  }
  OSSetISRDescriptor(overflowInterruptIndex,overflowDevice);
  /* Start the timer */
  *comparatorDevice->TimerControl |= comparatorDevice->TimerEnableBit;
  *overflowDevice->TimerControl |= clockSourceSelect | overflowDevice->TimerEnableBit | MC_2;
  return TRUE;
} /* end of SetTimerInterrupts */


/* OSScheduleTimerEvent: Entry point to insert an event into the event list associated
** with a timer. */
BOOL OSScheduleTimerEvent(void *event, UINT32 delay, UINT8 interruptIndex)
{
  TIMER_EVENT_NODE *timerEventNode;
  SOFTWARE_TIMER_ISR_DATA *device;
  INSERTQUEUE_OP des, *pendingOp;
  device = (SOFTWARE_TIMER_ISR_DATA *)OSGetISRDescriptor(interruptIndex);
  if ((timerEventNode = GetFreeNode(device)) == NULL)
     return FALSE;
  /* Because the timer ISR can shift the current time, the time at which this event oc-
  ** curs is done in 2 steps. The time is determined here and then transferred to the
  ** event at the last moment so that if there is a time shift, it can be detected and
  ** corrected. */
  do {
     des.Version = device->Time; // Save current 16-bit MSB time to detect time shifting
     des.Time = *device->Counter + delay + ((INT32)des.Version << 16);
  } while (des.Version != device->Time);
  timerEventNode->Time = -1;     // Set at an uninitialized sentinel value
  timerEventNode->Event = event;
  des.EventOp = InsertEventOp;   // Prepare the insertion so that other task may
  des.Done = FALSE;              // complete it.
  des.Left = (TIMER_EVENT_NODE *)&device->EventQueue;
  des.Node = timerEventNode;
  des.GeneratedInterrupt = FALSE;
  pendingOp = (INSERTQUEUE_OP *)device->PendingQueueOperation;
  if (pendingOp != NULL) {       // Is there an incomplete operation under way?
     if (pendingOp->EventOp == InsertEventOp)
        InsertQueueHelper(pendingOp,device,TRUE);
     else
        DeleteQueueHelper((DELETEQUEUE_OP *)pendingOp);
  }
  /* Mark the node to allow a single insertion of the node */
  timerEventNode->Next = (TIMER_EVENT_NODE *)&des;
  device->PendingQueueOperation = &des;    // Post the insertion for other tasks
  InsertQueueHelper(&des,device,TRUE);     // Do the insertion
  device->PendingQueueOperation = NULL;
  return TRUE;
} /* end of OSScheduleTimerEvent */


/* InsertQueueHelper: Performs the insertion described by a descriptor into a sorted
** queue. Whenever an enqueuer is interrupted in the midst of the insertion by another
** task, the latter task inserts the node of the interrupted enqueuer before completing
** its own operation.
** Parameters:
**   (1) (INSERTQUEUE_OP *) insertion operation to perform;
**   (2) (TIMER_ISR_DATA *) descriptor to the timer in order to retrieve the current time
**          and to generate an interrupt if needed;
**   (3) (BOOL) TRUE when not called from the timer ISR (SoftwareTimerIntHandler). */
void InsertQueueHelper(INSERTQUEUE_OP *des, SOFTWARE_TIMER_ISR_DATA *device, BOOL genInterrupt)
{
  INT32 scheduledTime;
  TIMER_EVENT_NODE *right, *left = des->Left;
  /* Finish completing the time at which the event takes place. This is done here so that
  ** the timer ISR can correct this time. Because the timer ISR shifts its current time
  ** to avoid overflow, the event occurrence time may not be finalized without the ISR's
  ** awareness. */
  if (OSINT32_LL(&des->Node->Time) == -1) {
     if (des->Version > device->Time) // If 16-bit MSB backup is greater than current a time shift occurred
        scheduledTime = des->Time - SHIFT_TIME_LIMIT;
     else
        scheduledTime = des->Time;
     while (OSINT32_LL(&des->Node->Time) == -1)
        if (OSINT32_SC(&des->Node->Time,scheduledTime))
           break;
  }
  while (TRUE) {         // Loop until done
     right = left->Next; // Get adjacent node
     if (des->Done)      // Has a higher priority task finished the work?
        break;
     else if (right == des->Node) { // Is the node already in the queue?
        des->Done = TRUE;           // Mark that node has been inserted
        break;
     }
     else if (right == NULL || des->Node->Time < right->Time) {
        UINTPTR *successorNode = (UINTPTR *)&des->Node->Next;
        while (OSUINTPTR_LL(successorNode) == (UINTPTR)des)
           if (OSUINTPTR_SC(successorNode,(UINTPTR)right))
              break;
        while (OSUINTPTR_LL((UINTPTR *)&left->Next) == (UINTPTR)right) {
           /* At this point an ABA problem can arise: Say that a task inserts node N
           ** between nodes A and B, and gets preempted. Another task completes the in-
           ** sertion, and then removes N. In this case, node N must not be reinserted.
           ** To correct this flaw, we simply need to check if another task completed the
           ** current insertion. */
           if (des->Done || OSUINTPTR_SC((UINTPTR *)&left->Next,(UINTPTR)des->Node))
              break;
        }
        des->Done = TRUE; // Mark that node has been inserted
        break;
     }
     else
        left = des->Left = right;
  }
  /* Generate a timer comparator interrupt if the inserted event is the first one. */
  if (genInterrupt && device->EventQueue == des->Node) {
     do {
        OSUINT16_LL(device->Compare);
        if (des->GeneratedInterrupt) return;
     } while (!OSUINT16_SC(device->Compare,des->Node->Time));
     if (*device->Compare <= *device->Counter)
        *device->PortIFG |= device->PortPinBit;
  }
  des->GeneratedInterrupt = TRUE;
} /* end of InsertQueueHelper */


/* OSUnScheduleTimerEvent: Entry point to remove the first occurrence of a specified
** event from the list associated with a timer. */
BOOL OSUnScheduleTimerEvent(void *event, UINT8 interruptIndex)
{
  DELETEQUEUE_OP des, *pendingOp;
  SOFTWARE_TIMER_ISR_DATA *device;
  des.EventOp = DeleteEventOp;  // Prepare the removal so that other task may complete
  des.Done = FALSE;             // it.
  device = (SOFTWARE_TIMER_ISR_DATA *)OSGetISRDescriptor(interruptIndex);
  des.Left = (TIMER_EVENT_NODE *)&device->EventQueue;
  des.Event = event;
  des.Node = (TIMER_EVENT_NODE *)&des;
  pendingOp = device->PendingQueueOperation;
  if (pendingOp != NULL) {      // Is there an incomplete operation under way?
     if (pendingOp->EventOp == InsertEventOp)
        InsertQueueHelper((INSERTQUEUE_OP *)pendingOp,device,TRUE);
     else
        DeleteQueueHelper(pendingOp);
  }
  device->PendingQueueOperation = &des;    // Post the removal for other tasks
  DeleteQueueHelper(&des);                 // Do the operation
  device->PendingQueueOperation = NULL;
  if (des.Node != (TIMER_EVENT_NODE *)&des) {
     ReleaseNode(device,des.Node);
     return TRUE;
  }
  else
     return FALSE;
} /* end of OSUnScheduleTimerEvent */


/* DeleteQueueHelper: Performs the delete operation described by a descriptor from a list
** of events. Whenever a deleter is interrupted in the midst of this operation by another
** task, the latter task removes the node of the interrupted deleter before completing
** its own operation. */
void DeleteQueueHelper(DELETEQUEUE_OP *des)
{
  TIMER_EVENT_NODE *right, *left = des->Left;
  while (TRUE) {             // Loop until done
     right = left->Next;     // Get adjacent node
     if (des->Done)          // Has a higher priority task finished the work?
        break;
     else if (right == NULL) {  // Is the sought node already removed from the queue?
        des->Done = TRUE;       // Mark that node has been removed
        break;
     }
     else if (des->Event == right->Event) {
        while (OSUINTPTR_LL((UINTPTR *)&des->Node) == (UINTPTR)des && !des->Done)
           if (OSUINTPTR_SC((UINTPTR *)&des->Node,(UINTPTR)right))
              break;
        while (OSUINTPTR_LL((UINTPTR *)&left->Next) == (UINTPTR)right)
           if (des->Done || OSUINTPTR_SC((UINTPTR *)&left->Next,(UINTPTR)right->Next))
              break;
        des->Done = TRUE;    // Mark that the node has been removed
        break;
     }
     else
        left = des->Left = right;
  }
} /* end of DeleteQueueHelper */


/* GetFreeNode: : Returns a node from the pool of free nodes. NULL is returned when
** none are available. The free node list is considered to be a stack of nodes. */
TIMER_EVENT_NODE *GetFreeNode(SOFTWARE_TIMER_ISR_DATA *des)
{
  TIMER_EVENT_NODE *node;
  while ((node = (TIMER_EVENT_NODE *)OSUINTPTR_LL((UINTPTR *)&des->FreeNodes)) != NULL)
     if (OSUINTPTR_SC((UINTPTR *)&des->FreeNodes,(UINTPTR)node->Next))
        break;
  return node;
} /* end of GetFreeNode */


/* ReleaseNode: Returns a node to the pool of free nodes. */
void ReleaseNode(SOFTWARE_TIMER_ISR_DATA *device, TIMER_EVENT_NODE *freeNode)
{
  while (TRUE) {
     freeNode->Next = (TIMER_EVENT_NODE *)OSUINTPTR_LL((UINTPTR *)&device->FreeNodes);
     if (OSUINTPTR_SC((UINTPTR *)&device->FreeNodes,(UINTPTR)freeNode))
        return;
  }
} /* end of ReleaseNode */


/* SoftwareTimerIntHandler: Software timer interrupt handler. */
void SoftwareTimerIntHandler(SOFTWARE_TIMER_ISR_DATA *device)
{
  TIMER_EVENT_NODE *eventNode;
  INSERTQUEUE_OP *pendingOp;
  INT32 timeMSB;
  /* First finalize any pending operation in case there is an inserted node that already
  ** has its occurrence time but is not yet in the queue. */
  pendingOp = (INSERTQUEUE_OP *)device->PendingQueueOperation;
  if (pendingOp != NULL) {          // Is there an incomplete operation under way?
     if (pendingOp->EventOp == InsertEventOp)
        InsertQueueHelper(pendingOp,device,FALSE);
     else
        DeleteQueueHelper((DELETEQUEUE_OP *)pendingOp);
  }
  device->PendingQueueOperation = NULL;
  do {
     /* shifting of the temporal values. */
     if (device->OverflowInterruptFlag) {          // Is timer overflow interrupt?
        device->OverflowInterruptFlag = FALSE;     // Clear interrupt flag
        device->Time += 1;                         // Increment MSB of time
        if (device->Time >= SHIFT_TIME_LIMIT_16) { // Shift all time value
           eventNode = device->EventQueue;
           while (eventNode != NULL) {
              eventNode->Time -= SHIFT_TIME_LIMIT;
              eventNode = eventNode->Next;
           }
           device->Time -= SHIFT_TIME_LIMIT_16;
        }
     }
     while (TRUE) {
        *device->Compare = 0;                      // Disable timer comparator
        device->ComparatorInterruptFlag = FALSE;   // Clear interrupt flag
        /* Schedule events that now occur */
        timeMSB = (INT32)device->Time << 16;
        while ((eventNode = device->EventQueue) != NULL && eventNode->Time <=
                                                         (timeMSB | *device->Counter)) {
           OSScheduleSuspendedTask(eventNode->Event);
           device->EventQueue = eventNode->Next;
           ReleaseNode(device,eventNode);
        }
        /* Program the next timer comparator */
        if (eventNode != NULL && (eventNode->Time & 0xFFFF0000) == timeMSB) {
           *device->Compare = (UINT16)eventNode->Time;
           if (*device->Compare > *device->Counter)
              break;
        }
        else
           break;
     }
     *device->PortIFG &= ~device->PortPinBit;
  } while (device->ComparatorInterruptFlag || device->OverflowInterruptFlag);
  *device->PortIE |= device->PortPinBit;
} /* end of SoftwareTimerIntHandler */


/* TimerIntHandler: Overflow or comparator timer interrupt handler. */
void TimerIntHandler(TIMER_ISR_DATA *device)
{
  *device->InterruptFlag = TRUE;                   // Save the state
  *device->PortIFG |= device->PortPinBit;          // Generate software interrupt
  *device->TimerControl |= device->TimerEnableBit; // Re-enable timer interrupt
} /* end of TimerIntHandler */
