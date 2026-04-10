/**
 * @file lv_port_indev_templ.c
 *
 */

/*Copy this file as "lv_port_indev.c" and set this value to "1" to enable content*/
#if 1
/************************************************
 * 一、我们在文件"lv_port_indev.c"顶部定义宏定义
 ************************************************/
#define LV_USE_INDEV_TOUCHPAD 	0x1u
#define LV_USE_INDEV_MOUSE	 	0x2u
#define LV_USE_INDEV_KEYPAD 	0x4u
#define LV_USE_INDEV_ENCODER 	0x8u
#define LV_USE_INDEV_BUTTON 	0x10u
#define LV_USE_INDEV  LV_USE_INDEV_KEYPAD		//使用keypad

/*********************
 *      INCLUDES
 *********************/
#include "lv_port_indev.h"
#include "lvgl.h"
#include "key.h"
#include "lv_group.h"
#include "tim.h"
/*********************
 *      DEFINES
 *********************/

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *  STATIC PROTOTYPES
 **********************/
#if ( LV_USE_INDEV & LV_USE_INDEV_TOUCHPAD ) == LV_USE_INDEV_TOUCHPAD
static void touchpad_init(void);
static void touchpad_read(lv_indev_drv_t * indev_drv, lv_indev_data_t * data);
static bool touchpad_is_pressed(void);
static void touchpad_get_xy(lv_coord_t * x, lv_coord_t * y);
#endif

#if ( LV_USE_INDEV & LV_USE_INDEV_MOUSE ) == LV_USE_INDEV_MOUSE
static void mouse_init(void);
static void mouse_read(lv_indev_drv_t * indev_drv, lv_indev_data_t * data);
static bool mouse_is_pressed(void);
static void mouse_get_xy(lv_coord_t * x, lv_coord_t * y);
#endif

#if ( LV_USE_INDEV & LV_USE_INDEV_KEYPAD ) == LV_USE_INDEV_KEYPAD
static void keypad_init(void);
static void keypad_read(lv_indev_drv_t * indev_drv, lv_indev_data_t * data);
static uint32_t keypad_get_key(void);
#endif

#if ( LV_USE_INDEV & LV_USE_INDEV_ENCODER ) == LV_USE_INDEV_ENCODER
static void encoder_init(void);
static void encoder_read(lv_indev_drv_t * indev_drv, lv_indev_data_t * data);
static void encoder_handler(void);
#endif

#if ( LV_USE_INDEV & LV_USE_INDEV_BUTTON ) == LV_USE_INDEV_BUTTON
static void button_init(void);
static void button_read(lv_indev_drv_t * indev_drv, lv_indev_data_t * data);
static int8_t button_get_pressed_id(void);
static bool button_is_pressed(uint8_t id);
#endif

/**********************
 *  STATIC VARIABLES
 **********************/
 #if ( LV_USE_INDEV & LV_USE_INDEV_TOUCHPAD ) == LV_USE_INDEV_TOUCHPAD
lv_indev_t * indev_touchpad;
#endif
#if ( LV_USE_INDEV & LV_USE_INDEV_MOUSE ) == LV_USE_INDEV_MOUSE
lv_indev_t * indev_mouse;
#endif
#if ( LV_USE_INDEV & LV_USE_INDEV_KEYPAD ) == LV_USE_INDEV_KEYPAD
lv_indev_t * indev_keypad;//需要保证句柄是全局变量
#endif
#if ( LV_USE_INDEV & LV_USE_INDEV_ENCODER ) == LV_USE_INDEV_ENCODER
lv_indev_t * indev_encoder;
#endif
#if ( LV_USE_INDEV & LV_USE_INDEV_BUTTON ) == LV_USE_INDEV_BUTTON
lv_indev_t * indev_button;
#endif

//static int32_t encoder_diff;
//static lv_indev_state_t encoder_state;

/**********************
 *      MACROS
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

void lv_port_indev_init(void)
{
    /**
     * Here you will find example implementation of input devices supported by LittelvGL:
     *  - Touchpad
     *  - Mouse (with cursor support)
     *  - Keypad (supports GUI usage only with key)
     *  - Encoder (supports GUI usage only with: left, right, push)
     *  - Button (external buttons to press points on the screen)
     *
     *  The `..._read()` function are only examples.
     *  You should shape them according to your hardware
     */

    static lv_indev_drv_t indev_drv;
#if ( LV_USE_INDEV & LV_USE_INDEV_TOUCHPAD ) == LV_USE_INDEV_TOUCHPAD
    /*------------------
     * Touchpad
     * -----------------*/

    /*Initialize your touchpad if you have*/
    touchpad_init();

    /*Register a touchpad input device*/
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = touchpad_read;
    indev_touchpad = lv_indev_drv_register(&indev_drv);
#endif
#if ( LV_USE_INDEV & LV_USE_INDEV_MOUSE ) == LV_USE_INDEV_MOUSE
    /*------------------
     * Mouse
     * -----------------*/

    /*Initialize your mouse if you have*/
    mouse_init();

    /*Register a mouse input device*/
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = mouse_read;
    indev_mouse = lv_indev_drv_register(&indev_drv);

    /*Set cursor. For simplicity set a HOME symbol now.*/
    lv_obj_t * mouse_cursor = lv_img_create(lv_scr_act());
    lv_img_set_src(mouse_cursor, LV_SYMBOL_HOME);
    lv_indev_set_cursor(indev_mouse, mouse_cursor);
#endif
#if ( LV_USE_INDEV & LV_USE_INDEV_KEYPAD ) == LV_USE_INDEV_KEYPAD
    /*------------------
     * Keypad
     * -----------------*/

    /*Initialize your keypad or keyboard if you have*/
    keypad_init();

    /*Register a keypad input device*/
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_KEYPAD;
    indev_drv.read_cb = keypad_read;
		/* 长按 300ms 开始连发，每 80ms 连发一次 */
		indev_drv.long_press_time = 300;
		indev_drv.long_press_repeat_time = 80;
    indev_keypad = lv_indev_drv_register(&indev_drv);

    /*Later you should create group(s) with `lv_group_t * group = lv_group_create()`,
     *add objects to the group with `lv_group_add_obj(group, obj)`
     *and assign this input device to group to navigate in it:
     *`lv_indev_set_group(indev_keypad, group);`*/
#endif
#if ( LV_USE_INDEV & LV_USE_INDEV_ENCODER ) == LV_USE_INDEV_ENCODER
    /*------------------
     * Encoder
     * -----------------*/

    /*Initialize your encoder if you have*/
    encoder_init();

    /*Register a encoder input device*/
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_ENCODER;
    indev_drv.read_cb = encoder_read;
    indev_encoder = lv_indev_drv_register(&indev_drv);

    /*Later you should create group(s) with `lv_group_t * group = lv_group_create()`,
     *add objects to the group with `lv_group_add_obj(group, obj)`
     *and assign this input device to group to navigate in it:
     *`lv_indev_set_group(indev_encoder, group);`*/
#endif
#if ( LV_USE_INDEV & LV_USE_INDEV_BUTTON ) == LV_USE_INDEV_BUTTON
    /*------------------
     * Button
     * -----------------*/

    /*Initialize your button if you have*/
    button_init();

    /*Register a button input device*/
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_BUTTON;
    indev_drv.read_cb = button_read;
    indev_button = lv_indev_drv_register(&indev_drv);

    /*Assign buttons to points on the screen*/
    static const lv_point_t btn_points[2] = {
        {10, 10},   /*Button 0 -> x:10; y:10*/
        {40, 100},  /*Button 1 -> x:40; y:100*/
    };
    lv_indev_set_button_points(indev_button, btn_points);
#endif

}

/**********************
 *   STATIC FUNCTIONS
 **********************/
#if ( LV_USE_INDEV & LV_USE_INDEV_TOUCHPAD ) == LV_USE_INDEV_TOUCHPAD
/*------------------
 * 触摸板
 * -----------------*/

/*Initialize your touchpad*/
static void touchpad_init(void)
{
    /*Your code comes here*/
}

/*Will be called by the library to read the touchpad*/
static void touchpad_read(lv_indev_drv_t * indev_drv, lv_indev_data_t * data)
{
    static lv_coord_t last_x = 0;
    static lv_coord_t last_y = 0;

    /*Save the pressed coordinates and the state*/
    if(touchpad_is_pressed()) {
        touchpad_get_xy(&last_x, &last_y);
        data->state = LV_INDEV_STATE_PR;
    }
    else {
        data->state = LV_INDEV_STATE_REL;
    }

    /*Set the last pressed coordinates*/
    data->point.x = last_x;
    data->point.y = last_y;
}

/*Return true is the touchpad is pressed*/
static bool touchpad_is_pressed(void)
{
    /*Your code comes here*/

    return false;
}

/*Get the x and y coordinates if the touchpad is pressed*/
static void touchpad_get_xy(lv_coord_t * x, lv_coord_t * y)
{
    /*Your code comes here*/

    (*x) = 0;
    (*y) = 0;
}
#endif
#if ( LV_USE_INDEV & LV_USE_INDEV_MOUSE ) == LV_USE_INDEV_MOUSE
/*------------------
 * 鼠标
 * -----------------*/

/*Initialize your mouse*/
static void mouse_init(void)
{
    /*Your code comes here*/
}

/*Will be called by the library to read the mouse*/
static void mouse_read(lv_indev_drv_t * indev_drv, lv_indev_data_t * data)
{
    /*Get the current x and y coordinates*/
    mouse_get_xy(&data->point.x, &data->point.y);

    /*Get whether the mouse button is pressed or released*/
    if(mouse_is_pressed()) {
        data->state = LV_INDEV_STATE_PR;
    }
    else {
        data->state = LV_INDEV_STATE_REL;
    }
}

/*Return true is the mouse button is pressed*/
static bool mouse_is_pressed(void)
{
    /*Your code comes here*/

    return false;
}

/*Get the x and y coordinates if the mouse is pressed*/
static void mouse_get_xy(lv_coord_t * x, lv_coord_t * y)
{
    /*Your code comes here*/

    (*x) = 0;
    (*y) = 0;
}
#endif
#if ( LV_USE_INDEV & LV_USE_INDEV_KEYPAD ) == LV_USE_INDEV_KEYPAD
/*------------------
 * 键盘
 * -----------------*/

/*初始化你的键盘*/
static void keypad_init(void)
{
    /*你的代码在这里*/
}

/*将由库调用以读取鼠标,keypad_read() 会被 LVGL 周期性调用*/
static void keypad_read(lv_indev_drv_t * indev_drv, lv_indev_data_t * data)
{
    static uint32_t last_key = 0;

    /*获取当前的 x 和 y 坐标*/
    /* 这里官方默认你有鼠标设备，但实际上可能没有，我们注释掉 */
    // mouse_get_xy(&data->point.x, &data->point.y);

    /*获取是否有按键被按下并保存按下的按键*/
    uint32_t act_key = keypad_get_key();
    if(act_key != 0) {
        data->state = LV_INDEV_STATE_PR;

        /*根据您的按键定义将按键转换为 LVGL 控制字符*/
        switch(act_key) {
            case 1:
                act_key = LV_KEY_NEXT;//下一个
                break;
            case 2:
                act_key = LV_KEY_PREV;//上一个
                break;
            case 3:
                act_key = LV_KEY_LEFT;//左键
                break;
            case 4:
                act_key = LV_KEY_RIGHT;//右键
                break;
            case 5:
                act_key = LV_KEY_ENTER;//回车键
                break;
            case 6:
                act_key = LV_KEY_ESC;//退出键
                break;
            case 7:
                act_key = LV_KEY_UP;//up
                break;
            case 8:
                act_key = LV_KEY_DOWN;//down
                break;
				/* 这里可以添加更多操作符 */
        }

        last_key = act_key;
    }
    else {
        data->state = LV_INDEV_STATE_REL;
    }

    data->key = last_key;
}

/*获取当前被按下的按键。如果没有按键被按下，则为0*/
static uint32_t keypad_get_key(void)
{
    key_id_t kid_edge;
    key_event_t ev = KEY_Scan(&kid_edge);   /* 每次调用都更新消抖状态 */
	
		if(ev == KEY_EVENT_SHORT_PRESS)
		{
				HAL_GPIO_WritePin(LCD_BL_GPIO_Port, LCD_BL_Pin, GPIO_PIN_SET);
			
				// 1. 停止定时器
				HAL_TIM_Base_Stop_IT(&htim3);

				// 2. 计数器 清0 → 从头开始计数
				__HAL_TIM_SET_COUNTER(&htim3, 0);

				// 3. 再次启动定时器
				HAL_TIM_Base_Start_IT(&htim3);
		}

    /* 1) 方向键：稳定按下期间持续返回 -> LVGL 才能识别“按住”并自动 repeat */
    if (KEY_IsPressedStable(Key_Down))  return 1;   /* 例如：下/next */
    if (KEY_IsPressedStable(Key_UP)) return 2;   		/* 例如：上/prev */
    if (KEY_IsPressedStable(Key_Left))  return 3;   /* 左 */
    if (KEY_IsPressedStable(Key_Right))  return 4;  /* 右 */

    /* 2) 其他键：仍然只在“确认按下瞬间”返回一次（避免长按变连发） */
    if (ev == KEY_EVENT_SHORT_PRESS)
    {
        switch (kid_edge)
        {
            case Key_Enter: return 5;   /* ENTER */
            case Key_Return: return 6;   /* ESC */
            case Key1: return 7;   /* up */
            case Key2: return 8;   /* down */					
            default: break;
        }
    }

    return 0;
}


#endif
#if ( LV_USE_INDEV & LV_USE_INDEV_ENCODER ) == LV_USE_INDEV_ENCODER
/*------------------
 * Encoder
 * -----------------*/

/*Initialize your keypad*/
static void encoder_init(void)
{
    /*Your code comes here*/
}

/*Will be called by the library to read the encoder*/
static void encoder_read(lv_indev_drv_t * indev_drv, lv_indev_data_t * data)
{

    data->enc_diff = encoder_diff;
    data->state = encoder_state;
}

/*Call this function in an interrupt to process encoder events (turn, press)*/
static void encoder_handler(void)
{
    /*Your code comes here*/

    encoder_diff += 0;
    encoder_state = LV_INDEV_STATE_REL;
}
#endif
#if ( LV_USE_INDEV & LV_USE_INDEV_BUTTON ) == LV_USE_INDEV_BUTTON
/*------------------
 * 按钮
 * -----------------*/

/*Initialize your buttons*/
static void button_init(void)
{
    /*Your code comes here*/
}

/*Will be called by the library to read the button*/
static void button_read(lv_indev_drv_t * indev_drv, lv_indev_data_t * data)
{

    static uint8_t last_btn = 0;

    /*Get the pressed button's ID*/
    int8_t btn_act = button_get_pressed_id();

    if(btn_act >= 0) {
        data->state = LV_INDEV_STATE_PR;
        last_btn = btn_act;
    }
    else {
        data->state = LV_INDEV_STATE_REL;
    }

    /*Save the last pressed button's ID*/
    data->btn_id = last_btn;
}

/*Get ID  (0, 1, 2 ..) of the pressed button*/
static int8_t button_get_pressed_id(void)
{
    uint8_t i;

    /*Check to buttons see which is being pressed (assume there are 2 buttons)*/
    for(i = 0; i < 2; i++) {
        /*Return the pressed button's ID*/
        if(button_is_pressed(i)) {
            return i;
        }
    }

    /*No button pressed*/
    return -1;
}

/*Test if `id` button is pressed or not*/
static bool button_is_pressed(uint8_t id)
{

    /*Your code comes here*/

    return false;
}
#endif

#else /*Enable this file at the top*/

/*This dummy typedef exists purely to silence -Wpedantic.*/
typedef int keep_pedantic_happy;
#endif
