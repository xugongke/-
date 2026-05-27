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
#include "lvgl.h"

#if LV_USE_GUIDER_SIMULATOR && LV_USE_FREERTOS
#include "freemaster_client.h"
#endif

#include "wiz_interface.h"
#include "user_data_manager.h"
#include "key.h"
#include "device_manager.h"
extern lv_indev_t * indev_keypad;
lv_group_t * g_keypad_group;//创建全局group(可被焦点选中的对象集合)指针，在lv_init后分配空间

/* 记录home页面最后聚焦的卡片 (0=太阳能, 1=设备在线, 2=告警) */
static uint8_t s_home_focus_card = 1;  /* 默认设备在线卡片 */

/* ======== Home 页事件处理 ======== */

static void screen_user_home_event_handler (lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    switch (code) {
    case LV_EVENT_SCREEN_LOADED:
    {
        lv_group_remove_all_objs(g_keypad_group);//清空group中的所有组件
        //给group添加3个数据卡片
        lv_group_add_obj(g_keypad_group, guider_ui.screen_user_home_card_solar);
        lv_group_add_obj(g_keypad_group, guider_ui.screen_user_home_card_device);
        lv_group_add_obj(g_keypad_group, guider_ui.screen_user_home_card_alert);
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
        lv_label_set_text(guider_ui.screen_user_home_label_ip, ip_buf);
        lv_label_set_text(guider_ui.screen_user_home_label_port, port_buf);
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
        lv_obj_set_style_border_color(guider_ui.screen_user_home_card_solar, lv_color_hex(0x2196F3), 0);
        lv_obj_set_style_border_opa(guider_ui.screen_user_home_card_solar, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(guider_ui.screen_user_home_card_solar, 3, 0);
        lv_obj_set_style_shadow_width(guider_ui.screen_user_home_card_solar, 20, 0);
        lv_obj_set_style_shadow_opa(guider_ui.screen_user_home_card_solar, 100, 0);
        lv_obj_set_style_shadow_ofs_y(guider_ui.screen_user_home_card_solar, 6, 0);
        /* 整体放大: 卡片 + 内部白色body一起变大 */
        lv_obj_set_size(guider_ui.screen_user_home_card_solar, 150, 108);
        lv_obj_set_pos(guider_ui.screen_user_home_card_solar, 15, 151);
        lv_obj_t *_body = lv_obj_get_child(guider_ui.screen_user_home_card_solar, 2);
        if(_body) { lv_obj_set_size(_body, 146, 66); lv_obj_set_pos(_body, 2, 38); }
    }
    else if (code == LV_EVENT_DEFOCUSED) {
        lv_obj_set_style_border_width(guider_ui.screen_user_home_card_solar, 0, 0);
        lv_obj_set_style_shadow_width(guider_ui.screen_user_home_card_solar, 12, 0);
        lv_obj_set_style_shadow_opa(guider_ui.screen_user_home_card_solar, 50, 0);
        lv_obj_set_style_shadow_ofs_y(guider_ui.screen_user_home_card_solar, 4, 0);
        /* 恢复原始大小 */
        lv_obj_set_size(guider_ui.screen_user_home_card_solar, 140, 100);
        lv_obj_set_pos(guider_ui.screen_user_home_card_solar, 20, 155);
        lv_obj_t *_body = lv_obj_get_child(guider_ui.screen_user_home_card_solar, 2);
        if(_body) { lv_obj_set_size(_body, 136, 62); lv_obj_set_pos(_body, 2, 36); }
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
        lv_obj_set_style_border_color(guider_ui.screen_user_home_card_device, lv_color_hex(0x4CAF50), 0);
        lv_obj_set_style_border_opa(guider_ui.screen_user_home_card_device, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(guider_ui.screen_user_home_card_device, 3, 0);
        lv_obj_set_style_shadow_width(guider_ui.screen_user_home_card_device, 20, 0);
        lv_obj_set_style_shadow_opa(guider_ui.screen_user_home_card_device, 100, 0);
        lv_obj_set_style_shadow_ofs_y(guider_ui.screen_user_home_card_device, 6, 0);
        /* 整体放大 */
        lv_obj_set_size(guider_ui.screen_user_home_card_device, 150, 108);
        lv_obj_set_pos(guider_ui.screen_user_home_card_device, 165, 151);
        lv_obj_t *_body = lv_obj_get_child(guider_ui.screen_user_home_card_device, 2);
        if(_body) { lv_obj_set_size(_body, 146, 66); lv_obj_set_pos(_body, 2, 38); }
    }
    else if (code == LV_EVENT_DEFOCUSED) {
        lv_obj_set_style_border_width(guider_ui.screen_user_home_card_device, 0, 0);
        lv_obj_set_style_shadow_width(guider_ui.screen_user_home_card_device, 12, 0);
        lv_obj_set_style_shadow_opa(guider_ui.screen_user_home_card_device, 50, 0);
        lv_obj_set_style_shadow_ofs_y(guider_ui.screen_user_home_card_device, 4, 0);
        /* 恢复原始大小 */
        lv_obj_set_size(guider_ui.screen_user_home_card_device, 140, 100);
        lv_obj_set_pos(guider_ui.screen_user_home_card_device, 170, 155);
        lv_obj_t *_body = lv_obj_get_child(guider_ui.screen_user_home_card_device, 2);
        if(_body) { lv_obj_set_size(_body, 136, 62); lv_obj_set_pos(_body, 2, 36); }
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
        lv_obj_set_style_border_color(guider_ui.screen_user_home_card_alert, lv_color_hex(0xFF9800), 0);
        lv_obj_set_style_border_opa(guider_ui.screen_user_home_card_alert, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(guider_ui.screen_user_home_card_alert, 3, 0);
        lv_obj_set_style_shadow_width(guider_ui.screen_user_home_card_alert, 20, 0);
        lv_obj_set_style_shadow_opa(guider_ui.screen_user_home_card_alert, 100, 0);
        lv_obj_set_style_shadow_ofs_y(guider_ui.screen_user_home_card_alert, 6, 0);
        /* 整体放大 */
        lv_obj_set_size(guider_ui.screen_user_home_card_alert, 150, 108);
        lv_obj_set_pos(guider_ui.screen_user_home_card_alert, 315, 151);
        lv_obj_t *_body = lv_obj_get_child(guider_ui.screen_user_home_card_alert, 2);
        if(_body) { lv_obj_set_size(_body, 146, 66); lv_obj_set_pos(_body, 2, 38); }
    }
    else if (code == LV_EVENT_DEFOCUSED) {
        lv_obj_set_style_border_width(guider_ui.screen_user_home_card_alert, 0, 0);
        lv_obj_set_style_shadow_width(guider_ui.screen_user_home_card_alert, 12, 0);
        lv_obj_set_style_shadow_opa(guider_ui.screen_user_home_card_alert, 50, 0);
        lv_obj_set_style_shadow_ofs_y(guider_ui.screen_user_home_card_alert, 4, 0);
        /* 恢复原始大小 */
        lv_obj_set_size(guider_ui.screen_user_home_card_alert, 140, 100);
        lv_obj_set_pos(guider_ui.screen_user_home_card_alert, 320, 155);
        lv_obj_t *_body = lv_obj_get_child(guider_ui.screen_user_home_card_alert, 2);
        if(_body) { lv_obj_set_size(_body, 136, 62); lv_obj_set_pos(_body, 2, 36); }
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
        }

        lv_obj_t *btn = lv_list_add_btn(guider_ui.screen_user_list_list_1, LV_SYMBOL_HOME, txt);

        /* ======== 默认样式 ======== */
        static lv_style_t style_list_btn;
        ui_init_style(&style_list_btn);
        lv_style_set_pad_top(&style_list_btn, 10);
        lv_style_set_pad_left(&style_list_btn, 14);
        lv_style_set_pad_right(&style_list_btn, 14);
        lv_style_set_pad_bottom(&style_list_btn, 10);
        lv_style_set_border_width(&style_list_btn, 0);
        lv_style_set_text_color(&style_list_btn, lv_color_hex(0x2C3E50));
        lv_style_set_text_font(&style_list_btn, &lv_font_SourceHanSerifSC_Regular_16);
        lv_style_set_text_opa(&style_list_btn, 255);
        lv_style_set_radius(&style_list_btn, 10);
        lv_style_set_bg_opa(&style_list_btn, LV_OPA_COVER);
        lv_style_set_bg_color(&style_list_btn, lv_color_hex(0xffffff));
        lv_style_set_bg_grad_dir(&style_list_btn, LV_GRAD_DIR_NONE);
        lv_style_set_shadow_width(&style_list_btn, 2);
        lv_style_set_shadow_color(&style_list_btn, lv_color_hex(0x000000));
        lv_style_set_shadow_opa(&style_list_btn, 15);
        lv_style_set_shadow_ofs_y(&style_list_btn, 2);
        lv_style_set_shadow_spread(&style_list_btn, 0);
        lv_obj_add_style(btn, &style_list_btn, LV_PART_MAIN|LV_STATE_DEFAULT);

        /* ======== 聚焦样式 (蓝色左边框) ======== */
        static lv_style_t style_list_btn_focused;
        ui_init_style(&style_list_btn_focused);
        lv_style_set_border_width(&style_list_btn_focused, 4);
        lv_style_set_border_color(&style_list_btn_focused, lv_color_hex(0x2196F3));
        lv_style_set_border_opa(&style_list_btn_focused, LV_OPA_COVER);
        lv_style_set_border_side(&style_list_btn_focused, LV_BORDER_SIDE_LEFT);
        lv_style_set_shadow_width(&style_list_btn_focused, 8);
        lv_style_set_shadow_color(&style_list_btn_focused, lv_color_hex(0x2196F3));
        lv_style_set_shadow_opa(&style_list_btn_focused, 30);
        lv_style_set_shadow_ofs_y(&style_list_btn_focused, 3);
        lv_style_set_bg_color(&style_list_btn_focused, lv_color_hex(0xffffff));
        lv_style_set_bg_opa(&style_list_btn_focused, LV_OPA_COVER);
        lv_style_set_radius(&style_list_btn_focused, 10);
        lv_obj_add_style(btn, &style_list_btn_focused, LV_PART_MAIN|LV_STATE_FOCUSED);

        lv_group_add_obj(g_keypad_group, btn);
        lv_obj_add_event_cb(btn, user_list_item_event_handler, LV_EVENT_CLICKED, (void*)(uintptr_t)i);
        lv_obj_add_event_cb(btn, screen_user_list_item_event_handler, LV_EVENT_KEY, (void*)(uintptr_t)i);

        /* 如果这是上次选中的编号，就让它获得焦点 */
        if(i == s_last_user_no) {
            lv_group_focus_obj(btn);
        }
    }

    /* 如果没有匹配到上次焦点的项，聚焦第一个 */
    lv_indev_set_group(indev_keypad, g_keypad_group);
    if(lv_group_get_focused(g_keypad_group) == NULL && end > start) {
        /* 焦点第一个btn */
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
            lv_obj_set_style_text_color(s_list_page_label, lv_color_hex(0x888888), 0);
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
        lv_group_remove_all_objs(g_keypad_group);
        lv_group_add_obj(g_keypad_group, guider_ui.screen_solar);
        lv_indev_set_group(indev_keypad, g_keypad_group);
				//设置初始焦点
        lv_group_focus_obj(guider_ui.screen_solar);
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

/* 告警类型 (与 device_state_t 错误位对应) */
typedef enum {
    ALERT_COMM_FAIL = 0,    /* 通信故障 (bit2) */
    ALERT_RELAY_ERR,        /* 继电器/开关异常 (bit5) */
    ALERT_TEMP_ERR,         /* 温度异常 (bit6) */
    ALERT_POWER_REVERSE,    /* 电源反接 (bit7) */
} alert_type_t;

#define ALERT_MAX_ITEMS 32  /* 告警列表最大项数 */

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
    lv_obj_set_size(item, 448, 26);
    lv_obj_set_style_radius(item, 4, 0);
    lv_obj_set_style_bg_color(item, lv_color_hex(0xFAFAFA), 0);
    lv_obj_set_style_bg_opa(item, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(item, 0, 0);
    lv_obj_set_style_pad_all(item, 0, 0);
    lv_obj_clear_flag(item, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(item, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_CHECKABLE);

    /* 聚焦样式 */
    static lv_style_t style_alert_focused;
    ui_init_style(&style_alert_focused);
    lv_style_set_border_width(&style_alert_focused, 3);
    lv_style_set_border_color(&style_alert_focused, lv_color_hex(0x2196F3));
    lv_style_set_border_opa(&style_alert_focused, LV_OPA_COVER);
    lv_style_set_border_side(&style_alert_focused, LV_BORDER_SIDE_LEFT);
    lv_style_set_bg_color(&style_alert_focused, lv_color_hex(0xE3F2FD));
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
    lv_obj_set_style_text_font(icon, &lv_font_montserratMedium_16, 0);
    lv_obj_set_style_bg_opa(icon, 0, 0);
    lv_obj_set_pos(icon, 12, 4);

    /* 类型名称 */
    lv_obj_t *type_lbl = lv_label_create(item);
    lv_label_set_text(type_lbl, alert_type_name(type));
    lv_obj_set_style_text_color(type_lbl, lv_color_hex(0x2C3E50), 0);
    lv_obj_set_style_text_font(type_lbl, &lv_font_SourceHanSerifSC_Regular_12, 0);
    lv_obj_set_style_bg_opa(type_lbl, 0, 0);
    lv_obj_set_pos(type_lbl, 32, 6);

    /* 地址 */
    lv_obj_t *addr_lbl = lv_label_create(item);
    lv_label_set_text(addr_lbl, addr_txt);
    lv_obj_set_style_text_color(addr_lbl, lv_color_hex(0x607D8B), 0);
    lv_obj_set_style_text_font(addr_lbl, &lv_font_SourceHanSerifSC_Regular_12, 0);
    lv_obj_set_style_bg_opa(addr_lbl, 0, 0);
    lv_obj_set_pos(addr_lbl, 120, 6);

    /* 存储设备下标到 user_data，供后续操作使用 */
    lv_obj_set_user_data(item, (void*)(uintptr_t)dev_idx);

    return item;
}

/**
 * @brief  遍历device_list，统计错误并填充告警列表
 */
static void alert_populate_list(void)
{
    lv_obj_clean(guider_ui.screen_alert_list);
    lv_group_remove_all_objs(g_keypad_group);

    int comm_cnt = 0, relay_cnt = 0, temp_cnt = 0, power_cnt = 0;
    int total = 0;

    for (uint16_t i = 0; i < device_count && total < ALERT_MAX_ITEMS; i++)
    {
        /* 只显示已入网设备 */
        if (device_list[i].state.bits.valid == 0) continue;

        /* 生成地址文本 */
        house_info_t house;
        parse_addr(device_list[i].addr, &house);
        char addr_txt[32];
        lv_snprintf(addr_txt, sizeof(addr_txt), "%d楼 %d单元 %d",
                    house.building, house.unit, house.room);

        /* 通信异常 (bit2) */
        if (device_list[i].state.bits.comm_err)
        {
            comm_cnt++;
            if (total < ALERT_MAX_ITEMS)
            {
                lv_obj_t *item = alert_create_item(guider_ui.screen_alert_list, i,
                                                    ALERT_COMM_FAIL, addr_txt);
                lv_group_add_obj(g_keypad_group, item);
                total++;
            }
        }

        /* 继电器异常 (bit5) */
        if (device_list[i].state.bits.relay_err)
        {
            relay_cnt++;
            if (total < ALERT_MAX_ITEMS)
            {
                lv_obj_t *item = alert_create_item(guider_ui.screen_alert_list, i,
                                                    ALERT_RELAY_ERR, addr_txt);
                lv_group_add_obj(g_keypad_group, item);
                total++;
            }
        }

        /* 温度异常 (bit6) */
        if (device_list[i].state.bits.temp_err)
        {
            temp_cnt++;
            if (total < ALERT_MAX_ITEMS)
            {
                lv_obj_t *item = alert_create_item(guider_ui.screen_alert_list, i,
                                                    ALERT_TEMP_ERR, addr_txt);
                lv_group_add_obj(g_keypad_group, item);
                total++;
            }
        }

        /* 电源反接 (bit7) */
        if (device_list[i].state.bits.power_reverse)
        {
            power_cnt++;
            if (total < ALERT_MAX_ITEMS)
            {
                lv_obj_t *item = alert_create_item(guider_ui.screen_alert_list, i,
                                                    ALERT_POWER_REVERSE, addr_txt);
                lv_group_add_obj(g_keypad_group, item);
                total++;
            }
        }
    }

    /* 更新统计栏标签 */
    char buf[32];
    lv_snprintf(buf, sizeof(buf), LV_SYMBOL_CLOSE " 通信异常: %d", comm_cnt);
    lv_label_set_text(guider_ui.screen_alert_stat_comm, buf);
    lv_snprintf(buf, sizeof(buf), LV_SYMBOL_BELL " 开关异常: %d", relay_cnt);
    lv_label_set_text(guider_ui.screen_alert_stat_relay, buf);
    lv_snprintf(buf, sizeof(buf), LV_SYMBOL_WARNING " 温度异常: %d", temp_cnt);
    lv_label_set_text(guider_ui.screen_alert_stat_temp, buf);
    lv_snprintf(buf, sizeof(buf), LV_SYMBOL_WARNING " 电源反接: %d", power_cnt);
    lv_label_set_text(guider_ui.screen_alert_stat_power, buf);

    /* 更新总数角标 */
    lv_snprintf(buf, sizeof(buf), "%d", total);
    lv_label_set_text(guider_ui.screen_alert_label_count, buf);

    lv_indev_set_group(indev_keypad, g_keypad_group);

    /* 聚焦第一个告警项 */
    if (total > 0)
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

void events_init(lv_ui *ui)
{

}
