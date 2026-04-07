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



void setup_scr_screen_user_home(lv_ui *ui)
{
    //Write codes screen_user_home
    ui->screen_user_home = lv_obj_create(NULL);
    lv_obj_set_size(ui->screen_user_home, 480, 320);
    lv_obj_set_scrollbar_mode(ui->screen_user_home, LV_SCROLLBAR_MODE_OFF);

    //Write style for screen_user_home, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_bg_opa(ui->screen_user_home, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes screen_user_home_user_list_btn
    ui->screen_user_home_user_list_btn = lv_btn_create(ui->screen_user_home);
    ui->screen_user_home_user_list_btn_label = lv_label_create(ui->screen_user_home_user_list_btn);
    lv_label_set_text(ui->screen_user_home_user_list_btn_label, "用户列表");
    lv_label_set_long_mode(ui->screen_user_home_user_list_btn_label, LV_LABEL_LONG_WRAP);
    lv_obj_align(ui->screen_user_home_user_list_btn_label, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_pad_all(ui->screen_user_home_user_list_btn, 0, LV_STATE_DEFAULT);
    lv_obj_set_width(ui->screen_user_home_user_list_btn_label, LV_PCT(100));
    lv_obj_set_pos(ui->screen_user_home_user_list_btn, 179, 223);
    lv_obj_set_size(ui->screen_user_home_user_list_btn, 100, 50);

    //Write style for screen_user_home_user_list_btn, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_bg_opa(ui->screen_user_home_user_list_btn, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui->screen_user_home_user_list_btn, lv_color_hex(0x2195f6), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(ui->screen_user_home_user_list_btn, LV_GRAD_DIR_NONE, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui->screen_user_home_user_list_btn, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_user_home_user_list_btn, 5, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->screen_user_home_user_list_btn, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->screen_user_home_user_list_btn, lv_color_hex(0xffffff), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->screen_user_home_user_list_btn, &lv_font_SourceHanSerifSC_Regular_16, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->screen_user_home_user_list_btn, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->screen_user_home_user_list_btn, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes screen_user_home_cont_1
    ui->screen_user_home_cont_1 = lv_obj_create(ui->screen_user_home);
    lv_obj_set_pos(ui->screen_user_home_cont_1, 0, 0);
    lv_obj_set_size(ui->screen_user_home_cont_1, 134, 62);
    lv_obj_set_scrollbar_mode(ui->screen_user_home_cont_1, LV_SCROLLBAR_MODE_OFF);

    //Write style for screen_user_home_cont_1, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_border_width(ui->screen_user_home_cont_1, 2, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(ui->screen_user_home_cont_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ui->screen_user_home_cont_1, lv_color_hex(0xffffff), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_border_side(ui->screen_user_home_cont_1, LV_BORDER_SIDE_FULL, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_user_home_cont_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->screen_user_home_cont_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->screen_user_home_cont_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->screen_user_home_cont_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->screen_user_home_cont_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->screen_user_home_cont_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->screen_user_home_cont_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes screen_user_home_label_Date
    ui->screen_user_home_label_Date = lv_label_create(ui->screen_user_home_cont_1);
    lv_label_set_text(ui->screen_user_home_label_Date, "2026-03-12");
    lv_label_set_long_mode(ui->screen_user_home_label_Date, LV_LABEL_LONG_WRAP);
    lv_obj_set_pos(ui->screen_user_home_label_Date, 10, 29);
    lv_obj_set_size(ui->screen_user_home_label_Date, 104, 15);

    //Write style for screen_user_home_label_Date, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_border_width(ui->screen_user_home_label_Date, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_user_home_label_Date, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->screen_user_home_label_Date, lv_color_hex(0x000000), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->screen_user_home_label_Date, &lv_font_montserratMedium_16, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->screen_user_home_label_Date, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(ui->screen_user_home_label_Date, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_line_space(ui->screen_user_home_label_Date, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->screen_user_home_label_Date, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->screen_user_home_label_Date, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->screen_user_home_label_Date, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->screen_user_home_label_Date, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->screen_user_home_label_Date, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->screen_user_home_label_Date, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->screen_user_home_label_Date, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes screen_user_home_label_Time
    ui->screen_user_home_label_Time = lv_label_create(ui->screen_user_home_cont_1);
    lv_label_set_text(ui->screen_user_home_label_Time, "14:49:57");
    lv_label_set_long_mode(ui->screen_user_home_label_Time, LV_LABEL_LONG_WRAP);
    lv_obj_set_pos(ui->screen_user_home_label_Time, 17, 10);
    lv_obj_set_size(ui->screen_user_home_label_Time, 88, 15);

    //Write style for screen_user_home_label_Time, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_border_width(ui->screen_user_home_label_Time, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_user_home_label_Time, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->screen_user_home_label_Time, lv_color_hex(0x000000), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->screen_user_home_label_Time, &lv_font_montserratMedium_16, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->screen_user_home_label_Time, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(ui->screen_user_home_label_Time, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_line_space(ui->screen_user_home_label_Time, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->screen_user_home_label_Time, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->screen_user_home_label_Time, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->screen_user_home_label_Time, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->screen_user_home_label_Time, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->screen_user_home_label_Time, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->screen_user_home_label_Time, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->screen_user_home_label_Time, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes screen_user_home_img_1
    ui->screen_user_home_img_1 = lv_img_create(ui->screen_user_home);
    lv_obj_add_flag(ui->screen_user_home_img_1, LV_OBJ_FLAG_CLICKABLE);
    lv_img_set_src(ui->screen_user_home_img_1, &_logo_shunpu_alpha_338x72);
    lv_img_set_pivot(ui->screen_user_home_img_1, 50,50);
    lv_img_set_angle(ui->screen_user_home_img_1, 0);
    lv_obj_set_pos(ui->screen_user_home_img_1, 67, 96);
    lv_obj_set_size(ui->screen_user_home_img_1, 338, 72);

    //Write style for screen_user_home_img_1, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_img_recolor_opa(ui->screen_user_home_img_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_img_opa(ui->screen_user_home_img_1, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_user_home_img_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_clip_corner(ui->screen_user_home_img_1, true, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes screen_user_home_cont_2
    ui->screen_user_home_cont_2 = lv_obj_create(ui->screen_user_home);
    lv_obj_set_pos(ui->screen_user_home_cont_2, 0, 257);
    lv_obj_set_size(ui->screen_user_home_cont_2, 114, 62);
    lv_obj_set_scrollbar_mode(ui->screen_user_home_cont_2, LV_SCROLLBAR_MODE_OFF);

    //Write style for screen_user_home_cont_2, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_border_width(ui->screen_user_home_cont_2, 2, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(ui->screen_user_home_cont_2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ui->screen_user_home_cont_2, lv_color_hex(0xffffff), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_border_side(ui->screen_user_home_cont_2, LV_BORDER_SIDE_FULL, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_user_home_cont_2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->screen_user_home_cont_2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->screen_user_home_cont_2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->screen_user_home_cont_2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->screen_user_home_cont_2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->screen_user_home_cont_2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->screen_user_home_cont_2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes screen_user_home_label_2
    ui->screen_user_home_label_2 = lv_label_create(ui->screen_user_home_cont_2);
    lv_label_set_text(ui->screen_user_home_label_2, "白昼");
    lv_label_set_long_mode(ui->screen_user_home_label_2, LV_LABEL_LONG_WRAP);
    lv_obj_set_pos(ui->screen_user_home_label_2, 14, 31);
    lv_obj_set_size(ui->screen_user_home_label_2, 78, 15);

    //Write style for screen_user_home_label_2, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_border_width(ui->screen_user_home_label_2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_user_home_label_2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->screen_user_home_label_2, lv_color_hex(0x000000), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->screen_user_home_label_2, &lv_font_SourceHanSerifSC_Regular_16, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->screen_user_home_label_2, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(ui->screen_user_home_label_2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_line_space(ui->screen_user_home_label_2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->screen_user_home_label_2, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->screen_user_home_label_2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->screen_user_home_label_2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->screen_user_home_label_2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->screen_user_home_label_2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->screen_user_home_label_2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->screen_user_home_label_2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes screen_user_home_label_1
    ui->screen_user_home_label_1 = lv_label_create(ui->screen_user_home_cont_2);
    lv_label_set_text(ui->screen_user_home_label_1, "天气");
    lv_label_set_long_mode(ui->screen_user_home_label_1, LV_LABEL_LONG_WRAP);
    lv_obj_set_pos(ui->screen_user_home_label_1, 15, 10);
    lv_obj_set_size(ui->screen_user_home_label_1, 78, 15);

    //Write style for screen_user_home_label_1, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_border_width(ui->screen_user_home_label_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_user_home_label_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->screen_user_home_label_1, lv_color_hex(0x000000), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->screen_user_home_label_1, &lv_font_SourceHanSerifSC_Regular_16, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->screen_user_home_label_1, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(ui->screen_user_home_label_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_line_space(ui->screen_user_home_label_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->screen_user_home_label_1, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->screen_user_home_label_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->screen_user_home_label_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->screen_user_home_label_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->screen_user_home_label_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->screen_user_home_label_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->screen_user_home_label_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes screen_user_home_cont_Signal
    ui->screen_user_home_cont_Signal = lv_obj_create(ui->screen_user_home);
    lv_obj_set_pos(ui->screen_user_home_cont_Signal, 429, 0);
    lv_obj_set_size(ui->screen_user_home_cont_Signal, 44, 50);
    lv_obj_set_scrollbar_mode(ui->screen_user_home_cont_Signal, LV_SCROLLBAR_MODE_OFF);

    //Write style for screen_user_home_cont_Signal, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_border_width(ui->screen_user_home_cont_Signal, 2, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(ui->screen_user_home_cont_Signal, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ui->screen_user_home_cont_Signal, lv_color_hex(0xffffff), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_border_side(ui->screen_user_home_cont_Signal, LV_BORDER_SIDE_FULL, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_user_home_cont_Signal, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->screen_user_home_cont_Signal, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->screen_user_home_cont_Signal, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->screen_user_home_cont_Signal, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->screen_user_home_cont_Signal, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->screen_user_home_cont_Signal, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->screen_user_home_cont_Signal, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes screen_user_home_line_1
    ui->screen_user_home_line_1 = lv_line_create(ui->screen_user_home_cont_Signal);
    static lv_point_t screen_user_home_line_1[] = {{0, 0},{0, 60},};
    lv_line_set_points(ui->screen_user_home_line_1, screen_user_home_line_1, 2);
    lv_obj_set_pos(ui->screen_user_home_line_1, 10, 24);
    lv_obj_set_size(ui->screen_user_home_line_1, 1, 10);

    //Write style for screen_user_home_line_1, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_line_width(ui->screen_user_home_line_1, 5, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_line_color(ui->screen_user_home_line_1, lv_color_hex(0x757575), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_line_opa(ui->screen_user_home_line_1, 131, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_line_rounded(ui->screen_user_home_line_1, true, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes screen_user_home_line_2
    ui->screen_user_home_line_2 = lv_line_create(ui->screen_user_home_cont_Signal);
    static lv_point_t screen_user_home_line_2[] = {{0, 0},{0, 60},};
    lv_line_set_points(ui->screen_user_home_line_2, screen_user_home_line_2, 2);
    lv_obj_set_pos(ui->screen_user_home_line_2, 20, 15);
    lv_obj_set_size(ui->screen_user_home_line_2, 1, 20);

    //Write style for screen_user_home_line_2, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_line_width(ui->screen_user_home_line_2, 5, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_line_color(ui->screen_user_home_line_2, lv_color_hex(0x757575), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_line_opa(ui->screen_user_home_line_2, 136, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_line_rounded(ui->screen_user_home_line_2, true, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes screen_user_home_line_3
    ui->screen_user_home_line_3 = lv_line_create(ui->screen_user_home_cont_Signal);
    static lv_point_t screen_user_home_line_3[] = {{0, 0},{0, 60},};
    lv_line_set_points(ui->screen_user_home_line_3, screen_user_home_line_3, 2);
    lv_obj_set_pos(ui->screen_user_home_line_3, 30, 5);
    lv_obj_set_size(ui->screen_user_home_line_3, 1, 30);

    //Write style for screen_user_home_line_3, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_line_width(ui->screen_user_home_line_3, 5, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_line_color(ui->screen_user_home_line_3, lv_color_hex(0x757575), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_line_opa(ui->screen_user_home_line_3, 130, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_line_rounded(ui->screen_user_home_line_3, true, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes screen_user_home_label_3
    ui->screen_user_home_label_3 = lv_label_create(ui->screen_user_home_cont_Signal);
    lv_label_set_text(ui->screen_user_home_label_3, "X");
    lv_label_set_long_mode(ui->screen_user_home_label_3, LV_LABEL_LONG_WRAP);
    lv_obj_set_pos(ui->screen_user_home_label_3, 8, 14);
    lv_obj_set_size(ui->screen_user_home_label_3, 25, 26);
    lv_obj_add_flag(ui->screen_user_home_label_3, LV_OBJ_FLAG_HIDDEN);

    //Write style for screen_user_home_label_3, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_border_width(ui->screen_user_home_label_3, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_user_home_label_3, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->screen_user_home_label_3, lv_color_hex(0xf00000), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->screen_user_home_label_3, &lv_font_montserratMedium_26, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->screen_user_home_label_3, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(ui->screen_user_home_label_3, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_line_space(ui->screen_user_home_label_3, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->screen_user_home_label_3, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->screen_user_home_label_3, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->screen_user_home_label_3, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->screen_user_home_label_3, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->screen_user_home_label_3, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->screen_user_home_label_3, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->screen_user_home_label_3, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //The custom code of screen_user_home.


    //Update current screen layout.
    lv_obj_update_layout(ui->screen_user_home);

    //Init events for screen.
    events_init_screen_user_home(ui);
}
