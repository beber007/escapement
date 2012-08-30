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
/* File ZottaOS_Atomic.h: Defines the LL/SC atomic instructions used with the types de-
**                        fined for ZottaOS kernels.
** Platform version: All MSP430 and CC430 microcontrollers.
** Version identifier: March 2012
** Authors: MIS-TIC
*/

#include "ZottaOS.h"

/* ATOMIC INSTRUCTIONS
** LL & SC Atomic Instruction Emulation
** The LL/SC pair of instructions are becoming increasing popular among high-end micro-
** controllers. Because they are neither given for the MSP430 or the CC430 and constitute
** a fundamental atomic instruction building block used within ZottaOS, they are emulated
** here. Basically the LL atomically reads the content of a memory location and reserves
** it. The SC instruction used in combination with the LL takes a memory location and a
** value as parameters. It checks whether the memory location is still reserved and if so
** applies the value parameter to it and clears the reservation. On the other hand, if
** the memory location is not reserved at the time of the call, the memory location is
** left unchanged. */

/* Because the MSP430 line of processors do not have a reservation bit, we can emulate
** the validity of the last LL instruction by clearing this indicator whenever there is a
** context switch. This action is done in ZottaOS_msp430XXX.asm or ZottaOS_cc430XXX.asm.
**/
BOOL _OSLLReserveBit;


/* OSUINT8_LL: This function emulates an LL on single byte operands and takes the address
** of the memory location that is to be reserved and returned. */
UINT8 OSUINT8_LL(UINT8 *memory)
{
  _OSLLReserveBit = TRUE;         // Mark reserved
  return *memory;                 // Return the contents of the memory location
} /* end of OSUINT8_LL */


/* OSUINT8_SC: This function emulates a SC operation on single byte operands and it is
** paired with its respective LL functions.
** This instruction takes 2 parameters: a memory location and its new contents; and re-
** turns a boolean indicating whether or not the memory location was modified or not. */
BOOL OSUINT8_SC(UINT8 *memory, UINT8 newValue)
{
  UINT16 state;
  state = _get_interrupt_state(); // Save interrupt enable bit value
  _disable_interrupts();
  if (_OSLLReserveBit) {          // Is the reservation still on?
     _OSLLReserveBit = FALSE;
     *memory = newValue;
     _set_interrupt_state(state); // Restore previous interrupt enable bit value
     return TRUE;
  }
  else {
     _set_interrupt_state(state); // Restore previous interrupt enable bit value
     return FALSE;
  }
} /* end of OSUINT8_SC */


/* OSUINT16_LL: Same as OSUINT8_LL but applied to unsigned 16-bit quantities. */
UINT16 OSUINT16_LL(UINT16 *memory)
{
  _OSLLReserveBit = TRUE;         // Mark reserved
  return *memory;                 // Return the contents of the memory location
} /* end of OSUINT16_LL */


/* OSUINT16_SC: Same as OSUINT8_SC but applied to unsigned 16-bit integers. */
BOOL OSUINT16_SC(UINT16 *memory, UINT16 newValue)
{
  UINT16 state;
  state = _get_interrupt_state(); // Save interrupt enable bit value
  _disable_interrupts();
  if (_OSLLReserveBit) {          // Is the reservation still on?
     _OSLLReserveBit = FALSE;
     *memory = newValue;
     _set_interrupt_state(state); // Restore previous interrupt enable bit value
     return TRUE;
  }
  else {
     _set_interrupt_state(state); // Restore previous interrupt enable bit value
     return FALSE;
  }
} /* end of OSUINT16_SC */


/* OSINT16_LL: Same as OSUINT16_LL but for signed integers. */
INT16 OSINT16_LL(INT16 *memory)
{
  _OSLLReserveBit = TRUE;         // Mark reserved
  return *memory;                 // Return the contents of the memory location
} /* end of OSINT16_LL */


/* OSINT16_SC: Same as OSUINT16_SC but for signed integers. */
BOOL OSINT16_SC(INT16 *memory, INT16 newValue)
{
  INT16 state;
  state = _get_interrupt_state(); // Save interrupt enable bit value
  _disable_interrupts();
  if (_OSLLReserveBit) {          // Is the reservation still on?
     _OSLLReserveBit = FALSE;
     *memory = newValue;
     _set_interrupt_state(state); // Restore previous interrupt enable bit value
     return TRUE;
  }
  else {
     _set_interrupt_state(state); // Restore previous interrupt enable bit value
     return FALSE;
  }
} /* end of OSINT16_SC */


/* OSUINT32_LL: Same as OSUINT8_LL but for 32-bit unsigned integers. */
UINT32 OSUINT32_LL(UINT32 *memory)
{
  UINT32 tmp;
  UINT16 state;
  state = _get_interrupt_state(); // Save interrupt enable bit value
  _OSLLReserveBit = TRUE;         // Mark reserved
  _disable_interrupts();
  tmp = *memory;                  // Return the contents of the memory location
  _set_interrupt_state(state);    // Restore previous interrupt enable bit value
  return tmp;
} /* end of OSUINT32_LL */


/* OSUINT32_SC: Same as OSUINT8_SC but for 32-bit unsigned integers. */
BOOL OSUINT32_SC(UINT32 *memory, UINT32 newValue)
{
  UINT16 state;
  state = _get_interrupt_state(); // Save interrupt enable bit value
  _disable_interrupts();
  if (_OSLLReserveBit) {          // Is the reservation still on?
     _OSLLReserveBit = FALSE;
     *memory = newValue;
     _set_interrupt_state(state); // Restore previous interrupt enable bit value
     return TRUE;
  }
  else {
     _set_interrupt_state(state); // Restore previous interrupt enable bit value
     return FALSE;
  }
} /* end of OSUINT32_SC */


/* OSINT32_LL: Same as OSUINT32_LL but for signed integers. */
INT32 OSINT32_LL(INT32 *memory)
{
  INT32 tmp;
  UINT16 state;
  state = _get_interrupt_state(); // Save interrupt enable bit value
  _OSLLReserveBit = TRUE;         // Mark reserved
  _disable_interrupts();
  tmp = *memory;                  // Return the contents of the memory location
  _set_interrupt_state(state);    // Restore previous interrupt enable bit value
  return tmp;
} /* end of OSINT32_LL */


/* OSINT32_SC: Same as OSUINT32_SC but for signed integers. */
BOOL OSINT32_SC(INT32 *memory, INT32 newValue)
{
  UINT16 state;
  state = _get_interrupt_state(); // Save interrupt enable bit value
  _disable_interrupts();
  if (_OSLLReserveBit) {          // Is the reservation still on?
     _OSLLReserveBit = FALSE;
     *memory = newValue;
     _set_interrupt_state(state); // Restore previous interrupt enable bit value
     return TRUE;
  }
  else {
     _set_interrupt_state(state); // Restore previous interrupt enable bit value
     return FALSE;
  }
} /* end of OSUINT32_SC */
