#include "ui_helpers.h"
#include "frequency.h"
#include "icons.h"
#include "eeprom.h"
#include "adc_mon.h"
#include "gps.h"

#include <inttypes.h>
#include <memory.h>
#include <stdio.h>


#define UI_MENU_VALUE_BUFFER_SIZE (UI_MENU_STR_LEN+1)

//------------------------------------------------------------------------------
// Formatting Functions
//------------------------------------------------------------------------------
void ui_format_ppb_5char(int32_t ppb_signed, char* buffer, size_t bufferSize)
{
    if (buffer == NULL || bufferSize == 0) {
        return;
    }

    if (bufferSize < 6) {
        memset(buffer, '#', bufferSize - 1);
        buffer[bufferSize - 1] = '\0';
        return;
    }

    if (ppb_signed == PPB_UNSET_VALUE) {
        snprintf(buffer, bufferSize, " N/A ");
        return;
    }

    uint32_t ppb = ABS_U32(ppb_signed);

    if (ppb < 9995u) {
        // Add sign
        buffer[0] = ppb_signed >= 0 ? ' ' : '-';

        // Format numeric value
        if (ppb < 1000u) {
            snprintf(buffer + 1, bufferSize - 1, "%" PRIu32 ".%02" PRIu32, ppb / 100u, ppb % 100u);
        } else {
            uint32_t tenths = (ppb + 5u) / 10u;
            snprintf(buffer + 1, bufferSize - 1, "%" PRIu32 ".%01" PRIu32, tenths / 10u, tenths % 10u);
        }
    } else {
        int32_t sign = ppb_signed < 0 ? -1 : 1;
        if (ppb < 999950u) {
            snprintf(buffer, bufferSize, "%5" PRIi32, sign * (int32_t)((ppb + 50u) / 100u));
        } else if (ppb < 99950000u) {
            snprintf(buffer, bufferSize, "%4" PRIi32 "K", sign * (int32_t)((ppb + 50000u) / 100000u));
        } else {
            snprintf(buffer, bufferSize, "%4" PRIi32 "M", sign * (int32_t)((ppb + 50000000u) / 100000000u));
        }
    }
}

void ui_format_ppb_9char(int32_t ppb_signed, char* buffer, size_t bufferSize)
{
    if (buffer == NULL || bufferSize == 0) {
        return;
    }

    if (bufferSize < 10) {
        memset(buffer, '#', bufferSize - 1);
        buffer[bufferSize - 1] = '\0';
        return;
    }

    if (ppb_signed == PPB_UNSET_VALUE) {
        strlcpy(buffer, "      N/A", bufferSize);
        return;
    }
    if (ppb_signed > 9999999) {
        strlcpy(buffer, "   > 100K", bufferSize);
        return;
    }
    if (ppb_signed < -9999999) {
        strlcpy(buffer, "  < -100K", bufferSize);
        return;
    }

    uint32_t ppb_abs = ABS_U32(ppb_signed);
    if (ppb_signed < 0 && ppb_signed > -100) {
        snprintf(buffer, bufferSize, "    -0.%02" PRIu32, ppb_abs);
    } else {
        snprintf(buffer, bufferSize, "%6" PRIi32 ".%02" PRIu32, ppb_signed / 100, ppb_abs % 100u);
    }
}

const char* ui_get_month_name_3char(uint8_t month_num)
{
    static const char months[13][4] = { "###", // Fallback for invalid month number
        "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };

    if (month_num < 1 || month_num > 12) {
        return months[0];
    }

    return months[month_num];
}

void ui_format_adc_voltage_fp2i3f_u16_8char(char* buf, size_t buf_char_count, const void* value, const char*)
{
    uint16_t mv = *(const uint16_t*)value;

    if (mv != ADC_MON_VOLTAGE_NO_DATA) {
        snprintf(buf, buf_char_count, "%2" PRIu16 ".%03" PRIu16 " V", (uint16_t)(mv / 1000), (uint16_t)(mv % 1000));
    } else {
        strlcpy(buf, "   N/A V", buf_char_count);
    }
}

void ui_format_adc_temperature_fp3i1f_i16_5char(char* buf, size_t buf_char_count, const void* value, const char*)
{
    int16_t temp = *(const int16_t*)value;

    if (temp != ADC_MON_TEMP_NO_DATA) {
        if (temp < 0 && temp > -10) {
            snprintf(buf, buf_char_count, " -0.%01" PRIi16, (int16_t)(-temp % 10));
        } else {
            snprintf(buf, buf_char_count, "%3" PRIi16 ".%01" PRIi16, (int16_t)(temp / 10), (int16_t)abs(temp % 10));
        }
    } else {
        strlcpy(buf, "  N/A", buf_char_count);
    }
}


//------------------------------------------------------------------------------
// Setting Change Functions
//------------------------------------------------------------------------------
void ui_change_setting_i8(int8_t* value, int32_t step, int8_t min, int8_t max, bool wrap_around)
{
    int32_t v = (int32_t)(*value) + step;

    if (v > max) {
        *value = wrap_around ? min : max;
    } else if (v < min) {
        *value = wrap_around ? max : min;
    } else {
        *value = (int8_t)v;
    }
}

void ui_change_setting_u8(uint8_t* value, int32_t step, uint8_t min, uint8_t max, bool wrap_around)
{
    int32_t v = (int32_t)(*value) + step;

    if (v > max) {
        *value = wrap_around ? min : max;
    } else if (v < min) {
        *value = wrap_around ? max : min;
    } else {
        *value = (uint8_t)v;
    }
}

void ui_change_setting_u16(uint16_t* value, int32_t step, uint16_t min, uint16_t max, bool wrap_around)
{
    int32_t v = (int32_t)(*value) + step;

    if (v > max) {
        *value = wrap_around ? min : max;
    } else if (v < min) {
        *value = wrap_around ? max : min;
    } else {
        *value = (uint16_t)v;
    }
}

void ui_change_setting_u32(uint32_t* value, int32_t step, uint32_t min, uint32_t max, bool wrap_around)
{
    int64_t v = (int64_t)(*value) + step;

    if (v > (int64_t)max) {
        *value = wrap_around ? min : max;
    } else if (v < (int64_t)min) {
        *value = wrap_around ? max : min;
    } else {
        *value = (uint32_t)v;
    }
}

int32_t ui_limit_i32(int32_t value, int32_t min, int32_t max)
{
    if (value < min) {
        return min;
    }
    if (value > max) {
        return max;
    }
    return value;
}

// Calculates an adaptive step increment (1, 2, 5, 10, 20, 50, 100, etc.)
// based on the magnitude of the current value and encoder direction.
// The step dynamically scales using a 1-2-5 progression per decade.
int32_t ui_get_adaptive_step(uint32_t value, int32_t encoder_step)
{
    int32_t sign = (encoder_step < 0) ? -1 : 1;

    // Shift boundary evaluation by -1 when decrementing to ensure exact
    // reverse symmetry at range transition points
    uint32_t effective_value = (sign < 0 && value > 0) ? (value - 1) : value;

    // Base step size for values below 10
    if (effective_value < 10) {
        return 1 * sign;
    }

    // Normalize value into the [10..99] decade range and calculate power of 10 scale
    uint32_t norm_val = effective_value;
    uint32_t scale    = 1;
    while (norm_val >= 100) {
        norm_val /= 10;
        scale *= 10;
    }

    // Determine step size based on 1-2-5 decade progression
    if (norm_val < 20) {
        return 1 * sign * scale;
    } else if (norm_val < 50) {
        return 2 * sign * scale;
    } else {
        return 5 * sign * scale;
    }
}


//------------------------------------------------------------------------------
// Standard Element Procedures
//------------------------------------------------------------------------------
void ui_format_i8(char* buf, size_t buf_char_count, const void* value, const char* fmt)
{
    snprintf(buf, buf_char_count, fmt, (unsigned int)*(const int8_t*)value);
}

void ui_format_u16(char* buf, size_t buf_char_count, const void* value, const char* fmt)
{
    snprintf(buf, buf_char_count, fmt, *(const uint16_t*)value);
}

void ui_format_u32(char* buf, size_t buf_char_count, const void* value, const char* fmt)
{
    snprintf(buf, buf_char_count, fmt, *(const uint32_t*)value);
}

void ui_format_i32(char* buf, size_t buf_char_count, const void* value, const char* fmt)
{
    snprintf(buf, buf_char_count, fmt, *(const int32_t*)value);
}

static void ui_menu_custom_draw_value(const UIElement* element, uint16_t value_offset, uint16_t text_color,
    const void* value, UIFormatValue format, const char* fmt)
{
    char s[UI_MENU_VALUE_BUFFER_SIZE] = { '\0' };

    if (format != NULL) {
        format(s, ARRAY_SIZE(s), value, fmt);
    }

    ST7735_WriteStringNoWrap(element->x + value_offset * 7, element->y + 1, element->height - 1, s, Font_7x10, text_color, UI_COLOR_BG);
}

void ui_proc_menu_readonly_common(const UIElement* element, UICommand command, int32_t encoder_step,
    const char* label, uint16_t value_offset, bool redraw_value, const void* value, UIFormatValue format, const char* fmt)
{
    // Draw label
    ui_draw_static_label(element, command, label, UI_COLOR_MENU_LABEL);

    if (redraw_value) {
        ui_menu_custom_draw_value(element, value_offset, UI_COLOR_MENU_LABEL, value, format, fmt);
    }

    ui_default_element_proc(element, command, encoder_step);
}

void ui_proc_menu_readonly_custom_u32(const UIElement* element, UICommand command, int32_t encoder_step,
    const char* label, uint32_t* ui_cache, uint32_t value, uint16_t value_offset, UIFormatValue format, const char* format_param)
{
    bool redraw = (command & UICommand_Init) || (value != *ui_cache);
    if (redraw) {
        *ui_cache = value;
    }

    ui_proc_menu_readonly_common(element, command, encoder_step, label, value_offset, redraw, &value, format, format_param);
}

void ui_proc_menu_readonly_custom_i32(const UIElement* element, UICommand command, int32_t encoder_step,
    const char* label, int32_t* ui_cache, int32_t value, uint16_t value_offset, UIFormatValue format, const char* format_param)
{
    bool redraw = (command & UICommand_Init) || (value != *ui_cache);
    if (redraw) {
        *ui_cache = value;
    }

    ui_proc_menu_readonly_common(element, command, encoder_step, label, value_offset, redraw, &value, format, format_param);
}

void ui_proc_menu_readonly_u32(const UIElement* element, UICommand command, int32_t encoder_step,
    const char* label, uint32_t* ui_cache, uint32_t value, uint16_t value_offset, const char* fmt)
{
    ui_proc_menu_readonly_custom_u32(element, command, encoder_step, label, ui_cache, value, value_offset, ui_format_u32, fmt);
}

void ui_proc_menu_readonly_i32(const UIElement* element, UICommand command, int32_t encoder_step,
    const char* label, int32_t* ui_cache, int32_t value, uint16_t value_offset, const char* fmt)
{
    ui_proc_menu_readonly_custom_i32(element, command, encoder_step, label, ui_cache, value, value_offset, ui_format_i32, fmt);
}

void ui_proc_menu_readonly_custom_u16(const UIElement* element, UICommand command, int32_t encoder_step,
    const char* label, uint16_t* ui_cache, uint16_t value, uint16_t value_offset, UIFormatValue format, const char* format_param)
{
    bool redraw = (command & UICommand_Init) || (value != *ui_cache);
    if (redraw) {
        *ui_cache = value;
    }

    ui_proc_menu_readonly_common(element, command, encoder_step, label, value_offset, redraw, &value, format, format_param);
}

void ui_proc_menu_readonly_custom_i16(const UIElement* element, UICommand command, int32_t encoder_step,
    const char* label, int16_t* ui_cache, int16_t value, uint16_t value_offset, UIFormatValue format, const char* format_param)
{
    bool redraw = (command & UICommand_Init) || (value != *ui_cache);
    if (redraw) {
        *ui_cache = value;
    }

    ui_proc_menu_readonly_common(element, command, encoder_step, label, value_offset, redraw, &value, format, format_param);
}

void ui_draw_static_label(const UIElement* element, UICommand command, const char* label, uint16_t color)
{
    if (command & UICommand_Init) {
        ST7735_WriteStringNoWrap(element->x, element->y + 1, element->height - 1, label, Font_7x10, color, UI_COLOR_BG);
    }
}

void ui_menu_draw_right_aligned(const UIElement* element, uint32_t offset_chars, const char* str, uint16_t text_color)
{
    if (offset_chars >= UI_MENU_STR_LEN) {
        return;
    }

    const uint32_t max_len = UI_MENU_STR_LEN - offset_chars;
    size_t         len     = strlen(str);

    if (len > max_len) {
        len = max_len;
    }

    if (len < max_len) {
        ST7735_FillRectangle(element->x + offset_chars * 7, element->y + 1, (max_len - len) * 7, element->height - 1, UI_COLOR_BG);
    }

    ST7735_WriteStringNoWrap(element->x + (UI_MENU_STR_LEN - len) * 7, element->y + 1, element->height - 1, str, Font_7x10, text_color, UI_COLOR_BG);
}

void ui_draw_coord_dd_right_aligned(const UIElement* element, int32_t coord_deg_x10M)
{
    char s[13] = { '\0' };

    if (coord_deg_x10M != PPB_EMPTY_DEG_X10M_COORD) {
        int32_t coord_abs   = (coord_deg_x10M < 0) ? -coord_deg_x10M : coord_deg_x10M;
        int32_t coord_int   = coord_abs / 10000000;
        int32_t coord_fract = coord_abs % 10000000;

        const char* sign = (coord_deg_x10M < 0) ? "-" : "";
        snprintf(s, ARRAY_SIZE(s), "%s%" PRIi32 ".%07" PRIi32, sign, coord_int, coord_fract);
    }

    uint16_t len    = (uint16_t)strlen(s);
    uint16_t offset = UI_MENU_STR_LEN - len;

    if (len < 12) {
        uint16_t empty_chars = 12 - len;
        ST7735_FillRectangle(element->x + 10 * 7, element->y + 1, empty_chars * 7, element->height - 1, UI_COLOR_BG);
    }

    ST7735_WriteStringNoWrap(element->x + offset * 7, element->y + 1, element->height - 1, s, Font_7x10, UI_COLOR_MENU_LABEL, UI_COLOR_BG);
}

void ui_draw_coord_ddm_right_aligned(const UIElement* element, const char* nmea_coord, char coord_ns_ew)
{
    char      min_str[20] = { 0 };
    char      deg_buf[4]  = { '\0' };
    char*     deg_str     = deg_buf;
    size_t    min_len     = 0;
    ptrdiff_t deg_len     = 0;

    // Find the decimal point separating whole minutes and fractional minutes
    const char* dot = strchr(nmea_coord, '.');
    if (dot != NULL && (dot - nmea_coord >= 2)) {
        // Minutes always occupy exactly two numeric characters immediately preceding the decimal point
        const char* min_start = dot - 2;

        // Format minutes and 'N/S'/'E/W' string
        snprintf(min_str, ARRAY_SIZE(min_str), " %s' %c", min_start, coord_ns_ew);
        min_len = strlen(min_str);

        // Format degrees
        deg_len = min_start - nmea_coord;
        if (deg_len < (ptrdiff_t)ARRAY_SIZE(deg_buf)) {
            strncpy(deg_str, nmea_coord, deg_len);
            deg_str[deg_len] = '\0';

            // Remove leading zeros
            while ((*deg_str == '0') && (deg_len > 1)) {
                ++deg_str;
                --deg_len;
            }
        } else {
            deg_len = 0;
        }
    }

    // Draw minutes
    uint16_t pos = UI_MENU_STR_LEN;
    if (pos >= min_len && min_len > 0) {
        pos -= min_len;
        ST7735_WriteStringNoWrap(element->x + pos * 7, element->y + 1, element->height - 1, min_str, Font_7x10, UI_COLOR_MENU_LABEL, UI_COLOR_BG);
    }

    // Draw degrees
    if (deg_len > 0) {
        // Draw degree sign
        if (pos >= 1) {
            --pos;
            ST7735_DrawImage(element->x + pos * 7, element->y + 1, 7, 10, icon_symbol_degree_7x10);
        }

        // Draw value
        if (pos >= deg_len) {
            pos -= deg_len;
            ST7735_WriteStringNoWrap(element->x + pos * 7, element->y + 1, element->height - 1, deg_str, Font_7x10, UI_COLOR_MENU_LABEL, UI_COLOR_BG);
        }
    }

    // Clear remaining space
    if (pos > 0) {
        ST7735_FillRectangle(element->x, element->y + 1, pos * 7, element->height - 1, UI_COLOR_BG);
    }
}

void ui_proc_menu_readonly_cstr(const UIElement* element, UICommand command, int32_t encoder_step, const char* label, const char* value)
{
    // Draw label
    ui_draw_static_label(element, command, label, UI_COLOR_MENU_LABEL);

    // Draw value
    if (command & UICommand_Init) {
        size_t len = strlen(value);
        if (len > UI_MENU_STR_LEN) {
            len = UI_MENU_STR_LEN;
        }

        ST7735_WriteStringNoWrap(element->x + (UI_MENU_STR_LEN - len) * 7, element->y + 1, element->height - 1, value, Font_7x10, UI_COLOR_MENU_LABEL, UI_COLOR_BG);
    }

    ui_default_element_proc(element, command, encoder_step);
}

void ui_proc_menu_edit_custom_i8(const UIElement* element, UICommand command, int32_t encoder_step,
    const char* label, int8_t* ui_cache, int8_t* ui_edit, volatile int8_t* ee_setting, uint16_t value_offset,
    void (*apply_value)(int8_t), void (*change_value)(int8_t*, int32_t), UIFormatValue format, const char* format_param)
{
    volatile const int8_t *value_to_draw = NULL;
    bool is_captured = ui_is_captured(element);

    if (command & UICommand_Release) {
        // Apply changes
        if (*ee_setting != *ui_edit) {
            *ee_setting = *ui_edit;
            ee_is_changed = true;

            if (apply_value != NULL) {
                apply_value(*ui_edit);
            }
        }
    }

    // Draw label
    ui_draw_static_label(element, command, label, UI_COLOR_MENU_LABEL);

    if (command & UICommand_Init) {
        // Draw value
        if (!is_captured) {
            value_to_draw = ee_setting;
        } else {
            value_to_draw = ui_edit;
        }
    }

    if ((ui_cache != NULL) && !is_captured && (*ee_setting != *ui_cache)) {
        value_to_draw = ee_setting;
    }

    // Value edit
    {
        if (command & UICommand_Capture) {
            *ui_edit = *ee_setting;
        }

        if (command & UICommand_EncoderStep) {
            // Modify setting
            if (change_value != NULL) {
                change_value(ui_edit, encoder_step);
            }

            // Draw new value
            value_to_draw = ui_edit;
        }
    }

    // Draw value
    if (value_to_draw) {
        int8_t drawn_value = *value_to_draw;
        if (ui_cache != NULL) {
            *ui_cache = drawn_value;
        }

        ui_menu_custom_draw_value(element, value_offset, UI_COLOR_TEXT, (const void*)&drawn_value, format, format_param);
    }

    ui_default_element_proc(element, command, encoder_step);
}

void ui_proc_menu_edit_i8(const UIElement* element, UICommand command, int32_t encoder_step,
    const char* label, int8_t* ui_cache, int8_t* ui_edit, volatile int8_t* ee_setting, uint16_t value_offset,
    const char* fmt, void (*apply_value)(int8_t), void (*change_value)(int8_t*, int32_t))
{
    ui_proc_menu_edit_custom_i8(element, command, encoder_step,
        label, ui_cache, ui_edit, ee_setting, value_offset,
        apply_value, change_value, ui_format_i8, fmt);
}

void ui_proc_menu_edit_custom_u32(const UIElement* element, UICommand command, int32_t encoder_step,
    const char* label, uint32_t* ui_cache, uint32_t* ui_edit, volatile uint32_t* ee_setting, uint16_t value_offset,
    void (*apply_value)(uint32_t), void (*change_value)(uint32_t*, int32_t), UIFormatValue format, const char* format_param)
{
    volatile const uint32_t *value_to_draw = NULL;
    bool is_captured = ui_is_captured(element);

    if (command & UICommand_Release) {
        // Apply changes
        if (*ee_setting != *ui_edit) {
            *ee_setting = *ui_edit;
            ee_is_changed = true;

            if (apply_value != NULL) {
                apply_value(*ui_edit);
            }
        }
    }

    // Draw label
    ui_draw_static_label(element, command, label, UI_COLOR_MENU_LABEL);

    if (command & UICommand_Init) {
        // Draw value
        if (!is_captured) {
            value_to_draw = ee_setting;
        } else {
            value_to_draw = ui_edit;
        }
    }

    if ((ui_cache != NULL) && !is_captured && (*ee_setting != *ui_cache)) {
        value_to_draw = ee_setting;
    }

    // Value edit
    {
        if (command & UICommand_Capture) {
            *ui_edit = *ee_setting;
        }

        if (command & UICommand_EncoderStep) {
            // Modify setting
            if (change_value != NULL) {
                change_value(ui_edit, encoder_step);
            }

            // Draw new value
            value_to_draw = ui_edit;
        }
    }

    // Draw value
    if (value_to_draw) {
        uint32_t drawn_value = *value_to_draw;
        if (ui_cache != NULL) {
            *ui_cache = drawn_value;
        }

        ui_menu_custom_draw_value(element, value_offset, UI_COLOR_TEXT, (const void*)&drawn_value, format, format_param);
    }

    ui_default_element_proc(element, command, encoder_step);
}

void ui_proc_menu_edit_u32(const UIElement* element, UICommand command, int32_t encoder_step,
    const char* label, uint32_t* ui_cache, uint32_t* ui_edit, volatile uint32_t* ee_setting, uint16_t value_offset,
    const char* fmt, void (*apply_value)(uint32_t), void (*change_value)(uint32_t*, int32_t))
{
    ui_proc_menu_edit_custom_u32(element, command, encoder_step,
        label, ui_cache, ui_edit, ee_setting, value_offset,
        apply_value, change_value, ui_format_u32, fmt);
}

void ui_proc_menu_edit_enum_u8(const UIElement* element, UICommand command, int32_t encoder_step,
    const char* label, uint8_t* ui_edit, volatile uint8_t* ee_setting, uint16_t value_offset, uint8_t max_value,
    void (*before_apply)(uint8_t), const char* (*value_to_string)(uint8_t))
{
    volatile const uint8_t* value_to_draw = NULL;

    if (command & UICommand_Release) {
        // Apply changes
        if (*ee_setting != *ui_edit) {
            if (before_apply != NULL) {
                before_apply(*ui_edit);
            }

            *ee_setting = *ui_edit;
            ee_is_changed = true;
        }
    }

    // Draw label
    ui_draw_static_label(element, command, label, UI_COLOR_MENU_LABEL);

    if (command & UICommand_Init) {
        // Draw value
        if (!ui_is_captured(element)) {
            value_to_draw = ee_setting;
        } else {
            value_to_draw = ui_edit;
        }
    }

    // Value selection
    {
        if (command & UICommand_Capture) {
            *ui_edit = *ee_setting;
        }

        if (command & UICommand_EncoderStep) {
            // Modify setting
            ui_change_setting_u8(ui_edit, encoder_step, 0, max_value, true);

            // Draw new value
            value_to_draw = ui_edit;
        }
    }

    // Draw value
    if (value_to_draw) {
        ui_menu_draw_right_aligned(element, value_offset, value_to_string(*value_to_draw), UI_COLOR_TEXT);
    }

    ui_default_element_proc(element, command, encoder_step);
}

void ui_proc_menu_link(const UIElement* element, UICommand command, int32_t encoder_step, const char* label, UIScreen* screen)
{
    // Draw label
    ui_draw_static_label(element, command, label, UI_COLOR_LINK);

    if (command & UICommand_Click) {
        ui_show_screen(screen);
    }

    ui_default_element_proc(element, command, encoder_step);
}

void ui_proc_icon_navigation_btn(const UIElement* element, UICommand command, int32_t encoder_step, uint16_t icon_width, uint16_t icon_height,
    const uint16_t* icon, UIScreen* target_screen)
{
    if (command & UICommand_Init) {
        ST7735_DrawImage(element->x, element->y, icon_width, icon_height, icon);
    }

    if ((command & UICommand_Click) && target_screen != NULL) {
        ui_show_screen(target_screen);
    }

    ui_default_element_proc(element, command, encoder_step);
}

void ui_proc_menu_label(const UIElement* element, UICommand command, int32_t encoder_step, const char* label)
{
    if (command & UICommand_Init) {
        ST7735_WriteStringNoWrap(element->x, element->y, 10, label, Font_7x10, UI_COLOR_MENU_LABEL, UI_COLOR_BG);
    }

    ui_default_element_proc(element, command, encoder_step);
}

void ui_proc_menu_page_switch(const UIElement* element, UICommand command, int32_t encoder_step, const uint16_t* icon_10x15, UIScreen* page)
{
    ui_proc_icon_navigation_btn(element, command, encoder_step, 10, 15, icon_10x15, page);
}

void ui_proc_menu_page_left(const UIElement* element, UICommand command, int32_t encoder_step, UIScreen* left_page)
{
    ui_proc_menu_page_switch(element, command, encoder_step, icon_page_left_10x15, left_page);
}

void ui_proc_menu_page_right(const UIElement* element, UICommand command, int32_t encoder_step, UIScreen* right_page)
{
    ui_proc_menu_page_switch(element, command, encoder_step, icon_page_right_10x15, right_page);
}

void ui_proc_menu_page_left_inactive(const UIElement* element, UICommand command, int32_t encoder_step)
{
    ui_proc_menu_page_switch(element, command, encoder_step, icon_page_left_inactive_10x15, NULL);
}

void ui_proc_menu_page_right_inactive(const UIElement* element, UICommand command, int32_t encoder_step)
{
    ui_proc_menu_page_switch(element, command, encoder_step, icon_page_right_inactive_10x15, NULL);
}

void ui_proc_checkbox(const UIElement* element, UICommand command, int32_t encoder_step, const char* label,
    volatile uint8_t* setting, bool is_eeprom_setting)
{
    // Draw label
    ui_draw_static_label(element, command, label, UI_COLOR_MENU_LABEL);

    if (command & UICommand_Click) {
        (*setting) = !(*setting);

        if (is_eeprom_setting) {
            ee_is_changed = true;
        }
    }

    // Draw check box
    if (command & (UICommand_Init | UICommand_Click)) {
        ST7735_DrawImage(element->x + 143, element->y, 11, 11, (*setting) ? icon_check_set_11x11 : icon_check_unset_11x11);
    }

    ui_default_element_proc(element, command, encoder_step);
}

//------------------------------------------------------------------------------
// LCD Adjustment Patterns
//------------------------------------------------------------------------------
void ui_draw_lcd_adjust_vcom_pattern(const UIElement* element)
{
    uint16_t w = element->width;
    uint16_t h = element->height;

    uint16_t color_dark  = ST7735_CLR2MEM(UI_COLOR_VCOM_DARK);
    uint16_t color_light = ST7735_CLR2MEM(UI_COLOR_VCOM_LIGHT);

    uint16_t chunk_buf[BATCH_DRAW_STACK_BUF_SIZE];
    uint16_t chunk_buf_idx = 0;

    ST7735_Batch_Begin(element->x, element->y, w, h);
    for (uint16_t y = 0; y < h; ++y) {
        for (uint16_t x = 0; x < w; ++x) {
            chunk_buf[chunk_buf_idx++] = ((x + y) & 1) ? color_dark : color_light;

            // Flush chunk when buffer is full
            if (chunk_buf_idx >= BATCH_DRAW_STACK_BUF_SIZE) {
                ST7735_Batch_Draw(chunk_buf, BATCH_DRAW_STACK_BUF_SIZE);
                chunk_buf_idx = 0;
            }
        }
    }

    // Flush remaining pixels at the end
    if (chunk_buf_idx > 0) {
        ST7735_Batch_Draw(chunk_buf, chunk_buf_idx);
    }

    ST7735_Batch_End();
}

void ui_draw_lcd_gamma_test_pattern(const UIElement* element)
{
    uint16_t w = element->width;
    uint16_t h = element->height;

    uint16_t chunk_buf[BATCH_DRAW_STACK_BUF_SIZE];

    ST7735_Batch_Begin(element->x, element->y, w, h);
    for (uint16_t y = 0; y < h; ++y) {
        // Determine color mask based on vertical position
        uint16_t mask = 0;
        if (y < 10 || y >= 70) {
            mask = 0xFFFF; // Grayscale
        } else if (y < 20) {
            mask = 0xF800; // Red
        } else if (y < 30) {
            mask = 0xFFE0; // Yellow
        } else if (y < 40) {
            mask = 0x07E0; // Green
        } else if (y < 50) {
            mask = 0x07FF; // Cyan
        } else if (y < 60) {
            mask = 0x001F; // Blue
        } else {
            mask = 0xF81F; // Magenta
        }

        // Draw horizontal gradient
        uint16_t chunk_buf_idx = 0;
        for (uint16_t x = 0; x < w; ++x) {
            uint32_t t = (w > 1) ? (255U * x / (w - 1)) : 0;
            chunk_buf[chunk_buf_idx++] = __REV16(ST7735_COLOR565(t, t, t) & mask);

            if (chunk_buf_idx >= BATCH_DRAW_STACK_BUF_SIZE) {
                ST7735_Batch_Draw(chunk_buf, BATCH_DRAW_STACK_BUF_SIZE);
                chunk_buf_idx = 0;
            }
        }

        if (chunk_buf_idx > 0) {
            ST7735_Batch_Draw(chunk_buf, chunk_buf_idx);
        }
    }

    ST7735_Batch_End();
}
