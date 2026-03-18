/**
  ******************************************************************************
  * @file    rx8025t_example.c
  * @brief   RX-8025T RTC example - Display time on LCD
  ******************************************************************************
  * @attention
  * This example demonstrates how to:
  * - Initialize RX-8025T RTC
  * - Read date and time
  * - Display on ILI9488 LCD using LVGL
  ******************************************************************************
  */

#include "rx8025t_example.h"
#include "rx8025t.h"
#include "ili9488.h"
#include <stdio.h>
#include <string.h>
#include "gui_guider.h"           // Gui Guider 生成的界面和控件的声明

/* LCD显示位置定义 */
#define TIME_LABEL_X        50
#define TIME_LABEL_Y        150
#define TIME_VALUE_X        150
#define TIME_VALUE_Y        150

#define DATE_LABEL_X        50
#define DATE_LABEL_Y        200
#define DATE_VALUE_X        150
#define DATE_VALUE_Y        200

#define WEEKDAY_LABEL_X     50
#define WEEKDAY_LABEL_Y     250
#define WEEKDAY_VALUE_X     150
#define WEEKDAY_VALUE_Y     250

/* 颜色定义 */
#define TEXT_COLOR          WHITE
#define LABEL_COLOR         LIGHTGRAY
#define BACKGROUND_COLOR    BLACK

/**
  * @brief  Update time display on LCD
  * @param  None
  * @retval HAL status
  */
HAL_StatusTypeDef RX8025T_UpdateTimeDisplay(void)
{
    RX8025T_DateTimeCompact datetime;
    char time_str[20];
    char date_str[20];
    char weekday_str[15];
    HAL_StatusTypeDef status;
    
    /* 读取日期和时间 */
    status = RX8025T_GetDateTime(&datetime);
    if (status != HAL_OK) {
        return status;
    }
    
    /* 格式化时间字符串 */
    lv_snprintf(time_str, sizeof(time_str), "%02d:%02d:%02d",
             datetime.hours, datetime.minutes, datetime.seconds);
    
    /* 格式化日期字符串 */
    lv_snprintf(date_str, sizeof(date_str), "20%02d-%02d-%02d",
             datetime.year, datetime.month, datetime.day);
    
//    /* 获取星期名称 */
//    RX8025T_GetWeekdayName(datetime.weekday, weekday_str, sizeof(weekday_str));
		
		/* 刷新 UI 文本 */
    lv_label_set_text(guider_ui.screen_user_home_label_Date, date_str);
		lv_label_set_text(guider_ui.screen_user_home_label_Time, time_str);

    return HAL_OK;
}

/**
  * @brief  Initialize RTC and display system
  * @param  None
  * @retval HAL status
  */
HAL_StatusTypeDef RX8025T_InitAndDisplay(void)
{
    HAL_StatusTypeDef status;
    RX8025T_DateTimeCompact initial_time;
    
    /* 初始化RX-8025T */
    status = RX8025T_Init();
    if (status != HAL_OK) {
        return status;
    }
    

    initial_time.seconds = 50;
    initial_time.minutes = 59;
    initial_time.hours = 12;
    initial_time.weekday = 3;  // Wednesday
    initial_time.day = 12;
    initial_time.month = 3;
    initial_time.year = 26;     // 2026
    
    status = RX8025T_SetDateTime(&initial_time);
    if (status != HAL_OK) {
        return status;
    }

    
    /* 第一次更新显示 */
    status = RX8025T_UpdateTimeDisplay();
    
    return status;
}

/**
  * @brief  RTC task for periodic update (call this in main loop or timer)
  * @param  None
  * @retval HAL status
  */
HAL_StatusTypeDef RX8025T_Task(void)
{
    static uint8_t last_second = 0xFF;
    RX8025T_Time current_time;
    HAL_StatusTypeDef status;
    
    /* 读取当前时间 */
    status = RX8025T_GetTime(&current_time);
    if (status != HAL_OK) {
        return status;
    }
//		//获取当前活动页面,这个判断页面,通过判断切换到主页的方式可能遇到主页已经切换但是标签还没生成的情况，就可能会访问到野指针,所以我直接判断标签组件是否有效
//    lv_obj_t *current_scr = lv_scr_act();
	
    /* 只有当秒数变化并且当前页面为主页时时才更新显示&& current_scr == guider_ui.screen_user_home */
    if (current_time.seconds != last_second && lv_obj_is_valid(guider_ui.screen_user_home_label_Date) && lv_obj_is_valid(guider_ui.screen_user_home_label_Time)) 
		{
        last_second = current_time.seconds;
        status = RX8025T_UpdateTimeDisplay();
    }
    
    return status;
}

/**
  * @brief  Display formatted date and time on specific position
  * @param  x: X position
  * @param  y: Y position
  * @param  color: Text color
  * @retval HAL status
  */
HAL_StatusTypeDef RX8025T_DisplayDateTime(uint16_t x, uint16_t y, uint16_t color)
{
    char datetime_str[30];
    HAL_StatusTypeDef status;
    
    /* 获取格式化的日期时间字符串 */
    status = RX8025T_GetDateTimeString(datetime_str, sizeof(datetime_str));
    if (status != HAL_OK) {
        return status;
    }
    
    /* 显示字符串 */
//    ILI9488_DrawString(x, y, datetime_str, color);
    
    return HAL_OK;
}

/**
  * @brief  Display simple time on specific position
  * @param  x: X position
  * @param  y: Y position
  * @param  color: Text color
  * @retval HAL status
  */
HAL_StatusTypeDef RX8025T_DisplayTime(uint16_t x, uint16_t y, uint16_t color)
{
    char time_str[20];
    HAL_StatusTypeDef status;
    
    /* 获取格式化的时间字符串 */
    status = RX8025T_GetTimeString(time_str, sizeof(time_str));
    if (status != HAL_OK) {
        return status;
    }
    
    /* 显示字符串 */
//    ILI9488_DrawString(x, y, time_str, color);
    
    return HAL_OK;
}

/**
  * @brief  Display simple date on specific position
  * @param  x: X position
  * @param  y: Y position
  * @param  color: Text color
  * @retval HAL status
  */
HAL_StatusTypeDef RX8025T_DisplayDate(uint16_t x, uint16_t y, uint16_t color)
{
    char date_str[20];
    HAL_StatusTypeDef status;
    
    /* 获取格式化的日期字符串 */
    status = RX8025T_GetDateString(date_str, sizeof(date_str));
    if (status != HAL_OK) {
        return status;
    }
    
    /* 显示字符串 */
//    ILI9488_DrawString(x, y, date_str, color);
    
    return HAL_OK;
}

/**
  * @brief  Simple test function for RX-8025T
  * @param  None
  * @retval HAL status
  */
HAL_StatusTypeDef RX8025T_Test(void)
{
    RX8025T_DateTimeCompact dt;
    HAL_StatusTypeDef status;
    
    printf("RX-8025T Test Starting...\n\r");
    
    /* 初始化 */
    status = RX8025T_Init();
    if (status != HAL_OK) {
        printf("RX-8025T Init Failed: %d\n\r", status);
        return status;
    }
    printf("RX-8025T Init OK\n\r");
    
    /* 读取时间 */
    status = RX8025T_GetDateTime(&dt);
    if (status != HAL_OK) {
        printf("RX-8025T Read Failed: %d\n\r", status);
        return status;
    }
    
    printf("Current Time: 20%02d-%02d-%02d %02d:%02d:%02d\n\r",
           dt.year, dt.month, dt.day,
           dt.hours, dt.minutes, dt.seconds);
    
    /* 显示在LCD上 */
    status = RX8025T_DisplayDateTime(50, 100, WHITE);
    
    return status;
}
