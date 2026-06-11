/*
* Copyright 2026 NXP
* Dark Theme - User Detail Screen
*/

#include "lvgl.h"
#include <stdio.h>
#include "gui_guider.h"
#include "events_init.h"
#include "widgets_init.h"
#include "custom.h"

/**
 * @brief  图表绘制回调：在每个柱子上方绘制数值标签
 */
static void detail_chart_draw_label_cb(lv_event_t *e)
{
    lv_obj_draw_part_dsc_t * dsc = lv_event_get_draw_part_dsc(e);
    if(dsc->part != LV_PART_ITEMS) return;

    lv_obj_t * chart = lv_event_get_target(e);
    lv_chart_series_t * ser = lv_chart_get_series_next(chart, NULL);
    if(!ser) return;
    if(dsc->id >= lv_chart_get_point_count(chart)) return;

    int16_t val = ser->y_points[dsc->id];
    if(val == LV_CHART_POINT_NONE) return;

    char txt[16];
    int a = (val < 0) ? -val : val;
    if(val < 0) lv_snprintf(txt, sizeof(txt), "-%d.%d", a / 10, a % 10);
    else         lv_snprintf(txt, sizeof(txt), "%d.%d", a / 10, a % 10);

    lv_draw_label_dsc_t lbl;
    lv_draw_label_dsc_init(&lbl);
    lbl.color = lv_color_hex(0x8B949E);
    lbl.font = &lv_font_SourceHanSerifSC_Regular_12;
    lbl.align = LV_TEXT_ALIGN_CENTER;

    lv_point_t size;
    lv_txt_get_size(&size, txt, lbl.font, 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);

    lv_area_t txt_area;
    txt_area.x1 = dsc->draw_area->x1 + (lv_area_get_width(dsc->draw_area) - size.x) / 2;
    txt_area.x2 = txt_area.x1 + size.x;
    txt_area.y2 = dsc->draw_area->y1 - 1;
    txt_area.y1 = txt_area.y2 - size.y;

    lv_draw_label(dsc->draw_ctx, &lbl, &txt_area, txt, NULL);
}

void setup_scr_screen_user_detail(lv_ui *ui)
{
    ui->screen_user_detail = lv_obj_create(NULL);
    lv_obj_set_size(ui->screen_user_detail, 480, 320);
    lv_obj_set_scrollbar_mode(ui->screen_user_detail, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_bg_color(ui->screen_user_detail, lv_color_hex(0x0D1117), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->screen_user_detail, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);

    /* ======== 标题栏 (40px) ======== */
    {
        lv_obj_t *h = lv_obj_create(ui->screen_user_detail);
        lv_obj_remove_style_all(h);
        lv_obj_set_size(h, 480, 40);
        lv_obj_set_pos(h, 0, 0);
        lv_obj_set_style_bg_color(h, lv_color_hex(0x161B22), 0);
        lv_obj_set_style_bg_opa(h, LV_OPA_COVER, 0);
        lv_obj_clear_flag(h, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_style_pad_all(h, 0, 0);
        lv_obj_set_style_shadow_width(h, 4, 0);
        lv_obj_set_style_shadow_color(h, lv_color_hex(0x000000), 0);
        lv_obj_set_style_shadow_opa(h, 40, 0);
        lv_obj_set_style_shadow_ofs_y(h, 2, 0);

        lv_obj_t *back = lv_label_create(h);
        lv_label_set_text(back, LV_SYMBOL_LEFT);
        lv_obj_set_style_text_color(back, lv_color_hex(0xF0C040), 0);
        lv_obj_set_style_text_font(back, &lv_font_SourceHanSerifSC_Regular_16, 0);
        lv_obj_set_style_bg_opa(back, 0, 0);
        lv_obj_set_pos(back, 14, 10);

        lv_obj_t *sep = lv_obj_create(h);
        lv_obj_remove_style_all(sep);
        lv_obj_set_size(sep, 1, 18);
        lv_obj_set_pos(sep, 38, 10);
        lv_obj_set_style_bg_color(sep, lv_color_hex(0x30363D), 0);
        lv_obj_set_style_bg_opa(sep, LV_OPA_COVER, 0);
        lv_obj_clear_flag(sep, LV_OBJ_FLAG_SCROLLABLE);

        ui->screen_user_detail_label_user = lv_label_create(h);
        lv_label_set_text(ui->screen_user_detail_label_user, "用户: --");
        lv_obj_set_style_text_color(ui->screen_user_detail_label_user, lv_color_hex(0xE6EDF3), 0);
        lv_obj_set_style_text_font(ui->screen_user_detail_label_user, &lv_font_SourceHanSerifSC_Regular_16, 0);
        lv_obj_set_style_bg_opa(ui->screen_user_detail_label_user, 0, 0);
        lv_obj_set_pos(ui->screen_user_detail_label_user, 48, 10);

        lv_obj_t *edit = lv_label_create(h);
        lv_label_set_text(edit, LV_SYMBOL_EDIT);
        lv_obj_set_style_text_color(edit, lv_color_hex(0x484F58), 0);
        lv_obj_set_style_text_font(edit, &lv_font_SourceHanSerifSC_Regular_16, 0);
        lv_obj_set_style_bg_opa(edit, 0, 0);
        lv_obj_align(edit, LV_ALIGN_RIGHT_MID, -14, 0);
    }

    /* ======== 7天柱状图卡片 ======== */
    {
        lv_obj_t *card = lv_obj_create(ui->screen_user_detail);
        lv_obj_remove_style_all(card);
        lv_obj_set_size(card, 464, 128);
        lv_obj_set_pos(card, 8, 44);
        lv_obj_set_style_radius(card, 10, 0);
        lv_obj_set_style_bg_color(card, lv_color_hex(0x161B22), 0);
        lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(card, 1, 0);
        lv_obj_set_style_border_color(card, lv_color_hex(0x30363D), 0);
        lv_obj_set_style_border_opa(card, 80, 0);
        lv_obj_set_style_shadow_width(card, 6, 0);
        lv_obj_set_style_shadow_color(card, lv_color_hex(0x000000), 0);
        lv_obj_set_style_shadow_opa(card, 40, 0);
        lv_obj_set_style_shadow_ofs_y(card, 2, 0);
        lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_style_pad_all(card, 4, 0);

        lv_obj_t *ct = lv_label_create(card);
        lv_label_set_text(ct, "近7日用电量 (kWh)");
        lv_obj_set_style_text_color(ct, lv_color_hex(0x3FB950), 0);
        lv_obj_set_style_text_font(ct, &lv_font_SourceHanSerifSC_Regular_16, 0);
        lv_obj_set_style_bg_opa(ct, 0, 0);
        lv_obj_set_pos(ct, 6, 2);

        lv_obj_t *ch = lv_chart_create(card);
        lv_obj_set_size(ch, 448, 96);
        lv_obj_set_pos(ch, 4, 20);
        lv_obj_set_style_bg_opa(ch, LV_OPA_TRANSP, LV_PART_MAIN);
        lv_obj_set_style_border_width(ch, 1, LV_PART_MAIN);
        lv_obj_set_style_border_color(ch, lv_color_hex(0x30363D), LV_PART_MAIN);
        lv_chart_set_type(ch, LV_CHART_TYPE_BAR);
        lv_chart_set_point_count(ch, 7);
        lv_chart_set_div_line_count(ch, 3, 0);
        lv_chart_set_range(ch, LV_CHART_AXIS_PRIMARY_Y, 0, 50);

        ui->screen_user_detail_chart = ch;  /* 保存chart引用，供数据填充时使用 */
        lv_chart_series_t *ser = lv_chart_add_series(ch, lv_color_hex(0x3FB950), LV_CHART_AXIS_PRIMARY_Y);
        for(int i = 0; i < 7; i++){
            ser->y_points[i] = (int16_t)0;
        }
        lv_chart_refresh(ch);
        lv_obj_set_style_size(ch, 16, LV_PART_INDICATOR);
        lv_obj_add_event_cb(ch, detail_chart_draw_label_cb, LV_EVENT_DRAW_PART_END, NULL);
    }

    /* ======== 用电量统计表格卡片 ======== */
    {
        lv_obj_t *card = lv_obj_create(ui->screen_user_detail);
        lv_obj_remove_style_all(card);
        lv_obj_set_size(card, 464, 110);
        lv_obj_set_pos(card, 8, 176);
        lv_obj_set_style_radius(card, 10, 0);
        lv_obj_set_style_bg_color(card, lv_color_hex(0x161B22), 0);
        lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(card, 1, 0);
        lv_obj_set_style_border_color(card, lv_color_hex(0x30363D), 0);
        lv_obj_set_style_border_opa(card, 80, 0);
        lv_obj_set_style_shadow_width(card, 6, 0);
        lv_obj_set_style_shadow_color(card, lv_color_hex(0x000000), 0);
        lv_obj_set_style_shadow_opa(card, 40, 0);
        lv_obj_set_style_shadow_ofs_y(card, 2, 0);
        lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_style_pad_all(card, 6, 0);

        lv_obj_t *st = lv_label_create(card);
        lv_label_set_text(st, "用电量统计 (kWh)");
        lv_obj_set_style_text_color(st, lv_color_hex(0x3FB950), 0);
        lv_obj_set_style_text_font(st, &lv_font_SourceHanSerifSC_Regular_16, 0);
        lv_obj_set_style_bg_opa(st, 0, 0);
        lv_obj_set_pos(st, 4, 2);

        ui->screen_user_detail_table_1 = lv_table_create(card);
        lv_obj_set_pos(ui->screen_user_detail_table_1, 4, 20);
        lv_obj_set_style_text_font(ui->screen_user_detail_table_1, &lv_font_SourceHanSerifSC_Regular_16, LV_PART_MAIN);
        lv_obj_set_style_bg_color(ui->screen_user_detail_table_1, lv_color_hex(0x161B22), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(ui->screen_user_detail_table_1, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_border_width(ui->screen_user_detail_table_1, 1, LV_PART_MAIN);
        lv_obj_set_style_border_color(ui->screen_user_detail_table_1, lv_color_hex(0x30363D), LV_PART_MAIN);
        lv_obj_set_style_pad_left(ui->screen_user_detail_table_1, 6, LV_PART_MAIN);
        lv_obj_set_style_pad_right(ui->screen_user_detail_table_1, 6, LV_PART_MAIN);
        lv_obj_set_style_pad_top(ui->screen_user_detail_table_1, 4, LV_PART_MAIN);
        lv_obj_set_style_pad_bottom(ui->screen_user_detail_table_1, 4, LV_PART_MAIN);

        lv_table_set_col_cnt(ui->screen_user_detail_table_1, 4);
        lv_table_set_row_cnt(ui->screen_user_detail_table_1, 2);
        lv_table_set_col_width(ui->screen_user_detail_table_1, 0, 106);
        lv_table_set_col_width(ui->screen_user_detail_table_1, 1, 106);
        lv_table_set_col_width(ui->screen_user_detail_table_1, 2, 106);
        lv_table_set_col_width(ui->screen_user_detail_table_1, 3, 106);

        lv_table_set_cell_value(ui->screen_user_detail_table_1, 0, 0, "日用电 ");
        lv_table_set_cell_value(ui->screen_user_detail_table_1, 0, 1, "月用电 ");
        lv_table_set_cell_value(ui->screen_user_detail_table_1, 0, 2, "年用电 ");
        lv_table_set_cell_value(ui->screen_user_detail_table_1, 0, 3, "总用电 ");

        lv_table_set_cell_value(ui->screen_user_detail_table_1, 1, 0, "-- kWh");
        lv_table_set_cell_value(ui->screen_user_detail_table_1, 1, 1, "-- kWh");
        lv_table_set_cell_value(ui->screen_user_detail_table_1, 1, 2, "-- kWh");
        lv_table_set_cell_value(ui->screen_user_detail_table_1, 1, 3, "-- kWh");

        /* 单元格样式 */
        lv_obj_set_style_text_color(ui->screen_user_detail_table_1, lv_color_hex(0xE6EDF3), LV_PART_ITEMS);
        lv_obj_set_style_text_font(ui->screen_user_detail_table_1, &lv_font_SourceHanSerifSC_Regular_16, LV_PART_ITEMS);
        lv_obj_set_style_text_align(ui->screen_user_detail_table_1, LV_TEXT_ALIGN_CENTER, LV_PART_ITEMS);
        lv_obj_set_style_bg_opa(ui->screen_user_detail_table_1, 0, LV_PART_ITEMS);
        lv_obj_set_style_border_width(ui->screen_user_detail_table_1, 1, LV_PART_ITEMS);
        lv_obj_set_style_border_opa(ui->screen_user_detail_table_1, 60, LV_PART_ITEMS);
        lv_obj_set_style_border_color(ui->screen_user_detail_table_1, lv_color_hex(0x30363D), LV_PART_ITEMS);
        lv_obj_set_style_border_side(ui->screen_user_detail_table_1, LV_BORDER_SIDE_FULL, LV_PART_ITEMS);
        lv_obj_set_style_pad_top(ui->screen_user_detail_table_1, 8, LV_PART_ITEMS);
        lv_obj_set_style_pad_bottom(ui->screen_user_detail_table_1, 8, LV_PART_ITEMS);
    }

    /* ======== 更新时间条 ======== */
    {
        ui->screen_user_detail_cont_1 = lv_obj_create(ui->screen_user_detail);
        lv_obj_remove_style_all(ui->screen_user_detail_cont_1);
        lv_obj_set_size(ui->screen_user_detail_cont_1, 460, 28);
        lv_obj_set_pos(ui->screen_user_detail_cont_1, 10, 290);
        lv_obj_clear_flag(ui->screen_user_detail_cont_1, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_style_radius(ui->screen_user_detail_cont_1, 14, 0);
        lv_obj_set_style_bg_color(ui->screen_user_detail_cont_1, lv_color_hex(0x161B22), 0);
        lv_obj_set_style_bg_opa(ui->screen_user_detail_cont_1, 200, 0);
        lv_obj_set_style_border_width(ui->screen_user_detail_cont_1, 1, 0);
        lv_obj_set_style_border_color(ui->screen_user_detail_cont_1, lv_color_hex(0x30363D), 0);
        lv_obj_set_style_border_opa(ui->screen_user_detail_cont_1, 60, 0);
        lv_obj_set_style_pad_all(ui->screen_user_detail_cont_1, 0, 0);

        lv_obj_t *clk = lv_label_create(ui->screen_user_detail_cont_1);
        lv_label_set_text(clk, LV_SYMBOL_BELL);
        lv_obj_set_style_text_color(clk, lv_color_hex(0xF0C040), 0);
        lv_obj_set_style_text_font(clk, &lv_font_SourceHanSerifSC_Regular_12, 0);
        lv_obj_set_style_bg_opa(clk, 0, 0);
        lv_obj_align(clk, LV_ALIGN_LEFT_MID, 8, 0);

        ui->screen_user_detail_label_4 = lv_label_create(ui->screen_user_detail_cont_1);
        lv_label_set_text(ui->screen_user_detail_label_4, "更新时间:");
        lv_obj_set_style_text_color(ui->screen_user_detail_label_4, lv_color_hex(0x8B949E), 0);
        lv_obj_set_style_text_font(ui->screen_user_detail_label_4, &lv_font_SourceHanSerifSC_Regular_12, 0);
        lv_obj_set_style_bg_opa(ui->screen_user_detail_label_4, 0, 0);
        lv_obj_set_pos(ui->screen_user_detail_label_4, 28, 8);

        ui->screen_user_detail_label_time = lv_label_create(ui->screen_user_detail_cont_1);
        lv_label_set_text(ui->screen_user_detail_label_time, "----");
        lv_obj_set_style_text_color(ui->screen_user_detail_label_time, lv_color_hex(0xE6EDF3), 0);
        lv_obj_set_style_text_font(ui->screen_user_detail_label_time, &lv_font_SourceHanSerifSC_Regular_12, 0);
        lv_obj_set_style_bg_opa(ui->screen_user_detail_label_time, 0, 0);
        lv_obj_set_pos(ui->screen_user_detail_label_time, 104, 8);
    }

    lv_obj_update_layout(ui->screen_user_detail);
    events_init_screen_user_detail(ui);
}
