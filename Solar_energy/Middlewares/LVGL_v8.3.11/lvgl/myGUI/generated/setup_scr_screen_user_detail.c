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
    lv_obj_set_size(ui->screen_user_detail, 480, 320);
    lv_obj_set_scrollbar_mode(ui->screen_user_detail, LV_SCROLLBAR_MODE_OFF);

    //Write style for screen_user_detail, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_bg_opa(ui->screen_user_detail, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes screen_user_detail_label_user
    ui->screen_user_detail_label_user = lv_label_create(ui->screen_user_detail);
    lv_label_set_text(ui->screen_user_detail_label_user, "用户：--");
    lv_label_set_long_mode(ui->screen_user_detail_label_user, LV_LABEL_LONG_WRAP);
    lv_obj_set_pos(ui->screen_user_detail_label_user, 171, 0);
    lv_obj_set_size(ui->screen_user_detail_label_user, 100, 19);

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

    //Write codes screen_user_detail_table_1
    ui->screen_user_detail_table_1 = lv_table_create(ui->screen_user_detail);
    lv_table_set_col_cnt(ui->screen_user_detail_table_1,2);
    lv_table_set_row_cnt(ui->screen_user_detail_table_1,4);
    lv_table_set_cell_value(ui->screen_user_detail_table_1,0,0,"Name");
    lv_table_set_cell_value(ui->screen_user_detail_table_1,1,0,"温度 ");
    lv_table_set_cell_value(ui->screen_user_detail_table_1,2,0,"功率 ");
    lv_table_set_cell_value(ui->screen_user_detail_table_1,3,0,"用电量 ");
    lv_table_set_cell_value(ui->screen_user_detail_table_1,0,1,"Value");
    lv_table_set_cell_value(ui->screen_user_detail_table_1,1,1,"50℃ ");
    lv_table_set_cell_value(ui->screen_user_detail_table_1,2,1,"100w");
    lv_table_set_cell_value(ui->screen_user_detail_table_1,3,1,"100kWh");
    lv_obj_set_pos(ui->screen_user_detail_table_1, 103, 74);
    lv_obj_set_scrollbar_mode(ui->screen_user_detail_table_1, LV_SCROLLBAR_MODE_OFF);

    //Write style for screen_user_detail_table_1, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_pad_top(ui->screen_user_detail_table_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->screen_user_detail_table_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->screen_user_detail_table_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->screen_user_detail_table_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->screen_user_detail_table_1, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui->screen_user_detail_table_1, lv_color_hex(0xffffff), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(ui->screen_user_detail_table_1, LV_GRAD_DIR_NONE, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui->screen_user_detail_table_1, 2, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(ui->screen_user_detail_table_1, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ui->screen_user_detail_table_1, lv_color_hex(0xd5dee6), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_border_side(ui->screen_user_detail_table_1, LV_BORDER_SIDE_FULL, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_user_detail_table_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->screen_user_detail_table_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write style for screen_user_detail_table_1, Part: LV_PART_ITEMS, State: LV_STATE_DEFAULT.
    lv_obj_set_style_text_color(ui->screen_user_detail_table_1, lv_color_hex(0x393c41), LV_PART_ITEMS|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->screen_user_detail_table_1, &lv_font_SourceHanSerifSC_Regular_16, LV_PART_ITEMS|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->screen_user_detail_table_1, 255, LV_PART_ITEMS|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->screen_user_detail_table_1, LV_TEXT_ALIGN_CENTER, LV_PART_ITEMS|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->screen_user_detail_table_1, 0, LV_PART_ITEMS|LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui->screen_user_detail_table_1, 3, LV_PART_ITEMS|LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(ui->screen_user_detail_table_1, 255, LV_PART_ITEMS|LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ui->screen_user_detail_table_1, lv_color_hex(0xd5dee6), LV_PART_ITEMS|LV_STATE_DEFAULT);
    lv_obj_set_style_border_side(ui->screen_user_detail_table_1, LV_BORDER_SIDE_FULL, LV_PART_ITEMS|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->screen_user_detail_table_1, 10, LV_PART_ITEMS|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->screen_user_detail_table_1, 10, LV_PART_ITEMS|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->screen_user_detail_table_1, 10, LV_PART_ITEMS|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->screen_user_detail_table_1, 10, LV_PART_ITEMS|LV_STATE_DEFAULT);

    //The custom code of screen_user_detail.


    //Update current screen layout.
    lv_obj_update_layout(ui->screen_user_detail);

    //Init events for screen.
    events_init_screen_user_detail(ui);
}
