#include "key.h"
#include "main.h"
#include "gui_guider.h"
#include "es1642_usage_guide.h"
#include "user_data_manager.h"
#include "device_manager.h"

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
				
        case Key1:
						return (HAL_GPIO_ReadPin(Key1_GPIO_Port, Key1_Pin) == GPIO_PIN_RESET);
				
        case Key2:
						return (HAL_GPIO_ReadPin(Key2_GPIO_Port, Key2_Pin) == GPIO_PIN_RESET);				

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

//用户界面按键按下回调函数
void screen_user_list_item_event_handler(lv_event_t *e)
{
    /* user_data 里放 user_no（1~50）获取用户编号 */
    uint32_t user_no = (uint32_t)(uintptr_t)lv_event_get_user_data(e);

    lv_event_code_t code = lv_event_get_code(e);
    switch (code) {
    case LV_EVENT_KEY:
    {
        uint32_t key = lv_event_get_key(e);
        if(key == LV_KEY_LEFT)//左键，用户离线
        {	
					int ret;
					const uint8_t new_psk[ES1642_SET_PSK_LEN] = {0x11,0x12,0x13,0x14,0x15,0x16,0x17,0x18};//统一网络口令
					uint8_t addr[6] = {0x11,0x12,0x13,0x14,0x15,0x16};
					ret = ES1642_SetPsk(addr, new_psk);
					if(ret != 0)
					{
						printf("给指定设备入网失败,ret:%d\r\n",ret);
					}
        }
				
        if(key == LV_KEY_RIGHT)//右键，用户上线
        {
					char dataa[3] = {0x11,0x22,0x33};
					es1642_response_t response; // 用于接收从机响应数据
					
					ES1642_SendUserData(device_list[0].addr, (const uint8_t *)dataa, 3, 0, &response);
						if(response.data[0] == 0xcc)
						{
							printf("接收到从机给我发送的数据:%#x\r\n",response.data[0]);
						}
        }
				
        if(key == LV_KEY_UP)
        {
					printf("开始搜索全部设备\r\n");
					ES1642_StartSearch(0,ES1642_SEARCH_RULE_ALL);
        }
				
        if(key == LV_KEY_DOWN)
        {
					printf("停止设备搜索\r\n");
					ES1642_StopSearch();
        }
				
        if(key == LV_KEY_ESC)//ESC键，返回首页
        {
					 s_last_user_no = 0;//回到首页，重置用户列表焦点
					 ui_load_scr_animation(&guider_ui, &guider_ui.screen_user_home, guider_ui.screen_user_home_del, &guider_ui.screen_user_list_del, setup_scr_screen_user_home, LV_SCR_LOAD_ANIM_NONE, 10, 10, true, true);
        }

        break;
    }
    default:
        break;
    }
}
