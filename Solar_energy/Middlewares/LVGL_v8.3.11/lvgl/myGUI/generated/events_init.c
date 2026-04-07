/*
* Copyright 2026 NXP
* NXP Proprietary. This software is owned or controlled by NXP and may only be used strictly in
* accordance with the applicable license terms. By expressly accepting such terms or by downloading, installing,
* activating and/or otherwise using the software, you are agreeing that you have read, and that you agree to
* comply with and are bound by, such license terms.  If you do not agree to be bound by the applicable license
* terms, then you may not retain, install, activate or otherwise use the software.
*/

#include "events_init.h"
#include <stdio.h>
#include "lvgl.h"

#if LV_USE_GUIDER_SIMULATOR && LV_USE_FREEMASTER
#include "freemaster_client.h"
#endif

#include "sdcard.h"
#include "user_status.h"
#include "device_manager.h"
extern lv_indev_t * indev_keypad;
lv_group_t * g_keypad_group;//创建全局group(可被焦点选中的对象集合)指针，在lv_init后分配空间

static void screen_user_home_event_handler (lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    switch (code) {
    case LV_EVENT_SCREEN_LOADED:
    {
        lv_group_remove_all_objs(g_keypad_group);//清空group中的所有组件
        //给group添加新组件
        lv_group_add_obj(g_keypad_group, guider_ui.screen_user_home_user_list_btn);
        //将按键添加进焦点组
        lv_indev_set_group(indev_keypad, g_keypad_group);
        //设置初始焦点
        lv_group_focus_obj(guider_ui.screen_user_home_user_list_btn);
        break;
    }
    default:
        break;
    }
}

static void screen_user_home_user_list_btn_event_handler (lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    switch (code) {
    case LV_EVENT_CLICKED:
    {
        ui_load_scr_animation(&guider_ui, &guider_ui.screen_user_list, guider_ui.screen_user_list_del, &guider_ui.screen_user_home_del, setup_scr_screen_user_list, LV_SCR_LOAD_ANIM_NONE, 10, 10, true, false);
        break;
    }
    default:
        break;
    }
}

void events_init_screen_user_home (lv_ui *ui)
{
    lv_obj_add_event_cb(ui->screen_user_home, screen_user_home_event_handler, LV_EVENT_ALL, ui);
    lv_obj_add_event_cb(ui->screen_user_home_user_list_btn, screen_user_home_user_list_btn_event_handler, LV_EVENT_ALL, ui);
}

static void screen_user_list_event_handler (lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    switch (code) {
    case LV_EVENT_SCREEN_LOADED:
    {
        house_info_t house;//创建户号结构体
        lv_group_remove_all_objs(g_keypad_group);//清空group中的所有组件
        /* 如果 GUI-Guider 已经往 list 里塞了东西，先清空 */
        lv_obj_clean(guider_ui.screen_user_list_list_1);

        //给group添加新组件
        for(uint32_t i = 0; i < device_count; i++)
        {
            char txt[100];
            if(device_list[i].valid != 1)
            {
                lv_snprintf(txt, sizeof(txt), "未入网   MAC %02X:%02X:%02X:%02X:%02X:%02X  ",
                            device_list[i].mac[0],device_list[i].mac[1],device_list[i].mac[2],device_list[i].mac[3],device_list[i].mac[4],device_list[i].mac[5]);
            }
            else if(device_list[i].valid == 1)
            {
                parse_addr(device_list[i].addr, &house);
                lv_snprintf(txt, sizeof(txt), "%d楼 %d单元 %d   MAC %02X:%02X:%02X:%02X:%02X:%02X ", house.building,house.unit,house.room,
                            device_list[i].mac[0],device_list[i].mac[1],device_list[i].mac[2],device_list[i].mac[3],device_list[i].mac[4],device_list[i].mac[5]);
            }


            /* 创建 list item（返回的是一个 btn） */
            lv_obj_t *btn = lv_list_add_btn(guider_ui.screen_user_list_list_1, LV_SYMBOL_HOME, txt);

            //修改list中按钮的样式
            static lv_style_t style_screen_user_list_list_1_extra_btns_main_default;
            ui_init_style(&style_screen_user_list_list_1_extra_btns_main_default);

            lv_style_set_pad_top(&style_screen_user_list_list_1_extra_btns_main_default, 5);
            lv_style_set_pad_left(&style_screen_user_list_list_1_extra_btns_main_default, 5);
            lv_style_set_pad_right(&style_screen_user_list_list_1_extra_btns_main_default, 5);
            lv_style_set_pad_bottom(&style_screen_user_list_list_1_extra_btns_main_default, 5);
            lv_style_set_border_width(&style_screen_user_list_list_1_extra_btns_main_default, 0);
            lv_style_set_text_color(&style_screen_user_list_list_1_extra_btns_main_default, lv_color_hex(0x0D3055));
            lv_style_set_text_font(&style_screen_user_list_list_1_extra_btns_main_default, &lv_font_SourceHanSerifSC_Regular_16);
            lv_style_set_text_opa(&style_screen_user_list_list_1_extra_btns_main_default, 255);
            lv_style_set_radius(&style_screen_user_list_list_1_extra_btns_main_default, 3);
            lv_style_set_bg_opa(&style_screen_user_list_list_1_extra_btns_main_default, 255);
            lv_style_set_bg_color(&style_screen_user_list_list_1_extra_btns_main_default, lv_color_hex(0xffffff));
            lv_style_set_bg_grad_dir(&style_screen_user_list_list_1_extra_btns_main_default, LV_GRAD_DIR_NONE);
            lv_obj_add_style(btn, &style_screen_user_list_list_1_extra_btns_main_default, LV_PART_MAIN|LV_STATE_DEFAULT);

            lv_group_add_obj(g_keypad_group, btn);//将按键添加进焦点组
            /* 给每个 btn 绑定同一个按下按钮时执行的回调函数，并用 user_data 传用户编号,LV_EVENT_CLICKED代表已按下 */
            lv_obj_add_event_cb(btn, user_list_item_event_handler, LV_EVENT_CLICKED, (void*)(uintptr_t)i);
            /* 再给每个按钮绑定按键操作回调函数 */
            lv_obj_add_event_cb(btn, screen_user_list_item_event_handler, LV_EVENT_KEY, (void*)(uintptr_t)i);

            /* 如果这是上次选中的编号，就让它获得焦点 */
            if(i == s_last_user_no) {
                lv_group_focus_obj(btn);
            }
        }
        //把“输入设备”绑定到“焦点管理组（group）
        lv_indev_set_group(indev_keypad, g_keypad_group);
        break;
    }
    default:
        break;
    }
}

void events_init_screen_user_list (lv_ui *ui)
{
    lv_obj_add_event_cb(ui->screen_user_list, screen_user_list_event_handler, LV_EVENT_ALL, ui);
}

static void screen_user_detail_event_handler (lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    switch (code) {
    case LV_EVENT_KEY:
    {
        uint32_t key = lv_event_get_key(e);
        if(key == LV_KEY_ESC)
        {
            ui_load_scr_animation(&guider_ui, &guider_ui.screen_user_list, guider_ui.screen_user_list_del, &guider_ui.screen_user_detail_del, setup_scr_screen_user_list, LV_SCR_LOAD_ANIM_NONE, 10, 10, true, true);
        }

        break;
    }
    case LV_EVENT_SCREEN_LOADED:
    {
        lv_group_remove_all_objs(g_keypad_group);//清空group中的所有组件
        //给group添加新组件
        lv_group_add_obj(g_keypad_group, guider_ui.screen_user_detail);
        //将按键添加进焦点组
        lv_indev_set_group(indev_keypad, g_keypad_group);
        //设置初始焦点
        lv_group_focus_obj(guider_ui.screen_user_detail);
        break;
    }
    default:
        break;
    }
}

void events_init_screen_user_detail (lv_ui *ui)
{
    lv_obj_add_event_cb(ui->screen_user_detail, screen_user_detail_event_handler, LV_EVENT_ALL, ui);
}


void events_init(lv_ui *ui)
{

}
