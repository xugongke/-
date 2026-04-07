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



void setup_scr_Startup_screen(lv_ui *ui)
{
    //Write codes Startup_screen
    ui->Startup_screen = lv_obj_create(NULL);
    lv_obj_set_size(ui->Startup_screen, 480, 320);
    lv_obj_set_scrollbar_mode(ui->Startup_screen, LV_SCROLLBAR_MODE_OFF);

    //Write style for Startup_screen, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_bg_opa(ui->Startup_screen, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes Startup_screen_img_1
    ui->Startup_screen_img_1 = lv_img_create(ui->Startup_screen);
    lv_obj_add_flag(ui->Startup_screen_img_1, LV_OBJ_FLAG_CLICKABLE);
    lv_img_set_src(ui->Startup_screen_img_1, &_logo_shunpu_alpha_338x72);
    lv_img_set_pivot(ui->Startup_screen_img_1, 50,50);
    lv_img_set_angle(ui->Startup_screen_img_1, 0);
    lv_obj_set_pos(ui->Startup_screen_img_1, 70, 123);
    lv_obj_set_size(ui->Startup_screen_img_1, 338, 72);

    //Write style for Startup_screen_img_1, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_img_recolor_opa(ui->Startup_screen_img_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_img_opa(ui->Startup_screen_img_1, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->Startup_screen_img_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_clip_corner(ui->Startup_screen_img_1, true, LV_PART_MAIN|LV_STATE_DEFAULT);

    //The custom code of Startup_screen.


    //Update current screen layout.
    lv_obj_update_layout(ui->Startup_screen);

}
