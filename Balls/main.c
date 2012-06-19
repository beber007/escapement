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
/* File main.c: This program displays a ball movement on a terminal screen according to
** the orientation of the board sensed by the accelerometer.
 * This program samples the following information with the ADC10 converter:
**    - The X and Y axis of the accelerometer (sampling period 100 ms);
**    - Internal temperature sensor (sampling period 1 s).
** All values are transmitted via the UART after sampling. UART baud rate is 9600 baud.
** ANSI escape sequences are sent via the UART to the display the position of a ball.
** All numerical values read from the ADC are also displayed on the terminal.
** Tested on "MSP-EXP430FR5739 - Experimenter Board" with the following configuration of
** the clocks:
**    - MCLK sourced from the DCO (DCOCLK) divided by 1;
**    - ACLK sourced from the DCO (DCOCLK) divided by 16;
**    - SMCLK sourced from the DCO (DCOCLK) divided by 8;
**    - DCO set for factory calibrated value of 8.00 MHz(DCORSEL = 0, DCOFSEL = 3).
** Version identifier: June 2012
** Authors: MIS-TIC */
#include "msp430fr5739.h"
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

// Ball specifications
#define TERMINAL_LINE 16
#define TERMINAL_COL  70
#define FRICTION_COEFFICENT 15

// Initial ball position
int X = 5;
int Y = 5;

// Internal function prototypes
void InitializeUARTHardware(void);
void SendUART(char *str);
unsigned int GetADC10Sample(unsigned int channel);
static char *CopyIntToString(char *s, int value, unsigned char padding);
static char *GetPositionString(char *dest, int x, int y);

void main(void)
{
  char buffer[25], *tmp;
  int index = 0;
  int temperature;
  int Acc_axe_X, Acc_axe_X_offset;
  int Acc_axe_Y, Acc_axe_Y_offset;

  /*** Disable watchdog ***/
  WDTCTL = WDTPW + WDTHOLD;

  /*** Set the system clock characteristics ***/
  CSCTL0_H = 0xA5;
  CSCTL1 |= DCOFSEL0 + DCOFSEL1;        // Set DCO to 8MHz
  CSCTL2 = SELA_3 + SELS_3 + SELM_3;    // Set ACLK = MCLK = SMCLK = DCO
  CSCTL3 = DIVA__16 + DIVS__8 + DIVM_0; // Set all dividers

  /*** Configure internal reference ***/
  while (REFCTL0 & REFGENBUSY);    // Wait while ref generator is busy
  REFCTL0 |= REFVSEL_0 + REFON;    // Select internal ref = 1.5V and internal reference ON
  __delay_cycles(400);             // Delay for ref to settle

  /*** Initialize ADC ***/
  ADC10CTL0 = ADC10ON + ADC10SHT_5;
  ADC10CTL1 = ADC10SHP;
  ADC10CTL2 = ADC10RES;
  //Enable A/D channel inputs
  ACC_PORT_SEL0 |= ACC_X_PIN + ACC_Y_PIN;
  ACC_PORT_SEL1 |= ACC_X_PIN + ACC_Y_PIN;
  ACC_PORT_DIR &= ~(ACC_X_PIN + ACC_Y_PIN);
  // Enable ACC_POWER
  ACC_PWR_PORT_DIR |= ACC_PWR_PIN;
  ACC_PWR_PORT_OUT |= ACC_PWR_PIN;
  // Allow the accelerometer to settle before sampling any data
  __delay_cycles(200000);

  /*** Configure GPIO ***/
  P1DIR |= BIT0;      // Enable debug flag

  /*** Configure UART ***/
  InitializeUARTHardware();

  /*** Get accelerometer offset ***/
  Acc_axe_X_offset = GetADC10Sample(ACC_X_CHANNEL);
  Acc_axe_Y_offset = GetADC10Sample(ACC_Y_CHANNEL);

  /*** Initialize Timer A ***/
  TA0CCR0 = 50000;                 // An interrupt is generate every 100ms
  TA0CTL = TASSEL_1 + MC_1 + TAIE; // use ACLK @ 500kHz

  /* Clear terminal screen */
  strcpy(buffer,"\x1B[2J");
  SendUART(buffer);

  /* Hide terminal cursor */
  strcpy(buffer,"\x1B[?25h");
  SendUART(buffer);

  /*** Enter in principal infinite loop ***/
  while (1) {
     P1OUT |= BIT0; // Set debug flag
     /*** Get new sample from X-axis accelerometer ***/
     Acc_axe_X = GetADC10Sample(ACC_X_CHANNEL) - Acc_axe_X_offset;
     // Clip value
     if (Acc_axe_X > 99)
        Acc_axe_X = 99;
     if (Acc_axe_X < -99)
        Acc_axe_X = -99;
     /*** Get new sample from Y-axis accelerometer ***/
     Acc_axe_Y = GetADC10Sample(ACC_Y_CHANNEL) - Acc_axe_Y_offset;
     // Clip value
     if (Acc_axe_Y > 99)
     Acc_axe_Y = 99;
     if (Acc_axe_Y < -99)
        Acc_axe_Y = -99;
     /*** Move ball ***/
     // Erase ball at previous position
     GetPositionString(buffer,X,Y);
     strcat(buffer," ");
     SendUART(buffer);
     // Calculate next ball position
     X += Acc_axe_X / FRICTION_COEFFICENT;
     if (X > TERMINAL_LINE)
        X = TERMINAL_LINE;
     else if (X < 0)
        X = 0;
     Y += Acc_axe_Y / FRICTION_COEFFICENT;
     if (Y > TERMINAL_COL)
        Y = TERMINAL_COL;
     else if (Y < 0)
        Y = 0;
     // Display ball at new position
     GetPositionString(buffer,X,Y);
     strcat(buffer,"O");
     SendUART(buffer);
     /*** Display accelerometer values ***/
     tmp = GetPositionString(buffer,4,72);
     tmp = CopyIntToString(tmp,Acc_axe_X,1);
     strcat(tmp," / ");
     CopyIntToString(tmp + 3,Acc_axe_Y,1);
     SendUART(buffer);
     if (9 == index) {
        /*** Temperature ***/
        temperature = GetADC10Sample(ACC_TEMP_CHANNEL);   // Get new temperature sample
        // Temp[deg. C] = (ADC10MEM0 * 1.5 / 1024 - 0.790) / 0.00255
        temperature = (int)((long)temperature * 147 >> 8) - 309;
        // Display temperature
        tmp = GetPositionString(buffer,2,71);
        tmp = CopyIntToString(tmp,temperature,1);
        strcat(tmp," deg. C");
        SendUART(buffer);
        index = 0;
     }
     else
        ++index;
     P1OUT &= ~BIT0;  // Clear debug flag
     __bis_SR_register(LPM3_bits+GIE); // Enter in LPM3
     __no_operation();                 // For debugging only
  }
} /* end of main */


/* Timer A0 ISR */
#pragma vector = TIMER0_A1_VECTOR
__interrupt void TIMER0_A1_ISR (void)
{
  TA0CTL &= ~TAIFG; // Clear interrupt flag
  __bic_SR_register_on_exit(LPM3_bits);
} /* end of TIMER0_A1_ISR */


// UART transmit buffer
char *TransmitBuffer;
char TransmitNB;
char TransmitIndex;

/* USCI ISR */
#pragma vector = USCI_A0_VECTOR
__interrupt void USCI_A0_ISR(void)
{
  UCA0TXBUF = TransmitBuffer[TransmitIndex++];
  TransmitNB--;
  if (TransmitNB == 0) {
    UCA0IE &= ~UCTXIE; // Disable transmit interrupt
    __bic_SR_register_on_exit(LPM3_bits);
  }
} /* end of USCI_A0_ISR */


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


/* Send string to UART */
void SendUART(char *str)
{
  TransmitBuffer = str;
  TransmitNB = strlen(str);
  TransmitIndex = 0;
  UCA0IE |= UCTXIE;                 // Enable TX interrupt
  __bis_SR_register(LPM3_bits+GIE); // Enter in LPM3
  __no_operation();                 // For debug only
} /* end of SendUART */


/* GetADC10Sample: Gets an ADC sample on the channel specified by the parameter */
unsigned int GetADC10Sample(unsigned int channel)
{
  while (ADC10CTL1 & BUSY);
  ADC10CTL0 &= ~ADC10ENC;
  ADC10MCTL0 = channel;
  ADC10CTL0 |= ADC10ENC + ADC10SC; // Sampling and conversion start
  while (ADC10CTL1 & BUSY);
  return ADC10MEM0;
} /* end of GetADC10Sample */


/* GetPositionString: Build escape sequence to draw string at position (x;y). */
char *GetPositionString(char *dest, int x, int y)
{
  strcpy(dest,"\x1b[");
  dest = CopyIntToString(dest + 2,x,0);
  *dest++ = ';';
  dest = CopyIntToString(dest,y,0);
  *dest++ = 'H';
  *dest = '\0';
  return dest;
} /* end of GetPositionString */


/* CopyIntToString: Convert i to string and copy it to string s */
char *CopyIntToString(char *s, int value, unsigned char padding)
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
