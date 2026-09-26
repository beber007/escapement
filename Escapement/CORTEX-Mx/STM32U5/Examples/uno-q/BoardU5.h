/* Copyright (c) 2026 Bertrand Hurst. All rights reserved.
** Escapement - Lightweight Power-Aware Real-Time OS.
** Distributed under the terms of LICENSE at the root of this repository.
*/
/* File BoardU5.h: The outputs the examples drive on the Arduino UNO Q, and the GPIO
** registers of its STM32U585 they need (RM0456, GPIO and RCC).
**
** A pin is named by its port and number, PIN(port,n). The first two outputs are the
** green of LED3 on PH11 and the blue of LED4 on PH15, which light on a low level; the
** other two come out on PB13 and PB14, D13 and D12 of the connector (datasheet
** ABX00162/ABX00173, 5 UI & Indicators and 9.6 JDIGITAL). PIN_LOW marks a pin that is on
** when low: SetPin turns it on all the same.
** Platform version: STM32U585 (Arduino UNO Q).
*/

#ifndef BOARD_U5_H
#define BOARD_U5_H

#define PORT_B 1
#define PORT_H 7
#define PIN(port,n)       ((UINT8)((port) << 4 | (n)))
#define PIN_LOW(port,n)   ((UINT8)(0x80 | (port) << 4 | (n)))   /* on when low */

#define FLAG1_PIN         PIN_LOW(PORT_H,11)   /* LED3, green */
#define FLAG2_PIN         PIN_LOW(PORT_H,15)   /* LED4, blue */
#define FLAG3_PIN         PIN(PORT_B,13)       /* D13 */
#define PROBE_PIN         PIN(PORT_B,14)       /* D12, measurement probe */

/* Ports on AHB2, 0x400 apart. MODER takes 01 for an output; BSRR sets a pin with the low
** half of the word and clears it with the high half, in one write that no interrupt can
** split. */
#define PIN_PORT(p)       (((p) >> 4) & 7)     /* without the mark of PIN_LOW */
#define GPIO_BASE(p)      (0x42020000 + 0x400 * PIN_PORT(p))
#define GPIO_MODER(p)     *((volatile UINT32 *)(GPIO_BASE(p) + 0x00))
#define GPIO_BSRR(p)      *((volatile UINT32 *)(GPIO_BASE(p) + 0x18))
#define PIN_NUMBER(p)     ((p) & 0xF)
#define PIN_HIGH(p)       (1u << PIN_NUMBER(p))
#define PIN_CLEAR(p)      (1u << (PIN_NUMBER(p) + 16))
#define SetPin(p)         (GPIO_BSRR(p) = ((p) & 0x80) ? PIN_CLEAR(p) : PIN_HIGH(p))
#define ClearPin(p)       (GPIO_BSRR(p) = ((p) & 0x80) ? PIN_HIGH(p) : PIN_CLEAR(p))

#define RCC_AHB2ENR1      *((volatile UINT32 *)(0x46020C00 + 0x8C))

/* InitializeFlag: Clocks the port of a pin and makes the pin an output, off. */
static inline void InitializeFlag(UINT8 pin)
{
  RCC_AHB2ENR1 |= 1u << PIN_PORT(pin); // GPIOAEN to GPIOHEN are bits 0 to 7
  (void)RCC_AHB2ENR1;                  // the clock runs before the port is written
  ClearPin(pin);
  GPIO_MODER(pin) = (GPIO_MODER(pin) & ~(3u << 2 * PIN_NUMBER(pin))) |
                    1u << 2 * PIN_NUMBER(pin);
}

#endif /* BOARD_U5_H */
