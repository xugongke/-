#ifndef __USER_STATUS_H
#define __USER_STATUS_H

#include "ff.h"
#include "stdint.h"
#include "lvgl.h"

#define USER_MAX 50
#define USER_STATUS_MAGIC 0x55AA55AA

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

