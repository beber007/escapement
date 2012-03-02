/* Copyright (c) 2006-2012 MIS Institute of the HEIG affiliated to the University of
** Applied Sciences of Western Switzerland. All rights reserved.
** IN NO EVENT SHALL THE MIS INSTITUTE NOR THE HEIG NOR THE UNIVERSITY OF APPLIED
** SCIENCES OF WESTERN SWITZERLAND BE LIABLE TO ANY PARTY FOR DIRECT, INDIRECT, SPECIAL,
** INCIDENTAL, OR CONSEQUENTIAL DAMAGES ARISING OUT OF THE USE OF THIS SOFTWARE AND ITS
** DOCUMENTATION, EVEN IF THE MIS INSTITUTE OR THE HEIG OR THE UNIVERSITY OF APPLIED
** SCIENCES OF WESTERN SWITZERLAND HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
** THE MIS INSTITUTE, THE HEIG AND THE UNIVERSITY OF APPLIED SCIENCES OF WESTERN SWITZER-
** LAND SPECIFICALLY DISCLAIMS ANY WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
** WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE. THE SOFTWARE PRO-
** VIDED HEREUNDER IS ON AN "AS IS" BASIS, AND THE MIS INSTITUTE NOR THE HEIG AND NOR THE
** UNIVERSITY OF APPLIED SCIENCES OF WESTERN SWITZERLAND HAVE NO OBLIGATION TO PROVIDE
** MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR MODIFICATIONS.
**
** File ZottaOS_Interrupts.c:
** Version date: January 2012
** Authors: MIS-TIC
*/

/* TODO: prendre en compte la taille max de la table des vecteur pour fixer la taille de _OSTabDevice */

#include "ZottaOS_CortexM3.h"
#include "ZottaOS_Timer.h"


/* Table of devices used by the interrupt handler and the peripheral devices to obtain
** the appropriate I/O routine. */
void *_OSTabDevice[68];

/* STM-32 specific interrupt vector table, which is added by the linker at the end of
** table CortextM3VectorTable which is defined in NTRTOS_CortexM3.c. */
__attribute__ ((section(".isr_vector_specific")))
void (* const STM32VectorTable[])(void) =
{
  _OSIOHandler,    /* OS_IO_WWDG                0  Window WatchDog Interrupt */
  _OSIOHandler,    /* OS_IO_PVD                 1  PVD through EXTI Line detection
                                                      Interrupt */
  _OSIOHandler,    /* OS_IO_TAMPER              2  Tamper Interrupt */
  _OSIOHandler,    /* OS_IO_RTC                 3  RTC global Interrupt */
  _OSIOHandler,    /* OS_IO_FLASH               4  FLASH global Interrupt */
  _OSIOHandler,    /* OS_IO_RCC                 5  RCC global Interrupt */
  _OSIOHandler,    /* OS_IO_EXTI0               6  EXTI Line0 Interrupt */
  _OSIOHandler,    /* OS_IO_EXTI1               7  EXTI Line1 Interrupt */
  _OSIOHandler,    /* OS_IO_EXTI2               8  EXTI Line2 Interrupt */
  _OSIOHandler,    /* OS_IO_EXTI3               9  EXTI Line3 Interrupt */
  _OSIOHandler,    /* OS_IO_EXTI4              10  EXTI Line4 Interrupt */
  _OSIOHandler,    /* OS_IO_DMA1_Channel1      11  DMA1 Channel 1 global Interrupt */
  _OSIOHandler,    /* OS_IO_DMA1_Channel2      12  DMA1 Channel 2 global Interrupt */
  _OSIOHandler,    /* OS_IO_DMA1_Channel3      13  DMA1 Channel 3 global Interrupt */
  _OSIOHandler,    /* OS_IO_DMA1_Channel4      14  DMA1 Channel 4 global Interrupt */
  _OSIOHandler,    /* OS_IO_DMA1_Channel5      15  DMA1 Channel 5 global Interrupt */
  _OSIOHandler,    /* OS_IO_DMA1_Channel6      16  DMA1 Channel 6 global Interrupt */
  _OSIOHandler,    /* OS_IO_DMA1_Channel7      17  DMA1 Channel 7 global Interrupt */
  _OSIOHandler,    /* OS_IO_ADC1_2             18  ADC1 and ADC2 global Interrupt */
  _OSIOHandler,    /* OS_IO_CAN1_TX            19  USB Device High Priority or CAN1 TX
                                                      Interrupts */
  _OSIOHandler,    /* OS_IO_CAN1_RX0           20  USB Device Low Priority or CAN1 RX0
                                                      Interrupts */
  _OSIOHandler,    /* OS_IO_CAN1_RX1           21  CAN1 RX1 Interrupt */
  _OSIOHandler,    /* OS_IO_CAN1_SCE           22  CAN1 SCE Interrupt */
  _OSIOHandler,    /* OS_IO_EXTI9_5            23  External Line[9:5] Interrupts */
  _OSIOHandler,    /* OS_IO_TIM1_BRK           24  TIM1 Break Interrupt */
  _OSIOHandler,    /* OS_IO_TIM1_UP            25  TIM1 Update Interrupt */
  _OSIOHandler,    /* OS_IO_TIM1_TRG_COM       26  TIM1 Trigger and Commutation
                                                      Interrupt */
  _OSIOHandler,    /* OS_IO_TIM1_CC            27  TIM1 Capture Compare Interrupt */
  _OSIOHandler,    /* OS_IO_TIM2               28  TIM2 global Interrupt */
  _OSIOHandler,    /* OS_IO_TIM3               29  TIM3 global Interrupt */
  _OSTimerHandler, /* OS_IO_TIM4               30  TIM4 global Interrupt */
  _OSIOHandler,    /* OS_IO_I2C1_EV            31  I2C1 Event Interrupt */
  _OSIOHandler,    /* OS_IO_I2C1_ER            32  I2C1 Error Interrupt */
  _OSIOHandler,    /* OS_IO_I2C2_EV            33  I2C2 Event Interrupt */
  _OSIOHandler,    /* OS_IO_I2C2_ER            34  I2C2 Error Interrupt */
  _OSIOHandler,    /* OS_IO_SPI1               35  SPI1 global Interrupt */
  _OSIOHandler,    /* OS_IO_SPI2               36  SPI2 global Interrupt */
  _OSIOHandler,    /* OS_IO_USART1             37  USART1 global Interrupt */
  _OSIOHandler,    /* OS_IO_USART2             38  USART2 global Interrupt */
  _OSIOHandler,    /* OS_IO_USART3             39  USART3 global Interrupt */
  _OSIOHandler,    /* OS_IO_EXTI15_10          40  External Line[15:10] Interrupts */
  _OSIOHandler,    /* OS_IO_RTCAlarm           41  RTC Alarm through EXTI Line
                                                      Interrupt */
  _OSIOHandler,    /* OS_IO_OTG_FS_WKUP        42  USB OTG FS WakeUp from suspend through
                                                      EXTI Line Interrupt */
  _OSIOHandler,    /* OS_IO_TIM8_BRK_TIM12     43  TIM8 Break Interrupt and TIM12 global
                                                      Interrupt */
  _OSIOHandler,    /* OS_IO_TIM8_UP_TIM13      44  TIM8 Update Interrupt and TIM13 global
                                                      Interrupt */
  _OSIOHandler,    /* OS_IO_TIM8_TRG_COM_TIM14 45  TIM8 Trigger and commutation Interrupt
                                                      and TIM14 global interrupt */
  _OSIOHandler,    /* OS_IO_TIM8_CC            46  TIM8 Capture Compare Interrupt */
  _OSIOHandler,    /* OS_IO_ADC3               47  ADC3 global Interrupt */
  _OSIOHandler,    /* OS_IO_FSMC               48  FSMC global Interrupt */
  _OSIOHandler,    /* OS_IO_SDIO               49  SDIO global Interrupt */
  _OSIOHandler,    /* OS_IO_TIM5               50  TIM5 global Interrupt */
  _OSIOHandler,    /* OS_IO_SPI3               51  SPI3 global Interrupt */
  _OSIOHandler,    /* OS_IO_UART4              52  UART4 global Interrupt */
  _OSIOHandler,    /* OS_IO_UART5              53  UART5 global Interrupt */
  _OSIOHandler,    /* OS_IO_TIM6               54  TIM6 global Interrupt */
  _OSIOHandler,    /* OS_IO_TIM7               55  TIM7 global Interrupt */
  _OSIOHandler,    /* OS_IO_DMA2_Channel1      56  DMA2 Channel 1 global Interrupt */
  _OSIOHandler,    /* OS_IO_DMA2_Channel2      57  DMA2 Channel 2 global Interrupt */
  _OSIOHandler,    /* OS_IO_DMA2_Channel3      58  DMA2 Channel 3 global Interrupt */
  _OSIOHandler,    /* OS_IO_DMA2_Channel4      59  DMA2 Channel 4 global Interrupt */
  _OSIOHandler,    /* OS_IO_DMA2_Channel5      60  DMA2 Channel 5 global Interrupt */
  _OSIOHandler,    /* OS_IO_ETH                61  Ethernet global Interrupt */
  _OSIOHandler,    /* OS_IO_ETH_WKUP           62  Ethernet wake-up through EXTI line
                                                      Interrupt */
  _OSIOHandler,    /* OS_IO_CAN2_TX            63  CAN2 TX Interrupt */
  _OSIOHandler,    /* OS_IO_CAN2_RX0           64  CAN2 RX0 Interrupt */
  _OSIOHandler,    /* OS_IO_CAN2_RX1           65  CAN2 RX1 Interrupt */
  _OSIOHandler,    /* OS_IO_CAN2_SCE           66  CAN2 SCE Interrupt */
  _OSIOHandler     /* OS_IO_OTG_FS             67  USB OTG FS global Interrupt */
};
