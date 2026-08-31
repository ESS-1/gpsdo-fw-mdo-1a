#ifndef _EEPROM_E0F460175E35_H_
#define _EEPROM_E0F460175E35_H_

#include "ee.h"
#include <stdint.h>
#include <stdbool.h>

typedef struct
{
    uint32_t total_writes;
    uint16_t pwm;
    uint8_t  brightness;
    /* Reading boolean from EEPROM results in unpredictable behavior so we use a char and cast it to a boolean */
    volatile uint8_t  pps_auto_sync;
    volatile uint32_t pps_sync_delay;
    volatile uint32_t pps_sync_threshold;
    uint8_t  pps_sync_on_ppb_lock;
    uint8_t  trend_h_scale;
    uint8_t  trend_v_scale;
    uint8_t  gps_model;
    uint32_t gps_baudrate;
    uint32_t gps_time_offset;
    volatile uint32_t ppb_lock_threshold;
    uint16_t pll_out1_preset;
    uint16_t pll_out2_preset;
    uint8_t  pll_out1_drive_strength;
    uint8_t  pll_out2_drive_strength;
    uint8_t  ocxo_model;
    volatile uint8_t  correction_algorithm;
    volatile uint32_t correction_factor;
    uint32_t warmup_time_seconds;
} ee_storage_t;

extern ee_storage_t ee_storage;

extern bool ee_is_changed;
bool ee_save_config();

#endif
