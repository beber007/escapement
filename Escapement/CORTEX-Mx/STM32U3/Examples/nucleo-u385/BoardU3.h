/* Copyright (c) 2026 Bertrand Hurst. All rights reserved.
** Escapement - Lightweight Power-Aware Real-Time OS.
** Distributed under the terms of LICENSE at the root of this repository.
*/
/* File BoardU3.h: The outputs the examples drive on the NUCLEO-U385RG-Q, and the GPIO
** registers of its STM32U385 they need (RM0487 rev. 3, GPIO 12 and RCC 10).
**
** A pin is named by its port and number, PIN(port,n). The outputs are pins of the Arduino
** connector, as on the NUCLEO-U575ZI-Q: D7 on PA8, D8 on PC7, D12 on PA6 and D13 on PA5,
** which also drives LD2 through a transistor (UM3062 rev. 2 and ST's BSP, as the plan of
** the port read them; not read again: the board decides). LD2 counts in the current the
** jumper of the MCU measures, as the others do not; B1, on PC13, is never read, its
** toggling disturbing the LSE (ES0626 rev. 3, 2.2.1). PIN_LOW marks a pin that is on when
** low: SetPin turns it on all the same; none is here.
** Platform version: STM32U385 (NUCLEO-U385RG-Q).
*/

#ifndef BOARD_U3_H
#define BOARD_U3_H

#define PORT_A 0
#define PORT_C 2
#define PIN(port,n)       ((UINT8)((port) << 4 | (n)))
#define PIN_LOW(port,n)   ((UINT8)(0x80 | (port) << 4 | (n)))   /* on when low */

#define FLAG1_PIN         PIN(PORT_A,8)        /* D7 */
#define FLAG2_PIN         PIN(PORT_C,7)        /* D8 */
#define FLAG3_PIN         PIN(PORT_A,5)        /* D13, LD2 */
#define PROBE_PIN         PIN(PORT_A,6)        /* D12, measurement probe */

/* Ports on AHB2, 0x400 apart from GPIOA at 0x42020000 (stm32u385xx.h; RM0487, memory
** map, p. 112). MODER takes 01 for an output; BSRR sets a pin with the low half of the
** word and clears it with the high half, in one write that no interrupt can split
** (12.6.1, 12.6.7). */
#define PIN_PORT(p)       (((p) >> 4) & 7)     /* without the mark of PIN_LOW */
#define GPIO_BASE(p)      (0x42020000 + 0x400 * PIN_PORT(p))
#define GPIO_MODER(p)     *((volatile UINT32 *)(GPIO_BASE(p) + 0x00))
#define GPIO_BSRR(p)      *((volatile UINT32 *)(GPIO_BASE(p) + 0x18))
#define PIN_NUMBER(p)     ((p) & 0xF)
#define PIN_HIGH(p)       (1u << PIN_NUMBER(p))
#define PIN_CLEAR(p)      (1u << (PIN_NUMBER(p) + 16))
#define SetPin(p)         (GPIO_BSRR(p) = ((p) & 0x80) ? PIN_CLEAR(p) : PIN_HIGH(p))
#define ClearPin(p)       (GPIO_BSRR(p) = ((p) & 0x80) ? PIN_HIGH(p) : PIN_CLEAR(p))

#define RCC_AHB2ENR1      *((volatile UINT32 *)(0x40030C00 + 0x8C))

/* InitializeFlag: Clocks the port of a pin and makes the pin an output, off. */
static inline void InitializeFlag(UINT8 pin)
{
  RCC_AHB2ENR1 |= 1u << PIN_PORT(pin); // GPIOAEN to GPIOHEN are bits 0 to 7 (p. 443)
  (void)RCC_AHB2ENR1;                  // 2 cycles of the bus before the port (p. 410)
  ClearPin(pin);
  GPIO_MODER(pin) = (GPIO_MODER(pin) & ~(3u << 2 * PIN_NUMBER(pin))) |
                    1u << 2 * PIN_NUMBER(pin);
}

#endif /* BOARD_U3_H */
