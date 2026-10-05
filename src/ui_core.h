#ifndef _UI_CORE_8DE9180F8ED6_H_
#define _UI_CORE_8DE9180F8ED6_H_

#include <stdint.h>
#include <stdbool.h>

extern bool ui_show_performance_timer;


#define UI_COLOR_BG                 (ST7735_BLACK)
#define UI_COLOR_BUTTON_BG          (ST7735_COLOR565(120, 120, 120))
#define UI_COLOR_INACTIVE_BUTTON_BG (ST7735_COLOR565(160, 160, 160))
#define UI_COLOR_FOCUS_FRAME        (ST7735_COLOR565(195, 205, 205))
#define UI_COLOR_CAPTURE_FRAME      (ST7735_COLOR565(255, 115, 21))
#define UI_COLOR_OUT_LABEL          (ST7735_COLOR565(34, 177, 76))
#define UI_COLOR_OUT_DIRECT         (ST7735_COLOR565(0, 150, 255))
#define UI_COLOR_PPS_INDICATOR      (ST7735_GREEN)
#define UI_COLOR_TEXT               (ST7735_WHITE)
#define UI_COLOR_INACTIVE_TEXT      (ST7735_COLOR565(100, 100, 100))
#define UI_COLOR_MENU_LABEL         (ST7735_COLOR565(170, 170, 170))
#define UI_COLOR_ERROR              (ST7735_RED)
#define UI_COLOR_MAP_MARKER         (ST7735_COLOR565(255, 0, 255))
#define UI_COLOR_LINK               (ST7735_WHITE)

#define UI_COLOR_TREND_BAR          (ST7735_COLOR565(255, 140, 25))
#define UI_COLOR_TREND_BG           (ST7735_COLOR565(95, 95, 95))
#define UI_COLOR_TREND_NODATA       (ST7735_COLOR565(30, 30, 30))
#define UI_COLOR_TREND_MAJOR_MARKS  (ST7735_WHITE)
#define UI_COLOR_TREND_MINOR_MARKS  (ST7735_COLOR565(175, 175, 175))
#define UI_COLOR_TREND_SCROLL       (ST7735_COLOR565(30, 230, 30))

typedef enum {
    UICommand_None           = 0x00000000,
    UICommand_Init           = 0x00000001, // The screen containing the element is activated
    UICommand_Focus          = 0x00000002, // The element gains focus
    UICommand_LostFocus      = 0x00000004, // The element loses focus
    UICommand_RestoreFocus   = 0x00000008, // The screen containing an already focused element is activated
    UICommand_Capture        = 0x00000010, // The element captures control
    UICommand_Release        = 0x00000020, // The element releases control
    UICommand_RestoreCapture = 0x00000040, // The screen containing an element that has already captured control is activated
    UICommand_EncoderStep    = 0x00000080, // The encoder is rotated while the element has captured control
    UICommand_Click          = 0x00000100, // The focused element receives a click
} UICommand;

#define UI_STYLE_NONE            0x0000U
#define UI_STYLE_FOCUSABLE       0x0001U
#define UI_STYLE_INPUT_CAPTURING 0x0002U
#define UI_STYLE_NOFRAME         0x0004U


struct UIElement;
typedef void (*UI_ElementProc)(const struct UIElement* element, UICommand command, int32_t encoder_step);

typedef struct UIElement {
    const uint16_t x;
    const uint16_t y;
    const uint16_t width;
    const uint16_t height;

    const uint16_t       styles;
    const UI_ElementProc proc;
} UIElement;

typedef struct {
    const UIElement* elements;
    const UIElement* focused_element;
    uint16_t         num_elements;
    bool             is_input_captured;
} UIScreen;

void      ui_default_element_proc(const UIElement* element, UICommand command, int32_t encoder_step);
bool      ui_is_captured(const UIElement* element);
UIScreen* ui_get_active_screen();

void      ui_show_screen(UIScreen* screen);
void      ui_run();

#endif
