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
*/
/* File icc_measure.c:
** Version date: July 2012
** Authors: MIS-TIC */

#include "ZottaOS.h"
#include "stm32l1xx.h"

static void InitCurrentMeasurement(void);
static UINT16 GetCurrentMeasurement (void);


int main(void)
{
  UINT16 Current[4];
  UINT32 i;

  /* Keep debugger connection during sleep mode */
  DBGMCU_Config(DBGMCU_SLEEP,ENABLE);

  /* Initialize Hardware */
  SystemInit();
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
  ADC_InitTypeDef ADC_InitStructure;
  GPIO_InitTypeDef GPIO_InitStructure;
  /* Enable GPIOA clock */
  RCC_AHBPeriphClockCmd(GPIOA, ENABLE);
  /* Configure ADC (GPIO_Pin_4) pin as analog */
  GPIO_InitStructure.GPIO_Pin = GPIO_Pin_4  ;
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AN;
  GPIO_Init( GPIOA, &GPIO_InitStructure);
  /* Enable HSI Clock */
  RCC_HSICmd(ENABLE);
  /*!< Wait till HSI is ready */
  while (RCC_GetFlagStatus(RCC_FLAG_HSIRDY) == RESET);
  /* Enable ADC clock */
  RCC_APB2PeriphClockCmd(RCC_APB2Periph_ADC1, ENABLE);
 /*  de-initialize ADC */
  ADC_DeInit(ADC1);
  /* ADC Configuration */
  ADC_StructInit(&ADC_InitStructure);
  ADC_InitStructure.ADC_Resolution = ADC_Resolution_12b;
  ADC_InitStructure.ADC_ScanConvMode = ENABLE;
  ADC_InitStructure.ADC_ContinuousConvMode = DISABLE;
  ADC_InitStructure.ADC_ExternalTrigConvEdge = ADC_ExternalTrigConvEdge_None;
  ADC_InitStructure.ADC_DataAlign = ADC_DataAlign_Right;
  ADC_InitStructure.ADC_NbrOfConversion = 1;
  ADC_Init(ADC1, &ADC_InitStructure);
  /* ADC1 regular channel4 configuration */
  ADC_RegularChannelConfig(ADC1, ADC_Channel_4, 1, ADC_SampleTime_192Cycles);
  ADC_DelaySelectionConfig(ADC1, ADC_DelayLength_Freeze);
  /* Enable ADC1 */
  ADC_Cmd(ADC1, ENABLE);
  /* Wait until ADC1 ON status */
  while (ADC_GetFlagStatus(ADC1, ADC_FLAG_ADONS) == RESET);
} /* end of InitCurrentMeasurement */


/* Current measurement */
UINT16 GetCurrentMeasurement (void)
{
  ADC_SoftwareStartConv(ADC1);
  while( ADC_GetFlagStatus(ADC1,ADC_FLAG_EOC) == 0);
  return ADC_GetConversionValue(ADC1) >> 2;
} /* end of GetCurrentMeasurement */
