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
/* File UARTEchoPico2.c: Sends back every byte received on UART0, from a handler called in
** interrupt context. Transposition of UARTSimpleEchoF4.c to the Raspberry Pi Pico.
**
** UART0 comes out on GP0 (TX, pin 1) and GP1 (RX, pin 2), the pins the Raspberry Pi Debug
** Probe expects. 115200 baud, 8N1.
**
** Platform version: RP2350 (Raspberry Pi Pico 2), unchanged from UARTEchoPico.c: the
** port hides what differs.
*/

#include "Escapement.h"
#include "Escapement_UART.h"

/* Transmit queue: the echo sends one byte at a time, but a few buffers absorb a burst of
** input without losing anything. */
#define UART_TRANSMIT_FIFO_NB_NODE   8
#define UART_TRANSMIT_FIFO_NODE_SIZE 1

#define UART_VECTOR OS_IO_UART0

static void UARTUserReceiveInterruptHandler(UINT8 data);


int main(void)
{
  extern void (* const CortexMxVectorTable[])(void);
  /* The firmware runs from SRAM: point the processor at the vector table placed there by
  ** the linker before any interrupt can be taken. */
  *((volatile UINT32 *)0xE000ED08) = (UINT32)CortexMxVectorTable;
  /* Leave the ring oscillator for the crystal, so the baud rate and the microsecond tick
  ** mean something. */
  OSInitializeSystemClocks();
  /* Initialize the UART driver and its hardware. */
  if (!OSInitUART(UART_TRANSMIT_FIFO_NB_NODE,UART_TRANSMIT_FIFO_NODE_SIZE,
                  UARTUserReceiveInterruptHandler,UART_VECTOR))
     while (TRUE);
  /* No periodic task here: everything happens on reception. The kernel runs its idle task,
  ** which sleeps until an interrupt arrives. */
  return OSStartMultitasking(NULL,NULL);
} /* end of main */


/* UARTUserReceiveInterruptHandler: Called for each byte received; queues it straight back
** for transmission. */
static void UARTUserReceiveInterruptHandler(UINT8 data)
{
  UINT8 *tmp = (UINT8 *)OSGetFreeNodeUART(UART_VECTOR);
  if (tmp != NULL) {
     *tmp = data;
     OSEnqueueUART(tmp,1,UART_VECTOR);
  }
} /* end of UARTUserReceiveInterruptHandler */
