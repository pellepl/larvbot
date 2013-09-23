/*
 * adc_driver.c
 *
 *  Created on: Sep 23, 2013
 *      Author: petera
 */

#include "adc_driver.h"

u8_t const adc_channels[] = {
    0,
    ADC_Channel_6,
    ADC_Channel_7,
    ADC_Channel_8,
    ADC_Channel_TempSensor,
    ADC_Channel_Vrefint,
    ADC_Channel_Vbat,
};

typedef enum {
  IDLE = 0,
  MONO_SINGLE,
  STEREO_SINGLE,
} adc_state;

static struct {
  volatile adc_state state;
  adc_cb cb;
  adc_channel channel0;
  adc_channel channel1;
} adc;

void ADC_init(void) {
  memset(&adc, 0, sizeof(adc));
  adc.state = IDLE;
  ADC_ITConfig(ADC1, ADC_IT_EOC, ENABLE);
  //ADC_ITConfig(ADC2, ADC_IT_EOC, ENABLE);
}

int ADC_sample_mono_single(adc_channel ch) {
  ADC_CommonInitTypeDef adc_common;

  if (adc.state != IDLE) {
    return ERR_ADC_BUSY;
  }
  adc.state = MONO_SINGLE;

  adc_common.ADC_Mode = ADC_Mode_Independent;
  adc_common.ADC_Prescaler = ADC_Prescaler_Div2;
  adc_common.ADC_DMAAccessMode = ADC_DMAAccessMode_Disabled;
  adc_common.ADC_TwoSamplingDelay = ADC_TwoSamplingDelay_5Cycles;
  ADC_CommonInit(&adc_common);
  adc.channel0  = ch;
  ADC_RegularChannelConfig(ADC1, adc_channels[ch], 1, ADC_SampleTime_3Cycles);

  ADC_SoftwareStartConv(ADC1);

  return ADC_OK;
}

int ADC_sample_stereo_single(adc_channel ch1, adc_channel ch2) {
  ADC_CommonInitTypeDef adc_common;

  if (adc.state != IDLE) {
    return ERR_ADC_BUSY;
  }
  adc.state = STEREO_SINGLE;

  adc_common.ADC_Mode = ADC_DualMode_RegSimult;
  adc_common.ADC_Prescaler = ADC_Prescaler_Div2;
  adc_common.ADC_DMAAccessMode = ADC_DMAAccessMode_Disabled;
  adc_common.ADC_TwoSamplingDelay = ADC_TwoSamplingDelay_5Cycles;
  ADC_CommonInit(&adc_common);
  adc.channel0  = ch1;
  adc.channel1  = ch2;
  ADC_RegularChannelConfig(ADC1, adc_channels[ch1], 1, ADC_SampleTime_3Cycles);
  ADC_RegularChannelConfig(ADC2, adc_channels[ch2], 1, ADC_SampleTime_3Cycles);

  ADC_SoftwareStartConv(ADC1);

  return ADC_OK;
}

int ADC_set_callback(adc_cb cb) {
  if (adc.state != IDLE) {
    return ERR_ADC_BUSY;
  }
  adc.cb = cb;
  return ADC_OK;
}

void ADC_irq() {
  if (ADC_GetITStatus(ADC1, ADC_IT_EOC) != RESET) {
    ADC_ClearITPendingBit(ADC1, ADC_IT_EOC);
    adc_state old_state = adc.state;
    adc.state = IDLE;
    if (adc.cb) {
      switch (old_state) {
      case MONO_SINGLE:
        adc.cb(adc.channel0, ADC_GetConversionValue(ADC1), ADC_NONE, 0);
        break;
      case STEREO_SINGLE:
        adc.cb(adc.channel0, ADC_GetConversionValue(ADC1),
               adc.channel1, ADC_GetConversionValue(ADC2));
        break;
      default:
        ASSERT(FALSE);
        break;
      }
    }
  }
  if (ADC_GetITStatus(ADC2, ADC_IT_EOC) != RESET) {
    ADC_ClearITPendingBit(ADC2, ADC_IT_EOC);
  }
}
