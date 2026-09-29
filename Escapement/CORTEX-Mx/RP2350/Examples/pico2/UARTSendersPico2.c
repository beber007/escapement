/* Copyright (c) 2026 Bertrand Hurst. All rights reserved.
** Escapement - Lightweight Power-Aware Real-Time OS.
** Distributed under the terms of LICENSE at the root of this repository.
*/
/* File UARTSendersPico2.c: Two tasks sending on UART0 at once, for escapement_pico2.robot.
**
** A task of 10 ms sends lines of 15 "a" for 6 ms of each period, one after the other as
** fast as buffers come free; a task of 500 us sends a line of 7 "b" at each instance,
** preempting the first. Every line on the port must come whole, each of its bytes once:
** an audit of the port found on 2026-09-29 that OSEnqueueUART primed the transmission at
** task level, with only the UART's own interrupt masked, so that a task preempting
** another inside it emptied the same buffer from where the other stood.
**
** Results, in words: 0 marker, 1 lines of "a" queued, 2 lines of "b" queued, 3 instances
** of the second task that found no buffer free.
** Platform version: RP2350 (Raspberry Pi Pico 2).
*/

#include "Escapement.h"
#include "Escapement_Timer.h"
#include "Escapement_UART.h"

#define MARKER      0x55534E44u          /* "USND" */
#define NB_NODE     4
#define NODE_SIZE   16
#define A_SENDING   6000                 /* us of each 10 ms */

volatile struct {
  UINT32 Marker, ALines, BLines, BNoNode;
} Results;

#define VTOR        *((volatile UINT32 *)0xE000ED08)

static void ATask(void *argument);
static void BTask(void *argument);


int main(void)
{
  extern void (* const CortexMxVectorTable[])(void);
  VTOR = (UINT32)CortexMxVectorTable;
  OSInitializeSystemClocks();
  Results.Marker = MARKER;
  if (!OSInitUART(NB_NODE,NODE_SIZE,NULL,OS_IO_UART0))
     while (TRUE);
  #if defined(ESCAPEMENT_VERSION_SOFT)
     OSCreateTask(ATask,7000,0,10000,10000,1,1,0,NULL);
     OSCreateTask(BTask,50,0,500,500,1,1,0,NULL);
  #else
     OSCreateTask(ATask,0,10000,10000,NULL);
     OSCreateTask(BTask,0,500,500,NULL);
  #endif
  return OSStartMultitasking(NULL,NULL);
} /* end of main */


/* Send: A line of count copies of c into a buffer, and the buffer queued; FALSE if none
** was free. */
static BOOL Send(UINT8 c, UINT8 count)
{
  UINT8 *node = (UINT8 *)OSGetFreeNodeUART(OS_IO_UART0), i;
  if (node == NULL)
     return FALSE;
  for (i = 0; i < count; i++)
     node[i] = c;
  node[count] = '\n';
  OSEnqueueUART(node,count + 1,OS_IO_UART0);
  return TRUE;
} /* end of Send */


static void ATask(void *argument)
{
  INT32 start = _OSGetActualTime();
  (void)argument;
  while ((((UINT32)_OSGetActualTime() - (UINT32)start) & 0x3FFFFFFF) < A_SENDING)
     if (Send('a',15))
        Results.ALines += 1;
  OSEndTask();
} /* end of ATask */


static void BTask(void *argument)
{
  (void)argument;
  if (Send('b',7))
     Results.BLines += 1;
  else
     Results.BNoNode += 1;
  OSEndTask();
} /* end of BTask */
