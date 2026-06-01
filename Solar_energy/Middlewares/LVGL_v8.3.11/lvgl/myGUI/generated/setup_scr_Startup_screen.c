/*
 * Copyright 2026 NXP
 * NXP Proprietary. This software is owned or controlled by NXP and may only be used strictly in
 * accordance with the applicable license terms. By expressly accepting such terms or by downloading, installing,
 * activating and/or otherwise using the software, you are agreeing that you have read, and that you agree to
 * comply with and are bound by, such license terms.  If you do not agree to be bound by the applicable license
 * terms, then you may not retain, install, activate or otherwise use the software.
 *
 * SUNPOEM Startup Animation
 * =========================
 * Pure text-based boot logo with premium animation sequence.
 * No image resources needed — saves significant flash space.
 *
 * Animation Timeline (480x320 screen):
 *   0ms      Dark background, all elements invisible
 *   100ms    "SUNPOEM" starts fading in + sliding up + golden glow
 *   400ms    Golden accent line starts growing from center
 *   700ms    Subtitle "Solar Energy System" starts fading in
 *  ~1100ms   All animations complete, hold until screen transition
 *  ~1500ms   RTC_Task transitions to home screen
 */

#include "lvgl.h"
#include <stdio.h>
#include "gui_guider.h"
#include "events_init.h"
#include "widgets_init.h"
#include "custom.h"

/* ====== Animation Callbacks ====== */

/**
 * @brief  Animate object overall opacity
 */
static void anim_opa_cb(void * var, int32_t v)
{
    lv_obj_set_style_opa(var, (lv_opa_t)v, LV_PART_MAIN|LV_STATE_DEFAULT);
}

/**
 * @brief  Animate main text Y position (slide up effect)
 *         Uses lv_obj_align to keep horizontally centered
 */
static void main_text_slide_cb(void * var, int32_t v)
{
    lv_obj_align(var, LV_ALIGN_CENTER, 0, v);
}

/**
 * @brief  Animate golden accent line width growing from center
 *         Re-aligns to keep centered as width changes
 */
static void line_grow_cb(void * var, int32_t v)
{
    lv_obj_set_width(var, v);
    lv_obj_align(var, LV_ALIGN_CENTER, 0, 24);
}

/**
 * @brief  Animate text shadow opacity (golden glow effect)
 */
static void shadow_glow_cb(void * var, int32_t v)
{
    lv_obj_set_style_shadow_opa(var, (lv_opa_t)v, LV_PART_MAIN|LV_STATE_DEFAULT);
}

/* ====== Screen Setup ====== */

void setup_scr_Startup_screen(lv_ui *ui)
{
    /* Create screen */
    ui->Startup_screen = lv_obj_create(NULL);
    lv_obj_set_size(ui->Startup_screen, 480, 320);
    lv_obj_set_scrollbar_mode(ui->Startup_screen, LV_SCROLLBAR_MODE_OFF);

    /* Dark premium background */
    lv_obj_set_style_bg_color(ui->Startup_screen, lv_color_hex(0x0D1117),
                              LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->Startup_screen, LV_OPA_COVER,
                            LV_PART_MAIN|LV_STATE_DEFAULT);

    /* -------------------------------------------------------
     *  Main Logo Text "SUNPOEM"
     *  Font: Montserrat 48 (geometric sans-serif, premium feel)
     * ------------------------------------------------------- */
    lv_obj_t *label_main = lv_label_create(ui->Startup_screen);
    lv_label_set_text(label_main, "SUNPOEM");
    lv_obj_set_style_text_font(label_main, &lv_font_montserratMedium_48,
                               LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(label_main, lv_color_hex(0xFFFFFF),
                                LV_PART_MAIN|LV_STATE_DEFAULT);
    /* Start fully transparent */
    lv_obj_set_style_opa(label_main, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    /* Golden glow shadow (warm, solar-themed) */
    lv_obj_set_style_shadow_color(label_main, lv_color_hex(0xF0C040),
                                  LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(label_main, 25,
                                  LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_opa(label_main, 0,
                                LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_ofs_x(label_main, 0,
                                  LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_ofs_y(label_main, 0,
                                  LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_spread(label_main, 4,
                                   LV_PART_MAIN|LV_STATE_DEFAULT);
    /* Initial position: slightly below center (animation will slide up) */
    lv_obj_align(label_main, LV_ALIGN_CENTER, 0, 15);

    /* Store in ui struct for compatibility */
    ui->Startup_screen_img_1 = label_main;

    /* -------------------------------------------------------
     *  Golden Accent Line
     *  Thin decorative line below the logo text
     * ------------------------------------------------------- */
    lv_obj_t *line_accent = lv_obj_create(ui->Startup_screen);
    lv_obj_remove_style_all(line_accent);
    lv_obj_set_size(line_accent, 0, 2);
    lv_obj_set_style_bg_color(line_accent, lv_color_hex(0xF0C040),
                              LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(line_accent, LV_OPA_COVER,
                            LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(line_accent, 1,
                            LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_color(line_accent, lv_color_hex(0xF0C040),
                                  LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(line_accent, 8,
                                  LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_opa(line_accent, 60,
                                LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_clear_flag(line_accent, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(line_accent, LV_ALIGN_CENTER, 0, 24);

    /* -------------------------------------------------------
     *  Subtitle Text
     * ------------------------------------------------------- */
    lv_obj_t *label_sub = lv_label_create(ui->Startup_screen);
    lv_label_set_text(label_sub, "Solar Energy System");
    lv_obj_set_style_text_font(label_sub, &lv_font_SourceHanSerifSC_Regular_16,
                               LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(label_sub, lv_color_hex(0x8899AA),
                                LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_opa(label_sub, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    /* Position accounting for letter spacing offset: centered under line */
    lv_obj_align(label_sub, LV_ALIGN_CENTER, 8, 52);

    /* -------------------------------------------------------
     *  Bottom Copyright Text (static, no animation)
     * ------------------------------------------------------- */
    lv_obj_t *label_copy = lv_label_create(ui->Startup_screen);
    lv_label_set_text(label_copy, LV_SYMBOL_COPY " 2026 SUNPOEM");
    lv_obj_set_style_text_font(label_copy, &lv_font_SourceHanSerifSC_Regular_12,
                               LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(label_copy, lv_color_hex(0x3A4550),
                                LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_align(label_copy, LV_ALIGN_BOTTOM_MID, 0, -8);

    /* =======================================================
     *  Animation Sequence
     * ======================================================= */
    lv_anim_t a;

    /* --- 1. Main text: Fade in (100ms → 700ms) --- */
    lv_anim_init(&a);
    lv_anim_set_var(&a, label_main);
    lv_anim_set_exec_cb(&a, anim_opa_cb);
    lv_anim_set_values(&a, 0, 255);
    lv_anim_set_time(&a, 600);
    lv_anim_set_delay(&a, 100);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
    lv_anim_start(&a);

    /* --- 2. Main text: Golden glow (100ms → 700ms) --- */
    lv_anim_init(&a);
    lv_anim_set_var(&a, label_main);
    lv_anim_set_exec_cb(&a, shadow_glow_cb);
    lv_anim_set_values(&a, 0, 100);
    lv_anim_set_time(&a, 600);
    lv_anim_set_delay(&a, 100);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
    lv_anim_start(&a);

    /* --- 3. Main text: Slide up (100ms → 700ms) --- */
    lv_anim_init(&a);
    lv_anim_set_var(&a, label_main);
    lv_anim_set_exec_cb(&a, main_text_slide_cb);
    lv_anim_set_values(&a, 15, -10);
    lv_anim_set_time(&a, 600);
    lv_anim_set_delay(&a, 100);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
    lv_anim_start(&a);

    /* --- 4. Accent line: Grow from center (400ms → 900ms) --- */
    lv_anim_init(&a);
    lv_anim_set_var(&a, line_accent);
    lv_anim_set_exec_cb(&a, line_grow_cb);
    lv_anim_set_values(&a, 0, 280);
    lv_anim_set_time(&a, 500);
    lv_anim_set_delay(&a, 400);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
    lv_anim_start(&a);

    /* --- 5. Subtitle: Fade in (700ms → 1100ms) --- */
    lv_anim_init(&a);
    lv_anim_set_var(&a, label_sub);
    lv_anim_set_exec_cb(&a, anim_opa_cb);
    lv_anim_set_values(&a, 0, 200);
    lv_anim_set_time(&a, 400);
    lv_anim_set_delay(&a, 700);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
    lv_anim_start(&a);

    /* Update layout */
    lv_obj_update_layout(ui->Startup_screen);
}
