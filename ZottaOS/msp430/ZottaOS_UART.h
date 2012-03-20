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
/* File ZottaOS_UART.c: Contains the implementation of an API allowing facilitated message
**                      transmissions and easy access to the receiver part of a UART.
** Platform version: All MSP430 and CC430 microcontrollers.
** Version identifier: March 2012
** Authors: MIS-TIC */

#ifndef _ZOTTAOS_UART_H_
#define _ZOTTAOS_UART_H_

/* This file provides an API to a UART device with the basic functions needed to easily
** send bytes. The provided implementation is simple and yet powerful in terms of its
** possibilities.
** The idea behind the implemented API is to associate a FIFO queue of messages to the
** transmitter part of the UART and to leave an entry point from where the application
** can supply the receiver part of the UART. With this API, we believe that developing an
** application requiring UART reception and transmission can be done in record time, as
** the designer would only need to concentrate on the input and output protocols. */


/* OSInitReceiveUART: Called at task creation time in the main program to initialize the
** internal data structure associated with the receiver part of a UART device. This func-
** tion must be called before doing any I/O related with the UART and can be called be-
** fore or after creating the application tasks but never from an application task.
** Low-level protocol, e.g. RS232 or I2C, and reception baud rate of an I/O port must
** be done by the application, typically in the main program. The kernel does not provide
** a function to accomplish this task because low-level hardware settings depend on the
** particular microcontroller at hand.
** Parameters:
**   (1) (BOOL (*userReceiveInterruptHandler)(UINT8)): ISR function that is called when
**       the UART receives a new byte from its input port.
**   (2) (UINT8) interruptIndex: Entry to the interrupt table corresponding to the recei-
**       ver part of a specific UART.
** Returned value: (BOOL) TRUE on success and FALSE if there's insufficient memory to
**   create the descriptor or the buffers that will be used in connection with the UART.
** */
BOOL OSInitReceiveUART(BOOL (*userReceiveInterruptHandler)(UINT8), UINT8 interruptIndex);


/* OSEnableReceiveInterruptUART: Re-enables receive interrupts for a particular UART.
** Parameter: (UINT8) receiveInterruptIndex: Entry to the interrupt table corresponding
**   to the receiver part of a UART device.
** Returned value: None. The function always succeeds. */
void OSEnableReceiveInterruptUART(UINT8 receiveInterruptIndex);


/* OSInitTransmitUART: Called at task creation time in the main program to initialize the
** internal data structure associated with a transmit UART device. This function initia-
** lizes the kernel internals in order to store the bytes that are to be sent on a speci-
** fied output port. The function must be called before doing any I/O related with the
** UART and can be called before or after creating the application tasks but never from
** an application task.
** The transmission baud rate of the UART must be done by the application, typically in
** the main program. The kernel does not provide a function to accomplish this task be-
** cause low-level hardware settings depend on the particular microcontroller at hand.
** Parameters:
**   (1) (UINT8) maxNodes: Maximum number of outstanding messages (or message parts) that
**       can be enqueued at once. This value also corresponds to the initial number of
**       free buffers that are created and inserted into the free list associated with a
**       particular UART device.
**   (2) (UINT8) maxNodeSize: Byte size of the buffers that are in the free list. This
**       value also corresponds to the maximum number of bytes that can be inserted into
**       a message that the UART can transmit.
**   (3) (UINT8) interruptIndex: Entry to the interrupt table corresponding to the trans-
**       mitter part of a specific UART.
** Returned value: (BOOL) TRUE on success and FALSE if there's insufficient memory to
**   create the descriptor or the buffers that will be used in connection with the UART.
** Note: Although the implementation is fully concurrent, the UART does not prevent mes-
** sage parts from being interleaved. Therefore maxNodeSize should be the maximal size of
** a message that is filled by an application task and transferred to the UART. However
** as the implementation guarantees a FIFO processing of the messages transferred to the
** UART, a long message can be broken into smaller parts when there is a single applica-
** tion task that transmits messages through the UART. */ 
BOOL OSInitTransmitUART(UINT8 maxNodes, UINT8 maxNodeSize, UINT8 interruptIndex);


/* OSGetFreeNodeUART: This function returns a free buffer that an application can fill
** with useful data and then transfer it to OSEnqueueUART. The free buffer should come
** from a pool of buffers that is associated with the same UART transmitter queue for
** which the application will later invoke an OSEnqueueUART operation.
** Parameter: (UINT8) transmitInterruptIndex: Entry to the interrupt table corresponding
**   to the transmitter part of the UART device for which the buffer will later be trans-
**   ferred to. This index should correspond to the third parameter of function OSEn-
**   queueUART.
** Returned value: If all the pre-allocated buffers created when calling OSInitTransmit-
**   UART are in use, OSGetFreeNodeUART returns NULL. Otherwise the function returns the
**   starting address of the buffer that can be filled. This same address can then be
**   transferred to OSEnqueueUART, or it can be released to the free pool of buffers by
**   calling function OSReleaseNodeUART. */
void *OSGetFreeNodeUART(UINT8 transmitInterruptIndex);


/* OSReleaseNodeUART: This function releases a buffer that was acquired from OSGetFree-
** NodeUART and returns it to the pool of available buffers associated with an UART. This
** function can be handy when the logic of the program first obtains a buffer from OSGet-
** FreeNodeUART to later realize that the buffer cannot be used.
** Once released, the buffer contents must neither be accessed nor modified.
** Parameters:
**   (1) (void *) buffer: Pointer to the buffer to free;
**   (2) (UINT8) transmitInterruptIndex: Entry to the interrupt table corresponding to
**       the transmitter part of the UART device from which the buffer was received from.
**       This corresponds to the parameter of function OSGetFreeNodeUART.
** Returned value: None. */
void OSReleaseNodeUART(void *buffer, UINT8 transmitInterruptIndex);


/* OSEnqueueUART: This function is called by an application task to output a string of
** bytes contained in a buffer that was provided by OSGetFreeNodeUART. The buffer that is
** transmitted with the call must not be freed with OSReleaseNodeUART as it is given to
** the UART which will later free it once its contents has been transmitted.
** Parameters:
**   (1) (void *) buffer: Byte contents to output from the UART;
**   (2) (UINT8) dataSize: Number of bytes to consider in the 1st parameter;
**   (3) (UINT8) transmitInterruptIndex: Entry to the interrupt table corresponding to
**       the transmitter part of the UART device for which the bytes are intended.
** Returned value: None. The function always succeeds as memory availability is checked
**   when obtaining the buffer from OSGetFreeNodeUART.
** Notes: It is important that the buffer supplied with this call originally comes from
** OSGetFreeNodeUART and for the same UART device. Failure to comply with this restric-
** tion will lead into an erratic behavior. */
void OSEnqueueUART(void *buffer, UINT8 dataSize, UINT8 transmitInterruptIndex);

#endif /* _ZOTTAOS_UART_H_ */
