/*
* Copyright 2026 NXP
* Dark Theme - Alert Page
*/

#include "lvgl.h"
#include <stdio.h>
#include "gui_guider.h"
#include "events_init.h"
#include "widgets_init.h"
#include "custom.h"

/* Alert types - matches device_state_t error bits */
typedef enum {
    ALERT_TYPE_COMM_FAIL = 0,
    ALERT_TYPE_RELAY_ERR,
    ALERT_TYPE_TEMP_ERR,
    ALERT_TYPE_POWER_REVERSE,
    ALERT_TYPE_COUNT
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
        case ALERT_TYPE_COMM_FAIL:    return lv_color_hex(0xFFC107);
        case ALERT_TYPE_RELAY_ERR:    return lv_color_hex(0xFF9800);
        case ALERT_TYPE_TEMP_ERR:     return lv_color_hex(0xF44336);
        case ALERT_TYPE_POWER_REVERSE:return lv_color_hex(0xFF5722);
        default:                      return lv_color_hex(0x9E9E9E);
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
    lv_obj_set_style_bg_color(ui->screen_alert, lv_color_hex(0x0D1117), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->screen_alert, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);

    /* ======== 标题栏 (40px) ======== */
    {
        lv_obj_t *h = lv_obj_create(ui->screen_alert);
        lv_obj_remove_style_all(h);
        lv_obj_set_size(h, 480, 40);
        lv_obj_set_pos(h, 0, 0);
        lv_obj_set_style_bg_color(h, lv_color_hex(0x161B22), 0);
        lv_obj_set_style_bg_opa(h, LV_OPA_COVER, 0);
        lv_obj_clear_flag(h, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_style_pad_all(h, 0, 0);
        lv_obj_set_style_shadow_width(h, 4, 0);
        lv_obj_set_style_shadow_color(h, lv_color_hex(0x000000), 0);
        lv_obj_set_style_shadow_opa(h, 40, 0);
        lv_obj_set_style_shadow_ofs_y(h, 2, 0);

        lv_obj_t *back = lv_label_create(h);
        lv_label_set_text(back, LV_SYMBOL_LEFT);
        lv_obj_set_style_text_color(back, lv_color_hex(0xF0C040), 0);
        lv_obj_set_style_text_font(back, &lv_font_SourceHanSerifSC_Regular_16, 0);
        lv_obj_set_style_bg_opa(back, 0, 0);
        lv_obj_set_pos(back, 14, 10);

        lv_obj_t *sep = lv_obj_create(h);
        lv_obj_remove_style_all(sep);
        lv_obj_set_size(sep, 1, 18);
        lv_obj_set_pos(sep, 38, 10);
        lv_obj_set_style_bg_color(sep, lv_color_hex(0x30363D), 0);
        lv_obj_set_style_bg_opa(sep, LV_OPA_COVER, 0);
        lv_obj_clear_flag(sep, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t *title = lv_label_create(h);
        lv_label_set_text(title, LV_SYMBOL_WARNING " 告警列表");
        lv_obj_set_style_text_color(title, lv_color_hex(0xE6EDF3), 0);
        lv_obj_set_style_text_font(title, &lv_font_SourceHanSerifSC_Regular_16, 0);
        lv_obj_set_style_bg_opa(title, 0, 0);
        lv_obj_set_pos(title, 48, 10);

        ui->screen_alert_label_count = lv_label_create(h);
        lv_label_set_text(ui->screen_alert_label_count, "0");
        lv_obj_set_style_text_color(ui->screen_alert_label_count, lv_color_hex(0xD29922), 0);
        lv_obj_set_style_text_font(ui->screen_alert_label_count, &lv_font_SourceHanSerifSC_Regular_16, 0);
        lv_obj_set_style_bg_opa(ui->screen_alert_label_count, 0, 0);
        lv_obj_align(ui->screen_alert_label_count, LV_ALIGN_RIGHT_MID, -14, 0);
    }

    /* ======== 统计栏 (暗色胶囊) ======== */
    {
        lv_obj_t *bar = lv_obj_create(ui->screen_alert);
        lv_obj_remove_style_all(bar);
        lv_obj_set_size(bar, 464, 30);
        lv_obj_set_pos(bar, 8, 42);
        lv_obj_clear_flag(bar, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_style_pad_all(bar, 0, 0);
        lv_obj_set_style_radius(bar, 8, 0);
        lv_obj_set_style_bg_color(bar, lv_color_hex(0x161B22), 0);
        lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(bar, 1, 0);
        lv_obj_set_style_border_color(bar, lv_color_hex(0x30363D), 0);
        lv_obj_set_style_border_opa(bar, 60, 0);

        ui->screen_alert_stat_comm = lv_label_create(bar);
        lv_label_set_text(ui->screen_alert_stat_comm, LV_SYMBOL_CLOSE " 通信: 0");
        lv_obj_set_style_text_color(ui->screen_alert_stat_comm, lv_color_hex(0xFFC107), 0);
        lv_obj_set_style_text_font(ui->screen_alert_stat_comm, &lv_font_SourceHanSerifSC_Regular_12, 0);
        lv_obj_set_style_bg_opa(ui->screen_alert_stat_comm, 0, 0);
        lv_obj_set_pos(ui->screen_alert_stat_comm, 8, 8);

        ui->screen_alert_stat_relay = lv_label_create(bar);
        lv_label_set_text(ui->screen_alert_stat_relay, LV_SYMBOL_BELL " 开关: 0");
        lv_obj_set_style_text_color(ui->screen_alert_stat_relay, lv_color_hex(0xFF9800), 0);
        lv_obj_set_style_text_font(ui->screen_alert_stat_relay, &lv_font_SourceHanSerifSC_Regular_12, 0);
        lv_obj_set_style_bg_opa(ui->screen_alert_stat_relay, 0, 0);
        lv_obj_set_pos(ui->screen_alert_stat_relay, 120, 8);

        ui->screen_alert_stat_temp = lv_label_create(bar);
        lv_label_set_text(ui->screen_alert_stat_temp, LV_SYMBOL_WARNING " 温度: 0");
        lv_obj_set_style_text_color(ui->screen_alert_stat_temp, lv_color_hex(0xF44336), 0);
        lv_obj_set_style_text_font(ui->screen_alert_stat_temp, &lv_font_SourceHanSerifSC_Regular_12, 0);
        lv_obj_set_style_bg_opa(ui->screen_alert_stat_temp, 0, 0);
        lv_obj_set_pos(ui->screen_alert_stat_temp, 240, 8);

        ui->screen_alert_stat_power = lv_label_create(bar);
        lv_label_set_text(ui->screen_alert_stat_power, LV_SYMBOL_WARNING " 反接: 0");
        lv_obj_set_style_text_color(ui->screen_alert_stat_power, lv_color_hex(0xFF5722), 0);
        lv_obj_set_style_text_font(ui->screen_alert_stat_power, &lv_font_SourceHanSerifSC_Regular_12, 0);
        lv_obj_set_style_bg_opa(ui->screen_alert_stat_power, 0, 0);
        lv_obj_set_pos(ui->screen_alert_stat_power, 356, 8);
    }

    /* ======== 告警列表卡片 (暗色) ======== */
    {
        lv_obj_t *card = lv_obj_create(ui->screen_alert);
        lv_obj_remove_style_all(card);
        lv_obj_set_size(card, 464, 320 - 1);
        lv_obj_set_pos(card, 8, 74);
        lv_obj_set_style_radius(card, 10, 0);
        lv_obj_set_style_bg_color(card, lv_color_hex(0x161B22), 0);
        lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(card, 1, 0);
        lv_obj_set_style_border_color(card, lv_color_hex(0x30363D), 0);
        lv_obj_set_style_border_opa(card, 80, 0);
        lv_obj_set_style_shadow_width(card, 6, 0);
        lv_obj_set_style_shadow_color(card, lv_color_hex(0x000000), 0);
        lv_obj_set_style_shadow_opa(card, 40, 0);
        lv_obj_set_style_shadow_ofs_y(card, 2, 0);
        lv_obj_set_style_pad_all(card, 4, 0);

        lv_obj_t *list_cont = lv_obj_create(card);
        lv_obj_remove_style_all(list_cont);
        lv_obj_set_size(list_cont, 456, 320 - 2 - 28);
        lv_obj_set_pos(list_cont, 0, 0);
        lv_obj_set_style_pad_all(list_cont, 2, 0);
        lv_obj_set_style_pad_row(list_cont, 4, 0);
        lv_obj_set_flex_flow(list_cont, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(list_cont, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_scrollbar_mode(list_cont, LV_SCROLLBAR_MODE_OFF);

        ui->screen_alert_list = list_cont;
    }

    /* ======== 底部提示区域（告警过多时显示） ======== */
    {
        ui->screen_alert_tip = lv_obj_create(ui->screen_alert);
        lv_obj_remove_style_all(ui->screen_alert_tip);
        lv_obj_set_size(ui->screen_alert_tip, 464, 28);
        lv_obj_set_pos(ui->screen_alert_tip, 8, 320 - 28 - 1);
        lv_obj_set_style_radius(ui->screen_alert_tip, 4, 0);
        lv_obj_set_style_bg_color(ui->screen_alert_tip, lv_color_hex(0x1C2333), 0);
        lv_obj_set_style_bg_opa(ui->screen_alert_tip, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(ui->screen_alert_tip, 1, 0);
        lv_obj_set_style_border_color(ui->screen_alert_tip, lv_color_hex(0xD29922), 0);
        lv_obj_set_style_border_opa(ui->screen_alert_tip, LV_OPA_COVER, 0);
        lv_obj_set_style_pad_all(ui->screen_alert_tip, 0, 0);
        lv_obj_clear_flag(ui->screen_alert_tip, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(ui->screen_alert_tip, LV_OBJ_FLAG_CLICKABLE);

        /* 创建tip内部文本标签 */
        lv_obj_t *tip_lbl = lv_label_create(ui->screen_alert_tip);
        lv_obj_set_style_text_color(tip_lbl, lv_color_hex(0xD29922), 0);
        lv_obj_set_style_text_font(tip_lbl, &lv_font_SourceHanSerifSC_Regular_12, 0);
        lv_obj_set_style_bg_opa(tip_lbl, 0, 0);
        lv_obj_align(tip_lbl, LV_ALIGN_CENTER, 0, 0);

        /* tip聚焦样式：橙色边框 */
        static lv_style_t style_tip_focused;
        ui_init_style(&style_tip_focused);
        lv_style_set_border_width(&style_tip_focused, 2);
        lv_style_set_border_color(&style_tip_focused, lv_color_hex(0xD29922));
        lv_style_set_border_opa(&style_tip_focused, LV_OPA_COVER);
        lv_style_set_border_side(&style_tip_focused, LV_BORDER_SIDE_FULL);
        lv_style_set_bg_color(&style_tip_focused, lv_color_hex(0x2D1F0E));
        lv_style_set_bg_opa(&style_tip_focused, LV_OPA_COVER);
        lv_style_set_radius(&style_tip_focused, 4);
        lv_obj_add_style(ui->screen_alert_tip, &style_tip_focused, LV_PART_MAIN | LV_STATE_FOCUSED);
    }

    /* ======== 页码标签 ======== */
    {
        ui->screen_alert_page_label = lv_label_create(ui->screen_alert);
        lv_obj_set_style_text_font(ui->screen_alert_page_label, &lv_font_SourceHanSerifSC_Regular_12, 0);
        lv_obj_set_style_text_color(ui->screen_alert_page_label, lv_color_hex(0x484F58), 0);
        lv_obj_align(ui->screen_alert_page_label, LV_ALIGN_BOTTOM_RIGHT, -14, -4);
    }

    lv_obj_update_layout(ui->screen_alert);
    events_init_screen_alert(ui);
}
