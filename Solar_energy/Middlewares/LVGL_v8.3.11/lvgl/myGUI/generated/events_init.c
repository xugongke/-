/*
* Copyright 2026 NXP
* NXP Proprietary. This software is owned or controlled by NXP and may only be used strictly in
* accordance with the applicable license terms. By expressly accepting such terms or by downloading, installing,
* activating and/or otherwise using the software, you are agreeing to you have read, or that you agree to
* comply with, be bound by, such license terms.  If you do not agree to be bound by the applicable license
* terms, then you may not retain, install, activate, or otherwise use the software.
*/

#include "events_init.h"
#include <stdio.h>
#include <string.h>
#include "lvgl.h"

#if LV_USE_GUIDER_SIMULATOR && LV_USE_FREERTOS
#include "freemaster_client.h"
#endif

#include "wiz_interface.h"
#include "user_data_manager.h"
#include "key.h"
#include "device_manager.h"
#include "battery.h"
#include "user_main.h"
#include "mppt.h"
#include "es1642_usage_guide.h"
#include "tcp_cmd_handler.h"
extern lv_indev_t * indev_keypad;
lv_group_t * g_keypad_group;//创建全局group(可被焦点选中的对象集合)指针，在lv_init后分配空间

/* 记录home页面最后聚焦的卡片 (0=太阳能, 1=设备在线, 2=告警) */
static uint8_t s_home_focus_card = 1;  /* 默认设备在线卡片 */

/* 按键重映射标志: 1=需要将NEXT/PREV重映射为DOWN/UP (Home页面和TCP设置页面) */
volatile uint8_t g_need_key_remap = 0;

/* ======== Home 页事件处理 ======== */

/* (已移除 s_home_focus_on_cont3，焦点状态通过 group 成员管理) */

/**
 * @brief  Home页面: 卡片按键处理
 *         LV_KEY_LEFT / LV_KEY_RIGHT → 在3张卡片间切换 (跳过cont_3)
 *         LV_KEY_UP (原↑键, 在lv_port_indev中由PREV重映射) → 跳到 cont_3
 *         LV_KEY_DOWN (原↓键, 由NEXT重映射) → 阻止
 *         cont_3 不在 group 中, 所以 UP/DOWN/LEFT/RIGHT 都不会自动移动焦点。
 */
static void screen_user_home_card_key_handler(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code != LV_EVENT_KEY) return;

    uint32_t key = lv_event_get_key(e);

    /* 记录当前聚焦的卡片索引 */
    lv_obj_t *focused = lv_group_get_focused(g_keypad_group);
    if (focused == guider_ui.screen_user_home_card_solar)      s_home_focus_card = 0;
    else if (focused == guider_ui.screen_user_home_card_device) s_home_focus_card = 1;
    else if (focused == guider_ui.screen_user_home_card_alert)  s_home_focus_card = 2;

    if (key == LV_KEY_LEFT) {
        /* 在3张卡片间反向循环 (不经过cont_3) */
        if (s_home_focus_card == 0) lv_group_focus_obj(guider_ui.screen_user_home_card_alert);
        else if (s_home_focus_card == 1) lv_group_focus_obj(guider_ui.screen_user_home_card_solar);
        else lv_group_focus_obj(guider_ui.screen_user_home_card_device);
        lv_event_stop_processing(e);
        lv_event_stop_bubbling(e);
    }
    else if (key == LV_KEY_RIGHT) {
        /* 在3张卡片间正向循环 (不经过cont_3) */
        if (s_home_focus_card == 0) lv_group_focus_obj(guider_ui.screen_user_home_card_device);
        else if (s_home_focus_card == 1) lv_group_focus_obj(guider_ui.screen_user_home_card_alert);
        else lv_group_focus_obj(guider_ui.screen_user_home_card_solar);
        lv_event_stop_processing(e);
        lv_event_stop_bubbling(e);
    }
    else if (key == LV_KEY_UP) {
        /* UP: 阻止 (cont_3不再加入焦点组) */
        lv_event_stop_processing(e);
        lv_event_stop_bubbling(e);
    }
    else if (key == LV_KEY_DOWN) {
        /* DOWN (原↓键/NEXT): 跳到底部设置按钮 */
        lv_group_focus_obj(guider_ui.screen_user_home_btn_setting);
        lv_event_stop_processing(e);
        lv_event_stop_bubbling(e);
    }
}

/**
 * @brief  Home页面: 设置按钮按键处理
 *         LV_KEY_UP → 回到上次聚焦的卡片
 *         LV_KEY_LEFT/RIGHT/DOWN → 阻止
 *         LV_KEY_ENTER → 触发CLICKED进入系统设置页
 */
static void screen_user_home_btn_setting_key_handler(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code != LV_EVENT_KEY) return;
    uint32_t key = lv_event_get_key(e);

    if (key == LV_KEY_UP) {
        /* UP: 回到上次聚焦的卡片 */
        lv_obj_t *cards[] = {
            guider_ui.screen_user_home_card_solar,
            guider_ui.screen_user_home_card_device,
            guider_ui.screen_user_home_card_alert
        };
        if (s_home_focus_card < 3) {
            lv_group_focus_obj(cards[s_home_focus_card]);
        } else {
            lv_group_focus_obj(guider_ui.screen_user_home_card_device);
        }
        lv_event_stop_processing(e);
        lv_event_stop_bubbling(e);
    }
    else if (key == LV_KEY_DOWN || key == LV_KEY_LEFT || key == LV_KEY_RIGHT) {
        /* 阻止, 不做任何操作 */
        lv_event_stop_processing(e);
        lv_event_stop_bubbling(e);
    }
    /* ENTER: 不拦截, 让LVGL触发CLICKED事件 */
}

/**
 * @brief  Home页面: 设置按钮点击事件 (进入系统设置页)
 */
static void screen_user_home_btn_setting_event_handler(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) {
        ui_load_scr_animation(&guider_ui, &guider_ui.screen_sys_setting,
                              guider_ui.screen_sys_setting_del,
                              &guider_ui.screen_user_home_del,
                              setup_scr_screen_sys_setting,
                              LV_SCR_LOAD_ANIM_NONE, 10, 10, false, false);
    }
}

/**
 * @brief  Home页面: cont_3 (顶部状态栏) 按键处理
 *         LV_KEY_DOWN (原↓键/NEXT) → 回到上次聚焦的卡片
 *         其他按键 → 阻止
 */
static void screen_user_home_cont3_key_handler(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code != LV_EVENT_KEY) return;

    uint32_t key = lv_event_get_key(e);

    if (key == LV_KEY_DOWN) {
        /* NEXT: 从 cont_3 回到上次聚焦的卡片 */
        lv_obj_set_style_border_width(guider_ui.screen_user_home_cont_3, 1, 0);
        lv_obj_set_style_border_color(guider_ui.screen_user_home_cont_3, lv_color_hex(0x30363D), 0);
        lv_obj_set_style_border_opa(guider_ui.screen_user_home_cont_3, 80, 0);

        /* 移除 cont_3, 重新添加 3 张卡片, 聚焦到上次记忆的卡片 */
        lv_group_remove_obj(guider_ui.screen_user_home_cont_3);
        lv_group_add_obj(g_keypad_group, guider_ui.screen_user_home_card_solar);
        lv_group_add_obj(g_keypad_group, guider_ui.screen_user_home_card_device);
        lv_group_add_obj(g_keypad_group, guider_ui.screen_user_home_card_alert);
        lv_group_add_obj(g_keypad_group, guider_ui.screen_user_home_btn_setting);

        lv_obj_t *cards[] = {
            guider_ui.screen_user_home_card_solar,
            guider_ui.screen_user_home_card_device,
            guider_ui.screen_user_home_card_alert
        };
        if(s_home_focus_card < 3) {
            lv_group_focus_obj(cards[s_home_focus_card]);
        } else {
            lv_group_focus_obj(guider_ui.screen_user_home_card_device);
        }
    }
    else if (key == LV_KEY_ENTER) {
        /* ENTER: 进入系统设置页 */
        lv_obj_set_style_border_width(guider_ui.screen_user_home_cont_3, 1, 0);
        lv_obj_set_style_border_color(guider_ui.screen_user_home_cont_3, lv_color_hex(0x30363D), 0);
        lv_obj_set_style_border_opa(guider_ui.screen_user_home_cont_3, 80, 0);
        ui_load_scr_animation(&guider_ui, &guider_ui.screen_sys_setting, guider_ui.screen_sys_setting_del, &guider_ui.screen_user_home_del, setup_scr_screen_sys_setting, LV_SCR_LOAD_ANIM_NONE, 10, 10, false, false);
    }
    /* 吞掉所有按键, 防止影响其他对象 */
    lv_event_stop_processing(e);
    lv_event_stop_bubbling(e);
}

static void screen_user_home_event_handler (lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    switch (code) {
    case LV_EVENT_SCREEN_LOADED:
    {
        g_need_key_remap = 1;  /* Home页面需要NEXT/PREV重映射 */
        lv_group_remove_all_objs(g_keypad_group);//清空group中的所有组件
        //给group添加3个数据卡片 (cont_3 不加入group, 焦点由回调手动控制)
        lv_group_add_obj(g_keypad_group, guider_ui.screen_user_home_card_solar);
        lv_group_add_obj(g_keypad_group, guider_ui.screen_user_home_card_device);
        lv_group_add_obj(g_keypad_group, guider_ui.screen_user_home_card_alert);
        lv_group_add_obj(g_keypad_group, guider_ui.screen_user_home_btn_setting);
        //为卡片和cont_3注册按键回调 (仅注册一次, 避免重复)
        {
            static bool s_home_key_cb_added = false;
            if (!s_home_key_cb_added) {
                lv_obj_add_event_cb(guider_ui.screen_user_home_card_solar,
                                    screen_user_home_card_key_handler, LV_EVENT_KEY, NULL);
                lv_obj_add_event_cb(guider_ui.screen_user_home_card_device,
                                    screen_user_home_card_key_handler, LV_EVENT_KEY, NULL);
                lv_obj_add_event_cb(guider_ui.screen_user_home_card_alert,
                                    screen_user_home_card_key_handler, LV_EVENT_KEY, NULL);
                /* cont_3 按键回调 (cont_3 不在group中, 但需要CLICKABLE接收按键事件) */
                lv_obj_add_flag(guider_ui.screen_user_home_cont_3, LV_OBJ_FLAG_CLICKABLE);
                lv_obj_clear_flag(guider_ui.screen_user_home_cont_3, LV_OBJ_FLAG_SCROLLABLE);
                lv_obj_add_event_cb(guider_ui.screen_user_home_cont_3,
                                    screen_user_home_cont3_key_handler, LV_EVENT_KEY, NULL);
                /* 设置按钮按键回调 */
                lv_obj_add_event_cb(guider_ui.screen_user_home_btn_setting,
                                    screen_user_home_btn_setting_key_handler, LV_EVENT_KEY, NULL);
                s_home_key_cb_added = true;
            }
        }
        //将按键添加进焦点组
        lv_indev_set_group(indev_keypad, g_keypad_group);
        //恢复上次聚焦的卡片
        {
            lv_obj_t *cards[] = {
                guider_ui.screen_user_home_card_solar,
                guider_ui.screen_user_home_card_device,
                guider_ui.screen_user_home_card_alert
            };
            if(s_home_focus_card < 3) {
                lv_group_focus_obj(cards[s_home_focus_card]);
            } else {
                lv_group_focus_obj(guider_ui.screen_user_home_card_device);
            }
        }
        //显示IP地址和端口号
        lv_label_set_text(guider_ui.screen_user_home_label_ip, server_ip_buf);
        lv_label_set_text(guider_ui.screen_user_home_label_port, server_port_buf);
        break;
    }
    default:
        break;
    }
}

/* 太阳能卡片: FOCUSED/DEFOCUSED/CLICKED */
static void screen_user_home_card_solar_event_handler (lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_FOCUSED) {
        lv_obj_set_style_border_color(guider_ui.screen_user_home_card_solar, lv_color_hex(0x58A6FF), 0);
        lv_obj_set_style_border_opa(guider_ui.screen_user_home_card_solar, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(guider_ui.screen_user_home_card_solar, 3, 0);
        lv_obj_set_style_shadow_width(guider_ui.screen_user_home_card_solar, 20, 0);
        lv_obj_set_style_shadow_opa(guider_ui.screen_user_home_card_solar, 100, 0);
        lv_obj_set_style_shadow_ofs_y(guider_ui.screen_user_home_card_solar, 6, 0);
        /* 整体放大: 卡片 + 内部body一起变大 */
        lv_obj_set_size(guider_ui.screen_user_home_card_solar, 150, 108);
        lv_obj_set_pos(guider_ui.screen_user_home_card_solar, 15, 151);
        lv_obj_t *_body = lv_obj_get_child(guider_ui.screen_user_home_card_solar, 1);
        if(_body) { lv_obj_set_size(_body, 146, 70); lv_obj_set_pos(_body, 2, 36); }
    }
    else if (code == LV_EVENT_DEFOCUSED) {
        lv_obj_set_style_border_color(guider_ui.screen_user_home_card_solar, lv_color_hex(0x30363D), 0);
        lv_obj_set_style_border_width(guider_ui.screen_user_home_card_solar, 1, 0);
        lv_obj_set_style_shadow_width(guider_ui.screen_user_home_card_solar, 10, 0);
        lv_obj_set_style_shadow_opa(guider_ui.screen_user_home_card_solar, 30, 0);
        lv_obj_set_style_shadow_ofs_y(guider_ui.screen_user_home_card_solar, 3, 0);
        /* 恢复原始大小 */
        lv_obj_set_size(guider_ui.screen_user_home_card_solar, 140, 100);
        lv_obj_set_pos(guider_ui.screen_user_home_card_solar, 20, 155);
        lv_obj_t *_body = lv_obj_get_child(guider_ui.screen_user_home_card_solar, 1);
        if(_body) { lv_obj_set_size(_body, 138, 64); lv_obj_set_pos(_body, 1, 33); }
    }
    else if (code == LV_EVENT_CLICKED) {
        s_home_focus_card = 0;  /* 记住: 太阳能卡片 */
        /* 跳转到太阳能发电量详情页 */
        ui_load_scr_animation(&guider_ui, &guider_ui.screen_solar, guider_ui.screen_solar_del, &guider_ui.screen_user_home_del, setup_scr_screen_solar, LV_SCR_LOAD_ANIM_NONE, 10, 10, false, false);
    }
}

/* 设备在线卡片: FOCUSED/DEFOCUSED/CLICKED */
static void screen_user_home_card_device_event_handler (lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_FOCUSED) {
        lv_obj_set_style_border_color(guider_ui.screen_user_home_card_device, lv_color_hex(0x3FB950), 0);
        lv_obj_set_style_border_opa(guider_ui.screen_user_home_card_device, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(guider_ui.screen_user_home_card_device, 3, 0);
        lv_obj_set_style_shadow_width(guider_ui.screen_user_home_card_device, 20, 0);
        lv_obj_set_style_shadow_opa(guider_ui.screen_user_home_card_device, 100, 0);
        lv_obj_set_style_shadow_ofs_y(guider_ui.screen_user_home_card_device, 6, 0);
        /* 整体放大 */
        lv_obj_set_size(guider_ui.screen_user_home_card_device, 150, 108);
        lv_obj_set_pos(guider_ui.screen_user_home_card_device, 165, 151);
        lv_obj_t *_body = lv_obj_get_child(guider_ui.screen_user_home_card_device, 1);
        if(_body) { lv_obj_set_size(_body, 146, 70); lv_obj_set_pos(_body, 2, 36); }
    }
    else if (code == LV_EVENT_DEFOCUSED) {
        lv_obj_set_style_border_color(guider_ui.screen_user_home_card_device, lv_color_hex(0x30363D), 0);
        lv_obj_set_style_border_width(guider_ui.screen_user_home_card_device, 1, 0);
        lv_obj_set_style_shadow_width(guider_ui.screen_user_home_card_device, 10, 0);
        lv_obj_set_style_shadow_opa(guider_ui.screen_user_home_card_device, 30, 0);
        lv_obj_set_style_shadow_ofs_y(guider_ui.screen_user_home_card_device, 3, 0);
        /* 恢复原始大小 */
        lv_obj_set_size(guider_ui.screen_user_home_card_device, 140, 100);
        lv_obj_set_pos(guider_ui.screen_user_home_card_device, 170, 155);
        lv_obj_t *_body = lv_obj_get_child(guider_ui.screen_user_home_card_device, 1);
        if(_body) { lv_obj_set_size(_body, 138, 64); lv_obj_set_pos(_body, 1, 33); }
    }
    else if (code == LV_EVENT_CLICKED) {
        s_home_focus_card = 1;  /* 记住: 设备在线卡片 */
        /* 无动画直接切换到用户列表页 (is_clean=false避免在卡片回调中清理home屏幕) */
        ui_load_scr_animation(&guider_ui, &guider_ui.screen_user_list, guider_ui.screen_user_list_del, &guider_ui.screen_user_home_del, setup_scr_screen_user_list, LV_SCR_LOAD_ANIM_NONE, 10, 10, false, false);
    }
}

/* 告警卡片: FOCUSED/DEFOCUSED/CLICKED */
static void screen_user_home_card_alert_event_handler (lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_FOCUSED) {
        lv_obj_set_style_border_color(guider_ui.screen_user_home_card_alert, lv_color_hex(0xD29922), 0);
        lv_obj_set_style_border_opa(guider_ui.screen_user_home_card_alert, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(guider_ui.screen_user_home_card_alert, 3, 0);
        lv_obj_set_style_shadow_width(guider_ui.screen_user_home_card_alert, 20, 0);
        lv_obj_set_style_shadow_opa(guider_ui.screen_user_home_card_alert, 100, 0);
        lv_obj_set_style_shadow_ofs_y(guider_ui.screen_user_home_card_alert, 6, 0);
        /* 整体放大 */
        lv_obj_set_size(guider_ui.screen_user_home_card_alert, 150, 108);
        lv_obj_set_pos(guider_ui.screen_user_home_card_alert, 315, 151);
        lv_obj_t *_body = lv_obj_get_child(guider_ui.screen_user_home_card_alert, 1);
        if(_body) { lv_obj_set_size(_body, 146, 70); lv_obj_set_pos(_body, 2, 36); }
    }
    else if (code == LV_EVENT_DEFOCUSED) {
        lv_obj_set_style_border_color(guider_ui.screen_user_home_card_alert, lv_color_hex(0x30363D), 0);
        lv_obj_set_style_border_width(guider_ui.screen_user_home_card_alert, 1, 0);
        lv_obj_set_style_shadow_width(guider_ui.screen_user_home_card_alert, 10, 0);
        lv_obj_set_style_shadow_opa(guider_ui.screen_user_home_card_alert, 30, 0);
        lv_obj_set_style_shadow_ofs_y(guider_ui.screen_user_home_card_alert, 3, 0);
        /* 恢复原始大小 */
        lv_obj_set_size(guider_ui.screen_user_home_card_alert, 140, 100);
        lv_obj_set_pos(guider_ui.screen_user_home_card_alert, 320, 155);
        lv_obj_t *_body = lv_obj_get_child(guider_ui.screen_user_home_card_alert, 1);
        if(_body) { lv_obj_set_size(_body, 138, 64); lv_obj_set_pos(_body, 1, 33); }
    }
    else if (code == LV_EVENT_CLICKED) {
        s_home_focus_card = 2;  /* 记住: 告警卡片 */
        /* 跳转到告警列表页 */
        ui_load_scr_animation(&guider_ui, &guider_ui.screen_alert, guider_ui.screen_alert_del, &guider_ui.screen_user_home_del, setup_scr_screen_alert, LV_SCR_LOAD_ANIM_NONE, 10, 10, false, false);
    }
}

void events_init_screen_user_home (lv_ui *ui)
{
    lv_obj_add_event_cb(ui->screen_user_home, screen_user_home_event_handler, LV_EVENT_ALL, ui);
    lv_obj_add_event_cb(ui->screen_user_home_card_solar, screen_user_home_card_solar_event_handler, LV_EVENT_ALL, ui);
    lv_obj_add_event_cb(ui->screen_user_home_card_device, screen_user_home_card_device_event_handler, LV_EVENT_ALL, ui);
    lv_obj_add_event_cb(ui->screen_user_home_card_alert, screen_user_home_card_alert_event_handler, LV_EVENT_ALL, ui);
    lv_obj_add_event_cb(ui->screen_user_home_btn_setting, screen_user_home_btn_setting_event_handler, LV_EVENT_ALL, ui);
}

/* ======== User List 页事件处理 (分页版) ======== */

#define LIST_PAGE_SIZE  6  /* 每页显示的最大设备数 (256设备÷20页, 每页~12KB) */

static uint16_t s_list_page = 0;    /* 当前页码 (0-based) */
static lv_obj_t *s_list_page_label = NULL;  /* 页码指示标签 */

/**
 * @brief  计算总页数
 */
static uint16_t list_get_total_pages(void)
{
    if(device_count == 0) return 1;
    return (device_count + LIST_PAGE_SIZE - 1) / LIST_PAGE_SIZE;
}

/**
 * @brief  填充当前页的列表项
 * @note   仅创建 s_list_page 对应的 LIST_PAGE_SIZE 个btn, 节省内存
 */
static void list_populate_current_page(void)
{
    house_info_t house;
    lv_obj_clean(guider_ui.screen_user_list_list_1);//清空当前列表中的内容
    lv_group_remove_all_objs(g_keypad_group);//清空group中的所有组件

    uint16_t start = s_list_page * LIST_PAGE_SIZE;
    uint16_t end = start + LIST_PAGE_SIZE;
    if(end > device_count) end = device_count;

    /* 更新页码指示 */
    if(s_list_page_label && lv_obj_is_valid(s_list_page_label)) {
        char page_txt[32];
        lv_snprintf(page_txt, sizeof(page_txt), "%d/%d页  共%d台 ",
                    s_list_page + 1, list_get_total_pages(), device_count);
        lv_label_set_text(s_list_page_label, page_txt);
    }

    for(uint16_t i = start; i < end; i++)
    {
        char txt[100];
			
        /* ======== 使用lv_obj_create代替lv_list_add_btn，避免lv_list默认聚焦样式 ======== */
        lv_obj_t *btn = lv_obj_create(guider_ui.screen_user_list_list_1);
        lv_obj_remove_style_all(btn);
        lv_obj_set_size(btn, 448, 38);
        lv_obj_set_style_radius(btn, 10, 0);
        lv_obj_set_style_bg_color(btn, lv_color_hex(0x161B22), 0);
        lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(btn, 1, 0);
        lv_obj_set_style_border_color(btn, lv_color_hex(0x30363D), 0);
        lv_obj_set_style_border_opa(btn, 60, 0);
        lv_obj_set_style_pad_all(btn, 0, 0);
        lv_obj_set_style_shadow_width(btn, 4, 0);
        lv_obj_set_style_shadow_color(btn, lv_color_hex(0x000000), 0);
        lv_obj_set_style_shadow_opa(btn, 30, 0);
        lv_obj_set_style_shadow_ofs_y(btn, 2, 0);
        lv_obj_clear_flag(btn, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(btn, LV_OBJ_FLAG_CLICKABLE);
			
        if(device_list[i].state.bits.valid == 0)
        {
            lv_snprintf(txt, sizeof(txt), "未入网   MAC %02X:%02X:%02X:%02X:%02X:%02X  ",
                        device_list[i].mac[0],device_list[i].mac[1],device_list[i].mac[2],
                        device_list[i].mac[3],device_list[i].mac[4],device_list[i].mac[5]);
        }
        else
        {
            parse_addr(device_list[i].addr, &house);
            lv_snprintf(txt, sizeof(txt), "%d楼 %d单元 %d   MAC %02X:%02X:%02X:%02X:%02X:%02X ",
                        house.building, house.unit, house.room,
                        device_list[i].mac[0],device_list[i].mac[1],device_list[i].mac[2],
                        device_list[i].mac[3],device_list[i].mac[4],device_list[i].mac[5]);
            /* 通信状态图标 (末尾): comm_err=1红色UNLINK, comm_err=0绿色LINK */
            lv_obj_t *status_icon = lv_label_create(btn);
            if(device_list[i].state.bits.comm_err) {
                lv_label_set_text(status_icon, LV_SYMBOL_UNLINK);
                lv_obj_set_style_text_color(status_icon, lv_color_hex(0xF44336), 0); /* 红色 */
            } else {
                lv_label_set_text(status_icon, LV_SYMBOL_LINK);
                lv_obj_set_style_text_color(status_icon, lv_color_hex(0x3FB950), 0); /* 绿色 */
            }
            lv_obj_set_style_text_font(status_icon, &lv_font_SourceHanSerifSC_Regular_16, 0);
            lv_obj_set_style_bg_opa(status_icon, 0, 0);
            lv_obj_align(status_icon, LV_ALIGN_RIGHT_MID, -10, 0);
        }

        /* 图标 */
        lv_obj_t *icon = lv_label_create(btn);
        lv_label_set_text(icon, LV_SYMBOL_HOME);
        lv_obj_set_style_text_color(icon, lv_color_hex(0xF0C040), 0);
        lv_obj_set_style_text_font(icon, &lv_font_SourceHanSerifSC_Regular_16, 0);
        lv_obj_set_style_bg_opa(icon, 0, 0);
        lv_obj_set_pos(icon, 14, 9);

        /* 文本 */
        lv_obj_t *lbl = lv_label_create(btn);
        lv_label_set_text(lbl, txt);
        lv_obj_set_style_text_color(lbl, lv_color_hex(0xE6EDF3), 0);
        lv_obj_set_style_text_font(lbl, &lv_font_SourceHanSerifSC_Regular_16, 0);
        lv_obj_set_style_bg_opa(lbl, 0, 0);
        lv_obj_set_pos(lbl, 40, 9);

        /* ======== 聚焦样式 (蓝色左边框) ======== */
        lv_obj_set_style_border_width(btn, 4, LV_PART_MAIN | LV_STATE_FOCUSED);
        lv_obj_set_style_border_color(btn, lv_color_hex(0x58A6FF), LV_PART_MAIN | LV_STATE_FOCUSED);
        lv_obj_set_style_border_opa(btn, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_FOCUSED);
        lv_obj_set_style_border_side(btn, LV_BORDER_SIDE_LEFT, LV_PART_MAIN | LV_STATE_FOCUSED);
        lv_obj_set_style_shadow_width(btn, 8, LV_PART_MAIN | LV_STATE_FOCUSED);
        lv_obj_set_style_shadow_color(btn, lv_color_hex(0x58A6FF), LV_PART_MAIN | LV_STATE_FOCUSED);
        lv_obj_set_style_shadow_opa(btn, 30, LV_PART_MAIN | LV_STATE_FOCUSED);
        lv_obj_set_style_shadow_ofs_y(btn, 3, LV_PART_MAIN | LV_STATE_FOCUSED);
        lv_obj_set_style_bg_color(btn, lv_color_hex(0x1C2333), LV_PART_MAIN | LV_STATE_FOCUSED);
        lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_FOCUSED);
        lv_obj_set_style_radius(btn, 10, LV_PART_MAIN | LV_STATE_FOCUSED);

        lv_group_add_obj(g_keypad_group, btn);
        lv_obj_add_event_cb(btn, user_list_item_event_handler, LV_EVENT_CLICKED, (void*)(uintptr_t)i);
        lv_obj_add_event_cb(btn, screen_user_list_item_event_handler, LV_EVENT_KEY, (void*)(uintptr_t)i);

        /* 如果这是上次选中的编号，就让它获得焦点 */
        if(i == s_last_user_no) {
            lv_group_focus_obj(btn);
        }
    }

    lv_indev_set_group(indev_keypad, g_keypad_group);

    if (end <= start) {
        /* 空列表：聚焦屏幕本身，绑定KEY事件回调以支持ESC返回 */
        lv_group_add_obj(g_keypad_group, guider_ui.screen_user_list);
        lv_obj_add_event_cb(guider_ui.screen_user_list, screen_user_list_item_event_handler, LV_EVENT_KEY, NULL);
        lv_group_focus_obj(guider_ui.screen_user_list);
    } else if (lv_group_get_focused(g_keypad_group) == NULL) {
        /* 有数据但没有匹配到上次焦点，聚焦第一项 */
        lv_obj_t *first_btn = lv_obj_get_child(guider_ui.screen_user_list_list_1, 0);
        if(first_btn) lv_group_focus_obj(first_btn);
    }
}

/**
 * @brief  重置页码到第1页 (供key.c ESC返回时调用)
 */
void List_ResetPage(void)
{
    s_list_page = 0;
}

/**
 * @brief  翻到下一页 (供key.c调用)
 */
void List_NextPage(void)
{
    uint16_t total = list_get_total_pages();
    if(s_list_page + 1 < total) {
        s_list_page++;
        /* 保存第一项索引，确保焦点落在新页的第一个 */
        s_last_user_no = s_list_page * LIST_PAGE_SIZE;
        list_populate_current_page();
    }
}

/**
 * @brief  翻到上一页 (供key.c调用)
 */
void List_PrevPage(void)
{
    if(s_list_page > 0) {
        s_list_page--;
        s_last_user_no = s_list_page * LIST_PAGE_SIZE;
        list_populate_current_page();
    }
}

static void screen_user_list_event_handler (lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    switch (code) {
    case LV_EVENT_SCREEN_LOADED:
    {
        g_need_key_remap = 0;  /* 列表页面不需要重映射, NEXT/PREV走默认group导航 */
        /* 根据 s_last_user_no 计算应该显示的页码 */
        if(device_count > 0 && s_last_user_no < device_count) {
            s_list_page = s_last_user_no / LIST_PAGE_SIZE;
        } else {
            s_list_page = 0;
        }

        /* 创建页码指示标签 (如果还没有创建) */
        if(s_list_page_label == NULL || !lv_obj_is_valid(s_list_page_label)) {
            s_list_page_label = lv_label_create(guider_ui.screen_user_list);
            lv_obj_set_style_text_font(s_list_page_label, &lv_font_SourceHanSerifSC_Regular_16, 0);
            lv_obj_set_style_text_color(s_list_page_label, lv_color_hex(0x484F58), 0);
            lv_obj_align(s_list_page_label, LV_ALIGN_BOTTOM_RIGHT, -10, -5);
        }

        list_populate_current_page();
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

/* ======== User Detail 页事件处理 ======== */

static void screen_user_detail_event_handler (lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    switch (code) {
    case LV_EVENT_KEY:
    {
        uint32_t key = lv_event_get_key(e);
        if(key == LV_KEY_ESC)
        {
            /* 无动画直接返回用户列表页 */
            ui_load_scr_animation(&guider_ui, &guider_ui.screen_user_list, guider_ui.screen_user_list_del, &guider_ui.screen_user_detail_del, setup_scr_screen_user_list, LV_SCR_LOAD_ANIM_NONE, 10, 10, true, true);
        }

        break;
    }
    case LV_EVENT_SCREEN_LOADED:
    {
        g_need_key_remap = 0;  /* 用户详情页面不需要重映射 */
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

/* ======== Solar 太阳能详情页事件处理 ======== */

static void screen_solar_event_handler (lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    switch (code) {
    case LV_EVENT_KEY:
    {
        uint32_t key = lv_event_get_key(e);
        if(key == LV_KEY_ESC)
        {
            /* 返回首页 */
            ui_load_scr_animation(&guider_ui, &guider_ui.screen_user_home, guider_ui.screen_user_home_del, &guider_ui.screen_solar_del, setup_scr_screen_user_home, LV_SCR_LOAD_ANIM_NONE, 10, 10, true, true);
        }
        break;
    }
    case LV_EVENT_SCREEN_LOADED:
    {
        g_need_key_remap = 0;  /* 太阳能详情页面不需要重映射 */
        lv_group_remove_all_objs(g_keypad_group);
        lv_group_add_obj(g_keypad_group, guider_ui.screen_solar);
        lv_indev_set_group(indev_keypad, g_keypad_group);
				//设置初始焦点
        lv_group_focus_obj(guider_ui.screen_solar);

        /* ======== 更新发电量数据到UI ======== */
        {
            char buf[32];

            /* 更新统计表格：总发电、年发电、月发电、日发电 */
            if (guider_ui.screen_solar_table)
            {
                snprintf(buf, sizeof(buf), "%.1f", (double)g_solar_energy.total_generation_kwh);
                lv_table_set_cell_value(guider_ui.screen_solar_table, 1, 0, buf);

                snprintf(buf, sizeof(buf), "%.1f", (double)g_solar_energy.annual_generation_kwh);
                lv_table_set_cell_value(guider_ui.screen_solar_table, 1, 1, buf);

                snprintf(buf, sizeof(buf), "%.1f", (double)g_solar_energy.monthly_generation_kwh);
                lv_table_set_cell_value(guider_ui.screen_solar_table, 1, 2, buf);

                snprintf(buf, sizeof(buf), "%.1f", (double)g_solar_energy.daily_generation_kwh);
                lv_table_set_cell_value(guider_ui.screen_solar_table, 1, 3, buf);
            }

            /* 更新15日发电量折线图 */
            if (guider_ui.screen_solar_chart)
            {
                int16_t val = 0;
                lv_chart_series_t *ser = lv_chart_get_series_next(guider_ui.screen_solar_chart, NULL);
                if (ser)
                {
                    /* 找出15日中的最大值，用于动态调整Y轴范围 */
                    float max_val = 0.0f;
                    for (int i = 0; i < SOLAR_HISTORY_DAYS; i++)
                    {
                        if (g_solar_energy.history_daily[i] > max_val)
                            max_val = g_solar_energy.history_daily[i];
                    }

                    /* 设置Y轴范围：最大值留20%余量，最小范围0~30(即300) */
                    int16_t y_max = (int16_t)(max_val * 10.0f * 1.2f + 0.5f);
                    if (y_max < 300) y_max = 300;
                    lv_chart_set_range(guider_ui.screen_solar_chart, LV_CHART_AXIS_PRIMARY_Y, 0, y_max);

                    /* 填充数据：kWh * 10 转整数，加0.5f做四舍五入避免浮点截断误差 */
                    for (int i = 0; i < SOLAR_HISTORY_DAYS; i++)
                    {
                        val = (int16_t)(g_solar_energy.history_daily[i] * 10.0f + 0.5f);
                        ser->y_points[i] = val;
                    }
                    lv_chart_refresh(guider_ui.screen_solar_chart);
                }
            }

            /* 更新底部时间标签 */
            if (guider_ui.screen_solar_label_time)
            {
                if (g_solar_energy.update_time.year != 0 || g_solar_energy.update_time.month != 0)
                {
                    snprintf(buf, sizeof(buf), "20%02d-%02d-%02d %02d:%02d:%02d",
                             g_solar_energy.update_time.year, g_solar_energy.update_time.month,
                             g_solar_energy.update_time.day, g_solar_energy.update_time.hours,
                             g_solar_energy.update_time.minutes, g_solar_energy.update_time.seconds);
                    lv_label_set_text(guider_ui.screen_solar_label_time, buf);
                }
                else
                {
                    lv_label_set_text(guider_ui.screen_solar_label_time, "----");
                }
            }
        }
        break;
    }
    default:
        break;
    }
}

void events_init_screen_solar (lv_ui *ui)
{
    lv_obj_add_event_cb(ui->screen_solar, screen_solar_event_handler, LV_EVENT_ALL, ui);
}


/* ======== Alert 告警页事件处理 ======== */

/* 告警类型、最大项数已在 device_manager.h 中定义 */

static const char * alert_type_name(alert_type_t t)
{
    switch(t) {
        case ALERT_COMM_FAIL:     return "通信异常 ";
        case ALERT_RELAY_ERR:     return "开关异常 ";
        case ALERT_TEMP_ERR:      return "温度异常 ";
        case ALERT_POWER_REVERSE: return "电源反接 ";
        default:                  return "未知 ";
    }
}

static lv_color_t alert_type_color(alert_type_t t)
{
    switch(t) {
        case ALERT_COMM_FAIL:     return lv_color_hex(0xFFC107);
        case ALERT_RELAY_ERR:     return lv_color_hex(0xFF9800);
        case ALERT_TEMP_ERR:      return lv_color_hex(0xF44336);
        case ALERT_POWER_REVERSE: return lv_color_hex(0xFF5722);
        default:                  return lv_color_hex(0x9E9E9E);
    }
}

static const char * alert_type_icon(alert_type_t t)
{
    switch(t) {
        case ALERT_COMM_FAIL:     return LV_SYMBOL_CLOSE;
        case ALERT_RELAY_ERR:     return LV_SYMBOL_BELL;
        case ALERT_TEMP_ERR:      return LV_SYMBOL_WARNING;
        case ALERT_POWER_REVERSE: return LV_SYMBOL_WARNING;
        default:                  return LV_SYMBOL_BELL;
    }
}

/* 编码/解码 告警项的user_data: 高4bit=alert_type, 低12bit=dev_idx */
#define ALERT_ENCODE(dev_idx, type)  ((void*)(uintptr_t)(((type) << 12) | ((dev_idx) & 0xFFF)))
#define ALERT_DECODE_IDX(ud)         ((int)(uintptr_t)(ud) & 0xFFF)
#define ALERT_DECODE_TYPE(ud)        ((int)(uintptr_t)(ud) >> 12)

/* 告警列表分页配置 */
#define ALERT_PAGE_SIZE  7  /* 每页最大告警项数 (列表高度188px / 每项29px ≈ 6) */
static uint16_t s_alert_page = 0;    /* 当前告警页码 (0-based) */

/* 前向声明 */
static void alert_item_event_handler(lv_event_t *e);
static void alert_dialog_btn_cb(lv_event_t *e);
static void alert_tip_event_handler(lv_event_t *e);
static void alert_clear_all_dialog_btn_cb(lv_event_t *e);
static void alert_populate_current_page(void);

/**
 * @brief  创建单个告警项控件
 * @param  parent   父容器
 * @param  dev_idx  设备在device_list中的下标
 * @param  type     告警类型
 * @param  addr_txt 地址文本 "X楼 X单元 XXXX"
 */
static lv_obj_t * alert_create_item(lv_obj_t *parent, int dev_idx,
                                     alert_type_t type, const char *addr_txt)
{
    lv_obj_t *item = lv_obj_create(parent);
    lv_obj_remove_style_all(item);
    lv_obj_set_size(item, 448, 25);
    lv_obj_set_style_radius(item, 4, 0);
    lv_obj_set_style_bg_color(item, lv_color_hex(0x161B22), 0);
    lv_obj_set_style_bg_opa(item, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(item, 1, 0);
    lv_obj_set_style_border_color(item, lv_color_hex(0x30363D), 0);
    lv_obj_set_style_border_opa(item, 60, 0);
    lv_obj_set_style_pad_all(item, 0, 0);
    lv_obj_clear_flag(item, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(item, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_CHECKABLE);

    /* 聚焦样式 */
    static lv_style_t style_alert_focused;
    ui_init_style(&style_alert_focused);
    lv_style_set_border_width(&style_alert_focused, 3);
    lv_style_set_border_color(&style_alert_focused, lv_color_hex(0x58A6FF));
    lv_style_set_border_opa(&style_alert_focused, LV_OPA_COVER);
    lv_style_set_border_side(&style_alert_focused, LV_BORDER_SIDE_LEFT);
    lv_style_set_bg_color(&style_alert_focused, lv_color_hex(0x1C2333));
    lv_style_set_bg_opa(&style_alert_focused, LV_OPA_COVER);
    lv_style_set_radius(&style_alert_focused, 4);
    lv_obj_add_style(item, &style_alert_focused, LV_PART_MAIN | LV_STATE_FOCUSED);

    /* 左侧颜色指示器 */
    lv_obj_t *dot = lv_obj_create(item);
    lv_obj_remove_style_all(dot);
    lv_obj_set_size(dot, 4, 20);
    lv_obj_set_pos(dot, 2, 3);
    lv_obj_set_style_radius(dot, 2, 0);
    lv_obj_set_style_bg_color(dot, alert_type_color(type), 0);
    lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);
    lv_obj_clear_flag(dot, LV_OBJ_FLAG_SCROLLABLE);

    /* 图标 */
    lv_obj_t *icon = lv_label_create(item);
    lv_label_set_text(icon, alert_type_icon(type));
    lv_obj_set_style_text_color(icon, alert_type_color(type), 0);
    lv_obj_set_style_text_font(icon, &lv_font_SourceHanSerifSC_Regular_16, 0);
    lv_obj_set_style_bg_opa(icon, 0, 0);
    lv_obj_set_pos(icon, 12, 4);

    /* 类型名称 */
    lv_obj_t *type_lbl = lv_label_create(item);
    lv_label_set_text(type_lbl, alert_type_name(type));
    lv_obj_set_style_text_color(type_lbl, lv_color_hex(0xE6EDF3), 0);
    lv_obj_set_style_text_font(type_lbl, &lv_font_SourceHanSerifSC_Regular_12, 0);
    lv_obj_set_style_bg_opa(type_lbl, 0, 0);
    lv_obj_set_pos(type_lbl, 32, 6);

    /* 地址 */
    lv_obj_t *addr_lbl = lv_label_create(item);
    lv_label_set_text(addr_lbl, addr_txt);
    lv_obj_set_style_text_color(addr_lbl, lv_color_hex(0x8B949E), 0);
    lv_obj_set_style_text_font(addr_lbl, &lv_font_SourceHanSerifSC_Regular_12, 0);
    lv_obj_set_style_bg_opa(addr_lbl, 0, 0);
    lv_obj_set_pos(addr_lbl, 120, 6);

    /* 存储设备下标到 user_data，供后续操作使用 */
    lv_obj_set_user_data(item, (void*)(uintptr_t)dev_idx);

    /* 告警项事件回调, user_data编码: 高4bit=alert_type, 低12bit=dev_idx */
    lv_obj_add_event_cb(item, alert_item_event_handler, LV_EVENT_ALL,
                         ALERT_ENCODE(dev_idx, type));

    return item;
}

/**
 * @brief  计算告警总页数
 */
static uint16_t alert_get_total_pages(void)
{
    if (g_alert_stats.item_count == 0) return 1;
    return (g_alert_stats.item_count + ALERT_PAGE_SIZE - 1) / ALERT_PAGE_SIZE;
}

/**
 * @brief  填充当前页的告警项
 */
static void alert_populate_current_page(void)
{
    lv_obj_clean(guider_ui.screen_alert_list);
    lv_group_remove_all_objs(g_keypad_group);

    uint16_t start = s_alert_page * ALERT_PAGE_SIZE;
    uint16_t end = start + ALERT_PAGE_SIZE;
    if (end > g_alert_stats.item_count) end = g_alert_stats.item_count;

    /* 更新页码指示 */
    if (guider_ui.screen_alert_page_label && lv_obj_is_valid(guider_ui.screen_alert_page_label))
    {
        char page_txt[32];
        lv_snprintf(page_txt, sizeof(page_txt), "%d/%d页  共%d项 ",
                    s_alert_page + 1, alert_get_total_pages(), g_alert_stats.item_count);
        lv_label_set_text(guider_ui.screen_alert_page_label, page_txt);
    }

    /* 创建当前页的告警项 */
    for (uint16_t i = start; i < end; i++)
    {
        int dev_idx = g_alert_items[i].dev_idx;
        alert_type_t type = g_alert_items[i].type;

        house_info_t house;
        parse_addr(device_list[dev_idx].addr, &house);
        char addr_txt[32];
        lv_snprintf(addr_txt, sizeof(addr_txt), "%d楼 %d单元 %d",
                    house.building, house.unit, house.room);

        lv_obj_t *item = alert_create_item(guider_ui.screen_alert_list, dev_idx,
                                            type, addr_txt);
        lv_group_add_obj(g_keypad_group, item);
    }

    /* 更新tip文本 */
    lv_obj_t *tip_lbl = lv_obj_get_child(guider_ui.screen_alert_tip, 0);
    if (tip_lbl)
    {
        char tip_txt[64];
        lv_snprintf(tip_txt, sizeof(tip_txt),
                    LV_SYMBOL_WARNING " 点击清除所有告警(%d项) ", g_alert_stats.err_total);
        lv_label_set_text(tip_lbl, tip_txt);
    }

    lv_group_add_obj(g_keypad_group, guider_ui.screen_alert_tip);

    /* 更新统计栏标签 */
    char buf[32];
    lv_snprintf(buf, sizeof(buf), LV_SYMBOL_CLOSE " 通信异常: %d", g_alert_stats.comm_cnt);
    lv_label_set_text(guider_ui.screen_alert_stat_comm, buf);
    lv_snprintf(buf, sizeof(buf), LV_SYMBOL_BELL " 开关异常: %d", g_alert_stats.relay_cnt);
    lv_label_set_text(guider_ui.screen_alert_stat_relay, buf);
    lv_snprintf(buf, sizeof(buf), LV_SYMBOL_WARNING " 温度异常: %d", g_alert_stats.temp_cnt);
    lv_label_set_text(guider_ui.screen_alert_stat_temp, buf);
    lv_snprintf(buf, sizeof(buf), LV_SYMBOL_WARNING " 电源反接: %d", g_alert_stats.power_cnt);
    lv_label_set_text(guider_ui.screen_alert_stat_power, buf);

    /* 更新总数角标 */
    lv_snprintf(buf, sizeof(buf), "%d", g_alert_stats.err_total);
    lv_label_set_text(guider_ui.screen_alert_label_count, buf);

    lv_indev_set_group(indev_keypad, g_keypad_group);

    /* 聚焦第一个告警项 */
    if (g_alert_stats.item_count > 0)
    {
        lv_obj_t *first = lv_obj_get_child(guider_ui.screen_alert_list, 0);
        if (first) lv_group_focus_obj(first);
    }
    else
    {
        /* 无告警时聚焦屏幕本身以接收按键事件 */
        lv_group_add_obj(g_keypad_group, guider_ui.screen_alert);
        lv_group_focus_obj(guider_ui.screen_alert);
    }
}

/**
 * @brief  根据预扫描数据填充告警列表（重置页码并刷新）
 */
static void alert_populate_list(void)
{
    s_alert_page = 0;
    alert_populate_current_page();
}

/**
 * @brief  对话框按钮统一回调 (通过 lv_msgbox_get_active_btn 区分是/否)
 *         btn index 0="否", 1="是"
 */
static void alert_dialog_btn_cb(lv_event_t *e)
{
    lv_obj_t *mbox = lv_event_get_user_data(e);
    uint16_t btn_id = lv_msgbox_get_active_btn(mbox);
		printf("btn_id:%d\r\n",btn_id);

    if (btn_id == 1)  /* "是" - 清除故障 */
    {
        /* 从msgbox的user_data恢复 dev_idx 和 alert_type */
        uintptr_t encoded = (uintptr_t)lv_obj_get_user_data(mbox);
        int dev_idx  = ALERT_DECODE_IDX((void*)encoded);
        int atype    = ALERT_DECODE_TYPE((void*)encoded);

        /* 清除对应的错误位 */
        if (dev_idx >= 0 && dev_idx < device_count)
        {
            switch ((alert_type_t)atype)
            {
                case ALERT_COMM_FAIL:    device_list[dev_idx].state.bits.comm_err     = 0;
                                         device_list[dev_idx].comm_fail_cnt = 0;      break;
                case ALERT_RELAY_ERR:    device_list[dev_idx].state.bits.relay_err    = 0; break;
                case ALERT_TEMP_ERR:     device_list[dev_idx].state.bits.temp_err     = 0; break;
                case ALERT_POWER_REVERSE:device_list[dev_idx].state.bits.power_reverse= 0; break;
                default: break;
            }
        }

        lv_msgbox_close(mbox);//关闭对话框

        /* 重新扫描告警数据并刷新列表 */
        alert_scan_devices();
        alert_populate_list();
        Alert_Widget_Update();
    }
    else  /* "否" - 仅关闭对话框 */
    {
        lv_msgbox_close(mbox);
        
        alert_populate_list();
    }
}

/**
 * @brief  告警项事件回调
 * @param  e  事件对象, user_data编码: 高4bit=alert_type, 低12bit=dev_idx
 */
static void alert_item_event_handler(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);

    if (code == LV_EVENT_FOCUSED)
    {
        /* 焦点变化时，自动滚动父容器使焦点项可见 */
        lv_obj_t *item = lv_event_get_target(e);
        lv_obj_scroll_to_view(item, LV_ANIM_OFF);
    }
    else if (code == LV_EVENT_KEY)
    {
        uint32_t key = lv_event_get_key(e);

        if (key == LV_KEY_ESC)
        {
            /* 返回首页 */
            ui_load_scr_animation(&guider_ui, &guider_ui.screen_user_home,
                                  guider_ui.screen_user_home_del,
                                  &guider_ui.screen_alert_del,
                                  setup_scr_screen_user_home,
                                  LV_SCR_LOAD_ANIM_NONE, 10, 10, true, true);
        }
        else if (key == LV_KEY_LEFT)
        {
            /* 上一页 */
            if (s_alert_page > 0)
            {
                s_alert_page--;
                alert_populate_current_page();
            }
            lv_event_stop_processing(e);
            lv_event_stop_bubbling(e);
        }
        else if (key == LV_KEY_RIGHT)
        {
            /* 下一页 */
            uint16_t total = alert_get_total_pages();
            if (s_alert_page + 1 < total)
            {
                s_alert_page++;
                alert_populate_current_page();
            }
            lv_event_stop_processing(e);
            lv_event_stop_bubbling(e);
        }
        else if (key == LV_KEY_ENTER)
        {
            /* 按下确认键 → 弹出确认对话框 */
            int dev_idx = ALERT_DECODE_IDX(lv_event_get_user_data(e));
            int atype   = ALERT_DECODE_TYPE(lv_event_get_user_data(e));

            /* 构建提示文本 */
            char msg_buf[80];
            house_info_t house;
            parse_addr(device_list[dev_idx].addr, &house);
            lv_snprintf(msg_buf, sizeof(msg_buf),
                        "是否手动清除故障?\n%s %d楼 %d单元 %d",
                        alert_type_name((alert_type_t)atype),
                        house.building, house.unit, house.room);

            /* 创建 msgbox 对话框 */
            static const char * btns[] = {"否 ", "是 ", ""};

            lv_obj_t *mbox = lv_msgbox_create(NULL, NULL, msg_buf, btns, false);
            lv_obj_set_style_bg_color(mbox, lv_color_hex(0x161B22), 0);
            lv_obj_set_style_bg_opa(mbox, LV_OPA_COVER, 0);
            lv_obj_set_style_radius(mbox, 10, 0);
            lv_obj_set_style_shadow_width(mbox, 20, 0);
            lv_obj_set_style_text_color(mbox, lv_color_hex(0xE6EDF3), 0);
            lv_obj_set_style_text_font(mbox, &lv_font_SourceHanSerifSC_Regular_16, 0);
            lv_obj_center(mbox);

            /* 将 dev_idx+alert_type 编码存到 msgbox 的 user_data */
            lv_obj_set_user_data(mbox, (void*)ALERT_ENCODE(dev_idx, atype));

            /* 获取按钮组 */
            lv_obj_t *btn_matrix = lv_msgbox_get_btns(mbox);
            if (btn_matrix)
            {
                /* 按钮样式 */
                static lv_style_t style_btn;
                ui_init_style(&style_btn);
                lv_style_set_radius(&style_btn, 6);
                lv_style_set_bg_color(&style_btn, lv_color_hex(0x21262D));
                lv_style_set_bg_opa(&style_btn, LV_OPA_COVER);
                lv_style_set_text_font(&style_btn, &lv_font_SourceHanSerifSC_Regular_16);
                lv_style_set_text_color(&style_btn, lv_color_hex(0xE6EDF3));
                lv_style_set_border_width(&style_btn, 1);
                lv_style_set_border_color(&style_btn, lv_color_hex(0x30363D));
                lv_style_set_border_opa(&style_btn, LV_OPA_COVER);
                lv_obj_add_style(btn_matrix, &style_btn, LV_PART_ITEMS | LV_STATE_DEFAULT);

                /* 聚焦样式 */
                static lv_style_t style_btn_focused;
                ui_init_style(&style_btn_focused);
                lv_style_set_bg_color(&style_btn_focused, lv_color_hex(0x58A6FF));
                lv_style_set_bg_opa(&style_btn_focused, LV_OPA_COVER);
                lv_style_set_text_color(&style_btn_focused, lv_color_hex(0xFFFFFF));
                lv_style_set_border_width(&style_btn_focused, 0);
                lv_obj_add_style(btn_matrix, &style_btn_focused, LV_PART_ITEMS | LV_STATE_FOCUSED);
            }

            /* 绑定按钮回调: 通过 btn_id 区分 "否"(0) / "是"(1) */
            lv_obj_add_event_cb(mbox, alert_dialog_btn_cb, LV_EVENT_PRESSED,
                                (void*)mbox);

            lv_group_remove_all_objs(g_keypad_group);//清空group中的所有组件
            /* 将消息框的按钮矩阵加入group以支持键盘操作 */
            lv_group_add_obj(g_keypad_group, btn_matrix);
            lv_group_focus_obj(btn_matrix);//设置初始焦点
            lv_indev_set_group(indev_keypad, g_keypad_group);
        }
    }
}
/**
 * @brief  清除所有故障对话框回调
 *         btn index 0="否", 1="是"
 */
static void alert_clear_all_dialog_btn_cb(lv_event_t *e)
{
    lv_obj_t *mbox = lv_event_get_user_data(e);
    uint16_t btn_id = lv_msgbox_get_active_btn(mbox);

    if (btn_id == 1)  /* "是" - 清除所有故障 */
    {
        /* 遍历所有设备，清除错误位 */
        for (uint16_t i = 0; i < device_count; i++)
        {
            if (device_list[i].state.bits.valid)
            {
                /* 已入网设备：保留valid位(bit0)，清除所有错误位，保留dc_heating位(bit1) */
                device_list[i].state.bits.comm_err      = 0;
                device_list[i].state.bits.relay_err     = 0;
                device_list[i].state.bits.temp_err      = 0;
                device_list[i].state.bits.power_reverse = 0;
                device_list[i].comm_fail_cnt = 0;
            }
            else
            {
                /* 未入网设备：state清零 */
                device_list[i].state.byte = 0x00;
            }
        }

        lv_msgbox_close(mbox);

        /* 重新扫描告警数据并刷新列表 */
        alert_scan_devices();
        alert_populate_list();
        Alert_Widget_Update();
    }
    else  /* "否" - 仅关闭对话框 */
    {
        lv_msgbox_close(mbox);
        alert_populate_list();
    }
}

/**
 * @brief  底部tip项事件回调（故障过多时的"清除所有"提示）
 */
static void alert_tip_event_handler(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);

    if (code == LV_EVENT_FOCUSED)
    {
        /* 焦点变化时，自动滚动父容器使tip可见 */
        lv_obj_t *item = lv_event_get_target(e);
        lv_obj_scroll_to_view(item, LV_ANIM_OFF);
    }
    else if (code == LV_EVENT_KEY)
    {
        uint32_t key = lv_event_get_key(e);

        if (key == LV_KEY_ESC)
        {
            /* 返回首页 */
            ui_load_scr_animation(&guider_ui, &guider_ui.screen_user_home,
                                  guider_ui.screen_user_home_del,
                                  &guider_ui.screen_alert_del,
                                  setup_scr_screen_user_home,
                                  LV_SCR_LOAD_ANIM_NONE, 10, 10, true, true);
        }
        else if (key == LV_KEY_ENTER)
        {
            /* 弹出确认对话框："是否手动清除所有故障?" */
            static const char * btns[] = {"否 ", "是 ", ""};

            lv_obj_t *mbox = lv_msgbox_create(NULL, NULL,
                              "是否手动清除所有故障?", btns, false);
            lv_obj_set_style_bg_color(mbox, lv_color_hex(0x161B22), 0);
            lv_obj_set_style_bg_opa(mbox, LV_OPA_COVER, 0);
            lv_obj_set_style_radius(mbox, 10, 0);
            lv_obj_set_style_shadow_width(mbox, 20, 0);
            lv_obj_set_style_text_color(mbox, lv_color_hex(0xE6EDF3), 0);
            lv_obj_set_style_text_font(mbox, &lv_font_SourceHanSerifSC_Regular_16, 0);
            lv_obj_center(mbox);

            /* 获取按钮组并设置样式 */
            lv_obj_t *btn_matrix = lv_msgbox_get_btns(mbox);
            if (btn_matrix)
            {
                /* 按钮样式 */
                static lv_style_t style_btn;
                ui_init_style(&style_btn);
                lv_style_set_radius(&style_btn, 6);
                lv_style_set_bg_color(&style_btn, lv_color_hex(0x21262D));
                lv_style_set_bg_opa(&style_btn, LV_OPA_COVER);
                lv_style_set_text_font(&style_btn, &lv_font_SourceHanSerifSC_Regular_16);
                lv_style_set_text_color(&style_btn, lv_color_hex(0xE6EDF3));
                lv_style_set_border_width(&style_btn, 1);
                lv_style_set_border_color(&style_btn, lv_color_hex(0x30363D));
                lv_style_set_border_opa(&style_btn, LV_OPA_COVER);
                lv_obj_add_style(btn_matrix, &style_btn, LV_PART_ITEMS | LV_STATE_DEFAULT);

                /* 聚焦样式 */
                static lv_style_t style_btn_focused;
                ui_init_style(&style_btn_focused);
                lv_style_set_bg_color(&style_btn_focused, lv_color_hex(0x58A6FF));
                lv_style_set_bg_opa(&style_btn_focused, LV_OPA_COVER);
                lv_style_set_text_color(&style_btn_focused, lv_color_hex(0xFFFFFF));
                lv_style_set_border_width(&style_btn_focused, 0);
                lv_obj_add_style(btn_matrix, &style_btn_focused, LV_PART_ITEMS | LV_STATE_FOCUSED);
            }

            /* 绑定按钮回调 */
            lv_obj_add_event_cb(mbox, alert_clear_all_dialog_btn_cb, LV_EVENT_PRESSED,
                                (void*)mbox);

            /* 切换焦点到对话框按钮 */
            lv_group_remove_all_objs(g_keypad_group);
            lv_group_add_obj(g_keypad_group, btn_matrix);
            lv_group_focus_obj(btn_matrix);
            lv_indev_set_group(indev_keypad, g_keypad_group);
        }
    }
}

//告警页面事件初始化函数
static void screen_alert_event_handler (lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    switch (code) {
    case LV_EVENT_KEY:
    {
        uint32_t key = lv_event_get_key(e);
        if(key == LV_KEY_ESC)
        {
            /* 返回首页 */
            ui_load_scr_animation(&guider_ui, &guider_ui.screen_user_home, guider_ui.screen_user_home_del, &guider_ui.screen_alert_del, setup_scr_screen_user_home, LV_SCR_LOAD_ANIM_NONE, 10, 10, true, true);
        }
        break;
    }
    case LV_EVENT_SCREEN_LOADED:
    {
        g_need_key_remap = 0;  /* 告警页面不需要重映射 */
        /* 注册tip事件回调（屏幕重建后旧对象已销毁，需要重新注册） */
        lv_obj_add_event_cb(guider_ui.screen_alert_tip, alert_tip_event_handler, LV_EVENT_ALL, NULL);
        alert_populate_list();
        break;
    }
    default:
        break;
    }
}

void events_init_screen_alert (lv_ui *ui)
{
    lv_obj_add_event_cb(ui->screen_alert, screen_alert_event_handler, LV_EVENT_ALL, ui);
}

/* ======== TCP 设置 页事件处理 (使用LVGL内置lv_keyboard) ======== */

#define TCP_IP_MAX_LEN  15
#define TCP_PORT_MAX_LEN 5

/**
 * @brief  当前活跃的输入框 (0=IP, 1=Port)
 */
static uint8_t s_tcp_active_field = 0;
static uint8_t s_setting_mode = 0;

/**
 * @brief  切换键盘关联的textarea并更新光标显示
 */
static void tcp_switch_textarea(void)
{
    if (s_setting_mode == 1) return;
    s_tcp_active_field ^= 1;  /* 0↔1 切换 */
    if (s_tcp_active_field == 0) {
        /* 切换到IP */
        lv_keyboard_set_textarea(guider_ui.screen_tcp_setting_kb,
                                 guider_ui.screen_tcp_setting_ta_ip);
        lv_obj_add_state(guider_ui.screen_tcp_setting_ta_ip, LV_STATE_FOCUSED);
        lv_obj_clear_state(guider_ui.screen_tcp_setting_ta_port, LV_STATE_FOCUSED);
        lv_textarea_set_cursor_click_pos(guider_ui.screen_tcp_setting_ta_ip, true);
    } else {
        /* 切换到Port */
        lv_keyboard_set_textarea(guider_ui.screen_tcp_setting_kb,
                                 guider_ui.screen_tcp_setting_ta_port);
        lv_obj_add_state(guider_ui.screen_tcp_setting_ta_port, LV_STATE_FOCUSED);
        lv_obj_clear_state(guider_ui.screen_tcp_setting_ta_ip, LV_STATE_FOCUSED);
        lv_textarea_set_cursor_click_pos(guider_ui.screen_tcp_setting_ta_port, true);
    }
}

/**
 * @brief  保存IP/Port并返回首页
 */
static void tcp_save(void)
{
    const char *ip_str = lv_textarea_get_text(guider_ui.screen_tcp_setting_ta_ip);
    if (s_setting_mode == 1) {
        uint8_t ip[4]={0},oc=0,v=0,hv=0;
        for(uint16_t i=0;i<=strlen(ip_str)&&oc<4;i++){
            if(ip_str[i]>='0'&&ip_str[i]<='9'){v=v*10+(ip_str[i]-'0');hv=1;}
            else{if(hv){ip[oc++]=v;v=0;hv=0;}}
        }
        if(oc<4)return;
        tcp_set_gateway_addr(ip);
        gateway_config_save();
        lv_snprintf(gateway_buf,sizeof(gateway_buf),"%d.%d.%d.%d ",ip[0],ip[1],ip[2],ip[3]);
        ui_load_scr_animation(&guider_ui,&guider_ui.screen_sys_setting,guider_ui.screen_sys_setting_del,&guider_ui.screen_tcp_setting_del,setup_scr_screen_sys_setting,LV_SCR_LOAD_ANIM_NONE,10,10,true,true);
        return;
    }
    const char *port_str = lv_textarea_get_text(guider_ui.screen_tcp_setting_ta_port);

    /* 解析IP */
    uint8_t ip[4] = {0};
    uint16_t port = 0;
    uint8_t octet = 0, val = 0, has_val = 0;
    for (uint16_t i = 0; i <= strlen(ip_str) && octet < 4; i++) {
        if (ip_str[i] >= '0' && ip_str[i] <= '9') {
            val = val * 10 + (ip_str[i] - '0');
            has_val = 1;
        } else {
            if (has_val) {
                ip[octet++] = val;
                val = 0;
                has_val = 0;
            }
        }
    }

    /* 解析Port */
    for (uint16_t i = 0; i < strlen(port_str); i++) {
        if (port_str[i] >= '0' && port_str[i] <= '9') {
            port = port * 10 + (port_str[i] - '0');
        }
    }

    /* 基本校验 */
    if (octet < 4 || port == 0) {
        printf("TCP设置无效: IP段=%d, Port=%d\r\n", octet, port);
        return;
    }

    printf("TCP设置: %d.%d.%d.%d:%d\r\n", ip[0], ip[1], ip[2], ip[3], port);
    tcp_set_server_addr(ip, port);

    /* 保存到TF卡 */
    tcp_config_save();

    /* 更新首页显示 */
    lv_snprintf(server_ip_buf, sizeof(server_ip_buf), "%d.%d.%d.%d ", ip[0], ip[1], ip[2], ip[3]);
    lv_snprintf(server_port_buf, sizeof(server_port_buf), "%d ", port);

    ui_load_scr_animation(&guider_ui, &guider_ui.screen_user_home,
                            guider_ui.screen_user_home_del,
                            &guider_ui.screen_tcp_setting_del,
                            setup_scr_screen_user_home,
                            LV_SCR_LOAD_ANIM_NONE, 10, 10, true, true);
}

/**
 * @brief  ESC键处理: 物理ESC键按下时返回首页
 */
static void tcp_kb_esc_handler(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if(code != LV_EVENT_KEY) return;
    uint32_t key = lv_event_get_key(e);
    if(key == LV_KEY_ESC) {
        ui_load_scr_animation(&guider_ui, &guider_ui.screen_user_home,
                              guider_ui.screen_user_home_del,
                              &guider_ui.screen_tcp_setting_del,
                              setup_scr_screen_user_home,
                              LV_SCR_LOAD_ANIM_NONE, 10, 10, true, true);
    }
}

/**
 * @brief  自定义键盘事件处理 (替换默认的 lv_keyboard_def_event_cb)
 *         在默认处理之前拦截 LV_SYMBOL_SAVE 和 LV_SYMBOL_OK
 *         其他按键交给默认处理器处理
 */
static void tcp_kb_custom_handler(lv_event_t *e)
{
    lv_obj_t *kb = lv_event_get_target(e);
    uint16_t btn_id = lv_btnmatrix_get_selected_btn(kb);
    if(btn_id == LV_BTNMATRIX_BTN_NONE) return;

    const char *txt = lv_btnmatrix_get_btn_text(kb, btn_id);
    if(txt == NULL) return;

    /* 拦截 LV_SYMBOL_SAVE → 保存IP/Port并返回首页 */
    if(strcmp(txt, LV_SYMBOL_SAVE) == 0) {
        tcp_save();
        return;  /* 不调用默认处理器, 避免插入文本 */
    }

    /* 拦截 LV_SYMBOL_OK → 切换IP/Port输入焦点 */
    if(strcmp(txt, LV_SYMBOL_OK) == 0) {
        tcp_switch_textarea();
        return;  /* 不调用默认处理器, 避免发送LV_EVENT_READY */
    }

    /* 其他按键: 交给默认处理器处理 (数字、小数点、退格、方向键等) */
    lv_keyboard_def_event_cb(e);
}

static void screen_tcp_setting_event_handler(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    switch (code) {
    case LV_EVENT_SCREEN_LOADED:
    {
        g_need_key_remap = 1;  /* TCP设置页面需要NEXT/PREV重映射 */
        /* 从当前 server_ip/server_port 读取并填充输入框 */
        uint8_t ip[4];
        uint16_t port;
        tcp_get_server_addr(ip, &port);

        char ip_str[TCP_IP_MAX_LEN + 1];
        char port_str[TCP_PORT_MAX_LEN + 1];
        lv_snprintf(ip_str, sizeof(ip_str), "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
        lv_snprintf(port_str, sizeof(port_str), "%d", port);

        /* 获取Port卡片(输入框的父容器), 网关模式下隐藏 */
        lv_obj_t *port_card = lv_obj_get_parent(guider_ui.screen_tcp_setting_ta_port);
        if(s_setting_mode==1){
            uint8_t gw[4]; tcp_get_gateway_addr(gw);
            lv_snprintf(ip_str,sizeof(ip_str),"%d.%d.%d.%d",gw[0],gw[1],gw[2],gw[3]);
            lv_textarea_set_text(guider_ui.screen_tcp_setting_ta_ip,ip_str);
            lv_textarea_set_text(guider_ui.screen_tcp_setting_ta_port,"");
            lv_obj_add_flag(port_card, LV_OBJ_FLAG_HIDDEN);   /* 网关模式: 隐藏Port卡片 */
        }else{
            lv_textarea_set_text(guider_ui.screen_tcp_setting_ta_ip, ip_str);
            lv_textarea_set_text(guider_ui.screen_tcp_setting_ta_port, port_str);
            lv_obj_clear_flag(port_card, LV_OBJ_FLAG_HIDDEN); /* TCP模式: 显示Port卡片 */
        }

        /* 只将键盘加入group, textarea不加入 (由键盘直接控制输入) */
        lv_group_remove_all_objs(g_keypad_group);
        lv_group_add_obj(g_keypad_group, guider_ui.screen_tcp_setting_kb);
        lv_indev_set_group(indev_keypad, g_keypad_group);

        /* 初始关联到IP输入框, 显示光标 */
        s_tcp_active_field = 0;
        lv_keyboard_set_textarea(guider_ui.screen_tcp_setting_kb,
                                 guider_ui.screen_tcp_setting_ta_ip);
        lv_obj_add_state(guider_ui.screen_tcp_setting_ta_ip, LV_STATE_FOCUSED);
        lv_obj_clear_state(guider_ui.screen_tcp_setting_ta_port, LV_STATE_FOCUSED);
        lv_textarea_set_cursor_click_pos(guider_ui.screen_tcp_setting_ta_ip, true);

        /* 移除默认键盘事件处理器, 替换为自定义处理器 */
        lv_obj_remove_event_cb(guider_ui.screen_tcp_setting_kb,
                               lv_keyboard_def_event_cb);
        /* 为数字按键添加对应的自定义事件 */
        lv_obj_add_event_cb(guider_ui.screen_tcp_setting_kb,
                            tcp_kb_custom_handler, LV_EVENT_VALUE_CHANGED, NULL);

        /* ESC键返回首页 */
        lv_obj_add_event_cb(guider_ui.screen_tcp_setting_kb,
                            tcp_kb_esc_handler, LV_EVENT_KEY, NULL);

        break;
    }
    default:
        break;
    }
}

void events_init_screen_tcp_setting(lv_ui *ui)
{
    lv_obj_add_event_cb(ui->screen_tcp_setting, screen_tcp_setting_event_handler, LV_EVENT_ALL, ui);
}

/* ======== Sys Setting 系统设置页事件处理 ======== */

static void screen_sys_setting_event_handler(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    switch (code) {
    case LV_EVENT_KEY:
    {
        uint32_t key = lv_event_get_key(e);
        if (key == LV_KEY_ESC)
        {
            /* 返回首页 */
            ui_load_scr_animation(&guider_ui, &guider_ui.screen_user_home,
                                  guider_ui.screen_user_home_del,
                                  &guider_ui.screen_sys_setting_del,
                                  setup_scr_screen_user_home,
                                  LV_SCR_LOAD_ANIM_NONE, 10, 10, true, true);
        }
        break;
    }
    case LV_EVENT_SCREEN_LOADED:
    {
        char buf[40];

        g_need_key_remap = 0;  /* 系统设置页面不需要重映射 */
        lv_group_remove_all_objs(g_keypad_group);
        /* 将3张卡片加入group, 支持上下键导航 */
        lv_group_add_obj(g_keypad_group, guider_ui.screen_sys_setting_card_mac);
        lv_group_add_obj(g_keypad_group, guider_ui.screen_sys_setting_card_ip);
        lv_group_add_obj(g_keypad_group, guider_ui.screen_sys_setting_card_building);
        lv_group_add_obj(g_keypad_group, guider_ui.screen_sys_setting_card_gw);
        lv_group_add_obj(g_keypad_group, guider_ui.screen_sys_setting_card_tcp);
        lv_indev_set_group(indev_keypad, g_keypad_group);
        lv_group_focus_obj(guider_ui.screen_sys_setting_card_mac);

        /* ======== 更新系统信息到UI ======== */

        /* 主机MAC地址 */
        lv_snprintf(buf, sizeof(buf), "%02X:%02X:%02X:%02X:%02X:%02X",
                    g_host_mac[0], g_host_mac[1], g_host_mac[2],
                    g_host_mac[3], g_host_mac[4], g_host_mac[5]);
        lv_label_set_text(guider_ui.screen_sys_setting_label_mac, buf);

        /* 楼栋号 */
        lv_snprintf(buf, sizeof(buf), "%d号楼", tcp_get_building_no());
        lv_label_set_text(guider_ui.screen_sys_setting_label_building, buf);

        /* 本机IP地址 */
        lv_label_set_text(guider_ui.screen_sys_setting_label_ip, client_ip_buf);
        lv_label_set_text(guider_ui.screen_sys_setting_label_gw, gateway_buf);

        /* TCP服务器 IP:端口 */
        lv_snprintf(buf, sizeof(buf), "%s:%s", server_ip_buf, server_port_buf);
        lv_label_set_text(guider_ui.screen_sys_setting_label_tcp, buf);
        break;
    }
    default:
        break;
    }
}

/* 系统设置页: TCP卡片点击(mode=0) */
static void screen_sys_setting_card_tcp_event_handler(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) {
        s_setting_mode = 0;
        ui_load_scr_animation(&guider_ui, &guider_ui.screen_tcp_setting,
                              guider_ui.screen_tcp_setting_del,
                              &guider_ui.screen_sys_setting_del,
                              setup_scr_screen_tcp_setting,
                              LV_SCR_LOAD_ANIM_NONE, 10, 10, false, false);
    }
}

static void screen_sys_setting_card_gw_event_handler(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) {
        s_setting_mode = 1;
        ui_load_scr_animation(&guider_ui, &guider_ui.screen_tcp_setting,
                              guider_ui.screen_tcp_setting_del,
                              &guider_ui.screen_sys_setting_del,
                              setup_scr_screen_tcp_setting,
                              LV_SCR_LOAD_ANIM_NONE, 10, 10, false, false);
    }
}

static void screen_sys_setting_card_key_handler(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_KEY) {
        uint32_t key = lv_event_get_key(e);
        if (key == LV_KEY_ESC) {
            ui_load_scr_animation(&guider_ui, &guider_ui.screen_user_home,
                                  guider_ui.screen_user_home_del,
                                  &guider_ui.screen_sys_setting_del,
                                  setup_scr_screen_user_home,
                                  LV_SCR_LOAD_ANIM_NONE, 10, 10, true, true);
        }
    }
}

void events_init_screen_sys_setting(lv_ui *ui)
{
    lv_obj_add_event_cb(ui->screen_sys_setting, screen_sys_setting_event_handler, LV_EVENT_ALL, ui);
    lv_obj_add_event_cb(ui->screen_sys_setting_card_tcp, screen_sys_setting_card_tcp_event_handler, LV_EVENT_ALL, ui);
    lv_obj_add_event_cb(ui->screen_sys_setting_card_gw, screen_sys_setting_card_gw_event_handler, LV_EVENT_ALL, ui);
    lv_obj_add_event_cb(ui->screen_sys_setting_card_gw, screen_sys_setting_card_key_handler, LV_EVENT_KEY, NULL);
    lv_obj_add_event_cb(ui->screen_sys_setting_card_mac, screen_sys_setting_card_key_handler, LV_EVENT_KEY, NULL);
    lv_obj_add_event_cb(ui->screen_sys_setting_card_building, screen_sys_setting_card_key_handler, LV_EVENT_KEY, NULL);
    lv_obj_add_event_cb(ui->screen_sys_setting_card_ip, screen_sys_setting_card_key_handler, LV_EVENT_KEY, NULL);
    lv_obj_add_event_cb(ui->screen_sys_setting_card_tcp, screen_sys_setting_card_key_handler, LV_EVENT_KEY, NULL);
}

void events_init(lv_ui *ui)
{

}
