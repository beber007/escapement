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
** Authors: MIS-TIC
**
** Escapement - Lightweight Power-Aware Real-Time OS, derived from ZottaOS.
** Modifications Copyright (c) 2026 Bertrand Hurst, distributed under the same terms;
** see LICENSE and NOTICE at the root of this repository.
*/
/* File Escapement_Interrupts.c: Defines the two tables that bind a peripheral interrupt
** to its handler. The first is the vector table proper, appended by the linker right
** after the Cortex-Mx system exceptions; every entry routes to _OSIOHandler, which reads
** the exception number and dispatches through the second table, _OSTabDevice.
** Unlike the STM32 port, the RP2040 has a single part number and no interrupt line shared
** between several timers, so no selector indirection is needed.
** Platform version: RP2040 (Raspberry Pi Pico).
*/

#include "Escapement.h"

extern void _OSIOHandler(void);

#ifdef DEBUG_MODE
   /* An interrupt was raised for a peripheral that has no descriptor installed. */
   static void UndefinedInterrupt(void)
   {
     _OSDisableInterrupts();
     while (TRUE);
   } /* end of UndefinedInterrupt */
#else
   #define UndefinedInterrupt _OSIOHandler
#endif


/* RP2040 vector table, appended to CortexMxVectorTable defined in Escapement_CortexMx.c */
__attribute__ ((section(".isr_vector_specific")))
void (* const RP2040VectorTable[])(void) = {
  _OSIOHandler,       /*  0  TIMER_IRQ_0   */
  _OSIOHandler,       /*  1  TIMER_IRQ_1   */
  _OSIOHandler,       /*  2  TIMER_IRQ_2   (Escapement_TimerEvent.c) */
  _OSIOHandler,       /*  3  TIMER_IRQ_3   (Escapement_TimerEvent.c) */
  UndefinedInterrupt, /*  4  PWM_IRQ_WRAP  */
  UndefinedInterrupt, /*  5  USBCTRL_IRQ   */
  UndefinedInterrupt, /*  6  XIP_IRQ       */
  UndefinedInterrupt, /*  7  PIO0_IRQ_0    */
  UndefinedInterrupt, /*  8  PIO0_IRQ_1    */
  UndefinedInterrupt, /*  9  PIO1_IRQ_0    */
  UndefinedInterrupt, /* 10  PIO1_IRQ_1    */
  UndefinedInterrupt, /* 11  DMA_IRQ_0     */
  UndefinedInterrupt, /* 12  DMA_IRQ_1     */
  _OSIOHandler,       /* 13  IO_IRQ_BANK0  */
  UndefinedInterrupt, /* 14  IO_IRQ_QSPI   */
  UndefinedInterrupt, /* 15  SIO_IRQ_PROC0 */
  UndefinedInterrupt, /* 16  SIO_IRQ_PROC1 */
  UndefinedInterrupt, /* 17  CLOCKS_IRQ    */
  UndefinedInterrupt, /* 18  SPI0_IRQ      */
  UndefinedInterrupt, /* 19  SPI1_IRQ      */
  _OSIOHandler,       /* 20  UART0_IRQ     */
  _OSIOHandler,       /* 21  UART1_IRQ     */
  UndefinedInterrupt, /* 22  ADC_IRQ_FIFO  */
  UndefinedInterrupt, /* 23  I2C0_IRQ      */
  UndefinedInterrupt, /* 24  I2C1_IRQ      */
  UndefinedInterrupt  /* 25  RTC_IRQ       */
};


/* Table of ISR descriptors, controlled by OSSetISRDescriptor and OSGetISRDescriptor and
** read by _OSIOHandler. */
void *_OSTabDevice[OS_IO_NB_ENTRIES] = { 0 };


/* OSSetISRDescriptor: Associates an ISR descriptor with a vector table entry.
** Parameters:
**   (1) (UINT16) index of the entry, see the OS_IO_xxx definitions;
**   (2) (void *) pointer to the descriptor, whose first field must be the handler. */
void OSSetISRDescriptor(UINT16 entry, void *descriptor)
{
  _OSTabDevice[entry] = descriptor;
} /* end of OSSetISRDescriptor */


/* OSGetISRDescriptor: Returns the descriptor associated with a vector table entry.
** Parameter: (UINT16) index of the entry, see the OS_IO_xxx definitions.
** Returned value: (void *) the descriptor, or NULL when none was installed. */
void *OSGetISRDescriptor(UINT16 entry)
{
  return _OSTabDevice[entry];
} /* end of OSGetISRDescriptor */
