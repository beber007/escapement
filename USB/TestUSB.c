/* Copyright (c) 2026 Bertrand Hurst. All rights reserved.
** Escapement - Lightweight Power-Aware Real-Time OS.
** Derived from prior work; see LICENSE and NOTICE at the root of this repository.
*/
/* File TestUSB.c: .
** Version identifier: April 2010
*/

#include "Escapement/Escapement_msp430.h"
#include "Escapement/USB/Usb.h"
#include "Escapement/USB/descriptors.h"
  
#ifdef _CDC_
   #define USB_INPUT OS_IO_USB_INPUT_ENDPOINT3
   #define USB_OUTPUT OS_IO_USB_OUTPUT_ENDPOINT3
   #define NB_NODE_OUTPUT_FIFO   5
   #define SIZE_NODE_OUTPUT_FIFO 64
   #define NB_NODE_INPUT_FIFO    5
   #define SIZE_NODE_INPUT_FIFO  64
#endif

#ifdef _HID_
   #define USB_INPUT OS_IO_USB_INPUT_ENDPOINT1
   #define USB_OUTPUT OS_IO_USB_OUTPUT_ENDPOINT1
   #define NB_NODE_OUTPUT_FIFO   0 // not used
   #define SIZE_NODE_OUTPUT_FIFO 0 // not used
   #define NB_NODE_INPUT_FIFO    2
   #define SIZE_NODE_INPUT_FIFO  64
#endif

typedef struct TaskParametersDef {
   UINT16 GPIO_Pin;
} TaskParametersDef;

static void TaskSimpleDelay(void *argument);
static void InitializeSystemClock(void);
static void USBOutputUserHandler(UINT8 *node, UINT8 size);

#define TogleBit(GPIO_Pin) P1OUT ^= GPIO_Pin; 


int main(void)
{
  TaskParametersDef *TaskParameters;
	 
  WDTCTL = WDTPW + WDTHOLD;  // Disable watchdog timer

  /* Initialisation GPIO ports */ 
  P1SEL = 0x00;   // Set to GPIO
  P1DIR = 0xF7;   // Set to output
  P1OUT = 0x00;   // Set initially low
  
  /* Set the system clock characteristics */
  InitializeSystemClock();

  /* Initailise USB */
  OSInitOutputUSB(USBOutputUserHandler,NB_NODE_OUTPUT_FIFO,
                  SIZE_NODE_OUTPUT_FIFO,USB_OUTPUT);
  OSInitInputUSB(NB_NODE_INPUT_FIFO,SIZE_NODE_INPUT_FIFO,USB_INPUT);
  OSInitUSB();

  /* Initailise task */
  TaskParameters = OSAlloca(sizeof(TaskParametersDef));
  TaskParameters->GPIO_Pin = 0x1;
  OSCreateTask(TaskSimpleDelay,0,1000,1000,TaskParameters);

  /* Start the OS so that it starts scheduling the user task */
  return OSStartMultitasking();
} /* end of main */


/* USB output user handler. */
void USBOutputUserHandler(UINT8 *node, UINT8 size)
{
  #ifdef _CDC_
     OSEnqueueOutputNodeUSB(node,size,USB_OUTPUT);
  #endif
  #ifdef _HID_
     UINT8 *buf, i;
     if ((buf = (UINT8 *)OSGetFreeNodeInputNodeUSB(USB_INPUT)) != NULL) {
        for (i = 0;i < size; i++)
           buf[i] = node[i];
        OSEnqueueInputUSB(buf,size,USB_INPUT);
     }
  #endif
} /* end of USBOutputHandler */


/* System clock initialisation */
void InitializeSystemClock(void)
{
  // ACLK = LFXT1 = 32768Hz, MCLK = SMCLK = default DCO = 32 x ACLK = 1048576Hz
  P5DIR &= ~0x10;                  // Set output direction for XT1 Pins
  P5SEL |= 0x10;                   // Analog function for XT1 Pins
  UCSCTL6 |= XCAP_3;               // Internal load cap
  UCSCTL6 &= ~XT1OFF;              // XT1 On
  while (SFRIFG1 & OFIFG) {        // check OFIFG fault flag
     UCSCTL7 &= ~(DCOFFG+XT1LFOFFG+XT2OFFG); // Clear OSC flaut Flags fault flags
     SFRIFG1 &= ~OFIFG;            // Clear OFIFG fault flag
  }
  // Initialize DCO to 2.45MHz
  __bis_SR_register(SCG0);         // Disable the FLL control loop
  UCSCTL0 = 0x0000;                // Set lowest possible DCOx, MODx
  UCSCTL1 = DCORSEL_6;             // Set RSELx for DCO = 16.98 MHz
  UCSCTL2 = FLLD_1 + 243;          // Set DCO Multiplier for 7.99MHz
                                   // (N + 1) * FLLRef = Fdco
                                   // (243 + 1) * 32768 = 7.99MHz
                                   // Set FLL Div = fDCOCLK/2
  __bic_SR_register(SCG0);         // Enable the FLL control loop
  // Worst-case settling time for the DCO when the DCO range bits have been
  // changed is n x 32 x 32 x f_MCLK / f_FLL_reference. See UCS chapter in 5xx
  // UG for optimization.
  // 32 x 32 x 7.99 MHz / 32,768 Hz = 249688 = MCLK cycles for DCO to settle
  __delay_cycles(249688);
  // Wait till FLL+ is settled at the correct DCO tap
  while (SFRIFG1 & OFIFG) {   // check OFIFG fault flag
     UCSCTL7 &= ~(DCOFFG+XT1LFOFFG+XT2OFFG); // Clear OSC flaut Flags fault flags
     SFRIFG1 &= ~OFIFG;        // Clear OFIFG fault flag
  }
} /* InitializeSystemClock */


/* TaskSimpleDelay: . */
void TaskSimpleDelay(void *argument)
{
  #ifdef _CDC_
     UINT8 *nodeInput, *nodeOutput;
     UINT16 size, i;
  #endif
  TaskParametersDef *TaskParameters = (TaskParametersDef *)argument;
  TogleBit(TaskParameters->GPIO_Pin);
  #ifdef _CDC_
     // Recuperer un noeu pour l'emission
     if ((nodeInput = (UINT8 *)OSGetFreeNodeInputNodeUSB(USB_INPUT)) != NULL) {
        // Recuperer un noeu de donnee recue
        if ((nodeOutput = (UINT8 *)OSDequeueOutputNodeUSB(&size,USB_OUTPUT)) != NULL) {
           for (i = 0;i < size; i++)
              nodeInput[i] = nodeOutput[i];
           OSEnqueueInputUSB(nodeInput,size,USB_INPUT);
           OSReleaseOutputNodeUSB(nodeOutput,USB_OUTPUT);
        }
        else
           // rendre le noeu pour l'emission non utilise
           OSReleaseInputNodeUSB(nodeInput,USB_INPUT);
     }
  #endif
  TogleBit(TaskParameters->GPIO_Pin);
  OSEndTask();
} /* end of TaskSimpleDelay */
