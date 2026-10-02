/* Copyright (c) 2026 Bertrand Hurst. All rights reserved.
** Escapement - Lightweight Power-Aware Real-Time OS.
** Distributed under the terms of LICENSE at the root of this repository.
*/
/* File SleepU5.c: The idle task in Stop 2 (Escapement_Stop2.c) under one periodic task,
** what the PPK2 is to measure.
**
** A task every 100 ms lights the green of LED3 for some 100 us of work, and reads TIM2, the
** kernel's microsecond, and LPTIM1: between two instances the idle task sleeps in Stop 2,
** TIM2 standing still, and moves TIM2 on by what LPTIM1 counted on waking. The sums of
** both over the run then agree within the two crystals, 15 ppm on the board (docs/
** stm32u5.md), plus what each sleep loses; and each instance starts one period after the
** one before, within a few microseconds, if the wake-up comes in time. Each instance
** also has a timer event of TIM5 wake a second task 40 ms on, which reads TIM2 in turn:
** the idle task sleeps in Stop 2 up to the event as up to the next instance, TIM5 moved
** on with TIM2, and the event comes when it was due, within a few microseconds.
**
** Built with make PHASES=n, the idle task alternates n seconds in Stop 2, D13 high, and n
** seconds in Sleep, D13 low (OSAllowStop2), the load the same: what the PPK2 compares,
** its digital input on D13 telling the phases apart in the one record.
**
** Every ten instances a line of text goes to Linux on LPUART1: "SLEEP" and, in
** hexadecimal, the words 1 to 16 of Results. Linux may send back a count, one byte after
** the other modulo 256, which LPUART1 receives through Stop 2 (Escapement_UART.c) and the
** handler checks. The idle task stays in Sleep for the few milliseconds of a line's
** sending, and for the window that follows each byte received (Escapement_Stop2.h).
**
** LPUART1 runs at 115,200 baud in the UNO Q's build (Makefile), the byte that wakes the
** chip sampled while HSI16 starts, which may make it anything (Escapement_UART.c). Linux
** therefore sends a
** wake-up byte, 0x00, waits some 5 ms for the chip's clock, then its count in a frame:
** 0x00, the bytes of the count and a CRC-16 encoded with COBS, 0x00 (docs/roadmap.md,
** item 6). Whatever the wake-up byte became ends in an empty frame, ignored, or in an
** invalid one, dropped and counted: a frame of the count dropped would show as bytes
** missing from it.
**
** Built for a NUCLEO-U575ZI-Q (Examples/nucleo-u575, BOARD_NUCLEO_U575), the reports go
** over USART1 instead, to the virtual COM port of the board's ST-LINK, at 115,200 baud,
** and nothing is received: USART1 stops in Stop 2, and receiving on it would keep the idle
** task out of Stop 2 (_OSUARTIdle, Escapement_UART.c). Words 11 to 13 stay at 0.
**
** Results, in words: 0 marker, 1 instances measured, 2 ticks of LPTIM1 summed, 3 us of
** TIM2 summed, 4 largest gap between two starts off the period, in us, 5 entries into
** Stop 2, 6 largest wake-up in ticks, 7 wake-ups past the next event, 8 1 if Stop 2
** could not be set up (no LSE), 9 timer events come, 10 largest gap between the time an
** event was due and the time it came, in us, early or late, 11 bytes received from Linux,
** 12 bytes out of the count, 13 bytes lost to an overrun, 14 wake-ups the HSE missed,
** PLL1 then on the MSIS until the next, 15 sleeps held in Sleep by the window of LPUART1,
** 16 frames dropped, too short, too long or their CRC wrong.
** Platform version: STM32U585 (Arduino UNO Q).
*/

#include "Escapement.h"
#include "Escapement_Timer.h"
#include "Escapement_LPTimer.h"
#include "Escapement_Stop2.h"
#include "Escapement_UART.h"
#include "Escapement_TimerEvent.h"
#include "BoardU5.h"

#define MARKER      0x534C5050u          /* "SLPP" */
/* Built as SleepWrapU5 with TIME_SCALE 1000 for escapement_u5.robot, which clocks TIM2,
** TIM5 and LPTIM1 as much faster (escapement_u5_wrap.repl): a tick is then a nanosecond,
** the 2^30 wrap comes after 1.07 s, and the load is the board's. */
#ifndef TIME_SCALE
   #define TIME_SCALE 1
#endif
#define PERIOD      (100000 * TIME_SCALE)  /* us */
#define WORK        4000                 /* loop turns, some 100 us at 160 MHz */
#define EVENT_DELAY (40000 * TIME_SCALE)   /* us */
#define REPORT_SIZE 150                /* SLEEP and 16 numbers of 8 digits at most, spaced */
#define FRAME_SIZE  64                 /* a frame as received, before decoding */

volatile struct {
  UINT32 Marker, Instances, Ticks, Micros, JitterMax, Entries, WakeMaxTicks, Late, NoLSE;
  UINT32 Events, EventOffMax, LinkBytes, LinkErrors, LinkOverruns, HSEMissed, LinkHeld;
  UINT32 LinkDropped;
} Results;

#ifndef BOARD_NUCLEO_U575
   static UINT8 LinkNext;                // the byte of the count expected next
   static UINT8 Frame[FRAME_SIZE];       // the frame being received, then decoded in place
   static UINT32 FrameLength;            // bytes received since the last 0x00
#endif

static INT32 EventDue;                   // TIM2's time the event is due at

/* The UART of the reports, and of the link where there is one. */
#ifdef BOARD_NUCLEO_U575
   #define LINK_UART      OS_IO_USART1
   #define LINK_HANDLER   NULL
#else
   #define LINK_UART      OS_IO_LPUART1
   #define LINK_HANDLER   LinkReceive
#endif

static void SleepTask(void *argument);
static void EventTask(void *argument);
#ifndef BOARD_NUCLEO_U575
   static void LinkReceive(UINT8 byte);
   static void LinkFrame(void);
#endif
static void Report(void);


int main(void)
{
  void *event;
  OSInitializeSystemClocks();
  Results.Marker = MARKER;
  InitializeFlag(FLAG1_PIN);
  #ifdef SLEEP_PHASES
     InitializeFlag(FLAG3_PIN);
     SetPin(FLAG3_PIN);
  #endif
  OSInitUART(1,REPORT_SIZE,LINK_HANDLER,LINK_UART);
  OSInitTimerEvent(1,1,OS_IO_TIM5);
  Results.NoLSE = !OSInitStop2();
  event = OSCreateEventDescriptor();
  #if defined(ESCAPEMENT_VERSION_SOFT)
     OSCreateSynchronousTask(EventTask,0,1000 * TIME_SCALE,0,event,NULL);
     OSCreateTask(SleepTask,50 * TIME_SCALE,0,PERIOD,PERIOD,1,1,0,event);
  #else
     OSCreateSynchronousTask(EventTask,1000 * TIME_SCALE,event,NULL);
     OSCreateTask(SleepTask,0,PERIOD,PERIOD,event);
  #endif
  return OSStartMultitasking(NULL,NULL);
} /* end of main */


/* SleepTask: The work, both counts summed from the second instance on, the gap between
** the starts, and a report every ten instances. */
static void SleepTask(void *argument)
{
  static BOOL started = FALSE;
  static UINT16 lastTicks;
  static INT32 lastMicros;
  UINT16 ticks = OSGetLPTimer();
  INT32 micros = _OSGetActualTime();
  UINT32 gap, off;
  volatile UINT32 i;
  OS_STOP2_COUNTS counts;
  SetPin(FLAG1_PIN);
  if (started) {
     gap = (UINT32)(micros - lastMicros) & 0x3FFFFFFF;                // across the wrap
     off = gap > PERIOD ? gap - PERIOD : PERIOD - gap;
     if (off > Results.JitterMax)
        Results.JitterMax = off;
     Results.Ticks += (UINT16)(ticks - lastTicks);
     Results.Micros += gap;
     Results.Instances += 1;
     if (Results.Instances % 10 == 0) {
        OSGetStop2Counts(&counts);
        Results.Entries = counts.Entries;
        Results.WakeMaxTicks = counts.WakeMaxTicks;
        Results.Late = counts.Late;
        Results.HSEMissed = counts.HSEMissed;
        Results.LinkHeld = counts.LinkHeld;
        Results.LinkOverruns = OSGetUARTOverruns(LINK_UART);
        Report();
     }
  }
  #ifdef SLEEP_PHASES
     /* The phase changes at a start: the next sleep is the first of the new one. */
     if (Results.Instances % (SLEEP_PHASES * 1000000 * TIME_SCALE / PERIOD) == 0) {
        if ((Results.Instances / (SLEEP_PHASES * 1000000 * TIME_SCALE / PERIOD)) % 2 == 0) {
           OSAllowStop2(TRUE);
           SetPin(FLAG3_PIN);
        }
        else {
           OSAllowStop2(FALSE);
           ClearPin(FLAG3_PIN);
        }
     }
  #endif
  started = TRUE;
  lastTicks = ticks;
  lastMicros = micros;
  for (i = 0; i < WORK; i += 1);
  EventDue = (_OSGetActualTime() + EVENT_DELAY) & 0x3FFFFFFF;
  OSScheduleTimerEvent(argument,EVENT_DELAY,OS_IO_TIM5);
  ClearPin(FLAG1_PIN);
  OSEndTask();
} /* end of SleepTask */


/* EventTask: The gap between the time the event was due and the time it came, early or
** late, on TIM2's 30 bits. */
static void EventTask(void *argument)
{
  INT32 off = (INT32)(((UINT32)_OSGetActualTime() - (UINT32)EventDue) << 2) >> 2;
  (void)argument;
  if (off < 0)
     off = -off;
  if ((UINT32)off > Results.EventOffMax)
     Results.EventOffMax = off;
  Results.Events += 1;
  OSSuspendSynchronousTask();
} /* end of EventTask */


#ifndef BOARD_NUCLEO_U575
/* LinkReceive: A byte from Linux, from the interrupt of LPUART1: one more of the frame,
** or the 0x00 that ends it. A frame longer than the buffer is kept counting, to be
** dropped at its end. */
static void LinkReceive(UINT8 byte)
{
  if (byte != 0) {
     if (FrameLength < FRAME_SIZE)
        Frame[FrameLength] = byte;
     FrameLength += 1;
  }
  else if (FrameLength > 0) {
     LinkFrame();
     FrameLength = 0;
  }
} /* end of LinkReceive */


/* LinkFrame: Decodes the frame received, COBS: each code byte gives the distance to the
** next 0x00 the encoding took out, none after a code of 0xFF or at the end. The bytes of
** a frame whose CRC-16 (CCITT, 0x1021 from 0xFFFF, as Python's binascii.crc_hqx) holds
** are each to be the one after the last, else an error, the count then taken up from
** it. */
static void LinkFrame(void)
{
  UINT32 in = 0, out = 0, code, k;
  UINT16 crc = 0xFFFF;
  if (FrameLength > FRAME_SIZE) {
     Results.LinkDropped += 1;
     return;
  }
  while (in < FrameLength) {
     code = Frame[in++];
     for (k = 1; k < code; k += 1) {
        if (in == FrameLength) {
           Results.LinkDropped += 1;     // a code pointing past the end
           return;
        }
        Frame[out++] = Frame[in++];
     }
     if (code < 0xFF && in < FrameLength)
        Frame[out++] = 0;
  }
  if (out < 3) {                         // a byte of the count and the CRC at least
     Results.LinkDropped += 1;
     return;
  }
  for (k = 0; k < out; k += 1) {
     UINT32 bit;
     crc ^= (UINT16)(Frame[k] << 8);
     for (bit = 0; bit < 8; bit += 1)
        crc = crc & 0x8000 ? (UINT16)(crc << 1 ^ 0x1021) : (UINT16)(crc << 1);
  }
  if (crc != 0) {                        // the CRC over the data and itself, big-endian
     Results.LinkDropped += 1;
     return;
  }
  for (k = 0; k < out - 2; k += 1) {
     if (Results.LinkBytes > 0 && Frame[k] != LinkNext)
        Results.LinkErrors += 1;
     LinkNext = (UINT8)(Frame[k] + 1);
     Results.LinkBytes += 1;
  }
} /* end of LinkFrame */
#endif


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


/* Report: Words 1 to 16 of Results as a line of text to Linux; none if the line before is
** still being sent. */
static void Report(void)
{
  UINT8 *line = (UINT8 *)OSGetFreeNodeUART(LINK_UART), *p;
  if (line == NULL)
     return;
  p = line;
  *p++ = 'S'; *p++ = 'L'; *p++ = 'E'; *p++ = 'E'; *p++ = 'P'; *p++ = ' ';
  p = PutHex(p,Results.Instances);
  p = PutHex(p,Results.Ticks);
  p = PutHex(p,Results.Micros);
  p = PutHex(p,Results.JitterMax);
  p = PutHex(p,Results.Entries);
  p = PutHex(p,Results.WakeMaxTicks);
  p = PutHex(p,Results.Late);
  p = PutHex(p,Results.NoLSE);
  p = PutHex(p,Results.Events);
  p = PutHex(p,Results.EventOffMax);
  p = PutHex(p,Results.LinkBytes);
  p = PutHex(p,Results.LinkErrors);
  p = PutHex(p,Results.LinkOverruns);
  p = PutHex(p,Results.HSEMissed);
  p = PutHex(p,Results.LinkHeld);
  p = PutHex(p,Results.LinkDropped);
  p[-1] = '\n';
  OSEnqueueUART(line,(UINT8)(p - line),LINK_UART);
} /* end of Report */
