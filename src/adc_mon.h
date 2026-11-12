#ifndef _ADC_MON_883A87E4CE31_H_
#define _ADC_MON_883A87E4CE31_H_

#include <stdint.h>

// Data constants
#define ADC_MON_VOLTAGE_NO_DATA   0xFFFF
#define ADC_MON_TEMP_NO_DATA      0x7FFF

// Calibration limits
#define ADC_MON_CAL_VREFINT_ADJ_MIN    (-50)
#define ADC_MON_CAL_VREFINT_ADJ_MAX      70

#define ADC_MON_CAL_VUSB_DIV_ADJ_MIN   (-50)
#define ADC_MON_CAL_VUSB_DIV_ADJ_MAX     50

#define ADC_MON_CAL_TEMP_V25_ADJ_MIN   (-120)
#define ADC_MON_CAL_TEMP_V25_ADJ_MAX     120

#define ADC_MON_CAL_TEMP_SLOPE_ADJ_MIN (-40)
#define ADC_MON_CAL_TEMP_SLOPE_ADJ_MAX   40

// Operating conditions
#define ADC_MON_VUSB_MIN_MV              4500 // 5V -10%
#define ADC_MON_VUSB_MAX_MV              5500 // 5V +10%

#define ADC_MON_TEMP_MAX_DECI_C          800  // 80 deg. C

// Variable exports
typedef enum : uint8_t {
    AdcMonStatus_Ok            = 0x00,
    AdcMonStatus_Vusb_Low      = 0x01,
    AdcMonStatus_Vusb_High     = 0x02,
    AdcMonStatus_Overheat      = 0x04,
} AdcMonStatus;

extern AdcMonStatus adc_mon_status;

extern volatile uint16_t adc_mon_vdda_mv;
extern volatile uint16_t adc_mon_vusb_mv;
extern volatile int16_t  adc_mon_temp_deci_c;

// Method exports
void adc_mon_init(void);
void adc_mon_run(void);

#endif
