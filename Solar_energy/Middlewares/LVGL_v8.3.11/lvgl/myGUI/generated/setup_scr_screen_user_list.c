/*
* Copyright 2026 NXP
* Dark Theme - User List Screen
*/

#include "lvgl.h"
#include <stdio.h>
#include "gui_guider.h"
#include "events_init.h"
#include "widgets_init.h"
#include "custom.h"


void setup_scr_screen_user_list(lv_ui *ui)
{
    /* ======== 创建屏幕 ======== */
    ui->screen_user_list = lv_obj_create(NULL);
    lv_obj_set_size(ui->screen_user_list, 480, 320);
    lv_obj_set_scrollbar_mode(ui->screen_user_list, LV_SCROLLBAR_MODE_OFF);

    /* 深色背景 */
    lv_obj_set_style_bg_color(ui->screen_user_list, lv_color_hex(0x0D1117), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->screen_user_list, LV_OPA_COVER, LV_PART_MAIN|LV_STATE_DEFAULT);

    /* ============================================================
     *  顶部导航栏 (深蓝色暗色风格)
     * ============================================================ */
    {
        lv_obj_t *header = lv_obj_create(ui->screen_user_list);
        lv_obj_remove_style_all(header);
        lv_obj_set_size(header, 480, 42);
        lv_obj_set_pos(header, 0, 0);
        lv_obj_set_style_bg_color(header, lv_color_hex(0x161B22), 0);
        lv_obj_set_style_bg_opa(header, LV_OPA_COVER, 0);
        lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_style_shadow_width(header, 4, 0);
        lv_obj_set_style_shadow_color(header, lv_color_hex(0x000000), 0);
        lv_obj_set_style_shadow_opa(header, 40, 0);
        lv_obj_set_style_shadow_ofs_y(header, 2, 0);
        lv_obj_set_style_shadow_spread(header, 0, 0);
        lv_obj_set_style_pad_all(header, 0, 0);
        lv_obj_set_style_border_width(header, 0, 0);
        lv_obj_set_style_border_color(header, lv_color_hex(0x30363D), 0);

        /* 返回箭头 */
        lv_obj_t *back_icon = lv_label_create(header);
        lv_label_set_text(back_icon, LV_SYMBOL_LEFT);
        lv_obj_set_style_text_color(back_icon, lv_color_hex(0xF0C040), 0);
        lv_obj_set_style_text_font(back_icon, &lv_font_montserratMedium_16, 0);
        lv_obj_set_style_bg_opa(back_icon, 0, 0);
        lv_obj_set_pos(back_icon, 14, 11);

        /* 分隔竖线 */
        lv_obj_t *sep = lv_obj_create(header);
        lv_obj_remove_style_all(sep);
        lv_obj_set_size(sep, 1, 18);
        lv_obj_set_pos(sep, 38, 11);
        lv_obj_set_style_bg_color(sep, lv_color_hex(0x30363D), 0);
        lv_obj_set_style_bg_opa(sep, LV_OPA_COVER, 0);
        lv_obj_clear_flag(sep, LV_OBJ_FLAG_SCROLLABLE);

        /* 标题 */
        lv_obj_t *title_lbl = lv_label_create(header);
        lv_label_set_text(title_lbl, "用户列表");
        lv_obj_set_style_text_color(title_lbl, lv_color_hex(0xE6EDF3), 0);
        lv_obj_set_style_text_font(title_lbl, &lv_font_SourceHanSerifSC_Regular_16, 0);
        lv_obj_set_style_bg_opa(title_lbl, 0, 0);
        lv_obj_set_pos(title_lbl, 48, 11);

        /* 右侧WiFi图标 */
        lv_obj_t *count_icon = lv_label_create(header);
        lv_label_set_text(count_icon, LV_SYMBOL_WIFI);
        lv_obj_set_style_text_color(count_icon, lv_color_hex(0x484F58), 0);
        lv_obj_set_style_text_font(count_icon, &lv_font_montserratMedium_16, 0);
        lv_obj_set_style_bg_opa(count_icon, 0, 0);
        lv_obj_align(count_icon, LV_ALIGN_RIGHT_MID, -14, 0);
    }

    /* ======== 兼容旧标签 (隐藏) ======== */
    ui->screen_user_list_label_1 = lv_label_create(ui->screen_user_list);
    lv_label_set_text(ui->screen_user_list_label_1, "");
    lv_obj_set_size(ui->screen_user_list_label_1, 0, 0);
    lv_obj_set_pos(ui->screen_user_list_label_1, 0, 0);
    lv_obj_set_style_bg_opa(ui->screen_user_list_label_1, 0, 0);
    lv_obj_add_flag(ui->screen_user_list_label_1, LV_OBJ_FLAG_HIDDEN);

    /* ============================================================
     *  列表区域 (暗色主题)
     * ============================================================ */
    ui->screen_user_list_list_1 = lv_list_create(ui->screen_user_list);
    lv_obj_set_pos(ui->screen_user_list_list_1, 8, 46);
    lv_obj_set_size(ui->screen_user_list_list_1, 464, 268);
    lv_obj_set_scrollbar_mode(ui->screen_user_list_list_1, LV_SCROLLBAR_MODE_OFF);

    /* 列表主体: 暗色背景 */
    static lv_style_t style_list_main;
    ui_init_style(&style_list_main);
    lv_style_set_pad_top(&style_list_main, 4);
    lv_style_set_pad_left(&style_list_main, 4);
    lv_style_set_pad_right(&style_list_main, 4);
    lv_style_set_pad_bottom(&style_list_main, 4);
    lv_style_set_pad_row(&style_list_main, 8);
    lv_style_set_bg_opa(&style_list_main, 0);
    lv_style_set_bg_grad_dir(&style_list_main, LV_GRAD_DIR_NONE);
    lv_style_set_border_width(&style_list_main, 0);
    lv_style_set_radius(&style_list_main, 0);
    lv_style_set_shadow_width(&style_list_main, 0);
    lv_obj_add_style(ui->screen_user_list_list_1, &style_list_main, LV_PART_MAIN|LV_STATE_DEFAULT);

    /* 滚动条隐藏 */
    static lv_style_t style_list_sb;
    ui_init_style(&style_list_sb);
    lv_style_set_bg_opa(&style_list_sb, 0);
    lv_obj_add_style(ui->screen_user_list_list_1, &style_list_sb, LV_PART_SCROLLBAR|LV_STATE_DEFAULT);

    lv_obj_update_layout(ui->screen_user_list);
    events_init_screen_user_list(ui);
}
