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
** Authors: MIS-TIC
**
** Escapement - Lightweight Power-Aware Real-Time OS, derived from ZottaOS.
** Modifications Copyright (c) 2026 Bertrand Hurst, distributed under the same terms;
** see LICENSE and NOTICE at the root of this repository.
*/
/* File Escapement_Interrupts.c: Defines the two tables that bind a peripheral interrupt
** to its handler. The first is the vector table proper, appended by the linker right
** after the Cortex-Mx system exceptions; the entries of the chip route to _OSIOHandler,
** which reads the exception number and dispatches through the second table,
** _OSTabDevice. Transposed from the STM32U5 port: 125 interrupts, numbered as in
** Escapement_Interrupts.h.
** Platform version: STM32U385 (NUCLEO-U385RG-Q), any STM32U375/385.
*/

#include "Escapement.h"

extern void _OSIOHandler(void);

/* STM32U385 vector table, appended to CortexMxVectorTable defined in
** Escapement_CortexMx.c (RM0487 rev. 3, table 134). Every entry goes to the dispatcher,
** the reserved ones too, so that an application can take any interrupt with
** OSSetISRDescriptor, the port's drivers the RCC, TIM2, TIM4 and USART1; the dispatcher
** traps one without a descriptor under DEBUG_MODE, which is all a reserved line, never
** raised, could reach. */
__attribute__ ((section(".isr_vector_specific")))
void (* const STM32U3VectorTable[])(void) = {
  _OSIOHandler,       /*   0  WWDG                  */
  _OSIOHandler,       /*   1  PVD_PVM               */
  _OSIOHandler,       /*   2  RTC                   */
  _OSIOHandler,       /*   3  RTC_S                 */
  _OSIOHandler,       /*   4  TAMP                  */
  _OSIOHandler,       /*   5  RAMCFG                */
  _OSIOHandler,       /*   6  FLASH                 */
  _OSIOHandler,       /*   7  FLASH_S               */
  _OSIOHandler,       /*   8  GTZC                  */
  _OSIOHandler,       /*   9  RCC                   */
  _OSIOHandler,       /*  10  RCC_S                 */
  _OSIOHandler,       /*  11  EXTI0                 */
  _OSIOHandler,       /*  12  EXTI1                 */
  _OSIOHandler,       /*  13  EXTI2                 */
  _OSIOHandler,       /*  14  EXTI3                 */
  _OSIOHandler,       /*  15  EXTI4                 */
  _OSIOHandler,       /*  16  EXTI5                 */
  _OSIOHandler,       /*  17  EXTI6                 */
  _OSIOHandler,       /*  18  EXTI7                 */
  _OSIOHandler,       /*  19  EXTI8                 */
  _OSIOHandler,       /*  20  EXTI9                 */
  _OSIOHandler,       /*  21  EXTI10                */
  _OSIOHandler,       /*  22  EXTI11                */
  _OSIOHandler,       /*  23  EXTI12                */
  _OSIOHandler,       /*  24  EXTI13                */
  _OSIOHandler,       /*  25  EXTI14                */
  _OSIOHandler,       /*  26  EXTI15                */
  _OSIOHandler,       /*  27  IWDG                  */
  _OSIOHandler,       /*  28  SAES                  */
  _OSIOHandler,       /*  29  GPDMA1_CH0            */
  _OSIOHandler,       /*  30  GPDMA1_CH1            */
  _OSIOHandler,       /*  31  GPDMA1_CH2            */
  _OSIOHandler,       /*  32  GPDMA1_CH3            */
  _OSIOHandler,       /*  33  GPDMA1_CH4            */
  _OSIOHandler,       /*  34  GPDMA1_CH5            */
  _OSIOHandler,       /*  35  GPDMA1_CH6            */
  _OSIOHandler,       /*  36  GPDMA1_CH7            */
  _OSIOHandler,       /*  37  ADC1                  */
  _OSIOHandler,       /*  38  DAC1                  */
  _OSIOHandler,       /*  39  FDCAN1_IT0            */
  _OSIOHandler,       /*  40  FDCAN1_IT1            */
  _OSIOHandler,       /*  41  TIM1_BRK_TERR_IERR    */
  _OSIOHandler,       /*  42  TIM1_UP               */
  _OSIOHandler,       /*  43  TIM1_TRG_COM_DIR_IDX  */
  _OSIOHandler,       /*  44  TIM1_CC               */
  _OSIOHandler,       /*  45  TIM2                  */
  _OSIOHandler,       /*  46  TIM3                  */
  _OSIOHandler,       /*  47  TIM4                  */
  _OSIOHandler,       /*  48  reserved              */
  _OSIOHandler,       /*  49  TIM6                  */
  _OSIOHandler,       /*  50  TIM7                  */
  _OSIOHandler,       /*  51  TIM12                 */
  _OSIOHandler,       /*  52  reserved              */
  _OSIOHandler,       /*  53  I3C1_EV               */
  _OSIOHandler,       /*  54  I3C1_ER               */
  _OSIOHandler,       /*  55  I2C1_EV               */
  _OSIOHandler,       /*  56  I2C1_ER               */
  _OSIOHandler,       /*  57  I2C2_EV               */
  _OSIOHandler,       /*  58  I2C2_ER               */
  _OSIOHandler,       /*  59  SPI1                  */
  _OSIOHandler,       /*  60  SPI2                  */
  _OSIOHandler,       /*  61  USART1                */
  _OSIOHandler,       /*  62  USART2                */
  _OSIOHandler,       /*  63  USART3                */
  _OSIOHandler,       /*  64  UART4                 */
  _OSIOHandler,       /*  65  UART5                 */
  _OSIOHandler,       /*  66  LPUART1               */
  _OSIOHandler,       /*  67  LPTIM1                */
  _OSIOHandler,       /*  68  LPTIM2                */
  _OSIOHandler,       /*  69  TIM15                 */
  _OSIOHandler,       /*  70  TIM16                 */
  _OSIOHandler,       /*  71  TIM17                 */
  _OSIOHandler,       /*  72  COMP                  */
  _OSIOHandler,       /*  73  USB                   */
  _OSIOHandler,       /*  74  CRS                   */
  _OSIOHandler,       /*  75  reserved              */
  _OSIOHandler,       /*  76  OCTOSPI1              */
  _OSIOHandler,       /*  77  HSP1                  */
  _OSIOHandler,       /*  78  SDMMC1                */
  _OSIOHandler,       /*  79  reserved              */
  _OSIOHandler,       /*  80  GPDMA1_CH8            */
  _OSIOHandler,       /*  81  GPDMA1_CH9            */
  _OSIOHandler,       /*  82  GPDMA1_CH10           */
  _OSIOHandler,       /*  83  GPDMA1_CH11           */
  _OSIOHandler,       /*  84  reserved              */
  _OSIOHandler,       /*  85  reserved              */
  _OSIOHandler,       /*  86  reserved              */
  _OSIOHandler,       /*  87  reserved              */
  _OSIOHandler,       /*  88  I2C3_EV               */
  _OSIOHandler,       /*  89  I2C3_ER               */
  _OSIOHandler,       /*  90  SAI1                  */
  _OSIOHandler,       /*  91  reserved              */
  _OSIOHandler,       /*  92  TSC                   */
  _OSIOHandler,       /*  93  AES                   */
  _OSIOHandler,       /*  94  RNG                   */
  _OSIOHandler,       /*  95  FPU                   */
  _OSIOHandler,       /*  96  HASH                  */
  _OSIOHandler,       /*  97  PKA                   */
  _OSIOHandler,       /*  98  LPTIM3                */
  _OSIOHandler,       /*  99  SPI3                  */
  _OSIOHandler,       /* 100  I3C2_EV               */
  _OSIOHandler,       /* 101  I3C2_ER               */
  _OSIOHandler,       /* 102  TIM8_BRK_TERR_IERR    */
  _OSIOHandler,       /* 103  TIM8_UP               */
  _OSIOHandler,       /* 104  TIM8_TRG_COM_DIR_IDX  */
  _OSIOHandler,       /* 105  TIM8_CC               */
  _OSIOHandler,       /* 106  reserved              */
  _OSIOHandler,       /* 107  ICACHE                */
  _OSIOHandler,       /* 108  reserved              */
  _OSIOHandler,       /* 109  LCD                   */
  _OSIOHandler,       /* 110  LPTIM4                */
  _OSIOHandler,       /* 111  reserved              */
  _OSIOHandler,       /* 112  ADF1                  */
  _OSIOHandler,       /* 113  ADC2                  */
  _OSIOHandler,       /* 114  FDCAN2_IT0            */
  _OSIOHandler,       /* 115  FDCAN2_IT1            */
  _OSIOHandler,       /* 116  I2C4_EV               */
  _OSIOHandler,       /* 117  I2C4_ER               */
  _OSIOHandler,       /* 118  reserved              */
  _OSIOHandler,       /* 119  SPI4                  */
  _OSIOHandler,       /* 120  reserved              */
  _OSIOHandler,       /* 121  reserved              */
  _OSIOHandler,       /* 122  reserved              */
  _OSIOHandler,       /* 123  PWR                   */
  _OSIOHandler        /* 124  PWR_S                 */
};


/* Table of ISR descriptors, controlled by OSSetISRDescriptor and OSGetISRDescriptor and
** read by _OSIOHandler. */
void *_OSTabDevice[OS_IO_NB_ENTRIES] = { 0 };


/* OSSetISRDescriptor: Associates an ISR descriptor with a vector table entry.
** Parameters:
**   (1) (UINT16) index of the entry, see the OS_IO_xxx definitions;
**   (2) (void *) pointer to the descriptor, whose first field must be the handler. */
void OSSetISRDescriptor(UINT16 entry, void *descriptor)
{
  _OSTabDevice[entry] = descriptor;
} /* end of OSSetISRDescriptor */


/* OSGetISRDescriptor: Returns the descriptor associated with a vector table entry.
** Parameter: (UINT16) index of the entry, see the OS_IO_xxx definitions.
** Returned value: (void *) the descriptor, or NULL when none was installed. */
void *OSGetISRDescriptor(UINT16 entry)
{
  return _OSTabDevice[entry];
} /* end of OSGetISRDescriptor */
