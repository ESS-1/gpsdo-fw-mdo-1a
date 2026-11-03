#include "ui_helpers.h"
#include "frequency.h"
#include "icons.h"
#include "eeprom.h"

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
            snprintf(buffer, bufferSize, "%5" PRId32, sign * (int32_t)((ppb + 50u) / 100u));
        } else if (ppb < 99950000u) {
            snprintf(buffer, bufferSize, "%4" PRId32 "K", sign * (int32_t)((ppb + 50000u) / 100000u));
        } else {
            snprintf(buffer, bufferSize, "%4" PRId32 "M", sign * (int32_t)((ppb + 50000000u) / 100000000u));
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

void ui_proc_menu_readonly_common(const UIElement* element, UICommand command, int32_t encoder_step,
    const char* label, uint16_t value_offset, bool redraw_value, const void* value, UIReadonlyFormat format, const char* fmt)
{
    // Draw label
    ui_draw_static_label(element, command, label, UI_COLOR_MENU_LABEL);

    if (redraw_value) {
        char s[UI_MENU_VALUE_BUFFER_SIZE] = { '\0' };

        if (format != NULL) {
            format(s, ARRAY_SIZE(s), value, fmt);
        }

        ST7735_WriteStringNoWrap(element->x + value_offset * 7, element->y + 1, element->height - 1, s, Font_7x10, UI_COLOR_MENU_LABEL, UI_COLOR_BG);
    }

    ui_default_element_proc(element, command, encoder_step);
}

void ui_proc_menu_readonly_custom_u32(const UIElement* element, UICommand command, int32_t encoder_step,
    const char* label, uint32_t* ui_cache, uint32_t value, uint16_t value_offset, UIReadonlyFormat format, const char* format_param)
{
    bool redraw = (command & UICommand_Init) || (value != *ui_cache);
    if (redraw) {
        *ui_cache = value;
    }

    ui_proc_menu_readonly_common(element, command, encoder_step, label, value_offset, redraw, &value, format, format_param);
}

void ui_proc_menu_readonly_custom_i32(const UIElement* element, UICommand command, int32_t encoder_step,
    const char* label, int32_t* ui_cache, int32_t value, uint16_t value_offset, UIReadonlyFormat format, const char* format_param)
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
    const char* label, uint16_t* ui_cache, uint16_t value, uint16_t value_offset, UIReadonlyFormat format, const char* format_param)
{
    bool redraw = (command & UICommand_Init) || (value != *ui_cache);
    if (redraw) {
        *ui_cache = value;
    }

    ui_proc_menu_readonly_common(element, command, encoder_step, label, value_offset, redraw, &value, format, format_param);
}

void ui_proc_menu_readonly_custom_i16(const UIElement* element, UICommand command, int32_t encoder_step,
    const char* label, int16_t* ui_cache, int16_t value, uint16_t value_offset, UIReadonlyFormat format, const char* format_param)
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

void ui_proc_menu_edit_custom_u32(const UIElement* element, UICommand command, int32_t encoder_step,
    const char* label, uint32_t* ui_cache, uint32_t* ui_edit, volatile uint32_t* ee_setting, uint16_t value_offset,
    void (*apply_value)(uint32_t), void (*change_value)(uint32_t*, int32_t), UIReadonlyFormat format, const char* format_param)
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
        if (ui_cache != NULL) {
            *ui_cache = *value_to_draw;
        }

        char s[UI_MENU_VALUE_BUFFER_SIZE] = { '\0' };

        if (format != NULL) {
            format(s, ARRAY_SIZE(s), (const void*)value_to_draw, format_param);
        }
        ST7735_WriteStringNoWrap(element->x + value_offset * 7, element->y + 1, element->height - 1, s, Font_7x10, UI_COLOR_TEXT, UI_COLOR_BG);
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
