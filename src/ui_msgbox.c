#include "ui_msgbox.h"

#include "icons.h"
#include "fonts.h"
#include "st7735.h"

#include <stdlib.h>


static void ui_msgbox_proc_back(const UIElement* element, UICommand command, int32_t encoder_step);
static void ui_msgbox_proc_label(const UIElement* element, UICommand command, int32_t encoder_step);
static void ui_msgbox_proc_ok(const UIElement* element, UICommand command, int32_t encoder_step);
static void ui_msgbox_proc_yes(const UIElement* element, UICommand command, int32_t encoder_step);
static void ui_msgbox_proc_no(const UIElement* element, UICommand command, int32_t encoder_step);


// Message Box screen
static const UIElement ui_msgbox_ok_screen_elements[] = {
    { 1,  1, 15,  16, UI_STYLE_FOCUSABLE, ui_msgbox_proc_back  },
    { 22, 6, 133, 34, UI_STYLE_NONE,      ui_msgbox_proc_label },
    { 70, 48, 38, 21, UI_STYLE_FOCUSABLE, ui_msgbox_proc_ok    },
};

static const UIElement ui_msgbox_yesno_screen_elements[] = {
    { 1,  1, 15,  16, UI_STYLE_FOCUSABLE, ui_msgbox_proc_back  },
    { 22, 6, 133, 34, UI_STYLE_NONE,      ui_msgbox_proc_label },
    { 43, 48, 39, 21, UI_STYLE_FOCUSABLE, ui_msgbox_proc_yes   },
    { 95, 48, 38, 21, UI_STYLE_FOCUSABLE, ui_msgbox_proc_no    },
};

static UIScreen ui_msgbox_screen = {
    NULL,
    NULL,
    0,
    false,
};


// Message Box state variables
static UIScreen*          ui_msgbox_previous_screen = NULL;
static const char* const* ui_msgbox_message         = NULL;
static UI_MsgBoxHandler   ui_msgbox_handler         = NULL;
static bool               ui_msgbox_type_error      = false;


// Message Box API
bool ui_msgbox(const char* const message[], UI_MsgBoxType type, UI_MsgBoxButton selected_button, UI_MsgBoxHandler handler)
{
    UIScreen *active_screen = ui_get_active_screen();
    if (active_screen == &ui_msgbox_screen) {
        // Cannot invoke a message box from within another message box handler
        return false;
    }

    ui_msgbox_previous_screen = active_screen;

    ui_msgbox_screen.focused_element   = NULL;
    ui_msgbox_screen.is_input_captured = false;

    // Set message box UI elements
    switch (type) {
    case UI_MsgBoxType_Ok:
    case UI_MsgBoxType_Error:
        ui_msgbox_screen.elements = ui_msgbox_ok_screen_elements;
        ui_msgbox_screen.num_elements = sizeof(ui_msgbox_ok_screen_elements) / sizeof(UIElement);
        break;

    case UI_MsgBoxType_YesNo:
        ui_msgbox_screen.elements = ui_msgbox_yesno_screen_elements;
        ui_msgbox_screen.num_elements = sizeof(ui_msgbox_yesno_screen_elements) / sizeof(UIElement);
        break;

    default:
        return false;
        break;
    }

    // Select message box button
    switch (selected_button) {
    case UI_MsgBoxButton_Ok:
        if (type == UI_MsgBoxType_Ok || type == UI_MsgBoxType_Error) {
            ui_msgbox_screen.focused_element = &ui_msgbox_screen.elements[2];
        }
        break;

    case UI_MsgBoxButton_Yes:
        if (type == UI_MsgBoxType_YesNo) {
            ui_msgbox_screen.focused_element = &ui_msgbox_screen.elements[2];
        }
        break;

    case UI_MsgBoxButton_No:
        if (type == UI_MsgBoxType_YesNo) {
            ui_msgbox_screen.focused_element = &ui_msgbox_screen.elements[3];
        }
        break;

    case UI_MsgBoxButton_Back:
        ui_msgbox_screen.focused_element = &ui_msgbox_screen.elements[0];
        break;
    }

    ui_msgbox_message = message;
    ui_msgbox_handler = handler;
    ui_msgbox_type_error = (type == UI_MsgBoxType_Error);

    // Show the message box
    ui_show_screen(&ui_msgbox_screen);

    return true;
}

static void ui_msgbox_return_to_previous_screen(UI_MsgBoxButton msgbox_result)
{
    UIScreen* prev_screen = ui_msgbox_previous_screen;
    UI_MsgBoxHandler handler = ui_msgbox_handler;

    ui_msgbox_previous_screen = NULL;
    ui_msgbox_message         = NULL;
    ui_msgbox_handler         = NULL;
    ui_msgbox_type_error      = false;

    ui_show_screen(prev_screen);

    if (handler) {
        handler(msgbox_result);
    }
}


// Message Box UI element procedures
static void ui_msgbox_proc_back(const UIElement* element, UICommand command, int32_t encoder_step)
{
    if (command & UICommand_Init) {
        ST7735_DrawImage(element->x, element->y, 15, 16, icon_back_15x16);
    }
    if (command & UICommand_Click) {
        ui_msgbox_return_to_previous_screen(UI_MsgBoxButton_Back);
    }

    ui_default_element_proc(element, command, encoder_step);
}

static void ui_msgbox_proc_label(const UIElement* element, UICommand command, int32_t encoder_step)
{
    if (command & UICommand_Init) {
        uint16_t y          = element->y;
        uint16_t text_color = ui_msgbox_type_error ? UI_COLOR_ERROR : UI_COLOR_TEXT;

        const char* const* line = ui_msgbox_message;
        while (line != NULL && *line != NULL)
        {
            ST7735_WriteStringNoWrap(element->x, y, 10, *line, Font_7x10, text_color, UI_COLOR_BG);
            y += 12;
            ++line;
        };

        ui_msgbox_message = NULL;
    }

    ui_default_element_proc(element, command, encoder_step);
}

static void ui_msgbox_button_proc(const UIElement* element, UICommand command, int32_t encoder_step,
    uint16_t text_offset_px, const char* text, UI_MsgBoxButton result)
{
    if (command & UICommand_Init) {
        ST7735_FillRectangle(element->x, element->y, element->width, element->height, UI_COLOR_BUTTON_BG);
        ST7735_WriteStringNoWrap(element->x + text_offset_px, element->y + 2, 18, text, Font_11x18, UI_COLOR_TEXT, UI_COLOR_BUTTON_BG);
    }
    if (command & UICommand_Click) {
        ui_msgbox_return_to_previous_screen(result);
    }

    ui_default_element_proc(element, command, encoder_step);
}

static void ui_msgbox_proc_ok(const UIElement* element, UICommand command, int32_t encoder_step)
{
    ui_msgbox_button_proc(element, command, encoder_step, 8, "OK", UI_MsgBoxButton_Ok);
}

static void ui_msgbox_proc_yes(const UIElement* element, UICommand command, int32_t encoder_step)
{
    ui_msgbox_button_proc(element, command, encoder_step, 3, "Yes", UI_MsgBoxButton_Yes);
}

static void ui_msgbox_proc_no(const UIElement* element, UICommand command, int32_t encoder_step)
{
    ui_msgbox_button_proc(element, command, encoder_step, 8, "No", UI_MsgBoxButton_No);
}
