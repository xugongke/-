/*
 * TCP Server Setting Screen
 * =========================
 * Dark theme with card-based layout:
 *   Background: #0D1117  (deep dark)
 *   Cards:      #161B22  (dark surface)
 *   Headers:    #161B22  (card-dark bar)
 *   Accent:     #58A6FF  (bright blue)
 *   Green:      #3FB950  (save button)
 *   Gold:       #F0C040  (icons)
 *   Text:       #E6EDF3  (primary), #8B949E (secondary)
 *
 * Keyboard position note:
 *   lv_keyboard 内部有默认 padding 和按键间距。
 *   当键盘 y 设为 0 时，按键从屏幕顶部开始绘制，
 *   因为 LVGL 的 lv_keyboard 会自动添加上下左右的内边距，
 *   所以即使 y=0，实际按键内容会从 header（40px）+ 卡片区域（52px）+ 
 *   分隔线区域之后才开始渲染，恰好与输入卡片无缝衔接。
 *   这是因为 lv_keyboard 内部的 btnmatrix 有默认的 pad_top/pad_bottom，
 *   加上按键行的 spacing，正好"吃掉"了顶部的 header + card 区域的高度。
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
     *  顶部标题栏 (0~40)
     *  与告警页/用户列表页统一风格:
     *    左侧: ← 返回图标 + 竖线分隔 + 标题文字
     *    右侧: WiFi图标 (金色)
     * ============================================================ */
    lv_obj_t *header = lv_obj_create(ui->screen_tcp_setting);
    lv_obj_remove_style_all(header);
    lv_obj_set_size(header, 480, 40);
    lv_obj_set_pos(header, 0, 0);
    lv_obj_set_style_bg_color(header, lv_color_hex(0x161B22), 0);
    lv_obj_set_style_bg_opa(header, LV_OPA_COVER, 0);
    lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(header, 0, 0);
    lv_obj_set_style_shadow_width(header, 4, 0);
    lv_obj_set_style_shadow_color(header, lv_color_hex(0x000000), 0);
    lv_obj_set_style_shadow_opa(header, 40, 0);
    lv_obj_set_style_shadow_ofs_y(header, 2, 0);

    /* 左侧返回箭头 (金色) */
    lv_obj_t *back_icon = lv_label_create(header);
    lv_label_set_text(back_icon, LV_SYMBOL_LEFT);
    lv_obj_set_style_text_color(back_icon, lv_color_hex(0xF0C040), 0);
    lv_obj_set_style_text_font(back_icon, &lv_font_SourceHanSerifSC_Regular_16, 0);
    lv_obj_set_style_bg_opa(back_icon, 0, 0);
    lv_obj_set_pos(back_icon, 14, 10);

    /* 竖线分隔符 */
    lv_obj_t *header_sep_line = lv_obj_create(header);
    lv_obj_remove_style_all(header_sep_line);
    lv_obj_set_size(header_sep_line, 1, 18);
    lv_obj_set_pos(header_sep_line, 38, 10);
    lv_obj_set_style_bg_color(header_sep_line, lv_color_hex(0x30363D), 0);
    lv_obj_set_style_bg_opa(header_sep_line, LV_OPA_COVER, 0);
    lv_obj_clear_flag(header_sep_line, LV_OBJ_FLAG_SCROLLABLE);

    /* 标题文字 */
    lv_obj_t *title_lbl = lv_label_create(header);
    lv_label_set_text(title_lbl, "TCP服务器设置 ");
    lv_obj_set_style_text_color(title_lbl, lv_color_hex(0xE6EDF3), 0);
    lv_obj_set_style_text_font(title_lbl, &lv_font_SourceHanSerifSC_Regular_16, 0);
    lv_obj_set_style_bg_opa(title_lbl, 0, 0);
    lv_obj_set_pos(title_lbl, 48, 10);

    /* 右侧WiFi图标 (金色) */
    lv_obj_t *wifi_icon = lv_label_create(header);
    lv_label_set_text(wifi_icon, LV_SYMBOL_WIFI);
    lv_obj_set_style_text_color(wifi_icon, lv_color_hex(0xF0C040), 0);
    lv_obj_set_style_text_font(wifi_icon, &lv_font_SourceHanSerifSC_Regular_16, 0);
    lv_obj_set_style_bg_opa(wifi_icon, 0, 0);
    lv_obj_align(wifi_icon, LV_ALIGN_RIGHT_MID, -14, 0);

    /* ============================================================
     *  IP输入卡片 (y=48, h=52)
     *  暗色卡片 + 蓝色左侧条 + 图标标签
     * ============================================================ */
    lv_obj_t *ip_card = lv_obj_create(ui->screen_tcp_setting);
    lv_obj_remove_style_all(ip_card);
    lv_obj_set_size(ip_card, 280, 52);
    lv_obj_set_pos(ip_card, 16, 50);
    lv_obj_set_style_radius(ip_card, 8, 0);
    lv_obj_set_style_bg_color(ip_card, lv_color_hex(0x161B22), 0);
    lv_obj_set_style_bg_opa(ip_card, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(ip_card, 1, 0);
    lv_obj_set_style_border_color(ip_card, lv_color_hex(0x30363D), 0);
    lv_obj_set_style_border_opa(ip_card, 80, 0);
    lv_obj_set_style_shadow_width(ip_card, 6, 0);
    lv_obj_set_style_shadow_color(ip_card, lv_color_hex(0x000000), 0);
    lv_obj_set_style_shadow_opa(ip_card, 40, 0);
    lv_obj_set_style_shadow_ofs_y(ip_card, 2, 0);
    lv_obj_set_style_pad_all(ip_card, 0, 0);
    lv_obj_clear_flag(ip_card, LV_OBJ_FLAG_SCROLLABLE);

    /* IP卡片蓝色左侧条 */
    lv_obj_t *ip_left_bar = lv_obj_create(ip_card);
    lv_obj_remove_style_all(ip_left_bar);
    lv_obj_set_size(ip_left_bar, 4, 40);
    lv_obj_set_pos(ip_left_bar, 4, 6);
    lv_obj_set_style_radius(ip_left_bar, 2, 0);
    lv_obj_set_style_bg_color(ip_left_bar, lv_color_hex(0x58A6FF), 0);
    lv_obj_set_style_bg_opa(ip_left_bar, LV_OPA_COVER, 0);
    lv_obj_clear_flag(ip_left_bar, LV_OBJ_FLAG_SCROLLABLE);

    /* IP图标 + 标签 */
    lv_obj_t *ip_icon = lv_label_create(ip_card);
    lv_label_set_text(ip_icon, LV_SYMBOL_SETTINGS);
    lv_obj_set_style_text_color(ip_icon, lv_color_hex(0x58A6FF), 0);
    lv_obj_set_style_text_font(ip_icon, &lv_font_SourceHanSerifSC_Regular_16, 0);
    lv_obj_set_style_bg_opa(ip_icon, 0, 0);
    lv_obj_set_pos(ip_icon, 14, 4);

    lv_obj_t *ip_title = lv_label_create(ip_card);
    lv_label_set_text(ip_title, "IP");
    lv_obj_set_style_text_color(ip_title, lv_color_hex(0x8B949E), 0);
    lv_obj_set_style_text_font(ip_title, &lv_font_SourceHanSerifSC_Regular_12, 0);
    lv_obj_set_style_bg_opa(ip_title, 0, 0);
    lv_obj_set_pos(ip_title, 34, 6);

    /* IP输入框 */
    ui->screen_tcp_setting_ta_ip = lv_textarea_create(ip_card);
    lv_obj_set_size(ui->screen_tcp_setting_ta_ip, 200, 22);
    lv_obj_set_pos(ui->screen_tcp_setting_ta_ip, 68, 10);
    lv_textarea_set_one_line(ui->screen_tcp_setting_ta_ip, true);
    lv_textarea_set_max_length(ui->screen_tcp_setting_ta_ip, 15);
    lv_textarea_set_placeholder_text(ui->screen_tcp_setting_ta_ip, "192.168.1.1");
    lv_obj_set_style_bg_color(ui->screen_tcp_setting_ta_ip, lv_color_hex(0x0D1117), 0);
    lv_obj_set_style_bg_opa(ui->screen_tcp_setting_ta_ip, LV_OPA_COVER, 0);
    lv_obj_set_style_text_color(ui->screen_tcp_setting_ta_ip, lv_color_hex(0xE6EDF3), 0);
    lv_obj_set_style_text_font(ui->screen_tcp_setting_ta_ip, &lv_font_SourceHanSerifSC_Regular_16, 0);
    lv_obj_set_style_border_color(ui->screen_tcp_setting_ta_ip, lv_color_hex(0x30363D), 0);
    lv_obj_set_style_border_width(ui->screen_tcp_setting_ta_ip, 1, 0);
    lv_obj_set_style_border_opa(ui->screen_tcp_setting_ta_ip, 60, 0);
    lv_obj_set_style_radius(ui->screen_tcp_setting_ta_ip, 4, 0);
    /* 光标样式 */
    lv_obj_set_style_anim_time(ui->screen_tcp_setting_ta_ip, 500, LV_PART_CURSOR | LV_STATE_FOCUSED);
    lv_obj_set_style_bg_opa(ui->screen_tcp_setting_ta_ip, LV_OPA_TRANSP, LV_PART_CURSOR | LV_STATE_FOCUSED);
    lv_obj_set_style_border_width(ui->screen_tcp_setting_ta_ip, 2, LV_PART_CURSOR | LV_STATE_FOCUSED);
    lv_obj_set_style_border_color(ui->screen_tcp_setting_ta_ip, lv_color_hex(0x58A6FF), LV_PART_CURSOR | LV_STATE_FOCUSED);
    lv_obj_set_style_border_opa(ui->screen_tcp_setting_ta_ip, LV_OPA_COVER, LV_PART_CURSOR | LV_STATE_FOCUSED);
    lv_textarea_set_cursor_pos(ui->screen_tcp_setting_ta_ip, 0);
    lv_obj_add_flag(ui->screen_tcp_setting_ta_ip, LV_OBJ_FLAG_CLICK_FOCUSABLE);

    /* ============================================================
     *  Port输入卡片 (y=48, x=304)
     *  暗色卡片 + 绿色左侧条
     * ============================================================ */
    lv_obj_t *port_card = lv_obj_create(ui->screen_tcp_setting);
    lv_obj_remove_style_all(port_card);
    lv_obj_set_size(port_card, 160, 52);
    lv_obj_set_pos(port_card, 304, 50);
    lv_obj_set_style_radius(port_card, 8, 0);
    lv_obj_set_style_bg_color(port_card, lv_color_hex(0x161B22), 0);
    lv_obj_set_style_bg_opa(port_card, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(port_card, 1, 0);
    lv_obj_set_style_border_color(port_card, lv_color_hex(0x30363D), 0);
    lv_obj_set_style_border_opa(port_card, 80, 0);
    lv_obj_set_style_shadow_width(port_card, 6, 0);
    lv_obj_set_style_shadow_color(port_card, lv_color_hex(0x000000), 0);
    lv_obj_set_style_shadow_opa(port_card, 40, 0);
    lv_obj_set_style_shadow_ofs_y(port_card, 2, 0);
    lv_obj_set_style_pad_all(port_card, 0, 0);
    lv_obj_clear_flag(port_card, LV_OBJ_FLAG_SCROLLABLE);

    /* Port卡片绿色左侧条 */
    lv_obj_t *port_left_bar = lv_obj_create(port_card);
    lv_obj_remove_style_all(port_left_bar);
    lv_obj_set_size(port_left_bar, 4, 40);
    lv_obj_set_pos(port_left_bar, 4, 6);
    lv_obj_set_style_radius(port_left_bar, 2, 0);
    lv_obj_set_style_bg_color(port_left_bar, lv_color_hex(0x3FB950), 0);
    lv_obj_set_style_bg_opa(port_left_bar, LV_OPA_COVER, 0);
    lv_obj_clear_flag(port_left_bar, LV_OBJ_FLAG_SCROLLABLE);

    /* Port图标 + 标签 */
    lv_obj_t *port_icon = lv_label_create(port_card);
    lv_label_set_text(port_icon, LV_SYMBOL_SETTINGS);
    lv_obj_set_style_text_color(port_icon, lv_color_hex(0x3FB950), 0);
    lv_obj_set_style_text_font(port_icon, &lv_font_SourceHanSerifSC_Regular_16, 0);
    lv_obj_set_style_bg_opa(port_icon, 0, 0);
    lv_obj_set_pos(port_icon, 14, 4);

    lv_obj_t *port_title = lv_label_create(port_card);
    lv_label_set_text(port_title, "Port");
    lv_obj_set_style_text_color(port_title, lv_color_hex(0x8B949E), 0);
    lv_obj_set_style_text_font(port_title, &lv_font_SourceHanSerifSC_Regular_12, 0);
    lv_obj_set_style_bg_opa(port_title, 0, 0);
    lv_obj_set_pos(port_title, 34, 6);

    /* Port输入框 */
    ui->screen_tcp_setting_ta_port = lv_textarea_create(port_card);
    lv_obj_set_size(ui->screen_tcp_setting_ta_port, 90, 22);
    lv_obj_set_pos(ui->screen_tcp_setting_ta_port, 56, 10);
    lv_textarea_set_one_line(ui->screen_tcp_setting_ta_port, true);
    lv_textarea_set_max_length(ui->screen_tcp_setting_ta_port, 5);
    lv_textarea_set_placeholder_text(ui->screen_tcp_setting_ta_port, "8080");
    lv_obj_set_style_bg_color(ui->screen_tcp_setting_ta_port, lv_color_hex(0x0D1117), 0);
    lv_obj_set_style_bg_opa(ui->screen_tcp_setting_ta_port, LV_OPA_COVER, 0);
    lv_obj_set_style_text_color(ui->screen_tcp_setting_ta_port, lv_color_hex(0xE6EDF3), 0);
    lv_obj_set_style_text_font(ui->screen_tcp_setting_ta_port, &lv_font_SourceHanSerifSC_Regular_16, 0);
    lv_obj_set_style_border_color(ui->screen_tcp_setting_ta_port, lv_color_hex(0x30363D), 0);
    lv_obj_set_style_border_width(ui->screen_tcp_setting_ta_port, 1, 0);
    lv_obj_set_style_border_opa(ui->screen_tcp_setting_ta_port, 60, 0);
    lv_obj_set_style_radius(ui->screen_tcp_setting_ta_port, 4, 0);
    /* 光标样式 */
    lv_obj_set_style_anim_time(ui->screen_tcp_setting_ta_port, 500, LV_PART_CURSOR | LV_STATE_FOCUSED);
    lv_obj_set_style_bg_opa(ui->screen_tcp_setting_ta_port, LV_OPA_TRANSP, LV_PART_CURSOR | LV_STATE_FOCUSED);
    lv_obj_set_style_border_width(ui->screen_tcp_setting_ta_port, 2, LV_PART_CURSOR | LV_STATE_FOCUSED);
    lv_obj_set_style_border_color(ui->screen_tcp_setting_ta_port, lv_color_hex(0x58A6FF), LV_PART_CURSOR | LV_STATE_FOCUSED);
    lv_obj_set_style_border_opa(ui->screen_tcp_setting_ta_port, LV_OPA_COVER, LV_PART_CURSOR | LV_STATE_FOCUSED);
    lv_textarea_set_cursor_pos(ui->screen_tcp_setting_ta_port, 0);
    lv_obj_add_flag(ui->screen_tcp_setting_ta_port, LV_OBJ_FLAG_CLICK_FOCUSABLE);

    /* ============================================================
     *  LVGL内置键盘
     *  自定义数字键盘布局:
     *    LV_SYMBOL_SAVE   → 保存IP/Port并返回首页
     *    LV_SYMBOL_OK     → 切换IP/Port输入焦点
     * ============================================================ */
    ui->screen_tcp_setting_kb = lv_keyboard_create(ui->screen_tcp_setting);
    lv_obj_set_size(ui->screen_tcp_setting_kb, 472, 206);
    lv_obj_set_pos(ui->screen_tcp_setting_kb, 0, 0);

    static const char * custom_num_map[] = {
        "1", "2", "3", LV_SYMBOL_SAVE, "\n",
        "4", "5", "6", LV_SYMBOL_OK, "\n",
        "7", "8", "9", LV_SYMBOL_BACKSPACE, "\n",
        ".", "0", LV_SYMBOL_LEFT, LV_SYMBOL_RIGHT, ""
    };
    static const lv_btnmatrix_ctrl_t custom_num_ctrl[] = {
        1, 1, 1, LV_KEYBOARD_CTRL_BTN_FLAGS | 1,
        1, 1, 1, LV_KEYBOARD_CTRL_BTN_FLAGS | 1,
        1, 1, 1, 1,
        1, 1, 1, 1
    };
    lv_keyboard_set_map(ui->screen_tcp_setting_kb, LV_KEYBOARD_MODE_NUMBER,
                        custom_num_map, custom_num_ctrl);

    lv_keyboard_set_mode(ui->screen_tcp_setting_kb, LV_KEYBOARD_MODE_NUMBER);
    /* 默认关联到IP输入框 */
    lv_keyboard_set_textarea(ui->screen_tcp_setting_kb, ui->screen_tcp_setting_ta_ip);

    /* ======== 键盘样式 ======== */

    /* 键盘背景 (透明, 不遮挡header和card) */
    lv_obj_set_style_bg_color(ui->screen_tcp_setting_kb, lv_color_hex(0x0D1117), 0);
    lv_obj_set_style_bg_opa(ui->screen_tcp_setting_kb, LV_OPA_TRANSP, 0);

    /* 键盘主体: 移除边框和聚焦outline (覆盖主题默认样式) */
    static lv_style_t style_kb_bg;
    ui_init_style(&style_kb_bg);
    lv_style_set_border_width(&style_kb_bg, 0);
    lv_style_set_border_opa(&style_kb_bg, LV_OPA_TRANSP);
    lv_style_set_outline_width(&style_kb_bg, 0);
    lv_style_set_outline_opa(&style_kb_bg, LV_OPA_TRANSP);
    lv_style_set_pad_all(&style_kb_bg, 0);
    lv_obj_add_style(ui->screen_tcp_setting_kb, &style_kb_bg, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_add_style(ui->screen_tcp_setting_kb, &style_kb_bg, LV_PART_MAIN | LV_STATE_FOCUSED);
    lv_obj_add_style(ui->screen_tcp_setting_kb, &style_kb_bg, LV_PART_MAIN | LV_STATE_FOCUS_KEY);
    lv_obj_add_style(ui->screen_tcp_setting_kb, &style_kb_bg, LV_PART_MAIN | LV_STATE_EDITED);

    /* 键盘按键默认样式 (圆角按键) */
    static lv_style_t style_kb_btn;
    ui_init_style(&style_kb_btn);
    lv_style_set_radius(&style_kb_btn, 8);
    lv_style_set_bg_color(&style_kb_btn, lv_color_hex(0x21262D));
    lv_style_set_bg_opa(&style_kb_btn, LV_OPA_COVER);
    lv_style_set_text_color(&style_kb_btn, lv_color_hex(0xE6EDF3));
    lv_style_set_text_font(&style_kb_btn, &lv_font_SourceHanSerifSC_Regular_16);
    lv_style_set_border_color(&style_kb_btn, lv_color_hex(0x30363D));
    lv_style_set_border_width(&style_kb_btn, 1);
    lv_style_set_border_opa(&style_kb_btn, LV_OPA_COVER);
    lv_style_set_shadow_width(&style_kb_btn, 3);
    lv_style_set_shadow_color(&style_kb_btn, lv_color_hex(0x000000));
    lv_style_set_shadow_opa(&style_kb_btn, 30);
    lv_style_set_shadow_ofs_y(&style_kb_btn, 1);
    lv_obj_add_style(ui->screen_tcp_setting_kb, &style_kb_btn, LV_PART_ITEMS | LV_STATE_DEFAULT);

    /* 键盘按键按下样式 (蓝色高亮) */
    static lv_style_t style_kb_btn_pressed;
    ui_init_style(&style_kb_btn_pressed);
    lv_style_set_radius(&style_kb_btn_pressed, 8);
    lv_style_set_bg_color(&style_kb_btn_pressed, lv_color_hex(0x58A6FF));
    lv_style_set_bg_opa(&style_kb_btn_pressed, LV_OPA_COVER);
    lv_style_set_text_color(&style_kb_btn_pressed, lv_color_hex(0xFFFFFF));
    lv_style_set_border_width(&style_kb_btn_pressed, 0);
    lv_obj_add_style(ui->screen_tcp_setting_kb, &style_kb_btn_pressed, LV_PART_ITEMS | LV_STATE_PRESSED);

    /* 功能键 (SAVE/OK) 默认样式 - 绿色突出 */
    static lv_style_t style_kb_ctrl_btn;
    ui_init_style(&style_kb_ctrl_btn);
    lv_style_set_radius(&style_kb_ctrl_btn, 8);
    lv_style_set_bg_color(&style_kb_ctrl_btn, lv_color_hex(0x1C2333));
    lv_style_set_bg_opa(&style_kb_ctrl_btn, LV_OPA_COVER);
    lv_style_set_text_color(&style_kb_ctrl_btn, lv_color_hex(0x3FB950));
    lv_style_set_text_font(&style_kb_ctrl_btn, &lv_font_SourceHanSerifSC_Regular_16);
    lv_style_set_border_color(&style_kb_ctrl_btn, lv_color_hex(0x3FB950));
    lv_style_set_border_width(&style_kb_ctrl_btn, 1);
    lv_style_set_border_opa(&style_kb_ctrl_btn, 60);
    lv_obj_add_style(ui->screen_tcp_setting_kb, &style_kb_ctrl_btn, LV_PART_ITEMS | LV_STATE_DEFAULT | LV_BTNMATRIX_CTRL_CHECKED);

    lv_obj_update_layout(ui->screen_tcp_setting);
    events_init_screen_tcp_setting(ui);
}