/*
* Copyright 2026 NXP
* NXP Proprietary. This software is owned or controlled by NXP and may only be used strictly in
* accordance with the applicable license terms. By expressly accepting such terms or by downloading, installing,
* activating and/or otherwise using the software, you are agreeing that you have read, and that you agree to
* comply with and be bound by, such license terms.  If you do not agree to be bound by the applicable license
* terms, then you may not retain, install, activate or otherwise use the software.
*/

#ifndef GUI_GUIDER_H
#define GUI_GUIDER_H
#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"

typedef struct
{
  
	lv_obj_t *Startup_screen;
	bool Startup_screen_del;
	lv_obj_t *Startup_screen_img_1;
	lv_obj_t *screen_user_home;
	bool screen_user_home_del;
	lv_obj_t *screen_user_home_card_solar;       /* 太阳能卡片(电压/电流/功率) */
	lv_obj_t *screen_user_home_card_solar_val;   /* 太阳能数值标签 */
	lv_obj_t *screen_user_home_card_device;      /* 设备在线卡片 */
	lv_obj_t *screen_user_home_card_device_val;  /* 设备数值标签 */
	lv_obj_t *screen_user_home_card_alert;       /* 告警卡片 */
	lv_obj_t *screen_user_home_card_alert_val;   /* 告警数值标签 */
	lv_obj_t *screen_user_home_cont_1;
	lv_obj_t *screen_user_home_label_Date;
	lv_obj_t *screen_user_home_digit_h1;     /* 小时十位 */
	lv_obj_t *screen_user_home_digit_h2;     /* 小时个位 */
	lv_obj_t *screen_user_home_digit_colon;  /* 冒号 ":" */
	lv_obj_t *screen_user_home_digit_m1;     /* 分钟十位 */
	lv_obj_t *screen_user_home_digit_m2;     /* 分钟个位 */
	lv_obj_t *screen_user_home_cont_2;
	lv_obj_t *screen_user_home_weather_icon;     /* 天气图标(彩色圆) */
	lv_obj_t *screen_user_home_daynight_dot;     /* 昼夜指示圆点 */
	lv_obj_t *screen_user_home_label_2;
	lv_obj_t *screen_user_home_label_1;
	lv_obj_t *screen_user_home_cont_3;
	lv_obj_t *screen_user_home_label_8;
	lv_obj_t *screen_user_home_label_ip;
	lv_obj_t *screen_user_home_label_10;
	lv_obj_t *screen_user_home_label_port;
	lv_obj_t *screen_user_list;
	bool screen_user_list_del;
	lv_obj_t *screen_user_list_label_1;
	lv_obj_t *screen_user_list_list_1;
	lv_obj_t *screen_user_detail;
	bool screen_user_detail_del;
	lv_obj_t *screen_user_detail_label_user;
	lv_obj_t *screen_user_detail_table_1;
	lv_obj_t *screen_user_detail_cont_1;
	lv_obj_t *screen_user_detail_label_4;
	lv_obj_t *screen_user_detail_label_time;
	lv_obj_t *screen_user_detail_chart;         /* 七日用电量柱形图 */
	lv_obj_t *screen_solar;                     /* 太阳能详情页 */
	bool screen_solar_del;
	lv_obj_t *screen_solar_chart;               /* 15日发电量折线图 */
	lv_obj_t *screen_solar_table;               /* 发电量统计表格 */
	lv_obj_t *screen_solar_cont_time;           /* 底部时间胶囊 */
	lv_obj_t *screen_solar_label_time;          /* 时间数值 */
	lv_obj_t *screen_alert;                     /* 告警页面 */
	bool screen_alert_del;
	lv_obj_t *screen_alert_list;                /* 告警列表 */
	lv_obj_t *screen_alert_label_count;         /* 告警总数标签 */
	lv_obj_t *screen_alert_stat_comm;           /* 通信异常计数标签 */
	lv_obj_t *screen_alert_stat_relay;          /* 开关异常计数标签 */
	lv_obj_t *screen_alert_stat_temp;           /* 温度异常计数标签 */
	lv_obj_t *screen_alert_stat_power;          /* 电源反接计数标签 */
	lv_obj_t *screen_alert_tip;                 /* 底部告警提示容器 */
	lv_obj_t *screen_alert_page_label;          /* 告警页码标签 */
	lv_obj_t *screen_tcp_setting;               /* TCP服务器设置页 */
	bool screen_tcp_setting_del;
	lv_obj_t *screen_tcp_setting_ta_ip;         /* IP地址输入框 */
	lv_obj_t *screen_tcp_setting_ta_port;       /* 端口输入框 */
	lv_obj_t *screen_tcp_setting_kb;            /* 虚拟键盘 */
	lv_obj_t *screen_user_home_btn_setting;     /* 首页底部设置按钮 */
	lv_obj_t *screen_sys_setting;               /* 系统设置页 */
	bool screen_sys_setting_del;
	lv_obj_t *screen_sys_setting_card_mac;      /* MAC地址卡片(可选中) */
	lv_obj_t *screen_sys_setting_card_building; /* 楼栋号卡片(可选中) */
	lv_obj_t *screen_sys_setting_card_tcp;      /* TCP服务器卡片(可选中,点击进入设置) */
	lv_obj_t *screen_sys_setting_label_mac;     /* MAC地址值 */
	lv_obj_t *screen_sys_setting_label_building;/* 楼栋号值 */
	lv_obj_t *screen_sys_setting_label_tcp;     /* TCP服务器IP:端口值 */
}lv_ui;

typedef void (*ui_setup_scr_t)(lv_ui * ui);

void ui_init_style(lv_style_t * style);

void ui_load_scr_animation(lv_ui *ui, lv_obj_t ** new_scr, bool new_scr_del, bool * old_scr_del, ui_setup_scr_t setup_scr,
                           lv_scr_load_anim_t anim_type, uint32_t time, uint32_t delay, bool is_clean, bool auto_del);

void ui_animation(void * var, int32_t duration, int32_t delay, int32_t start_value, int32_t end_value, lv_anim_path_cb_t path_cb,
                       uint16_t repeat_cnt, uint32_t repeat_delay, uint32_t playback_time, uint32_t playback_delay,
                       lv_anim_exec_xcb_t exec_cb, lv_anim_start_cb_t start_cb, lv_anim_ready_cb_t ready_cb, lv_anim_deleted_cb_t deleted_cb);


void init_scr_del_flag(lv_ui *ui);

void setup_ui(lv_ui *ui);

void init_keyboard(lv_ui *ui);

extern lv_ui guider_ui;


void setup_scr_Startup_screen(lv_ui *ui);
void setup_scr_screen_user_home(lv_ui *ui);
void setup_scr_screen_user_list(lv_ui *ui);
void setup_scr_screen_user_detail(lv_ui *ui);
void setup_scr_screen_solar(lv_ui *ui);
void setup_scr_screen_alert(lv_ui *ui);
void setup_scr_screen_tcp_setting(lv_ui *ui);
void setup_scr_screen_sys_setting(lv_ui *ui);
LV_FONT_DECLARE(lv_font_SourceHanSerifSC_Regular_16)
LV_FONT_DECLARE(lv_font_montserratMedium_48)
LV_FONT_DECLARE(lv_font_SourceHanSerifSC_Regular_12)
LV_FONT_DECLARE(lv_font_weather_16)


#ifdef __cplusplus
}
#endif
#endif
