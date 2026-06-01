/*
* Copyright 2026 NXP
* NXP Proprietary. This software is owned or controlled by NXP and may only be used strictly in
* accordance with the applicable license terms. By expressly accepting such terms or by downloading, installing,
* activating and/or otherwise using the software, you are agreeing to you have read, and that you agree to
* comply with and be bound by, such license terms.  If you do not agree to be bound by the applicable license
* terms, then you may not retain, install, activate or otherwise use the software.
 *
 * Dark Theme Home Screen
 * ======================
 * Color scheme matches the SUNPOEM startup animation:
 *   Background: #0D1117  (deep dark)
 *   Cards:      #161B22  (dark surface)
 *   Headers:    #1C2333  (navy bar)
 *   Accent:     #F0C040  (golden)
 *   Solar:      #58A6FF  (bright blue)
 *   Device:     #3FB950  (bright green)
 *   Alert:      #D29922  (warm amber)
 *   Text:       #E6EDF3  (primary), #8B949E (secondary)
 */

#include "lvgl.h"
#include <stdio.h>
#include "gui_guider.h"
#include "weather_icons.h"
#include "events_init.h"
#include "widgets_init.h"
#include "custom.h"
#include "battery.h"


void setup_scr_screen_user_home(lv_ui *ui)
{
    /* ======== 创建屏幕 ======== */
    ui->screen_user_home = lv_obj_create(NULL);
    lv_obj_set_size(ui->screen_user_home, 480, 320);
    lv_obj_set_scrollbar_mode(ui->screen_user_home, LV_SCROLLBAR_MODE_OFF);

    /* 深色背景 */
    lv_obj_set_style_bg_color(ui->screen_user_home, lv_color_hex(0x0D1117), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->screen_user_home, LV_OPA_COVER, LV_PART_MAIN|LV_STATE_DEFAULT);

    /* ============================================================
     *  3个数据卡片 (太阳能 / 设备在线 / 告警)
     *  暗色卡片 + 金色左侧条 + 暗色数值区域
     *  卡片: 140x100, 间距10, 总宽 440, 居中偏移 20, y: 155
     * ============================================================ */

    /* 太阳能卡片 - 蓝色主题 */
    ui->screen_user_home_card_solar = lv_obj_create(ui->screen_user_home);
    lv_obj_set_size(ui->screen_user_home_card_solar, 140, 100);
    lv_obj_set_pos(ui->screen_user_home_card_solar, 20, 155);
    lv_obj_set_style_radius(ui->screen_user_home_card_solar, 12, 0);
    lv_obj_set_style_bg_color(ui->screen_user_home_card_solar, lv_color_hex(0x161B22), 0);
    lv_obj_set_style_bg_opa(ui->screen_user_home_card_solar, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(ui->screen_user_home_card_solar, 1, 0);
    lv_obj_set_style_border_color(ui->screen_user_home_card_solar, lv_color_hex(0x30363D), 0);
    lv_obj_set_style_border_opa(ui->screen_user_home_card_solar, LV_OPA_COVER, 0);
    lv_obj_set_style_shadow_width(ui->screen_user_home_card_solar, 10, 0);
    lv_obj_set_style_shadow_color(ui->screen_user_home_card_solar, lv_color_hex(0x58A6FF), 0);
    lv_obj_set_style_shadow_opa(ui->screen_user_home_card_solar, 30, 0);
    lv_obj_set_style_shadow_ofs_y(ui->screen_user_home_card_solar, 3, 0);
    lv_obj_set_style_shadow_spread(ui->screen_user_home_card_solar, 0, 0);
    lv_obj_set_style_pad_all(ui->screen_user_home_card_solar, 0, 0);
    lv_obj_clear_flag(ui->screen_user_home_card_solar, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(ui->screen_user_home_card_solar, LV_OBJ_FLAG_CLICKABLE);
    {
        /* 头部: 蓝色区域 + 图标+标题 */
        lv_obj_t *header = lv_obj_create(ui->screen_user_home_card_solar);
        lv_obj_remove_style_all(header);
        lv_obj_set_size(header, 138, 32);
        lv_obj_set_pos(header, 1, 1);
        lv_obj_set_style_radius(header, 11, 0);
        lv_obj_set_style_bg_color(header, lv_color_hex(0x58A6FF), 0);
        lv_obj_set_style_bg_opa(header, LV_OPA_COVER, 0);
        lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t *icon_lbl = lv_label_create(header);
        lv_label_set_text(icon_lbl, LV_SYMBOL_CHARGE);
        lv_obj_set_style_text_color(icon_lbl, lv_color_hex(0xffffff), 0);
        lv_obj_set_style_text_font(icon_lbl, &lv_font_SourceHanSerifSC_Regular_16, 0);
        lv_obj_set_style_bg_opa(icon_lbl, 0, 0);
        lv_obj_set_pos(icon_lbl, 10, 6);

        lv_obj_t *title_lbl = lv_label_create(header);
        lv_label_set_text(title_lbl, "太阳能 ");
        lv_obj_set_style_text_color(title_lbl, lv_color_hex(0xffffff), 0);
        lv_obj_set_style_text_font(title_lbl, &lv_font_SourceHanSerifSC_Regular_16, 0);
        lv_obj_set_style_bg_opa(title_lbl, 0, 0);
        lv_obj_set_pos(title_lbl, 30, 6);

        /* 暗色数值区域 */
        lv_obj_t *body = lv_obj_create(ui->screen_user_home_card_solar);
        lv_obj_remove_style_all(body);
        lv_obj_set_size(body, 138, 64);
        lv_obj_set_pos(body, 1, 33);
        lv_obj_set_style_radius(body, 0, 0);
        lv_obj_set_style_bg_color(body, lv_color_hex(0x161B22), 0);
        lv_obj_set_style_bg_opa(body, LV_OPA_COVER, 0);
        lv_obj_clear_flag(body, LV_OBJ_FLAG_SCROLLABLE);

        ui->screen_user_home_card_solar_val = lv_label_create(body);
        lv_label_set_text(ui->screen_user_home_card_solar_val, "0.0V");
        lv_obj_set_style_text_color(ui->screen_user_home_card_solar_val, lv_color_hex(0x58A6FF), 0);
        lv_obj_set_style_text_font(ui->screen_user_home_card_solar_val, &lv_font_SourceHanSerifSC_Regular_16, 0);
        lv_obj_set_style_text_opa(ui->screen_user_home_card_solar_val, 255, 0);
        lv_obj_set_style_bg_opa(ui->screen_user_home_card_solar_val, 0, 0);
        lv_obj_set_style_text_align(ui->screen_user_home_card_solar_val, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_align(ui->screen_user_home_card_solar_val, LV_ALIGN_CENTER, 0, 2);
        lv_obj_clear_flag(ui->screen_user_home_card_solar_val, LV_OBJ_FLAG_SCROLLABLE);
    }

    /* 设备在线卡片 - 绿色主题 */
    ui->screen_user_home_card_device = lv_obj_create(ui->screen_user_home);
    lv_obj_set_size(ui->screen_user_home_card_device, 140, 100);
    lv_obj_set_pos(ui->screen_user_home_card_device, 170, 155);
    lv_obj_set_style_radius(ui->screen_user_home_card_device, 12, 0);
    lv_obj_set_style_bg_color(ui->screen_user_home_card_device, lv_color_hex(0x161B22), 0);
    lv_obj_set_style_bg_opa(ui->screen_user_home_card_device, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(ui->screen_user_home_card_device, 1, 0);
    lv_obj_set_style_border_color(ui->screen_user_home_card_device, lv_color_hex(0x30363D), 0);
    lv_obj_set_style_border_opa(ui->screen_user_home_card_device, LV_OPA_COVER, 0);
    lv_obj_set_style_shadow_width(ui->screen_user_home_card_device, 10, 0);
    lv_obj_set_style_shadow_color(ui->screen_user_home_card_device, lv_color_hex(0x3FB950), 0);
    lv_obj_set_style_shadow_opa(ui->screen_user_home_card_device, 30, 0);
    lv_obj_set_style_shadow_ofs_y(ui->screen_user_home_card_device, 3, 0);
    lv_obj_set_style_shadow_spread(ui->screen_user_home_card_device, 0, 0);
    lv_obj_set_style_pad_all(ui->screen_user_home_card_device, 0, 0);
    lv_obj_clear_flag(ui->screen_user_home_card_device, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(ui->screen_user_home_card_device, LV_OBJ_FLAG_CLICKABLE);
    {
        lv_obj_t *header = lv_obj_create(ui->screen_user_home_card_device);
        lv_obj_remove_style_all(header);
        lv_obj_set_size(header, 138, 32);
        lv_obj_set_pos(header, 1, 1);
        lv_obj_set_style_radius(header, 11, 0);
        lv_obj_set_style_bg_color(header, lv_color_hex(0x3FB950), 0);
        lv_obj_set_style_bg_opa(header, LV_OPA_COVER, 0);
        lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t *icon_lbl = lv_label_create(header);
        lv_label_set_text(icon_lbl, LV_SYMBOL_WIFI);
        lv_obj_set_style_text_color(icon_lbl, lv_color_hex(0xffffff), 0);
        lv_obj_set_style_text_font(icon_lbl, &lv_font_SourceHanSerifSC_Regular_16, 0);
        lv_obj_set_style_bg_opa(icon_lbl, 0, 0);
        lv_obj_set_pos(icon_lbl, 10, 6);

        lv_obj_t *title_lbl = lv_label_create(header);
        lv_label_set_text(title_lbl, "设备在线");
        lv_obj_set_style_text_color(title_lbl, lv_color_hex(0xffffff), 0);
        lv_obj_set_style_text_font(title_lbl, &lv_font_SourceHanSerifSC_Regular_16, 0);
        lv_obj_set_style_bg_opa(title_lbl, 0, 0);
        lv_obj_set_pos(title_lbl, 30, 6);

        lv_obj_t *body = lv_obj_create(ui->screen_user_home_card_device);
        lv_obj_remove_style_all(body);
        lv_obj_set_size(body, 138, 64);
        lv_obj_set_pos(body, 1, 33);
        lv_obj_set_style_radius(body, 0, 0);
        lv_obj_set_style_bg_color(body, lv_color_hex(0x161B22), 0);
        lv_obj_set_style_bg_opa(body, LV_OPA_COVER, 0);
        lv_obj_clear_flag(body, LV_OBJ_FLAG_SCROLLABLE);

        ui->screen_user_home_card_device_val = lv_label_create(body);
        lv_label_set_text(ui->screen_user_home_card_device_val, "0 / 0");
        lv_obj_set_style_text_color(ui->screen_user_home_card_device_val, lv_color_hex(0x3FB950), 0);
        lv_obj_set_style_text_font(ui->screen_user_home_card_device_val, &lv_font_SourceHanSerifSC_Regular_16, 0);
        lv_obj_set_style_text_opa(ui->screen_user_home_card_device_val, 255, 0);
        lv_obj_set_style_bg_opa(ui->screen_user_home_card_device_val, 0, 0);
        lv_obj_align(ui->screen_user_home_card_device_val, LV_ALIGN_CENTER, 0, 2);
        lv_obj_clear_flag(ui->screen_user_home_card_device_val, LV_OBJ_FLAG_SCROLLABLE);
    }

    /* 告警卡片 - 橙色主题 */
    ui->screen_user_home_card_alert = lv_obj_create(ui->screen_user_home);
    lv_obj_set_size(ui->screen_user_home_card_alert, 140, 100);
    lv_obj_set_pos(ui->screen_user_home_card_alert, 320, 155);
    lv_obj_set_style_radius(ui->screen_user_home_card_alert, 12, 0);
    lv_obj_set_style_bg_color(ui->screen_user_home_card_alert, lv_color_hex(0x161B22), 0);
    lv_obj_set_style_bg_opa(ui->screen_user_home_card_alert, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(ui->screen_user_home_card_alert, 1, 0);
    lv_obj_set_style_border_color(ui->screen_user_home_card_alert, lv_color_hex(0x30363D), 0);
    lv_obj_set_style_border_opa(ui->screen_user_home_card_alert, LV_OPA_COVER, 0);
    lv_obj_set_style_shadow_width(ui->screen_user_home_card_alert, 10, 0);
    lv_obj_set_style_shadow_color(ui->screen_user_home_card_alert, lv_color_hex(0xD29922), 0);
    lv_obj_set_style_shadow_opa(ui->screen_user_home_card_alert, 30, 0);
    lv_obj_set_style_shadow_ofs_y(ui->screen_user_home_card_alert, 3, 0);
    lv_obj_set_style_shadow_spread(ui->screen_user_home_card_alert, 0, 0);
    lv_obj_set_style_pad_all(ui->screen_user_home_card_alert, 0, 0);
    lv_obj_clear_flag(ui->screen_user_home_card_alert, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(ui->screen_user_home_card_alert, LV_OBJ_FLAG_CLICKABLE);
    {
        lv_obj_t *header = lv_obj_create(ui->screen_user_home_card_alert);
        lv_obj_remove_style_all(header);
        lv_obj_set_size(header, 138, 32);
        lv_obj_set_pos(header, 1, 1);
        lv_obj_set_style_radius(header, 11, 0);
        lv_obj_set_style_bg_color(header, lv_color_hex(0xD29922), 0);
        lv_obj_set_style_bg_opa(header, LV_OPA_COVER, 0);
        lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t *icon_lbl = lv_label_create(header);
        lv_label_set_text(icon_lbl, LV_SYMBOL_WARNING);
        lv_obj_set_style_text_color(icon_lbl, lv_color_hex(0xffffff), 0);
        lv_obj_set_style_text_font(icon_lbl, &lv_font_SourceHanSerifSC_Regular_16, 0);
        lv_obj_set_style_bg_opa(icon_lbl, 0, 0);
        lv_obj_set_pos(icon_lbl, 10, 6);

        lv_obj_t *title_lbl = lv_label_create(header);
        lv_label_set_text(title_lbl, "告警");
        lv_obj_set_style_text_color(title_lbl, lv_color_hex(0xffffff), 0);
        lv_obj_set_style_text_font(title_lbl, &lv_font_SourceHanSerifSC_Regular_16, 0);
        lv_obj_set_style_bg_opa(title_lbl, 0, 0);
        lv_obj_set_pos(title_lbl, 30, 6);

        lv_obj_t *body = lv_obj_create(ui->screen_user_home_card_alert);
        lv_obj_remove_style_all(body);
        lv_obj_set_size(body, 138, 64);
        lv_obj_set_pos(body, 1, 33);
        lv_obj_set_style_radius(body, 0, 0);
        lv_obj_set_style_bg_color(body, lv_color_hex(0x161B22), 0);
        lv_obj_set_style_bg_opa(body, LV_OPA_COVER, 0);
        lv_obj_clear_flag(body, LV_OBJ_FLAG_SCROLLABLE);

        ui->screen_user_home_card_alert_val = lv_label_create(body);
        lv_label_set_text(ui->screen_user_home_card_alert_val, "0");
        lv_obj_set_style_text_color(ui->screen_user_home_card_alert_val, lv_color_hex(0xD29922), 0);
        lv_obj_set_style_text_font(ui->screen_user_home_card_alert_val, &lv_font_SourceHanSerifSC_Regular_16, 0);
        lv_obj_set_style_text_opa(ui->screen_user_home_card_alert_val, 255, 0);
        lv_obj_set_style_bg_opa(ui->screen_user_home_card_alert_val, 0, 0);
        lv_obj_align(ui->screen_user_home_card_alert_val, LV_ALIGN_CENTER, 0, 2);
        lv_obj_clear_flag(ui->screen_user_home_card_alert_val, LV_OBJ_FLAG_SCROLLABLE);
    }

    /* ============================================================
     *  数码管风格数字时钟 (暗色卡片+金色字)
     * ============================================================ */

    ui->screen_user_home_cont_1 = lv_obj_create(ui->screen_user_home);
    lv_obj_remove_style_all(ui->screen_user_home_cont_1);
    lv_obj_set_pos(ui->screen_user_home_cont_1, 100, 40);
    lv_obj_set_size(ui->screen_user_home_cont_1, 280, 100);
    lv_obj_clear_flag(ui->screen_user_home_cont_1, LV_OBJ_FLAG_SCROLLABLE);

    #define DIGIT_CARD(var, x) do { \
        var = lv_obj_create(ui->screen_user_home_cont_1); \
        lv_obj_remove_style_all(var); \
        lv_obj_set_size(var, 48, 64); \
        lv_obj_set_pos(var, x, 0); \
        lv_obj_set_style_radius(var, 8, 0); \
        lv_obj_set_style_bg_color(var, lv_color_hex(0x161B22), 0); \
        lv_obj_set_style_bg_opa(var, LV_OPA_COVER, 0); \
        lv_obj_set_style_shadow_width(var, 6, 0); \
        lv_obj_set_style_shadow_color(var, lv_color_hex(0x000000), 0); \
        lv_obj_set_style_shadow_opa(var, 60, 0); \
        lv_obj_set_style_shadow_ofs_y(var, 2, 0); \
        lv_obj_set_style_border_width(var, 1, 0); \
        lv_obj_set_style_border_color(var, lv_color_hex(0x30363D), 0); \
        lv_obj_set_style_border_opa(var, 80, 0); \
        lv_obj_clear_flag(var, LV_OBJ_FLAG_SCROLLABLE); \
        lv_obj_t *_lbl = lv_label_create(var); \
        lv_label_set_text(_lbl, "0"); \
        lv_obj_set_style_text_color(_lbl, lv_color_hex(0xF0C040), 0); \
        lv_obj_set_style_text_font(_lbl, &lv_font_montserratMedium_48, 0); \
        lv_obj_align(_lbl, LV_ALIGN_CENTER, 0, -2); \
    } while(0)

    DIGIT_CARD(ui->screen_user_home_digit_h1, 30);
    DIGIT_CARD(ui->screen_user_home_digit_h2, 82);
    DIGIT_CARD(ui->screen_user_home_digit_m1, 150);
    DIGIT_CARD(ui->screen_user_home_digit_m2, 202);
    #undef DIGIT_CARD

    /* 冒号 (金色圆点) */
    ui->screen_user_home_digit_colon = lv_obj_create(ui->screen_user_home_cont_1);
    lv_obj_remove_style_all(ui->screen_user_home_digit_colon);
    lv_obj_set_size(ui->screen_user_home_digit_colon, 16, 64);
    lv_obj_set_pos(ui->screen_user_home_digit_colon, 134, 0);
    lv_obj_clear_flag(ui->screen_user_home_digit_colon, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t *dot_top = lv_obj_create(ui->screen_user_home_digit_colon);
    lv_obj_remove_style_all(dot_top);
    lv_obj_set_size(dot_top, 8, 8);
    lv_obj_align(dot_top, LV_ALIGN_TOP_MID, 0, 18);
    lv_obj_set_style_radius(dot_top, 4, 0);
    lv_obj_set_style_bg_color(dot_top, lv_color_hex(0xF0C040), 0);
    lv_obj_set_style_bg_opa(dot_top, LV_OPA_COVER, 0);
    lv_obj_t *dot_bot = lv_obj_create(ui->screen_user_home_digit_colon);
    lv_obj_remove_style_all(dot_bot);
    lv_obj_set_size(dot_bot, 8, 8);
    lv_obj_align(dot_bot, LV_ALIGN_TOP_MID, 0, 38);
    lv_obj_set_style_radius(dot_bot, 4, 0);
    lv_obj_set_style_bg_color(dot_bot, lv_color_hex(0xF0C040), 0);
    lv_obj_set_style_bg_opa(dot_bot, LV_OPA_COVER, 0);

    /* 日期标签 */
    ui->screen_user_home_label_Date = lv_label_create(ui->screen_user_home_cont_1);
    lv_label_set_text(ui->screen_user_home_label_Date, "2026-03-12");
    lv_obj_set_size(ui->screen_user_home_label_Date, 280, 20);
    lv_obj_set_pos(ui->screen_user_home_label_Date, 0, 74);
    lv_obj_set_style_text_color(ui->screen_user_home_label_Date, lv_color_hex(0x8B949E), 0);
    lv_obj_set_style_text_font(ui->screen_user_home_label_Date, &lv_font_SourceHanSerifSC_Regular_16, 0);
    lv_obj_set_style_text_opa(ui->screen_user_home_label_Date, 255, 0);
    lv_obj_set_style_text_align(ui->screen_user_home_label_Date, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_bg_opa(ui->screen_user_home_label_Date, 0, 0);

    /* ============================================================
     *  天气区域 (右下角) - 带彩色图标圆 + 昼夜指示
     * ============================================================ */
    ui->screen_user_home_cont_2 = lv_obj_create(ui->screen_user_home);
    lv_obj_remove_style_all(ui->screen_user_home_cont_2);
    lv_obj_set_pos(ui->screen_user_home_cont_2, 358, 262);
    lv_obj_set_size(ui->screen_user_home_cont_2, 166, 54);
    lv_obj_clear_flag(ui->screen_user_home_cont_2, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(ui->screen_user_home_cont_2, 0, 0);
    lv_obj_set_style_pad_all(ui->screen_user_home_cont_2, 0, 0);

    /* 天气彩色圆 */
    ui->screen_user_home_weather_icon = lv_obj_create(ui->screen_user_home_cont_2);
    lv_obj_remove_style_all(ui->screen_user_home_weather_icon);
    lv_obj_set_size(ui->screen_user_home_weather_icon, 38, 38);
    lv_obj_set_pos(ui->screen_user_home_weather_icon, 10, 8);
    lv_obj_set_style_radius(ui->screen_user_home_weather_icon, 19, 0);
    lv_obj_set_style_bg_color(ui->screen_user_home_weather_icon, lv_color_hex(0xD29922), 0);
    lv_obj_set_style_bg_opa(ui->screen_user_home_weather_icon, LV_OPA_COVER, 0);
    lv_obj_set_style_shadow_width(ui->screen_user_home_weather_icon, 6, 0);
    lv_obj_set_style_shadow_color(ui->screen_user_home_weather_icon, lv_color_hex(0xD29922), 0);
    lv_obj_set_style_shadow_opa(ui->screen_user_home_weather_icon, 40, 0);
    lv_obj_set_style_shadow_ofs_y(ui->screen_user_home_weather_icon, 2, 0);
    lv_obj_clear_flag(ui->screen_user_home_weather_icon, LV_OBJ_FLAG_SCROLLABLE);
    {
        lv_obj_t *icon_sym = lv_label_create(ui->screen_user_home_weather_icon);
        lv_label_set_text(icon_sym, WEATHER_ICON_SUN);
        lv_obj_set_style_text_color(icon_sym, lv_color_hex(0xffffff), 0);
        lv_obj_set_style_text_font(icon_sym, &lv_font_weather_30, 0);
        lv_obj_set_style_bg_opa(icon_sym, 0, 0);
        lv_obj_align(icon_sym, LV_ALIGN_CENTER, 0, 0);
    }

    /* 天气文字 */
    ui->screen_user_home_label_1 = lv_label_create(ui->screen_user_home_cont_2);
    lv_label_set_text(ui->screen_user_home_label_1, "晴天 ");
    lv_obj_set_pos(ui->screen_user_home_label_1, 52, 4);
    lv_obj_set_style_text_color(ui->screen_user_home_label_1, lv_color_hex(0xE6EDF3), 0);
    lv_obj_set_style_text_font(ui->screen_user_home_label_1, &lv_font_SourceHanSerifSC_Regular_16, 0);
    lv_obj_set_style_text_opa(ui->screen_user_home_label_1, 255, 0);
    lv_obj_set_style_bg_opa(ui->screen_user_home_label_1, 0, 0);

    /* 昼夜图标 */
    ui->screen_user_home_daynight_dot = lv_label_create(ui->screen_user_home_cont_2);
    lv_label_set_text(ui->screen_user_home_daynight_dot, "\xE2\x98\x80");
    lv_obj_set_pos(ui->screen_user_home_daynight_dot, 52, 30);
    lv_obj_set_style_text_color(ui->screen_user_home_daynight_dot, lv_color_hex(0xF0C040), 0);
    lv_obj_set_style_text_font(ui->screen_user_home_daynight_dot, &lv_font_SourceHanSerifSC_Regular_16, 0);
    lv_obj_set_style_bg_opa(ui->screen_user_home_daynight_dot, 0, 0);

    /* 昼夜文字标签 */
    ui->screen_user_home_label_2 = lv_label_create(ui->screen_user_home_cont_2);
    lv_label_set_text(ui->screen_user_home_label_2, "\xE7\x99\xBD\xE5\xA4\xA9 ");
    lv_obj_set_pos(ui->screen_user_home_label_2, 70, 30);
    lv_obj_set_style_text_color(ui->screen_user_home_label_2, lv_color_hex(0xE6EDF3), 0);
    lv_obj_set_style_text_font(ui->screen_user_home_label_2, &lv_font_SourceHanSerifSC_Regular_16, 0);
    lv_obj_set_style_text_opa(ui->screen_user_home_label_2, 255, 0);
    lv_obj_set_style_bg_opa(ui->screen_user_home_label_2, 0, 0);

    /* ============================================================
     *  底部信息栏 (左下角)
     * ============================================================ */
    {
        lv_obj_t *sys_label = lv_label_create(ui->screen_user_home);
        lv_label_set_text(sys_label, LV_SYMBOL_HOME" 太阳能智能监控系统 ");
        lv_obj_set_pos(sys_label, 12, 290);
        lv_obj_set_style_text_color(sys_label, lv_color_hex(0xE6EDF3), 0);
        lv_obj_set_style_text_font(sys_label, &lv_font_SourceHanSerifSC_Regular_16, 0);
        lv_obj_set_style_text_opa(sys_label, 200, 0);
        lv_obj_set_style_bg_opa(sys_label, 0, 0);
    }

    /* ============================================================
     *  顶部状态栏 (IP + 端口) - 药丸胶囊风格 (暗色主题)
     * ============================================================ */
    ui->screen_user_home_cont_3 = lv_obj_create(ui->screen_user_home);
    lv_obj_remove_style_all(ui->screen_user_home_cont_3);
    lv_obj_set_pos(ui->screen_user_home_cont_3, 130, 3);
    lv_obj_set_size(ui->screen_user_home_cont_3, 220, 28);
    lv_obj_clear_flag(ui->screen_user_home_cont_3, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(ui->screen_user_home_cont_3, 14, 0);
    lv_obj_set_style_bg_color(ui->screen_user_home_cont_3, lv_color_hex(0x161B22), 0);
    lv_obj_set_style_bg_opa(ui->screen_user_home_cont_3, 200, 0);
    lv_obj_set_style_border_width(ui->screen_user_home_cont_3, 1, 0);
    lv_obj_set_style_border_color(ui->screen_user_home_cont_3, lv_color_hex(0x30363D), 0);
    lv_obj_set_style_border_opa(ui->screen_user_home_cont_3, 80, 0);
    lv_obj_set_style_shadow_width(ui->screen_user_home_cont_3, 3, 0);
    lv_obj_set_style_shadow_color(ui->screen_user_home_cont_3, lv_color_hex(0x000000), 0);
    lv_obj_set_style_shadow_opa(ui->screen_user_home_cont_3, 30, 0);
    lv_obj_set_style_shadow_ofs_y(ui->screen_user_home_cont_3, 1, 0);
    lv_obj_set_style_pad_all(ui->screen_user_home_cont_3, 0, 0);

    {
        lv_obj_t *wifi_icon = lv_label_create(ui->screen_user_home_cont_3);
        lv_label_set_text(wifi_icon, LV_SYMBOL_WIFI);
        lv_obj_set_style_text_color(wifi_icon, lv_color_hex(0xF0C040), 0);
        lv_obj_set_style_text_font(wifi_icon, &lv_font_SourceHanSerifSC_Regular_12, 0);
        lv_obj_set_style_bg_opa(wifi_icon, 0, 0);
        lv_obj_align(wifi_icon, LV_ALIGN_LEFT_MID, 8, 0);
    }

    ui->screen_user_home_label_8 = lv_label_create(ui->screen_user_home_cont_3);
    lv_label_set_text(ui->screen_user_home_label_8, "IP:");
    lv_obj_set_style_text_color(ui->screen_user_home_label_8, lv_color_hex(0x8B949E), 0);
    lv_obj_set_style_text_font(ui->screen_user_home_label_8, &lv_font_SourceHanSerifSC_Regular_12, 0);
    lv_obj_set_style_bg_opa(ui->screen_user_home_label_8, 0, 0);
    lv_obj_align(ui->screen_user_home_label_8, LV_ALIGN_LEFT_MID, 24, 0);

    ui->screen_user_home_label_ip = lv_label_create(ui->screen_user_home_cont_3);
    lv_label_set_text(ui->screen_user_home_label_ip, "");
    lv_obj_set_style_text_color(ui->screen_user_home_label_ip, lv_color_hex(0xE6EDF3), 0);
    lv_obj_set_style_text_font(ui->screen_user_home_label_ip, &lv_font_SourceHanSerifSC_Regular_12, 0);
    lv_obj_set_style_bg_opa(ui->screen_user_home_label_ip, 0, 0);
    lv_obj_align(ui->screen_user_home_label_ip, LV_ALIGN_LEFT_MID, 44, 0);

    {
        lv_obj_t *sep = lv_obj_create(ui->screen_user_home_cont_3);
        lv_obj_remove_style_all(sep);
        lv_obj_set_size(sep, 1, 14);
        lv_obj_align(sep, LV_ALIGN_LEFT_MID, 152, 0);
        lv_obj_set_style_bg_color(sep, lv_color_hex(0x30363D), 0);
        lv_obj_set_style_bg_opa(sep, LV_OPA_COVER, 0);
        lv_obj_clear_flag(sep, LV_OBJ_FLAG_SCROLLABLE);
    }

    ui->screen_user_home_label_10 = lv_label_create(ui->screen_user_home_cont_3);
    lv_label_set_text(ui->screen_user_home_label_10, "P:");
    lv_obj_set_style_text_color(ui->screen_user_home_label_10, lv_color_hex(0x8B949E), 0);
    lv_obj_set_style_text_font(ui->screen_user_home_label_10, &lv_font_SourceHanSerifSC_Regular_12, 0);
    lv_obj_set_style_bg_opa(ui->screen_user_home_label_10, 0, 0);
    lv_obj_align(ui->screen_user_home_label_10, LV_ALIGN_LEFT_MID, 160, 0);

    ui->screen_user_home_label_port = lv_label_create(ui->screen_user_home_cont_3);
    lv_label_set_text(ui->screen_user_home_label_port, "");
    lv_obj_set_style_text_color(ui->screen_user_home_label_port, lv_color_hex(0xE6EDF3), 0);
    lv_obj_set_style_text_font(ui->screen_user_home_label_port, &lv_font_SourceHanSerifSC_Regular_12, 0);
    lv_obj_set_style_bg_opa(ui->screen_user_home_label_port, 0, 0);
    lv_obj_align(ui->screen_user_home_label_port, LV_ALIGN_LEFT_MID, 178, 0);

    //初始化/刷新信号强度指示器组件
    Signal_Widget_Init(guider_ui.screen_user_home);
    //初始化/刷新电池电量指示器组件
    Battery_Widget_Init(guider_ui.screen_user_home);

    lv_obj_update_layout(ui->screen_user_home);
    events_init_screen_user_home(ui);
}
