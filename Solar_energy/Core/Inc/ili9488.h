#ifndef __ILI9488_H
#define __ILI9488_H

#include "stm32f4xx_hal.h"
#include "lv_port_disp.h"

#define ILI9488_WIDTH   480
#define ILI9488_HEIGHT  320

/* RGB565 颜色 */
#define BLACK   0x0000
#define WHITE   0xFFFF
#define RED     0xF800
#define GREEN   0x07E0
#define BLUE    0x001F

void ILI9488_Init(void);
void ILI9488_Clear(uint16_t color);
void ILI9488_SetRotation(uint8_t r);
void ILI9488_DrawPixel(uint16_t x, uint16_t y, uint16_t color);
void ILI9488_DrawLine(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color);
void ILI9488_DrawRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color);
void ILI9488_FillRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color);
void LCD_DispFlush(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2,lv_color_t * color_p);
void LCD_DMA_DispFlush(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, lv_color_t * color_p);
#endif


