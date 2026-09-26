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
** _OSTabDevice. Transposed from the RP2350 port: 126 interrupts, numbered as in
** Escapement_Interrupts.h.
** Platform version: STM32U585 (Arduino UNO Q), any STM32U5.
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
** Escapement_CortexMx.c. Every interrupt of the chip goes to the dispatcher, so that an
** application can take any of them with OSSetISRDescriptor, the port's drivers TIM2,
** TIM5 and USART1; the dispatcher traps one without a descriptor under DEBUG_MODE. The
** reserved entries alone go to UndefinedInterrupt. */
__attribute__ ((section(".isr_vector_specific")))
void (* const STM32U5VectorTable[])(void) = {
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
  UndefinedInterrupt, /*  28  reserved              */
  _OSIOHandler,       /*  29  GPDMA1_Channel0       */
  _OSIOHandler,       /*  30  GPDMA1_Channel1       */
  _OSIOHandler,       /*  31  GPDMA1_Channel2       */
  _OSIOHandler,       /*  32  GPDMA1_Channel3       */
  _OSIOHandler,       /*  33  GPDMA1_Channel4       */
  _OSIOHandler,       /*  34  GPDMA1_Channel5       */
  _OSIOHandler,       /*  35  GPDMA1_Channel6       */
  _OSIOHandler,       /*  36  GPDMA1_Channel7       */
  _OSIOHandler,       /*  37  ADC1                  */
  _OSIOHandler,       /*  38  DAC1                  */
  _OSIOHandler,       /*  39  FDCAN1_IT0            */
  _OSIOHandler,       /*  40  FDCAN1_IT1            */
  _OSIOHandler,       /*  41  TIM1_BRK              */
  _OSIOHandler,       /*  42  TIM1_UP               */
  _OSIOHandler,       /*  43  TIM1_TRG_COM          */
  _OSIOHandler,       /*  44  TIM1_CC               */
  _OSIOHandler,       /*  45  TIM2                  */
  _OSIOHandler,       /*  46  TIM3                  */
  _OSIOHandler,       /*  47  TIM4                  */
  _OSIOHandler,       /*  48  TIM5                  */
  _OSIOHandler,       /*  49  TIM6                  */
  _OSIOHandler,       /*  50  TIM7                  */
  _OSIOHandler,       /*  51  TIM8_BRK              */
  _OSIOHandler,       /*  52  TIM8_UP               */
  _OSIOHandler,       /*  53  TIM8_TRG_COM          */
  _OSIOHandler,       /*  54  TIM8_CC               */
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
  _OSIOHandler,       /*  73  OTG_FS                */
  _OSIOHandler,       /*  74  CRS                   */
  _OSIOHandler,       /*  75  FMC                   */
  _OSIOHandler,       /*  76  OCTOSPI1              */
  _OSIOHandler,       /*  77  PWR_S3WU              */
  _OSIOHandler,       /*  78  SDMMC1                */
  _OSIOHandler,       /*  79  SDMMC2                */
  _OSIOHandler,       /*  80  GPDMA1_Channel8       */
  _OSIOHandler,       /*  81  GPDMA1_Channel9       */
  _OSIOHandler,       /*  82  GPDMA1_Channel10      */
  _OSIOHandler,       /*  83  GPDMA1_Channel11      */
  _OSIOHandler,       /*  84  GPDMA1_Channel12      */
  _OSIOHandler,       /*  85  GPDMA1_Channel13      */
  _OSIOHandler,       /*  86  GPDMA1_Channel14      */
  _OSIOHandler,       /*  87  GPDMA1_Channel15      */
  _OSIOHandler,       /*  88  I2C3_EV               */
  _OSIOHandler,       /*  89  I2C3_ER               */
  _OSIOHandler,       /*  90  SAI1                  */
  _OSIOHandler,       /*  91  SAI2                  */
  _OSIOHandler,       /*  92  TSC                   */
  UndefinedInterrupt, /*  93  reserved              */
  _OSIOHandler,       /*  94  RNG                   */
  _OSIOHandler,       /*  95  FPU                   */
  _OSIOHandler,       /*  96  HASH                  */
  UndefinedInterrupt, /*  97  reserved              */
  _OSIOHandler,       /*  98  LPTIM3                */
  _OSIOHandler,       /*  99  SPI3                  */
  _OSIOHandler,       /* 100  I2C4_ER               */
  _OSIOHandler,       /* 101  I2C4_EV               */
  _OSIOHandler,       /* 102  MDF1_FLT0             */
  _OSIOHandler,       /* 103  MDF1_FLT1             */
  _OSIOHandler,       /* 104  MDF1_FLT2             */
  _OSIOHandler,       /* 105  MDF1_FLT3             */
  _OSIOHandler,       /* 106  UCPD1                 */
  _OSIOHandler,       /* 107  ICACHE                */
  UndefinedInterrupt, /* 108  reserved              */
  UndefinedInterrupt, /* 109  reserved              */
  _OSIOHandler,       /* 110  LPTIM4                */
  _OSIOHandler,       /* 111  DCACHE1               */
  _OSIOHandler,       /* 112  ADF1                  */
  _OSIOHandler,       /* 113  ADC4                  */
  _OSIOHandler,       /* 114  LPDMA1_Channel0       */
  _OSIOHandler,       /* 115  LPDMA1_Channel1       */
  _OSIOHandler,       /* 116  LPDMA1_Channel2       */
  _OSIOHandler,       /* 117  LPDMA1_Channel3       */
  _OSIOHandler,       /* 118  DMA2D                 */
  _OSIOHandler,       /* 119  DCMI_PSSI             */
  _OSIOHandler,       /* 120  OCTOSPI2              */
  _OSIOHandler,       /* 121  MDF1_FLT4             */
  _OSIOHandler,       /* 122  MDF1_FLT5             */
  _OSIOHandler,       /* 123  CORDIC                */
  _OSIOHandler,       /* 124  FMAC                  */
  _OSIOHandler        /* 125  LSECSSD               */
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
