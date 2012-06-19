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
/* File MeasureTempVCC.c: This application returns the temperature and the supply voltage,
**  which are then printed on a hyperterminal. It consists of the following tasks:
**  -- Task TaskStartMeasure periodically initiates the data acquisition that is carried
**     out by the ADC12 so that 10 measures are done per second.
**  -- Interrupt routine ADCUserInterruptHandler is called whenever there is an interrupt
**     for Memory Buffer 1 (OS_IO_ADC12_ADM1) for the ADC12 peripheral device. This rou-
**     tine updates the mean values of the temperature and of the supply voltage, which
**     are read by the other tasks of the application. Each measure is also associated
**     with its own counter that counts the number of samples included in its mean, and
**     once they reach a given threshold, respectively MIN_MEANTEMPERATURE_SAMPLES and
**     MIN_MEANVCC_SAMPLES, global variables MeanTempReady and MeanVccReady are set.
**  -- Periodic task TaskOutputVCC converts the mean supply voltage into a character
**     string and streams it to the UART output. This string may directly be printed on a
**     hyperterminal screen. This task is executed every second and if MeanVccReady is
**     not yet set, skips a turn.
**  -- Interrupt handler UARTUserReceiveInterruptHandler is called whenever the UART re-
**     ceives a byte from the hyperterminal that requests the mean temperature. When this
**     character is either 'c' or 'C', the returned value is requested in Celsius. The
**     returned value is in Fahrenheit when either 'f' or 'F' is received. When a tempe-
**     rature request is received, event TemperatureOutputEvent is generated, which will
**     then starts task TaskOutputTemperature.
**  -- Event-driven task TaskOutputTemperature converts the mean temperature into either
**     Celsius or Fahrenheit and then again into a string of characters before sending it
**     to the UART output. This task also re-enables RX interrupts so that the next tem-
**     perature request can be received.
**
** This sample illustrates:
**     -- 2 periodic tasks and an event-driven task;
**     -- How to use the UART API with a stream fed by 2 tasks;
**     -- How to assign interrupt routines to a specific interrupt vector;
**     -- Sharing of global variables that are read and modified in a single instruction.
**
** To set up this sample program, you should:
**   (1) Run ZottaOS Configurator Tool for your MSP430 (must have an ADC12) and
**      (a) select the TX and RX interrupts of either USART0, USCIA0 or USCI_A0 (depends
**          on your particular MSP),
**      (b) select interrupt Memory Buffer 1 of ADC12,
**      (c) and set ACLK to XT1 with a watch crystal (MCLK can be set to any reasonable
**          frequency).
**               MSP430
**          -----------------
**         |              XIN|-
**         |                 | 32kHz
**         |             XOUT|-
**         |                 |
**         |             TX  |----------->
**         |                 | 9600 - 8N1
**         |             RX  |<-----------
**
**   (2) You can then run a hyperterminal on a PC and transmit characters that are echoed
**       back.
** Version identifier: March 2012
** Authors: MIS-TIC */

#include "ZottaOS.h"
#include "ZottaOS_UART.h"

/* The following parameters must be modified to accommodate the specific msp430 that is
** used and correspond to the pins of USART0.
** Example for msp430F149: */
#define UART0_PIN_SEL_REGISTER  P3SEL
#define UART0_TX_PIN_NUMBER     BIT4
#define UART0_RX_PIN_NUMBER     BIT5

/* UART entries to the interrupt table generated by ZottaOS Configurator Tool are renamed
** with generic defines to satisfy all MSP430 having a UART. */
#if defined(OS_IO_USART0RX) && defined(OS_IO_USART0TX)
   #define UART_RX_VECTOR OS_IO_USART0RX
   #define UART_TX_VECTOR OS_IO_USART0TX
#elif defined(OS_IO_USCIA0RX) && defined(OS_IO_USCIA0TX)
   #define UART_RX_VECTOR OS_IO_USCIA0RX
   #define UART_TX_VECTOR OS_IO_USCIA0TX
#elif defined(OS_IO_USCI_A0_RX) && defined(OS_IO_USCI_A0_RX)
   #define UART_RX_VECTOR OS_IO_USCI_A0_RX
   #define UART_TX_VECTOR OS_IO_USCI_A0_TX
#else
   #error UART RX and TX vector are not define correctly
#endif

/* UART transmit FIFO buffer size definitions.
** We need to transmit messages of at most 7 bytes with two different tasks. If we assume
** that these tasks have periods greater than the time needed to transfer their messages,
** each task can never have a pending message when it is dispatched. Therefore 2 buffers
** are sufficient. */
#define UART_TRANSMIT_FIFO_NB_NODE    2
#define UART_TRANSMIT_FIFO_NODE_SIZE  8

/* Temperatures and supply voltage are averaged with weighting factors decreasing exponen-
** tially (this is referred to as an exponential moving average, EMA). The idea is to
** maintain a previous average in a single variable and to compute a new average by at-
** tributing weights to the new sample and to the previous average:
**     average = alpha * sample + (1-alpha) * average
** where alpha is a number between 0 and 1 and corresponds to the weight that is given to
** a new sample relative to the previous average.
** To avoid computing divisions but with shift operators, the above formula can be norma-
** lized by 256:
**     average = (alpha * sample + (256-alpha) * average) / 256
** where reasonable values for alpha are now between 1 and 255
**  */
#define AVERAGE_TEMPERATURE_FACTOR          64    /* alpha applied to temperatures */
#define INVERSE_AVERAGE_TEMPERATURE_FACTOR  192   /* 256 - AVERAGE_TEMPERATURE_FACTOR */
#define AVERAGE_VCC_FACTOR                  64    /* alpha applied to supply voltage */
#define INVERSE_AVERAGE_VCC_FACTOR          192   /* 256 - AVERAGE_VCC_FACTOR */
/* Number of samples to measure before outputting the mean values. */
#define MIN_MEANTEMPERATURE_SAMPLES         20    /* Can be any value samller than 256 */
#define MIN_MEANVCC_SAMPLES                 20

/* Requested temperature unit: Temperature values are sent to the hyperterminal in their
** requested unit. This can in Celsius or Fahrenheit and global variable TemperatureCon-
** vertionType holds the requested temperature unit to satisfy. */
#define CELSIUS    0
#define FAHRENHEIT 1

/* Global variable declarations */
static UINT8 TemperatureConvertionType = CELSIUS;
static UINT16 MeanTemperature = 0; /* Mean temperature modified by ADCUserInterruptHand-
                                   ** ler and read by TaskOutputTemperature */
static BOOL MeanTempReady = FALSE; /* MeanTempReady: Becomes TRUE when MeanTemperature
                              ** includes at least MIN_MEANTEMPERATURE_SAMPLES samples */
static UINT16 MeanVoltage = 0;     /* MeanVoltage: Mean supply voltage modified by ADCUser-
                                   ** InterruptHandler and read by TaskOutputVCC */
static BOOL MeanVccReady = FALSE;  /* MeanVccReady: Becomes TRUE when MeanVoltage includes
                                   ** at least MIN_MEANVCC_SAMPLES samples */
static void *TemperatureOutputEvent;  /* Event to start TaskOutputTemperature, i.e. the
                                   ** the task responsible to send the the temperature
                                   ** at the user's request. */

/* Function prototypes */
// 1. Initialization functions
static void InitializeUARTHardware(void);
static void InitializeADCHardware(void);
// 2. Interrupt handlers
typedef struct ADCInterruptDescriptorDef {
  void (*InterruptHandler)(struct ADCInterruptDescriptorDef *);
} ADCInterruptDescriptorDef;
typedef struct UARTInterruptDescriptorDef { // Custom interrupt handler descriptor
  void (*InterruptHandler)(struct UARTInterruptDescriptorDef *);
  UINT8 *HardwareTransmitBuffer;       // Output UART data transmit buffer
  UINT8 *HardwareControlRegister;      // Register controlling the transmit interrupt
  UINT8 EnableTransmitInterruptBit;    // Enable interrupt bit 
} UARTInterruptDescriptorDef;
static void ADCUserInterruptHandler(ADCInterruptDescriptorDef *descriptor);
static BOOL UARTUserReceiveInterruptHandler(UINT8 data);
static void CustomUARTHandler(UARTInterruptDescriptorDef *descriptor);
// 3. Application tasks
static void TaskStartMeasure(void *argument);
static void TaskOutputVCC(void *argument);
static void TaskOutputTemperature(void *argument);


/* Variables declarations */
const UINT8 WelcomeMessage[] = "Hello, \n\r"
                               "Press a key to continue...\n\r"
                               "\n\r";
static BOOL WaitMain = TRUE;

int main(void)
{
  ADCInterruptDescriptorDef *ADCDescriptor;
  UARTInterruptDescriptorDef *UARTDescriptor;
  void (*UARTHandler)(UARTInterruptDescriptorDef *);
  WDTCTL = WDTPW + WDTHOLD;  // Disable watchdog timer
  /* Set the system clock characteristics */
  OSInitializeSystemClocks();
  /* Create an event to output the temperature at the reception of a command. */
  TemperatureOutputEvent = OSCreateEventDescriptor();
  /* Task declarations */
  #if defined(ZOTTAOS_VERSION_HARD)
     OSCreateTask(TaskStartMeasure,0,3279,3279,NULL);
     OSCreateTask(TaskOutputVCC,0,32768,32768,NULL);
     OSCreateSynchronousTask(TaskOutputTemperature,65536,TemperatureOutputEvent,NULL);
  #elif defined(ZOTTAOS_VERSION_SOFT)
     OSCreateTask(TaskStartMeasure,0,0,3279,3279,1,1,0,NULL);
     OSCreateTask(TaskOutputVCC,0,0,32768,32768,1,1,0,NULL);
     OSCreateSynchronousTask(TaskOutputTemperature,0,65536,TemperatureOutputEvent,NULL);
  #else
     #error Wrong kernel version
  #endif  
  /* Initialize kernel ADC interrupt structure */
  if ((ADCDescriptor = 
       (ADCInterruptDescriptorDef *)OSMalloc(sizeof(ADCInterruptDescriptorDef))) == NULL)
     return FALSE;
  ADCDescriptor->InterruptHandler = ADCUserInterruptHandler;
  OSSetISRDescriptor(OS_IO_ADC12_ADM1,ADCDescriptor);
  /* Initialize ADC hardware */
  InitializeADCHardware();
  /* Initialize ZottaOS I/O UART drivers */
  OSInitTransmitUART(UART_TRANSMIT_FIFO_NB_NODE,UART_TRANSMIT_FIFO_NODE_SIZE,UART_TX_VECTOR);
  OSInitReceiveUART(UARTUserReceiveInterruptHandler,UART_RX_VECTOR);
  /* Initialize USART or USCI 0 hardware */
  InitializeUARTHardware();

  /* Backup default UART interrupt handler created by OSInitTransmitUART */
  UARTDescriptor = ((UARTInterruptDescriptorDef *)OSGetISRDescriptor(UART_TX_VECTOR));
  UARTHandler = UARTDescriptor->InterruptHandler;
  /* Set custom UART handler */
  UARTDescriptor->InterruptHandler = CustomUARTHandler;
  _enable_interrupt();  
  /* Start UART transfert */
  *(UARTDescriptor->HardwareControlRegister) |= UARTDescriptor->EnableTransmitInterruptBit;
  /* Wait until a key is pressed */
  while (WaitMain); 
  _disable_interrupt();  
  /* Restore default UART interrupt handler */
  UARTDescriptor->InterruptHandler = UARTHandler;
  /* Start the OS so that it starts scheduling the user task */
  return OSStartMultitasking();
} /* end of main */


/* InitializeADCHardware: Initialize ADC hardware. */
void InitializeADCHardware(void)
{ /* Sequences channels once. */
  #if OS_MSP430_FAMILY == OS_MSP430_FAMILY_1XX || OS_MSP430_FAMILY == OS_MSP430_FAMILY_2XX
     ADC12CTL0 = ADC12ON + ADC12REFON + ADC12SHT02 + ADC12MSC + ADC12REF2_5V;
     ADC12CTL1 = ADC12SHP + ADC12CONSEQ_1 + ADC12SSEL_0;
     ADC12MCTL0 = ADC12SREF_1 + ADC12INCH_10;            // Temperature sensor
     ADC12MCTL1 = ADC12SREF_1 + ADC12INCH_11 + ADC12EOS; // (AVCC – AVSS) / 2
  #elif OS_MSP430_FAMILY == OS_MSP430_FAMILY_5XX
     REFCTL0 &= ~REFMSTR; // Reset REFMSTR to hand over control to ADC12_A ref control registers  
     ADC12CTL0 = ADC12ON + ADC12REFON + ADC12SHT0_8 + ADC12MSC + ADC12REF2_5V;
     ADC12CTL1 = ADC12SHP + ADC12CONSEQ_1;
     ADC12MCTL0 = ADC12SREF_1 + ADC12INCH_10;            // Temperature sensor
     ADC12MCTL1 = ADC12SREF_1 + ADC12INCH_11 + ADC12EOS; // (AVCC – AVSS) / 2
  #endif
} /* end of InitializeADCHardware */


/* TaskStartMeasure: Periodic task that starts a new ADC measure sequence. The applica-
** tion receives an OS_IO_ADC12_ADM1 interrupt when these measures are complete. */
void TaskStartMeasure(void *argument)
{ /* Start ADC conversion */
  ADC12IFG &= ~(BIT1 + BIT0);
  ADC12CTL0 |= ADC12ENC | ADC12SC | ADC12REFON;
  ADC12IE |= BIT1;
  OSEndTask();
} /* end of TaskStartMeasure */


/* ADCUserInterruptHandler: Handles OS_IO_ADC12_ADM1 interrupts.
** This function is called every time a new acquisition is done, and it computes the new
** averages for the temperature and the supply voltage, which are then outputted separately
** by 2 different tasks.
** Global variables MeanTemperature and MeanVoltage are modified. */
void ADCUserInterruptHandler(ADCInterruptDescriptorDef *descriptor)
{
  static UINT8 samples = 0;
  /* Disable ADC measurements */
  ADC12IFG = 0;
  ADC12CTL0 &= ~(ADC12ENC | ADC12SC | ADC12REFON);
  // Compute new averages for temperature and supply voltage.
  MeanTemperature = (AVERAGE_TEMPERATURE_FACTOR * (UINT32)ADC12MEM0 +
                      INVERSE_AVERAGE_TEMPERATURE_FACTOR * (UINT32)MeanTemperature) >> 8;
  MeanVoltage = (AVERAGE_VCC_FACTOR * (UINT32)ADC12MEM1 +
                      INVERSE_AVERAGE_VCC_FACTOR * (UINT32)MeanVoltage) >> 8;
  // The above computed averages are shared with the tasks that will output them to the
  // UART if we have reached to minimum number of samples.
    if (!MeanVccReady || !MeanTempReady) {
       MeanVccReady = ++samples >= MIN_MEANVCC_SAMPLES;
       MeanTempReady = samples >= MIN_MEANTEMPERATURE_SAMPLES;
    }
} /* end of ADCUserInterruptHandler */


/* InitializeUARTHardware: Initialize USART or USCI 0 hardware. */
void InitializeUARTHardware(void)
{
  UART0_PIN_SEL_REGISTER |= UART0_RX_PIN_NUMBER + UART0_TX_PIN_NUMBER;
  #if defined(OS_IO_USART0RX) && defined(OS_IO_USART0TX)
     #if defined (__MSP430F122__) || defined (__MSP430F1222__)
        ME2 |= UTXE0 + URXE0;  // Enable USART0 TXD/RXD
     #else
        ME1 |= UTXE0 + URXE0;  // Enable USART0 TXD/RXD
     #endif
     UCTL0 |= CHAR;            // 8-bit characters
     UTCTL0 |= SSEL0;          // UCLK = ACLK (32,768 kHz)
     UBR00 = 0x03;             // 9600 Baud ->  UxBR0 = 3, UxBR1 = 0, UxMCTL = 0x4A
     UBR10 = 0x00;
     UMCTL0 = 0x4A;
     UCTL0 &= ~SWRST;          // Initialize USART state machine
     #if defined (__MSP430F122__) || defined (__MSP430F1222__)
        IE2 |= URXIE0;         // Enable USART0 RX interrupt
     #else
        IE1 |= URXIE0;         // Enable USART0 RX interrupt
     #endif
  #elif defined(OS_IO_USCIA0RX) && defined(OS_IO_USCIA0TX)
     UCA0CTL1 |= UCSWRST;      // Put state machine in reset
     UCA0CTL1 |= UCSSEL_1;     // Select ACLK (32,768 kHz) as clock source
     UCA0BR0 = 3;              // 9600 Baud ->  UCBRx = 3, UCBRSx = 3, UCBRFx = 0
     UCA0BR1 = 0;
     UCA0MCTL |= UCBRS_3 + UCBRF_0;
     UCA0CTL1 &= ~UCSWRST;     // Initialize USCI state machine
     IE2 |= UCA0RXIE;          // Enable USCIA0 RX interrupt
  #elif defined(OS_IO_USCI_A0_RX) && defined(OS_IO_USCI_A0_TX)
     UCA0CTL1 |= UCSWRST;      // Put state machine in reset
     UCA0CTL1 |= UCSSEL_1;     // Select ACLK (32,768 kHz) as clock source
     UCA0BR0 = 3;              // 9600 Baud ->  UCBRx = 3, UCBRSx = 3, UCBRFx = 0
     UCA0BR1 = 0;
     UCA0MCTL |= UCBRS_3 + UCBRF_0;
     UCA0CTL1 &= ~UCSWRST;     // Initialize USCI state machine
     UCA0IE |= UCRXIE;         // Enable USCI_A0 RX interrupt
  #else
     #error UART RX and TX vectors are not defined correctly
  #endif
} /* end of InitializeUARTHardware */


/* UARTUserReceiveInterruptHandler: Claude custom handler pour la transmission WelcomeMessage
** effectué dans main.*/
void CustomUARTHandler(UARTInterruptDescriptorDef *descriptor)
{
  static UINT8 index = 0;
  *(descriptor->HardwareTransmitBuffer) = WelcomeMessage[index++];
  /* If there are more data to send, we need to re-enable transmit buffer empty inter-
  ** rupts so that the next byte can be sent. */
  if (WelcomeMessage[index] != '\0')
     *(descriptor->HardwareControlRegister) |= descriptor->EnableTransmitInterruptBit;
} /* end of UARTUserReceiveInterruptHandler */


/* UARTUserReceiveInterruptHandler: This function is called every time a new byte is
** received, and upon reception of a temperature request command, it generates a Tempera-
** tureDisplayEvent that triggers task TaskOutputTemperature, which in turn outputs the
** last measured temperature as a string of characters.
** The received byte serves 2 purposes: As well as being employed as a request, it also
** conveys the unit of value that should be sent back.
** Remark: UART reception is re-enabled by TaskOutputTemperature. */
BOOL UARTUserReceiveInterruptHandler(UINT8 data)
{
  if (!WaitMain) { 
     if (data == 'c' || data == 'C') {
        TemperatureConvertionType = CELSIUS;
        OSScheduleSuspendedTask(TemperatureOutputEvent);
        return FALSE;
     }
     else if (data == 'f' || data == 'F') {
        TemperatureConvertionType = FAHRENHEIT;
        OSScheduleSuspendedTask(TemperatureOutputEvent);
        return FALSE;
     }
     else
        return TRUE;
  }
  else {
     WaitMain = FALSE;
     return TRUE;
  }
} /* end of UARTUserReceiveInterruptHandler */


/* TaskOutputTemperature: Outputs the current temperature to the UART in response to a
** TemperatureDisplayEvent, which is generated when a temperature request is received by
** the UART. The temperature is sent as a string of characters along with its unit ("oF"
** or "oC". The string ends with a carriage return and line feed for the hyperterminal
** displayed text.
** Remark: We assume that resulting temperature T is an integral value -999 <= T <= 999
** regardless of its unit. */
void TaskOutputTemperature(void *argument)
{
  UINT8 i, *freeNode;
  INT8 temperature;
  if (MeanTempReady && (freeNode = (UINT8 *)OSGetFreeNodeUART(UART_TX_VECTOR)) != NULL) {
     // Convert and send temperature value
     if (TemperatureConvertionType == CELSIUS)
        #if OS_MSP430_FAMILY == OS_MSP430_FAMILY_1XX || OS_MSP430_FAMILY == OS_MSP430_FAMILY_2XX || \
            OS_MSP430_FAMILY == OS_MSP430_FAMILY_4XX
           // oC = ((x / 4096) * 2500 mV) - 986 mV) / 3.55mV = x * 11 / 64 - 278
           temperature = ((UINT32)MeanTemperature * 11 >> 6) - 278;
        #elif OS_MSP430_FAMILY == OS_MSP430_FAMILY_5XX || OS_MSP430_FAMILY == OS_MSP430_FAMILY_6XX
           // oC = ((x / 4096) * 2500 mV) - 680 mV) / 2.25mV = (x * 8889 / 1024 - 9671) / 32
           temperature = (((UINT32)MeanTemperature * 8889 >> 10) - 9671) >> 5;
        #else
           #error Unknown temperature sensor for selected MSP430 family
        #endif
     else // TemperatureConvertionType == FAHRENHEIT
        #if OS_MSP430_FAMILY == OS_MSP430_FAMILY_1XX || OS_MSP430_FAMILY == OS_MSP430_FAMILY_2XX || \
            OS_MSP430_FAMILY == OS_MSP430_FAMILY_4XX
           // oF = ((x / 4096) * 2500 mV) - 986 mV) / 3.55mV * 9 / 5 + 32 = x * 315 / 1024 - 468
           temperature = ((UINT32)MeanTemperature * 315 >> 10) - 468;
        #elif OS_MSP430_FAMILY == OS_MSP430_FAMILY_5XX || OS_MSP430_FAMILY == OS_MSP430_FAMILY_6XX
           // oF = ((x / 4096) * 2500 mV) - 680 mV) / 2.25mV * 9 / 5 + 32 = x * 125 / 256 - 512
           temperature = ((UINT32)MeanTemperature * 125 >> 8) - 512;
        #else
           #error Unknown temperature sensor for selected MSP430 family
        #endif
     i = 0;
     //Check for negative temperature
     if (temperature < 0) {
        freeNode[i++] = '-';
        temperature = -temperature;
     }
     if (temperature > 99) {
        freeNode[i++] = '0' + temperature / 100;
        temperature %= 100;
        freeNode[i++] = '0' + temperature / 10;
        temperature %= 10;
     }
     else if (temperature > 9) {
        freeNode[i++] = '0' + temperature / 10;
        temperature %= 10;
     }
     freeNode[i++] = '0' + temperature;
     freeNode[i++] = 'o';
     if (TemperatureConvertionType == CELSIUS)
        freeNode[i++] = 'C';
     else // TemperatureConvertionType == FAHRENHEIT
        freeNode[i++] = 'F';
     freeNode[i++] = '\n';
     freeNode[i++] = '\r';
     OSEnqueueUART(freeNode,i,UART_TX_VECTOR);
  }
  OSEnableReceiveInterruptUART(UART_RX_VECTOR);
  OSSuspendSynchronousTask();
} /* end of TaskOutputTemperature */


/* TaskOutputVCC: Periodically send Vcc to the UART as a string of character digits fol-
** lowed by its unit (Volts). The string ends with a carriage return and line feed to
** accommodate the hyperterminal displayed text. The current average of Vcc is held in
** global variable MeanVoltage.
** Remark: We assume that 0.1 V <= Vcc <= 9.9 V. */
void TaskOutputVCC(void *argument)
{
  UINT8 *freeNode;
  UINT16 Vcc;
  // Convert and send voltage value
  if (MeanVccReady && (freeNode = (UINT8 *)OSGetFreeNodeUART(UART_TX_VECTOR)) != NULL) {
     // Convert Vcc
     Vcc = MeanVoltage / 82;         // Vcc =  MeanVoltage * 2.5 / 4096 * 10 * 2 
     freeNode[0] = '0' + Vcc / 10;
     freeNode[1] = '.';
     freeNode[2] = '0' + Vcc % 10;
     freeNode[3] = 'V';
     freeNode[4] = '\n';
     freeNode[5] = '\r';
     OSEnqueueUART(freeNode,6,UART_TX_VECTOR);
  }
  OSEndTask();
} /* end of TaskOutputVCC */
