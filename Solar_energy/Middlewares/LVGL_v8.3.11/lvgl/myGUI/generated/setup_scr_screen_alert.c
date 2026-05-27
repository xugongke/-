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

/* Alert types - matches device_state_t error bits */
typedef enum {
    ALERT_TYPE_COMM_FAIL = 0,   /* 通信故障 (bit2) */
    ALERT_TYPE_RELAY_ERR,       /* 继电器/开关异常 (bit5) */
    ALERT_TYPE_TEMP_ERR,        /* 温度异常 (bit6) */
    ALERT_TYPE_POWER_REVERSE,   /* 电源反接 (bit7) */
    ALERT_TYPE_COUNT            /* 类型总数 */
} alert_type_t;

static const char * alert_type_name(alert_type_t t)
{
    switch(t) {
        case ALERT_TYPE_COMM_FAIL:    return "通信异常 ";
        case ALERT_TYPE_RELAY_ERR:    return "开关异常 ";
        case ALERT_TYPE_TEMP_ERR:     return "温度异常 ";
        case ALERT_TYPE_POWER_REVERSE:return "电源反接 ";
        default:                      return "未知 ";
    }
}

static lv_color_t alert_type_color(alert_type_t t)
{
    switch(t) {
        case ALERT_TYPE_COMM_FAIL:    return lv_color_hex(0xFFC107); /* amber */
        case ALERT_TYPE_RELAY_ERR:    return lv_color_hex(0xFF9800); /* orange */
        case ALERT_TYPE_TEMP_ERR:     return lv_color_hex(0xF44336); /* red */
        case ALERT_TYPE_POWER_REVERSE:return lv_color_hex(0xFF5722); /* deep orange */
        default:                      return lv_color_hex(0x9E9E9E); /* grey */
    }
}

static const char * alert_type_icon(alert_type_t t)
{
    switch(t) {
        case ALERT_TYPE_COMM_FAIL:    return LV_SYMBOL_CLOSE;
        case ALERT_TYPE_RELAY_ERR:    return LV_SYMBOL_BELL;
        case ALERT_TYPE_TEMP_ERR:     return LV_SYMBOL_WARNING;
        case ALERT_TYPE_POWER_REVERSE:return LV_SYMBOL_WARNING;
        default:                      return LV_SYMBOL_BELL;
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
        lv_label_set_text(ui->screen_alert_label_count, "0");
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

        /* 4 stat labels - stored in UI struct for dynamic update */
        ui->screen_alert_stat_comm = lv_label_create(bar);
        lv_label_set_text(ui->screen_alert_stat_comm, LV_SYMBOL_CLOSE " 通信异常: 0");
        lv_obj_set_style_text_color(ui->screen_alert_stat_comm, lv_color_hex(0xFFC107), 0);
        lv_obj_set_style_text_font(ui->screen_alert_stat_comm, &lv_font_SourceHanSerifSC_Regular_12, 0);
        lv_obj_set_style_bg_opa(ui->screen_alert_stat_comm, 0, 0);
        lv_obj_set_pos(ui->screen_alert_stat_comm, 4, 8);

        ui->screen_alert_stat_relay = lv_label_create(bar);
        lv_label_set_text(ui->screen_alert_stat_relay, LV_SYMBOL_BELL " 开关异常: 0");
        lv_obj_set_style_text_color(ui->screen_alert_stat_relay, lv_color_hex(0xFF9800), 0);
        lv_obj_set_style_text_font(ui->screen_alert_stat_relay, &lv_font_SourceHanSerifSC_Regular_12, 0);
        lv_obj_set_style_bg_opa(ui->screen_alert_stat_relay, 0, 0);
        lv_obj_set_pos(ui->screen_alert_stat_relay, 120, 8);

        ui->screen_alert_stat_temp = lv_label_create(bar);
        lv_label_set_text(ui->screen_alert_stat_temp, LV_SYMBOL_WARNING " 温度异常: 0");
        lv_obj_set_style_text_color(ui->screen_alert_stat_temp, lv_color_hex(0xF44336), 0);
        lv_obj_set_style_text_font(ui->screen_alert_stat_temp, &lv_font_SourceHanSerifSC_Regular_12, 0);
        lv_obj_set_style_bg_opa(ui->screen_alert_stat_temp, 0, 0);
        lv_obj_set_pos(ui->screen_alert_stat_temp, 240, 8);

        ui->screen_alert_stat_power = lv_label_create(bar);
        lv_label_set_text(ui->screen_alert_stat_power, LV_SYMBOL_WARNING " 电源反接: 0");
        lv_obj_set_style_text_color(ui->screen_alert_stat_power, lv_color_hex(0xFF5722), 0);
        lv_obj_set_style_text_font(ui->screen_alert_stat_power, &lv_font_SourceHanSerifSC_Regular_12, 0);
        lv_obj_set_style_bg_opa(ui->screen_alert_stat_power, 0, 0);
        lv_obj_set_pos(ui->screen_alert_stat_power, 356, 8);
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
    }

    lv_obj_update_layout(ui->screen_alert);
    events_init_screen_alert(ui);
}
