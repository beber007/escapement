/* Copyright (c) 2006 MIS Institute of the HEIG affiliated to the University of Applied
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
/* File TestKernel.c: 
** Version identifier: April 2010
** Authors: MIS-TIC */

/*       MSP430
**  -----------------
** |            XT1IN|-
** |                 | 32kHz
** |           XT1OUT|-
** |                 |
** |            XT2IN|-
** |                 | 4MHz
** |           XT2OUT|-
** |                 |    _____________
** |             P1.0|---|
** |             P1.1|---| Oscilloscope
** |             P1.2|---|
** |                 |   |
** |                 |
*/

/* Claude: Description de l'application
** 
**
*/

#include "ZottaOS/ZottaOS.h"

typedef struct TaskParametersDef {
   UINT16 GPIO_Pin;
   UINT32 Delay;
} TaskParametersDef;

static void TaskVariableDelay(void *argument);
static void TaskFixDelay(void *argument);

#define TogleBit(GPIO_Pin) P1OUT ^= GPIO_Pin; 

//typedef struct InterruptDescriptorDef {
//  void (*InterruptHandler)(struct InterruptDescriptorDef *);
//} InterruptDescriptorDef;

//void P20InterruptHandler(InterruptDescriptorDef *desc)
//{
//  volatile UINT32 i;
//  TogleBit(0x2);
//  OSSetProcessorSpeed(0);
//  for(i = 0; i < 2000; i += 3);
//  TogleBit(0x2);
//  P2IE |= 0x1;
//}

int main(void)
{
  TaskParametersDef *TaskParameters;
	 
  WDTCTL = WDTPW + WDTHOLD;  // Disable watchdog timer

  // Init ports 
  P1DIR = 0x07;   // Set to output
  P1OUT = 0x00;   // Set initially low
  
  P2DIR = 0x00;   // Set to input
  
  P11DIR |= 0x07;
  P11SEL |= 0x07;
  
  /* Set the system clock characteristics */
  OSInitializeSystemClocks();
  
  TaskParameters = OSMalloc(sizeof(TaskParametersDef));
  TaskParameters->GPIO_Pin = 0x1;
  TaskParameters->Delay = 1000;
  OSCreateTask(TaskFixDelay,500,0,10000,10000,TaskParameters);

  TaskParameters = OSMalloc(sizeof(TaskParametersDef));
  TaskParameters->GPIO_Pin = 0x2;
  TaskParameters->Delay = 2000;
  OSCreateTask(TaskFixDelay,1000,0,20000,10000,TaskParameters);

  TaskParameters = OSMalloc(sizeof(TaskParametersDef));
  TaskParameters->GPIO_Pin = 0x4;
  TaskParameters->Delay = 13000;
  OSCreateTask(TaskVariableDelay,15000,0,20000,17000,TaskParameters);

  //OSSetISRDescriptor(OS_IO_PORT2_0,(InterruptDescriptorDef *)OSMalloc(sizeof(InterruptDescriptorDef)));
  //((InterruptDescriptorDef *)OSGetISRDescriptor(OS_IO_PORT2_0))->InterruptHandler = P20InterruptHandler;
  //P2IE |= 0x1;
  
  /* Start the OS so that it starts scheduling the user task */
  return OSStartMultitasking();
} /* end of main */


/* TaskVariableDelay: . */
void TaskVariableDelay(void *argument)
{
  static UINT32 k = 0;
  UINT32 i;
  TaskParametersDef *TaskParameters = (TaskParametersDef *)argument;
  TogleBit(TaskParameters->GPIO_Pin);
  if (k >= TaskParameters->Delay)
     k = 0;
  else
     k += 5;
  for (i = 0; i < k; i++);
  TogleBit(TaskParameters->GPIO_Pin);
  OSEndTask();
} /* end of TaskVariableDelay */


/* TaskFixDelay: . */
void TaskFixDelay(void *argument)
{
  volatile UINT32 i;
  TaskParametersDef *TaskParameters = (TaskParametersDef *)argument;
  TogleBit(TaskParameters->GPIO_Pin);
  for(i = 0; i < TaskParameters->Delay; i += 3);
  TogleBit(TaskParameters->GPIO_Pin);
  OSEndTask();
} /* end of TaskFixDelay */
