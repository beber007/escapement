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
/* File Balls_Demol_EXP430F5438.c: Bouncing balls on TI evaluation kit MSP-EXP430F5438.
** Version identifier: March 2012
** Authors: MIS-TIC */

/* The purpose of this program is to demonstrate most of the available features in
** ZottaOS with a non-trivial application having periodic and aperiodic tasks, multiple
** sources of interruptions and a RS232 connection with a PC.
** The application runs on TI evaluation kit MSP-EXP430F5438, which namely has an LCD
** display, accelerometers, a trackpoint button (joystick) allowing navigation, and two
** pushbuttons referred to as S1 and S2.
**
** What This Application Does
** ---------------------------
** claude Le banner haut contient le nom de la version de ZottaOS ("ZottaOSHard Demo" ou "ZottaOSSoft Demo") et l'heure
** Le banner bas contient de gauche à droite la temperature, vcc, la mesure de la charge du cpu et en plus pour
** la version soft le niveau de charge de la tâche de fond.
** A banner with the text "Demo ZottaOS" is displayed at the top of the LCD screen. The
** lower part of the LCD displays the ambient temperature, the supply voltage (VCC) ap-
** plied to the microcontroller and finally the current time. Finally, the middle part
** of the screen delimits an area where 3 balls follows the physical orientation of the
** evaluation kit, bouncing off from the boundaries when they hit them.
**
** Trackpoint Button Functions
** ---------------------------
** The trackpoint allows the user to adjust the system time:
**  - pushing left or right respectively increases and decreases the minute setting;
**  - pushing upwards or downwards respectively increases and decreases the hour setting.
**
** claude le bouton S1 sert à régler le contraste et le bouton S2 permet lors de l'utilisation 
** de la version soft à régler le niveau de charge de la tâche de fond. 
**
** Addition Information
** --------------------
** The reference point of gravity is taken when pressing on the trackpoint selection
** button when the menu is shown on the LCD. This also starts the application per se.
** The ambient temperature is periodically measured via ADC12 and displayed in the lower
** left part of the LCD.
** The supply voltage applied to the microcontroller is also periodically measured by the
** ADC12 and displayed on the center of the lower part of the LCD.
** The accelerometers measure the movement of the evaluation kit; these measurements make
** it possible to move the ball displayed on the LCD.
** It is also possible to connect the evaluation kit to a PC through a 9600 Baud USB con-
** nector. This additional functionality uses the UART API of ZottaOS. The user can then
** remotely employ the keys 'a', 's', 'w', and 'y' to emulate the 4 directions of the
** trackpoint button and set the system time. The system time is also sent to the PC con-
** sole each time the system time changes.
**
** Notes
** -----
**
** claude
** Pour que la mesure de la charge du cpu puisse être réalisée, il est nécessaire qu'une connection
** soit réalisée entre le point de test de SMCLK et le pin P4.7 du connecteur des ports, comme indiqué
** sur la figure ci-dessous. 
**              MSP430
**         -----------------
**        |            P11.2|--------o SMCLK Test point
**        |                 |        |
**        |                 |        |
**        |             P4.7|--------o P4.7 on PORT connector
**        |                 |
** 
** The LCD is turned off after an inactivity of at least 10 seconds to make additional
** energy savings. As soon as there is an interrupt (button input or a detection of move-
** ment), the LCD is turned back on.
**
** ZottaOS Settings To Do in ZottaOSHard.h (See page 73 of ZottaOS_User_Manual.pdf)
** ---------------------------------------
** claude uniquement pour ZottaOSHard
** (1) Symbolic constant SCHEDULER_REAL_TIME_MODE set to DEADLINE_MONOTONIC_SCHEDULING 
**
** claude ZottaOSconf.exe Project Properties
** ------------------
** Balls_Demo_EXP430F5438.zot contient la configuration
*/

#include "../../../ZottaOS.h"
#include "../../UART.h"
#include <string.h>
#include "LCD_EXP430F5438/hal_lcd_ZottaOS.h"

// claude à contrôler
/* The application is based on modified LCD HAL files supplied by TI */

/*******************/
/* ISR Descriptors */
/*******************/
typedef struct ButtonsDescriptorDef { // Button interrupt descriptor
  void (*ButtonsInterruptHandler)(struct ButtonsDescriptorDef *);
} ButtonsDescriptorDef;

typedef struct RTCDescriptorDef { // RTC interrupt descriptor
  void (*RTCInterruptHandler)(struct RTCDescriptorDef *);
} RTCDescriptorDef;

typedef struct LoadTimerDescriptorDef { // Load timer interrupt descriptor
  void (*LoadTimerInterruptHandler)(struct LoadTimerDescriptorDef *);
} LoadTimerDescriptorDef;

typedef struct ADC12DescriptorDef { // ISR descriptor for ADC12
   void (*ADC12InterruptHandler)(struct ADC12DescriptorDef *descriptor);
   INT16 CalibrationOffsetX;
   INT16 CalibrationOffsetY;
} ADC12DescriptorDef;


/************************************/
/* Global Variables and Definitions */
/************************************/
#define LCD_TIMEOUT_RESET  0x00 // Reset value used to turn off the LCD display
#define LCD_WAKEUP         0xFE // LCD in wake-up mode
#define LCD_SLEEP          0xFF // LCD in sleep mode
static UINT8 LCDState = LCD_TIMEOUT_RESET; // Current LCD state
static volatile BOOL LcdLock = FALSE;      // Lock access to the LCD
static UINT8 ContrastLevel = 0;            // LCD current contrast level

/* claude explication du fonctionnement du banner
** 
** 12345678901234567
** ZottaOSHard 00:00
** 
** Le banner est la ligne de 17 char en haut de l'écran.
** Il se décompose en trois partie :
**   11 premier char affichage du nom de l'application partie mobile (scroll)
**   un char de séparation
**   les 5 dernier char permettent l'affichage de l'heure
**   
** BANNER_STRING  chaîne de char affichier initialement
** BANNER_HIDDEN_STRING  chaîne de char afficher le du scroll du banner
** BANNER_HIDDEN_STRING_SIZE taille de la chaîne BANNER_HIDDEN_STRING  
*/ 
#if defined(ZOTTAOS_VERSION_HARD)
   #define BANNER_STRING "ZottaOSHard      "  
   #define BANNER_HIDDEN_STRING " Demo "
   #define BANNER_HIDDEN_STRING_SIZE 6
#elif defined(ZOTTAOS_VERSION_SOFT)
   #define BANNER_STRING "ZottaOSSoft      "  
   #define BANNER_HIDDEN_STRING " Demo "
   #define BANNER_HIDDEN_STRING_SIZE 6
#else
   #error Wrong kernel version
#endif  

static unsigned int HiddenString[BANNER_HIDDEN_STRING_SIZE][FONT_HEIGHT];

static BOOL CalibrationRequest = TRUE;     // claude: Si vrais mises à jour des valeur de calibration (offset)    
typedef struct ADC12Data {
   INT32 Temperature;
   INT32 Vcc;
}ADC12Data;
static void *ADC12Buffer;

#define LOAD_TIMER_CLOCK_FREQUENCY 1500000 // LOAD_TIMER_CLOCK_FREQUENCY = SMCLK / 8 =  12MHz / 8 = 1.5MHz 
#define PERIODE_DRAW_LOAD_TASK 60000       // Load calculation task period
static UINT32 LoadTimerHigh = 0;           // claude partie haute du compteur utilisé pour calculer la charge du cpu 
#if defined(ZOTTAOS_VERSION_SOFT)
   static UINT8 DummyLoadSelection = 0;    // claude permet de mémoriser le niveau de charge de la tâche de fond
#endif  

typedef struct BallParameters {
   int x, y;         // current position
   int radius;
   int oldX, oldY;   // previously drawn position
   int lastX, lastY; // position prior to computing new coordinate
   struct BallParameters *next;
   float resistance; // friction coefficient
   float speedX;     // velocity
   float speedY;
} BallParameters;
static BallParameters *BallQueue = NULL;


/**********/
/* Events */
/**********/
void *ButtonLeftEvent;
void *ButtonUpEvent;
void *ButtonDownEvent;
void *ButtonRightEvent;
void *ButtonSelectEvent;
void *ButtonS1Event;
void *ButtonS2Event;
void *LCDWakeupEvent;
void *DrawClockEvent;
void *UartReceiveEvent;


/********************************/
/* Internal Function Prototypes */
/********************************/
static void InitRTC(void);
static void InitADC12(void);
static void InitLCD(void);
static void InitIOPorts(void);
static void InitButtons(void);
static void InitUART(void);
static void InitLoadTimer(void);
static void CreateBall(INT16 x, INT16 y, INT16 radius, float resistance);
static void AdjustContrast(UINT8 level);
static void DrawClock(void);
static void Wait(UINT32 wait);

static void RTCInterruptMinute(RTCDescriptorDef *descriptor);
static void ButtonsInterruptDown(ButtonsDescriptorDef *descriptor);
static void ButtonsInterruptUp(ButtonsDescriptorDef *descriptor);
static void ButtonsInterruptRight(ButtonsDescriptorDef *descriptor);
static void ButtonsInterruptLeft(ButtonsDescriptorDef *descriptor);
static void ButtonsInterruptSelect(ButtonsDescriptorDef *descriptor);
static void ButtonsInterruptS1(ButtonsDescriptorDef *descriptor);
static void ButtonsInterruptS2(ButtonsDescriptorDef *descriptor);
static BOOL UARTReceiverInterrupt(UINT8 data);
static void LoadTimerInterrupt(LoadTimerDescriptorDef *descriptor);
static void ADC12Interrupt(ADC12DescriptorDef *descriptor);


/**********************/
/* Task Definitions   */
/**********************/
static void AdcStartReadTask(void *argument);
static void DrawBallTask(void *argument);
static void DrawLoadTask(void *argument);
static void DrawTemperatureTask(void *argument);
static void DrawVoltageTask(void *argument);
static void DrawClockTask(void *argument);
static void ButtonLeftTask(void *argument);
static void ButtonUpTask(void *argument);
static void ButtonDownTask(void *argument);
static void ButtonRightTask(void *argument);
static void ButtonSelectTask(void *argument);
static void ButtonS1Task(void *argument);
static void ButtonS2Task(void *argument);
static void UartReceiveTask(void *argument);
static void LCDSleepTask(void *argument);
static void LCDWakeupTask(void *argument);
static void BannerTask(void *argument);
#if defined(ZOTTAOS_VERSION_SOFT)
   static void DummyWorkTask(void *argument);
#endif


int main(void)
{
  WDTCTL = WDTPW + WDTHOLD;  // Disable watchdog timer
  
  OSInitializeSystemClocks();

  // Create all the events used in the application
  ButtonLeftEvent = OSCreateEventDescriptor();
  ButtonUpEvent = OSCreateEventDescriptor();
  ButtonDownEvent = OSCreateEventDescriptor();
  ButtonRightEvent = OSCreateEventDescriptor();
  ButtonSelectEvent = OSCreateEventDescriptor();
  ButtonS1Event = OSCreateEventDescriptor();
  ButtonS2Event = OSCreateEventDescriptor();
  DrawClockEvent = OSCreateEventDescriptor();
  LCDWakeupEvent = OSCreateEventDescriptor();
  UartReceiveEvent = OSCreateEventDescriptor();
  
  CreateBall(30,30,5,0.89f);
  CreateBall(50,50,5,0.91f);

 #if defined(ZOTTAOS_VERSION_HARD)
     // Create all the aperiodic tasks
     OSCreateSynchronousTask(LCDWakeupTask,180000,LCDWakeupEvent,NULL);
     OSCreateSynchronousTask(DrawClockTask,18000,DrawClockEvent,NULL);
     OSCreateSynchronousTask(ButtonLeftTask,18000,ButtonLeftEvent,NULL);
     OSCreateSynchronousTask(ButtonUpTask,18000,ButtonUpEvent,NULL);
     OSCreateSynchronousTask(ButtonDownTask,18000,ButtonDownEvent,NULL);
     OSCreateSynchronousTask(ButtonRightTask,18000,ButtonRightEvent,NULL);
     OSCreateSynchronousTask(ButtonSelectTask,18000,ButtonSelectEvent,NULL);
     OSCreateSynchronousTask(ButtonS1Task,18000,ButtonS1Event,NULL);
     OSCreateSynchronousTask(ButtonS2Task,18000,ButtonS2Event,NULL);
     OSCreateSynchronousTask(UartReceiveTask,18000,UartReceiveEvent,NULL);
     // Create all the periodic tasks
     OSCreateTask(LCDSleepTask,0,180000,180000,NULL);
     OSCreateTask(DrawTemperatureTask,0,180000,180000,NULL);
     OSCreateTask(DrawVoltageTask,0,60000,60000,NULL);
     OSCreateTask(DrawLoadTask,0,PERIODE_DRAW_LOAD_TASK,1999,NULL);
     OSCreateTask(AdcStartReadTask,0,3000,3000,NULL);
     OSCreateTask(BannerTask,0,3000,3000,NULL);
     OSCreateTask(DrawBallTask,0,3000,3000,NULL);
  #elif defined(ZOTTAOS_VERSION_SOFT)
     // Create all the aperiodic tasks
     OSCreateSynchronousTask(LCDWakeupTask,2000,180000,LCDWakeupEvent,NULL);
     OSCreateSynchronousTask(DrawClockTask,200,18000,DrawClockEvent,NULL);
     OSCreateSynchronousTask(ButtonLeftTask,2,18000,ButtonLeftEvent,NULL);
     OSCreateSynchronousTask(ButtonUpTask,2,18000,ButtonUpEvent,NULL);
     OSCreateSynchronousTask(ButtonDownTask,2,18000,ButtonDownEvent,NULL);
     OSCreateSynchronousTask(ButtonRightTask,2,18000,ButtonRightEvent,NULL);
     OSCreateSynchronousTask(ButtonSelectTask,2,18000,ButtonSelectEvent,NULL);
     OSCreateSynchronousTask(ButtonS1Task,4,18000,ButtonS1Event,NULL);
     OSCreateSynchronousTask(ButtonS2Task,2,18000,ButtonS2Event,NULL);
     OSCreateSynchronousTask(UartReceiveTask,1,18000,UartReceiveEvent,NULL);
     // Create all the periodic tasks
     OSCreateTask(LCDSleepTask,1400,0,180000,180000,1,1,0,NULL);
     OSCreateTask(DrawTemperatureTask,1600,0,180000,180000,1,1,0,NULL);
     OSCreateTask(DrawVoltageTask,650,0,60000,60000,1,1,0,NULL);
     OSCreateTask(DrawLoadTask,600,0,PERIODE_DRAW_LOAD_TASK,1999,1,1,0,NULL);
     OSCreateTask(AdcStartReadTask,1,0,3000,3000,1,1,0,NULL);
     OSCreateTask(BannerTask,190,0,3000,3000,1,8,0,NULL);
     OSCreateTask(DrawBallTask,1400,0,3000,3000,1,10,0,NULL);
     OSCreateTask(DummyWorkTask,9300,0,15000,15000,1,1,0,NULL);
  #else
     #error Wrong kernel version
  #endif  

  __asm("\teint"); // Enable interrupts

  // Initialize all hardware devices
  InitIOPorts();
  InitRTC();
  InitADC12();
  InitLCD();
  InitUART();
  DrawClock();
  Wait(0x40000); // Wait until all bytes are sent by the UART
  InitButtons();
  InitLoadTimer();
 
  /* Start the OS so that it starts scheduling the application tasks */
  return OSStartMultitasking();
} /* end of main */


/* CreateBall: Creates a ball by adding a new BallParameters structure onto the dynamic
** list of balls (global variable BallQueue).
** Parameters:
**   - x, y: coordinates of the ball to create
**   - radius: constant radius
**   - resistance: a value between 0.5f and 1.0f representing the surface friction that
**                 decelerates the ball from frame to frame. */
void CreateBall(INT16 x, INT16 y, INT16 radius, float resistance)
{  
  BallParameters *ball;
  ball = (BallParameters *)OSMalloc(sizeof(BallParameters));
  ball->x = ball->oldX = x;
  ball->y = ball->oldY = y;
  ball->radius = radius;
  ball->resistance = resistance;
  ball->speedX = ball->speedY = 0.0f;
  ballParameters->next = BallQueue; 
  BallQueue = ballParameters;
} /* end of CreateBall */


/* InitLoadTimer: claude Initializes the timer TB0 for cpu load calculation. */
void InitLoadTimer(void)
{
  LoadTimerDescriptorDef *LoadTimerDescriptor;
  LoadTimerDescriptor = (LoadTimerDescriptorDef *)OSMalloc(sizeof(LoadTimerDescriptorDef));
  LoadTimerDescriptor->LoadTimerInterruptHandler = LoadTimerInterrupt;
  OSSetISRDescriptor(OS_IO_TIMER0_B1_TB,LoadTimerDescriptor);
  TB0CTL |= TBSSEL_0 | TBIE | ID_3 | MC_2; // Start the timer
} /* end of InitLoadTimer */


/* InitRTC: Initializes the RTC to have an interrupt every minute. */
void InitRTC(void)
{  
  RTCDescriptorDef *RTCDescriptorMinute;
  RTCDescriptorMinute = (RTCDescriptorDef *)OSMalloc(sizeof(RTCDescriptorDef));
  RTCDescriptorMinute->RTCInterruptHandler = RTCInterruptMinute;
  OSSetISRDescriptor(OS_IO_RTC_TEV,RTCDescriptorMinute);
  RTCCTL01 |= RTCMODE + RTCHOLD + RTCTEVIE;
  RTCHOUR = 0;
  RTCMIN = 0;
  RTCSEC = 0;  
  RTCDAY = 1;
  RTCMON = 1;
  RTCYEAR = 2009;
  RTCCTL01 &= ~RTCHOLD;
} /* end of InitRTC */


/* InitIOPorts: Initializes all IO ports. */
void InitIOPorts(void)
{
  /* P1.0 and P1.1 Leds */
  P1OUT = 0x0;
  P1DIR = 0xFF;  
  P1SEL = 0x0;
  /* P2.1 to P2.7 Buttons */
  P2OUT |= 0xFE;
  P2DIR &= ~0xFE;
  P2REN |= 0xFE; 
  P2SEL &= ~0xFE;
  /* P3 not use */
  P3OUT = 0x0;  
  P3DIR = 0xFF;
  P3SEL = 0x0;
  /* P4.7 input clock for timer TB0 (for load calculation) */
  P4OUT = 0x0;  
  P4DIR = 0x7F;
  P4SEL = 0x80;
  /* P5.7 USB RX Pin
  ** P5.6 USB TX Pin */
  P5OUT = 0x0;    
  P5DIR = 0x7F; // 
  P5REN |= BIT7;
  P5SEL = BIT7 + BIT6;
  /* P6.0 Accelerometer power
  ** P6.1 and P6.2 ADC12 input (accelerometer)
  ** P6.6 Audio power */
  P6OUT = 0x41;    
  P6DIR = 0xF9;
  P6SEL = 0x06;  
  /* P7 not use */
  P7OUT = 0x0;  
  P7DIR = 0xFF;
  P7SEL = 0x0003;  
  /* P8 not use */
  P8OUT = 0x0;  
  P8DIR = 0xFF;
  P8SEL = 0x0003;  
  /* P9 not use */
  P9OUT = 0x0;  
  P9DIR = 0xFF;
  P9SEL = 0x0;  
  /* P10.0 USB RST pin, if enabled with J5 */
  P10OUT = 0x0;  
  P10DIR = 0xFE;
  P10SEL = 0x0;  
  /* P11.0 ACLK Output
  ** P11.1 MCLK Output
  ** P11.2 SMCLK Output */
  P11OUT = 0x0;
  P11DIR = 0xFF;
  P11SEL = 0x07;
  /* PJ JTAG */
  PJOUT = 0x0;    
  PJDIR = 0xFF;
} /* end of InitIOPorts */


/* InitADC12: Sets up the ADC12 to measure the movements, temperature and voltage. */
void InitADC12(void)
{
  ADC12DescriptorDef *ADC12Descriptor;
  REFCTL0 &= ~REFMSTR; // Set ADC12 registers to control voltage reference 
  ADC12CTL0 &= ~ADC12ENC; // Ensure ENC is clear  
  ADC12CTL0 = ADC12ON + ADC12SHT0_7 + ADC12MSC + ADC12REF2_5V + ADC12REFON;
  ADC12CTL1 = ADC12SHP + ADC12CONSEQ_1;  
  ADC12MCTL0 = ADC12INCH_1; // Set accelerometer X channel
  ADC12MCTL1 = ADC12INCH_2; // Set accelerometer Y channel
  ADC12MCTL2 = ADC12SREF_1 + ADC12INCH_10; // Set temperature channel
  ADC12MCTL3 = ADC12SREF_1 + ADC12INCH_11 + ADC12EOS; // Set Vcc channel
  Wait(0xFFFF); // Wait for reference stabilization
  /* Initialize intertask communication with an asynchronous 3-slot buffer. */
  ADC12Buffer = OSInitBuffer(sizeof(ADC12Data),OS_BUFFER_TYPE_3_SLOT,NULL);
  /* Register the interrupt handler for the ADC. */
  ADC12Descriptor = (ADC12DescriptorDef *)OSMalloc(sizeof(ADC12DescriptorDef));
  ADC12Descriptor->ADC12InterruptHandler = ADC12Interrupt;
  OSSetISRDescriptor(OS_IO_ADC12_ADM3,ADC12Descriptor);
} /* end of InitADC12 */


/* InitLCD: Initialize LCD and backlight. */
void InitLCD(void)
{
  UINT8 i,j;
  char *tmp = BANNER_HIDDEN_STRING;
  BallParameters *ball;
  halLcdInit();
  halLcdBackLightInit();
  halLcdSetBackLight(16); // 0 = off, 16 = max
  halLcdClearScreen(); 
  /* claude intialisation des char caché */
  for (j = 0; j < BANNER_HIDDEN_STRING_SIZE; j++)
     for (i = 0; i < FONT_HEIGHT; i++)
        HiddenString[j][i] = 0xFFFF - fonts[fonts_lookup[tmp[j]] * (FONT_HEIGHT + 1) + i];
  halLcdPrintLine(BANNER_STRING,0,INVERT_TEXT);
  halLcdPrintLine("Joy. left  min. +",1,0);
  halLcdPrintLine("Joy. right min. -",2,0);
  halLcdPrintLine("Joy. up    hour +",3,0);
  halLcdPrintLine("Joy. down  hour -",4,0);
  halLcdPrintLine("S1 Adj. contraste",5,0);
  #if defined(ZOTTAOS_VERSION_SOFT)
     halLcdPrintLine("S2 Adj. load     ",6,0);
  #endif  
  halLcdPrintLine("Press joystick...",8,0);
  AdjustContrast(ContrastLevel);
  /* Busy wait until user presses the joystick. This also allows the user to set the LCD
  ** contrast and time. */ // Beber: aussi l'heure?
  while ((P2IN & 0x08) != 0x00) {
     if ((P2IN & 0x40) == 0x0) {   /* Adjust contrast if S1 is pressed */
        if (++ContrastLevel > 7)
           ContrastLevel = 0;
        AdjustContrast(ContrastLevel);
        Wait(0x50000); 
     }
  }
  /* claude Keep only the banner row */
  halLcdPrintLine("                 ",1,OVERWRITE_TEXT);
  halLcdPrintLine("                 ",2,OVERWRITE_TEXT);
  halLcdPrintLine("                 ",3,OVERWRITE_TEXT);
  halLcdPrintLine("                 ",4,OVERWRITE_TEXT);
  halLcdPrintLine("                 ",5,OVERWRITE_TEXT);
  halLcdPrintLine("                 ",6,OVERWRITE_TEXT);
  halLcdPrintLine("                 ",8,INVERT_TEXT);
  /* Draw the balls at their initial positions. */
  for (ball = BallQueue; ball != NULL; ball = ball->next) {
     halLcdFilledCircle(ball->x,ball->y,ball->radius,PIXEL_ON); 
     halLcdFilledCircle(ball->x,ball->y,ball->radius - 1,PIXEL_ON);
  }
} /* end of InitLCD */


/* InitButtons: Initialize the trackpoint button ISR. */
void InitButtons(void)
{
  ButtonsDescriptorDef *ButtonsDescriptor;
  ButtonsDescriptor = (ButtonsDescriptorDef *)OSMalloc(sizeof(ButtonsDescriptorDef));
  ButtonsDescriptor->ButtonsInterruptHandler = ButtonsInterruptDown;
  OSSetISRDescriptor(OS_IO_PORT2_5,ButtonsDescriptor);
  ButtonsDescriptor = (ButtonsDescriptorDef *)OSMalloc(sizeof(ButtonsDescriptorDef));
  ButtonsDescriptor->ButtonsInterruptHandler = ButtonsInterruptUp;
  OSSetISRDescriptor(OS_IO_PORT2_4,ButtonsDescriptor);
  ButtonsDescriptor = (ButtonsDescriptorDef *)OSMalloc(sizeof(ButtonsDescriptorDef));
  ButtonsDescriptor->ButtonsInterruptHandler = ButtonsInterruptSelect;
  OSSetISRDescriptor(OS_IO_PORT2_3,ButtonsDescriptor);
  ButtonsDescriptor = (ButtonsDescriptorDef *)OSMalloc(sizeof(ButtonsDescriptorDef));
  ButtonsDescriptor->ButtonsInterruptHandler = ButtonsInterruptRight;
  OSSetISRDescriptor(OS_IO_PORT2_2,ButtonsDescriptor);
  ButtonsDescriptor = (ButtonsDescriptorDef *)OSMalloc(sizeof(ButtonsDescriptorDef));
  ButtonsDescriptor->ButtonsInterruptHandler = ButtonsInterruptLeft;
  OSSetISRDescriptor(OS_IO_PORT2_1,ButtonsDescriptor);
  ButtonsDescriptor = (ButtonsDescriptorDef *)OSMalloc(sizeof(ButtonsDescriptorDef));
  ButtonsDescriptor->ButtonsInterruptHandler = ButtonsInterruptS1;
  OSSetISRDescriptor(OS_IO_PORT2_6,ButtonsDescriptor);
  ButtonsDescriptor = (ButtonsDescriptorDef *)OSMalloc(sizeof(ButtonsDescriptorDef));
  ButtonsDescriptor->ButtonsInterruptHandler = ButtonsInterruptS2;
  OSSetISRDescriptor(OS_IO_PORT2_7,ButtonsDescriptor);
  P2IFG = 0x0;   // Clear interrupts flags
  P2IES |= 0xFE; // Allow interrupts on the rising edges
  P2IE |= 0xFE;  // Enable interrupts
} /* end of InitButtons */


/* InitUART: Initialize the UART and then send the menu with the options that the user
** can remotely send to the evaluation kit. */
void InitUART(void)
{
  char *string;
  OSInitTransmitUART(10,40,OS_IO_USCI_A1_TX);
  OSInitReceiveUART(UARTReceiverInterrupt,OS_IO_USCI_A1_RX);
  UCA1CTL1 = SWRT;       // Reset State
  UCA1CTL0 = UCMODE_0;
  UCA1CTL0 &= ~UC7BIT;   // 8-bit characters
  UCA1CTL1 |= UCSSEL_1;
  UCA1BR0 = 3;           // 32768 Hz / 9600 bauds
  UCA1BR1 = 0;
  UCA1MCTL = 0x6;
  UCA1CTL1 &= ~SWRT;
  UCA1IE |= UCRXIE;
  string = (char *)OSGetFreeNodeUART(OS_IO_USCI_A1_TX);
  strcpy(string,"Demo ZottaOS\n\n\r");
  OSEnqueueUART((UINT8 *)string,strlen(string),OS_IO_USCI_A1_TX);
  string = (char *)OSGetFreeNodeUART(OS_IO_USCI_A1_TX);
  strcpy(string,"Key 'w' increments minute\n\r");
  OSEnqueueUART((UINT8 *)string,strlen(string),OS_IO_USCI_A1_TX);
  string = (char *)OSGetFreeNodeUART(OS_IO_USCI_A1_TX);
  strcpy(string,"Key 'y' decrements minute\n\r");
  OSEnqueueUART((UINT8 *)string,strlen(string),OS_IO_USCI_A1_TX);
  string = (char *)OSGetFreeNodeUART(OS_IO_USCI_A1_TX);
  strcpy(string,"Key 's' increments hour\n\r");
  OSEnqueueUART((UINT8 *)string,strlen(string),OS_IO_USCI_A1_TX);
  string = (char *)OSGetFreeNodeUART(OS_IO_USCI_A1_TX);
  strcpy(string,"Key 'a' decrements hour\n\r");
  OSEnqueueUART((UINT8 *)string,strlen(string),OS_IO_USCI_A1_TX);
  string = (char *)OSGetFreeNodeUART(OS_IO_USCI_A1_TX);
  strcpy(string,"\n\r");
  OSEnqueueUART((UINT8 *)string,strlen(string),OS_IO_USCI_A1_TX);
} /* end of InitUART */


/* AdjustContrast: Set the contrast of the LCD display */
void AdjustContrast(UINT8 level)
{
  if (!LcdLock && LCDState != LCD_WAKEUP && LCDState != LCD_SLEEP) {
     LcdLock = TRUE;
     switch (level) {
        case 0:
           halLcdSetContrast(95);
           break;
        case 1:
           halLcdSetContrast(100);
           break;
        case 2:
           halLcdSetContrast(105);
           break;
        case 3:
           halLcdSetContrast(110);
           break;
        case 4:
           halLcdSetContrast(115);
           break;
        case 5:
           halLcdSetContrast(120);
           break;
        case 6:
           halLcdSetContrast(125);
           break;
        case 7:
           halLcdSetContrast(127);
        default:
           break;
     }
     LcdLock = FALSE;
  }
} /* end of AdjustContrast */


/* DrawClock: Function whose purpose is to display the time at the bottom right of the
** LCD display.*/
void DrawClock(void)
{
  char clockStr[6];
  char *clockStrUART;
  UINT8 tmp;
  if (!LcdLock) {
     /* Convert time into a string of characters */
     tmp = RTCHOUR;
     clockStr[0] = '0';
     while (tmp >= 10) {
        clockStr[0]++;
        tmp -= 10;
     }
     clockStr[1] = '0' + tmp;
     clockStr[2] = ':';
     tmp = RTCMIN;
     clockStr[3] = '0';
     while (tmp >= 10) {
        clockStr[3]++;
        tmp -= 10;
     }
     clockStr[4] = '0' + tmp;
     clockStr[5] = '\0';
     /* Display the new time value on the LCD */
     LcdLock = TRUE;
     halLcdPrintLineCol(clockStr,0,12,OVERWRITE_TEXT + INVERT_TEXT);
     LcdLock = FALSE;
  }
  // Send the new time value to the the console via the UART
  clockStrUART = (char *)OSGetFreeNodeUART(OS_IO_USCI_A1_TX);
  strcpy(clockStrUART,clockStr);
  strcat(clockStrUART,"\r");
  OSEnqueueUART((UINT8 *)clockStrUART,strlen(clockStrUART),OS_IO_USCI_A1_TX);
} /* end of DrawClock */


/* Wait: Actively wait for a duration equal to the parameter. This function is called by
** main to temporize until the menus are sent so that there processing does not become
** part of the WCET of the tasks. Note that this code cannot be inserted in main because
** of the local variable bug. */
void Wait(UINT32 wait)
{
  volatile UINT32 i;
  for (i = 0; i < wait; i++);
} /* end of Wait */


/* LCDSleepTask: Periodic Task to turn off the LCD if nothing was displayed to the LCD
** during 3 consecutive executions of this task. Every time that a task outputs to the
** LCD the task resets the counter. Every time that this task executes, it increments a
** counter and once this counter reaches the value 2, the LCD is turned off. */
void LCDSleepTask(void *argument)
{
  if (!LcdLock && LCDState != LCD_WAKEUP && LCDState != LCD_SLEEP) {
     LcdLock = TRUE;
     if (LCDState == 2) {
        halLcdSetBackLight(0);
        halLcdStandby();
        LCDState = LCD_SLEEP;
     }
     else
        LCDState++;
     LcdLock = FALSE;
  }
  OSEndTask();
} /* end of LCDSleepTask */


/* LCDWakeupTask: Event-driven task that wakes up the LCD. This task is scheduled very
** time that a task needs to display something on the LCD and notices that the LCD is
** turned off. */
void LCDWakeupTask(void *argument)
{
  if (!LcdLock) {
     LcdLock = TRUE;
     halLcdInit();
     halLcdInit();
     halLcdSetBackLight(16); // 0 = off, 16 = max
     LCDState = LCD_TIMEOUT_RESET;
     LcdLock = FALSE;
     OSScheduleSuspendedTask(DrawClockEvent);
  }
  OSSuspendSynchronousTask();
} /* end of LCDWakeupTask */


/* ButtonUpTask: Event-driven task scheduled by the button and UART ISRs and whose pur-
** pose is to increment the minutes. The task reschedules itself allowing to continuously
** count while the button is pressed, and as soon as the button is released, it re-enables
** the interrupts for this button. The LCD is waked up if necessary. */
void ButtonUpTask(void *argument)
{
  static BOOL first = TRUE;
  if (first || (P2IN & 0x10) == 0x00) {
     if (RTCMIN != 59)
        RTCMIN++;
     else
        RTCMIN = 0;
     first = FALSE;
     OSScheduleSuspendedTask(DrawClockEvent);
  }
  if ((P2IN & 0x10) == 0x00)
     OSScheduleSuspendedTask(ButtonUpEvent);
  else {
     first = TRUE;
     P2IFG &= ~0x10;
     P2IE |= 0x10;
  }
  if (LCDState == LCD_SLEEP) {
     OSScheduleSuspendedTask(LCDWakeupEvent);
     LCDState = LCD_WAKEUP;
  }
  else
     LCDState = LCD_TIMEOUT_RESET; // Reset the LCD turn-off counter
  OSSuspendSynchronousTask();
} /* end of ButtonUpTask */


/* ButtonDownTask: Same as ButtonUpTask but decrements the minutes. */
void ButtonDownTask(void *argument)
{
  static BOOL first = TRUE;
  if (first || (P2IN & 0x20) == 0x00) {
     if (RTCMIN != 0)
        RTCMIN--;
     else
        RTCMIN = 59;
     first = FALSE;
     OSScheduleSuspendedTask(DrawClockEvent);
  }
  if ((P2IN & 0x20) == 0x00)
     OSScheduleSuspendedTask(ButtonDownEvent);
  else {
     first = TRUE;
     P2IFG &= ~0x20;
     P2IE |= 0x20;
  }
  if (LCDState == LCD_SLEEP) {
     OSScheduleSuspendedTask(LCDWakeupEvent);
     LCDState = LCD_WAKEUP;
  }
  else
    LCDState = LCD_TIMEOUT_RESET; // Reset the LCD turn-off counter
  OSSuspendSynchronousTask();
} /* end of ButtonDownTask */


/* ButtonRightTask: Same as ButtonUpTask but increments the hours. */
void ButtonRightTask(void *argument)
{
  static BOOL first = TRUE;
  if (first || (P2IN & 0x04) == 0x00) {
     if (RTCHOUR != 23)
        RTCHOUR++;
     else
        RTCHOUR = 0;
     first = FALSE;
     OSScheduleSuspendedTask(DrawClockEvent);
  }
  if ((P2IN & 0x04) == 0x00)
     OSScheduleSuspendedTask(ButtonRightEvent);
  else {
     first = TRUE;
     P2IFG &= ~0x04;
     P2IE |= 0x04;
  }
  if (LCDState == LCD_SLEEP) {
     OSScheduleSuspendedTask(LCDWakeupEvent);
     LCDState = LCD_WAKEUP;
  }
  else
     LCDState = LCD_TIMEOUT_RESET; // Reset the LCD turn-off counter
  OSSuspendSynchronousTask();
} /* end of ButtonRightTask */


/* ButtonLeftTask: Same as ButtonUpTask but decrements the hours. */
void ButtonLeftTask(void *argument)
{
  static BOOL first = TRUE;
  if (first || (P2IN & 0x02) == 0x00) {
     if (RTCHOUR != 0)
        RTCHOUR--;
     else
        RTCHOUR = 23;
     first = FALSE;
     OSScheduleSuspendedTask(DrawClockEvent);
  }
  if ((P2IN & 0x02) == 0x00)
     OSScheduleSuspendedTask(ButtonLeftEvent);
  else {
     first = TRUE;
     P2IFG &= ~0x02;
     P2IE |= 0x02;
  }
  if (LCDState == LCD_SLEEP) { 
     OSScheduleSuspendedTask(LCDWakeupEvent);
     LCDState = LCD_WAKEUP;
  }
  else
     LCDState = LCD_TIMEOUT_RESET; // Reset the LCD turn-off counter
  OSSuspendSynchronousTask();
} /* end of ButtonLeftTask */


/* ButtonSelectTask: claude executer lors de la pression du button central du jostick.
** A chaque que le boutton est pressé les valeur de calibration (offset) des accelerometre sont mis à jour
** dans l'interruption de l'ADC12 (via CalibrationRequest)*/
void ButtonSelectTask(void *argument)
{
  static BOOL first = TRUE;
  if (first || (P2IN & 0x08) == 0x00) {
      CalibrationRequest = TRUE;
     first = FALSE;
  }
  if ((P2IN & 0x08) == 0x00)
     OSScheduleSuspendedTask(ButtonSelectEvent);
  else {
     first = TRUE;
     P2IFG &= ~0x08;
     P2IE |= 0x08;
  }
  if (LCDState == LCD_SLEEP) { 
     OSScheduleSuspendedTask(LCDWakeupEvent);
     LCDState = LCD_WAKEUP;
  }
  else
     LCDState = LCD_TIMEOUT_RESET; // Reset the LCD turn-off counter
  OSSuspendSynchronousTask();
} /* end of ButtonSelectTask */


/* ButtonS1Task: claude executer lors de la pression du button S1. Permet de faire varier le contraste du LCD */
void ButtonS1Task(void *argument)
{
  static BOOL first = TRUE;
  if (first || (P2IN & 0x40) == 0x00) {
     if (++ContrastLevel > 7)
        ContrastLevel = 0;
     first = FALSE;
  }
  if ((P2IN & 0x40) == 0x00)
     OSScheduleSuspendedTask(ButtonS1Event);
  else {
     first = TRUE;
     P2IFG &= ~0x40;
     P2IE |= 0x40;
  }
  if (LCDState == LCD_SLEEP) { 
     OSScheduleSuspendedTask(LCDWakeupEvent);
     LCDState = LCD_WAKEUP;
  }
  else
    LCDState = LCD_TIMEOUT_RESET; // Reset the LCD turn-off counter
  AdjustContrast(ContrastLevel);
  OSSuspendSynchronousTask();
} /* end of ButtonS1Task */


/* ButtonS2Task: claude executer lors de la pression du button S2. fait varirier le temp d'execution de la tâche de fond*/
void ButtonS2Task(void *argument)
{
  static BOOL first = TRUE;
  if (first || (P2IN & 0x80) == 0x00) {
     #if defined(ZOTTAOS_VERSION_SOFT)
        DummyLoadSelection++;
        if (DummyLoadSelection == 3)
           DummyLoadSelection = 0;
     #endif  
     first = FALSE;
  }
  if ((P2IN & 0x80) == 0x00)
     OSScheduleSuspendedTask(ButtonS2Event);
  else {
     first = TRUE;
     P2IFG &= ~0x80;
     P2IE |= 0x80;
  }
  if (LCDState == LCD_SLEEP) { 
     OSScheduleSuspendedTask(LCDWakeupEvent);
     LCDState = LCD_WAKEUP;
  }
  else
     LCDState = LCD_TIMEOUT_RESET; // Reset the LCD turn-off counter
  OSSuspendSynchronousTask();
} /* end of ButtonS2Task */


/* UartReceiveTask: When a character is received from the UART, a dedicated aperiodic
** task is started for every valid command; these tasks do not re-enable UART interrupts.
** Also, when any character is received including those that are valid, this aperiodic
** task is signaled by the UART ISR so that it can re-enable the interrupt. This trick
** creates a minimum interarrival period between two consecutive interrupts. */
void UartReceiveTask(void *argument)
{
  UCA1IE |= UCRXIE; // Enable UART interrupt
  OSSuspendSynchronousTask();
} /* end of ButtonLeftTask */


/* DrawTemperatureTask: Periodic task that displays the temperature at the bottom right
** of the LCD display. */
void DrawTemperatureTask(void *argument)
{
  ADC12Data data;
  char str[5];
  if (!LcdLock && LCDState != LCD_WAKEUP && LCDState != LCD_SLEEP) {
     while (OSGetCopyBuffer(ADC12Buffer,OS_READ_MULTIPLE,(UINT8 *)&data) !=
                                                                   sizeof(ADC12Data));
     if (data.Temperature / 10 == 0)
        str[0] = ' ';
     else
        str[0] = '0' + data.Temperature / 10;
     str[1] = '0' + data.Temperature % 10;
     str[2] = '^';
     str[3]='C';
     str[4] = 0x0;
     LcdLock = TRUE;
     halLcdPrintLineCol(str,8,0,OVERWRITE_TEXT + INVERT_TEXT);
     LcdLock = FALSE;
  }
  OSEndTask();
} /* end of DrawTemperatureTask */


/* DrawVoltageTask: Periodic task that displays the supply voltage VCC centered at the
** bottom of the LCD display. This task also adjusts the contrast of the LCD. */
void DrawVoltageTask(void *argument)
{
  ADC12Data data;
  UINT16 vcc;
  char str[6];
  if (!LcdLock && LCDState != LCD_WAKEUP && LCDState != LCD_SLEEP) {
     while (OSGetCopyBuffer(ADC12Buffer,OS_READ_MULTIPLE,(UINT8 *)&data) !=
                                                                   sizeof(ADC12Data));
     vcc = data.Vcc;
     str[0] = '0';
     str[1] = '.';
     str[2] = '0';
     while (vcc >= 10) {
        str[0]++;
        vcc -= 10;
     }
     str[2] += vcc;
     str[3] = 'V';  
     str[4] = 0x0;
     LcdLock = TRUE;
     halLcdPrintLineCol(str,8,5,OVERWRITE_TEXT + INVERT_TEXT);
     LcdLock = FALSE;
  }
  OSEndTask();
} /* End of DrawVoltageTask */


/* DrawLoadTask: claude calcul et affiche la charge du cpu. */
void DrawLoadTask(void *argument)
{
  const UINT32 max = (LOAD_TIMER_CLOCK_FREQUENCY / 32768) * (PERIODE_DRAW_LOAD_TASK / 100);
  static UINT32 old = 0;
  char string [4];
  UINT32 current;
  UINT32 diff;
  current = LoadTimerHigh << 16 | TB0R;
  diff = current - old;
  old = current;
  diff /= max;
  if (diff < 100) {
     string[0] = diff / 10 + '0';
     string[1] = diff % 10 + '0';
  }
  else {
     string[0] = ' ';
     string[1] = ' ';
  }
  string[2] = '%';
  string[3] = 0x0;
  if (!LcdLock && LCDState != LCD_WAKEUP && LCDState != LCD_SLEEP) {
     LcdLock = TRUE;
     halLcdPrintLineCol(string,8,10,OVERWRITE_TEXT + INVERT_TEXT);
     LcdLock = FALSE;
  }
  OSEndTask();
} /* End of DrawLoadTask */


#if defined(ZOTTAOS_VERSION_SOFT)
   /* DummyWorkTask: Tâche permettant d'augmenter la charge du cpu afin de mettre en évidence le fonctionement des tache temp-réel tendre.
   ** la charge varie lorsque l'on presse sure le boutton S2. */
   void DummyWorkTask(void *argument)
   {
     char tmp [4];
     UINT32 i;
     if (LCDState != LCD_WAKEUP && LCDState != LCD_SLEEP) {
        switch(DummyLoadSelection) {
           case 1:
              P1OUT ^= 0x01;
              for (i = 0; i < 153500; i++);
              P1OUT ^= 0x01;
              tmp[0] = '4';
              tmp[1] = '0';
              break;
           case 2:   
              P1OUT ^= 0x01;
              for (i = 0; i < 220000; i++);
              P1OUT ^= 0x01;
              tmp[0] = '6';
              tmp[1] = '0';
              break;
           default:
              tmp[0] = '0';
              tmp[1] = '0';
              break;
        }
        tmp[2] = '%';
        tmp[3] = 0x0;
        if (!LcdLock) {
           LcdLock = TRUE;
           halLcdPrintLineCol(tmp,8,14,OVERWRITE_TEXT + INVERT_TEXT);
           LcdLock = FALSE;
        }
     }
     OSEndTask();
   } /* End of DummyWorkTask */
#endif

/* DrawClockTask: Aperiodic task whose purpose is to display the time at the bottom
** right of the LCD display. This task can either be scheduled by RTCInterrupt or when
** the user takes an action that sets the time. */
void DrawClockTask(void *argument)
{
  if (LCDState != LCD_WAKEUP && LCDState != LCD_SLEEP) {
     LCDState = LCD_TIMEOUT_RESET;
     DrawClock();
  }
  OSSuspendSynchronousTask();
} /* end of DrawClockTask */


/* AdcStartReadTask:  Periodic task that triggers an ADC measure. The measured values are
** the accelerometers, the supply voltage and the temperature, which are then stored in
** the slots of an asynchronous reader-writer protocol. Functions halAdcReadTempVcc and
** halAccelerometerRead will later allow the reading of these measures. */
void AdcStartReadTask(void *argument)
{
  ADC12IFG &= ~(BIT3+BIT2+BIT1+BIT0);
  ADC12CTL0 |=  ADC12ENC | ADC12SC | ADC12REFON;  
  ADC12IE |= BIT3;
  OSEndTask();
} /* end of AdcStartReadTask */


/* BannerTask: claude Tâche fesant défiler le texte du titre sur la première ligne du LCD  */
void BannerTask(void *argument)
{
  #if defined(ZOTTAOS_VERSION_SOFT)
     static UINT8 previousInstance = 7;
     UINT8 currentInstance;
     INT8 nbInstance;
  #endif
  if (!LcdLock && LCDState != LCD_WAKEUP && LCDState != LCD_SLEEP) {
     LcdLock = TRUE;
     P4OUT ^= 0x2;
     #if defined(ZOTTAOS_VERSION_SOFT)
        currentInstance = OSGetTaskInstance();
        nbInstance = currentInstance - previousInstance;
        if (nbInstance == 0)
           nbInstance = 1;
        else if (nbInstance < 0)
           nbInstance += 8;
        halLcdScrollLine(0,nbInstance,10,HiddenString,BANNER_HIDDEN_STRING_SIZE);
        previousInstance = currentInstance;
     #else
        halLcdScrollLine(0,1,10,HiddenString,BANNER_HIDDEN_STRING_SIZE);
     #endif
     P4OUT ^= 0x2;
     LcdLock = FALSE;
  }
  OSEndTask();
} /* end of BannerTask */


/* DrawBallTask: Periodic task that updates the displayed movement of the bouncing
** balls. */
void DrawBallTask(void *argument)
{
  BallParameters *ball;
  for (ball = BallQueue; ball != NULL; ball = ball->next)
     if ((ball->oldX != ball->x || ball->oldY != ball->y)) {
        if (LCDState == LCD_SLEEP) { // Is the display turned off?
           OSScheduleSuspendedTask(LCDWakeupEvent);
           LCDState = LCD_WAKEUP;
        }
        else if (LCDState != LCD_WAKEUP && !LcdLock) {
           LCDState = LCD_TIMEOUT_RESET; // Reset the LCD turn-off counter
           LcdLock = TRUE;
           P4OUT ^= 0x1;
           for (ball = BallQueue; ball != NULL; ball = ball->next) {
              halLcdFilledCircle(ball->oldX,ball->oldY,ball->radius,PIXEL_OFF);
              halLcdFilledCircle(ball->x,ball->y,ball->radius,PIXEL_ON);
              ball->oldX = ball->x;
              ball->oldY = ball->y;
           }
           P4OUT ^= 0x1;
           LcdLock = FALSE;
        }
        break;
     }
  }
  OSEndTask();
} /* end of DrawBallTask */


/* ButtonsInterrupt: ISR for the trackpoint button and which is invoked when one of its
** buttons is pressed. The interrupt corresponding to the pressed button is re-enbled at
** the end of the aperiodic task that this routine schedules. This trick somewhat avoids
** the burst created when the user continuously presses on a key. */
void ButtonsInterruptDown(ButtonsDescriptorDef *descriptor)
{
  OSScheduleSuspendedTask(ButtonDownEvent);
} /* end of ButtonsInterruptDown */

void ButtonsInterruptUp(ButtonsDescriptorDef *descriptor)
{
  OSScheduleSuspendedTask(ButtonUpEvent);
} /* end of ButtonsInterruptUp */

void ButtonsInterruptRight(ButtonsDescriptorDef *descriptor)
{
  OSScheduleSuspendedTask(ButtonRightEvent);
} /* end of ButtonsInterruptRight */

void ButtonsInterruptLeft(ButtonsDescriptorDef *descriptor)
{
  OSScheduleSuspendedTask(ButtonLeftEvent);
} /* end of ButtonsInterruptLeft */

void ButtonsInterruptSelect(ButtonsDescriptorDef *descriptor)
{  
  OSScheduleSuspendedTask(ButtonSelectEvent);
} /* end of ButtonsInterruptLeft */

void ButtonsInterruptS1(ButtonsDescriptorDef *descriptor)
{
  OSScheduleSuspendedTask(ButtonS1Event);
} /* end of ButtonsInterruptS1 */

void ButtonsInterruptS2(ButtonsDescriptorDef *descriptor)
{
  OSScheduleSuspendedTask(ButtonS2Event);
} /* end of ButtonsInterruptS2 */

/* UARTReceiverInterrupt: ISR for the receiver part of the UART. The interrupt is
** reenbled at the end of the aperiodic task that this routine schedules. This trick
** somewhat avoids the burst created when the user continuously presses on a key. */
BOOL UARTReceiverInterrupt(UINT8 data)
{
  switch (data) {
     case 'a':  // LEFT
        OSScheduleSuspendedTask(ButtonLeftEvent);
        break;
     case 's':  // RIGHT
        OSScheduleSuspendedTask(ButtonRightEvent);
        break;
     case 'w':  // UP
        OSScheduleSuspendedTask(ButtonUpEvent);
        break;
     case 'y':  // DOWN
        OSScheduleSuspendedTask(ButtonDownEvent);
     default:
        break;
  }
  OSScheduleSuspendedTask(UartReceiveEvent);
  return TRUE;
} /* end of UARTReceiverInterrupt */


/* RTCInterrupt: ISR for the RTC to update the display. */
void RTCInterruptMinute(RTCDescriptorDef *descriptor)
{
  // Update time every minute
  OSScheduleSuspendedTask(DrawClockEvent);
  RTCCTL01 |= RTCTEVIE;   // Enable RTC interrupt
} /* end of RTCInterruptMinute */


/* LoadTimerInterrupt: ISR for the Timer to calculate the CPU load. */
void LoadTimerInterrupt(LoadTimerDescriptorDef *descriptor)
{
  LoadTimerHigh++;
  TB0CTL |= TBIE; // Enable Timer0 B overflow interrupt
} /* end of LoadTimerInterrupt */


float Sqrt(float x)
{
  int i;
  float r, nextR;
  if (x == 0.0f) return 0.0f;
  nextR = x / 2.0;
  for (i = 0; i < 10; i += 1) {
     r = nextR;
     nextR = (x/r +r) / 2.0;
     if (nextR - r == 0.0f) break;
  }
  return nextR;
}

typedef struct {
  float x, y;
} Vector;

#define WALLRIGHT  (LCD_COL - 3)
#define WALLBOTTOM (LCD_ROW - 15)
#define WALLLEFT   0
#define WALLTOP    12

// 2 balls collide when their bounding boxes overlap
void ResolveCollision(BallParameters *ball, BallParameters *otherBall, int dx, int dy)
{
  Vector delta, mtd, v;
  float r, vn, impulse;
  float dist, d;
  int tmp;
  /* Get the minimum translation distance */
  delta.x = ball->x - otherBall->x; delta.y = ball->y - otherBall->y;
  r = ball->radius + otherBall->radius;
  dist = delta.x * delta.x + delta.y * delta.y;
  if (dist >= r*r) return; // they aren't colliding
  // resolve intersection
  if (dist == 0.0f || (d = Sqrt(dist)) == 0.0f) { // Special case. Balls are exactly on top of each other.
     mtd.x = -dx; mtd.y = -dy; // Don't want to divide by zero.
     d = Sqrt(dx * dx + dy * dy);
     mtd.x = mtd.x / d * r; mtd.y = mtd.y / d * r;
  }
  else { // minimum translation distance to push balls apart after intersecting
     r = (r - d) / d;
     mtd.x = delta.x * r;
     mtd.y = delta.y * r;
  }
  if (mtd.x < 0) delta.x = -mtd.x;
  else delta.x = mtd.x;
  if (mtd.y < 0) delta.y = -mtd.y;
  else delta.y = mtd.y;
  // Push-pull them apart (balls have equal mass)
  ball->x += mtd.x * 0.5; otherBall->x -= mtd.x * 0.5;
  if ((tmp = ball->radius - ball->x + WALLLEFT) > 0) {
     otherBall->x += tmp; ball->x += tmp;
  }
  else if ((tmp = ball->x - WALLRIGHT + ball->radius) > 0) {
     otherBall->x -= tmp; ball->x -= tmp;
  }
  if ((tmp = otherBall->radius - otherBall->x + WALLLEFT) > 0) {
     otherBall->x += tmp; ball->x += tmp;
  }
  else if ((tmp = otherBall->x - WALLRIGHT + otherBall->radius) > 0) {
     otherBall->x -= tmp; ball->x -= tmp;
  }
  ball->y += mtd.y * 0.5; otherBall->y -= mtd.y * 0.5;
  if ((tmp = ball->radius - ball->y + WALLTOP) > 0) {
     otherBall->y += tmp; ball->y += tmp;
  }
  else if ((tmp = ball->y - WALLBOTTOM + ball->radius) > 0) {
     otherBall->y -= tmp; ball->y -= tmp;
  }
  if ((tmp = otherBall->radius - otherBall->y + WALLTOP) > 0) {
     otherBall->y += tmp; ball->y += tmp;
  }
  else if ((tmp = otherBall->y - WALLBOTTOM + otherBall->radius) > 0) {
     otherBall->y -= tmp; ball->y -= tmp;
  }
  // impact speed
  v.x = ball->speedX - otherBall->speedX;
  v.y = ball->speedY - otherBall->speedY;
  // normalize mtd
  r = Sqrt(mtd.x * mtd.x + mtd.y * mtd.y);
  if (r != 0.0f) {
     mtd.x /= r; mtd.y /= r;
  }
  else {
     mtd.x = 0.0f; mtd.y = 0.0f;
  }
  vn = v.x * mtd.x + v.y * mtd.y;
  // Is sphere intersecting but moving away from each other already?
  if (vn > 0.0f) return;
  // collision impulse (-1 - restitution) * vn / 2.0f;
  impulse = -1.85f * vn / 2.0f;
  mtd.x *= impulse; mtd.y *= impulse; 
  // change in momentum
  ball->speedX += mtd.x; ball->speedY += mtd.y;
  otherBall->speedX -= mtd.x; otherBall->speedY -= mtd.y;
}


/* Interrupt Handler for ACD12*/
void ADC12Interrupt(ADC12DescriptorDef *descriptor)
{
  ADC12Data data;
  BallParameters *ball;
  INT16 dx;
  INT16 dy;
P4OUT ^= 0x4;
  dx = ADC12MEM0;
  dy = -ADC12MEM1;
  /* Update calibration values */
  if (CalibrationRequest) {
     descriptor->CalibrationOffsetX = dx;
     descriptor->CalibrationOffsetY = dy;
     CalibrationRequest = FALSE;
  }
  /* Update balls position */
  dx = (dx - descriptor->CalibrationOffsetX) / 25;
  dy = (dy - descriptor->CalibrationOffsetY) / 25;
  for (ball = BallQueue; ball != NULL; ball = ball->next) {
     ball->lastX = ball->x; ball->lastY = ball->y;
     ball->speedX -= dx;  // Update the unit velocity of this ball
     ball->speedY -= dy;
     ball->speedX *= ball->resistance; // Simulate some resistance and exponentially reduce the speed
     ball->speedY *= ball->resistance;
     if (ball->speedX < 1.0 && ball->speedX > -1.0) // Prevents continuous movements
        ball->speedX = 0.0;
     if (ball->speedY < 1.0 && ball->speedY > -1.0)
        ball->speedY = 0.0;
     ball->x += ball->speedX; // Update the ball's position with a unit time
     ball->y += ball->speedY;
     if (ball->x < WALLLEFT + ball->radius) { // Detect a left border contact
        ball->speedX = -ball->speedX;
        ball->x = WALLLEFT + ball->radius;
     }
     if (ball->x >= WALLRIGHT - ball->radius) { // Detect a right border contact
        ball->speedX = -ball->speedX;
        ball->x = WALLRIGHT - ball->radius;
     }
     if (ball->y < WALLTOP + ball->radius) { // Detect a top border contact
        ball->speedY = -ball->speedY;
        ball->y = WALLTOP + ball->radius; 
     }
     if (ball->y >= WALLBOTTOM - ball->radius) { // Detect a bottom border contact
        ball->speedY = -ball->speedY;
        ball->y = WALLBOTTOM - ball->radius;
     }
  }
  ResolveCollision(BallQueue,BallQueue->next,dx,dy);
  if (BallQueue->x == BallQueue->lastX && BallQueue->y == BallQueue->lastY) {
     ball = BallQueue->next;
     if (ball->x == ball->lastX && ball->y == ball->lastY) {
        BallQueue->speedX = ball->speedX = dx;
        BallQueue->speedY = ball->speedY = dy;
     }
  }
  // Convert VCC
  data.Vcc = ADC12MEM3; 
  data.Vcc = data.Vcc * 50 / 4096; 
  // Convert Temperature
  // ((A10/4096*2500mV) - 680mV)*(1/2.25mV) = (A10/4096*1111) - 302
  // = (A10 - 1113) * (666 / 4096)
  data.Temperature = ADC12MEM2;
  data.Temperature = ((data.Temperature - 1113) * 666) / 4096;
  OSWriteBuffer(ADC12Buffer,(UINT8 *)&data,sizeof(ADC12Data));
  ADC12IFG = 0;
  ADC12CTL0 &= ~( ADC12ENC | ADC12SC );    
P4OUT ^= 0x4;
} /* end of ADC12Interrupt */
