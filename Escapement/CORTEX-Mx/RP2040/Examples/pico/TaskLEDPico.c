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

/* Parameters handed to each task instance */
typedef struct TaskParametersDef {
   UINT8 Pin;       /* output driven by the task */
   UINT32 Delay;    /* number of loop iterations before releasing the output */
} TaskParametersDef;

static void InitializeFlag(UINT8 pin);
static void FixedDelayTask(void *argument);
static void VariableDelayTask(void *argument);

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
  /* Create the 3 tasks. Periods are expressed in microseconds, the resolution of the
  ** RP2040 timer.
  ** The delays are counts of volatile loop iterations, each costing about a dozen cycles
  ** on a Cortex-M0+, so roughly 1 us with the core at the 12 MHz of the crystal. The load
  ** stays around 17%, well clear of the kernel's overload guard.
  **     500 / 10000 -> 5%
  **    1000 / 20000 -> 5%
  **   4000 / 60000  -> 6.7% */
  TaskParameters = (TaskParametersDef *)OSMalloc(sizeof(TaskParametersDef));
  TaskParameters->Pin = FLAG1_PIN;
  TaskParameters->Delay = 500;
  OSCreateTask(FixedDelayTask,0,10000,10000,TaskParameters);
  TaskParameters = (TaskParametersDef *)OSMalloc(sizeof(TaskParametersDef));
  TaskParameters->Pin = FLAG2_PIN;
  TaskParameters->Delay = 1000;
  OSCreateTask(FixedDelayTask,0,20000,20000,TaskParameters);
  TaskParameters = (TaskParametersDef *)OSMalloc(sizeof(TaskParametersDef));
  TaskParameters->Pin = FLAG3_PIN;
  TaskParameters->Delay = 4000;
  OSCreateTask(VariableDelayTask,0,60000,60000,TaskParameters);
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
