#ifndef _UI_HELPERS_78145532759C_H_
#define _UI_HELPERS_78145532759C_H_

#include "ui_core.h"

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>


#define UI_MENU_STR_LEN           22u

#define BATCH_DRAW_STACK_BUF_SIZE 32

// Swap bytes for ST7735 SPI format; used over __REV16 to evaluate constants at compile time
#define ST7735_CLR2MEM(x) ((((uint16_t)(x) & 0x00FF) << 8) | (((uint16_t)(x) & 0xFF00) >> 8))

#define ARRAY_SIZE(arr)           (sizeof(arr) / sizeof((arr)[0]))

// Absolute value of int32_t as uint32_t (handles INT32_MIN)
#define ABS_U32(v)                (((v) < 0) ? -(uint32_t)(v) : (uint32_t)(v))


//------------------------------------------------------------------------------
// Formatting Functions
//------------------------------------------------------------------------------
void ui_format_ppb_5char(int32_t ppb_signed, char* buffer, size_t bufferSize);
void ui_format_ppb_9char(int32_t ppb_signed, char* buffer, size_t bufferSize);
const char* ui_get_month_name_3char(uint8_t month_num);
void ui_format_adc_voltage_fp2i3f_8char(char* buf, size_t buf_char_count, const void* value, const char*);
void ui_format_adc_temperature_fp3i1f_5char(char* buf, size_t buf_char_count, const void* value, const char*);

//------------------------------------------------------------------------------
// Setting Change Functions
//------------------------------------------------------------------------------
void ui_change_setting_i8(int8_t* value, int32_t step, int8_t min, int8_t max, bool wrap_around);
void ui_change_setting_u8(uint8_t* value, int32_t step, uint8_t min, uint8_t max, bool wrap_around);
void ui_change_setting_u16(uint16_t* value, int32_t step, uint16_t min, uint16_t max, bool wrap_around);
void ui_change_setting_u32(uint32_t* value, int32_t step, uint32_t min, uint32_t max, bool wrap_around);

int32_t ui_limit_i32(int32_t value, int32_t min, int32_t max);
int32_t ui_get_adaptive_step(uint32_t value, int32_t encoder_step);

//------------------------------------------------------------------------------
// Standard Element Procedures
//------------------------------------------------------------------------------
typedef void (*UIFormatValue)(char* buf, size_t buf_char_count, const void* value, const char* format_param);

void ui_format_u16(char* buf, size_t buf_char_count, const void* value, const char* fmt);
void ui_format_u32(char* buf, size_t buf_char_count, const void* value, const char* fmt);
void ui_format_i32(char* buf, size_t buf_char_count, const void* value, const char* fmt);

void ui_proc_menu_readonly_common(const UIElement* element, UICommand command, int32_t encoder_step,
    const char* label, uint16_t value_offset, bool redraw_value, const void* value, UIFormatValue format, const char* fmt);
void ui_proc_menu_readonly_custom_u32(const UIElement* element, UICommand command, int32_t encoder_step,
    const char* label, uint32_t* ui_cache, uint32_t value, uint16_t value_offset, UIFormatValue format, const char* format_param);
void ui_proc_menu_readonly_custom_i32(const UIElement* element, UICommand command, int32_t encoder_step,
    const char* label, int32_t* ui_cache, int32_t value, uint16_t value_offset, UIFormatValue format, const char* format_param);
void ui_proc_menu_readonly_u32(const UIElement* element, UICommand command, int32_t encoder_step,
    const char* label, uint32_t* ui_cache, uint32_t value, uint16_t value_offset, const char* fmt);
void ui_proc_menu_readonly_i32(const UIElement* element, UICommand command, int32_t encoder_step,
    const char* label, int32_t* ui_cache, int32_t value, uint16_t value_offset, const char* fmt);
void ui_proc_menu_readonly_custom_u16(const UIElement* element, UICommand command, int32_t encoder_step,
    const char* label, uint16_t* ui_cache, uint16_t value, uint16_t value_offset, UIFormatValue format, const char* format_param);
void ui_proc_menu_readonly_custom_i16(const UIElement* element, UICommand command, int32_t encoder_step,
    const char* label, int16_t* ui_cache, int16_t value, uint16_t value_offset, UIFormatValue format, const char* format_param);

void ui_draw_static_label(const UIElement* element, UICommand command, const char* label, uint16_t color);
void ui_menu_draw_right_aligned(const UIElement* element, uint32_t offset_chars, const char* str, uint16_t text_color);
void ui_draw_coord_dd_right_aligned(const UIElement* element, int32_t coord_deg_x10M);
void ui_draw_coord_ddm_right_aligned(const UIElement* element, const char* nmea_coord, char coord_ns_ew);

void ui_proc_menu_readonly_cstr(const UIElement* element, UICommand command, int32_t encoder_step, const char* label, const char* value);
void ui_proc_menu_edit_custom_u32(const UIElement* element, UICommand command, int32_t encoder_step,
    const char* label, uint32_t* ui_cache, uint32_t* ui_edit, volatile uint32_t* ee_setting, uint16_t value_offset,
    void (*apply_value)(uint32_t), void (*change_value)(uint32_t*, int32_t), UIFormatValue format, const char* format_param);
void ui_proc_menu_edit_u32(const UIElement* element, UICommand command, int32_t encoder_step,
    const char* label, uint32_t* ui_cache, uint32_t* ui_edit, volatile uint32_t* ee_setting, uint16_t value_offset,
    const char* fmt, void (*apply_value)(uint32_t), void (*change_value)(uint32_t*, int32_t));
void ui_proc_menu_edit_enum_u8(const UIElement* element, UICommand command, int32_t encoder_step,
    const char* label, uint8_t* ui_edit, volatile uint8_t* ee_setting, uint16_t value_offset, uint8_t max_value,
    void (*before_apply)(uint8_t), const char* (*value_to_string)(uint8_t));

void ui_proc_menu_link(const UIElement* element, UICommand command, int32_t encoder_step, const char* label, UIScreen* screen);
void ui_proc_icon_navigation_btn(const UIElement* element, UICommand command, int32_t encoder_step, uint16_t icon_width, uint16_t icon_height,
    const uint16_t* icon, UIScreen* target_screen);
void ui_proc_menu_label(const UIElement* element, UICommand command, int32_t encoder_step, const char* label);

void ui_proc_menu_page_switch(const UIElement* element, UICommand command, int32_t encoder_step, const uint16_t* icon_10x15, UIScreen* page);
void ui_proc_menu_page_left(const UIElement* element, UICommand command, int32_t encoder_step, UIScreen* left_page);
void ui_proc_menu_page_right(const UIElement* element, UICommand command, int32_t encoder_step, UIScreen* right_page);
void ui_proc_menu_page_left_inactive(const UIElement* element, UICommand command, int32_t encoder_step);
void ui_proc_menu_page_right_inactive(const UIElement* element, UICommand command, int32_t encoder_step);
void ui_proc_checkbox(const UIElement* element, UICommand command, int32_t encoder_step, const char* label,
    volatile uint8_t* setting, bool is_eeprom_setting);

//------------------------------------------------------------------------------
// LCD Adjustment Patterns
//------------------------------------------------------------------------------
void ui_draw_lcd_adjust_vcom_pattern(const UIElement* element);
void ui_draw_lcd_gamma_test_pattern(const UIElement* element);

#endif
