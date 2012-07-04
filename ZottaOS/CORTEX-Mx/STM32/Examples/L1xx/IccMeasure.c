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
** Version date: June 2012
** Authors: MIS-TIC */

#include "ZottaOS.h"
#include "stm32l1xx.h"

#define VREF 		        1.224L // Theoretically BandGAP 1.224 Volt
#define ADC_CONV 	        4096   // ADC Converter LSBIdeal = VREF/4096 or VDA/4096
#define IDD_MEASURE_PORT	GPIOA
#define IDD_MEASURE         GPIO_Pin_4

static void Init_ADC(void);

static UINT16 Current_Measurement (void);
static float Vdd_appli(void);

/* Function to return the VDD measurement */
float Vdd_appli(void)
{
  UINT8 i;
  UINT16 MeasurINT = 0;
  float f_Vdd_appli ;
  /* Read the BandGap value on ADC converter*/
  ADC_TempSensorVrefintCmd(ENABLE);
  /* ADC1 regular channel 17 for VREF configuration */
  ADC_RegularChannelConfig(ADC1,ADC_Channel_17,1,ADC_SampleTime_192Cycles);
  for(i = 4; i > 0; i--) {
     ADC_SoftwareStartConv(ADC1);
     while (ADC_GetFlagStatus(ADC1, ADC_FLAG_EOC) == 0);
     MeasurINT += ADC_GetConversionValue(ADC1);
  }
  ADC_TempSensorVrefintCmd(DISABLE);
  /* We use the Theoretically value */
  f_Vdd_appli = (VREF / (MeasurINT >> 2)) * ADC_CONV;
  f_Vdd_appli *= 1000L; // convert Vdd_appli into mV
  return f_Vdd_appli;
}

/* Current measurement */
UINT16 Current_Measurement (void)
{
  UINT16 i, res = 0;
  for (i = 4; i > 0; i--) {
     ADC_SoftwareStartConv(ADC1);
     while( ADC_GetFlagStatus(ADC1,ADC_FLAG_EOC) == 0);
     res += ADC_GetConversionValue(ADC1);
  }
  return (res >> 2);
}


int main(void)
{
  UINT16 Current;
  /* Stop timer during debugger connection */
  #if ZOTTAOS_TIMER == OS_IO_TIM11
     DBGMCU_APB2PeriphConfig(DBGMCU_TIM11_STOP,ENABLE);
  #elif ZOTTAOS_TIMER == OS_IO_TIM10
     DBGMCU_APB2PeriphConfig(DBGMCU_TIM10_STOP,ENABLE);
  #elif ZOTTAOS_TIMER == OS_IO_TIM9
     DBGMCU_APB2PeriphConfig(DBGMCU_TIM9_STOP,ENABLE);
  #elif ZOTTAOS_TIMER == OS_IO_TIM5
     DBGMCU_APB1PeriphConfig(DBGMCU_TIM5_STOP,ENABLE);
  #elif ZOTTAOS_TIMER == OS_IO_TIM4
     DBGMCU_APB1PeriphConfig(DBGMCU_TIM4_STOP,ENABLE);
  #elif ZOTTAOS_TIMER == OS_IO_TIM3
     DBGMCU_APB1PeriphConfig(DBGMCU_TIM3_STOP,ENABLE);
  #elif ZOTTAOS_TIMER == OS_IO_TIM2
     DBGMCU_APB1PeriphConfig(DBGMCU_TIM2_STOP,ENABLE);
#endif
  /* Keep debugger connection during sleep mode */
  DBGMCU_Config(DBGMCU_SLEEP,ENABLE);
  /* Initialize Hardware */
  SystemInit();
  /* Init ADC channel 24 */
  Init_ADC();
  /* */
  Current = Current_Measurement();
  Current = (Current * Vdd_appli() / ADC_CONV) * 20L; // Convert measured value in uA
  while(1);

  /* Start the OS so that it starts scheduling the user tasks */
  return OSStartMultitasking(NULL,NULL);
} /* end of main */


/* ADC initialization (ADC_Channel_4) */
void Init_ADC(void)
{
  ADC_InitTypeDef ADC_InitStructure;
  GPIO_InitTypeDef GPIO_InitStructure;
  /* Configure ADC (IDD_MEASURE) pin as analog */
  GPIO_InitStructure.GPIO_Pin = IDD_MEASURE  ;
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AN;
  GPIO_Init( IDD_MEASURE_PORT, &GPIO_InitStructure);
  RCC_APB2PeriphClockCmd(RCC_APB2Periph_ADC1, ENABLE); // Enable ADC clock
  ADC_DeInit(ADC1); /* de-initialize ADC */
  /*  ADC configured as follow:
    - NbrOfChannel = 1 - ADC_Channel_4
    - Mode = Single ConversionMode(ContinuousConvMode disabled)
    - Resolution = 12Bits
    - Prescaler = /1
    - sampling time 192 */
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
  ADC_PowerDownCmd(ADC1, ADC_PowerDown_Idle_Delay, ENABLE);
  ADC_Cmd(ADC1, ENABLE); /* Enable ADC1 */
  /* Wait until ADC1 ON status */
  while (ADC_GetFlagStatus(ADC1, ADC_FLAG_ADONS) == RESET);
} /* end of Init_ADC */
