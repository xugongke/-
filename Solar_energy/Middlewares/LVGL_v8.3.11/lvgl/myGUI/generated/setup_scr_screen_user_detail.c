/*
* Copyright 2026 NXP
* NXP Proprietary. This software is owned or controlled by NXP and may only be used strictly in
* accordance with the applicable license terms. By expressly accepting such terms or by downloading, installing,
* activating and/or otherwise using the software, you are agreeing that you have read, or that you agree to
* comply with, be bound by, such license terms.  If you do not agree to be bound by the applicable license
* terms, then you may not retain, install, activate, or otherwise use the software.
*/

#include "lvgl.h"
#include <stdio.h>
#include "gui_guider.h"
#include "events_init.h"
#include "widgets_init.h"
#include "custom.h"



void setup_scr_screen_user_detail(lv_ui *ui)
{
    /* ======== 创建屏幕 ======== */
    ui->screen_user_detail = lv_obj_create(NULL);
    lv_obj_set_size(ui->screen_user_detail, 480, 320);
    lv_obj_set_scrollbar_mode(ui->screen_user_detail, LV_SCROLLBAR_MODE_OFF);

    /* 浅灰白背景 (与 home / list 统一) */
    lv_obj_set_style_bg_color(ui->screen_user_detail, lv_color_hex(0xF0F2F5), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->screen_user_detail, LV_OPA_COVER, LV_PART_MAIN|LV_STATE_DEFAULT);

    /* ============================================================
     *  顶部导航栏 (深蓝色, 与 list 风格完全统一)
     *  高度 42px, 底部阴影, 白色标题 + 返回箭头
     * ============================================================ */
    {
        lv_obj_t *header = lv_obj_create(ui->screen_user_detail);
        lv_obj_remove_style_all(header);
        lv_obj_set_size(header, 480, 42);
        lv_obj_set_pos(header, 0, 0);
        lv_obj_set_style_bg_color(header, lv_color_hex(0x2C3E50), 0);
        lv_obj_set_style_bg_opa(header, LV_OPA_COVER, 0);
        lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_style_shadow_width(header, 6, 0);
        lv_obj_set_style_shadow_color(header, lv_color_hex(0x000000), 0);
        lv_obj_set_style_shadow_opa(header, 30, 0);
        lv_obj_set_style_shadow_ofs_y(header, 3, 0);
        lv_obj_set_style_shadow_spread(header, 0, 0);
        lv_obj_set_style_pad_all(header, 0, 0);

        /* 返回箭头 (白色) */
        lv_obj_t *back_icon = lv_label_create(header);
        lv_label_set_text(back_icon, LV_SYMBOL_LEFT);
        lv_obj_set_style_text_color(back_icon, lv_color_hex(0xffffff), 0);
        lv_obj_set_style_text_font(back_icon, &lv_font_montserratMedium_16, 0);
        lv_obj_set_style_bg_opa(back_icon, 0, 0);
        lv_obj_set_pos(back_icon, 14, 11);

        /* 分隔竖线 */
        lv_obj_t *sep = lv_obj_create(header);
        lv_obj_remove_style_all(sep);
        lv_obj_set_size(sep, 1, 18);
        lv_obj_set_pos(sep, 38, 11);
        lv_obj_set_style_bg_color(sep, lv_color_hex(0x546e7a), 0);
        lv_obj_set_style_bg_opa(sep, LV_OPA_COVER, 0);
        lv_obj_clear_flag(sep, LV_OBJ_FLAG_SCROLLABLE);

        /* 标题 "用户详情" (白色) */
        lv_obj_t *title_lbl = lv_label_create(header);
        lv_label_set_text(title_lbl, "\xE7\x94\xA8\xE6\x88\xB7\xE8\xAF\xA6\xE6\x83\x85");  /* 用户详情 */
        lv_obj_set_style_text_color(title_lbl, lv_color_hex(0xffffff), 0);
        lv_obj_set_style_text_font(title_lbl, &lv_font_SourceHanSerifSC_Regular_16, 0);
        lv_obj_set_style_bg_opa(title_lbl, 0, 0);
        lv_obj_set_pos(title_lbl, 48, 11);

        /* 右侧编辑图标 */
        lv_obj_t *edit_icon = lv_label_create(header);
        lv_label_set_text(edit_icon, LV_SYMBOL_EDIT);
        lv_obj_set_style_text_color(edit_icon, lv_color_hex(0x8899aa), 0);
        lv_obj_set_style_text_font(edit_icon, &lv_font_montserratMedium_16, 0);
        lv_obj_set_style_bg_opa(edit_icon, 0, 0);
        lv_obj_align(edit_icon, LV_ALIGN_RIGHT_MID, -14, 0);
    }

    /* ============================================================
     *  用户名称标签 (在导航栏下方, 蓝色胶囊背景)
     * ============================================================ */
    ui->screen_user_detail_label_user = lv_label_create(ui->screen_user_detail);
    lv_label_set_text(ui->screen_user_detail_label_user, "\xE7\x94\xA8\xE6\x88\xB7\xEF\xBC\x9A--");  /* 用户：-- */
    lv_label_set_long_mode(ui->screen_user_detail_label_user, LV_LABEL_LONG_WRAP);
    lv_obj_set_pos(ui->screen_user_detail_label_user, 16, 50);
    lv_obj_set_size(ui->screen_user_detail_label_user, 448, 24);
    /* 蓝色主题色文字 */
    lv_obj_set_style_border_width(ui->screen_user_detail_label_user, 0, 0);
    lv_obj_set_style_radius(ui->screen_user_detail_label_user, 0, 0);
    lv_obj_set_style_text_color(ui->screen_user_detail_label_user, lv_color_hex(0x2C3E50), 0);
    lv_obj_set_style_text_font(ui->screen_user_detail_label_user, &lv_font_SourceHanSerifSC_Regular_16, 0);
    lv_obj_set_style_text_opa(ui->screen_user_detail_label_user, 255, 0);
    lv_obj_set_style_text_letter_space(ui->screen_user_detail_label_user, 0, 0);
    lv_obj_set_style_text_line_space(ui->screen_user_detail_label_user, 0, 0);
    lv_obj_set_style_text_align(ui->screen_user_detail_label_user, LV_TEXT_ALIGN_LEFT, 0);
    lv_obj_set_style_bg_opa(ui->screen_user_detail_label_user, 0, 0);
    lv_obj_set_style_pad_all(ui->screen_user_detail_label_user, 0, 0);
    lv_obj_set_style_shadow_width(ui->screen_user_detail_label_user, 0, 0);

    /* ============================================================
     *  数据表格 (白色圆角卡片 + 阴影 + 现代表格样式)
     *  居中显示, 圆角 12, 柔和阴影
     * ============================================================ */
    ui->screen_user_detail_table_1 = lv_table_create(ui->screen_user_detail);
    lv_table_set_col_cnt(ui->screen_user_detail_table_1, 2);
    lv_table_set_row_cnt(ui->screen_user_detail_table_1, 4);
    lv_table_set_cell_value(ui->screen_user_detail_table_1, 0, 0, "\xE9\xA1\xB9\xE7\x9B\xAE");       /* 项目 */
    lv_table_set_cell_value(ui->screen_user_detail_table_1, 1, 0, "\xE6\x97\xA5\xE7\x94\xA8\xE7\x94\xB5\xE9\x87\x8F");  /* 日用电量 */
    lv_table_set_cell_value(ui->screen_user_detail_table_1, 2, 0, "\xE6\x9C\x88\xE7\x94\xA8\xE7\x94\xB5\xE9\x87\x8F");  /* 月用电量 */
    lv_table_set_cell_value(ui->screen_user_detail_table_1, 3, 0, "\xE5\xB9\xB4\xE7\x94\xA8\xE7\x94\xB5\xE9\x87\x8F");  /* 年用电量 */
    lv_table_set_cell_value(ui->screen_user_detail_table_1, 0, 1, "\xE6\x95\xB0\xE5\x80\xBC");       /* 数值 */
    lv_table_set_cell_value(ui->screen_user_detail_table_1, 1, 1, "10w ");
    lv_table_set_cell_value(ui->screen_user_detail_table_1, 2, 1, "100w ");
    lv_table_set_cell_value(ui->screen_user_detail_table_1, 3, 1, "100kWh ");
    lv_obj_set_pos(ui->screen_user_detail_table_1, 40, 80);
    lv_obj_set_scrollbar_mode(ui->screen_user_detail_table_1, LV_SCROLLBAR_MODE_OFF);

    /* 表格: 主体样式 (白色圆角卡片 + 阴影) */
    lv_obj_set_style_pad_top(ui->screen_user_detail_table_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->screen_user_detail_table_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->screen_user_detail_table_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->screen_user_detail_table_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->screen_user_detail_table_1, LV_OPA_COVER, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui->screen_user_detail_table_1, lv_color_hex(0xffffff), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(ui->screen_user_detail_table_1, LV_GRAD_DIR_NONE, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui->screen_user_detail_table_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_user_detail_table_1, 12, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->screen_user_detail_table_1, 8, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_color(ui->screen_user_detail_table_1, lv_color_hex(0x000000), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_opa(ui->screen_user_detail_table_1, 25, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_ofs_y(ui->screen_user_detail_table_1, 3, LV_PART_MAIN|LV_STATE_DEFAULT);

    /* 表格: 单元格样式 (无边框, 底部细线分隔, 居中) */
    lv_obj_set_style_text_color(ui->screen_user_detail_table_1, lv_color_hex(0x2C3E50), LV_PART_ITEMS|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->screen_user_detail_table_1, &lv_font_SourceHanSerifSC_Regular_16, LV_PART_ITEMS|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->screen_user_detail_table_1, 255, LV_PART_ITEMS|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->screen_user_detail_table_1, LV_TEXT_ALIGN_CENTER, LV_PART_ITEMS|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->screen_user_detail_table_1, 0, LV_PART_ITEMS|LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui->screen_user_detail_table_1, 1, LV_PART_ITEMS|LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(ui->screen_user_detail_table_1, 60, LV_PART_ITEMS|LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ui->screen_user_detail_table_1, lv_color_hex(0xE0E5EC), LV_PART_ITEMS|LV_STATE_DEFAULT);
    lv_obj_set_style_border_side(ui->screen_user_detail_table_1, LV_BORDER_SIDE_BOTTOM, LV_PART_ITEMS|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->screen_user_detail_table_1, 10, LV_PART_ITEMS|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->screen_user_detail_table_1, 10, LV_PART_ITEMS|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->screen_user_detail_table_1, 16, LV_PART_ITEMS|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->screen_user_detail_table_1, 16, LV_PART_ITEMS|LV_STATE_DEFAULT);

    /* 设置列宽 */
    lv_table_set_col_width(ui->screen_user_detail_table_1, 0, 180);
    lv_table_set_col_width(ui->screen_user_detail_table_1, 1, 180);

    /* ============================================================
     *  底部信息栏 (更新时间, 胶囊药丸风格)
     *  深色半透明背景, 与 home 状态栏风格统一
     * ============================================================ */
    ui->screen_user_detail_cont_1 = lv_obj_create(ui->screen_user_detail);
    lv_obj_remove_style_all(ui->screen_user_detail_cont_1);
    lv_obj_set_pos(ui->screen_user_detail_cont_1, 40, 268);
    lv_obj_set_size(ui->screen_user_detail_cont_1, 400, 28);
    lv_obj_clear_flag(ui->screen_user_detail_cont_1, LV_OBJ_FLAG_SCROLLABLE);
    /* 药丸胶囊: 深色半透明背景 + 圆角 */
    lv_obj_set_style_radius(ui->screen_user_detail_cont_1, 14, 0);
    lv_obj_set_style_bg_color(ui->screen_user_detail_cont_1, lv_color_hex(0x2C3E50), 0);
    lv_obj_set_style_bg_opa(ui->screen_user_detail_cont_1, 160, 0);
    lv_obj_set_style_shadow_width(ui->screen_user_detail_cont_1, 3, 0);
    lv_obj_set_style_shadow_color(ui->screen_user_detail_cont_1, lv_color_hex(0x000000), 0);
    lv_obj_set_style_shadow_opa(ui->screen_user_detail_cont_1, 20, 0);
    lv_obj_set_style_shadow_ofs_y(ui->screen_user_detail_cont_1, 1, 0);
    lv_obj_set_style_pad_all(ui->screen_user_detail_cont_1, 0, 0);

    /* 时钟图标 (白色) */
    {
        lv_obj_t *clock_icon = lv_label_create(ui->screen_user_detail_cont_1);
        lv_label_set_text(clock_icon, LV_SYMBOL_BELL);
        lv_obj_set_style_text_color(clock_icon, lv_color_hex(0x8899aa), 0);
        lv_obj_set_style_text_font(clock_icon, &lv_font_montserratMedium_12, 0);
        lv_obj_set_style_bg_opa(clock_icon, 0, 0);
        lv_obj_align(clock_icon, LV_ALIGN_LEFT_MID, 8, 0);
    }

    /* "更新时间:" 标签 (浅灰白色) */
    ui->screen_user_detail_label_4 = lv_label_create(ui->screen_user_detail_cont_1);
    lv_label_set_text(ui->screen_user_detail_label_4, "\xE6\x9B\xB4\xE6\x96\xB0\xE6\x97\xB6\xE9\x97\xB4: ");  /* 更新时间: */
    lv_label_set_long_mode(ui->screen_user_detail_label_4, LV_LABEL_LONG_WRAP);
    lv_obj_set_pos(ui->screen_user_detail_label_4, 24, 5);
    lv_obj_set_size(ui->screen_user_detail_label_4, 78, 19);
    lv_obj_set_style_border_width(ui->screen_user_detail_label_4, 0, 0);
    lv_obj_set_style_radius(ui->screen_user_detail_label_4, 0, 0);
    lv_obj_set_style_text_color(ui->screen_user_detail_label_4, lv_color_hex(0x8899aa), 0);
    lv_obj_set_style_text_font(ui->screen_user_detail_label_4, &lv_font_montserratMedium_12, 0);
    lv_obj_set_style_text_opa(ui->screen_user_detail_label_4, 255, 0);
    lv_obj_set_style_text_align(ui->screen_user_detail_label_4, LV_TEXT_ALIGN_LEFT, 0);
    lv_obj_set_style_bg_opa(ui->screen_user_detail_label_4, 0, 0);
    lv_obj_set_style_pad_all(ui->screen_user_detail_label_4, 0, 0);
    lv_obj_set_style_shadow_width(ui->screen_user_detail_label_4, 0, 0);

    /* 时间数值 (白色) */
    ui->screen_user_detail_label_time = lv_label_create(ui->screen_user_detail_cont_1);
    lv_label_set_text(ui->screen_user_detail_label_time, "2026\xE5\xB9\xB4" "4\xE6\x9C\x88" "23\xE6\x97\xA5 13:51:24");  /* 2026年4月23日 13:51:24 */
    lv_label_set_long_mode(ui->screen_user_detail_label_time, LV_LABEL_LONG_WRAP);
    lv_obj_set_pos(ui->screen_user_detail_label_time, 94, 5);
    lv_obj_set_size(ui->screen_user_detail_label_time, 280, 19);
    lv_obj_set_style_border_width(ui->screen_user_detail_label_time, 0, 0);
    lv_obj_set_style_radius(ui->screen_user_detail_label_time, 0, 0);
    lv_obj_set_style_text_color(ui->screen_user_detail_label_time, lv_color_hex(0xffffff), 0);
    lv_obj_set_style_text_font(ui->screen_user_detail_label_time, &lv_font_montserratMedium_12, 0);
    lv_obj_set_style_text_opa(ui->screen_user_detail_label_time, 255, 0);
    lv_obj_set_style_text_align(ui->screen_user_detail_label_time, LV_TEXT_ALIGN_LEFT, 0);
    lv_obj_set_style_bg_opa(ui->screen_user_detail_label_time, 0, 0);
    lv_obj_set_style_pad_all(ui->screen_user_detail_label_time, 0, 0);
    lv_obj_set_style_shadow_width(ui->screen_user_detail_label_time, 0, 0);

    /*The custom code of screen_user_detail.*/

    /*Update current screen layout.*/
    lv_obj_update_layout(ui->screen_user_detail);

    /*Init events for screen.*/
    events_init_screen_user_detail(ui);
}
