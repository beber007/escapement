/* Copyright (c) 2026 Bertrand Hurst. All rights reserved.
** Escapement - Lightweight Power-Aware Real-Time OS.
** Derived from prior work; see LICENSE and NOTICE at the root of this repository.
*/
/* File TaskAperio.c: This application demonstrates the use of interrupts and event-driven
** tasks in which the event-driven tasks are scheduled whenever the user presses a button.
** There are 2 buttons in this application. Each button raises an interrupt which triggers
** a specific event-driven task. The event-driven task retriggers itself if its associated
** button is still pressed at the end of its execution.
** This application requires the configuration of interrupt of I/O port #4 and the setting
** of the system clocks.
** To build this application, run "Escapement Configurator tool" and load the configuration
** file EXP430FR5739.zot to generate the assembly and header files, and to get assembler
** function OSInitializeSystemClocks that configures the clocks as shown below:
**    - MCLK sourced from the DCO (DCOCLK) divided by 1;
**    - ACLK sourced from the DCO (DCOCLK) divided by 8;
**    - SMCLK sourced from the DCO (DCOCLK) divided by 8;
**    - DCO set for factory calibrated value of 8.00 MHz(DCORSEL = 0, DCOFSEL = 3).
** You can then attach an oscilloscope to pins P1.0 through P1.2 to see the changes of
** the GPIO output states.
**              MSP430
**         -----------------
**        |             P4.0|--- <-- Button
**        |             P4.1|--- <-- Button
**        |                 |    _____________
**        |             P1.0|---| Oscilloscope
**        |             P1.1|---|
**        |                 |   |
** Although trivial, this simple application uses a single code template to instantiate
** 2 different tasks, each having its particular set of parameters.
** Tested on "MSP-EXP430FR5739 - Experimenter Board".
** Version identifier: June 2012

#include "Escapement.h"


// Button interrupt descriptor
typedef struct ButtonDescriptorDef {
   void (*ButtonInterruptHandler)(struct ButtonDescriptorDef *);
   void *event;        // To schedule the associated event-driven task from the ISR
} ButtonDescriptorDef;

// Event-driven task configuration passed as parameter
typedef struct TaskParametersDef {
   UINT16 Flag_Pin;    // Output pin for the task
   UINT16 Button_Pin;  // Input pin for the task
   UINT32 Delay;       // Active delay iterations done by the task (debugging purposes)
   void *event;        // Needed to reschedule itself
} TaskParametersDef;

// Internal function prototypes
static void ButtonTask(void *argument);
static void ButtonISR(ButtonDescriptorDef *buttonDescriptor);

#define ToggleBit(Flag_Pin) (P1OUT ^= Flag_Pin);

#define EnableInt(Button_Pin) (P4IE |= Button_Pin)
#define ClearInt(Button_Pin) (P4IFG &= ~Button_Pin)
#define IsPressed(Button_Pin) ((P4IN & Button_Pin) != Button_Pin)

int main(void)
{
  ButtonDescriptorDef *buttonDescriptor;
  TaskParametersDef *taskParameters;
  void *event;

  WDTCTL = WDTPW + WDTHOLD;  // Disable watchdog timer

  /* Set the system clock characteristics */
  OSInitializeSystemClocks();

  // Initialize output I/O ports
  P1OUT = 0x00;   // Initially start at low
  P1DIR = 0x03;   // Set to output
  P4OUT |= 0x3;
  P4REN |= 0x3;
  P4IES |= 0x3;
  P4IFG &= ~0x3;
  P4IE |= 0x3;

  #if defined(ESCAPEMENT_VERSION_HARD)
     event = OSCreateEventDescriptor();
     // Register button P4.0 interrupt
     buttonDescriptor = (ButtonDescriptorDef *)OSMalloc(sizeof(ButtonDescriptorDef));
     buttonDescriptor->ButtonInterruptHandler = ButtonISR;
     buttonDescriptor->event = event;
     OSSetISRDescriptor(OS_IO_PORT4_0,buttonDescriptor);
     // Create the first event-driven task
     taskParameters = (TaskParametersDef *)OSMalloc(sizeof(TaskParametersDef));
     taskParameters->Delay = 1000;      // Impulse width to visualize the task execution
     taskParameters->Flag_Pin = BIT0;
     taskParameters->Button_Pin = BIT0;
     taskParameters->event = event;     // Needed to reschedule itself
     OSCreateSynchronousTask(ButtonTask,10000,event,taskParameters);
     // Register button P4.1 interrupt
     event = OSCreateEventDescriptor();
     buttonDescriptor = (ButtonDescriptorDef *)OSMalloc(sizeof(ButtonDescriptorDef));
     buttonDescriptor->ButtonInterruptHandler = ButtonISR;
     buttonDescriptor->event = event;
     OSSetISRDescriptor(OS_IO_PORT4_1,buttonDescriptor);
     // Create the second event-driven task that
     taskParameters = (TaskParametersDef *)OSMalloc(sizeof(TaskParametersDef));
     taskParameters->Delay = 2000;
     taskParameters->Flag_Pin = BIT1;
     taskParameters->Button_Pin = BIT1;
     taskParameters->event = event;
     OSCreateSynchronousTask(ButtonTask,20000,event,taskParameters);
  #elif defined(ESCAPEMENT_VERSION_SOFT)
     #error Untested
  #else
     #error Wrong kernel version
  #endif  

  /* Start the OS so that it starts scheduling the user tasks */
  return OSStartMultitasking(NULL,NULL);
} /* end of main */


/* ButtonISR: Interrupt handler of a button. */
void ButtonISR(ButtonDescriptorDef *buttonDescriptor)
{ // Schedule task specified in the descriptor
  OSScheduleSuspendedTask(buttonDescriptor->event);
} /* end of ButtonISR */


/* ButtonTask: Checks if the button is still pressed and if not re-enables the button's
** ISR. (Also toggles a GPIO pin for debugging purposes.) */
void ButtonTask(void *argument)
{
  UINT32 i;
  TaskParametersDef *TaskParameters = (TaskParametersDef *)argument;
  ToggleBit(TaskParameters->Flag_Pin);
  for (i = 0; i < TaskParameters->Delay; i += 1);
  ToggleBit(TaskParameters->Flag_Pin);
  if (IsPressed(TaskParameters->Button_Pin))         // Is button still pressed?
     OSScheduleSuspendedTask(TaskParameters->event); // Yes, reschedule the task
  else {
     // No, clear interrupt flag to prevent button rebounds
     ClearInt(TaskParameters->Button_Pin);
     EnableInt(TaskParameters->Button_Pin);
  }
  OSSuspendSynchronousTask();
} /* end of ButtonTask */
