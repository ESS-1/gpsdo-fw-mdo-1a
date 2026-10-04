#ifndef __ST7735_H_CONFIG__
#define __ST7735_H_CONFIG__

#include "boot_data_defs.h"

#define ST7735_SPI_PORT hspi1
extern SPI_HandleTypeDef ST7735_SPI_PORT;

#define ST7735_RES_Pin       LCD_RES_Pin
#define ST7735_RES_GPIO_Port LCD_RES_GPIO_Port
#define ST7735_CS_Pin        LCD_CS_Pin
#define ST7735_CS_GPIO_Port  LCD_CS_GPIO_Port
#define ST7735_DC_Pin        LCD_DC_Pin
#define ST7735_DC_GPIO_Port  LCD_DC_GPIO_Port

#define ST7735_INIT_CMDS     (G_BOOT_DATA->display_init_commands.commands)

// 160x80 with ST7735S (no color inversion), rotate left
#define ST7735_XSTART   0
#define ST7735_YSTART   24
#define ST7735_WIDTH    160
#define ST7735_HEIGHT   80
#define ST7735_ROTATION (ST7735_MADCTL_MX | ST7735_MADCTL_MV | ST7735_MADCTL_BGR)

#endif // __ST7735_H_CONFIG__
