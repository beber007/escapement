/* Copyright (c) 20011 MIS Institute of the HEIG affiliated to the University of Applied
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
/* File Balls_Demol_EXP430F5438.c: Bouncing balls on TI evaluation kit MSP-EXP430F5438.
** Version identifier: July 2011
** Authors: MIS-TIC */

/* The purpose of this program is to demonstrate most of the available features in
** ZottaOS with a non-trivial application having periodic and aperiodic tasks, multiple
** sources of interruptions and a RS232 connection with a PC.
** The application runs on TI evaluation kit MSP-EXP430F5438, which namely has an LCD
** display, accelerometers, and a trackpoint button allowing navigation. claude + 2 button S1 et S2
**
** What This Application Does
** ---------------------------
** claude Le banner haut contient le nom de la version de Zotta (ZottaOSHard ou ZottaOSSoft) et l'heure
** Le banner bas contient de gauche à droite la temperature, vcc, la mesure de la charge du cpu et pour
** la version soft le .
** A banner with the text "Demo ZottaOS" is displayed at the top of the LCD screen. The
** lower part of the LCD displays the ambient temperature, the supply voltage (VCC) ap-
** plied to the microcontroller and finally the current time. Finally, the middle part
** of the screen delimits an area where 3 balls follows the physical orientation of the
** evaluation kit, bouncing off from the boundaries when it hits them.
**
** Trackpoint Button Functions
** ---------------------------
** The trackpoint allows the user to adjust the system time:
**   - pushing left or right respectively increases and decreases the minute setting;
**   - pushing upwards or downwards respectively increases and decreases the hour setting.
**
** claude le bouton S1 sert à régler le contraste et le bouton S2 permet lors de l'utilisation 
** de la version soft de régler le niveau 
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
** ZottaOS Settings To Do in ZottaOS_5xx.h (See page 73 of ZottaOS_User_Manual.pdf)
** ---------------------------------------
** (1) Symbolic constant MSPSERIES set to MSP430F5xx
** (2) Symbolic constant SCHEDULER_REAL_TIME_MODE set to DEADLINE_MONOTONIC_SCHEDULING
** (3) Symbolic constant POWER_MANAGEMENT can be set to NONE or to DM_SLACK
**     IMPORTANT: Because of bugs CPU17 and FLASH28 related to MSP430F543X (documented in
**                SLAZxxx dated 5/20/2008), power management mode DM_SLACK may reset or
**                stall.
**
** Tested with Code Composer Essentials Version: 3.2.2.1.8
** Project Properties
** ------------------
** (1) C/C++ Build: Tool Settings: MSP430 Comipler v3.0: Runtime Model Options: 
**     Silicon version: msp
** (2) TI Build Settings: Runtime Support Library: rts430.lib
** (3) C/C++ Build: Tool Settings: MSP430 Linker v3.0: Runtime Environment: Set C system stack size: 0xC00
** Specified in file lnk_xms430f5438_ZottaOS.cmd
** (1) Main C stack size: 0x400
*/

#include "../../../ZottaOS.h"
#include "../../UART.h"
#include "string.h"
#include "LCD_EXP430F5438/hal_lcd_ZottaOS.h"

// claude
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
#define LCD_TIMEOUT_RESET 0x00 // Reset value used to turnoff the LCD display
#define LCD_WAKEUP        0xFE // LCD in wake-up mode
#define LCD_SLEEP         0xFF // LCD in sleep mode
static UINT8 LCDState = LCD_TIMEOUT_RESET; // Current state of the LCD
static BOOL LcdLock = FALSE;               // Lock access to the LCD
static UINT8 ContrastLevel = 0;            // LCD current contrast LCD level

static BOOL CalibrationRequest = TRUE;     // claude: Si vrais mis à jour des valeur de calibration (offset)    
typedef struct ADC12DataDef {
   INT32 Temperature;
   INT32 Vcc;
}ADC12DataDef;
static void *ADC12Buffer;

#define LOAD_TIMER_CLOCK_FREQUENCY 1500000 // SMCLK / 8 -> 12MHz / 8 = 1.5MHz 
#define PERIODE_DRAW_LOAD_TASK 60000       // Load calculation task period
static UINT32 LoadTimerHigh = 0; 
#if defined(ZOTTAOS_VERSION_SOFT)
   static UINT8 DummyLoadSelection = 0;
#endif  

typedef struct BallParametersDef {
   INT16 x;
   INT16 y;
   INT16 radius;
   INT16 oldx;
   INT16 oldy;
   struct BallParametersDef *next;
   float speed;
   float ballSpeedX;
   float ballSpeedY;
} BallParametersDef;
static BallParametersDef *BallQueue = NULL;


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
static BallParametersDef *CreateBallParameters(INT16 x, INT16 y, INT16 radius, float speed);
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
     OSCreateTask(AdcStartReadTask,0,2000,2000,NULL);
     OSCreateTask(DrawBallTask,0,3000,3000,CreateBallParameters(30,30,5,0.89));
     OSCreateTask(DrawBallTask,0,3000,3000,CreateBallParameters(50,50,5,0.91));
     OSCreateTask(DrawBallTask,0,3000,3000,CreateBallParameters(70,70,7,0.93));
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
     OSCreateTask(AdcStartReadTask,1,0,2000,2000,1,1,0,NULL);
     OSCreateTask(DrawBallTask,800,0,3000,3000,1,10,0,CreateBallParameters(30,30,5,0.89));
     OSCreateTask(DrawBallTask,800,0,3000,3000,1,10,0,CreateBallParameters(50,50,5,0.91));
     OSCreateTask(DrawBallTask,800,0,3000,3000,1,1,0,CreateBallParameters(70,70,7,0.93));
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


/* CreateBall: Create ball parameters structur and return it. */
BallParametersDef *CreateBallParameters(INT16 x, INT16 y, INT16 radius, float speed)
{  
  BallParametersDef *BallParameters;
  BallParameters = (BallParametersDef *)OSMalloc(sizeof(BallParametersDef)); 
  BallParameters->x = BallParameters->oldx = x;
  BallParameters->y = BallParameters->oldy = y;
  BallParameters->ballSpeedX = 0;
  BallParameters->ballSpeedY = 0;
  BallParameters->speed = speed;
  BallParameters->radius = radius;
  BallParameters->next = BallQueue; 
  BallQueue = BallParameters;
  return BallParameters;
} /* end of CreateBall */


/* InitLoadTimer: Initializes the timer TB0 for cpu load calculation. */
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
  /* P3 */
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
  /* P7 */
  P7OUT = 0x0;  
  P7DIR = 0xFF;
  P7SEL = 0x0003;  
  /* P8 */
  P8OUT = 0x0;  
  P8DIR = 0xFF;
  P8SEL = 0x0003;  
  /* P9 */
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
  ADC12MCTL0 = ADC12INCH_1; // Set acceleromater X channel
  ADC12MCTL1 = ADC12INCH_2; // Set acceleromater Y channel
  ADC12MCTL2 = ADC12SREF_1 + ADC12INCH_10; // Set temperature channel
  ADC12MCTL3 = ADC12SREF_1 + ADC12INCH_11 + ADC12EOS; // Set Vcc channel
  Wait(0xFFFF); // Wait for reference stabilization
  /* Initialize intertask communication with an asynchronous 3-slot buffer. */
  ADC12Buffer = OSInitBuffer(sizeof(ADC12DataDef),OS_BUFFER_TYPE_3_SLOT,NULL);
  /* Register the interrupt handler for the ADC. */
  ADC12Descriptor = (ADC12DescriptorDef *)OSMalloc(sizeof(ADC12DescriptorDef));
  ADC12Descriptor->ADC12InterruptHandler = ADC12Interrupt;
  OSSetISRDescriptor(OS_IO_ADC12_ADM3,ADC12Descriptor);
} /* end of InitADC12 */


/* InitLCD: Initialize LCD and backlight. */
void InitLCD(void)
{
  BallParametersDef *aBall;
  halLcdInit();
  halLcdBackLightInit();
  halLcdSetBackLight(16); // 0 = off, 16 = max
  halLcdClearScreen(); 
  #if defined(ZOTTAOS_VERSION_HARD)
     halLcdPrintLine("ZottaOSHard      ",0,INVERT_TEXT);
  #elif defined(ZOTTAOS_VERSION_SOFT)
     halLcdPrintLine("ZottaOSSoft      ",0,INVERT_TEXT);
  #else
     #error Wrong kernel version
  #endif  
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
  while ((P2IN & 0x08) != 0x00) {
     if ((P2IN & 0x40) == 0x0) {
        ContrastLevel++;
        if (ContrastLevel > 7)
           ContrastLevel = 0;
        AdjustContrast(ContrastLevel);
        Wait(0x50000); 
     }
  }
  halLcdPrintLine("                 ",1,OVERWRITE_TEXT);
  halLcdPrintLine("                 ",2,OVERWRITE_TEXT);
  halLcdPrintLine("                 ",3,OVERWRITE_TEXT);
  halLcdPrintLine("                 ",4,OVERWRITE_TEXT);
  halLcdPrintLine("                 ",5,OVERWRITE_TEXT);
  halLcdPrintLine("                 ",6,OVERWRITE_TEXT);
  halLcdPrintLine("                 ",8,INVERT_TEXT);

  aBall = BallQueue; 
  while (aBall != NULL) {
     halLcdCircle(aBall->x,aBall->y,aBall->radius,PIXEL_ON); 
     halLcdCircle(aBall->x,aBall->y,aBall->radius - 1,PIXEL_ON); 
     aBall = aBall->next;
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
        break;
     default:
        break;
  }
} /* end of AdjustContrast */


/* DrawClock: Function whose purpose is to display the time at the bottom right of the
** LCD display.*/
void DrawClock(void)
{
  char clockStr[6];
  char *clockStrUART;
  UINT8 tmp;
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
  // Display the new time value on the LCD
  halLcdPrintLineCol(clockStr,0,12,OVERWRITE_TEXT + INVERT_TEXT);
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
  P1OUT ^= 0x02;
	
  if (!LcdLock) {
     LcdLock = TRUE;
     halLcdInit();
     halLcdInit();
     halLcdSetBackLight(16); // 0 = off, 16 = max
     LCDState = LCD_TIMEOUT_RESET;
     LcdLock = FALSE;
     OSScheduleSuspendedTask(DrawClockEvent);
  }
  P1OUT ^= 0x02;
  
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
  if (!LcdLock) {
     LcdLock = TRUE;
     if (LCDState == LCD_SLEEP) {
        OSScheduleSuspendedTask(LCDWakeupEvent);
        LCDState = LCD_WAKEUP;
     }
     else
       LCDState = LCD_TIMEOUT_RESET; // Reset the LCD turn-off counter
     LcdLock = FALSE;
  }
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
  if (!LcdLock) {
     LcdLock = TRUE;
     if (LCDState == LCD_SLEEP) {
        OSScheduleSuspendedTask(LCDWakeupEvent);
        LCDState = LCD_WAKEUP;
     }
     else
       LCDState = LCD_TIMEOUT_RESET; // Reset the LCD turn-off counter
     LcdLock = FALSE;
  }
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
  if (!LcdLock) {
     LcdLock = TRUE;
     if (LCDState == LCD_SLEEP) {
        OSScheduleSuspendedTask(LCDWakeupEvent);
        LCDState = LCD_WAKEUP;
     }
     else
       LCDState = LCD_TIMEOUT_RESET; // Reset the LCD turn-off counter
     LcdLock = FALSE;
  }
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
  if (!LcdLock) {
     LcdLock = TRUE;
     if (LCDState == LCD_SLEEP) { 
        OSScheduleSuspendedTask(LCDWakeupEvent);
        LCDState = LCD_WAKEUP;
     }
     else
       LCDState = LCD_TIMEOUT_RESET; // Reset the LCD turn-off counter
     LcdLock = FALSE;
  }
  OSSuspendSynchronousTask();
} /* end of ButtonLeftTask */


/* ButtonSelectTask: . */
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
  if (!LcdLock) {
     LcdLock = TRUE;
     if (LCDState == LCD_SLEEP) { 
        OSScheduleSuspendedTask(LCDWakeupEvent);
        LCDState = LCD_WAKEUP;
     }
     else
       LCDState = LCD_TIMEOUT_RESET; // Reset the LCD turn-off counter
     LcdLock = FALSE;
  }
  OSSuspendSynchronousTask();
} /* end of ButtonSelectTask */


/* ButtonS1Task: . */
void ButtonS1Task(void *argument)
{
  static BOOL first = TRUE;
  if (first || (P2IN & 0x40) == 0x00) {
     ContrastLevel++;
     if (ContrastLevel > 7)
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
  if (!LcdLock) {
     LcdLock = TRUE;
     if (LCDState == LCD_SLEEP) { 
        OSScheduleSuspendedTask(LCDWakeupEvent);
        LCDState = LCD_WAKEUP;
     }
     else
       LCDState = LCD_TIMEOUT_RESET; // Reset the LCD turn-off counter
     LcdLock = FALSE;
  }
  AdjustContrast(ContrastLevel);
  OSSuspendSynchronousTask();
} /* end of ButtonS1Task */


/* ButtonS2Task: . */
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
  if (!LcdLock) {
     LcdLock = TRUE;
     if (LCDState == LCD_SLEEP) { 
        OSScheduleSuspendedTask(LCDWakeupEvent);
        LCDState = LCD_WAKEUP;
     }
     else
       LCDState = LCD_TIMEOUT_RESET; // Reset the LCD turn-off counter
     LcdLock = FALSE;
  }
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
  ADC12DataDef data;
  char str[5];
  if (!LcdLock && LCDState != LCD_WAKEUP && LCDState != LCD_SLEEP) {
     while (OSGetCopyBuffer(ADC12Buffer,OS_READ_MULTIPLE,(UINT8 *)&data) !=
                                                                   sizeof(ADC12DataDef));
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
  ADC12DataDef data;
  UINT16 vcc;
  char str[6];
  if (!LcdLock && LCDState != LCD_WAKEUP && LCDState != LCD_SLEEP) {
     while (OSGetCopyBuffer(ADC12Buffer,OS_READ_MULTIPLE,(UINT8 *)&data) !=
                                                                   sizeof(ADC12DataDef));
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


/* DrawLoadTask: . */
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
   /* DummyWorkTask: . */
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
     }
     if (!LcdLock && LCDState != LCD_WAKEUP && LCDState != LCD_SLEEP) {
        LcdLock = TRUE;
        halLcdPrintLineCol(tmp,8,14,OVERWRITE_TEXT + INVERT_TEXT);
        LcdLock = FALSE;
     }
     OSEndTask();
   } /* End of DummyWorkTask */
#endif

/* DrawClockTask: Aperiodic task whose purpose is to display the time at the bottom
** right of the LCD display. This task can either be scheduled by RTCInterrupt or when
** the user takes an action that sets the time. */
void DrawClockTask(void *argument)
{
  if (!LcdLock && LCDState != LCD_WAKEUP && LCDState != LCD_SLEEP) {
     LcdLock = TRUE;
     LCDState = LCD_TIMEOUT_RESET;
     DrawClock();
     LcdLock = FALSE;
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


/* DrawBallTask: Periodic task that updates the displayed movement of the bouncing
** ball. */
void DrawBallTask(void *argument)
{
  BallParametersDef *aBall = (BallParametersDef *)argument;
  if ((aBall->oldx != aBall->x || aBall->oldy != aBall->y) && !LcdLock) {
     if (LCDState == LCD_SLEEP) { // Is the display turned off?
        OSScheduleSuspendedTask(LCDWakeupEvent); 
        LCDState = LCD_WAKEUP;
     }
     else if (LCDState != LCD_WAKEUP) { // Is the display turned off?
        LCDState = LCD_TIMEOUT_RESET; // Reset the LCD turn-off counter
        LcdLock = TRUE;
        /* Erase the ball */
        halLcdCircle(aBall->oldx,aBall->oldy,aBall->radius,PIXEL_OFF); 
        halLcdCircle(aBall->oldx,aBall->oldy,aBall->radius - 1,PIXEL_OFF);
        aBall->oldx = aBall->x; 
        aBall->oldy = aBall->y;
        /* Display all balls */
        aBall = BallQueue; 
        while (aBall != NULL) {
           halLcdCircle(aBall->oldx,aBall->oldy,aBall->radius,PIXEL_ON); 
           halLcdCircle(aBall->oldx,aBall->oldy,aBall->radius - 1,PIXEL_ON);
           aBall = aBall->next;
        } 
        LcdLock = FALSE;
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


/* Interrupt Handler for ACD12*/
void ADC12Interrupt(ADC12DescriptorDef *descriptor)
{
  ADC12DataDef data;
  BallParametersDef *aBall;
  INT16 dx;
  INT16 dy;
  dx = ADC12MEM0;
  dy = ADC12MEM1;
  /* Update calibration values */
  if (CalibrationRequest) {
     descriptor->CalibrationOffsetX = dx;
     descriptor->CalibrationOffsetY = dy;
     CalibrationRequest = FALSE;
  }
  dx = (dx - descriptor->CalibrationOffsetX) / 35;
  dy = (dy - descriptor->CalibrationOffsetY) / 35;
  aBall = BallQueue; 
  while (aBall != NULL) {
     aBall->ballSpeedX -= dx;  // Update the speed of this ball
     aBall->ballSpeedY += dy;
     aBall->ballSpeedX *= aBall->speed; // Simulate some resistance and exponentially reduce the speed
     aBall->ballSpeedY *= aBall->speed;
     if (aBall->ballSpeedX < 1.0 && aBall->ballSpeedX > -1.0) // Prevents continuous movements
        aBall->ballSpeedX = 0.0;
     if (aBall->ballSpeedY < 1.0 && aBall->ballSpeedY > -1.0)
        aBall->ballSpeedY = 0.0;
     aBall->x += aBall->ballSpeedX; // Update the ball's position
     aBall->y += aBall->ballSpeedY;
     if (aBall->x < aBall->radius) { // Detect a left border contact
        aBall->ballSpeedX = -aBall->ballSpeedX;
        aBall->x = aBall->radius;
     }
     if (aBall->x >= (LCD_COL - (3 + aBall->radius))) { // Detect a right border contact
        aBall->ballSpeedX = -aBall->ballSpeedX;
        aBall->x = LCD_COL - (3 + aBall->radius);
     }
     if (aBall->y < 12 + aBall->radius) { // Detect a top border contact
        aBall->ballSpeedY = -aBall->ballSpeedY;
        aBall->y = 12 + aBall->radius; 
     }
     if (aBall->y >= (LCD_ROW - (15 + aBall->radius))) { // Detect a bottom border contact
        aBall->ballSpeedY = -aBall->ballSpeedY;
        aBall->y = LCD_ROW - (15 + aBall->radius);
     }
     aBall = aBall->next;
  }
  // Convert VCC
  data.Vcc = ADC12MEM3; 
  data.Vcc = data.Vcc * 50 / 4096; 
  // Convert Temperature
  // ((A10/4096*2500mV) - 680mV)*(1/2.25mV) = (A10/4096*1111) - 302
  // = (A10 - 1113) * (666 / 4096)
  data.Temperature = ADC12MEM2;
  data.Temperature = ((data.Temperature - 1113) * 666) / 4096;
  OSWriteBuffer(ADC12Buffer,(UINT8 *)&data,sizeof(ADC12DataDef));
  ADC12IFG = 0;
  ADC12CTL0 &= ~( ADC12ENC | ADC12SC );    
} /* end of ADC12Interrupt */
