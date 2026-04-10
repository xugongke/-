#include "ili9488.h"
#include "main.h"
#include <stdlib.h>
#include <stdio.h>
#include "dma.h"
#include "tim.h"
/* ========== FMC 映射地址 ========== */
/* Bank1 NE1 = 0x60000000 */
#define LCD_BASE    0x60000000U
#define LCD_CMD     (*(__IO uint16_t *)(LCD_BASE))
#define LCD_DATA    (*(__IO uint16_t *)(LCD_BASE | (1U << 7)))   // A6 -> +1

/* ========== ILI9488 命令 ========== */
#define ILI9488_SWRESET   0x01
#define ILI9488_SLPOUT    0x11
#define ILI9488_DISPON    0x29
#define ILI9488_CASET     0x2A
#define ILI9488_PASET     0x2B
#define ILI9488_RAMWR     0x2C
#define ILI9488_MADCTL    0x36
#define ILI9488_COLMOD    0x3A

/* ========== 低层写函数 ========== */
static inline void ILI9488_WriteCommand(uint16_t cmd)
{
    LCD_CMD = cmd;
}

static inline void ILI9488_WriteData(uint16_t data)
{
    LCD_DATA = data;
}

static void ILI9488_Delay(uint32_t ms)
{
    HAL_Delay(ms);
}

/* ========== 设置窗口 ========== */
static void ILI9488_SetAddressWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1)
{
		// 设置水平区域（列）
    ILI9488_WriteCommand(ILI9488_CASET);
    ILI9488_WriteData(x0 >> 8);
    ILI9488_WriteData(x0 & 0xFF);
    ILI9488_WriteData(x1 >> 8);
    ILI9488_WriteData(x1 & 0xFF);

		// 设置垂直区域（行）
    ILI9488_WriteCommand(ILI9488_PASET);
    ILI9488_WriteData(y0 >> 8);
    ILI9488_WriteData(y0 & 0xFF);
    ILI9488_WriteData(y1 >> 8);
    ILI9488_WriteData(y1 & 0xFF);

		// 写入数据
    ILI9488_WriteCommand(ILI9488_RAMWR);
}

/* ========== 设置方向 ========== */
/*
  r = 0: 竖屏 320x480
  r = 1: 横屏 480x320
  r = 2: 竖屏反向
  r = 3: 横屏反向
*/
void ILI9488_SetRotation(uint8_t r)
{
    ILI9488_WriteCommand(ILI9488_MADCTL);

    switch (r & 3)
    {
        case 0:
            // MX=0 MY=0 MV=0
            ILI9488_WriteData(0x48); // BGR=1
            break;
        case 1:
            // MV=1
            ILI9488_WriteData(0x28);
            break;
        case 2:
            ILI9488_WriteData(0x88);
            break;
        case 3:
            ILI9488_WriteData(0xE8);
            break;
    }
}

/* ========== 初始化（RGB565 + 320x480） ========== */
void ILI9488_Init(void)
{
    /* 如果你有 LCD_RST 引脚，请在这里拉低再拉高 */
    HAL_GPIO_WritePin(LCD_RES_GPIO_Port, LCD_RES_Pin, GPIO_PIN_RESET);
    HAL_Delay(20);
    HAL_GPIO_WritePin(LCD_RES_GPIO_Port, LCD_RES_Pin, GPIO_PIN_SET);
    HAL_Delay(120);

    /* 软件复位 */
    ILI9488_WriteCommand(ILI9488_SWRESET);
    ILI9488_Delay(150);

    /* 退出睡眠 */
    ILI9488_WriteCommand(ILI9488_SLPOUT);
    ILI9488_Delay(150);

    /* 像素格式：RGB565 */
    ILI9488_WriteCommand(ILI9488_COLMOD);
    ILI9488_WriteData(0x55);  // 16bit/pixel
    ILI9488_Delay(10);

    /* MADCTL：默认竖屏 + BGR */
    ILI9488_SetRotation(3);

    /* 显示开 */
    ILI9488_WriteCommand(ILI9488_DISPON);
		HAL_GPIO_WritePin(LCD_BL_GPIO_Port, LCD_BL_Pin, GPIO_PIN_SET);
		HAL_TIM_Base_Start_IT(&htim3);
    ILI9488_Delay(120);
}

/* ========== 清屏函数（高速写） ========== */
void ILI9488_Clear(uint16_t color)
{
    uint32_t total = (uint32_t)ILI9488_WIDTH * ILI9488_HEIGHT;

    ILI9488_SetAddressWindow(0, 0, ILI9488_WIDTH - 1, ILI9488_HEIGHT - 1);

    /* RGB565：一次写 16bit */
    while (total--)
    {
        LCD_DATA = color;
    }
}
/* ========== 画点函数 ========== */
void ILI9488_DrawPixel(uint16_t x, uint16_t y, uint16_t color)
{
//    if (x >= 480 || y >= 320) return;  // 保证在屏幕范围内

    // 设置绘制区域：一个点
    ILI9488_SetAddressWindow(x, y, x, y);

    // 写入像素数据
    ILI9488_WriteData(color);
}
/* ========== 画线函数 ========== */
void ILI9488_DrawLine(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color)
{
    int dx = abs(x2 - x1);
    int dy = abs(y2 - y1);
    int sx = (x1 < x2) ? 1 : -1;
    int sy = (y1 < y2) ? 1 : -1;
    int err = dx - dy;

    while (1)
    {
        // 画点
        ILI9488_DrawPixel(x1, y1, color);

        if (x1 == x2 && y1 == y2)
            break;

        int e2 = err;
        if (e2 > -dy)
        {
            err -= dy;
            x1 += sx;
        }
        if (e2 < dx)
        {
            err += dx;
            y1 += sy;
        }
    }
}
/* ========== 画空心矩形函数 ========== */
void ILI9488_DrawRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color)
{
    // 绘制四条边
    ILI9488_DrawLine(x, y, x + w - 1, y, color);             // 上边
    ILI9488_DrawLine(x, y, x, y + h - 1, color);             // 左边
    ILI9488_DrawLine(x + w - 1, y, x + w - 1, y + h - 1, color); // 右边
    ILI9488_DrawLine(x, y + h - 1, x + w - 1, y + h - 1, color); // 下边
}
/* ========== 画填充矩形函数 ========== */
void ILI9488_FillRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color)
{
    for (uint16_t i = x; i < x + w; i++) {
        for (uint16_t j = y; j < y + h; j++) {
            ILI9488_DrawPixel(i, j, color);
        }
    }
}
//矩形填充加速
void LCD_DispFlush(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, lv_color_t * color_p)
{
    uint32_t total = (uint32_t)(x2 - x1 + 1) * (uint32_t)(y2 - y1 + 1);
		//设置显示区域
    ILI9488_SetAddressWindow(x1, y1, x2, y2);

    uint16_t *p = (uint16_t *)color_p;
    while(total--) {
        LCD_DATA = *p++;
    }
}
//DMA搬运加速
void LCD_DMA_DispFlush(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, lv_color_t * color_p)
{
    uint32_t total = (uint32_t)(x2 - x1 + 1) * (uint32_t)(y2 - y1 + 1);
		//设置显示区域
    ILI9488_SetAddressWindow(x1, y1, x2, y2);
	//启动DMA搬运,这里要给LCD_DATA宏定义取地址，因为LCD_DATA是把地址解引用
		HAL_DMA_Start_IT(&hdma_memtomem_dma2_stream4, (uint32_t) color_p, (uint32_t) &LCD_DATA,total);
}



