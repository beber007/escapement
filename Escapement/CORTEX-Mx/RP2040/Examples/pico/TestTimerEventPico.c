/* Copyright (c) 2026 Bertrand Hurst. All rights reserved.
** Escapement - Lightweight Power-Aware Real-Time OS.
** Derived from prior work; see LICENSE and NOTICE at the root of this repository.
*/
/* File TestTimerEventPico.c: Shows how to use Escapement_TimerEvent. Transposition of
** TestTimerEventF4.c: two periodic tasks each raise an output and schedule an event on
** alarm 2 of the timer, which wakes an event-driven task that lowers it.
**
** GPIO 2 goes high every 5 ms for 1 ms, GPIO 3 every 10 ms for 2 ms. The period of each
** output is the kernel's timer at work, its high time the event manager's, and both show
** that each event-driven task ran when it was woken.
** Platform version: RP2040.
*/

#include "Escapement.h"
#include "Escapement_TimerEvent.h"

#define FLAG1_PIN 2
#define FLAG2_PIN 3

#define EVENT_TIMER_INDEX OS_IO_TIMER_2

static void InitializeFlag(UINT8 pin);
static void SetLed1Task(void *argument);
static void ClearLed1Task(void *argument);
static void SetLed2Task(void *argument);
static void ClearLed2Task(void *argument);

#define RESETS_RESET      *((volatile UINT32 *)0x4000C000)
#define RESETS_RESET_DONE *((volatile UINT32 *)0x4000C008)
#define RESETS_IO_BANK0   (1u << 5)
#define RESETS_PADS_BANK0 (1u << 8)

#define IO_BANK0_CTRL(p)  *((volatile UINT32 *)(0x40014000 + 0x04 + 8 * (p)))
#define PADS_BANK0_GPIO(p) *((volatile UINT32 *)(0x4001C000 + 0x04 + 4 * (p)))
#define PADS_OD_BIT       (1u << 7)
#define PADS_IE_BIT       (1u << 6)
#define FUNCSEL_SIO       5

#define SIO_GPIO_OUT_SET  *((volatile UINT32 *)(0xD0000000 + 0x14))
#define SIO_GPIO_OUT_CLR  *((volatile UINT32 *)(0xD0000000 + 0x18))
#define SIO_GPIO_OE_SET   *((volatile UINT32 *)(0xD0000000 + 0x24))

#define VTOR              *((volatile UINT32 *)0xE000ED08)


int main(void)
{
  void *event;
  extern void (* const CortexMxVectorTable[])(void);
  VTOR = (UINT32)CortexMxVectorTable;
  OSInitializeSystemClocks();
  #if defined(ESCAPEMENT_VERSION_HARD_PA)
     OSInitProcessorSpeed();
  #endif
  RESETS_RESET &= ~(RESETS_IO_BANK0 | RESETS_PADS_BANK0);
  while ((RESETS_RESET_DONE & (RESETS_IO_BANK0 | RESETS_PADS_BANK0)) !=
         (RESETS_IO_BANK0 | RESETS_PADS_BANK0));
  InitializeFlag(FLAG1_PIN);
  InitializeFlag(FLAG2_PIN);
  /* Two pending events at most, the alarm at the priority of the application. */
  OSInitTimerEvent(2,1,EVENT_TIMER_INDEX);
  /* Each periodic task passes the event it schedules as its argument. */
  #if defined(ESCAPEMENT_VERSION_SOFT)
     event = OSCreateEventDescriptor();
     OSCreateSynchronousTask(ClearLed1Task,0,1000,0,event,NULL);
     OSCreateTask(SetLed1Task,0,0,5000,5000,1,1,0,event);
     event = OSCreateEventDescriptor();
     OSCreateSynchronousTask(ClearLed2Task,0,2000,0,event,NULL);
     OSCreateTask(SetLed2Task,0,0,10000,10000,1,1,0,event);
  #elif defined(ESCAPEMENT_VERSION_HARD_PA)
     /* Execution times at 125 MHz, generous. The workload of an event-driven task is its
     ** execution time divided by the share of the processor left to it, here its delay;
     ** the utilization of both, 20/1000 + 20/2000, is 8 in 256ths. */
     event = OSCreateEventDescriptor();
     OSCreateSynchronousTask(ClearLed1Task,20,1000,8,event,NULL);
     OSCreateTask(SetLed1Task,20,0,5000,5000,event);
     event = OSCreateEventDescriptor();
     OSCreateSynchronousTask(ClearLed2Task,20,2000,8,event,NULL);
     OSCreateTask(SetLed2Task,20,0,10000,10000,event);
  #else
     event = OSCreateEventDescriptor();
     OSCreateSynchronousTask(ClearLed1Task,1000,event,NULL);
     OSCreateTask(SetLed1Task,0,5000,5000,event);
     event = OSCreateEventDescriptor();
     OSCreateSynchronousTask(ClearLed2Task,2000,event,NULL);
     OSCreateTask(SetLed2Task,0,10000,10000,event);
  #endif
  return OSStartMultitasking(NULL,NULL);
} /* end of main */


/* InitializeFlag: Drives a pin from the SIO block as a plain output. */
static void InitializeFlag(UINT8 pin)
{
  PADS_BANK0_GPIO(pin) = (PADS_BANK0_GPIO(pin) & ~PADS_OD_BIT) | PADS_IE_BIT;
  IO_BANK0_CTRL(pin) = FUNCSEL_SIO;
  SIO_GPIO_OE_SET = 1u << pin;
  SIO_GPIO_OUT_CLR = 1u << pin;
} /* end of InitializeFlag */


/* SetLed1Task: Raises GPIO 2 and has it lowered 1000 us later. */
static void SetLed1Task(void *argument)
{
  SIO_GPIO_OUT_SET = 1u << FLAG1_PIN;
  OSTrace(OS_TRACE_MARK,FLAG1_PIN,0);
  OSScheduleTimerEvent(argument,1000,EVENT_TIMER_INDEX);
  OSEndTask();
} /* end of SetLed1Task */


/* ClearLed1Task: Lowers the output SetLed1Task raised. */
static void ClearLed1Task(void *argument)
{
  SIO_GPIO_OUT_CLR = 1u << FLAG1_PIN;
  OSTrace(OS_TRACE_MARK,FLAG1_PIN,1);
  OSSuspendSynchronousTask();
} /* end of ClearLed1Task */


/* SetLed2Task: Raises GPIO 3 and has it lowered 2000 us later. */
static void SetLed2Task(void *argument)
{
  SIO_GPIO_OUT_SET = 1u << FLAG2_PIN;
  OSTrace(OS_TRACE_MARK,FLAG2_PIN,0);
  OSScheduleTimerEvent(argument,2000,EVENT_TIMER_INDEX);
  OSEndTask();
} /* end of SetLed2Task */


/* ClearLed2Task: Lowers the output SetLed2Task raised. */
static void ClearLed2Task(void *argument)
{
  SIO_GPIO_OUT_CLR = 1u << FLAG2_PIN;
  OSTrace(OS_TRACE_MARK,FLAG2_PIN,1);
  OSSuspendSynchronousTask();
} /* end of ClearLed2Task */
