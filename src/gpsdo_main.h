#pragma once

#include <stdbool.h>
#include <stdint.h>

void init_ext_clock();
void set_lcd_vcom(uint16_t vcom);
void load_settings(bool restore_defaults, bool apply_settings);
void gpsdo();
