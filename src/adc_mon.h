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

#define ADC_MON_CAL_VREFINT_MIN      1150
#define ADC_MON_CAL_VREFINT_NOM      1200
#define ADC_MON_CAL_VREFINT_MAX      1270

#define ADC_MON_CAL_VUSB_DIV_MIN     1950
#define ADC_MON_CAL_VUSB_DIV_NOM     2000
#define ADC_MON_CAL_VUSB_DIV_MAX     2050

#define ADC_MON_CAL_TEMP_V25_MIN     1310
#define ADC_MON_CAL_TEMP_V25_NOM     1430
#define ADC_MON_CAL_TEMP_V25_MAX     1550

#define ADC_MON_CAL_TEMP_SLOPE_MIN   390
#define ADC_MON_CAL_TEMP_SLOPE_NOM   430
#define ADC_MON_CAL_TEMP_SLOPE_MAX   470

#endif
