/* Copyright (c) 2026 Bertrand Hurst. Part of Escapement, distributed under the terms of
** LICENSE at the root of this repository.
*/
/* File ClockU3.c: How long each step of the clock set-up takes on the board, for the
** points of docs/stm32u3.md that only the board can decide: the LSE's start, LSESYSRDY,
** the lock of the MSI on it, R1RDY, BOOSTRDY and the MSIS at 96 MHz.
**
** The backup domain is reset first (BDRST, RCC_BDCR, RM0487 p. 471-473), which stops the
** LSE that a run before left going, so that OSInitializeSystemClocks starts it as after a
** power-on. The cycle counter of the DWT counts from there, and the port, built with
** OS_CLOCK_TIMES (make build/ClockU3.elf), reads it at each step (Escapement_Processor.c).
** Every step but the last runs at the 12 MHz of reset: 12 cycles a microsecond.
**
** Once a second, a line on USART1, at 115,200 baud: "CLOCK" and, in hexadecimal, the
** cycles of each step, the LSE's start (from LSEON to LSERDY), LSESYSRDY, the lock,
** what came between the lock and the raise, R1RDY, BOOSTRDY, the MSIS at its frequency;
** then 1 if the MSI locked, and the times it was locked again since.
** Platform version: STM32U385 (NUCLEO-U385RG-Q).
*/

#include "Escapement.h"
#include "Escapement_UART.h"

#define RCC_AHB1ENR2      *((volatile UINT32 *)(0x40030C00 + 0x094))
#define RCC_AHB1ENR2_PWREN (1u << 2)
#define RCC_BDCR          *((volatile UINT32 *)(0x40030C00 + 0x110))
#define RCC_BDCR_BDRST    (1u << 16)
#define PWR_DBPR          *((volatile UINT32 *)(0x40030800 + 0x28))
#define PWR_DBPR_DBP      (1u << 0)
/* The DWT's cycle counter, which TRCENA in DEMCR powers (ARMv8-M, C1.5, D1.2). */
#define DEMCR             *((volatile UINT32 *)0xE000EDFC)
#define DEMCR_TRCENA      (1u << 24)
#define DWT_CTRL          *((volatile UINT32 *)0xE0001000)
#define DWT_CYCCNT        *((volatile UINT32 *)0xE0001004)

#define STEPS             8
#define LINE_SIZE         96
#define UART_VECTOR       OS_IO_USART1

#if defined(ESCAPEMENT_VERSION_SOFT)
   #define PERIODIC(task,wcet,period) OSCreateTask(task,wcet,0,period,period,1,1,0,NULL)
#else
   #define PERIODIC(task,wcet,period) OSCreateTask(task,0,period,period,NULL)
#endif

extern UINT32 _OSClockTimes[STEPS];

static void ReportTask(void *argument);
static UINT8 *PutHex(UINT8 *p, UINT32 value);


int main(void)
{
  /* PWR's clock, two cycles of its bus, then the backup domain open to writes and reset;
  ** LockMSI opens it again. */
  RCC_AHB1ENR2 |= RCC_AHB1ENR2_PWREN;
  (void)RCC_AHB1ENR2;
  PWR_DBPR |= PWR_DBPR_DBP;
  while ((PWR_DBPR & PWR_DBPR_DBP) == 0);
  RCC_BDCR |= RCC_BDCR_BDRST;
  RCC_BDCR &= ~RCC_BDCR_BDRST;
  DEMCR |= DEMCR_TRCENA;
  DWT_CYCCNT = 0;
  DWT_CTRL |= 1;
  OSInitializeSystemClocks();
  if (!OSInitUART(2,LINE_SIZE,NULL,UART_VECTOR))
     while (TRUE);
  PERIODIC(ReportTask,200,1000000);
  return OSStartMultitasking(NULL,NULL);
} /* end of main */


/* ReportTask: The cycles of each step, as a line on USART1. */
static void ReportTask(void *argument)
{
  UINT8 *line = (UINT8 *)OSGetFreeNodeUART(UART_VECTOR), *p;
  UINT32 i;
  (void)argument;
  if (line != NULL) {
     p = line;
     *p++ = 'C'; *p++ = 'L'; *p++ = 'O'; *p++ = 'C'; *p++ = 'K'; *p++ = ' ';
     for (i = 1; i < STEPS; i += 1)
        p = PutHex(p,_OSClockTimes[i] - _OSClockTimes[i - 1]);
     p = PutHex(p,OSMSILocked());
     p = PutHex(p,OSGetMSIRelocks());
     p[-1] = '\n';
     OSEnqueueUART(line,(UINT8)(p - line),UART_VECTOR);
  }
  OSEndTask();
} /* end of ReportTask */


/* PutHex: A number in hexadecimal, without leading zeros, and a space after it. */
static UINT8 *PutHex(UINT8 *p, UINT32 value)
{
  INT32 shift = 28;
  while (shift > 0 && (value >> shift) == 0)
     shift -= 4;
  for (; shift >= 0; shift -= 4)
     *p++ = "0123456789abcdef"[value >> shift & 0xF];
  *p++ = ' ';
  return p;
} /* end of PutHex */
