/* Copyright (c) 2026 Bertrand Hurst. All rights reserved.
** Escapement - Lightweight Power-Aware Real-Time OS.
** Derived from prior work; see LICENSE and NOTICE at the root of this repository.
*/
/* File Escapement_Processor.h: Defined architecture specifics for Escapement kernels. Note that
**                           these definitions are not intended for the user.
** Platform version: All MSP430 and CC430 microcontrollers.
** Version identifier: March 2012
*/

#ifndef ESCAPEMENT_PROCESSOR_H_
#define ESCAPEMENT_PROCESSOR_H_

/* Non-blocking algorithms use a marker that needs to be part of the address. These algo-
** rithms operate in RAM and for which a MSP430 or CC430 MSB address is never used. */
#define MARKEDBIT    0x8000u
#define UNMARKEDBIT  0x7FFFu

#define _OSDisableInterrupts() _disable_interrupt()
#define _OSEnableInterrupts()  _enable_interrupt()

/* ASSEMBLER ROUTINES DEFINED IN Escapement_msp430XXX.asm or Escapement_cc430XXX.asm */
void _OSSleep(void);
void _OSScheduleTask(void);

#endif /* ESCAPEMENT_PROCESSOR_H_ */
