#include "ZottaOS_Types.h"
#include "ZottaOS_Processor.h"
#include "ZottaOS_msp430.h"


void _OSSleep(void)
{
  /* set bits CPUOFF, SCG0 and SCG1 into the SR register to enter in LPM3 sleep mode */
  __asm("\tbis.w #0xD0,sr");
}



#define STATE_RUNNING       0x01 /* These values are also used in ZottaOS_msp430XXX.asm */

typedef struct MinimalTCB {
  struct MinimalTCB *Next[2]; // Next TCB in the list where this task is located
                              // [0]: ready queue link, [1]: arrival queue link
  UINT8 TaskState;            // Current state of the task
} MinimalTCB;

extern MinimalTCB *_OSActiveTask;

/* ASSEMBLER ROUTINES DEFINED IN ZottaOS_msp430XXX.asm or ZottaOS_cc430XXX.asm */
void _OSEndIntClearNoSaveCtx(void);  // Continue running a preempted task
void _OSStartNextReadyTask(void);    // Start a new task instance

void _OSScheduleTask(void)
{
  if (_OSActiveTask->TaskState & STATE_RUNNING)
     _OSEndIntClearNoSaveCtx(); // Continue running a preempted task
  else
     _OSStartNextReadyTask();   // Start a new task instance
}


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
** left unchanged.
** All but 16-bit pointer LL/SC manipulations are given in ZottaOS.h. In fact, 16-bit
** manipulations are exactly like unsigned 16-bit integers albeit with different types.
** As we do not foresee an application use of pointers, we define the prototypes here. */

BOOL _OSLLReserveBit;

/* OSUINT32_LL: This function emulates an LL on 32-bit operands and takes the address of
** the memory location that is to be reserved and returned. */
UINT32 OSUINT32_LL(UINT32 *memory)
{
  UINT32 tmp;
  UINT16 state;
  state = _get_interrupt_state(); // Save interrupt enable bit value
  _disable_interrupts();
  _OSLLReserveBit = TRUE;         // Mark reserved
  tmp = *memory;                  // Return the contents of the memory location
  _set_interrupt_state(state);    // Restore previous interrupt enable bit value
  return tmp;
} /* end of OSUINT32_LL */

/* OSUINT32_SC: This function emulates a SC operation on 32 bit operands and it is paired
** with its respective LL functions.
** This instruction takes 2 parameters: a memory location and its new contents; and re-
** turns a boolean indicating whether or not the memory location was modified or not. */
BOOL OSUINT32_SC(UINT32 *memory, UINT32 newValue)
{
  BOOL tmp;
  UINT16 state;
  state = _get_interrupt_state(); // Save interrupt enable bit value
  _disable_interrupts();
  if (_OSLLReserveBit) {          // Is the reservation still on?
     _OSLLReserveBit = FALSE;
     *memory = newValue;
     tmp = TRUE;
  }
  else
     tmp = FALSE;
  _set_interrupt_state(state);    // Restore previous interrupt enable bit value
  return tmp;
} /* end of OSUINT32_SC */

/* OSINT32_LL: This function emulates an LL on 32-bit operands and takes the address of
** the memory location that is to be reserved and returned. */
INT32 OSINT32_LL(INT32 *memory)
{
  INT32 tmp;
  UINT16 state;
  state = _get_interrupt_state(); // Save interrupt enable bit value
  _disable_interrupts();
  _OSLLReserveBit = TRUE;         // Mark reserved
  tmp = *memory;                  // Return the contents of the memory location
  _set_interrupt_state(state);    // Restore previous interrupt enable bit value
  return tmp;
} /* end of OSINT32_LL */

/* OSINT32_SC: This function emulates a SC operation on 32 bit operands and it is paired
** with its respective LL functions.
** This instruction takes 2 parameters: a memory location and its new contents; and re-
** turns a boolean indicating whether or not the memory location was modified or not. */
BOOL OSINT32_SC(INT32 *memory, INT32 newValue)
{
  BOOL tmp;
  UINT16 state;
  state = _get_interrupt_state(); // Save interrupt enable bit value
  _disable_interrupts();
  if (_OSLLReserveBit) {          // Is the reservation still on?
     _OSLLReserveBit = FALSE;
     *memory = newValue;
     tmp = TRUE;
  }
  else
     tmp = FALSE;
  _set_interrupt_state(state);    // Restore previous interrupt enable bit value
  return tmp;
} /* end of OSUINT32_SC */

/* OSUINT16_LL: This function emulates an LL on 16-bit operands and takes the address of
** the memory location that is to be reserved and returned. */
UINT16 OSUINT16_LL(UINT16 *memory)
{
  UINT16 tmp,state;
  state = _get_interrupt_state(); // Save interrupt enable bit value
  _disable_interrupts();
  _OSLLReserveBit = TRUE;         // Mark reserved
  tmp = *memory;                  // Return the contents of the memory location
  _set_interrupt_state(state);    // Restore previous interrupt enable bit value
  return tmp;
} /* end of OSUINT16_LL */

/* OSUINT16_SC: This function emulates a SC operation on 16 bit operands and it is paired
** with its respective LL functions.
** This instruction takes 2 parameters: a memory location and its new contents; and re-
** turns a boolean indicating whether or not the memory location was modified or not. */
BOOL OSUINT16_SC(UINT16 *memory, UINT16 newValue)
{
  BOOL tmp;
  UINT16 state;
  state = _get_interrupt_state(); // Save interrupt enable bit value
  _disable_interrupts();
  if (_OSLLReserveBit) {          // Is the reservation still on?
     _OSLLReserveBit = FALSE;
     *memory = newValue;
     tmp = TRUE;
  }
  else
     tmp = FALSE;
  _set_interrupt_state(state);    // Restore previous interrupt enable bit value
  return tmp;
} /* end of OSUINT16_SC */

/* OSINT16_LL: This function emulates an LL on 16-bit operands and takes the address of
** the memory location that is to be reserved and returned. */
INT16 OSINT16_LL(INT16 *memory)
{
  INT16 tmp;
  UINT16 state;
  state = _get_interrupt_state(); // Save interrupt enable bit value
  _disable_interrupts();
  _OSLLReserveBit = TRUE;         // Mark reserved
  tmp = *memory;                  // Return the contents of the memory location
  _set_interrupt_state(state);    // Restore previous interrupt enable bit value
  return tmp;
} /* end of OSINT16_LL */

/* OSINT16_SC: This function emulates a SC operation on 16 bit operands and it is paired
** with its respective LL functions.
** This instruction takes 2 parameters: a memory location and its new contents; and re-
** turns a boolean indicating whether or not the memory location was modified or not. */
BOOL OSINT16_SC(INT16 *memory, INT16 newValue)
{
  BOOL tmp;
  INT16 state;
  state = _get_interrupt_state(); // Save interrupt enable bit value
  _disable_interrupts();
  if (_OSLLReserveBit) {          // Is the reservation still on?
     _OSLLReserveBit = FALSE;
     *memory = newValue;
     tmp = TRUE;
  }
  else
     tmp = FALSE;
  _set_interrupt_state(state);    // Restore previous interrupt enable bit value
  return tmp;
} /* end of OSINT16_SC */

/* OSUINT8_LL: Same as OSUINT16_LL but applied to a single byte.*/
UINT8 OSUINT8_LL(UINT8 *memory)
{
  UINT16 tmp,state;
  state = _get_interrupt_state(); // Save interrupt enable bit value
  _disable_interrupts();
  _OSLLReserveBit = TRUE;         // Mark reserved
  tmp = *memory;                  // Return the contents of the memory location
  _set_interrupt_state(state);    // Restore previous interrupt enable bit value
  return tmp;
} /* end of OSUINT8_LL */

/* OSUINT8_SC: Same as OSUINT16_SC but applied to to a single byte. */
BOOL OSUINT8_SC(UINT8 *memory, UINT8 newValue)
{
  BOOL tmp;
  UINT16 state;
  state = _get_interrupt_state(); // Save interrupt enable bit value
  _disable_interrupts();
  if (_OSLLReserveBit) {          // Is the reservation still on?
     _OSLLReserveBit = FALSE;
     *memory = newValue;
     tmp = TRUE;
  }
  else
     tmp = FALSE;
  _set_interrupt_state(state);    // Restore previous interrupt enable bit value
  return tmp;
} /* end of OSUINT8_SC */
