#ifndef __KEY_H
#define __KEY_H

#include "main.h"
#include <stdint.h>

typedef enum
{
		Key_UP = 0,	//上
		Key_Down,		//下
    Key_Left,		//左
    Key_Right,	//右
		Key_Enter,	//确认
		Key_Return,	//退出
		Key1,
		Key2,
		KEY_ID_COUNT
} key_id_t;


typedef enum
{
    KEY_EVENT_NONE = 0,
    KEY_EVENT_SHORT_PRESS//短按事件触发一次
} key_event_t;

/**
 * @brief  每隔固定周期调用一次（建议 10ms）
 * @return KEY_EVENT_SHORT_PRESS 表示“短按事件触发一次”，否则 NONE
 *
 * 注意：此函数一次只返回一个事件（若多键同时按，按扫描顺序返回）
 */
key_event_t KEY_Scan(key_id_t *key_id);
uint8_t KEY_IsPressedStable(key_id_t id);
#endif
