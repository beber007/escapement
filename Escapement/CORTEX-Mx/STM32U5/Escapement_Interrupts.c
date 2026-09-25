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
** after the Cortex-Mx system exceptions; the entries of the peripherals the port drives
** route to _OSIOHandler, which reads the exception number and dispatches through the
** second table, _OSTabDevice. Transposed from the RP2350 port: 126 interrupts, numbered
** as in Escapement_Interrupts.h.
** Platform version: STM32U575 (NUCLEO-U575ZI-Q).
*/

#include "Escapement.h"

extern void _OSIOHandler(void);

#ifdef DEBUG_MODE
   /* An interrupt was raised for a peripheral that has no descriptor installed. */
   static void UndefinedInterrupt(void)
   {
     _OSDisableInterrupts();
     while (TRUE);
   } /* end of UndefinedInterrupt */
#else
   #define UndefinedInterrupt _OSIOHandler
#endif


/* STM32U575 vector table, appended to CortexMxVectorTable defined in
** Escapement_CortexMx.c. The timers and USART1 are those of Escapement_Timer.c,
** Escapement_TimerEvent.c and Escapement_UART.c. */
__attribute__ ((section(".isr_vector_specific")))
void (* const STM32U5VectorTable[])(void) = {
  UndefinedInterrupt, /*   0  WWDG                  */
  UndefinedInterrupt, /*   1  PVD_PVM               */
  UndefinedInterrupt, /*   2  RTC                   */
  UndefinedInterrupt, /*   3  RTC_S                 */
  UndefinedInterrupt, /*   4  TAMP                  */
  UndefinedInterrupt, /*   5  RAMCFG                */
  UndefinedInterrupt, /*   6  FLASH                 */
  UndefinedInterrupt, /*   7  FLASH_S               */
  UndefinedInterrupt, /*   8  GTZC                  */
  UndefinedInterrupt, /*   9  RCC                   */
  UndefinedInterrupt, /*  10  RCC_S                 */
  UndefinedInterrupt, /*  11  EXTI0                 */
  UndefinedInterrupt, /*  12  EXTI1                 */
  UndefinedInterrupt, /*  13  EXTI2                 */
  UndefinedInterrupt, /*  14  EXTI3                 */
  UndefinedInterrupt, /*  15  EXTI4                 */
  UndefinedInterrupt, /*  16  EXTI5                 */
  UndefinedInterrupt, /*  17  EXTI6                 */
  UndefinedInterrupt, /*  18  EXTI7                 */
  UndefinedInterrupt, /*  19  EXTI8                 */
  UndefinedInterrupt, /*  20  EXTI9                 */
  UndefinedInterrupt, /*  21  EXTI10                */
  UndefinedInterrupt, /*  22  EXTI11                */
  UndefinedInterrupt, /*  23  EXTI12                */
  UndefinedInterrupt, /*  24  EXTI13                */
  UndefinedInterrupt, /*  25  EXTI14                */
  UndefinedInterrupt, /*  26  EXTI15                */
  UndefinedInterrupt, /*  27  IWDG                  */
  UndefinedInterrupt, /*  28  reserved              */
  UndefinedInterrupt, /*  29  GPDMA1_Channel0       */
  UndefinedInterrupt, /*  30  GPDMA1_Channel1       */
  UndefinedInterrupt, /*  31  GPDMA1_Channel2       */
  UndefinedInterrupt, /*  32  GPDMA1_Channel3       */
  UndefinedInterrupt, /*  33  GPDMA1_Channel4       */
  UndefinedInterrupt, /*  34  GPDMA1_Channel5       */
  UndefinedInterrupt, /*  35  GPDMA1_Channel6       */
  UndefinedInterrupt, /*  36  GPDMA1_Channel7       */
  UndefinedInterrupt, /*  37  ADC1                  */
  UndefinedInterrupt, /*  38  DAC1                  */
  UndefinedInterrupt, /*  39  FDCAN1_IT0            */
  UndefinedInterrupt, /*  40  FDCAN1_IT1            */
  UndefinedInterrupt, /*  41  TIM1_BRK              */
  UndefinedInterrupt, /*  42  TIM1_UP               */
  UndefinedInterrupt, /*  43  TIM1_TRG_COM          */
  UndefinedInterrupt, /*  44  TIM1_CC               */
  _OSIOHandler,       /*  45  TIM2                  */
  UndefinedInterrupt, /*  46  TIM3                  */
  UndefinedInterrupt, /*  47  TIM4                  */
  _OSIOHandler,       /*  48  TIM5                  */
  UndefinedInterrupt, /*  49  TIM6                  */
  UndefinedInterrupt, /*  50  TIM7                  */
  UndefinedInterrupt, /*  51  TIM8_BRK              */
  UndefinedInterrupt, /*  52  TIM8_UP               */
  UndefinedInterrupt, /*  53  TIM8_TRG_COM          */
  UndefinedInterrupt, /*  54  TIM8_CC               */
  UndefinedInterrupt, /*  55  I2C1_EV               */
  UndefinedInterrupt, /*  56  I2C1_ER               */
  UndefinedInterrupt, /*  57  I2C2_EV               */
  UndefinedInterrupt, /*  58  I2C2_ER               */
  UndefinedInterrupt, /*  59  SPI1                  */
  UndefinedInterrupt, /*  60  SPI2                  */
  _OSIOHandler,       /*  61  USART1                */
  UndefinedInterrupt, /*  62  USART2                */
  UndefinedInterrupt, /*  63  USART3                */
  UndefinedInterrupt, /*  64  UART4                 */
  UndefinedInterrupt, /*  65  UART5                 */
  UndefinedInterrupt, /*  66  LPUART1               */
  UndefinedInterrupt, /*  67  LPTIM1                */
  UndefinedInterrupt, /*  68  LPTIM2                */
  UndefinedInterrupt, /*  69  TIM15                 */
  UndefinedInterrupt, /*  70  TIM16                 */
  UndefinedInterrupt, /*  71  TIM17                 */
  UndefinedInterrupt, /*  72  COMP                  */
  UndefinedInterrupt, /*  73  OTG_FS                */
  UndefinedInterrupt, /*  74  CRS                   */
  UndefinedInterrupt, /*  75  FMC                   */
  UndefinedInterrupt, /*  76  OCTOSPI1              */
  UndefinedInterrupt, /*  77  PWR_S3WU              */
  UndefinedInterrupt, /*  78  SDMMC1                */
  UndefinedInterrupt, /*  79  SDMMC2                */
  UndefinedInterrupt, /*  80  GPDMA1_Channel8       */
  UndefinedInterrupt, /*  81  GPDMA1_Channel9       */
  UndefinedInterrupt, /*  82  GPDMA1_Channel10      */
  UndefinedInterrupt, /*  83  GPDMA1_Channel11      */
  UndefinedInterrupt, /*  84  GPDMA1_Channel12      */
  UndefinedInterrupt, /*  85  GPDMA1_Channel13      */
  UndefinedInterrupt, /*  86  GPDMA1_Channel14      */
  UndefinedInterrupt, /*  87  GPDMA1_Channel15      */
  UndefinedInterrupt, /*  88  I2C3_EV               */
  UndefinedInterrupt, /*  89  I2C3_ER               */
  UndefinedInterrupt, /*  90  SAI1                  */
  UndefinedInterrupt, /*  91  SAI2                  */
  UndefinedInterrupt, /*  92  TSC                   */
  UndefinedInterrupt, /*  93  reserved              */
  UndefinedInterrupt, /*  94  RNG                   */
  UndefinedInterrupt, /*  95  FPU                   */
  UndefinedInterrupt, /*  96  HASH                  */
  UndefinedInterrupt, /*  97  reserved              */
  UndefinedInterrupt, /*  98  LPTIM3                */
  UndefinedInterrupt, /*  99  SPI3                  */
  UndefinedInterrupt, /* 100  I2C4_ER               */
  UndefinedInterrupt, /* 101  I2C4_EV               */
  UndefinedInterrupt, /* 102  MDF1_FLT0             */
  UndefinedInterrupt, /* 103  MDF1_FLT1             */
  UndefinedInterrupt, /* 104  MDF1_FLT2             */
  UndefinedInterrupt, /* 105  MDF1_FLT3             */
  UndefinedInterrupt, /* 106  UCPD1                 */
  UndefinedInterrupt, /* 107  ICACHE                */
  UndefinedInterrupt, /* 108  reserved              */
  UndefinedInterrupt, /* 109  reserved              */
  UndefinedInterrupt, /* 110  LPTIM4                */
  UndefinedInterrupt, /* 111  DCACHE1               */
  UndefinedInterrupt, /* 112  ADF1                  */
  UndefinedInterrupt, /* 113  ADC4                  */
  UndefinedInterrupt, /* 114  LPDMA1_Channel0       */
  UndefinedInterrupt, /* 115  LPDMA1_Channel1       */
  UndefinedInterrupt, /* 116  LPDMA1_Channel2       */
  UndefinedInterrupt, /* 117  LPDMA1_Channel3       */
  UndefinedInterrupt, /* 118  DMA2D                 */
  UndefinedInterrupt, /* 119  DCMI_PSSI             */
  UndefinedInterrupt, /* 120  OCTOSPI2              */
  UndefinedInterrupt, /* 121  MDF1_FLT4             */
  UndefinedInterrupt, /* 122  MDF1_FLT5             */
  UndefinedInterrupt, /* 123  CORDIC                */
  UndefinedInterrupt, /* 124  FMAC                  */
  UndefinedInterrupt  /* 125  LSECSSD               */
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
