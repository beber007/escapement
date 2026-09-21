/* Copyright (c) 2026 Bertrand Hurst. All rights reserved.
** Escapement - Lightweight Power-Aware Real-Time OS.
** Derived from prior work; see LICENSE and NOTICE at the root of this repository.
*/
/* File icc_measure.c:
** Version date: July 2012
*/

#include "Escapement.h"
#include "BoardL1.h"

static void InitCurrentMeasurement(void);
static UINT16 GetCurrentMeasurement (void);


int main(void)
{
  /* volatile: the readings are for the debugger and the loops are the settling time,
  ** neither of which -O2 would otherwise keep. */
  volatile UINT16 Current[4];
  volatile UINT32 i;

  /* Keep debugger connection during sleep mode */
  BoardKeepDebugInSleep();

  /* Initialize Hardware */
  SystemInit();
  BoardInitClock();
  InitCurrentMeasurement();

  OSInitProcessorSpeed();
 _OSEnableInterrupts();

  while(1) {
	  // Set Vcore to 1.8V and SYSCLK to 32 MHz
	  OSSetProcessorSpeed(OS_32MHZ_SPEED);
      for (i = 0; i < 100000; i++);
      Current[0] = GetCurrentMeasurement();

      // Set Vcore to 1.5V and SYSCLK to 16 MHz
	  OSSetProcessorSpeed(OS_16MHZ_SPEED);
      for (i = 0; i < 100000; i++);
      Current[1] = GetCurrentMeasurement();

	  // Set Vcore to 1.2V and SYSCLK to 4 MHz
  	  OSSetProcessorSpeed(OS_4MHZ_SPEED);
      for (i = 0; i < 100000; i++);
      Current[2] = GetCurrentMeasurement();

      // Set Vcore to 1.5V and SYSCLK to 16 MHz
	  OSSetProcessorSpeed(OS_16MHZ_SPEED);
      for (i = 0; i < 100000; i++);
      Current[3] = GetCurrentMeasurement();
  }


  /* Start the OS so that it starts scheduling the user tasks */
  return OSStartMultitasking(NULL,NULL);
} /* end of main */


/* ADC initialization (ADC_Channel_4) */
void InitCurrentMeasurement(void)
{
  /* Enable GPIOA clock */
  BoardEnablePort(GPIOA);
  /* Configure ADC (PA4) pin as analog */
  GPIOA->MODER |= 3u << (2 * 4);
  /* Enable HSI Clock */
  RCC->CR |= RCC_CR_HSION;
  /*!< Wait till HSI is ready */
  while ((RCC->CR & RCC_CR_HSIRDY) == 0);
  /* Enable ADC clock */
  RCC->APB2ENR |= RCC_APB2ENR_ADC1EN;
  /*  de-initialize ADC */
  RCC->APB2RSTR |= RCC_APB2RSTR_ADC1RST;
  RCC->APB2RSTR &= ~RCC_APB2RSTR_ADC1RST;
  /* ADC Configuration: 12-bit resolution, scan mode, single conversion, no external
  ** trigger, right-aligned data, one conversion in the regular sequence */
  ADC1->CR1 = ADC_CR1_SCAN;
  ADC1->CR2 = 0;
  ADC1->SQR1 = 0;
  /* ADC1 regular channel4 configuration: first in the sequence, sampled for 192 cycles */
  ADC1->SQR5 = 4;
  ADC1->SMPR3 = (ADC1->SMPR3 & ~ADC_SMPR3_SMP4) | (6u << 12);
  /* Freeze the ADC until the converted data has been read */
  ADC1->CR2 |= ADC_CR2_DELS_0;
  /* Enable ADC1 */
  ADC1->CR2 |= ADC_CR2_ADON;
  /* Wait until ADC1 ON status */
  while ((ADC1->SR & ADC_SR_ADONS) == 0);
} /* end of InitCurrentMeasurement */


/* Current measurement */
UINT16 GetCurrentMeasurement (void)
{
  ADC1->CR2 |= ADC_CR2_SWSTART;
  while ((ADC1->SR & ADC_SR_EOC) == 0);
  return ADC1->DR >> 2;
} /* end of GetCurrentMeasurement */
