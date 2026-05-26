/*
* Copyright 2026 NXP
* NXP Confidential - DO NOT SHARE
* Alert page - real-time alert list
*/

#include "lvgl.h"
#include <stdio.h>
#include "gui_guider.h"
#include "events_init.h"
#include "widgets_init.h"
#include "custom.h"

/* Maximum alert items displayed */
#define ALERT_MAX_ITEMS 8

/* Alert types */
typedef enum {
    ALERT_TYPE_OFFLINE = 0,     /* 设备离线 */
    ALERT_TYPE_OVERPOWER,       /* 用电异常 */
    ALERT_TYPE_COMM_FAIL,       /* 通信故障 */
    ALERT_TYPE_OVERTEMP,        /* 温度异常 */
} alert_type_t;

/* Fake alert data for demo */
typedef struct {
    alert_type_t type;
    const char *addr;       /* "X楼 X单元 XXXX" */
    const char *time;       /* "MM-DD HH:MM" */
} alert_item_t;

static const alert_item_t demo_alerts[] = {
    { ALERT_TYPE_OFFLINE,    "3楼 2单元 0501", "05-25 08:30" },
    { ALERT_TYPE_OVERPOWER,  "1楼 1单元 0102", "05-25 07:15" },
    { ALERT_TYPE_COMM_FAIL,  "5楼 3单元 1204", "05-24 22:45" },
    { ALERT_TYPE_OVERTEMP,   "2楼 1单元 0303", "05-24 18:20" },
    { ALERT_TYPE_OFFLINE,    "4楼 2单元 0806", "05-24 15:10" },
    { ALERT_TYPE_OVERPOWER,  "1楼 3单元 0601", "05-24 12:30" },
    { ALERT_TYPE_COMM_FAIL,  "3楼 1单元 0402", "05-23 09:55" },
    { ALERT_TYPE_OFFLINE,    "2楼 2单元 0705", "05-23 06:00" },
};

#define ALERT_COUNT (sizeof(demo_alerts) / sizeof(demo_alerts[0]))

static const char * alert_type_name(alert_type_t t)
{
    switch(t) {
        case ALERT_TYPE_OFFLINE:    return "设备离线";
        case ALERT_TYPE_OVERPOWER:  return "用电异常";
        case ALERT_TYPE_COMM_FAIL:  return "通信故障";
        case ALERT_TYPE_OVERTEMP:   return "温度异常";
        default:                    return "未知";
    }
}

static lv_color_t alert_type_color(alert_type_t t)
{
    switch(t) {
        case ALERT_TYPE_OFFLINE:    return lv_color_hex(0xF44336); /* red */
        case ALERT_TYPE_OVERPOWER:  return lv_color_hex(0xFF9800); /* orange */
        case ALERT_TYPE_COMM_FAIL:  return lv_color_hex(0xFFC107); /* amber */
        case ALERT_TYPE_OVERTEMP:   return lv_color_hex(0xFF5722); /* deep orange */
        default:                    return lv_color_hex(0x9E9E9E); /* grey */
    }
}

static const char * alert_type_icon(alert_type_t t)
{
    switch(t) {
        case ALERT_TYPE_OFFLINE:    return LV_SYMBOL_WARNING;
        case ALERT_TYPE_OVERPOWER:  return LV_SYMBOL_BELL;
        case ALERT_TYPE_COMM_FAIL:  return LV_SYMBOL_CLOSE;
        case ALERT_TYPE_OVERTEMP:   return LV_SYMBOL_WARNING;
        default:                    return LV_SYMBOL_BELL;
    }
}

void setup_scr_screen_alert(lv_ui *ui)
{
    ui->screen_alert = lv_obj_create(NULL);
    lv_obj_set_size(ui->screen_alert, 480, 320);
    lv_obj_set_scrollbar_mode(ui->screen_alert, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_bg_color(ui->screen_alert, lv_color_hex(0xF0F2F5), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->screen_alert, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);

    /* ======== header (40px) ======== */
    {
        lv_obj_t *h = lv_obj_create(ui->screen_alert);
        lv_obj_remove_style_all(h);
        lv_obj_set_size(h, 480, 40);
        lv_obj_set_pos(h, 0, 0);
        lv_obj_set_style_bg_color(h, lv_color_hex(0x2C3E50), 0);
        lv_obj_set_style_bg_opa(h, LV_OPA_COVER, 0);
        lv_obj_clear_flag(h, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_style_pad_all(h, 0, 0);

        lv_obj_t *back = lv_label_create(h);
        lv_label_set_text(back, LV_SYMBOL_LEFT);
        lv_obj_set_style_text_color(back, lv_color_hex(0xffffff), 0);
        lv_obj_set_style_text_font(back, &lv_font_montserratMedium_16, 0);
        lv_obj_set_style_bg_opa(back, 0, 0);
        lv_obj_set_pos(back, 14, 10);

        lv_obj_t *sep = lv_obj_create(h);
        lv_obj_remove_style_all(sep);
        lv_obj_set_size(sep, 1, 18);
        lv_obj_set_pos(sep, 38, 10);
        lv_obj_set_style_bg_color(sep, lv_color_hex(0x546e7a), 0);
        lv_obj_set_style_bg_opa(sep, LV_OPA_COVER, 0);
        lv_obj_clear_flag(sep, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t *title = lv_label_create(h);
        lv_label_set_text(title, LV_SYMBOL_WARNING " 告警列表");
        lv_obj_set_style_text_color(title, lv_color_hex(0xffffff), 0);
        lv_obj_set_style_text_font(title, &lv_font_SourceHanSerifSC_Regular_16, 0);
        lv_obj_set_style_bg_opa(title, 0, 0);
        lv_obj_set_pos(title, 48, 10);

        /* count badge */
        ui->screen_alert_label_count = lv_label_create(h);
        char cnt_buf[16];
        lv_snprintf(cnt_buf, sizeof(cnt_buf), "%d", (int)ALERT_COUNT);
        lv_label_set_text(ui->screen_alert_label_count, cnt_buf);
        lv_obj_set_style_text_color(ui->screen_alert_label_count, lv_color_hex(0xffffff), 0);
        lv_obj_set_style_text_font(ui->screen_alert_label_count, &lv_font_montserratMedium_16, 0);
        lv_obj_set_style_bg_opa(ui->screen_alert_label_count, 0, 0);
        lv_obj_align(ui->screen_alert_label_count, LV_ALIGN_RIGHT_MID, -14, 0);
    }

    /* ======== stats bar (y=42, h=30) ======== */
    {
        lv_obj_t *bar = lv_obj_create(ui->screen_alert);
        lv_obj_remove_style_all(bar);
        lv_obj_set_size(bar, 464, 30);
        lv_obj_set_pos(bar, 8, 42);
        lv_obj_clear_flag(bar, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_style_pad_all(bar, 0, 0);

        /* 3 stat labels: offline / abnormal / comm fail */
        lv_obj_t *s1 = lv_label_create(bar);
        lv_label_set_text(s1, LV_SYMBOL_CLOSE " 离线: 3");
        lv_obj_set_style_text_color(s1, lv_color_hex(0xF44336), 0);
        lv_obj_set_style_text_font(s1, &lv_font_SourceHanSerifSC_Regular_12, 0);
        lv_obj_set_style_bg_opa(s1, 0, 0);
        lv_obj_set_pos(s1, 4, 8);

        lv_obj_t *s2 = lv_label_create(bar);
        lv_label_set_text(s2, LV_SYMBOL_BELL " 异常: 2");
        lv_obj_set_style_text_color(s2, lv_color_hex(0xFF9800), 0);
        lv_obj_set_style_text_font(s2, &lv_font_SourceHanSerifSC_Regular_12, 0);
        lv_obj_set_style_bg_opa(s2, 0, 0);
        lv_obj_set_pos(s2, 120, 8);

        lv_obj_t *s3 = lv_label_create(bar);
        lv_label_set_text(s3, LV_SYMBOL_WARNING " 通信: 2");
        lv_obj_set_style_text_color(s3, lv_color_hex(0xFFC107), 0);
        lv_obj_set_style_text_font(s3, &lv_font_SourceHanSerifSC_Regular_12, 0);
        lv_obj_set_style_bg_opa(s3, 0, 0);
        lv_obj_set_pos(s3, 240, 8);

        lv_obj_t *s4 = lv_label_create(bar);
        lv_label_set_text(s4, LV_SYMBOL_WARNING " 温度: 1");
        lv_obj_set_style_text_color(s4, lv_color_hex(0xFF5722), 0);
        lv_obj_set_style_text_font(s4, &lv_font_SourceHanSerifSC_Regular_12, 0);
        lv_obj_set_style_bg_opa(s4, 0, 0);
        lv_obj_set_pos(s4, 356, 8);
    }

    /* ======== alert list card (y=74, h=244) ======== */
    {
        lv_obj_t *card = lv_obj_create(ui->screen_alert);
        lv_obj_remove_style_all(card);
        lv_obj_set_size(card, 464, 244);
        lv_obj_set_pos(card, 8, 74);
        lv_obj_set_style_radius(card, 8, 0);
        lv_obj_set_style_bg_color(card, lv_color_hex(0xffffff), 0);
        lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
        lv_obj_set_style_shadow_width(card, 4, 0);
        lv_obj_set_style_shadow_opa(card, 20, 0);
        lv_obj_set_style_shadow_ofs_y(card, 2, 0);
        lv_obj_set_style_border_width(card, 0, 0);
        lv_obj_set_style_pad_all(card, 4, 0);

        /* scrollable container for alert items */
        lv_obj_t *list_cont = lv_obj_create(card);
        lv_obj_remove_style_all(list_cont);
        lv_obj_set_size(list_cont, 456, 236);
        lv_obj_set_pos(list_cont, 0, 0);
        lv_obj_set_style_pad_all(list_cont, 2, 0);
        lv_obj_set_style_pad_row(list_cont, 4, 0);
        lv_obj_set_flex_flow(list_cont, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(list_cont, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_scrollbar_mode(list_cont, LV_SCROLLBAR_MODE_AUTO);

        ui->screen_alert_list = list_cont;

        /* 创建警报项 */
        for(int i = 0; i < (int)ALERT_COUNT && i < ALERT_MAX_ITEMS; i++)
        {
            lv_obj_t *item = lv_obj_create(list_cont);
            lv_obj_remove_style_all(item);
            lv_obj_set_size(item, 448, 26);
            lv_obj_set_style_radius(item, 4, 0);
            lv_obj_set_style_bg_color(item, lv_color_hex(0xFAFAFA), 0);
            lv_obj_set_style_bg_opa(item, LV_OPA_COVER, 0);
            lv_obj_set_style_border_width(item, 0, 0);
            lv_obj_set_style_pad_all(item, 0, 0);
            lv_obj_clear_flag(item, LV_OBJ_FLAG_SCROLLABLE);

            /* 左侧颜色指示器 */
            lv_obj_t *dot = lv_obj_create(item);
            lv_obj_remove_style_all(dot);
            lv_obj_set_size(dot, 4, 20);
            lv_obj_set_pos(dot, 2, 3);
            lv_obj_set_style_radius(dot, 2, 0);
            lv_obj_set_style_bg_color(dot, alert_type_color(demo_alerts[i].type), 0);
            lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);
            lv_obj_clear_flag(dot, LV_OBJ_FLAG_SCROLLABLE);

            /* 图标 */
            lv_obj_t *icon = lv_label_create(item);
            lv_label_set_text(icon, alert_type_icon(demo_alerts[i].type));
            lv_obj_set_style_text_color(icon, alert_type_color(demo_alerts[i].type), 0);
            lv_obj_set_style_text_font(icon, &lv_font_montserratMedium_16, 0);
            lv_obj_set_style_bg_opa(icon, 0, 0);
            lv_obj_set_pos(icon, 12, 4);

            /* 类型名称 */
            lv_obj_t *type_lbl = lv_label_create(item);
            lv_label_set_text(type_lbl, alert_type_name(demo_alerts[i].type));
            lv_obj_set_style_text_color(type_lbl, lv_color_hex(0x2C3E50), 0);
            lv_obj_set_style_text_font(type_lbl, &lv_font_SourceHanSerifSC_Regular_12, 0);
            lv_obj_set_style_bg_opa(type_lbl, 0, 0);
            lv_obj_set_pos(type_lbl, 32, 6);

            /* 地址 */
            lv_obj_t *addr_lbl = lv_label_create(item);
            lv_label_set_text(addr_lbl, demo_alerts[i].addr);
            lv_obj_set_style_text_color(addr_lbl, lv_color_hex(0x607D8B), 0);
            lv_obj_set_style_text_font(addr_lbl, &lv_font_SourceHanSerifSC_Regular_12, 0);
            lv_obj_set_style_bg_opa(addr_lbl, 0, 0);
            lv_obj_set_pos(addr_lbl, 120, 6);

            /* 时间（右对齐） */
            lv_obj_t *time_lbl = lv_label_create(item);
            lv_label_set_text(time_lbl, demo_alerts[i].time);
            lv_obj_set_style_text_color(time_lbl, lv_color_hex(0x90A4AE), 0);
            lv_obj_set_style_text_font(time_lbl, &lv_font_SourceHanSerifSC_Regular_12, 0);
            lv_obj_set_style_bg_opa(time_lbl, 0, 0);
            lv_obj_align(time_lbl, LV_ALIGN_RIGHT_MID, -4, 0);
        }
    }

    lv_obj_update_layout(ui->screen_alert);
    events_init_screen_alert(ui);
}
