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
/* File Escapement_UART.h: User interface of the UART driver.
** Platform version: RP2040 (Raspberry Pi Pico).
*/

#ifndef ESCAPEMENT_UART_H
#define ESCAPEMENT_UART_H

/* OSInitUART: Creates the descriptor of a UART, its transmit queue, configures the pins
** and the baud rate, and installs the interrupt handler.
** Parameters:
**   (1) (UINT8) number of buffers in the transmit queue;
**   (2) (UINT8) size in bytes of each of these buffers;
**   (3) pointer to the function called on every byte received, from interrupt context;
**   (4) (UINT8) OS_IO_UART0 or OS_IO_UART1.
** Returned value: TRUE when the allocation succeeded. */
BOOL OSInitUART(UINT8 maxNodes, UINT8 maxNodeSize, void (*ReceiveHandler)(UINT8),
                UINT8 interruptIndex);

/* OSGetFreeNodeUART: Returns a free buffer, to be filled and then handed to OSEnqueueUART,
** or NULL when the queue is full. */
void *OSGetFreeNodeUART(UINT8 interruptIndex);

/* OSReleaseNodeUART: Gives back a buffer obtained from OSGetFreeNodeUART without sending
** it. */
void OSReleaseNodeUART(void *buffer, UINT8 interruptIndex);

/* OSEnqueueUART: Queues a buffer for transmission and wakes up the sending interrupt. */
void OSEnqueueUART(void *buffer, UINT8 dataSize, UINT8 interruptIndex);

#endif /* ESCAPEMENT_UART_H */
