#include "int.h"
#include "frequency.h"
#include "tim.h"
#include "eeprom.h"
#include <stdlib.h>
#include <string.h>

volatile bool            allow_adjustment    = false;
static volatile uint32_t previous_capture    = 0;
volatile uint32_t        frequency           = 0;
static volatile uint32_t capture             = 0;
static volatile uint32_t pps_capture         = 0;
volatile uint32_t        num_samples         = 0;
static volatile uint32_t timer_overflows     = 0;
static volatile uint32_t pps_overflows       = 0;
volatile uint32_t        device_uptime       = 0;
static volatile uint8_t  first               = 1;
static volatile uint32_t last_pps            = 0;
volatile uint32_t        last_pps_out        = 0;
volatile bool            pps_out_up          = false;
static volatile bool     pps_led_toogle      = false;
volatile int32_t         ppb_frequency       = 0;
volatile int32_t         ppb_frequency_error = PPB_UNSET_VALUE;
volatile int32_t         ppb_correction      = 0;
volatile int32_t         ppb_millis          = 0;
volatile int32_t         pps_error           = 0;
volatile int32_t         pps_millis          = 0;
static volatile uint32_t pps_shift_count     = 0;
volatile uint32_t        pps_sync_count      = 0;
volatile bool            sync_pps_out        = false;
volatile bool            is_ppb_current      = false;
volatile bool            gps_lock_status     = false;
volatile bool            suppress_adjustment = false;

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef* htim)
{
    if (htim == &htim1) {
        timer_overflows++;
        pps_overflows++;
    } else if (htim == &htim2) {
        // TIM2 is configure for 1 second count
        // PPS output signal
        HAL_GPIO_WritePin(PPS_OUTPUT_GPIO_Port, PPS_OUTPUT_Pin, 1);
        pps_capture = HAL_TIM_ReadCapturedValue(htim, TIM_CHANNEL_1);
        pps_overflows = 0;
        last_pps_out = HAL_GetTick();
        pps_out_up = true;
        // PPS LED1 blink
        HAL_GPIO_WritePin(LED1_GPIO_Port, LED1_Pin, pps_led_toogle);
        pps_led_toogle = !pps_led_toogle;
        // Update uptime
        device_uptime++;

        if(HAL_GetTick() - last_pps > 1500)
        {   // No GPS PPS output
            if(gps_lock_status)
            {   // GPS lock lost => update status
                gps_lock_status = false;
                HAL_GPIO_WritePin(GPS_LOCK_OUTPUT_GPIO_Port, GPS_LOCK_OUTPUT_Pin, 1);
            }

            is_ppb_current = false;
        }
    }
}

static int32_t compute_square_adjustment(int32_t error, uint32_t factor, uint32_t factor_increment)
{
    int32_t result = ((float)(abs(error) * error))*((factor/10)+factor_increment);
    // Prevent from returning 0
    if(result == 0) result = ((error>=0) ? 1 : -1);
    // Limit adjustment to reasonable values
    if(result > 1000) result = 1000;
    if(result < -1000) result = -1000;
    return result;
}

static void apply_adjustment(int32_t adjustment)
{
    if (suppress_adjustment)
    {
        return;
    }

    if ((TIM1->CCR2 + adjustment) > 0xFFFF)
    {
        TIM1->CCR2 = 0xFFFF;
    }
    else if ((((int32_t)TIM1->CCR2) + adjustment) < 0)
    {
        TIM1->CCR2 = 0;
    }
    else
    {
        TIM1->CCR2 += adjustment;
    }
}

void dankar_correction_algo(int32_t current_error)
{
    if (current_error != 0) {
        // Use error^2 to adjust PWM for larger errors, but preserve sign.
        // Make even smaller adjustments close to 0.
        // This is all just guesses and should be investigated more fully.
        int32_t adjustment = 0;

        if (abs(current_error) > 10) {
            adjustment = compute_square_adjustment(current_error,ee_storage.correction_factor,1);
        } else if (abs(current_error) > 2) {
            adjustment = compute_square_adjustment(current_error,ee_storage.correction_factor,0);
        } else {
            adjustment = current_error;
        }
        // Apply it
        apply_adjustment(-adjustment);
        ppb_correction = -adjustment;
    }
    else {
        ppb_correction = 0;
    }
}

void fredzo_correction_algo(int32_t current_error)
{
    if (current_error != 0) {
        int32_t adjustment = 0;
        if (abs(current_error) >= 16) {
            adjustment = compute_square_adjustment(current_error,ee_storage.correction_factor,2);
        } else if (abs(current_error) >= 8) {
            adjustment = compute_square_adjustment(current_error,ee_storage.correction_factor,1);
        } else if (abs(current_error) >= 2) {
            adjustment = compute_square_adjustment(current_error,ee_storage.correction_factor,0);
        } else {
            adjustment = current_error;
        }
        // Apply it
        apply_adjustment(-adjustment);
        ppb_correction = -adjustment;
    }
    else {
        ppb_correction = 0;
    }
}

void eric_h_correction_algo(bool overshootSuppression, int32_t current_error)
{
    const float  ema_a      = 0.33f;
    static float ema_err_hz = 0.0f;

    int32_t current_ppb = frequency_ppb_x100;
    int32_t adjustment = 0;

    if (current_ppb != PPB_UNSET_VALUE)
    {
        ema_err_hz = overshootSuppression
            ? ema_a * current_error + ((1 - ema_a) * ema_err_hz)
            : 0.0f;

        if (abs(current_ppb) > 0)
        {
            const int factor = ee_storage.correction_factor;
            int interval = 1;

            // Calculate adjustment.
            adjustment = -current_ppb / factor;
            if (adjustment == 0)
            {
                // Adjustment is less than 1 per interval.
                adjustment = current_ppb > 0 ? -1 : 1;
                interval = factor / (abs(current_ppb) % factor);
            }

            if (overshootSuppression &&
                (ema_err_hz == 0 || ((ema_err_hz > 0) != (current_ppb > 0))))
            {
                adjustment = 0;
            }

            // Apply adjustment.
            if (device_uptime % interval == 0)
            {
                apply_adjustment(adjustment);
            }
            else
            {
                adjustment = 0;
            }
        }
    }
    ppb_correction = adjustment;
}

// This gets run each time PPS goes high
void HAL_TIM_IC_CaptureCallback(TIM_HandleTypeDef* htim)
{
    if (htim->Channel == HAL_TIM_ACTIVE_CHANNEL_1) {

        capture = HAL_TIM_ReadCapturedValue(htim, TIM_CHANNEL_1);

        uint32_t current_tick = HAL_GetTick();
        // Ignore first capture and do a sanity check on elapsed time since previous PPS
        if (!first && current_tick - last_pps < 1300) {
            // See if we need to resync MCU PPS Out
            pps_error = (capture - pps_capture + /*(TIM1->ARR + 1)*/ 65536 * pps_overflows) - TARGET_FREQ /*HAL_RCC_GetHCLKFreq()*/;
            if(ee_storage.pps_auto_sync && (sync_pps_out ||(abs(pps_error) >= ee_storage.pps_sync_threshold)))
            {
                pps_shift_count++;
                if(sync_pps_out || (pps_shift_count > ee_storage.pps_sync_delay))
                {   // Force sync by reseting TIM2
                    TIM2->CNT = TIM2->ARR;
                    pps_sync_count++;
                    pps_shift_count = 0;
                    sync_pps_out = false;
                }
            }
            else
            {   // Reset shift count if we are below threshold
                pps_shift_count = 0;
            }

            // Frequency detection for VCO adjustment
            frequency = capture - previous_capture + /*(TIM1->ARR + 1)*/ 65536 * timer_overflows;

            int32_t current_error = frequency_get_error();
            // Filter out obvious glitches, the OCXO should never be this far from the target frequency
            bool is_glitch = (current_error == PPB_UNSET_VALUE) || (current_error > 2000 || current_error < -2000);

            if (allow_adjustment && !is_glitch)
            {   // No correction during warmup

                // Choose from 4 correction algorithms :
                // - Dankar (original code from Dankar + added correction factor defaulted to values that match the original code)
                // - Fredzo (same logic as dankar's algo, but with faster correction when frequency error is >= 2)
                // - Eric-H (algo based on ppm value rather than frequency error (uses 128s rolling average rather than instant values))
                // - Eric-H+ (Eric-H algorithm modified for control loop overshoot suppression)
                switch(ee_storage.correction_algorithm)
                {
                    case CORRECTION_ALGO_DANKAR:
                        dankar_correction_algo(current_error);
                        break;
                    case CORRECTION_ALGO_ERIC_H:
                        eric_h_correction_algo(false, current_error);
                        break;
                    case CORRECTION_ALGO_ERIC_H_PLUS:
                        eric_h_correction_algo(true, current_error);
                        break;
                    default:
                    case CORRECTION_ALGO_FREDZO:
                        fredzo_correction_algo(current_error);
                        break;
                }
            }

            // Save values for ppb and pps display
            ppb_frequency = frequency;
            ppb_frequency_error = current_error;
            ppb_millis = current_tick - last_pps - 1000;
            pps_millis = (int32_t)(((int64_t)pps_error * 10000000LL) / TARGET_FREQ); // Clock is 'TARGET_FREQ' Hz and we want the value in tenth of a microsecond so 10 000 000 / TARGET_FREQ

            if (allow_adjustment && !is_glitch)
            {   // Also remove warmup samples from circular buffer
                circbuf_add(&circular_buffer, current_error);
                if (num_samples < CIRCULAR_BUFFER_LEN) {
                    num_samples++;
                }
                frequency_update_ppb_and_stability();
                is_ppb_current = true;
            }
        }

        previous_capture = capture;
        timer_overflows  = 0;
        first            = 0;

        // Update last PPS time
        last_pps     = current_tick;

        if(!gps_lock_status)
        {   // Update GPS lock status
            gps_lock_status = true;
            HAL_GPIO_WritePin(GPS_LOCK_OUTPUT_GPIO_Port, GPS_LOCK_OUTPUT_Pin, 0);
        }
    }
}

const char* ocxo_model_type_to_string(uint8_t model)
{
    switch(model)
    {
        case OCXO_MODEL_ISOTEMP:
            return "ISOTEMP";

        case OCXO_MODEL_OX256B:
            return "OX256B";

        default:
        case OCXO_MODEL_UNKNOWN:
            return "Unknown";
    }
}

const char* correction_algo_type_to_string(uint8_t model)
{
    switch (model)
    {
        case CORRECTION_ALGO_DANKAR:
            return "Dankar";

        case CORRECTION_ALGO_FREDZO:
            return "Fredzo";

        case CORRECTION_ALGO_ERIC_H:
            return "Eric H";

        default:
        case CORRECTION_ALGO_ERIC_H_PLUS:
            return "Eric H+";
    }
}

void set_brightness(uint8_t brightness)
{
    uint32_t pwm_value = brightness * 0xFFFF / 100;
    TIM1->CCR3 = pwm_value;
}

uint32_t get_default_correction_factor(correction_algo_type algo)
{
    switch(algo)
    {
        case CORRECTION_ALGO_DANKAR:
            return 10;
            break;
        case CORRECTION_ALGO_ERIC_H:
        case CORRECTION_ALGO_ERIC_H_PLUS:
            return 300;
            break;
        default:
        case CORRECTION_ALGO_FREDZO:
            return 10;
            break;
    }
}

void increment_correction_factor_value(uint32_t* value, correction_algo_type algo, int32_t increment)
{
    uint32_t minVal;
    uint32_t maxVal;
    uint32_t incFactor;
    switch(algo)
    {
        case CORRECTION_ALGO_DANKAR:
            minVal = 1;
            maxVal = 60;
            incFactor = 1;
            break;
        case CORRECTION_ALGO_ERIC_H:
        case CORRECTION_ALGO_ERIC_H_PLUS:
            minVal = 10;
            maxVal = 600;
            incFactor = 10;
            break;
        default:
        case CORRECTION_ALGO_FREDZO:
            minVal = 1;
            maxVal = 60;
            incFactor = 1;
            break;
    }

    uint32_t new_factor = *value + (incFactor*increment);
    if(new_factor < minVal) {
        *value = minVal;
    } else if(new_factor > maxVal) {
        *value = maxVal;
    } else {
        *value = new_factor;
    }
}

uint32_t get_default_warmup_time(ocxo_model_type model)
{
    switch(model)
    {
        case OCXO_MODEL_OX256B:
            return 150;
            break;
        default:
        case OCXO_MODEL_ISOTEMP:
            return 120;
            break;
    }
}
