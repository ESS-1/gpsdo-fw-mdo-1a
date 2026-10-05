#include "bootlog.h"
#include "main.h"
#include "st7735.h"
#include <stdint.h>

#define BOOTLOG_BG_COLOR (ST7735_COLOR565(50, 50, 50))

static int32_t bootlog_current_line = 0;

void bootlog_write(int32_t x, int32_t y, uint16_t color, const char* msg)
{
    ST7735_WriteString(1 + x * 7, 1 + y * 11, msg, Font_7x10, color, BOOTLOG_BG_COLOR);
}

void bootlog_init()
{
    ST7735_FillScreenFast(BOOTLOG_BG_COLOR);
}

void bootlog_add(const char* msg)
{
    bootlog_write(0, bootlog_current_line++, ST7735_WHITE, msg);
}

void bootlog_set_status(bool status)
{
    if (status) {
        bootlog_write(16, bootlog_current_line - 1, ST7735_GREEN, "[ OK ]");
    } else {
        bootlog_write(16, bootlog_current_line - 1, ST7735_RED, "[FAIL]");
    }
}

void bootlog_error(const char* msg)
{
    bootlog_write(0, bootlog_current_line++, ST7735_RED, msg);
}
