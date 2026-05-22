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


void setup_scr_screen_solar(lv_ui *ui)
{
    /* ======== 创建屏幕 ======== */
    ui->screen_solar = lv_obj_create(NULL);
    lv_obj_set_size(ui->screen_solar, 480, 320);
    lv_obj_set_scrollbar_mode(ui->screen_solar, LV_SCROLLBAR_MODE_OFF);

    /* 浅灰白背景 */
    lv_obj_set_style_bg_color(ui->screen_solar, lv_color_hex(0xF0F2F5), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->screen_solar, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);

    /* 顶部导航栏 */
    {
        lv_obj_t *header = lv_obj_create(ui->screen_solar);
        lv_obj_remove_style_all(header);
        lv_obj_set_size(header, 480, 42);
        lv_obj_set_pos(header, 0, 0);
        lv_obj_set_style_bg_color(header, lv_color_hex(0x2C3E50), 0);
        lv_obj_set_style_bg_opa(header, LV_OPA_COVER, 0);
        lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_style_pad_all(header, 0, 0);

        lv_obj_t *title_lbl = lv_label_create(header);
        lv_label_set_text(title_lbl, LV_SYMBOL_LEFT " 太阳能发电量");
        lv_obj_set_style_text_color(title_lbl, lv_color_hex(0xffffff), 0);
        lv_obj_set_style_text_font(title_lbl, &lv_font_SourceHanSerifSC_Regular_16, 0);
        lv_obj_set_style_bg_opa(title_lbl, 0, 0);
        lv_obj_set_pos(title_lbl, 14, 11);
    }

    /* 中间内容区 - 简单的文本标签 (最小化测试) */
    {
        lv_obj_t *info_label = lv_label_create(ui->screen_solar);
        lv_label_set_text(info_label, "太阳能发电量详情页\n\n总发电量: 12568.5 kWh\n年发电量: 3245.8 kWh\n月发电量: 256.4 kWh\n日发电量: 19.1 kWh");
        lv_obj_set_style_text_color(info_label, lv_color_hex(0x2C3E50), 0);
        lv_obj_set_style_text_font(info_label, &lv_font_SourceHanSerifSC_Regular_16, 0);
        lv_obj_set_style_bg_opa(info_label, 0, 0);
        lv_obj_set_pos(info_label, 20, 60);
    }

    /* 底部提示 */
    {
        lv_obj_t *hint = lv_label_create(ui->screen_solar);
        lv_label_set_text(hint, "按ESC返回");
        lv_obj_set_style_text_color(hint, lv_color_hex(0x888888), 0);
        lv_obj_set_style_text_font(hint, &lv_font_SourceHanSerifSC_Regular_16, 0);
        lv_obj_set_style_bg_opa(hint, 0, 0);
        lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -10);
    }

    lv_obj_update_layout(ui->screen_solar);
    events_init_screen_solar(ui);
}
