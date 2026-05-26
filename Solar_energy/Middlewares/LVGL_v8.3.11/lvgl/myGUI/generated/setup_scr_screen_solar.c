/*
* Copyright 2026 NXP
* NXP Confidential - DO NOT SHARE
*/

#include "lvgl.h"
#include <stdio.h>
#include "gui_guider.h"
#include "events_init.h"
#include "widgets_init.h"
#include "custom.h"

/* 假数据 - 稍后用 SD 卡读取替换 */

static const float solar_daily_kwh[15] = {
    15.2f, 18.5f, 12.0f, 21.0f, 19.5f,
    17.5f, 19.1f, 22.3f, 16.8f, 14.5f,
    20.1f, 23.5f, 18.7f, 15.9f, 21.6f
};

static const float solar_total_kwh  = 12568.5f;
static const float solar_year_kwh   = 3245.8f;
static const float solar_month_kwh  = 256.4f;
static const float solar_today_kwh  = 21.6f;

static void fmt_val(char *buf, int val_x10)
{
    int a = val_x10 < 0 ? -val_x10 : val_x10;
    int ipart = a / 10;
    int dpart = a % 10;
    if(val_x10 < 0) lv_snprintf(buf, 32, "-%d.%d", ipart, dpart);
    else             lv_snprintf(buf, 32, "%d.%d", ipart, dpart);
}

static int f2x10(float v)
{
    return (v >= 0) ? (int)(v * 10 + 0.5f) : (int)(v * 10 - 0.5f);
}

/**
 * @brief  Chart draw callback: draw value labels above each data point/bar
 * @note   Values are stored as ×10 integers (e.g., 15.2 → 152), displayed as "15.2"
 */
static void chart_draw_value_label_cb(lv_event_t *e)
{
    lv_obj_draw_part_dsc_t * dsc = lv_event_get_draw_part_dsc(e);
    if(dsc->part != LV_PART_ITEMS) return;

    lv_obj_t * chart = lv_event_get_target(e);

    /* For line charts, skip line-segment events (draw_area is large) */
    if(lv_chart_get_type(chart) == LV_CHART_TYPE_LINE) {
        if(lv_area_get_width(dsc->draw_area) > 10 ||
           lv_area_get_height(dsc->draw_area) > 10)
            return;
    }

    lv_chart_series_t * ser = lv_chart_get_series_next(chart, NULL);
    if(!ser) return;
    if(dsc->id >= lv_chart_get_point_count(chart)) return;

    int16_t val = ser->y_points[dsc->id];
    if(val == LV_CHART_POINT_NONE) return;

    /* Format value (×10 → "x.y") */
    char txt[16];
    int a = (val < 0) ? -val : val;
    if(val < 0) lv_snprintf(txt, sizeof(txt), "-%d.%d", a / 10, a % 10);
    else         lv_snprintf(txt, sizeof(txt), "%d.%d", a / 10, a % 10);

    /* Draw label above point/bar */
    lv_draw_label_dsc_t lbl;
    lv_draw_label_dsc_init(&lbl);
    lbl.color = lv_color_hex(0x333333);
    lbl.font = &lv_font_montserratMedium_12;
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

void setup_scr_screen_solar(lv_ui *ui)
{
    ui->screen_solar = lv_obj_create(NULL);
    lv_obj_set_size(ui->screen_solar, 480, 320);
    lv_obj_set_scrollbar_mode(ui->screen_solar, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_bg_color(ui->screen_solar, lv_color_hex(0xF0F2F5), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->screen_solar, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);

    /* header (40px) */
    {
        lv_obj_t * h = lv_obj_create(ui->screen_solar);
        lv_obj_remove_style_all(h);
        lv_obj_set_size(h, 480, 40);
        lv_obj_set_pos(h, 0, 0);
        lv_obj_set_style_bg_color(h, lv_color_hex(0x2C3E50), 0);
        lv_obj_set_style_bg_opa(h, LV_OPA_COVER, 0);
        lv_obj_clear_flag(h, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_style_pad_all(h, 0, 0);

        lv_obj_t * t = lv_label_create(h);
        lv_label_set_text(t, LV_SYMBOL_LEFT " 太阳能发电量");
        lv_obj_set_style_text_color(t, lv_color_hex(0xffffff), 0);
        lv_obj_set_style_text_font(t, &lv_font_SourceHanSerifSC_Regular_16, 0);
        lv_obj_set_style_bg_opa(t, 0, 0);
        lv_obj_set_pos(t, 14, 11);
    }

    /* line chart card: y=42 to y=188 = 146px */
    {
        lv_obj_t * c = lv_obj_create(ui->screen_solar);
        lv_obj_remove_style_all(c);
        lv_obj_set_size(c, 464, 146);
        lv_obj_set_pos(c, 8, 42);
        lv_obj_set_style_radius(c, 8, 0);
        lv_obj_set_style_bg_color(c, lv_color_hex(0xffffff), 0);
        lv_obj_set_style_bg_opa(c, LV_OPA_COVER, 0);
        lv_obj_set_style_shadow_width(c, 4, 0);
        lv_obj_set_style_shadow_opa(c, 20, 0);
        lv_obj_set_style_shadow_ofs_y(c, 2, 0);
        lv_obj_set_style_border_width(c, 0, 0);
        lv_obj_clear_flag(c, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_style_pad_all(c, 4, 0);

        lv_obj_t * ct = lv_label_create(c);
        lv_label_set_text(ct, "近15日发电量 (kWh)");
        lv_obj_set_style_text_color(ct, lv_color_hex(0x2C3E50), 0);
        lv_obj_set_style_text_font(ct, &lv_font_SourceHanSerifSC_Regular_16, 0);
        lv_obj_set_style_bg_opa(ct, 0, 0);
        lv_obj_set_pos(ct, 6, 2);

        lv_obj_t * ch = lv_chart_create(c);
        lv_obj_set_size(ch, 448, 105);
        lv_obj_set_pos(ch, 4, 20);
        lv_obj_set_style_bg_opa(ch, LV_OPA_TRANSP, LV_PART_MAIN);
        lv_obj_set_style_border_width(ch, 1, LV_PART_MAIN);
        lv_obj_set_style_border_color(ch, lv_color_hex(0xE0E0E0), LV_PART_MAIN);
        lv_chart_set_type(ch, LV_CHART_TYPE_LINE);
        lv_chart_set_point_count(ch, 15);
        lv_chart_set_div_line_count(ch, 4, 0);
        lv_chart_set_range(ch, LV_CHART_AXIS_PRIMARY_Y, 0, 300);

        lv_chart_series_t * s = lv_chart_add_series(ch, lv_color_hex(0x2196F3), LV_CHART_AXIS_PRIMARY_Y);
        for(int i = 0; i < 15; i++){
            s->y_points[i] = (int16_t)f2x10(solar_daily_kwh[i]);
        }
        lv_chart_refresh(ch);
        lv_obj_set_style_size(ch, 4, LV_PART_INDICATOR);
        lv_obj_add_event_cb(ch, chart_draw_value_label_cb, LV_EVENT_DRAW_PART_END, NULL);
    }

    /* table card: y=190 to y=318 = 128px */
    {
        lv_obj_t * tc = lv_obj_create(ui->screen_solar);
        lv_obj_remove_style_all(tc);
        lv_obj_set_size(tc, 464, 128);
        lv_obj_set_pos(tc, 8, 190);
        lv_obj_set_style_radius(tc, 8, 0);
        lv_obj_set_style_bg_color(tc, lv_color_hex(0xffffff), 0);
        lv_obj_set_style_bg_opa(tc, LV_OPA_COVER, 0);
        lv_obj_set_style_shadow_width(tc, 4, 0);
        lv_obj_set_style_shadow_opa(tc, 20, 0);
        lv_obj_set_style_shadow_ofs_y(tc, 2, 0);
        lv_obj_set_style_border_width(tc, 0, 0);
        lv_obj_clear_flag(tc, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_style_pad_all(tc, 4, 0);

        lv_obj_t * tt = lv_label_create(tc);
        lv_label_set_text(tt, "发电量统计 (kWh)");
        lv_obj_set_style_text_color(tt, lv_color_hex(0x2C3E50), 0);
        lv_obj_set_style_text_font(tt, &lv_font_SourceHanSerifSC_Regular_16, 0);
        lv_obj_set_style_bg_opa(tt, 0, 0);
        lv_obj_set_pos(tt, 6, 2);

        lv_obj_t * tb = lv_table_create(tc);
        lv_obj_set_pos(tb, 4, 20);
        lv_obj_set_style_text_font(tb, &lv_font_SourceHanSerifSC_Regular_16, LV_PART_MAIN);
        lv_obj_set_style_border_width(tb, 1, LV_PART_MAIN);
        lv_obj_set_style_border_color(tb, lv_color_hex(0xE0E0E0), LV_PART_MAIN);
        lv_obj_set_style_pad_left(tb, 6, LV_PART_MAIN);
        lv_obj_set_style_pad_right(tb, 6, LV_PART_MAIN);
        lv_obj_set_style_pad_top(tb, 4, LV_PART_MAIN);
        lv_obj_set_style_pad_bottom(tb, 4, LV_PART_MAIN);
        lv_obj_set_style_bg_color(tb, lv_color_hex(0xffffff), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(tb, LV_OPA_COVER, LV_PART_MAIN);

        lv_table_set_col_cnt(tb, 4);
        lv_table_set_row_cnt(tb, 2);
        lv_table_set_col_width(tb, 0, 106);
        lv_table_set_col_width(tb, 1, 106);
        lv_table_set_col_width(tb, 2, 106);
        lv_table_set_col_width(tb, 3, 106);

        lv_table_set_cell_value(tb, 0, 0, "总发电 ");
        lv_table_set_cell_value(tb, 0, 1, "年发电 ");
        lv_table_set_cell_value(tb, 0, 2, "月发电 ");
        lv_table_set_cell_value(tb, 0, 3, "日发电 ");

        char b0[32], b1[32], b2[32], b3[32];
        fmt_val(b0, f2x10(solar_total_kwh));
        fmt_val(b1, f2x10(solar_year_kwh));
        fmt_val(b2, f2x10(solar_month_kwh));
        fmt_val(b3, f2x10(solar_today_kwh));

        lv_table_set_cell_value(tb, 1, 0, b0);
        lv_table_set_cell_value(tb, 1, 1, b1);
        lv_table_set_cell_value(tb, 1, 2, b2);
        lv_table_set_cell_value(tb, 1, 3, b3);
    }

    lv_obj_update_layout(ui->screen_solar);
    events_init_screen_solar(ui);
}
