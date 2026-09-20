/* Copyright (c) 2026 Bertrand Hurst. All rights reserved.
** Escapement - Lightweight Power-Aware Real-Time OS.
** Derived from prior work; see LICENSE and NOTICE at the root of this repository.
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
