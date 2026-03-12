#include "key.h"
#include "main.h"


/* ====== 用户可调参数 ====== */
#define KEY_SCAN_PERIOD_MS     10u
#define KEY_DEBOUNCE_MS        35u   // 消抖时间（建议 20~50ms）
#define KEY_DEBOUNCE_TICKS  ((KEY_DEBOUNCE_MS + KEY_SCAN_PERIOD_MS - 1u) / KEY_SCAN_PERIOD_MS)
#if (KEY_DEBOUNCE_TICKS < 1)
#undef KEY_DEBOUNCE_TICKS
#define KEY_DEBOUNCE_TICKS 1
#endif

/* ====== 按键“是否按下”的硬件判定（按你的原理图） ====== */
static inline uint8_t KEY_HW_IsPressed(key_id_t id)
{
    switch (id)
    {
        case Key_UP:
            return (HAL_GPIO_ReadPin(Key_UP_GPIO_Port, Key_UP_Pin) == GPIO_PIN_RESET);

        case Key_Down:
						return (HAL_GPIO_ReadPin(Key_Down_GPIO_Port, Key_Down_Pin) == GPIO_PIN_RESET);
            
        case Key_Left:
            return (HAL_GPIO_ReadPin(Key_Left_GPIO_Port, Key_Left_Pin) == GPIO_PIN_RESET);

        case Key_Right:
						return (HAL_GPIO_ReadPin(Key_Right_GPIO_Port, Key_Right_Pin) == GPIO_PIN_RESET);
				
        case Key_Enter:
						return (HAL_GPIO_ReadPin(Key_Enter_GPIO_Port,	Key_Enter_Pin) == GPIO_PIN_RESET);

        case Key_Return:
						return (HAL_GPIO_ReadPin(Key_Return_GPIO_Port, Key_Return_Pin) == GPIO_PIN_RESET);			

        default:
            return 0;
    }
}

/* ====== 消抖状态机 ====== */
typedef struct
{
    uint8_t stable;      // 当前“稳定态”(0未按/1按下)
    uint8_t last_raw;    // 上一次采样的原始值
    uint8_t cnt;         // 稳定计数
} key_filter_t;

static key_filter_t s_key[KEY_ID_COUNT] = {0};

key_event_t KEY_Scan(key_id_t *key_id)
{
    key_event_t ev = KEY_EVENT_NONE;
    key_id_t ev_id = (key_id_t)0;

    if (key_id) *key_id = (key_id_t)0;

    for (key_id_t id = (key_id_t)0; id < KEY_ID_COUNT; id++)
    {
        uint8_t raw = KEY_HW_IsPressed(id);
				//如果本次调用KEY_Scan读取到的按键状态和上次一样
        if (raw == s_key[id].last_raw) 
				{
            if (s_key[id].cnt < 255) s_key[id].cnt++;
        } 
				else 
				{
            s_key[id].cnt = 0;
            s_key[id].last_raw = raw;
        }
				
        if (s_key[id].cnt >= (uint8_t)KEY_DEBOUNCE_TICKS)
        {		//达到稳定计数后，将 cnt 固定为阈值，避免溢出。
            s_key[id].cnt = (uint8_t)KEY_DEBOUNCE_TICKS;
						//如果稳定态与之前不同：更新 stable。
            if (raw != s_key[id].stable)
            {
                s_key[id].stable = raw;

                /* 只在“确认按下”这一刻记录一个短按事件 */
                if ((raw == 1u) && (ev == KEY_EVENT_NONE))
                {
                    ev = KEY_EVENT_SHORT_PRESS;
                    ev_id = id;
                }
            }
        }
    }

    if (key_id && ev != KEY_EVENT_NONE) *key_id = ev_id;
    return ev;
}
uint8_t KEY_IsPressedStable(key_id_t id)
{
    if(id >= KEY_ID_COUNT) return 0;
    return s_key[id].stable;
}

