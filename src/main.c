#include "main.h"
#include "gpio.h"
#include "i2c.h"
#include "spi.h"
#include "eeprom.h"
#include "frequency.h"
#include "gps.h"
#include "ui.h"
#include "ui_msgbox.h"
#include "int.h"
#include "tim.h"
#include "pll.h"
#include "bootlog.h"
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// All times in ms
#define PPS_PULSE_WIDTH         100
#define GPS_FRAME_WAIT_DELAY    10000

static bool auto_sync_pps_done = false;


void init_ext_clock()
{
    // MX_GPIO_Init() sets a high level on OCXO_EN; wait for contact bounce to settle before proceeding
    HAL_Delay(750);

    // Initialize minimal peripherals required to configure the external clock
    MX_GPIO_Init();
    MX_I2C1_Init();
    MX_SPI1_Init();

    bootlog_init();
    bootlog_add(BUILD_FW_MODEL "   ver. " BUILD_FW_VERSION);
    bootlog_add("Initializing...");
    bootlog_add("Enable OCXO");

    // Wait for OCXO startup
    HAL_Delay(500);

    // Init SI5351 PLL
    bool pll_fail = false;
    bootlog_add("Init PLL");
    if (pll_init_primary_pll()) {
        bootlog_set_status(true);

        // Wait for PLL lock
        bootlog_add("Wait PLL Lock");
        if (pll_wait_primary_lock()) {
            bootlog_set_status(true);

            // Enable output
            bootlog_add("Enable Output");
            pll_enable_primary_output();
            bootlog_set_status(true);
        } else {
            bootlog_set_status(false);
            pll_fail = true;
            bootlog_error("PLL lock failure!");
        }
    } else {
        bootlog_set_status(false);
        pll_fail = true;
        bootlog_error("PLL init failure!");
    }

    if (pll_fail) {
        // Turn off OCXO
        HAL_GPIO_WritePin(OCXO_EN_GPIO_Port, OCXO_EN_Pin, 0);
        bootlog_add("Disable OCXO");

        Error_Handler();
        return;
    }

    // Switch to the normal operation mode
    bootlog_add("Use OCXO clock...");
    HAL_Delay(750);
}

static void enable_usb()
{
    // Turn on 1.5K USB D+ pull-up
    HAL_GPIO_WritePin(USB_DP_PULLUP_GPIO_Port, USB_DP_PULLUP_Pin, GPIO_PIN_SET);

    GPIO_InitTypeDef gpio_init = { 0 };
    gpio_init.Pin              = USB_DP_PULLUP_Pin;
    gpio_init.Mode             = GPIO_MODE_OUTPUT_PP;
    gpio_init.Pull             = GPIO_NOPULL;
    gpio_init.Speed            = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(USB_DP_PULLUP_GPIO_Port, &gpio_init);
}

void load_settings(bool restore_defaults, bool apply_settings)
{
    // Do NOT reset 'total_writes' on 'restore_defaults == true'
    if (ee_storage.total_writes == 0xffffffff) {
        ee_storage.total_writes = 0;
        ee_is_changed         = true;
    }

    // Read OCXO model first since we'll use it to choose PWM starting point
    if (restore_defaults || ee_storage.ocxo_model > OCXO_MODEL_MAX) {
        ee_storage.ocxo_model = OCXO_MODEL_UNKNOWN;
        ee_is_changed         = true;
    }

    if (restore_defaults || ee_storage.pwm == 0xffff) {
        // Pwm not initialized choose starting point based on OCXO model
        switch(ee_storage.ocxo_model)
        {
            case OCXO_MODEL_OX256B:
                ee_storage.pwm = 44500; // about 2.5V - several of the OX256B units I have show zero error at this control voltage
                break;
            case OCXO_MODEL_ISOTEMP:
            case OCXO_MODEL_UNKNOWN:
            default:
                ee_storage.pwm = 35600; // about 2V - typical center value of OCXO control voltage
                break;
        }
        ee_is_changed         = true;
    }

    if (restore_defaults || ee_storage.brightness == 0xff) {
        ee_storage.brightness = 50;
        ee_is_changed         = true;
    }

    if (restore_defaults || ee_storage.pps_auto_sync == 0xff) {
        ee_storage.pps_auto_sync = true;
        ee_is_changed            = true;
    }

    if (restore_defaults || ee_storage.pps_sync_delay == 0xffffffff) {
        ee_storage.pps_sync_delay = 10;
        ee_is_changed             = true;
    }

    if (restore_defaults || ee_storage.pps_sync_threshold == 0xffffffff) {
        ee_storage.pps_sync_threshold = 30000;
        ee_is_changed                 = true;
    }

    if (restore_defaults || ee_storage.pps_sync_on_ppb_lock == 0xff) {
        ee_storage.pps_sync_on_ppb_lock = true;
        ee_is_changed                   = true;
    }

    if (restore_defaults || ee_storage.trend_h_scale > UI_Trend_HScale_Max) {
        ee_storage.trend_h_scale = UI_Trend_HScale_Auto;
        ee_is_changed            = true;
    }

    if (restore_defaults || ee_storage.trend_v_scale > UI_Trend_VScale_Max) {
        ee_storage.trend_v_scale = UI_Trend_VScale_Auto;
        ee_is_changed            = true;
    }

    // Check for custom gps baudrate
    if (restore_defaults || ee_storage.gps_baudrate == 0xffffffff) {
        ee_storage.gps_baudrate = GPS_DEFAULT_BAUDRATE;
        ee_is_changed           = true;
    }

    if (restore_defaults || ee_storage.gps_time_offset == 0xffffffff) {
        ee_storage.gps_time_offset = -GPS_MIN_TIME_OFFSET;
        ee_is_changed              = true;
    }

    if (restore_defaults || ee_storage.gps_model > GPS_MODEL_MAX) {
        ee_storage.gps_model = GPS_MODEL_UNKNOWN;
        ee_is_changed        = true;
    }

    // PPB lock threshold (*100)
    if (restore_defaults || ee_storage.ppb_lock_threshold == 0xffffffff) {
        ee_storage.ppb_lock_threshold = DEFAULT_PPB_LOCK_THRESHOLD;
        ee_is_changed                 = true;
    }

    // Correction algorithm
    if (restore_defaults || ee_storage.correction_algorithm > CORRECTION_ALGO_MAX) {
        ee_storage.correction_algorithm = CORRECTION_ALGO_ERIC_H_PLUS;
        ee_is_changed                   = true;
    }

    // Correction factor
    if (restore_defaults || ee_storage.correction_factor == 0xffffffff) {
        ee_storage.correction_factor = get_default_correction_factor(ee_storage.correction_algorithm);
        ee_is_changed                = true;
    }

    // Warmup time
    if (restore_defaults || ee_storage.warmup_time_seconds == 0xffffffff) {
        ee_storage.warmup_time_seconds = get_default_warmup_time(ee_storage.ocxo_model);
        ee_is_changed                  = true;
    }

    // PLL output 1 preset
    if (restore_defaults || ee_storage.pll_out1_preset >= pll_out1_preset_count) {
        ee_storage.pll_out1_preset = 0;
        ee_is_changed              = true;
    }

    // PLL output 2 preset
    if (restore_defaults || ee_storage.pll_out2_preset >= pll_out2_preset_count) {
        ee_storage.pll_out2_preset = 1;
        ee_is_changed              = true;
    }

    // PLL output 1 drive strength
    if (restore_defaults || ee_storage.pll_out1_drive_strength > SI5351_DRIVE_STRENGTH_8MA) {
        ee_storage.pll_out1_drive_strength = SI5351_DRIVE_STRENGTH_2MA;
        ee_is_changed                      = true;
    }

    // PLL output 2 drive strength
    if (restore_defaults || ee_storage.pll_out2_drive_strength > SI5351_DRIVE_STRENGTH_8MA) {
        ee_storage.pll_out2_drive_strength = SI5351_DRIVE_STRENGTH_2MA;
        ee_is_changed                      = true;
    }


    if (apply_settings) {
        // Apply settings
        TIM1->CCR2 = ee_storage.pwm;
        set_brightness(ee_storage.brightness);
        gps_time_offset = ee_storage.gps_time_offset+GPS_MIN_TIME_OFFSET;

        pll_configure_output(1, &(pll_out1_presets[ee_storage.pll_out1_preset]), ee_storage.pll_out1_drive_strength);
        pll_configure_output(2, &(pll_out2_presets[ee_storage.pll_out2_preset]), ee_storage.pll_out2_drive_strength);

        gps_setbaudrate(ee_storage.gps_baudrate);
    }
}

static void restore_defaults_handler(UI_MsgBoxButton result)
{
    if (result == UI_MsgBoxButton_Yes) {
        // Reset defaults without applying
        load_settings(true, false);
    }
}

void gpsdo()
{
    // Set the status of the latest message in init_ext_clock()
    bootlog_set_status(true);

    HAL_TIM_Base_Start_IT(&htim2);

    EE_Init(&ee_storage, sizeof(ee_storage_t));
    EE_Read();

    frequency_start_backlight();

    HAL_TIM_Base_Start(&htim4);
    HAL_TIM_Encoder_Start(&htim4, TIM_CHANNEL_ALL);

    // If the encoder button is held, prompt to restore defaults
    if (HAL_GPIO_ReadPin(ROTARY_PRESS_GPIO_Port, ROTARY_PRESS_Pin) == GPIO_PIN_RESET) {
        // Wait for the button to be released
        while (HAL_GPIO_ReadPin(ROTARY_PRESS_GPIO_Port, ROTARY_PRESS_Pin) == GPIO_PIN_RESET) { }
        // Debounce
        HAL_Delay(50);

        static const char* const msg[] = {
            "Encoder held",
            "on boot.",
            "Reset settings?",
            NULL };
        ui_msgbox(msg, UI_MsgBoxType_YesNo, UI_MsgBoxButton_No, restore_defaults_handler);

        // Run the UI loop while the message box is active
        while (ui_get_active_screen() != NULL) {
            ui_run();
        }
    }

    enable_usb();
    gps_start_it();

    load_settings(false, true);

    ui_trend_init();
    ui_show_screen(&ui_main_screen);

    HAL_Delay(100);
    frequency_start_tracking();

    while (1) {
        uint32_t now = HAL_GetTick();
        if(pps_out_up && now-last_pps_out >= PPS_PULSE_WIDTH)
        {
            HAL_GPIO_WritePin(PPS_OUTPUT_GPIO_Port, PPS_OUTPUT_Pin, 0);
            pps_out_up = false;
        }
        if (!frequency_adjustment_allowed() && (now >= (ee_storage.warmup_time_seconds * 1000)))
        {   // Start adjusting the VCO after some time
            frequency_allow_adjustment(true);
        }
        if((now - last_frame_receive_time) > GPS_FRAME_WAIT_DELAY)
        {   // We've not been receiving a frame from GPS for too long, try and restart UART
            gps_reset_uart();
            last_frame_receive_time = now;
        }

        gps_run();
        pll_run();
        ui_trend_run();
        ui_run();

        // Check if we need resync PPS output
        if (!auto_sync_pps_done && ee_storage.pps_sync_on_ppb_lock && frequency_stability == FREQ_STABILITY_STABLE)
        {
            sync_pps_out = true;
            auto_sync_pps_done = true; // Only auto-sync one time per session
        }
    }
}
