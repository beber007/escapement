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
/* File ZottaOS_Processor.h: Defined architecture specifics for ZottaOS kernels. Note that
**                           these definitions are not intended for the user.
** Version identifier: March 2012
** Authors: MIS-TIC
*/

#ifndef ZOTTAOS_PROCESSOR_H_
#define ZOTTAOS_PROCESSOR_H_

/* Non-blocking algorithms use a marker that needs to be part of the address. These algo-
** rithms operate in RAM and for which a MSP430 or CC430 MSB address is never used. */
#define MARKEDBIT    0x8000u
#define UNMARKEDBIT  0x7FFFu

#define _OSDisableInterrupts() _disable_interrupt()
#define _OSEnableInterrupts()  _enable_interrupt()

/* ASSEMBLER ROUTINES DEFINED IN ZottaOS_msp430XXX.asm or ZottaOS_cc430XXX.asm */
void _OSSleep(void);
void _OSScheduleTask(void);

#endif /* ZOTTAOS_PROCESSOR_H_ */
