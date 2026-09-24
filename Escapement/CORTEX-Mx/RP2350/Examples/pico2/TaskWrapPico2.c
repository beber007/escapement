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
/* File TaskWrapPico2.c: TaskWrapPico.c on the Raspberry Pi Pico 2, the same tasks and
** periods with the registers of the RP2350: periodic tasks whose only purpose is to cross
** the 2^30 boundary of the kernel clock while they run.
**
** The kernel counts time modulo 2^30 and shifts every temporal variable back when its
** counter wraps. The counter of the RP2350 never wraps in practice either, so the port
** rebuilds that boundary with ALARM1, 2^30 ticks after the kernel starts: 17 min 54 s at
** the 1 us tick of the chip. escapement_pico2.robot clocks the emulated timer 1000 times
** faster, so a tick is a nanosecond and the boundary comes after 1.07 s; the periods
** below are scaled by the same factor, leaving the kernel with the load it would have on
** the board. On a board this image runs a thousand times slower than intended.
**
** The outputs are those of TaskLEDPico: GPIO 25, 2 and 3 for three tasks of 100, 200 and
** 600 ms, and GPIO 4 for a probe toggled every 50 ms, whose period is timed after the
** boundary.
** Platform version: RP2350 (Raspberry Pi Pico 2).
*/

#include "Escapement.h"

#define FLAG1_PIN 25
#define FLAG2_PIN  2
#define FLAG3_PIN  3
#define PROBE_PIN  4

typedef struct TaskParametersDef {
   UINT8 Pin;
   UINT32 Delay;
} TaskParametersDef;

static void InitializeFlag(UINT8 pin);
static void FixedDelayTask(void *argument);
static void ProbeTask(void *argument);

/* Periods and execution times are in ticks of a nanosecond. Every task is hard: (1,1)-firm
** under the soft kernel. The execution times are far above what the tasks take at
** 125 MHz. */
#if defined(ESCAPEMENT_VERSION_SOFT)
   #define CreateTask(task,wcet,period,parameters) \
              OSCreateTask(task,wcet,0,period,period,1,1,0,parameters)
#else
   #define CreateTask(task,wcet,period,parameters) \
              OSCreateTask(task,0,period,period,parameters)
#endif

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
  TaskParametersDef *TaskParameters;
  extern void (* const CortexMxVectorTable[])(void);
  VTOR = (UINT32)CortexMxVectorTable;
  OSInitializeSystemClocks();
  RESETS_RESET &= ~(RESETS_IO_BANK0 | RESETS_PADS_BANK0);
  while ((RESETS_RESET_DONE & (RESETS_IO_BANK0 | RESETS_PADS_BANK0)) !=
         (RESETS_IO_BANK0 | RESETS_PADS_BANK0));
  InitializeFlag(FLAG1_PIN);
  InitializeFlag(FLAG2_PIN);
  InitializeFlag(FLAG3_PIN);
  InitializeFlag(PROBE_PIN);
  TaskParameters = (TaskParametersDef *)OSMalloc(sizeof(TaskParametersDef));
  TaskParameters->Pin = FLAG1_PIN;
  TaskParameters->Delay = 500;
  CreateTask(FixedDelayTask,100000,100000000,TaskParameters);
  TaskParameters = (TaskParametersDef *)OSMalloc(sizeof(TaskParametersDef));
  TaskParameters->Pin = FLAG2_PIN;
  TaskParameters->Delay = 1000;
  CreateTask(FixedDelayTask,200000,200000000,TaskParameters);
  TaskParameters = (TaskParametersDef *)OSMalloc(sizeof(TaskParametersDef));
  TaskParameters->Pin = FLAG3_PIN;
  TaskParameters->Delay = 2000;
  CreateTask(FixedDelayTask,400000,600000000,TaskParameters);
  CreateTask(ProbeTask,20000,50000000,NULL);
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


/* FixedDelayTask: Raises its output, burns a fixed number of iterations, lowers it. */
static void FixedDelayTask(void *argument)
{
  volatile UINT32 i;
  TaskParametersDef *TaskParameters = (TaskParametersDef *)argument;
  SIO_GPIO_OUT_SET = 1u << TaskParameters->Pin;
  for (i = 0; i < TaskParameters->Delay; i += 1);
  SIO_GPIO_OUT_CLR = 1u << TaskParameters->Pin;
  OSEndTask();
} /* end of FixedDelayTask */


/* ProbeTask: Toggles its output, so that the output's period is twice the task's. */
static void ProbeTask(void *argument)
{
  static UINT32 level = 0;
  level ^= 1;
  if (level) SIO_GPIO_OUT_SET = 1u << PROBE_PIN; else SIO_GPIO_OUT_CLR = 1u << PROBE_PIN;
  OSEndTask();
} /* end of ProbeTask */
