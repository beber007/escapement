/*
 * ZottaOS_Processor.h
 *
 *  Created on: 2 mars 2012
 *      Author: bertrand.hurst
 */

#ifndef ZOTTAOS_PROCESSOR_H_
#define ZOTTAOS_PROCESSOR_H_

#include "ZottaOS_msp430.h"

/* Non-blocking algorithms use a marker that needs to be part of the address. These algo-
** rithms operate in RAM and for which a MSP430 or CC430 MSB address is never used. */
#define MARKEDBIT    0x8000u
#define UNMARKEDBIT  0x7FFFu

#define _OSUINTPTR_LL OSUINT16_LL
#define _OSUINTPTR_SC OSUINT16_SC

/* _OSScheduleTask: .*/
void _OSScheduleTask(void);

#define _OSDisableInterrupts() _disable_interrupt()
#define _OSEnableInterrupts()  _enable_interrupt()



#endif /* ZOTTAOS_PROCESSOR_H_ */
