/*
 * adc_driver.h
 *
 *  Created on: Sep 23, 2013
 *      Author: petera
 */

#ifndef ADC_DRIVER_H_
#define ADC_DRIVER_H_

#include "system.h"

#define ADC_OK              0
#define ERR_ADC_BUSY        -1

typedef enum {
  ADC_NONE = 0,
  ADC_CH0,
  ADC_CH1,
  ADC_CH2,
  ADC_TEMP,
  ADC_VREF,
  ADC_VBAT,
  _ADC_CHANNELS
} adc_channel;

typedef void (*adc_cb)(adc_channel ch1, u32_t val1, adc_channel ch2, u32_t val2);

void ADC_init(void);

int ADC_sample_mono_single(adc_channel ch);
int ADC_sample_stereo_single(adc_channel ch1, adc_channel ch2);

int ADC_sample_mono_continuous(adc_channel ch);
int ADC_sample_stereo_continuous(adc_channel ch1, adc_channel ch2, u32_t freq);
int ADC_sample_continuous_stop(void);

int ADC_set_callback(adc_cb cb);

u32_t ADC_get_freq(void);

void ADC_irq();

#endif /* ADC_DRIVER_H_ */
