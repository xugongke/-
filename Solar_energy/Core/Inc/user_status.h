#ifndef __USER_STATUS_H
#define __USER_STATUS_H

#include "ff.h"
#include "stdint.h"
#include "lvgl.h"
#include "main.h" // For LCD_BL_GPIO_Port and LCD_BL_Pin

#define USER_MAX 50
#define USER_STATUS_MAGIC 0x55AA55AA

// LCD Backlight Control
#define LCD_BACKLIGHT_ON()   HAL_GPIO_WritePin(LCD_BL_GPIO_Port, LCD_BL_Pin, GPIO_PIN_SET)
#define LCD_BACKLIGHT_OFF()  HAL_GPIO_WritePin(LCD_BL_GPIO_Port, LCD_BL_Pin, GPIO_PIN_RESET)

// Default idle timeout in milliseconds (e.g., 30 seconds)
#define LCD_IDLE_TIMEOUT_MS  (30 * 1000UL)

typedef struct
{
    uint32_t magic;
    uint32_t version;
    uint8_t bitmap[8];
    uint32_t crc;
}USER_STATUS_FILE;

void USER_STATUS_Init(void);

void USER_LOG(uint8_t user,char *event);

uint8_t USER_IsOnline(uint8_t user);

void USER_SetOnline(uint8_t user);

void USER_SetOffline(uint8_t user);

void USER_STATUS_Save(void);

extern uint8_t dirty_flag;

void screen_user_list_item_event_handler(lv_event_t *e);
#endif
