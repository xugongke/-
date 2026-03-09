/*
* Copyright 2026 NXP
* NXP Proprietary. This software is owned or controlled by NXP and may only be used strictly in
* accordance with the applicable license terms. By expressly accepting such terms or by downloading, installing,
* activating and/or otherwise using the software, you are agreeing that you have read, and that you agree to
* comply with and are bound by, such license terms.  If you do not agree to be bound by the applicable license
* terms, then you may not retain, install, activate or otherwise use the software.
*/

#include "lvgl.h"
#include <stdio.h>
#include "gui_guider.h"
#include "events_init.h"
#include "widgets_init.h"
#include "custom.h"



void setup_scr_screen_user_detail(lv_ui *ui)
{
    //Write codes screen_user_detail
    ui->screen_user_detail = lv_obj_create(NULL);
    lv_obj_set_size(ui->screen_user_detail, 480, 272);
    lv_obj_set_scrollbar_mode(ui->screen_user_detail, LV_SCROLLBAR_MODE_OFF);

    //Write style for screen_user_detail, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_bg_opa(ui->screen_user_detail, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes screen_user_detail_label_user
    ui->screen_user_detail_label_user = lv_label_create(ui->screen_user_detail);
    lv_label_set_text(ui->screen_user_detail_label_user, "用户：--");
    lv_label_set_long_mode(ui->screen_user_detail_label_user, LV_LABEL_LONG_WRAP);
    lv_obj_set_pos(ui->screen_user_detail_label_user, 173, 11);
    lv_obj_set_size(ui->screen_user_detail_label_user, 102, 22);

    //Write style for screen_user_detail_label_user, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_border_width(ui->screen_user_detail_label_user, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_user_detail_label_user, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->screen_user_detail_label_user, lv_color_hex(0x000000), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->screen_user_detail_label_user, &lv_font_SourceHanSerifSC_Regular_16, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->screen_user_detail_label_user, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(ui->screen_user_detail_label_user, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_line_space(ui->screen_user_detail_label_user, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->screen_user_detail_label_user, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->screen_user_detail_label_user, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->screen_user_detail_label_user, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->screen_user_detail_label_user, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->screen_user_detail_label_user, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->screen_user_detail_label_user, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->screen_user_detail_label_user, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes screen_user_detail_label_day
    ui->screen_user_detail_label_day = lv_label_create(ui->screen_user_detail);
    lv_label_set_text(ui->screen_user_detail_label_day, "日：I=-- A V=-- V");
    lv_label_set_long_mode(ui->screen_user_detail_label_day, LV_LABEL_LONG_WRAP);
    lv_obj_set_pos(ui->screen_user_detail_label_day, 154, 57);
    lv_obj_set_size(ui->screen_user_detail_label_day, 223, 32);

    //Write style for screen_user_detail_label_day, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_border_width(ui->screen_user_detail_label_day, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_user_detail_label_day, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->screen_user_detail_label_day, lv_color_hex(0x000000), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->screen_user_detail_label_day, &lv_font_SourceHanSerifSC_Regular_16, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->screen_user_detail_label_day, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(ui->screen_user_detail_label_day, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_line_space(ui->screen_user_detail_label_day, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->screen_user_detail_label_day, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->screen_user_detail_label_day, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->screen_user_detail_label_day, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->screen_user_detail_label_day, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->screen_user_detail_label_day, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->screen_user_detail_label_day, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->screen_user_detail_label_day, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes screen_user_detail_label_month
    ui->screen_user_detail_label_month = lv_label_create(ui->screen_user_detail);
    lv_label_set_text(ui->screen_user_detail_label_month, "月：I=-- A V=-- V");
    lv_label_set_long_mode(ui->screen_user_detail_label_month, LV_LABEL_LONG_WRAP);
    lv_obj_set_pos(ui->screen_user_detail_label_month, 154, 122);
    lv_obj_set_size(ui->screen_user_detail_label_month, 217, 32);

    //Write style for screen_user_detail_label_month, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_border_width(ui->screen_user_detail_label_month, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_user_detail_label_month, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->screen_user_detail_label_month, lv_color_hex(0x000000), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->screen_user_detail_label_month, &lv_font_SourceHanSerifSC_Regular_16, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->screen_user_detail_label_month, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(ui->screen_user_detail_label_month, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_line_space(ui->screen_user_detail_label_month, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->screen_user_detail_label_month, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->screen_user_detail_label_month, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->screen_user_detail_label_month, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->screen_user_detail_label_month, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->screen_user_detail_label_month, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->screen_user_detail_label_month, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->screen_user_detail_label_month, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes screen_user_detail_label_year
    ui->screen_user_detail_label_year = lv_label_create(ui->screen_user_detail);
    lv_label_set_text(ui->screen_user_detail_label_year, "年：I=-- A V=-- V");
    lv_label_set_long_mode(ui->screen_user_detail_label_year, LV_LABEL_LONG_WRAP);
    lv_obj_set_pos(ui->screen_user_detail_label_year, 154, 187);
    lv_obj_set_size(ui->screen_user_detail_label_year, 223, 32);

    //Write style for screen_user_detail_label_year, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_border_width(ui->screen_user_detail_label_year, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_user_detail_label_year, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->screen_user_detail_label_year, lv_color_hex(0x000000), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->screen_user_detail_label_year, &lv_font_SourceHanSerifSC_Regular_16, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->screen_user_detail_label_year, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(ui->screen_user_detail_label_year, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_line_space(ui->screen_user_detail_label_year, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->screen_user_detail_label_year, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->screen_user_detail_label_year, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->screen_user_detail_label_year, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->screen_user_detail_label_year, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->screen_user_detail_label_year, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->screen_user_detail_label_year, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->screen_user_detail_label_year, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //The custom code of screen_user_detail.


    //Update current screen layout.
    lv_obj_update_layout(ui->screen_user_detail);

    //Init events for screen.
    events_init_screen_user_detail(ui);
}
