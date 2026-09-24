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
/* File TestTimerEventPico2.c: Shows how to use Escapement_TimerEvent. Transposition of
** TestTimerEventF4.c: two periodic tasks each raise an output and schedule an event on
** alarm 2 of the timer, which wakes an event-driven task that lowers it.
**
** GPIO 2 goes high every 5 ms for 1 ms, GPIO 3 every 10 ms for 2 ms. The period of each
** output is the kernel's timer at work, its high time the event manager's, and both show
** that each event-driven task ran when it was woken.
** Platform version: RP2350 (Raspberry Pi Pico 2), transposed from TestTimerEventPico.c:
** the registers of the pins differ.
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

#define RESETS_RESET      *((volatile UINT32 *)0x40020000)
#define RESETS_RESET_DONE *((volatile UINT32 *)0x40020008)
#define RESETS_IO_BANK0   (1u << 6)
#define RESETS_PADS_BANK0 (1u << 9)

#define IO_BANK0_CTRL(p)  *((volatile UINT32 *)(0x40028000 + 0x04 + 8 * (p)))
#define PADS_BANK0_GPIO(p) *((volatile UINT32 *)(0x40038000 + 0x04 + 4 * (p)))
#define PADS_ISO_BIT      (1u << 8)   /* new on the RP2350: pad isolated from its signal */
#define PADS_OD_BIT       (1u << 7)
#define PADS_IE_BIT       (1u << 6)
#define FUNCSEL_SIO       5

#define SIO_GPIO_OUT_SET  *((volatile UINT32 *)(0xD0000000 + 0x18))
#define SIO_GPIO_OUT_CLR  *((volatile UINT32 *)(0xD0000000 + 0x20))
#define SIO_GPIO_OE_SET   *((volatile UINT32 *)(0xD0000000 + 0x38))

#define VTOR              *((volatile UINT32 *)0xE000ED08)


int main(void)
{
  void *event;
  extern void (* const CortexMxVectorTable[])(void);
  VTOR = (UINT32)CortexMxVectorTable;
  OSInitializeSystemClocks();
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
  /* The pads of the RP2350 come out of reset isolated from their signal; the isolation
  ** goes once the SIO drives the pin, in the order of gpio_set_function in the pico-sdk. */
  PADS_BANK0_GPIO(pin) &= ~PADS_ISO_BIT;
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
