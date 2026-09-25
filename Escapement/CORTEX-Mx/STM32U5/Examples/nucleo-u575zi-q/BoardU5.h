/* Copyright (c) 2026 Bertrand Hurst. All rights reserved.
** Escapement - Lightweight Power-Aware Real-Time OS.
** Distributed under the terms of LICENSE at the root of this repository.
*/
/* File BoardU5.h: The outputs the examples drive on the NUCLEO-U575ZI-Q, and the GPIO
** registers of the STM32U575 they need (RM0456, GPIO and RCC).
**
** A pin is named by its port and number, PIN(port,n). The first two outputs are the
** green and blue LEDs of the board, LD1 on PC7 and LD2 on PB7; the other two come out on
** PA5 and PA6, D13 and D12 of the Arduino connector. LD3, the red LED, is on port G, which
** needs VDDIO2 to be declared valid first, and is left alone.
** Platform version: STM32U575 (NUCLEO-U575ZI-Q).
*/

#ifndef BOARD_U5_H
#define BOARD_U5_H

#define PORT_A 0
#define PORT_B 1
#define PORT_C 2
#define PIN(port,n)       ((UINT8)((port) << 4 | (n)))

#define FLAG1_PIN         PIN(PORT_C,7)   /* LD1, green */
#define FLAG2_PIN         PIN(PORT_B,7)   /* LD2, blue */
#define FLAG3_PIN         PIN(PORT_A,5)   /* D13 */
#define PROBE_PIN         PIN(PORT_A,6)   /* D12, measurement probe */

/* Ports on AHB2, 0x400 apart. MODER takes 01 for an output; BSRR sets a pin with the low
** half of the word and resets it with the high half, in one store that no interrupt can
** split. */
#define GPIO_BASE(p)      (0x42020000 + 0x400 * ((p) >> 4))
#define GPIO_MODER(p)     *((volatile UINT32 *)(GPIO_BASE(p) + 0x00))
#define GPIO_BSRR(p)      *((volatile UINT32 *)(GPIO_BASE(p) + 0x18))
#define PIN_NUMBER(p)     ((p) & 0xF)
#define SetPin(p)         (GPIO_BSRR(p) = 1u << PIN_NUMBER(p))
#define ClearPin(p)       (GPIO_BSRR(p) = 1u << (PIN_NUMBER(p) + 16))

#define RCC_AHB2ENR1      *((volatile UINT32 *)(0x46020C00 + 0x8C))

/* InitializeFlag: Clocks the port of a pin and makes the pin a low output. */
static inline void InitializeFlag(UINT8 pin)
{
  RCC_AHB2ENR1 |= 1u << (pin >> 4);   // GPIOAEN, GPIOBEN, GPIOCEN are bits 0, 1, 2
  (void)RCC_AHB2ENR1;                 // the clock runs before the port is written
  ClearPin(pin);
  GPIO_MODER(pin) = (GPIO_MODER(pin) & ~(3u << 2 * PIN_NUMBER(pin))) |
                    1u << 2 * PIN_NUMBER(pin);
}

#endif /* BOARD_U5_H */
