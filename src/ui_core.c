#include "ui_core.h"
#include "main.h"
#include "encoder.h"
#include "fonts.h"
#include "st7735.h"
#include "st7735_config.h"

#include <stdio.h>
#include <inttypes.h>

bool ui_show_performance_timer = false;

static UIScreen* ui_current_screen         = NULL;
static UIScreen* ui_screen_to_show         = NULL;
static bool      ui_screen_to_show_pending = false;

#define UI_PERF_TIMER_MEASUREMENT_WINDOW 1000

static void ui_draw_performance_timer(uint32_t time_spent)
{
    if (time_spent > 99999) {
        time_spent = 99999;
    }

    char s[6] = { '\0' };
    snprintf(s, sizeof(s) / sizeof(s[0]), "%5" PRIu32, time_spent);
    ST7735_WriteStringNoWrap(0, 0, 10, s, Font_7x10, ST7735_GREEN, UI_COLOR_BG);
}

static void ui_process_performance_timer(bool init)
{
    static uint32_t start_tick = 0;
    static uint32_t iteration  = UINT32_MAX;

    if (!ui_show_performance_timer || init) {
        iteration = UINT32_MAX;
        return;
    }

    if (iteration >= UI_PERF_TIMER_MEASUREMENT_WINDOW) {
        if (iteration != UINT32_MAX) {
            ui_draw_performance_timer(HAL_GetTick() - start_tick);
        }

        iteration = 0;
        start_tick = HAL_GetTick();
    }
    ++iteration;
}

void ui_default_element_proc(const UIElement* element, UICommand command, int32_t encoder_step)
{
    // Draw/clear frame
    if ((command & (UICommand_Focus | UICommand_LostFocus | UICommand_RestoreFocus | UICommand_Capture | UICommand_Release | UICommand_RestoreCapture))
        && ((element->styles & UI_STYLE_NOFRAME) == 0))
    {
        uint16_t frame_color;

        if (command & (UICommand_Focus | UICommand_Release | UICommand_RestoreFocus)) {
            // If an element releases control, it remains focused
            frame_color = UI_COLOR_FOCUS_FRAME;
        } else if (command & (UICommand_Capture | UICommand_RestoreCapture)) {
            frame_color = UI_COLOR_CAPTURE_FRAME;
        } else {
            frame_color = UI_COLOR_BG;
        }

        uint16_t x1 = element->x - 1;
        uint16_t y1 = element->y - 1;
        uint16_t w  = element->width + 2;
        uint16_t h  = element->height + 2;

        ST7735_FillRectangle(x1,     y1,     1, h, frame_color);
        ST7735_FillRectangle(x1,     y1,     w, 1, frame_color);
        ST7735_FillRectangle(x1+w-1, y1,     1, h, frame_color);
        ST7735_FillRectangle(x1,     y1+h-1, w, 1, frame_color);
    }
}

bool ui_is_captured(const UIElement* element)
{
    return (ui_current_screen != NULL) && ui_current_screen->is_input_captured && (ui_current_screen->focused_element == element);
}

UIScreen* ui_get_active_screen()
{
    return ui_screen_to_show_pending
        ? ui_screen_to_show
        : ui_current_screen;
}

static void ui_force_set_screen(UIScreen* screen)
{
    ST7735_FillRectangle(0, 0, ST7735_WIDTH, ST7735_HEIGHT, UI_COLOR_BG);
    ui_current_screen = screen;

    if (ui_current_screen == NULL) {
        return;
    }

    for (int32_t i = 0; i < screen->num_elements; ++i)
    {
        const UIElement *element = &(screen->elements[i]);
        UICommand        command = UICommand_Init;

        if ((screen->focused_element == NULL) && (element->styles & UI_STYLE_FOCUSABLE)) {
            screen->focused_element = element;
            command |= UICommand_Focus;
        } else if (screen->focused_element == element) {
            command |= (screen->is_input_captured ? UICommand_RestoreCapture : UICommand_RestoreFocus);
        }

        element->proc(element, command, 0);
    }

    // Init/reset performance timer
    ui_process_performance_timer(true);
}

void ui_show_screen(UIScreen* screen)
{
    ui_screen_to_show = screen;
    ui_screen_to_show_pending = true;
}

void ui_run()
{
    if (ui_screen_to_show_pending) {
        // Activate new screen
        ui_force_set_screen(ui_screen_to_show);
        ui_screen_to_show = NULL;
        ui_screen_to_show_pending = false;
    }

    if (ui_current_screen == NULL) {
        return;
    }

    const int32_t num_elements = ui_current_screen->num_elements;
    if (num_elements < 1) {
        return;
    }

    // Read encoder
    bool    click = encoder_get_click();
    int32_t step  = encoder_get_step();
    step          = (step > 0) - (step < 0);

    if (ui_current_screen->focused_element != NULL) {
        if (click) {
            const UIElement* element = ui_current_screen->focused_element;
            if (element->styles & UI_STYLE_INPUT_CAPTURING) {
                // Capture/release input on click
                UICommand command = ui_current_screen->is_input_captured
                    ? UICommand_Release
                    : UICommand_Capture;
                ui_current_screen->is_input_captured = !ui_current_screen->is_input_captured;
                element->proc(element, command, 0);
            } else {
                // Send click command to non-capturing element
                element->proc(element, UICommand_Click, 0);
            }
        } else if (ui_current_screen->is_input_captured && step != 0) {
            // Send step command to the element that captured input
            const UIElement* element = ui_current_screen->focused_element;
            element->proc(element, UICommand_EncoderStep, step);
        }
    }

    if (ui_screen_to_show_pending) {
        // We will show a new screen; no need to update the current controls
        return;
    }

    // Scroll through controls
    if (!click && step != 0 && !ui_current_screen->is_input_captured)
    {
        int32_t current_idx = (ui_current_screen->focused_element != NULL)
            ? ui_current_screen->focused_element - ui_current_screen->elements
            : -1;

        // Find the next element to focus
        int32_t next_element_idx = current_idx;
        int32_t dir = (step < 0) ? -1 : 1;
        int32_t attempts = 0;
        bool    next_element_focusable = false;

        do
        {
            next_element_idx += dir;
            if (next_element_idx < 0) {
                next_element_idx = num_elements - 1;
            } else if (next_element_idx >= num_elements) {
                next_element_idx = 0;
            }

            next_element_focusable = ((ui_current_screen->elements[next_element_idx].styles & UI_STYLE_FOCUSABLE) != 0);
            ++attempts;
        } while (!next_element_focusable && attempts < num_elements);

        // Change focused element
        if (next_element_focusable && next_element_idx != current_idx) {
            const UIElement* focused_element = ui_current_screen->focused_element;
            const UIElement* next_element    = &(ui_current_screen->elements[next_element_idx]);

            ui_current_screen->focused_element = next_element;

            if (focused_element != NULL) {
                focused_element->proc(focused_element, UICommand_LostFocus, 0);
            }

            next_element->proc(next_element, UICommand_Focus, 0);
        }
    }

    // Update all controls
    for (int32_t i = 0; i < num_elements; ++i) {
        const UIElement* element = &(ui_current_screen->elements[i]);
        element->proc(element, UICommand_None, 0);
    }

    // Show performance timer
    ui_process_performance_timer(false);
}
