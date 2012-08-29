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
*/
/* File Balls.c: This program displays three balls movement on a terminal screen according
** to the orientation of the board sensed by the accelerometer.
** This program samples the following information with the ADC10 converter:
**    - The X and Y axis of the accelerometer (sampling period 100 ms);
**    - Internal temperature sensor (sampling period 1 s).
** All values are transmitted via the UART after sampling. UART baud rate is 9600 baud.
** ANSI escape sequences are sent via the UART to the display the position of the balls.
** All numerical values read from the ADC are also displayed on the terminal.
** Tested on "MSP-EXP430FR5739 - Experimenter Board" with the following configuration of
** the clocks:
**    - MCLK sourced from the DCO (DCOCLK) divided by 1;
**    - ACLK sourced from the DCO (DCOCLK) divided by 8;
**    - SMCLK sourced from the DCO (DCOCLK) divided by 8;
**    - DCO set for factory calibrated value of 8.00 MHz(DCORSEL = 0, DCOFSEL = 3).
** Version identifier: June 2012
** Authors: MIS-TIC */
#include "ZottaOS.h"
#include "ZottaOS_UART.h"
#include <string.h>

// Pin definitions
#define ACC_PWR_PIN      BIT7
#define ACC_PWR_PORT_DIR P2DIR
#define ACC_PWR_PORT_OUT P2OUT
#define ACC_PORT_DIR     P3DIR
#define ACC_PORT_OUT     P3OUT
#define ACC_PORT_SEL0    P3SEL0
#define ACC_PORT_SEL1    P3SEL1
#define ACC_X_PIN        BIT0
#define ACC_Y_PIN        BIT1

// Accelerometer input channel definitions
#define ACC_TEMP_CHANNEL (ADC10INCH_10 + ADC10SREF_1)
#define ACC_X_CHANNEL    (ADC10INCH_12 + ADC10SREF_0)
#define ACC_Y_CHANNEL    (ADC10INCH_13 + ADC10SREF_0)

// Ball structure
typedef struct BallDef {
   INT16 X, Y; // Current position
   UINT8 Coef; // Friction coefficient
   char *Char; // The character representing the ball
} BallDef;


// Internal function prototypes
static void InitializeUARTHardware(void);
static UINT16 GetADC10Sample(UINT16 channel);
static char *CopyIntToString(char *s, INT16 value, UINT8 padding);
static char *GetPositionString(char *dest, INT16 x, INT16 y);

static void SampleADCTask(void *argument);
static void DrawTempTask(void *argument);
static void DrawAccTask(void *argument);
static void DrawBallTask(void *argument);

// Global variables
static INT16 Acc_axe_X, Acc_axe_X_offset;
static INT16 Acc_axe_Y, Acc_axe_Y_offset;
static INT16 Temperature;

#define ToggleBit(GPIO_Pin) P1OUT ^= GPIO_Pin;

int main(void)
{
  BallDef *aBall;
  char *freeNode;

  /*** Disable watchdog timer ***/
  WDTCTL = WDTPW + WDTHOLD;

  /*** Set the system clock characteristics ***/
  OSInitializeSystemClocks();

  /*** Configure internal reference ***/
  while(REFCTL0 & REFGENBUSY);     // Wait while ref generator is busy
  REFCTL0 |= REFVSEL_0 + REFON;    // Select internal ref = 1.5V and internal reference ON
  __delay_cycles(400);             // Delay for Ref to settle

  /*** Initialize ADC ***/
  ADC10CTL0 = ADC10ON + ADC10SHT_5;
  ADC10CTL1 = ADC10SHP;
  ADC10CTL2 = ADC10RES;
  // Enable A/D channel inputs
  ACC_PORT_SEL0 |= ACC_X_PIN + ACC_Y_PIN;
  ACC_PORT_SEL1 |= ACC_X_PIN + ACC_Y_PIN;
  ACC_PORT_DIR &= ~(ACC_X_PIN + ACC_Y_PIN);
  // Enable ACC_POWER
  ACC_PWR_PORT_DIR |= ACC_PWR_PIN;
  ACC_PWR_PORT_OUT |= ACC_PWR_PIN;
  // Allow the accelerometer to settle before sampling any data
  __delay_cycles(200000);

  /*** Configure GPIO ***/
  P1DIR |= BIT0 | BIT1 | BIT2 | BIT3; // Enable debug flag

  /*** Configure UART ***/
  InitializeUARTHardware();
  OSInitTransmitUART(8,18,OS_IO_USCI_A0_TX); // 8 buffer of 18 characters each

  /*** Get accelerometer offset ***/
  Acc_axe_X_offset = GetADC10Sample(ACC_X_CHANNEL);
  Acc_axe_Y_offset = GetADC10Sample(ACC_Y_CHANNEL);

  /*** Create tasks ****/
  #if defined(ZOTTAOS_VERSION_HARD)
     OSCreateTask(SampleADCTask,0,100000,10000,NULL);
     OSCreateTask(DrawAccTask,0,500000,500000,NULL);
     OSCreateTask(DrawTempTask,0,1000000,1000000,NULL);
     aBall = (BallDef *)OSMalloc(sizeof(BallDef));
     aBall->X = 5; aBall->Y = 5;    // Set initial ball position
     aBall->Coef = 10;              // Friction coefficient
     aBall->Char = "Z";             // The character representing the ball
     OSCreateTask(DrawBallTask,0,100000,100000,aBall);
     aBall = (BallDef *)OSMalloc(sizeof(BallDef));
     aBall->X = 10; aBall->Y = 10;  // Set initial ball position
     aBall->Coef = 15;              // Friction coefficient
     aBall->Char = "O";             // The character representing the ball
     OSCreateTask(DrawBallTask,0,100000,100000,aBall);
     aBall = (BallDef *)OSMalloc(sizeof(BallDef));
     aBall->X = 15; aBall->Y = 15;  // Set initial ball position
     aBall->Coef = 20;              // Friction coefficient
     aBall->Char = "T";             // The character representing the ball
     OSCreateTask(DrawBallTask,0,100000,100000,aBall);
  #elif defined(ZOTTAOS_VERSION_SOFT)
     #error Untested with ZottaOS-Soft
  #else
     #error Wrong kernel version
  #endif

  /*** Initialize terminal ***/
  // Clear terminal screen
  if ((freeNode = (char *)OSGetFreeNodeUART(OS_IO_USCI_A0_TX)) != NULL) {
     strcpy(freeNode,"\x1B[2J");
     OSEnqueueUART(freeNode,strlen(freeNode),OS_IO_USCI_A0_TX);
  }
  // Hide terminal cursor
  if ((freeNode = (char *)OSGetFreeNodeUART(OS_IO_USCI_A0_TX)) != NULL) {
     strcpy(freeNode,"\x1B[?25h");
     OSEnqueueUART(freeNode,strlen(freeNode),OS_IO_USCI_A0_TX);
  }
  /* Start the OS so that it starts scheduling the user tasks */
  return OSStartMultitasking(NULL,NULL);
} /* end of main */


/* SampleADCTask: Sample X and Y accelerometer axis and internal temperature sensor. */
void SampleADCTask(void *argument)
{
  static UINT8 index = 0;
  INT16 x,y;
  P1OUT |= BIT0; // Set debug flag
  x = GetADC10Sample(ACC_X_CHANNEL) - Acc_axe_X_offset;
  // Clip value
  if (x > 99)
     Acc_axe_X = 99;
  if (x < -99)
     Acc_axe_X = -99;
  else
     Acc_axe_X = x;
  y = GetADC10Sample(ACC_Y_CHANNEL) - Acc_axe_Y_offset;
  // Clip value
  if (y > 99)
     Acc_axe_Y = 99;
  if (y < -99)
     Acc_axe_Y = -99;
  else
     Acc_axe_Y = y;
  if (9 == index) {
     // Temp[deg. C] = (ADC10Sample * 1.5 / 1024 - 0.790) / 0.00255
     Temperature = ((long)GetADC10Sample(ACC_TEMP_CHANNEL) * 147 >> 8) - 309;
     index = 0;
  }
  else
     ++index;
  P1OUT &= ~BIT0; // Clear debug flag
  OSEndTask();
} /* end of SampleADCTask */


/* DrawTempTask: Display temperature on terminal. */
void DrawTempTask(void *argument)
{
  char *freeNode, *tmp;
  P1OUT |= BIT1; // Set debug flag
  if ((freeNode = (char *)OSGetFreeNodeUART(OS_IO_USCI_A0_TX)) != NULL) {
     tmp = GetPositionString(freeNode,2,71);
     tmp = CopyIntToString(tmp,Temperature,TRUE);
     strcat(tmp," deg. C");
     OSEnqueueUART(freeNode,strlen(freeNode),OS_IO_USCI_A0_TX);
  }
  P1OUT &= ~BIT1; // Clear debug flag
  OSEndTask();
} /* DrawTempTask */


/* DrawAccTask: Display accelerometer axis values on terminal. */
void DrawAccTask(void *argument)
{
  char *freeNode, *tmp;
  INT16 x, y;
  P1OUT |= BIT2; // Set debug flag
  do { // Get the accelerometer pair value
     x = Acc_axe_X;
     y = Acc_axe_Y;
  } while (x != Acc_axe_X);
  if ((freeNode = (char *)OSGetFreeNodeUART(OS_IO_USCI_A0_TX)) != NULL) {
     tmp = GetPositionString(freeNode,4,72);
     tmp = CopyIntToString(tmp,x,TRUE);
     strcat(tmp," / ");
     CopyIntToString(tmp + 3,y,TRUE);
     OSEnqueueUART(freeNode,strlen(freeNode),OS_IO_USCI_A0_TX);
  }
  P1OUT &= ~BIT2; // Clear debug flag
  OSEndTask();
} /* DrawAccTask */


/* DrawBallTask: Erase ball and draw ball at next position. */
void DrawBallTask(void *argument)
{
  BallDef *aBall = (BallDef *)argument;
  char *freeNode;
  INT16 accX, accY;
  P1OUT |= BIT3; // Set debug flag
  /*** Get x and y axis accelerometer values ***/
  do {
     accX = Acc_axe_X;
     accY = Acc_axe_Y;
  } while (accX != Acc_axe_X);
  /*** Erase ball at previous position ***/
  if ((freeNode = (char *)OSGetFreeNodeUART(OS_IO_USCI_A0_TX)) != NULL) {
     GetPositionString(freeNode,aBall->X,aBall->Y);
     strcat(freeNode," ");
     OSEnqueueUART(freeNode,strlen(freeNode),OS_IO_USCI_A0_TX);
  }
  /*** Calculate next ball position ***/
  aBall->X += accX / aBall->Coef;
  if (aBall->X > 16)
     aBall->X = 16;
  else if (aBall->X < 0)
     aBall->X = 0;
  aBall->Y += accY / aBall->Coef;
  if (aBall->Y > 70)
     aBall->Y = 70;
  else if (aBall->Y < 0)
     aBall->Y = 0;
  /*** Display ball at new position ***/
  if ((freeNode = (char *)OSGetFreeNodeUART(OS_IO_USCI_A0_TX)) != NULL) {
     GetPositionString(freeNode,aBall->X,aBall->Y);
     strcat(freeNode,aBall->Char);
     OSEnqueueUART(freeNode,strlen(freeNode),OS_IO_USCI_A0_TX);
  }
  P1OUT &= ~BIT3; // Clear debug flag
  OSEndTask();
} /* end of DrawBallTask */


/* InitializeUARTHardware: Initializes USCI 0 hardware. */
void InitializeUARTHardware(void)
{
  P2SEL1 |= BIT0 + BIT1;
  P2SEL0 &= ~(BIT0 + BIT1);
  UCA0CTL1 |= UCSWRST;       // Put state machine in reset
  UCA0CTL1 |= UCSSEL__SMCLK; // Select SMCLK (1 MHz) as clock source
  UCA0BR0 = 6;               // 9600 Baud -> UCBRx = 6, UCBRSx = 0x20, UCBRFx = 8,
  UCA0BR1 = 0;               // UCOS16 -> 1
  UCA0MCTLW |= 0x2081;
  UCA0CTL1 &= ~UCSWRST;      // Initialize USCI state machine
  UCA0IE |= UCRXIE;          // Enable USCI_A0 RX interrupt
} /* end of InitializeUARTHardware */


/* GetADC10Sample: Gets an ADC sample on the channel specified by the parameter */
UINT16 GetADC10Sample(UINT16 channel)
{
  while (ADC10CTL1 & BUSY);
  ADC10CTL0 &= ~ADC10ENC;
  ADC10MCTL0 = channel;
  ADC10CTL0 |= ADC10ENC + ADC10SC; // Sampling and conversion start
  while (ADC10CTL1 & BUSY);
  return ADC10MEM0;
} /* end of GetADC10Sample */


/* GetPositionString: Build escape sequence to draw string at position (x;y). */
char *GetPositionString(char *dest, INT16 x, INT16 y)
{
  strcpy(dest,"\x1b[");
  dest = CopyIntToString(dest + 2,x,FALSE);
  *dest++ = ';';
  dest = CopyIntToString(dest,y,FALSE);
  *dest++ = 'H';
  *dest = '\0';
  return dest;
} /* end of GetPositionString */


/* CopyIntToString: Convert i to string and copy it to string s */
char *CopyIntToString(char *s, INT16 value, UINT8 padding)
{
  if (value < 0) {
     *s++ = '-';
     value = -value;
  }
  else if (padding)
     *s++ = ' ';
  if (value < 10) {
     if (padding)
        *s++ = ' ';
  }
  else {
     for (*s = '0'; value >= 10; value -= 10)
        s[0]++;
     ++s;
  }
  *s++ = '0' + value;
  *s = '\0';
  return s;
} /* end of CopyIntToString */
