/*
 * TCP Server Setting Screen
 * =========================
 * Uses LVGL's built-in lv_keyboard + lv_textarea for IP/Port input.
 * Dark theme, keyboard fills the bottom of screen.
 */

#include "lvgl.h"
#include <stdio.h>
#include "gui_guider.h"
#include "events_init.h"
#include "widgets_init.h"
#include "user_main.h"

void setup_scr_screen_tcp_setting(lv_ui *ui)
{
    /* ======== 创建屏幕 ======== */
    ui->screen_tcp_setting = lv_obj_create(NULL);
    lv_obj_set_size(ui->screen_tcp_setting, 480, 320);
    lv_obj_set_scrollbar_mode(ui->screen_tcp_setting, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_bg_color(ui->screen_tcp_setting, lv_color_hex(0x0D1117), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->screen_tcp_setting, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);

    /* ============================================================
     *  顶部标题栏 (0~36)
     * ============================================================ */
    lv_obj_t *header = lv_obj_create(ui->screen_tcp_setting);
    lv_obj_remove_style_all(header);
    lv_obj_set_size(header, 480, 36);
    lv_obj_set_pos(header, 0, 0);
    lv_obj_set_style_bg_color(header, lv_color_hex(0x1C2333), 0);
    lv_obj_set_style_bg_opa(header, LV_OPA_COVER, 0);
    lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *title_icon = lv_label_create(header);
    lv_label_set_text(title_icon, LV_SYMBOL_WIFI);
    lv_obj_set_style_text_color(title_icon, lv_color_hex(0xF0C040), 0);
    lv_obj_set_style_text_font(title_icon, &lv_font_SourceHanSerifSC_Regular_16, 0);
    lv_obj_set_style_bg_opa(title_icon, 0, 0);
    lv_obj_set_pos(title_icon, 10, 8);

    lv_obj_t *title_lbl = lv_label_create(header);
    lv_label_set_text(title_lbl, "TCP服务器设置 ");
    lv_obj_set_style_text_color(title_lbl, lv_color_hex(0xE6EDF3), 0);
    lv_obj_set_style_text_font(title_lbl, &lv_font_SourceHanSerifSC_Regular_16, 0);
    lv_obj_set_style_bg_opa(title_lbl, 0, 0);
    lv_obj_set_pos(title_lbl, 30, 8);

    /* ============================================================
     *  IP输入框 (38~68)
     * ============================================================ */
    lv_obj_t *ip_label = lv_label_create(ui->screen_tcp_setting);
    lv_label_set_text(ip_label, "IP:");
    lv_obj_set_style_text_color(ip_label, lv_color_hex(0x8B949E), 0);
    lv_obj_set_style_text_font(ip_label, &lv_font_SourceHanSerifSC_Regular_16, 0);
    lv_obj_set_style_bg_opa(ip_label, 0, 0);
    lv_obj_set_pos(ip_label, 10, 44);

    ui->screen_tcp_setting_ta_ip = lv_textarea_create(ui->screen_tcp_setting);
    lv_obj_set_size(ui->screen_tcp_setting_ta_ip, 180, 26);
    lv_obj_set_pos(ui->screen_tcp_setting_ta_ip, 38, 38);
    lv_textarea_set_one_line(ui->screen_tcp_setting_ta_ip, true);
    lv_textarea_set_max_length(ui->screen_tcp_setting_ta_ip, 15);
    lv_textarea_set_placeholder_text(ui->screen_tcp_setting_ta_ip, "192.168.1.1");
    lv_obj_set_style_bg_color(ui->screen_tcp_setting_ta_ip, lv_color_hex(0x161B22), 0);
    lv_obj_set_style_bg_opa(ui->screen_tcp_setting_ta_ip, LV_OPA_COVER, 0);
    lv_obj_set_style_text_color(ui->screen_tcp_setting_ta_ip, lv_color_hex(0xE6EDF3), 0);
    lv_obj_set_style_text_font(ui->screen_tcp_setting_ta_ip, &lv_font_SourceHanSerifSC_Regular_16, 0);
    lv_obj_set_style_border_color(ui->screen_tcp_setting_ta_ip, lv_color_hex(0x30363D), 0);
    lv_obj_set_style_border_width(ui->screen_tcp_setting_ta_ip, 1, 0);
    /* 隐藏光标闪烁, 更像标签 */
    lv_obj_set_style_anim_time(ui->screen_tcp_setting_ta_ip, 0, LV_PART_CURSOR | LV_STATE_FOCUSED);

    /* ============================================================
     *  Port输入框
     * ============================================================ */
    lv_obj_t *port_label = lv_label_create(ui->screen_tcp_setting);
    lv_label_set_text(port_label, "Port:");
    lv_obj_set_style_text_color(port_label, lv_color_hex(0x8B949E), 0);
    lv_obj_set_style_text_font(port_label, &lv_font_SourceHanSerifSC_Regular_16, 0);
    lv_obj_set_style_bg_opa(port_label, 0, 0);
    lv_obj_set_pos(port_label, 230, 44);

    ui->screen_tcp_setting_ta_port = lv_textarea_create(ui->screen_tcp_setting);
    lv_obj_set_size(ui->screen_tcp_setting_ta_port, 90, 26);
    lv_obj_set_pos(ui->screen_tcp_setting_ta_port, 272, 38);
    lv_textarea_set_one_line(ui->screen_tcp_setting_ta_port, true);
    lv_textarea_set_max_length(ui->screen_tcp_setting_ta_port, 5);
    lv_textarea_set_placeholder_text(ui->screen_tcp_setting_ta_port, "8080");
    lv_obj_set_style_bg_color(ui->screen_tcp_setting_ta_port, lv_color_hex(0x161B22), 0);
    lv_obj_set_style_bg_opa(ui->screen_tcp_setting_ta_port, LV_OPA_COVER, 0);
    lv_obj_set_style_text_color(ui->screen_tcp_setting_ta_port, lv_color_hex(0xE6EDF3), 0);
    lv_obj_set_style_text_font(ui->screen_tcp_setting_ta_port, &lv_font_SourceHanSerifSC_Regular_16, 0);
    lv_obj_set_style_border_color(ui->screen_tcp_setting_ta_port, lv_color_hex(0x30363D), 0);
    lv_obj_set_style_border_width(ui->screen_tcp_setting_ta_port, 1, 0);

    /* textarea聚焦样式 (蓝色边框表示当前活动) */
    static lv_style_t style_ta_focused;
    ui_init_style(&style_ta_focused);
    lv_style_set_border_color(&style_ta_focused, lv_color_hex(0x58A6FF));
    lv_style_set_border_width(&style_ta_focused, 2);
    lv_style_set_border_opa(&style_ta_focused, LV_OPA_COVER);
    lv_obj_add_style(ui->screen_tcp_setting_ta_ip, &style_ta_focused, LV_PART_MAIN | LV_STATE_FOCUSED);
    lv_obj_add_style(ui->screen_tcp_setting_ta_port, &style_ta_focused, LV_PART_MAIN | LV_STATE_FOCUSED);

    /* ============================================================
     *  LVGL内置键盘 (填满屏幕底部: y=72, h=248)
     *  数字模式自带确认键(READY)和取消键(CANCEL)
     * ============================================================ */
    ui->screen_tcp_setting_kb = lv_keyboard_create(ui->screen_tcp_setting);
    lv_obj_set_size(ui->screen_tcp_setting_kb, 480, 248);
    lv_obj_set_pos(ui->screen_tcp_setting_kb, 0, 10);
    lv_keyboard_set_mode(ui->screen_tcp_setting_kb, LV_KEYBOARD_MODE_NUMBER);
    /* 默认关联到IP输入框 */
    lv_keyboard_set_textarea(ui->screen_tcp_setting_kb, ui->screen_tcp_setting_ta_ip);

    /* 键盘背景 */
    lv_obj_set_style_bg_color(ui->screen_tcp_setting_kb, lv_color_hex(0x0D1117), 0);
    lv_obj_set_style_bg_opa(ui->screen_tcp_setting_kb, LV_OPA_COVER, 0);

    /* 键盘按键默认样式 */
    static lv_style_t style_kb_btn;
    ui_init_style(&style_kb_btn);
    lv_style_set_radius(&style_kb_btn, 4);
    lv_style_set_bg_color(&style_kb_btn, lv_color_hex(0x21262D));
    lv_style_set_bg_opa(&style_kb_btn, LV_OPA_COVER);
    lv_style_set_text_color(&style_kb_btn, lv_color_hex(0xE6EDF3));
    lv_style_set_text_font(&style_kb_btn, &lv_font_SourceHanSerifSC_Regular_16);
    lv_style_set_border_color(&style_kb_btn, lv_color_hex(0x30363D));
    lv_style_set_border_width(&style_kb_btn, 1);
    lv_style_set_border_opa(&style_kb_btn, LV_OPA_COVER);
    lv_obj_add_style(ui->screen_tcp_setting_kb, &style_kb_btn, LV_PART_ITEMS | LV_STATE_DEFAULT);

    /* 键盘按键按下样式 */
    static lv_style_t style_kb_btn_pressed;
    ui_init_style(&style_kb_btn_pressed);
    lv_style_set_bg_color(&style_kb_btn_pressed, lv_color_hex(0x58A6FF));
    lv_style_set_bg_opa(&style_kb_btn_pressed, LV_OPA_COVER);
    lv_style_set_text_color(&style_kb_btn_pressed, lv_color_hex(0xFFFFFF));
    lv_obj_add_style(ui->screen_tcp_setting_kb, &style_kb_btn_pressed, LV_PART_ITEMS | LV_STATE_PRESSED);

    /* 键盘按键聚焦样式 (用于键盘导航) */
    static lv_style_t style_kb_btn_focused;
    ui_init_style(&style_kb_btn_focused);
    lv_style_set_bg_color(&style_kb_btn_focused, lv_color_hex(0x58A6FF));
    lv_style_set_bg_opa(&style_kb_btn_focused, LV_OPA_COVER);
    lv_style_set_text_color(&style_kb_btn_focused, lv_color_hex(0xFFFFFF));
    lv_style_set_border_width(&style_kb_btn_focused, 0);
    lv_obj_add_style(ui->screen_tcp_setting_kb, &style_kb_btn_focused, LV_PART_ITEMS | LV_STATE_FOCUSED);

    lv_obj_update_layout(ui->screen_tcp_setting);
    events_init_screen_tcp_setting(ui);
}