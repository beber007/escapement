/* Copyright (c) 2026 Bertrand Hurst. All rights reserved.
** Escapement - Lightweight Power-Aware Real-Time OS.
** Derived from prior work; see LICENSE and NOTICE at the root of this repository.
*/
/* File TaskLEDPico.c: Three periodic tasks that each toggle an output while they run.
** Transposition of TaskLEDF4.c to the Raspberry Pi Pico. The first task drives GPIO 25,
** the on-board LED, so the board shows it is alive without any instrument.
** Platform version: RP2040.
*/

#include "Escapement.h"

#define FLAG1_PIN 25   /* on-board LED */
#define FLAG2_PIN  2
#define FLAG3_PIN  3
/* Measurement probe, driven by a task of 1 ms period. The overload guard this task used
** to trip came from the core frequency, which had stayed on the crystal: now that
** OSInitializeSystemClocks engages the PLL, a scheduling round costs 7 us on average and
** the probe runs. See docs/rp2040.md. */
#define PROBE_PIN  4

/* Parameters handed to each task instance */
typedef struct TaskParametersDef {
   UINT8 Pin;       /* output driven by the task */
   UINT32 Delay;    /* number of loop iterations before releasing the output */
} TaskParametersDef;

static void InitializeFlag(UINT8 pin);
static void FixedDelayTask(void *argument);
static void VariableDelayTask(void *argument);
static void ProbeTask(void *argument);

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
  TaskParametersDef *TaskParameters;
  /* The firmware runs from SRAM: point the processor at the vector table placed there by
  ** the linker before any interrupt can be taken. */
  extern void (* const CortexMxVectorTable[])(void);
  VTOR = (UINT32)CortexMxVectorTable;
  /* Leave the ring oscillator, imprecise and around 6 MHz, for the 12 MHz crystal: the
  ** microsecond tick of the timer, and therefore the task periods, depend on it. */
  OSInitializeSystemClocks();
  /* Release the two GPIO blocks from reset. */
  RESETS_RESET &= ~(RESETS_IO_BANK0 | RESETS_PADS_BANK0);
  while ((RESETS_RESET_DONE & (RESETS_IO_BANK0 | RESETS_PADS_BANK0)) !=
         (RESETS_IO_BANK0 | RESETS_PADS_BANK0));
  InitializeFlag(FLAG1_PIN);
  InitializeFlag(FLAG2_PIN);
  InitializeFlag(FLAG3_PIN);
  InitializeFlag(PROBE_PIN);
  /* Create the 3 tasks. Periods are expressed in microseconds, the resolution of the
  ** RP2040 timer: 10, 20 and 60 ms, plus the 1 ms probe below.
  ** The delays are counts of volatile loop iterations. How long one takes depends on the
  ** core frequency — 125 MHz once OSInitializeSystemClocks has engaged the PLL — and on
  ** the optimisation level the example is built with, so no load figure is quoted here;
  ** it is low enough that the kernel's overload guard never fires. The periods themselves
  ** were checked against an external frequency counter, see the README. */
  TaskParameters = (TaskParametersDef *)OSMalloc(sizeof(TaskParametersDef));
  TaskParameters->Pin = FLAG1_PIN;
  TaskParameters->Delay = 500;
  #if defined(ESCAPEMENT_VERSION_SOFT)
     /* Under the soft kernel the first two tasks are (1,3)-firm, as in TaskLEDF4: one
     ** instance in three is mandatory, the others run if the declared execution times,
     ** generous here, leave room for them. The probe stays (1,1), i.e. hard. */
     OSCreateTask(FixedDelayTask,100,0,10000,10000,1,3,0,TaskParameters);
  #else
     OSCreateTask(FixedDelayTask,0,10000,10000,TaskParameters);
  #endif
  TaskParameters = (TaskParametersDef *)OSMalloc(sizeof(TaskParametersDef));
  TaskParameters->Pin = FLAG2_PIN;
  TaskParameters->Delay = 1000;
  #if defined(ESCAPEMENT_VERSION_SOFT)
     OSCreateTask(FixedDelayTask,200,0,20000,20000,1,3,0,TaskParameters);
  #else
     OSCreateTask(FixedDelayTask,0,20000,20000,TaskParameters);
  #endif
  TaskParameters = (TaskParametersDef *)OSMalloc(sizeof(TaskParametersDef));
  TaskParameters->Pin = FLAG3_PIN;
  TaskParameters->Delay = 4000;
  #if defined(ESCAPEMENT_VERSION_SOFT)
     OSCreateTask(VariableDelayTask,1000,0,60000,60000,1,1,0,TaskParameters);
  #else
     OSCreateTask(VariableDelayTask,0,60000,60000,TaskParameters);
  #endif
  /* Measurement probe, 1 ms period, no payload. */
  #if defined(ESCAPEMENT_VERSION_SOFT)
     OSCreateTask(ProbeTask,20,0,1000,1000,1,1,0,NULL);
  #else
     OSCreateTask(ProbeTask,0,1000,1000,NULL);
  #endif
  /* Start the OS so that it starts scheduling the user tasks */
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


static void ProbeTask(void *argument)
{
  static UINT32 level = 0;
  level ^= 1;
  if (level) SIO_GPIO_OUT_SET = 1u << PROBE_PIN; else SIO_GPIO_OUT_CLR = 1u << PROBE_PIN;
  OSEndTask();
}


/* VariableDelayTask: Same, but the number of iterations grows on each instance until it
** reaches the limit, then restarts from 1. */
static void VariableDelayTask(void *argument)
{
  volatile static UINT32 k;
  volatile UINT32 i;
  TaskParametersDef *TaskParameters = (TaskParametersDef *)argument;
  if (k >= TaskParameters->Delay)
     k = 1;
  else
     k += 1;
  SIO_GPIO_OUT_SET = 1u << TaskParameters->Pin;
  for (i = 0; i < k; i += 1);
  SIO_GPIO_OUT_CLR = 1u << TaskParameters->Pin;
  OSEndTask();
} /* end of VariableDelayTask */
