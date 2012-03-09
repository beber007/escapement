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
** Requires TI LCD HAL file modified for ZottaOS and either ZottaOS-Hard or ZottaOS-Soft
** Authors: MIS-TIC */

/* The purpose of this program is to demonstrate most of the features available in
** ZottaOS with a non-trivial application having periodic and event-driven tasks, multi-
** ple sources of interruptions and a RS232 connection with a PC.
** The application runs on TI evaluation kit MSP-EXP430F5438, which namely has an LCD
** display, accelerometers, a trackpoint button (joystick) allowing navigation, and two
** pushbuttons referred to as S1 and S2.
**
** How to Compile This Application
** -------------------------------
** 1. Run ZottaOSconf.exe and open the configuration file Balls_Demo_EXP430F5438.zot at
**    step 1. This file holds all peripheral interrupts and settings for the system clock
**    module (UCS), and which will create function OSInitializeSystemClocks().
**    At step 2, select the kernel (ZottaOS-Hard or ZottaOS-Soft)
**    Go to 7 and generate files
**        ZottaOS_msp430f5419A_36A_38A.h
**    and
**        ZottaOS_msp430f5419A_36A_38A.asm
** 2. If you selected ZottaOS-Hard, edit file ZottaOSHard.h and set the symbolic constant
**    SCHEDULER_REAL_TIME_MODE to DEADLINE_MONOTONIC_SCHEDULING.
**
** What This Application Does
** --------------------------
** When turned on, the application first displays a summary of the available commands to
** set the time and to adjust the contrast. The application begins as soon as the track-
** point button is pressed and displays:
**            +------------------------+-----------------+
** line 1:    |    Scrolling Banner    |   System Time   |
**            +------------------------+-----------------+
** lines 2-8: |                                          |
**            +-------------+-----+----------+-----------+
** line 9:    | Temperature | Vcc | CPU load | Bkgd Task |
**            +-------------+-----+----------+-----------+
** The first line displays a scrolling banner with at most 11 shown characters giving the
** version of the ZottaOS kernel ("ZottaOSHard Demo" or "ZottaOSSoft Demo"). The top line
** ends with the current time shown in hours and minutes. Once the application is start-
** ed, this time can be set using the trackpoint positional buttons.
** The next 7 lines show 5 black balls that bounce against themselves and off the bounda-
** ries following the physical orientation of the evaluation kit. The accelerometers mea-
** sure the movement of the evaluation kit; these measurements make it possible to move
** the ball displayed on the LCD.
** The bottom line of the LCD displays the ambient temperature, the supply voltage (VCC)
** applied to the microcontroller, the current processor load, and for ZottaOS-Soft, the
** load of a synthetic background task whose purpose is to decrease the available time
** left to scroll the banner and to display the balls.
** The peak CPU utilization is typically between 80 and 85%, and when the background task
** is applied with a high workload, only mandatory instances of the scrolling banner and
** the ball drawings are scheduled, Hence the number of screen drawing instances vary but
** their minimal number are evenly spaced. However the ball positions, having a hard real
** time constraints, are always computed and up to date. Now because the ball positional
** computation time depends on the number of collisions between the balls that occur,
** this time is variable, and when there are few, optional instances of the scrolling
** banner and ball drawings can be scheduled, showing more frequent refreshes.
** The point to note is that the screen drawing instances are abandoned in a controlled
** matter in order to accommodate the high CPU utilization.
** Finally, the LCD is turned off after an inactivity of at least 10 seconds to make ad-
** ditional energy savings. As soon as there is an interrupt (button input or a detection
** of movement), the LCD is turned back on.
**
** Trackpoint Button Functions
** ---------------------------
** The trackpoint allows the user to adjust the system time:
**  - pushing left or right respectively decreases and increases the minute setting;
**  - pushing upwards or downwards respectively increases and decreases the hour setting.
** The reference point of gravity is taken when pressing on the trackpoint selection
** button. This can be done while the application is displaying the command summary (1st
** screen) or any time while the balls are drawn. (2nd screen).
** Pressing the trackpoint selection also displays the 2nd screen, and the only way to
** redraw the 1st screen is to reset the MSP-EXP430F5438 board.
**
** Pushbutton S1 and S2
** --------------------
** These are located under the LCD (when faced upwards) with S1 being on the left of S2.
** S1 adjusts the screen contrast by cyclically stepping through higher contrasts until a
** reasonable limit is reached and then restarts with its minimum value.
** S2 is only functional with ZottaOS-Soft and cyclically steps through higher workloads
** of the background task. Once the maximum load of 60% is reached, the load restarts at
** 0%.
**
** Addition Information
** --------------------
** The ambient temperature and the supply voltage applied to the microcontroller are pe-
** riodically measured via ADC12 and displayed at the bottom left part of the LCD.
**
** The CPU load is measured with Timer0_B by having it count only when the processor is
** active and then regularly displaying the ratio between the counter and the displayed
** period. In order to do this, the input of port pin P4.7, which is also the external
** source of Timer0_B and accessible from the port pin connector pad, must be connected
** to the output of P11.2, which is the SMCLK test point.
**              MSP430
**         -----------------
**        |            P11.2|--------o SMCLK Test point
**        |                 |        |
**        |                 |        |
**        |             P4.7|--------o P4.7 on PORT connector
**        |                 |
**
** It is also possible to connect the evaluation kit to a PC through a 9600 Baud USB con-
** nector. This additional functionality uses the UART API of ZottaOS. The user can then
** remotely employ the keys 'a', 's', 'w', and 'y' to emulate the 4 directions of the
** trackpoint button and set the system time. The system time is also sent to the PC con-
** sole each time the system time changes.
**
** Structure of This File
** ----------------------
** The application is contained into a single file divided into sections that deal with
** a specific peripheral or functionality. This are:
**  1. Global definitions, variables and events referenced out of their section;
**  2. The entry point to a section, typically a function that initializes a section;
**  3. The main function with the initializations of the GPIOs;
**  4. The top-left scrolling banner functions and task;
**  5. The LCD initialization and its tasks to alter its mode;
**  6. The RTC and its tasks to periodically display the time and to modify its setting;
**  7. The trackpoint button with its ISRs to modify the time setting and LCD contrast;
**  8. The UART initialization and its ISR to remotely modify the time setting;
**  9. The timer ISR used to measure the processor load and the task that periodically
**     displays its value.
**  10. The ADC initialization and its ISR which measures the accelerometers, Vcc and
**      the current temperature, as well as the tasks that individually displays these
**      last 2 quantities.
**  11. The LCD contrast with its ISR bound to S1 and the task that modifies its value.
**  12. The synthetic task that wastes CPU cycles to show the benefit of ZottaOS-Soft
**      along with the ISR bound to S2 that alters its load.
**  13. And finally the functions that compute the ball positions and tasks that indivi-
**      dually displays them.
**
** Tasks Contained in This Application (Claude/Beber: a mettre a jour)
** -----------------------------------
** This section lists the tasks in the order of the execution priority.
**   Task Identifier           WCET   Period    m   k        Notes
**   BannerTask                 190     3000    1   8
**   LCDWakeupTask             2000   180000
**   LCDSleepTask              1400   180000    1   1
**   DrawClockTask              200    18000
**   DecrementHourTask           2     18000
**   IncrementHourTask           2     18000
**   IncrementMinuteTask         2     18000
**   DecrementMinuteTask         2     18000
**   TrackpointButtonSelectTask  2     18000
**   UartReceiveTask             1     18000
**   DrawCPULoadTask           600     60000    1   1        (deadline = 1999)
**   AdcStartReadTask            1      3000    1   1
**   DrawTemperatureTask      1600    180000    1   1
**   DrawVoltageTask           650     60000    1   1
**   S1ButtonTask                4     18000
**   SyntheticTask            9300     15000    1   1        (only with ZottaOS-Soft)
**   S2ButtonTask                4     18000
**   DrawBallTask             1400      3000    1   1        (5 instances)
** Notes: 1. Event-driven tasks are identified by the fact that they have have no (m,k)
**           contraints;
**        2. Containts are given for ZottaOS-Soft but under ZottaOS-Hard, these are equi-
**           valent to (1,1);
**        3. All task deadlines are equal to their period unless indicated. */

#include "../../../ZottaOS.h"
#include "../../UART.h"
#include <string.h>
#include "LCD_EXP430F5438/hal_lcd_ZottaOS.h"

/* Sanity Test */
#if !defined(ZOTTAOS_VERSION_HARD) && !defined(ZOTTAOS_VERSION_SOFT)
    #error Wrong kernel version
#endif


/************************************/
/* Global Definitions and Variables */
/************************************/
/* Global variable LCDState starts at value LCD_TIMEOUT_RESET and is periodically incre-
** mented by task LCDSleepTask until its value reaches 3 at which time LCDSleepTask for-
** ces the LCD to go into standby. All the tasks that draw onto the LCD reset LCDState to
** its initial value. Hence when nothing is redrawn on the screen within 3 consecutive
** executions of LCDSleepTask, the LCD is turned-off. The LCD stays in stand-by until the
** event LCDWakeupEvent is triggered.
** Because we do not want to continuously reactivate the LCD when the time changes for
** example, the trick is to have certain tasks that draw onto the LCD only when it is ac-
** tive and others that force a wake-up (e.g. when a ball changes its coordinates indi-
** cating that the user has also moved the board).
** State LCD_WAKEUP is a transient of the LCD and indicates that a task has triggered
** event LCDWakeupEvent. This somewhat limits a burst of wake-up demands. */
#define LCD_TIMEOUT_RESET  0x00 // Reset value used to turn off the LCD display
#define LCD_WAKEUP         0xFE // LCD in wake-up mode
#define LCD_SLEEP          0xFF // LCD in standby mode
static UINT8 LCDState = LCD_TIMEOUT_RESET; // Current LCD state

/* Writing onto the LCD typically requires sending a stream of commands to the LCD con-
** troller, where each command cannot be intervene with others. Rather than serializing
** these commands through a fifo queue processed by the LCD controller, we choose to lock
** access to the LCD whenever a write is in progress. */
static volatile BOOL LCDLock = FALSE;      // Lock access to the LCD


/*************************************************/
/* Global Events That Trigger Event-Driven Tasks */
/*************************************************/
/* These are events associated to tasks that are triggered by other tasks. */
/* Event LCDWakeupEvent schedules task LCDWakeupTask to reset the LCD and bring it out of
** its standby state. This task is started whenever the LCD is in standby and the user
** creates an interrupt with the trackpoint button or with pushbuttons S1 and S2, or
** moves the board. */
static void *LCDWakeupEvent;

/* Event DrawClockEvent schedules task DrawClockTask to redraw the current time at the
** top-left of the LCD. This task is started whenever the user takes an action that modi-
** fies the current time setting. Otherwise, DrawClockTask is scheduled only when the RTC
** schedules it every minute. */
static void *DrawClockEvent;


/******************************************/
/* Entry Points to the Different Sections */
/*****************************************/
static void Wait(UINT32 wait);
static void InitIOPorts(void);
static void InitBanner(void);
static void InitLCD(void);
static void InitRTC(void);
static void InitTrackpointButton(BOOL *requestADCCalibration);
static void InitUART(void);
static void InitCPULoadTask(void);
static BOOL *InitADC12(void);
static void InitContrastButton(void);
static void SetLCDContrastLevel(BOOL increment);
static void InitSyntheticBackgroundTask(void);
static void InitBalls(void);
static void SetInitialBallVelocities(INT16 dx, INT16 dy);
static void SetNewBallCoordinates(INT16 dx, INT16 dy);


int main(void)
{
  WDTCTL = WDTPW + WDTHOLD;   // Disable watchdog timer
  OSInitializeSystemClocks(); // This function is created by ZottaOSconf.exe

  __asm("\teint"); // Enable interrupts

  /* Initialize all hardware peripherals and their associated tasks */
  InitIOPorts();
  InitLCD();     // Displays the commands and waits until the user asks for the 2nd page.
  InitBanner();
  InitRTC();     // Also draws the initial current time
  InitContrastButton();
  InitUART();
  Wait(0x40000); // Wait until all bytes are sent by the UART
  InitCPULoadTask();
  InitSyntheticBackgroundTask();
  InitBalls();   // Also draws the initial positions of the balls
  InitTrackpointButton(InitADC12());
  /* Start the OS so that it starts scheduling the application tasks */
  return OSStartMultitasking();
} /* end of main */


/* Wait: Actively waits for a duration equal to the parameter. This function is called
** prior to OSStartMultitasking to temporize until the menus are sent so that their pro-
** cessing does not become part of the WCET of the tasks. */
void Wait(UINT32 wait)
{
  volatile UINT32 i;
  for (i = 0; i < wait; i++);
} /* end of Wait */


/* InitIOPorts: Initializes all IO ports. */
void InitIOPorts(void)
{
  /* P1.0 and P1.1 Leds */
  #if defined(ZOTTAOS_VERSION_SOFT)

  #else
     P1OUT = 0x0;
     P1DIR = 0xFF;
     P1SEL = 0x0;
  #endif
  /* P2.1 to P2.5 Trackpoint with selector
  ** P2.6 Pushbutton S1
  ** P2.7 Pushbutton S2 */
  P2OUT |= 0xFE;
  P2DIR &= ~0xFE;
  P2REN |= 0xFE;
  P2SEL &= ~0xFE;
  /* P3 not use */
  P3OUT = 0x0;
  P3DIR = 0xFF;
  P3SEL = 0x0;
  /* P4.7 input clock for Timer0_B (for load calculation) */
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
  ** P6.1 and P6.2 ADC12 input (two-axis accelerometer) */
  P6OUT = 0x41;
  P6DIR = 0xF9;
  P6SEL = 0x06;
  /* P7 not use */
  P7OUT = 0x0;
  P7DIR = 0xFF;
  P7SEL = 0x0003;     // beber? n'est-ce pas 0
  /* P8 not use */
  P8OUT = 0x0;
  P8DIR = 0xFF;
  P8SEL = 0x0003;     // beber? n'est-ce pas 0
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


/* Scrolling Banner Functions ---------------------------------------------------------*/
/* When the application is started (1st page showing the available commands), the 1st
** line of the screen only shows the kernel version of ZottaOS. Once the user switches to
** the 2nd page, the current time with 5 characters is added to the right:
**      Column number:    01234567890123456
**      Displayed text:   ZottaOSHard 00:00
** To display a text longer than 11 characters we need to scroll this text. Because the
** TI implementation uses an auxiliary buffer holding everything that is displayed, we
** can store the initial text into that buffer and then scroll the 11 characters contain-
** ed in the LCD RAM memory padding the missing characters with characters taken from
** another table. The text is therefore decomposed into 2 parts:
**   - BANNER_STRING: 1st part of the text stored in the auxilary buffer and which is
**                    initially displayed by function InitLCD().
**   - BANNER_HIDDEN_STRING: remaining part of the text that starts at column 11 and that
**                    are used to complete the displayed area that is uncovered when
**                    scrolling.
*/ 
#if defined(ZOTTAOS_VERSION_HARD)
   #define BANNER_STRING  "ZottaOSHard      " /* 1st part of the text */
#elif defined(ZOTTAOS_VERSION_SOFT)
   #define BANNER_STRING  "ZottaOSSoft      "
#endif
#define BANNER_HIDDEN_STRING  " Demo "        /* remaining part */
#define BANNER_HIDDEN_STRING_SIZE  6          /* Number of remaining chars to consider */

static unsigned int HiddenString[BANNER_HIDDEN_STRING_SIZE][FONT_HEIGHT];
static void BannerTask(void *argument);

/* InitBanner: Prepares the displayed banner and the periodic task that scrolls it. */
void InitBanner(void)
{
  UINT8 i, j;
  char *tmp = BANNER_HIDDEN_STRING;
  /* Prepare the banner with its hidden characters */
  for (j = 0; j < BANNER_HIDDEN_STRING_SIZE; j++)
     for (i = 0; i < FONT_HEIGHT; i++)
        HiddenString[j][i] = 0xFFFF - fonts[fonts_lookup[tmp[j]] * (FONT_HEIGHT + 1) + i];
  #if defined(ZOTTAOS_VERSION_HARD)
     OSCreateTask(BannerTask,0,3000,3000,NULL);
  #elif defined(ZOTTAOS_VERSION_SOFT)
     OSCreateTask(BannerTask,190,0,3000,3000,1,8,0,NULL);
  #endif
} /* end of InitBanner */

/* BannerTask: Periodic task that scrolls the first 11 characters on the 1st line of the
** screen, wrapping around the whole text that must be displayed. */
void BannerTask(void *argument)
{
  #if defined(ZOTTAOS_VERSION_SOFT)
     static UINT8 previousInstance = 7;
     UINT8 currentInstance;
     INT8 nbInstance;
  #endif
  if (!LCDLock && LCDState != LCD_WAKEUP && LCDState != LCD_SLEEP) {
     LCDLock = TRUE;
     #if defined(ZOTTAOS_VERSION_SOFT)
        currentInstance = OSGetTaskInstance();
        nbInstance = currentInstance - previousInstance;
        if (nbInstance == 0)
           nbInstance = 8;
        else if (nbInstance < 0)
           nbInstance += 8;
        halLcdScrollLine(0,nbInstance,10,HiddenString,BANNER_HIDDEN_STRING_SIZE);
        previousInstance = currentInstance;
     #else
        halLcdScrollLine(0,1,10,HiddenString,BANNER_HIDDEN_STRING_SIZE);
     #endif
     LCDLock = FALSE;
  }
  OSEndTask();
} /* end of BannerTask */
/* End of Scrolling Banner Functions Section ------------------------------------------*/


/* LCD --------------------------------------------------------------------------------*/
/* This section deals with the initial phase of the application and creates the tasks
** that puts the LCD into standby mode and then allows it to wake-up when requested.
 */

static void LCDSleepTask(void *argument);
static void LCDWakeupTask(void *argument);

/* InitLCD: Initializes the LCD and its backlight, and then displays the 1st screen with
** the available commands before clearing the screen so that the various tasks of the
** application can draw their contents. */
void InitLCD(void)
{
  halLcdInit();
  halLcdBackLightInit();
  halLcdSetBackLight(16); // 0 = off, 16 = max
  halLcdClearScreen();

  LCDWakeupEvent = OSCreateEventDescriptor();
  #if defined(ZOTTAOS_VERSION_HARD)
     OSCreateSynchronousTask(LCDWakeupTask,180000,LCDWakeupEvent,NULL);
     OSCreateTask(LCDSleepTask,0,180000,180000,NULL);
  #elif defined(ZOTTAOS_VERSION_SOFT)
     OSCreateSynchronousTask(LCDWakeupTask,2000,180000,LCDWakeupEvent,NULL);
     OSCreateTask(LCDSleepTask,1400,0,180000,180000,1,1,0,NULL);
  #endif

  halLcdPrintLine(BANNER_STRING,0,INVERT_TEXT);
  halLcdPrintLine("Joy. left  hour -",1,0);
  halLcdPrintLine("Joy. right hour +",2,0);
  halLcdPrintLine("Joy. up    min. +",3,0);
  halLcdPrintLine("Joy. down  min. -",4,0);
  halLcdPrintLine("S1 adj. contrast ",5,0);
  #if defined(ZOTTAOS_VERSION_SOFT)
     halLcdPrintLine("S2 adj. load     ",6,0);
  #endif
  halLcdPrintLine("Press joystick...",8,0);
  SetLCDContrastLevel(FALSE);   // Set to the 1st predefined contrast level
  /* Busy wait until user presses the trackpoint selection button. While waiting, the
  ** user can also set the LCD contrast. */
  while ((P2IN & 0x08) != 0) {
     if ((P2IN & 0x40) == 0) {   // Step through the contrast level if S1 is pressed
        SetLCDContrastLevel(TRUE);
        Wait(0x50000);
     }
  }
  /* Clear the screen by overwriting the unwanted lines with spaces. */
  halLcdPrintLine("                 ",1,OVERWRITE_TEXT);
  halLcdPrintLine("                 ",2,OVERWRITE_TEXT);
  halLcdPrintLine("                 ",3,OVERWRITE_TEXT);
  halLcdPrintLine("                 ",4,OVERWRITE_TEXT);
  halLcdPrintLine("                 ",5,OVERWRITE_TEXT);
  halLcdPrintLine("                 ",6,OVERWRITE_TEXT);
  halLcdPrintLine("                 ",8,INVERT_TEXT);
} /* end of InitLCD */

/* LCDSleepTask: Periodic task to turn off the LCD if nothing was displayed to the LCD
** during 3 consecutive executions of this task. Every time that a task outputs to the
** LCD the task resets the counter. Every time that this task executes, it increments a
** global counter and once this counter reaches the value 2, the LCD is turned off. */
void LCDSleepTask(void *argument)
{
  if (!LCDLock && LCDState != LCD_WAKEUP && LCDState != LCD_SLEEP) {
     LCDLock = TRUE;
     if (LCDState == 2) {
        halLcdSetBackLight(0);
        halLcdStandby();
        LCDState = LCD_SLEEP;
     }
     else
        LCDState++;
     LCDLock = FALSE;
  }
  OSEndTask();
} /* end of LCDSleepTask */

/* LCDWakeupTask: Event-driven task that wakes up the LCD. This task is scheduled every
** time that a task needs to display something on the LCD and notices that the LCD is
** turned off. */
void LCDWakeupTask(void *argument)
{
  if (!LCDLock && (LCDState == LCD_WAKEUP || LCDState == LCD_SLEEP)) {
     LCDLock = TRUE;
     halLcdInit();
     halLcdSetBackLight(16); // 0 = off, 16 = max
     LCDState = LCD_TIMEOUT_RESET;
     LCDLock = FALSE;
     OSScheduleSuspendedTask(DrawClockEvent);
  }
  OSSuspendSynchronousTask();
} /* end of LCDWakeupTask */
/* End of LCD Section -----------------------------------------------------------------*/


/* RTC Interrupts and Time Related Tasks ----------------------------------------------*/
/* This section of the application initializes the RTC which is used to keep track of the
** time and its associated interrupt handler which schedules an event-driven task that
** outputs the time in hours and minutes on the LCD.
** This section also creates event-driven tasks that can be invoked to modify the time
** setting. */

typedef struct RTCDescriptorDef { // RTC interrupt descriptor
  void (*RTCInterruptHandler)(struct RTCDescriptorDef *);
} RTCDescriptorDef;

static void RTCMinuteInterrupt(RTCDescriptorDef *descriptor);
static void DrawClockTask(void *argument);
static void DrawClock(void);
static void *DecrementHourEvent;   // Decrement hours (DecrementHourTask)
static void DecrementHourTask(void *argument);
static void *IncrementHourEvent;   // Increment hours (IncrementHourTask)
static void IncrementHourTask(void *argument);
static void *IncrementMinuteEvent; // Increment minutes (IncrementMinuteTask)
static void IncrementMinuteTask(void *argument);
static void *DecrementMinuteEvent; // Decrement minutes (DecrementMinuteTask)
static void DecrementMinuteTask(void *argument);

/* InitRTC: Initializes the RTC to have an interrupt every minute, and also creates all
** event-driven tasks that are related to the RTC. This includes the event-driven tasks
** that directly modify the RTC time registers and are scheduled when the user sets the
** time with the trackpoint button or remotely by the UART interface. */
void InitRTC(void)
{  
  RTCDescriptorDef *des;
  des = (RTCDescriptorDef *)OSMalloc(sizeof(RTCDescriptorDef));
  des->RTCInterruptHandler = RTCMinuteInterrupt;
  OSSetISRDescriptor(OS_IO_RTC_TEV,des); // RTC timer event interrupt
  // Set to calendar mode with an interrupt when the minutes changes
  RTCCTL01 |= RTCMODE + RTCHOLD + RTCTEVIE;
  RTCHOUR = 0;
  RTCMIN = 0;
  RTCSEC = 0;  
  RTCDAY = 1;            // Don't care value
  RTCMON = 1;            // Don't care value
  RTCYEAR = 2009;        // Don't care value
  RTCCTL01 &= ~RTCHOLD;  // Set RTC operational
  /* Create the tasks that can displays the on the LCD. */  
  DrawClockEvent = OSCreateEventDescriptor();
  #if defined(ZOTTAOS_VERSION_HARD)
     OSCreateSynchronousTask(DrawClockTask,18000,DrawClockEvent,NULL);
  #elif defined(ZOTTAOS_VERSION_SOFT)
     OSCreateSynchronousTask(DrawClockTask,200,18000,DrawClockEvent,NULL);
  #endif
  /* Create the tasks that can modify the time registers of the RTC. */
  DecrementHourEvent = OSCreateEventDescriptor();
  #if defined(ZOTTAOS_VERSION_HARD)
     OSCreateSynchronousTask(DecrementHourTask,18000,DecrementHourEvent,NULL);
  #elif defined(ZOTTAOS_VERSION_SOFT)
     OSCreateSynchronousTask(DecrementHourTask,2,18000,DecrementHourEvent,NULL);
  #endif
  IncrementHourEvent = OSCreateEventDescriptor();
  #if defined(ZOTTAOS_VERSION_HARD)
     OSCreateSynchronousTask(IncrementHourTask,18000,IncrementHourEvent,NULL);
  #elif defined(ZOTTAOS_VERSION_SOFT)
     OSCreateSynchronousTask(IncrementHourTask,2,18000,IncrementHourEvent,NULL);
  #endif
  IncrementMinuteEvent = OSCreateEventDescriptor();
  #if defined(ZOTTAOS_VERSION_HARD)
     OSCreateSynchronousTask(IncrementMinuteTask,18000,IncrementMinuteEvent,NULL);
  #elif defined(ZOTTAOS_VERSION_SOFT)
     OSCreateSynchronousTask(IncrementMinuteTask,2,18000,IncrementMinuteEvent,NULL);
  #endif
  DecrementMinuteEvent = OSCreateEventDescriptor();
  #if defined(ZOTTAOS_VERSION_HARD)
     OSCreateSynchronousTask(DecrementMinuteTask,18000,DecrementMinuteEvent,NULL);
  #elif defined(ZOTTAOS_VERSION_SOFT)
     OSCreateSynchronousTask(DecrementMinuteTask,2,18000,DecrementMinuteEvent,NULL);
  #endif
  DrawClock();
} /* end of InitRTC */

/* RTCMinuteInterrupt: ISR for the RTC to generate an event that updates the display. */
void RTCMinuteInterrupt(RTCDescriptorDef *des)
{
  OSScheduleSuspendedTask(DrawClockEvent); // Repaints the time every minute
  RTCCTL01 |= RTCTEVIE;                    // Re-enable next RTC interrupt
} /* end of RTCMinuteInterrupt */

/* DrawClockTask: Event-driven task whose purpose is to display the time at the top right
** of the LCD display. This task can either be scheduled by RTCInterruptMinute or when
** the user takes an action that modified the time.
** Scheduled by event DrawClockEvent */
void DrawClockTask(void *argument)
{
  if (LCDState != LCD_WAKEUP && LCDState != LCD_SLEEP) {
     LCDState = LCD_TIMEOUT_RESET;
     DrawClock();
  }
  OSSuspendSynchronousTask();
} /* end of DrawClockTask */

/* DrawClock: Function whose purpose is to display the time at the top right of the LCD
** display after the user presses on the trackpoint selection button. This is the main
** function done by task DrawClockTask but which is also directly called when the user
** switches from the 1st screen showing the command summary to the 2nd where the balls
** are drawn. */
void DrawClock(void)
{
  char *clockStrUART, clockStr[6];
  UINT8 tmp;
  if (!LCDLock) {
     /* Convert time into a string of characters */
     clockStr[0] = '0';
     for (tmp = RTCHOUR; tmp >= 10; tmp -= 10)
        clockStr[0]++;
     clockStr[1] = '0' + tmp;
     clockStr[2] = ':';
     clockStr[3] = '0';
     for (tmp = RTCMIN; tmp >= 10; tmp -= 10)
        clockStr[3]++;
     clockStr[4] = '0' + tmp;
     clockStr[5] = '\0';
     /* Display the new time on the LCD */
     LCDLock = TRUE;
     halLcdPrintLineCol(clockStr,0,12,OVERWRITE_TEXT + INVERT_TEXT);
     LCDLock = FALSE;
  }
  // Send the new time value to the the console via the UART
  clockStrUART = (char *)OSGetFreeNodeUART(OS_IO_USCI_A1_TX);
  strcpy(clockStrUART,clockStr);
  strcat(clockStrUART,"\r");
  OSEnqueueUART((UINT8 *)clockStrUART,strlen(clockStrUART),OS_IO_USCI_A1_TX);
} /* end of DrawClock */

/* DecrementHourTask: Event-driven task scheduled by the trackpoint button and UART ISRs
** and whose purpose is to decrement the hours. The task reschedules itself allowing to
** continuously count while the button is pressed, and as soon as the button is released,
** it re-enables the interrupts for this button. The LCD is waked up if necessary. */
void DecrementHourTask(void *argument)
{
  static BOOL first = TRUE;
  if (first || (P2IN & 0x02) == 0) {
     if (RTCHOUR != 0)
        RTCHOUR--;
     else
        RTCHOUR = 23;
     first = FALSE;
     OSScheduleSuspendedTask(DrawClockEvent);
     OSScheduleSuspendedTask(DecrementHourEvent);
  }
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
} /* end of DecrementHourTask */

/* IncrementHourTask: Same as DecrementHourTask but increments the hours. */
void IncrementHourTask(void *argument)
{
  static BOOL first = TRUE;
  if (first || (P2IN & 0x04) == 0) {
     if (RTCHOUR != 23)
        RTCHOUR++;
     else
        RTCHOUR = 0;
     first = FALSE;
     OSScheduleSuspendedTask(DrawClockEvent);
     OSScheduleSuspendedTask(IncrementHourEvent);
  }
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
} /* end of IncrementHourTask */

/* IncrementMinuteTask: Same as DecrementHourTask but increments the minutes. */
void IncrementMinuteTask(void *argument)
{
  static BOOL first = TRUE;
  if (first || (P2IN & 0x10) == 0) {
     if (RTCMIN != 59)
        RTCMIN++;
     else
        RTCMIN = 0;
     first = FALSE;
     OSScheduleSuspendedTask(DrawClockEvent); // Draw the new time
     OSScheduleSuspendedTask(IncrementMinuteEvent);
  }
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
} /* end of IncrementMinuteTask */

/* DecrementMinuteTask: Same as DecrementHourTask but decrements the minutes. */
void DecrementMinuteTask(void *argument)
{
  static BOOL first = TRUE;
  if (first || (P2IN & 0x20) == 0) {
     if (RTCMIN != 0)
        RTCMIN--;
     else
        RTCMIN = 59;
     first = FALSE;
     OSScheduleSuspendedTask(DrawClockEvent);
     OSScheduleSuspendedTask(DecrementMinuteEvent);
  }
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
} /* end of DecrementMinuteTask */
/* End of RTC Interrupts and Time Related Tasks Section -------------------------------*/



/* Trackpoint Button Functions --------------------------------------------------------*/
/* The trackpoint has 5 interrupt capabilities: 4 for the direction (up, down, left, and
** right), and a selector that is pressed. The 4 coordinate positions modify the hour and
** minute settings of the RTC, and the selector forces the ADC12 to take a new reference
** point of gravity. */

typedef struct TrackpointDescriptorDef { // Generic trackpoint button interrupt descriptor
  void (*interruptHandler)(struct TrackpointDescriptorDef *);
} TrackpointDescriptorDef;

static void TrackpointLeftButtonInterrupt(TrackpointDescriptorDef *descriptor);
static void TrackpointRightButtonInterrupt(TrackpointDescriptorDef *descriptor);
static void TrackpointUpButtonInterrupt(TrackpointDescriptorDef *descriptor);
static void TrackpointDownButtonInterrupt(TrackpointDescriptorDef *descriptor);

typedef struct TrackpointSelectorDescriptorDef {    // Selector button descriptor
  void (*interruptHandler)(struct TrackpointSelectorDescriptorDef *);
  void *signaledEvent;
  BOOL *requestADCCalibration;
} TrackpointSelectorDescriptorDef;

static void TrackpointSelectButtonInterrupt(TrackpointSelectorDescriptorDef *selector);
static void TrackpointButtonSelectTask(void *argument);


/* InitTrackpoint: Initialize the trackpoint ISR. */
void InitTrackpointButton(BOOL *requestADCCalibration)
{
  TrackpointDescriptorDef *tpButton;
  TrackpointSelectorDescriptorDef *sel;
  tpButton = (TrackpointDescriptorDef *)OSMalloc(sizeof(TrackpointDescriptorDef));
  tpButton->interruptHandler = TrackpointLeftButtonInterrupt;
  OSSetISRDescriptor(OS_IO_PORT2_1,tpButton);

  tpButton = (TrackpointDescriptorDef *)OSMalloc(sizeof(TrackpointDescriptorDef));
  tpButton->interruptHandler = TrackpointRightButtonInterrupt;
  OSSetISRDescriptor(OS_IO_PORT2_2,tpButton);

  tpButton = (TrackpointDescriptorDef *)OSMalloc(sizeof(TrackpointDescriptorDef));
  tpButton->interruptHandler = TrackpointUpButtonInterrupt;
  OSSetISRDescriptor(OS_IO_PORT2_4,tpButton);

  tpButton = (TrackpointDescriptorDef *)OSMalloc(sizeof(TrackpointDescriptorDef));
  tpButton->interruptHandler = TrackpointDownButtonInterrupt;
  OSSetISRDescriptor(OS_IO_PORT2_5,tpButton);

  sel = (TrackpointSelectorDescriptorDef *)OSMalloc(sizeof(TrackpointSelectorDescriptorDef));
  sel->interruptHandler = TrackpointSelectButtonInterrupt;
  sel->signaledEvent = OSCreateEventDescriptor();
  sel->requestADCCalibration = requestADCCalibration;
  OSSetISRDescriptor(OS_IO_PORT2_3,sel);
  #if defined(ZOTTAOS_VERSION_HARD)
     OSCreateSynchronousTask(TrackpointButtonSelectTask,18000,sel->signaledEvent,sel);
  #elif defined(ZOTTAOS_VERSION_SOFT)
     OSCreateSynchronousTask(TrackpointButtonSelectTask,2,18000,sel->signaledEvent,sel);
  #endif

  P2IFG &= ~0x3E; // Clear interrupts flags
  P2IES |= 0x3E;  // Allow interrupts on descending edges
  P2IE |= 0x3E;   // Enable interrupts
} /* end of InitTrackpointButton */

/* Trackpoint Button Interrupts: ISRs for the trackpoint button and which are invoked
** when one of its 5 positions is pressed. The interrupt corresponding to the pressed
** button is re-enbled at the end of the event-driven task that these routines schedule.
** This trick somewhat avoids the burst created when the user continuously presses on the
** button. */
void TrackpointLeftButtonInterrupt(TrackpointDescriptorDef *descriptor)
{
  OSScheduleSuspendedTask(DecrementHourEvent); // Schedule DecrementHourTask
} /* end of TrackpointLeftButtonInterrupt */

void TrackpointRightButtonInterrupt(TrackpointDescriptorDef *descriptor)
{
  OSScheduleSuspendedTask(IncrementHourEvent); // Schedule IncrementTask
} /* end of TrackpointRightButtonInterrupt */

void TrackpointUpButtonInterrupt(TrackpointDescriptorDef *descriptor)
{
  OSScheduleSuspendedTask(IncrementMinuteEvent); // Schdeule IncrementMinuteTask
} /* end of TrackpointUpButtonInterrupt */

void TrackpointDownButtonInterrupt(TrackpointDescriptorDef *descriptor)
{
  OSScheduleSuspendedTask(DecrementMinuteEvent); // Schedule DecrementMinuteTask
} /* end of TrackpointDownButtonInterrupt */

void TrackpointSelectButtonInterrupt(TrackpointSelectorDescriptorDef *selector)
{
  OSScheduleSuspendedTask(selector->signaledEvent);
} /* end of TrackpointSelectButtonInterrupt */

/* TrackpointButtonSelectTask: Event-driven task that forces the ADC12 to recalibrate its
** reference point of gravity whene the user presses on the trackpoint selector button.
** This task receives the whole ISR descriptor which contains the flag which it sets so
** that the ADC recalibrates its x- and y-axis accelerometers the next time it has a
** measure. */
void TrackpointButtonSelectTask(void *arg)
{
  static BOOL first = TRUE;
  TrackpointSelectorDescriptorDef *selector = (TrackpointSelectorDescriptorDef *)arg;
  if (first || (P2IN & 0x08) == 0) {
     *selector->requestADCCalibration = TRUE;
     first = FALSE;
     OSScheduleSuspendedTask(selector->signaledEvent);
  }
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
} /* end of TrackpointButtonSelectTask */
/* End of Trackpoint Button Functions Section -----------------------------------------*/



/* UART Functions ---------------------------------------------------------------------*/
static BOOL UARTReceiverInterrupt(UINT8 data);
static void UartReceiveTask(void *argument);
static void *UartReceiveEvent; // Starts a new UartReceiveTask instance

/* InitUART: Initializes the UART and then sends the menu with the options that the user
** can remotely send to the evaluation kit. */
void InitUART(void)
{
  char *string;
  UartReceiveEvent = OSCreateEventDescriptor();
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
  #if defined(ZOTTAOS_VERSION_HARD)
     OSCreateSynchronousTask(UartReceiveTask,18000,UartReceiveEvent,NULL);
  #elif defined(ZOTTAOS_VERSION_SOFT)
     OSCreateSynchronousTask(UartReceiveTask,1,18000,UartReceiveEvent,NULL);
  #endif
} /* end of InitUART */

/* UARTReceiverInterrupt: ISR for the receiver part of the UART. The interrupt is re-
** enabled at the end of the event-driven task that this routine schedules. This trick
** avoids the burst created when the user continuously presses on a key. */
BOOL UARTReceiverInterrupt(UINT8 data)
{
  switch (data) {
     case 'a':  case 'A':  // LEFT (decrements hour)
        OSScheduleSuspendedTask(DecrementHourEvent);
        break;
     case 's':  case 'S':  // RIGHT (increments hour)
        OSScheduleSuspendedTask(IncrementHourEvent);
        break;
     case 'w':  case 'W':  // UP (increments minute)
        OSScheduleSuspendedTask(IncrementMinuteEvent);
        break;
     case 'y':  case 'Y':  // DOWN (decrements minute)
        OSScheduleSuspendedTask(DecrementMinuteEvent);
     default:
        break;
  }
  /* Start the event-driven task that will re-enable UART receive interrupts. */
  OSScheduleSuspendedTask(UartReceiveEvent);
  return TRUE;
} /* end of UARTReceiverInterrupt */

/* UartReceiveTask: When a character is received from the UART, a dedicated event-driven
** task is started for every valid command; these tasks do not re-enable UART interrupts.
** Also, when any character is received including those that are valid, this event-driven
** task is signaled by the UART ISR so that it can re-enable the interrupt. This trick
** creates a minimum interarrival period between two consecutive interrupts. */
void UartReceiveTask(void *argument)
{
  UCA1IE |= UCRXIE; // Enable UART interrupt
  OSSuspendSynchronousTask();
} /* end of UartReceiveTask */
/* End of UART Functions Section -----------------------------------------------------*/



/* Processor Load Measurement and Display ---------------------------------------------*/
/* The basic idea behind finding the current CPU load is to use a timer that increments a
** counter when MCLK is also active and not in a sleep mode, i.e. only when the FLL is
** active. This can be done by sourcing a timer from SMCLK also connected to the FLL; but
** doing so stops MCLK but not the oscillator as it also sources a peripheral device. To
** get around this state of affairs, the trick is to connect the output test pin of SMCLK
** to the external source entry of the timer. In this way the system clock module (UCS)
** is fooled and halts the FLL when the processor falls into a sleep mode. */

typedef struct CPULoadTimerDescriptorDef { // Load timer interrupt descriptor
  void (*CPULoadTimerInterruptHandler)(struct CPULoadTimerDescriptorDef *);
  UINT32 timerHigh;   // Upper 16 bits of the CPU load counter
} CPULoadTimerDescriptorDef;

#define CPULOAD_TIMER_CLOCK_FREQUENCY  3125000  // = SMCLK / 8 =  25MHz / 8 = 3.125MHz
#define PERIOD_DRAW_CPULOAD_TASK       60000    // CPU load calculation task period

static CPULoadTimerDescriptorDef *InitCPULoadTimer(void);
static void CPULoadTimerInterrupt(CPULoadTimerDescriptorDef *des);
static void DrawCPULoadTask(void *argument);

/* InitCPULoadTask: Entry point to install Timer0_B overflow interrupts and to create
** the periodic task that outputs the CPU load onto the LCD. */
void InitCPULoadTask(void)
{
  CPULoadTimerDescriptorDef *des = InitCPULoadTimer();
  #if defined(ZOTTAOS_VERSION_HARD)
     OSCreateTask(DrawCPULoadTask,0,PERIOD_DRAW_CPULOAD_TASK,1999,&des->timerHigh);
  #elif defined(ZOTTAOS_VERSION_SOFT)
     OSCreateTask(DrawCPULoadTask,600,0,PERIOD_DRAW_CPULOAD_TASK,1999,1,1,0,&des->timerHigh);
  #endif
} /* end of InitCPULoadTimer */

/* InitCPULoadTimer: Initializes timer Timer0_B for CPU load calculations. */
CPULoadTimerDescriptorDef *InitCPULoadTimer(void)
{
  CPULoadTimerDescriptorDef *loadTimer;
  loadTimer = (CPULoadTimerDescriptorDef *)OSMalloc(sizeof(CPULoadTimerDescriptorDef));
  loadTimer->CPULoadTimerInterruptHandler = CPULoadTimerInterrupt;
  loadTimer->timerHigh = 0;
  OSSetISRDescriptor(OS_IO_TIMER0_B1_TB,loadTimer);
  // Source Timer0_B from its external entry (TB0CLK) divided by 8 and make it count un-
  // til 0xFFFF at which time there is an overflow interrupt.
  TB0CTL |= TBSSEL_0 | TBIE | ID_3 | MC_2; // Start the timer
  return loadTimer;
} /* end of InitCPULoadTimer */

/* CPULoadTimerInterrupt: ISR for overflows of Timer0_B. */
void CPULoadTimerInterrupt(CPULoadTimerDescriptorDef *des)
{
  des->timerHigh++;
  TB0CTL |= TBIE;   // Re-enable Timer0 B for the next overflow interrupt
} /* end of CPULoadTimerInterrupt */

/* DrawCPULoadTask: Periodic task that outputs the current CPU load onto the LCD. The
** load is simply given by the ratio of the timer counter representing the number of
** ticks that the processor is active to the number of ticks composing the task's activa-
** tion period.
** The task receives a pointer to the number of times the timer overflowed. */
void DrawCPULoadTask(void *argument)
{
  const UINT32 period = 57220;
            // = CPULOAD_TIMER_CLOCK_FREQUENCY / 327680 * PERIOD_DRAW_CPULOAD_TASK / 10;
  static UINT32 oldCount = 0;
  char string[4];
  UINT32 currentLoad, currentCount;
  currentCount = *(UINT32 *)argument << 16 | TB0R;
  currentLoad = (currentCount - oldCount) / period;
  oldCount = currentCount; // Save the current count for the next activation
  if (currentLoad < 10) {
     string[0] = ' ';
     string[1] = currentLoad + '0';
  }
  else if (currentLoad < 100) {
     string[0] = currentLoad / 10 + '0';
     string[1] = currentLoad % 10 + '0';
  }
  else {
     string[0] = ' ';
     string[1] = ' ';
  }
  string[2] = '%';
  string[3] = '\0';
  if (!LCDLock && LCDState != LCD_WAKEUP && LCDState != LCD_SLEEP) {
     LCDLock = TRUE;
     halLcdPrintLineCol(string,8,10,OVERWRITE_TEXT + INVERT_TEXT);
     LCDLock = FALSE;
  }
  OSEndTask();
} /* end of DrawCPULoadTask */
/* End of Processor Load Measurement and Display Section ------------------------------*/


/* ADC12 Functions --------------------------------------------------------------------*/
/* The ADC12 is a central component of this application as it is it that collects most of
** the information that is displayed on the LCD or that triggers the computation of the
** ball positions. */

// ISR descriptor also used by other tasks that interface with the ADC12.
typedef struct ADC12DescriptorDef {
   void (*interruptHandler)(struct ADC12DescriptorDef *descriptor);
   INT16 xAxisOffset;       // Reference point of gravity of the 2-axis accelerators 
   INT16 yAxisOffset;
   BOOL requestCalibration; // TRUE if next accelerator measure is a new reference
   void *measures;          // Pointer to a 3-slot buffer holding the temperature and Vcc
} ADC12DescriptorDef;

typedef struct {            // Asynchronous 3-slot buffer type modified by the ADC12 ISR
   INT32 temperature;       // and periodically read by tasks DrawTemperatureTask and
   INT32 Vcc;               // and DrawVoltageTask
} ADC12Data;

static void ADC12Interrupt(ADC12DescriptorDef *descriptor);
static void AdcStartReadTask(void *argument);
static void DrawTemperatureTask(void *adcDataBuffer);
static void DrawVoltageTask(void *adcDataBuffer);

/* InitADC12: Sets up the ADC12 to measure the movements, temperature and voltage.
** This function returns a pointer to a flag indicating whether the next accelerator mea-
** sure serves as a reference point. This value is passed on to the selector of the track-
** point button. */
BOOL *InitADC12(void)
{
  ADC12DescriptorDef *des;
  REFCTL0 &= ~REFMSTR;    // Set ADC12 registers to control voltage reference 
  ADC12CTL0 &= ~ADC12ENC; // Ensure conversion is disabled
  ADC12CTL0 = ADC12ON + ADC12SHT0_7 + ADC12MSC + ADC12REF2_5V + ADC12REFON;
  ADC12CTL1 = ADC12SHP + ADC12CONSEQ_1;  
  ADC12MCTL0 = ADC12INCH_1; // Set accelerometer X channel
  ADC12MCTL1 = ADC12INCH_2; // Set accelerometer Y channel
  ADC12MCTL2 = ADC12SREF_1 + ADC12INCH_10; // Set temperature channel
  ADC12MCTL3 = ADC12SREF_1 + ADC12INCH_11 + ADC12EOS; // Set Vcc channel
  Wait(0xFFFF); // Wait for reference stabilization
  des = (ADC12DescriptorDef *)OSMalloc(sizeof(ADC12DescriptorDef));
  /* Initialize intertask communication with an asynchronous 3-slot buffer. This allows
  ** the ADC12 to prepare the VCC and the temperature and which will later be drawn onto
  ** the LCD by dedicated periodic tasks DrawVoltageTask and DrawTemperatureTask. */
  des->measures = OSInitBuffer(sizeof(ADC12Data),OS_BUFFER_TYPE_3_SLOT,NULL);
  /* Register the interrupt handler for the ADC. */  
  des->interruptHandler = ADC12Interrupt;
  OSSetISRDescriptor(OS_IO_ADC12_ADM3,des);
  /* Assure that the 1st accelerator measure is a new reference point of gravity */
  des->requestCalibration = TRUE;
  /* Create the periodic tasks that display the measured values on the LCD. */
  #if defined(ZOTTAOS_VERSION_HARD)
     OSCreateTask(AdcStartReadTask,0,3000,3000,NULL);
     OSCreateTask(DrawTemperatureTask,0,180000,180000,des->measures);
     OSCreateTask(DrawVoltageTask,0,60000,60000,des->measures);
  #elif defined(ZOTTAOS_VERSION_SOFT)
     OSCreateTask(AdcStartReadTask,1,0,3000,3000,1,1,0,NULL);
     OSCreateTask(DrawTemperatureTask,1600,0,180000,180000,1,1,0,des->measures);
     OSCreateTask(DrawVoltageTask,650,0,60000,60000,1,1,0,des->measures);
  #endif
  return &des->requestCalibration;
} /* end of InitADC12 */

/* ACD12 Interrupt handler */
void ADC12Interrupt(ADC12DescriptorDef *descriptor)
{
  ADC12Data data;
  INT16 dx, dy;
  dx = ADC12MEM0;
  dy = -ADC12MEM1;
  /* Update calibration values */
  if (descriptor->requestCalibration) {
     descriptor->xAxisOffset = dx;
     descriptor->yAxisOffset = dy;
     descriptor->requestCalibration = FALSE;
  }
  /* Update balls positions */
  dx = (dx - descriptor->xAxisOffset) >> 5;
  if (dx > 4) dx = 4;
  else if (dx < -4) dx = -4;
  dy = (dy - descriptor->yAxisOffset) >> 5;
  if (dy > 4) dy = 4;
  else if (dy < -4) dy = -4;
  SetInitialBallVelocities(dx,dy);
  SetNewBallCoordinates(dx,dy);
  /* Get VCC (see SLAU208H page 489) */
  /* Vcc * 10 = A11 * 2.5V * 2 * 10 / 4096 */
  data.Vcc = ADC12MEM3;         // Convert into INT32 for the next computation
  data.Vcc = data.Vcc * 50 >> 12;
  /* Convert temperature
  ** ((A10 / 4096 * 2500mV) - 680mV) * (1 / 2.25mV) = (A10 / 4096 * 1111) - 302
  ** = (A10 - 1113) * (666 / 4096) */
  data.temperature = ADC12MEM2; // Convert into INT32 for the next computation
  data.temperature = (data.temperature - 1113) * 666 >> 12;
  /* Update the values in the buffer shared between tasks DrawTemperatureTask and Draw-
  ** VoltageTask */
  OSWriteBuffer(descriptor->measures,(UINT8 *)&data,sizeof(ADC12Data));
  ADC12IFG = 0;
  ADC12CTL0 &= ~(ADC12ENC | ADC12SC);
} /* end of ADC12Interrupt */

/* AdcStartReadTask: Periodic task that triggers an ADC measure. The measured values are
** the accelerometers, the supply voltage and the temperature, which are then stored in
** the slots of an asynchronous reader-writer protocol. */
void AdcStartReadTask(void *argument)
{
  ADC12IFG &= ~(BIT3+BIT2+BIT1+BIT0);
  ADC12CTL0 |=  ADC12ENC | ADC12SC | ADC12REFON;  
  ADC12IE |= BIT3;
  OSEndTask();
} /* end of AdcStartReadTask */

/* DrawTemperatureTask: Periodic task that displays the temperature at the bottom right
** of the LCD display.
** The argument of this task points to a 3-slot buffer holding the most recently measured
** temperature. */
void DrawTemperatureTask(void *adcDataBuffer)
{
  ADC12Data data;
  UINT16 temp;
  char str[5];
  if (!LCDLock && LCDState != LCD_WAKEUP && LCDState != LCD_SLEEP) {
     while (OSGetCopyBuffer(adcDataBuffer,OS_READ_MULTIPLE,(UINT8 *)&data) !=
                                                                      sizeof(ADC12Data));
     if ((temp = data.temperature) < 10)
        str[0] = ' ';
     else
        for (str[0] = '0'; temp >= 10; temp -= 10)
           str[0]++;
     str[1] = '0' + temp;
     str[2] = '^';
     str[3]='C';
     str[4] = '\0';
     LCDLock = TRUE;
     halLcdPrintLineCol(str,8,0,OVERWRITE_TEXT + INVERT_TEXT);
     LCDLock = FALSE;
  }
  OSEndTask();
} /* end of DrawTemperatureTask */

/* DrawVoltageTask: Periodic task that displays the supply voltage VCC centered at the
** bottom of the LCD display. This task also adjusts the contrast of the LCD.
** The argument of this task points to a 3-slot buffer holding the most recently measured
** Vcc. */
void DrawVoltageTask(void *adcDataBuffer)
{
  ADC12Data data;
  UINT16 vcc;
  char str[5];
  if (!LCDLock && LCDState != LCD_WAKEUP && LCDState != LCD_SLEEP) {
     while (OSGetCopyBuffer(adcDataBuffer,OS_READ_MULTIPLE,(UINT8 *)&data) !=
                                                                      sizeof(ADC12Data));
     str[0] = '0';
     for (vcc = data.Vcc; vcc >= 10; vcc -= 10)
        str[0]++;
     str[1] = '.';
     str[2] = '0' + vcc;
     str[3] = 'V';
     str[4] = '\0';
     LCDLock = TRUE;
     halLcdPrintLineCol(str,8,5,OVERWRITE_TEXT + INVERT_TEXT);
     LCDLock = FALSE;
  }
  OSEndTask();
} /* end of DrawVoltageTask */
/* End of ADC12 Functions Section -----------------------------------------------------*/



/* LCD Contrast Setting Functions -----------------------------------------------------*/
/* InitContrastButton: Initializes the contrast button ISR and creates the event-driven
** task that increments and sets the LCD contrast level. */

typedef struct ButtonDescriptorDef { // Trackpoint Button interrupt descriptor
  void (*buttonInterruptHandler)(struct ButtonDescriptorDef *);
  void *signaledEvent;
} ButtonDescriptorDef;


static void S1ButtonInterrupt(ButtonDescriptorDef *des);
static void S1ButtonTask(void *taskEvent);

void InitContrastButton(void)
{
  ButtonDescriptorDef *des;
  des = (ButtonDescriptorDef *)OSMalloc(sizeof(ButtonDescriptorDef));
  des->buttonInterruptHandler = S1ButtonInterrupt;
  des->signaledEvent = OSCreateEventDescriptor();
  OSSetISRDescriptor(OS_IO_PORT2_6,des);
  #if defined(ZOTTAOS_VERSION_HARD)
     OSCreateSynchronousTask(S1ButtonTask,18000,des->signaledEvent,des->signaledEvent);
  #elif defined(ZOTTAOS_VERSION_SOFT)
     OSCreateSynchronousTask(S1ButtonTask,4,18000,des->signaledEvent,des->signaledEvent);
  #endif
  P2IFG &= ~0x40; // Clear interrupts flags
  P2IES |= 0x40;  // Allow interrupts on descending edges
  P2IE |= 0x40;   // Enable interrupts
} /* end of InitContrastButton */

/* S1ButtonInterrupt: ISR for pushbutton S1 that signals task S1ButtonTask to increment
** and set the LCD contrast level. */
void S1ButtonInterrupt(ButtonDescriptorDef *des)
{
  OSScheduleSuspendedTask(des->signaledEvent);
} /* end of S1ButtonInterrupt */

/* S1ButtonTask: Event-driven task signaled by the ISR for button S1 and which sets the
** next contrast level every time the user presses on S1. */
void S1ButtonTask(void *taskEvent)
{
  static BOOL first = TRUE;
  if (first || (P2IN & 0x40) == 0) {
     SetLCDContrastLevel(TRUE);
     first = FALSE;
     OSScheduleSuspendedTask(taskEvent);
  }
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
  OSSuspendSynchronousTask();
} /* end of S1ButtonTask */

/* SetLCDContrastLevel: Sets the contrast of the LCD and optionally steps to the next
** predefined level. */
void SetLCDContrastLevel(BOOL increment)
{
  static UINT8 contrastLevel = 0;        // Current LCD contrast level
  if (increment && ++contrastLevel > 7)  // Step through the 7 predefined levels
     contrastLevel = 0;
  if (!LCDLock && LCDState != LCD_WAKEUP && LCDState != LCD_SLEEP) {
     LCDLock = TRUE;
     switch (contrastLevel) {
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
     LCDLock = FALSE;
  }
} /* end of SetLCDContrastLevel */
/* End of LCD Contrast Setting Functions Section --------------------------------------*/



/* Synthetic Task and Its Load --------------------------------------------------------*/
/* Functions to create a synthetic background task whose only purpose is to consume or
** waste CPU cycles and show the effect of high CPU utilization on the QoS of the tasks
** that scrolls the banner and paint the balls onto the LCD screen. The load of the back-
** ground task can be modified by pressing on button S2 which is connected to port pin
** P2.7.
** Although the synthetic task is created only for ZottaOS-Soft, the interrupt handler
** for S2 is still installed in ZottaOS-Hard to reactivate the LCD screen when S2 is
** pressed. */

typedef struct SyntheticTaskDescriptorDef {
   void (*buttonInterruptHandler)(struct SyntheticTaskDescriptorDef *);
   void *modifyLoadEvent; // Event to step to the next load
   #ifdef ZOTTAOS_VERSION_SOFT
      UINT8 workLoad;     // Current load brought by the synthetic task
   #endif
} SyntheticTaskDescriptorDef;

static SyntheticTaskDescriptorDef *InitSyntheticTaskLoadButton(void);
static void S2ButtonInterrupt(SyntheticTaskDescriptorDef *descriptor);
static void S2ButtonTask(void *argument);
#ifdef ZOTTAOS_VERSION_SOFT
   static void SyntheticTask(void *argument);
#endif

/* InitSyntheticBackgroundTask: Entry point to create the synthetic task and the ISR
** handler that can vary its load. */
void InitSyntheticBackgroundTask(void)
{
  SyntheticTaskDescriptorDef *des = InitSyntheticTaskLoadButton();
  #ifdef ZOTTAOS_VERSION_SOFT
     OSCreateTask(SyntheticTask,9300,0,15000,15000,1,1,0,&des->workLoad);
  #endif
} /* end of InitSyntheticBackgroundTask */

/* InitSyntheticTaskLoadButton: Initializes the dummy load button selector ISR. */
SyntheticTaskDescriptorDef *InitSyntheticTaskLoadButton(void)
{
  SyntheticTaskDescriptorDef *des;
  des = (SyntheticTaskDescriptorDef *)OSMalloc(sizeof(SyntheticTaskDescriptorDef));
  des->buttonInterruptHandler = S2ButtonInterrupt;
  des->modifyLoadEvent = OSCreateEventDescriptor();
  #ifdef ZOTTAOS_VERSION_SOFT
     des->workLoad = 0;
  #endif
  OSSetISRDescriptor(OS_IO_PORT2_7,des);
  #if defined(ZOTTAOS_VERSION_HARD)
     OSCreateSynchronousTask(S2ButtonTask,18000,des->modifyLoadEvent,des);
  #elif defined(ZOTTAOS_VERSION_SOFT)
     OSCreateSynchronousTask(S2ButtonTask,4,18000,des->modifyLoadEvent,des);
  #endif
  P2IFG &= ~0x80; // Clear interrupts flags
  P2IES |= 0x80;  // Allow interrupts on descending edges
  P2IE |= 0x80;   // Enable interrupts
  return des;
} /* end of InitSyntheticTaskLoadButton */

/* S2ButtonInterrupt: ISR for pushbutton S2 that signals task S2ButtonTask to cyclically
** step to the next load level of the synthetic task. */
void S2ButtonInterrupt(SyntheticTaskDescriptorDef *descriptor)
{
  OSScheduleSuspendedTask(descriptor->modifyLoadEvent);
} /* end of S2ButtonInterrupt */

/* S2ButtonTask: Event-driven task signaled by the ISR for button S2 and which sets the
** next load that will be executed by the periodic task SyntheticTask. */
void S2ButtonTask(void *argument)
{
  static BOOL first = TRUE;
  SyntheticTaskDescriptorDef *des = (SyntheticTaskDescriptorDef *)argument;
  if (first || (P2IN & 0x80) == 0) {
     #ifdef ZOTTAOS_VERSION_SOFT
        if (++des->workLoad == 3)
           des->workLoad = 0;
     #endif
     first = FALSE;
     OSScheduleSuspendedTask(des->modifyLoadEvent);
  }
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
} /* end of S2ButtonTask */

#ifdef ZOTTAOS_VERSION_SOFT
/* SyntheticTask: Dummy periodic task that creates a controlled load on the processor,
** thus reducing the fraction of time that is available to paint the balls and to scroll
** the banner.
** The task's argument is a pointer to the current load that the task should waste. */
void SyntheticTask(void *argument)
{
  char tmp[4];
  UINT32 i;
  if (LCDState != LCD_WAKEUP && LCDState != LCD_SLEEP) {
     switch (*(UINT8 *)argument) {
        case 1:
           P1OUT ^= 0x01;
           for (i = 0; i < 153500; i++);
           P1OUT ^= 0x01;
           tmp[0] = '4'; tmp[1] = '0';
           break;
        case 2:
           P1OUT ^= 0x01;
           for (i = 0; i < 220000; i++);
           P1OUT ^= 0x01;
           tmp[0] = '6'; tmp[1] = '0';
           break;
        default:
           tmp[0] = '0'; tmp[1] = '0';
     }
     tmp[2] = '%'; tmp[3] = '\0';
     if (!LCDLock) {
        LCDLock = TRUE;
        halLcdPrintLineCol(tmp,8,14,OVERWRITE_TEXT + INVERT_TEXT);
        LCDLock = FALSE;
     }
  }
  OSEndTask();
} /* end of SyntheticTask */
#endif
/* End of Synthetic Task and Its Load Section------------------------------------------*/



/* Bouncing Ball Functions ------------------------------------------------------------*/
/* The definitions and functions below all pertain to the displayed balls. All but 3
** functions are confined to this section:
**  - InitBalls: Called prior to OSStartMultitasking to create the balls.
**  - SetInitialBallVelocities: Called by the ADC12 ISR to set new velocities.
**  - SetNewBallCoordinates: Called by the ADC ISR to determine the new positions.*/

typedef struct BallParameters {
   INT16 x, y;               // current position
   INT8 radius;              // ball radius
   INT16 resistance;         // friction coefficient x 128
   INT16 speedX, speedY;     // velocity vector
   INT16 dx, dy;             // delta velocity in current frame
   INT16 wallX, wallY;       // maximum wall compensation
   INT16 oldX, oldY;         // previously drawn position
   struct BallParameters *taboo;
   struct BallParameters *next;
} BallParameters;

static BallParameters *BallQueue = NULL; // Global list of balls

static BallParameters *CreateBall(INT16 x, INT16 y, INT8 radius, INT16 resistance);
static void DrawBallTask(void *argument);
static BOOL WereBallsOverlapping(BallParameters *ball, BallParameters *otherBall);
static INT16 GetNextWallReboundEventTime(INT16 remainingFraction);
typedef enum {LEFTWALL,RIGHTWALL,TOPWALL,BOTTOMWALL} WALL;
static void ResolveImmediateWallCollision(BallParameters *ball, INT16 remainingFraction, WALL wall);
static void UpdateBallPositions(INT16 remainingFraction, INT16 nextEvent, BOOL resolveWallRebounds);
static INT16 GetNextBallCollisionEventTime(BallParameters **b1, BallParameters **b2);
static INT16 TimeToBallCollision(BallParameters *ball, BallParameters *otherBall);
static void SetCollisionVelosity(BallParameters *ball, INT16 remainingFraction);
static void ResetCollisionVelosity(BallParameters *ball);
static BOOL ResolveCollision(BallParameters *ball, BallParameters *otherBall);
static void CorrectWallNegativeVelocity(BallParameters *ball);
static INT16 Sqrt(INT32 x);


#define BALLS 7  /* Number of balls that are in the application. */

/* Creates the tasks that draw the bouncing balls and initially display them. */
void InitBalls(void)
{
  BallParameters *ball;
  #if defined(ZOTTAOS_VERSION_HARD)
     OSCreateTask(DrawBallTask,0,3000,3000,CreateBall(20,20,5,94));
     OSCreateTask(DrawBallTask,0,3000,3000,CreateBall(30,30,5,96));
     OSCreateTask(DrawBallTask,0,3000,3000,CreateBall(40,40,5,98));
     OSCreateTask(DrawBallTask,0,3000,3000,CreateBall(50,50,5,100));
     OSCreateTask(DrawBallTask,0,3000,3000,CreateBall(60,60,5,102));
     OSCreateTask(DrawBallTask,0,3000,3000,CreateBall(70,70,5,102));
     OSCreateTask(DrawBallTask,0,3000,3000,CreateBall(80,80,5,94));
  #elif defined(ZOTTAOS_VERSION_SOFT)
     OSCreateTask(DrawBallTask,1400,0,3000,3000,1,1,0,CreateBall(30,30,5,96));
     OSCreateTask(DrawBallTask,1400,0,3000,3000,1,1,0,CreateBall(50,50,5,98));
     OSCreateTask(DrawBallTask,1400,0,3000,3000,1,1,0,CreateBall(70,60,5,100));
  #endif
  /* Draw the balls at their initial positions. */
  for (ball = BallQueue; ball != NULL; ball = ball->next)
     halLcdFilledCircle(ball->x,ball->y,ball->radius,PIXEL_ON);
} /* end of InitBalls */

/* CreateBall: Creates a ball by adding a new BallParameters structure onto the dynamic
** list of balls (global variable BallQueue).
** Parameters:
**   - x, y: coordinates of the ball to create
**   - radius: constant radius
**   - resistance: a value between 64 (=0.5) and 128 (=1.0) representing the surface
**                 friction that decelerates the ball from frame to frame. */
BallParameters *CreateBall(INT16 x, INT16 y, INT8 radius, INT16 resistance)
{
  BallParameters *ball;
  ball = (BallParameters *)OSMalloc(sizeof(BallParameters));
  ball->oldX = ball->x = x;
  ball->oldY = ball->y = y;
  ball->radius = radius;
  ball->resistance = resistance;
  ball->speedX = ball->speedY = 0;
  ball->next = BallQueue;
  BallQueue = ball;
  return ball;
} /* end of CreateBall */

/* DrawBallTask: Periodic task that updates the displayed movement of the bouncing
** balls. */
void DrawBallTask(void *argument)
{
  BallParameters *other, *ball = (BallParameters *)argument;
  if (ball->oldX != ball->x || ball->oldY != ball->y) 
     if (LCDState == LCD_SLEEP) { // Is the display turned off?
        OSScheduleSuspendedTask(LCDWakeupEvent);
        LCDState = LCD_WAKEUP;
     }
     else if (LCDState != LCD_WAKEUP && !LCDLock) {
        LCDState = LCD_TIMEOUT_RESET; // Reset the LCD turn-off counter
        LCDLock = TRUE;
        halLcdFilledSmallCircle(ball->oldX,ball->oldY,ball->radius,PIXEL_OFF);
        halLcdFilledSmallCircle(ball->x,ball->y,ball->radius,PIXEL_ON);
        for (other = BallQueue; other != NULL; other = other->next)
           if (other != ball && WereBallsOverlapping(ball,other))
               halLcdFilledSmallCircle(other->oldX,other->oldY,other->radius,PIXEL_ON);
        LCDLock = FALSE;
        ball->oldX = ball->x; ball->oldY = ball->y;
     }
  OSEndTask();
} /* end of DrawBallTask */

/* WereBallsOverlapping: Determines whether 2 balls were superposed prior to redrawing
** one of them. */
BOOL WereBallsOverlapping(BallParameters *ball, BallParameters *otherBall)
{
  INT16 c; // Can support screens of 180 x 180
  struct {
     INT16 x, y;
  } deltaPos;
  deltaPos.x = ball->oldX - otherBall->oldX; deltaPos.y = ball->oldY - otherBall->oldY;
  c = ball->radius + otherBall->radius;
  c = deltaPos.x * deltaPos.x + deltaPos.y * deltaPos.y - c * c;
  return c < 0;
} /* end of WereBallsOverlapping */

#define WALLRIGHT      (LCD_COL - 3)
#define WALLBOTTOM     (LCD_ROW - 15)
#define WALLLEFT       0
#define WALLTOP        12
#define MAXCOLLISIONS  BALLS * 2 * (BALLS + 1)

/* SetInitialBallVelocities: Modifies the velocities of each ball marking the start of a
** frame. A frame consists of all the ball events that occur between 2 ADC measurements.
** (See SetNewBallCoordinates below) */
void SetInitialBallVelocities(INT16 dx, INT16 dy)
{
  BallParameters *ball;
  for (ball = BallQueue; ball != NULL; ball = ball->next) {
     ball->dx = ball->speedX; ball->dy = ball->speedY;
     ball->speedX -= dx;  // Update velocity of this ball
     ball->speedY -= dy;
     ball->wallX = ball->wallY = 0;
     /* Simulate some resistance to reduce the speed and converge to 0. Bounds below are
     ** set to max i s.t. floor(i * 128/resistance) = i, so that there is at least 1 unit
     ** of decrease: Select i from for the ball with the highest resistance
           i = 9 for resistance = 120         i = 8 for resistance = 119
           i = 7 for resistance = 118         i = 6 for resistance = 116 - 117
           i = 5 for resistance = 112 - 155   i = 4 for resistance = 107 - 111
           i = 3 for resistance = 96 - 106    i = 2 for resistance < 96         */
     if (-3 <= ball->speedX && ball->speedX <= 3)
        ball->speedX -= (ball->speedX > 0) - (ball->speedX < 0);
     else
        ball->speedX = (ball->speedX * ball->resistance + 64) >> 7;
     if (-3 <= ball->speedY && ball->speedY <= 3)
        ball->speedY -= (ball->speedY > 0) - (ball->speedY < 0);
     else
        ball->speedY = (ball->speedY * ball->resistance + 64) >> 7;
     /* Set the acceleration done during this frame, only if the velocities are in the
     ** same direction as the previous frame. */
     if (dx * ball->speedX < 0) ball->dx -= ball->speedX;
     else ball->dx = 0;
     if (dy * ball->speedY < 0) ball->dy -= ball->speedY;
     else ball->dy = 0;
     /* Reset the ball against a wall if ball's velocity is normal to the wall. */
     if (ball->speedX < 0) {      // check left vertical wall
        if (WALLLEFT + ball->radius == ball->x) {
           ball->wallX = -ball->dx; ball->dx = ball->speedX = 0;
        }
     }
     else if (ball->speedX > 0) { // check right vertical wall
        if (WALLRIGHT - ball->radius == ball->x) {
           ball->wallX = -ball->dx; ball->dx = ball->speedX = 0;
        }
     }
     else if (WALLLEFT + ball->radius == ball->x)
        if (dy < 0) ball->wallX = dy;
        else ball->wallX = -dy;
     else if (WALLRIGHT - ball->radius == ball->x)
        if (dy < 0) ball->wallX = -dy;
        else ball->wallX = dy;
     if (ball->speedY < 0) {      // check top horizontal wall
        if (WALLTOP + ball->radius == ball->y) {
           ball->wallY = -ball->dy; ball->dy = ball->speedY = 0;
        }
     }
     else if (ball->speedY > 0) { // check bottom horizontal wall
        if (WALLBOTTOM - ball->radius == ball->y) {
           ball->wallY = -ball->dy; ball->dy = ball->speedY = 0;
        }
     }
     else if (WALLTOP + ball->radius == ball->y)
        if (dx < 0) ball->wallY = dx;
        else ball->wallY = -dx;
     else if (WALLBOTTOM - ball->radius == ball->y)
        if (dx < 0) ball->wallY = -dx;
        else ball->wallY = dx;
     ball->taboo = NULL;
  }
} /* end of SetInitialBallVelocities */

/* SetNewBallCoordinates: Main loop iterating over all ball events occurring within a
** frame. Basically time advances from one event to the next until the all events are
** covered. */
void SetNewBallCoordinates(INT16 dx, INT16 dy)
{
  #define CORRECT_SPEEDS_AT_IMPACT
  INT16 frame = 256, wallEvent, collisionEvent;
  BallParameters *b1, *b2; // Balls involved in the earliest collision
  BallParameters *ball;
  #ifdef CORRECT_SPEEDS_AT_IMPACT
     BOOL correctImpactSpeeds;
  #endif
  INT8 collisions= 0;
  wallEvent = GetNextWallReboundEventTime(frame);
  collisionEvent = GetNextBallCollisionEventTime(&b1,&b2);
  while (frame > 0 || collisionEvent == 0) {
     /* Get the first event which can be a collision or a rebound. */
     if (wallEvent <= collisionEvent && wallEvent != 0)
        UpdateBallPositions(frame-=wallEvent,wallEvent,TRUE);
     else {
        UpdateBallPositions(frame-=collisionEvent,collisionEvent,FALSE);
        #ifdef CORRECT_SPEEDS_AT_IMPACT
           correctImpactSpeeds = b1->speedX * b2->speedX < 0 && b1->speedY * b2->speedY < 0;
           if (correctImpactSpeeds) {
              SetCollisionVelosity(b1,frame);
              SetCollisionVelosity(b2,frame);
           }
        #endif
        if (!ResolveCollision(b1,b2) && collisionEvent == 0) {
           b1->taboo = b2; b2->taboo = b1;
        }
        else
           for (ball = BallQueue; ball != NULL; ball = ball->next)
              ball->taboo = NULL;
        #ifdef CORRECT_SPEEDS_AT_IMPACT
           if (correctImpactSpeeds) {
              ResetCollisionVelosity(b1);
              ResetCollisionVelosity(b2);
           }
        #endif
        CorrectWallNegativeVelocity(b1);
        CorrectWallNegativeVelocity(b2);
        if (++collisions > MAXCOLLISIONS) {
           break;
        }
     }
     wallEvent = GetNextWallReboundEventTime(frame);
     collisionEvent = GetNextBallCollisionEventTime(&b1,&b2);
  }
} /* end of SetNewBallCoordinates */

/* GetNextWallReboundEventTime: Returns the earlist fraction of time that a ball rebounds
** on a wall.
** Side effect: Balls immediately bouncing on a wall are immediately dealt with to avoid
** computing the earliest collision between 2 balls and then having to recall this func-
** tion. */
INT16 GetNextWallReboundEventTime(INT16 remainingFraction)
{
  BallParameters *ball;
  INT16 tmp, time, eventTime = remainingFraction;
  for (ball = BallQueue; ball != NULL; ball = ball->next) {
     if (ball->speedX < 0) {      // check left vertical wall
        tmp = WALLLEFT + ball->radius - ball->x;
        if (tmp > ball->speedX) { // ball rebounds
           if ((time = (tmp << 8) / ball->speedX) == 0)
              ResolveImmediateWallCollision(ball,remainingFraction,LEFTWALL);
           else if (time < eventTime)
              eventTime = time;
        }
     }
     else if (ball->speedX > 0) { // check right vertical wall
        tmp = WALLRIGHT - ball->radius - ball->x;
        if (tmp < ball->speedX) {   // ball rebounds
           if ((time = (tmp << 8) / ball->speedX) == 0)
              ResolveImmediateWallCollision(ball,remainingFraction,RIGHTWALL);
           else if (time < eventTime)
              eventTime = time;              
        }
     }
     if (ball->speedY < 0) {      // check top horizontal wall
        tmp = WALLTOP + ball->radius - ball->y;
        if (tmp > ball->speedY) {   // ball rebounds
           if ((time = (tmp << 8) / ball->speedY) == 0)
              ResolveImmediateWallCollision(ball,remainingFraction,TOPWALL);
           else if (time < eventTime)
              eventTime = time;
        }
     }
     else if (ball->speedY > 0) { // check bottom horizontal wall
        tmp = WALLBOTTOM - ball->radius - ball->y;
        if (tmp < ball->speedY) {   // ball rebounds
           if ((time = (tmp << 8) / ball->speedY) == 0)
              ResolveImmediateWallCollision(ball,remainingFraction,BOTTOMWALL);
           else if (time < eventTime)
              eventTime = time;
        }
     }
  }
  return eventTime;
} /* end of GetNextWallReboundEventTime */

/* ResolveImmediateWallCollision: Processes wall boundary collision and simulates a re-
** bound. This function first corrects the speed at which the ball hits the wall before
** Inverting the velocity vector. */
void ResolveImmediateWallCollision(BallParameters *ball, INT16 remainingFraction, WALL wall)
{
  INT8 delta;
  switch (wall) {
     case LEFTWALL:
     case RIGHTWALL: // new velocity = -velocity - 2.0 * (1.0-elapsedFaction) * dx
        delta = (ball->dx * remainingFraction + 128) >> 8;
        ball->speedX = -ball->speedX - (delta << 1);
        ball->dx -= delta;
        break;
     case TOPWALL:
     case BOTTOMWALL:
        delta = (ball->dy * remainingFraction + 128) >> 8;
        ball->speedY = -ball->speedY - (delta << 1);
        ball->dy = delta;
  }
  switch (wall) {
     case LEFTWALL:
        if (ball->speedX < 1) {
           ball->speedX = 0; ball->dx = 0;
        }
        break;
     case RIGHTWALL:
        if (ball->speedX > 1) {
           ball->speedX = 0; ball->dx = 0;
        }
        break;
     case TOPWALL:
        if (ball->speedY < 1) {
           ball->speedY = 0; ball->dy = 0;
        }
        break;
     case BOTTOMWALL:
        if (ball->speedY > 1) {
           ball->speedY = 0; ball->dy = 0;
        }
  }
} /* end of ResolveImmediateWallCollision */

/* UpdateBallPositions: Advances the ball to the next event, which can be a wall rebound,
** two balls colliding or the end of the frame. */
void UpdateBallPositions(INT16 remainingFraction, INT16 nextEvent, BOOL resolveWallRebounds)
{
  BallParameters *ball;
  for (ball = BallQueue; ball != NULL; ball = ball->next) {
     ball->x += (ball->speedX * nextEvent + 128) >> 8;
     if (ball->x <= ball->radius) { // Detect a left border contact
        if (resolveWallRebounds)
           ResolveImmediateWallCollision(ball,remainingFraction,LEFTWALL);
        ball->x = ball->radius;
     }
     else if (ball->x >= WALLRIGHT - ball->radius) { // Detect a right border contact
        if (resolveWallRebounds)
           ResolveImmediateWallCollision(ball,remainingFraction,RIGHTWALL);
        ball->x = WALLRIGHT - ball->radius;
     }
     ball->y += ((ball->speedY * nextEvent + 128) >> 8);
     if (ball->y <= ball->radius) { // Detect a top border contact
        if (resolveWallRebounds)
           ResolveImmediateWallCollision(ball,remainingFraction,TOPWALL);
        ball->y = ball->radius; 
     }
     else if (ball->y >= WALLBOTTOM - ball->radius) { // Detect a bottom border contact
        if (resolveWallRebounds)
           ResolveImmediateWallCollision(ball,remainingFraction,BOTTOMWALL);
        ball->y = WALLBOTTOM - ball->radius;
     }
  }
} /* end of UpdateBallPositions */

/* GetNextBallCollisionEventTime: Returns the fraction of time until balls collide. */
INT16 GetNextBallCollisionEventTime(BallParameters **b1, BallParameters **b2)
{
  BallParameters *ball, *otherBall;
  INT16 time, eventTime = 256;
  for (ball = BallQueue; ball != NULL; ball = ball->next)
     for (otherBall = ball->next; otherBall != NULL; otherBall = otherBall->next)
        if (ball->taboo != otherBall)
           if ((time = TimeToBallCollision(ball,otherBall)) < eventTime) {
              eventTime = time; *b1 = ball; *b2 = otherBall;
              if (eventTime == 0) return 0;
           }
  return eventTime;
} /* end of GetNextBallCollisionEventTime */

/* TimeToBallCollision: Returns the fraction of time until 2 specified balls collides by
** inflating one ball and deflating the other so that the problem reduces to the find the
** time a particle intersects a sphere. */
INT16 TimeToBallCollision(BallParameters *ball, BallParameters *otherBall)
{
  INT32 det;
  INT16 a, b, c, time;
  struct {
     INT16 x, y;
  } deltaV, deltaPos;
  deltaV.x = ball->speedX - otherBall->speedX; deltaV.y = ball->speedY - otherBall->speedY;
  a = deltaV.x * deltaV.x + deltaV.y * deltaV.y;
  if (a != 0) {
     deltaPos.x = ball->x - otherBall->x; deltaPos.y = ball->y - otherBall->y;
     b = deltaPos.x * deltaV.x + deltaPos.y * deltaV.y;
     if (b < 0) {
        c = ball->radius + otherBall->radius;
        c = deltaPos.x * deltaPos.x + deltaPos.y * deltaPos.y - c * c;
        if (c <= 0)
           return 0;  // immediate collision
        det = b * b - a * c; // Determinant = b^2 - ac
        if (det >= 0 && (time = -((b + Sqrt(det)) << 8) / a) >= 0)
           return time;
     }
  }
  return 256; // no collision
} /* end of TimeToBallCollision */

/* claude */
void SetCollisionVelosity(BallParameters *ball, INT16 remainingFraction)
{
  INT16 delta;
  delta = (ball->dx * remainingFraction + 128) >> 8;
  ball->speedX += delta;
  ball->dx = delta;
  delta = (ball->dy * remainingFraction + 128) >> 8;
  ball->speedY += delta;
  ball->dy = delta;
} /* end of SetCollisionVelosity */

/* claude */
void ResetCollisionVelosity(BallParameters *ball)
{
  ball->speedX -= ball->dx;
  ball->speedY -= ball->dy;
} /* end of ResetCollisionVelosity */

/* ResolveCollision: Determines the velocities of 2 balls resulting from two-dimensional
** classical elastic collision. */
BOOL ResolveCollision(BallParameters *ball, BallParameters *otherBall)
{
  INT32 v1n, v1t, v2n, v2t;
  struct {
     INT16 x, y;
  } v_n, v_un, v_ut;
  INT16 newSpeed;
  BOOL changed;
  // Compute unit normal and unit tangent vectors
  v_n.x = ball->x - otherBall->x; v_n.y = ball->y - otherBall->y;
  v1n = Sqrt(v_n.x*v_n.x+v_n.y*v_n.y);
  // Get unit vector normal to the collision
  v_un.x = (v_n.x << 8)/ v1n; v_un.y = (v_n.y << 8)/ v1n;
  v_ut.x = -v_un.y; v_ut.y = v_un.x;          // unit tangent vector
  // Compute scalar projections of velocities onto v_un and v_ut
  v1n = (v_un.x * otherBall->speedX + v_un.y * otherBall->speedY + 128) >> 8;
  v1t = (v_ut.x * otherBall->speedX + v_ut.y * otherBall->speedY + 128) >> 8;
  v2n = (v_un.x * ball->speedX + v_un.y * ball->speedY + 128) >> 8;
  v2t = (v_ut.x * ball->speedX + v_ut.y * ball->speedY + 128) >> 8;
  // Set new velocities from computed new normal and tangential velocity vectors
  newSpeed = (v2n * v_un.x + v1t * v_ut.x + 128) >> 8;
  changed = otherBall->speedX != newSpeed;
  otherBall->speedX = newSpeed;
  newSpeed = (v2n * v_un.y + v1t * v_ut.y + 128) >> 8;
  changed = changed || otherBall->speedY != newSpeed;
  otherBall->speedY = newSpeed;
  newSpeed = (v1n * v_un.x + v2t * v_ut.x + 128) >> 8;
  changed = changed || ball->speedX != newSpeed;
  ball->speedX = newSpeed;
  newSpeed = (v1n * v_un.y + v2t * v_ut.y + 128) >> 8;
  changed = changed || ball->speedY != newSpeed;
  ball->speedY = newSpeed;
  return changed;
} /* end of ResolveCollision */

/* CorrectWallNegativeVelocity: Balls that lean on a wall at the start of a frame and are
** involved in a collision have their velocities compensated so that they act as wall to
** colliding balls. */
void CorrectWallNegativeVelocity(BallParameters *ball)
{
  if (ball->wallX != 0)
     if (ball->wallX > 0 && ball->speedX > 0)
        if (ball->wallX >= ball->speedX)
           ball->speedX = 0;
        else
           ball->speedX -= ball->wallX;
     else if (ball->wallX < 0 && ball->speedX < 0)
        if (ball->wallX <= ball->speedX)
           ball->speedX = 0;
        else
           ball->speedX -= ball->wallX;
  if (ball->wallY != 0)
     if (ball->wallY > 0 && ball->speedY > 0)
        if (ball->wallY >= ball->speedY)
           ball->speedY = 0;
        else
           ball->speedY -= ball->wallY;
     else if (ball->wallY < 0 && ball->speedY < 0)
        if (ball->wallY <= ball->speedY)
           ball->speedY = 0;
        else
           ball->speedY -= ball->wallY;
} /* end of CorrectWallNegativeVelocity */

/* Sqrt: Returns the square root of an integer, i.e., sqrt(x), using Newton's iterative
** approach. This function assumes that x >= 0 and stops as soon as 2 successive itera-
** tions produce the same value or that 10 iterations are reached. */
INT16 Sqrt(INT32 x)
{
  INT32 r, nextR;
  UINT8 i; // Used to bound the number of iterations
  if (x == 0) return 0;
  nextR = x >> 1;
  for (i = 0; i < 10; i += 1) {
     r = nextR;
     nextR >>= 1;
     nextR = ((x + nextR)/r + r) >> 1;
     if (nextR - r == 0) break;
  }
  return nextR;
} /* end of Sqrt */
/* End of Bouncing Ball Section -------------------------------------------------------*/
