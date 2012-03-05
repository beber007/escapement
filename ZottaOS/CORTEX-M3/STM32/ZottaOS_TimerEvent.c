/* Copyright (c) 2012 MIS Institute of the HEIG affiliated to the University of Applied
** Sciences of Western Switzerland. All rights reserved.
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
/* File TimerEvent.c: Implements an event queue that is associated with a timer device.
** Events that are inserted into the queue are specified along with a delay, and as soon
** as this delay has expired, the event is scheduled.
**
** Version identifier: February 2012
** Authors: MIS-TIC */

#include "ZottaOS_CortexM3.h"
#include "ZottaOS.h"
#include "ZottaOS_TimerEvent.h"

/* Because the timer continues ticking, when we wish to set a new value for the timer
** comparator, the difference in time between the new value and the previous must be such
** that when the assignment is done, the timer has not passed the comparator value. This
** is guaranteed by TIMER_OFFSET. */
#define TIMER_OFFSET 2
/* Because events are sorted by the time at which they occur, and that this time is mon-
** otonically increasing, the relative time reference wraparounds. We therefore need to
** periodically shift the occurrence times. This is done very SHIFT_TIME_LIMIT tics. */
#define SHIFT_TIME_LIMIT 0x40000000 // = 2^30


#define BASE_TIM2 0x40000000
#define BASE_TIM3 0x40000400
#define BASE_TIM4 0x40000800
#define BASE_TIM5 0x40000C00

#define OFFSET_STATUS           0x10
#define OFFSET_EVENT_GENERATION 0x14
#define OFFSET_COUNTER          0x24
#define OFFSET_COMPARATOR       0x34


typedef struct TIMER_EVENT_NODE { // Blocks that in the event queue
  struct TIMER_EVENT_NODE *Next;
  void *Event;
  INT32 Time;
} TIMER_EVENT_NODE;

typedef struct { // Queue of events with their associated time of occurrence
  TIMER_EVENT_NODE *Next;
} TIMER_EVENT_NODE_HEAD;

typedef struct TIMER_ISR_DATA {
  void (*TimerIntHandler)(struct TIMER_ISR_DATA *);
  UINT16 *Status;
  UINT16 *EventGeneration;
  UINT16 *Counter;
  UINT16 *Comparator;
  INT32 Time;                        // Current time
  void *PendingQueueOperation;       // Interrupted operations that are not complete
  TIMER_EVENT_NODE_HEAD *EventQueue; // List of events sorted by their time of occurrence
  TIMER_EVENT_NODE *FreeNodes;       // Pool of free nodes used to link events
} TIMER_ISR_DATA;

typedef enum {InsertEventOp,DeleteEventOp} EVENTOP;

typedef struct {
  EVENTOP EventOp;
  volatile BOOL Done;
  TIMER_EVENT_NODE *Left;
  TIMER_EVENT_NODE *Node;
  volatile BOOL GeneratedInterrupt;
} INSERTQUEUE_OP;

typedef struct {
  EVENTOP EventOp;
  volatile BOOL Done;
  TIMER_EVENT_NODE *Left;
  TIMER_EVENT_NODE *Node;
  volatile BOOL GenerateInterrupt;
  void *Event;
} DELETEQUEUE_OP;


static void InsertQueueHelper(INSERTQUEUE_OP *des, TIMER_ISR_DATA *device, BOOL genInt);
static void DeleteQueueHelper(DELETEQUEUE_OP *des);
static TIMER_EVENT_NODE *GetFreeNode(TIMER_ISR_DATA *device);
static void ReleaseNode(TIMER_ISR_DATA *device, TIMER_EVENT_NODE *node);
static void TimerIntHandler(TIMER_ISR_DATA *device);

/* OSInitTimerEvent: Creates an ISR descriptor block holding the specifics of a timer
** device that is used as an event handler and which can schedule a list of event at
** their occurrence time. */
void OSInitTimerEvent(UINT8 nbNode, UINT8 interruptIndex)
{
  TIMER_ISR_DATA *device;
  UINT8 i;
  device = (TIMER_ISR_DATA *)OSMalloc(sizeof(TIMER_ISR_DATA));
  device->TimerIntHandler = TimerIntHandler;
  OSSetISRDescriptor(interruptIndex,device);
  switch (interruptIndex) { /* Specific timer device registers */
     case OS_IO_TIM2:
        device->Status = (UINT16 *)(BASE_TIM2 + OFFSET_STATUS);
        device->EventGeneration = (UINT16 *)(BASE_TIM2 + OFFSET_EVENT_GENERATION);
        device->Counter = (UINT16 *)(BASE_TIM2 + OFFSET_COUNTER);
        device->Comparator = (UINT16 *)(BASE_TIM2 + OFFSET_COMPARATOR);
        break;
     case OS_IO_TIM3:
        device->Status = (UINT16 *)(BASE_TIM3 + OFFSET_STATUS);
        device->EventGeneration = (UINT16 *)(BASE_TIM3 + OFFSET_EVENT_GENERATION);
        device->Counter = (UINT16 *)(BASE_TIM3 + OFFSET_COUNTER);
        device->Comparator = (UINT16 *)(BASE_TIM3 + OFFSET_COMPARATOR);
        break;
     case OS_IO_TIM4:
        device->Status = (UINT16 *)(BASE_TIM4 + OFFSET_STATUS);
        device->EventGeneration = (UINT16 *)(BASE_TIM4 + OFFSET_EVENT_GENERATION);
        device->Counter = (UINT16 *)(BASE_TIM4 + OFFSET_COUNTER);
        device->Comparator = (UINT16 *)(BASE_TIM4 + OFFSET_COMPARATOR);
        break;
     case OS_IO_TIM5:
        device->Status = (UINT16 *)(BASE_TIM5 + OFFSET_STATUS);
        device->EventGeneration = (UINT16 *)(BASE_TIM5 + OFFSET_EVENT_GENERATION);
        device->Counter = (UINT16 *)(BASE_TIM5 + OFFSET_COUNTER);
        device->Comparator = (UINT16 *)(BASE_TIM5 + OFFSET_COMPARATOR);
     default: break;
  }
  device->Time = 0;
  device->PendingQueueOperation = NULL;
  /* Initialize event queue */
  device->EventQueue = (TIMER_EVENT_NODE_HEAD *)OSMalloc(sizeof(TIMER_EVENT_NODE_HEAD));
  device->EventQueue->Next = NULL;
  /* Create a pool of free event nodes */
  device->FreeNodes = (TIMER_EVENT_NODE *)OSMalloc(nbNode * sizeof(TIMER_EVENT_NODE));
  for (i = 0; i < nbNode - 1; i += 1)
     device->FreeNodes[i].Next = &device->FreeNodes[i+1];
  device->FreeNodes[i].Next = NULL;
} /* end of OSInitTimerEvent */


/* OSScheduleTimerEvent: Entry point to insert an event into the event list associated
** with a timer. */
BOOL OSScheduleTimerEvent(void *event, UINT32 delay, UINT8 interruptIndex)
{
  TIMER_EVENT_NODE *timerEventNode;
  TIMER_ISR_DATA *device;
  INSERTQUEUE_OP des, *pendingOp;
  device = (TIMER_ISR_DATA *)OSGetISRDescriptor(interruptIndex);
  if ((timerEventNode = GetFreeNode(device)) == NULL)
     return FALSE;
  /* Because the timer ISR can shift the current time, the time at which this event oc-
  ** curs is done in 2 steps. */
  timerEventNode->Time = (*device->Counter + delay) | 0x80000000;
  timerEventNode->Event = event;
  des.EventOp = InsertEventOp;  // Prepare the insertion so that other task may complete
  des.Done = FALSE;             // it.
  des.Left = (TIMER_EVENT_NODE *)device->EventQueue;
  des.Node = timerEventNode;
  des.GeneratedInterrupt = FALSE;
  pendingOp = (INSERTQUEUE_OP *)device->PendingQueueOperation;
  if (pendingOp != NULL) {      // Is there an incomplete operation under way?
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
**   (3) (BOOL) TRUE when not called from the timer ISR. */
void InsertQueueHelper(INSERTQUEUE_OP *des, TIMER_ISR_DATA *device, BOOL genInterrupt)
{
  INT32 scheduledTime;
  TIMER_EVENT_NODE *right, *left = des->Left;
  /* Finish completing the time at which the event takes place. This is done here so that
  ** the timer ISR can set this time. Because the timer ISR shifts its current time to
  ** avoid overflow, the event occurrence time may not be finalized without the ISR's
  ** awareness. */
  while ((scheduledTime = OSINT32_LL(&des->Node->Time)) < 0)
     if (OSINT32_SC(&des->Node->Time,(scheduledTime ^ 0x80000000) + device->Time))
        break;
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
        while (_OSUINTPTR_LL(successorNode) == (UINTPTR)des)
           if (_OSUINTPTR_SC(successorNode,(UINTPTR)right))
              break;
        while (_OSUINTPTR_LL((UINTPTR *)&left->Next) == (UINTPTR)right) {
           /* At this point an ABA problem can arise: Say that a task inserts node N
           ** between nodes A and B, and gets preempted. Another task completes the in-
           ** sertion, and then removes N. In this case, node N must not be reinserted.
           ** To correct this flaw, we simply need to check if another task completed the
           ** current insertion. */
           if (des->Done || _OSUINTPTR_SC((UINTPTR *)&left->Next,(UINTPTR)des->Node))
              break;
        }
        des->Done = TRUE; // Mark that node has been inserted
        break;
     }
     else
        left = des->Left = right;
  }
  /* Generate a timer comparator interrupt if the inserted event is the first one. */
  if (genInterrupt)
     while (device->EventQueue->Next == des->Node) {
         OSUINT16_LL(device->EventGeneration);
         if (des->GeneratedInterrupt || OSUINT16_SC(device->EventGeneration,2))
            break;
     }
  des->GeneratedInterrupt = TRUE;
} /* end of InsertQueueHelper */


/* OSUnScheduleTimerEvent: Entry point to remove the first occurrence of a specified
** event from the list associated with a timer. */
BOOL OSUnScheduleTimerEvent(void *event, UINT8 interruptIndex)
{
  DELETEQUEUE_OP des, *pendingOp;
  TIMER_ISR_DATA *device;
  des.EventOp = DeleteEventOp;  // Prepare the removal so that other task may complete
  des.Done = FALSE;             // it.
  device = (TIMER_ISR_DATA *)OSGetISRDescriptor(interruptIndex);
  des.Left = (TIMER_EVENT_NODE *)device->EventQueue;
  des.Event = event;
  des.Node = (TIMER_EVENT_NODE *)&des;
  pendingOp = device->PendingQueueOperation;
  if (pendingOp != NULL) {      // Is there an incomplete operation under way?
     if (pendingOp->EventOp == InsertEventOp)
        InsertQueueHelper((INSERTQUEUE_OP *)pendingOp,device,TRUE);
     else
        DeleteQueueHelper(pendingOp);
  }
  device->PendingQueueOperation = &des;    // Post the insertion for other tasks
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
     else if (right == NULL) {  // Is the seeked node already removed from the queue?
        des->Done = TRUE;       // Mark that node has been inserted
        break;
     }
     else if (des->Event == right->Event) {
        while (_OSUINTPTR_LL((UINTPTR *)&des->Node) == (UINTPTR)des && !des->Done)
           if (_OSUINTPTR_SC((UINTPTR *)&des->Node,(UINTPTR)right))
              break;
        while (_OSUINTPTR_LL((UINTPTR *)&left->Next) == (UINTPTR)right)
           if (des->Done || _OSUINTPTR_SC((UINTPTR *)&left->Next,(UINTPTR)right->Next))
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
TIMER_EVENT_NODE *GetFreeNode(TIMER_ISR_DATA *des)
{
  TIMER_EVENT_NODE *node;
  while ((node = (TIMER_EVENT_NODE *)_OSUINTPTR_LL((UINTPTR *)&des->FreeNodes)) != NULL)
     if (_OSUINTPTR_SC((UINTPTR *)&des->FreeNodes,(UINTPTR)node->Next))
        break;
  return node;
} /* end of GetFreeNode */


/* ReleaseNode: Returns a node to the pool of free nodes. */
void ReleaseNode(TIMER_ISR_DATA *device, TIMER_EVENT_NODE *freeNode)
{
  while (TRUE) {
     freeNode->Next = (TIMER_EVENT_NODE *)_OSUINTPTR_LL((UINTPTR *)&device->FreeNodes);
     if (_OSUINTPTR_SC((UINTPTR *)&device->FreeNodes,(UINTPTR)freeNode))
        return;
  }
} /* end of ReleaseNode */


/* TimerIntHandler: ISR routine called whenever the timer device gets an interrupt. */
void TimerIntHandler(TIMER_ISR_DATA *device)
{
  UINT16 time16;
  TIMER_EVENT_NODE *eventNode;
  INSERTQUEUE_OP *pendingOp;
  *device->Comparator = 0;       // Disable timer comparator */
  /* First finalize any pending operation in case there is an inserted node that already
  ** has its occurrence time but is not yet in the queue. */
  pendingOp = (INSERTQUEUE_OP *)device->PendingQueueOperation;
  if (pendingOp != NULL) {       // Is there an incomplete operation under way?
     if (pendingOp->EventOp == InsertEventOp)
        InsertQueueHelper(pendingOp,device,FALSE);
     else
        DeleteQueueHelper((DELETEQUEUE_OP *)pendingOp);
  }
  device->PendingQueueOperation = NULL;
  /* Test interrupt source */
  if (*device->Status & 2)       // Is comparator interrupt pending?
     *device->Status &= ~2;      // Clear interrupt flag
  if (*device->Status & 1) {     // Is timer overflow interrupt?
     *device->Status &= ~1;      // Clear interrupt flag
     device->Time += 0x10000;    // Increment most significant word of Time
     if (device->Time >= SHIFT_TIME_LIMIT) {   // Shift all time value
        eventNode = device->EventQueue->Next;
        while (eventNode != NULL) {
           eventNode->Time -= SHIFT_TIME_LIMIT;
           eventNode = eventNode->Next;
        }
        device->Time -= SHIFT_TIME_LIMIT;
     }
  }
  // Schedule events that now occur
  eventNode = device->EventQueue->Next;
  while (eventNode != NULL && eventNode->Time <= (device->Time | *device->Counter)) {
     OSScheduleSuspendedTask(eventNode->Event);
     device->EventQueue->Next = eventNode->Next;
     ReleaseNode(device,eventNode);
     eventNode = device->EventQueue->Next;
  }
  // Program timer comparator
  if (eventNode != NULL && (eventNode->Time & 0xFFFF0000) == device->Time) {
     time16 = (UINT16)eventNode->Time;
     if (time16 - TIMER_OFFSET < *device->Counter)
        *device->EventGeneration = 2;
     else
        *device->Comparator = time16;
  }
} /* end of TimerIntHandler */
