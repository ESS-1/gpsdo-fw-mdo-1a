#ifndef _ADC_MON_883A87E4CE31_H_
#define _ADC_MON_883A87E4CE31_H_

#include <stdint.h>

#define ADC_MON_VOLTAGE_NO_DATA   0xFFFF
#define ADC_MON_TEMP_NO_DATA      0x7FFF

void adc_mon_init(void);
void adc_mon_run(void);

extern volatile uint16_t adc_mon_vdda_mv;
extern volatile uint16_t adc_mon_vusb_mv;
extern volatile int16_t  adc_mon_temp_deci_c;

#define ADC_MON_CAL_VREFINT_ADJ_MIN    (-50)
#define ADC_MON_CAL_VREFINT_ADJ_MAX      70

#define ADC_MON_CAL_VUSB_DIV_ADJ_MIN   (-50)
#define ADC_MON_CAL_VUSB_DIV_ADJ_MAX     50

#define ADC_MON_CAL_TEMP_V25_ADJ_MIN   (-120)
#define ADC_MON_CAL_TEMP_V25_ADJ_MAX     120

#define ADC_MON_CAL_TEMP_SLOPE_ADJ_MIN (-40)
#define ADC_MON_CAL_TEMP_SLOPE_ADJ_MAX   40

#endif
