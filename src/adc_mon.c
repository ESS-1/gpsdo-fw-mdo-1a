#include "adc_mon.h"
#include "adc.h"
#include "timer.h"
#include "eeprom.h"

#define ADC_MON_MEASUREMENT_INTERVAL 350
#define ADC_MON_CALIBRATION_INTERVAL 30000

#define ADC_MAX_CODE                 4095UL // 12-bit ADC full scale

#define ADC_MON_CAL_VREFINT_NOM      1200
#define ADC_MON_CAL_VUSB_DIV_NOM     2000
#define ADC_MON_CAL_TEMP_V25_NOM     1430
#define ADC_MON_CAL_TEMP_SLOPE_NOM   430

AdcMonStatus adc_mon_status = AdcMonStatus_Ok;

static bool     adc_mon_is_idle          = true;
static uint32_t adc_mon_last_measurement = 0;
static uint32_t adc_mon_last_calibration = 0;

volatile uint16_t adc_mon_vdda_mv     = ADC_MON_VOLTAGE_NO_DATA; // MCU supply rail in mV
volatile uint16_t adc_mon_vusb_mv     = ADC_MON_VOLTAGE_NO_DATA; // USB voltage in mV
volatile int16_t  adc_mon_temp_deci_c = ADC_MON_TEMP_NO_DATA;    // Temperature in 0.1 deg. C


static void adc_mon_calibrate(void)
{
    HAL_ADCEx_Calibration_Start(&hadc1);
}

static void adc_mon_update_status(void)
{
    AdcMonStatus new_status = AdcMonStatus_Ok;

    if (adc_mon_vusb_mv < ADC_MON_VUSB_MIN_MV) {
        new_status |= AdcMonStatus_Vusb_Low;
    } else if (adc_mon_vusb_mv > ADC_MON_VUSB_MAX_MV) {
        new_status |= AdcMonStatus_Vusb_High;
    }

    if (adc_mon_temp_deci_c > ADC_MON_TEMP_MAX_DECI_C) {
        new_status |= AdcMonStatus_Overheat;
    }

    adc_mon_status = new_status;
}

static void adc_mon_update_measurements(void)
{
    // Read ADC injected conversion results
    uint32_t raw_vusb    = HAL_ADCEx_InjectedGetValue(&hadc1, ADC_INJECTED_RANK_1);
    uint32_t raw_vrefint = HAL_ADCEx_InjectedGetValue(&hadc1, ADC_INJECTED_RANK_2);
    uint32_t raw_temp    = HAL_ADCEx_InjectedGetValue(&hadc1, ADC_INJECTED_RANK_3);

    // Guard against division by zero
    if (raw_vrefint == 0) {
        adc_mon_vdda_mv     = ADC_MON_VOLTAGE_NO_DATA;
        adc_mon_vusb_mv     = ADC_MON_VOLTAGE_NO_DATA;
        adc_mon_temp_deci_c = ADC_MON_TEMP_NO_DATA;
        return;
    }

    // Calculate Vcc (Vdda rail) using the internal bandgap
    uint32_t vref   = (uint32_t)((int32_t)ADC_MON_CAL_VREFINT_NOM + (int32_t)ee_storage.calibration.adc_adj_vrefint_mv);
    uint32_t vdda   = ((vref * ADC_MAX_CODE) + (raw_vrefint / 2)) / raw_vrefint;
    adc_mon_vdda_mv = (uint16_t)vdda;

    // Calculate Vusb
    uint64_t vusb_div   = (uint64_t)((int32_t)ADC_MON_CAL_VUSB_DIV_NOM + (int32_t)ee_storage.calibration.adc_adj_vusb_div_ratio);
    uint32_t denom_vusb = raw_vrefint * 1000;
    uint64_t numer_vusb = (uint64_t)raw_vusb * (uint64_t)vref * vusb_div;
    adc_mon_vusb_mv     = (uint16_t)((numer_vusb + (denom_vusb / 2)) / denom_vusb);

    // Calculate MCU temperature
    int32_t temp_mv_x10 = (int32_t)(((raw_temp * vdda * 10) + (ADC_MAX_CODE / 2)) / ADC_MAX_CODE);
    int32_t t_v25       = (int32_t)ADC_MON_CAL_TEMP_V25_NOM + (int32_t)ee_storage.calibration.adc_adj_temp_v25_mv;
    int32_t t_slope     = (int32_t)ADC_MON_CAL_TEMP_SLOPE_NOM + (int32_t)ee_storage.calibration.adc_adj_temp_slope;
    adc_mon_temp_deci_c = (int16_t)((((t_v25 * 10 - temp_mv_x10) * 100 + (t_slope / 2)) / t_slope) + 250);

    // Update status
    adc_mon_update_status();
}

void adc_mon_init(void)
{
    adc_mon_calibrate();
    timer_reset(&adc_mon_last_calibration);
    timer_reset(&adc_mon_last_measurement);
}

void adc_mon_run(void)
{
    if (adc_mon_is_idle) {
        // Calibrate ADC periodically to compensate for temperature drift
        if (timer_is_elapsed(&adc_mon_last_calibration, ADC_MON_CALIBRATION_INTERVAL, false)) {
            adc_mon_calibrate();
        }

        // Start a new measurement cycle every 250 ms
        if (timer_is_elapsed(&adc_mon_last_measurement, ADC_MON_MEASUREMENT_INTERVAL, false)) {
            // Start injected conversion sequence (3 channels: IN8, Vrefint, Temp Sensor)
            HAL_ADCEx_InjectedStart(&hadc1);
            adc_mon_is_idle = false;
        }
    } else if (HAL_IS_BIT_SET(hadc1.Instance->SR, ADC_FLAG_JEOC)) {
        // The injected conversion sequence has completed
        adc_mon_update_measurements();
        adc_mon_is_idle = true;
    }
}
