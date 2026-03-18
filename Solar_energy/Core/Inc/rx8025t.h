/**
  ******************************************************************************
  * @file    rx8025t.h
  * @brief   RX-8025T RTC driver header file
  ******************************************************************************
  * @attention
  * RX-8025T I2C Address: 0x32 (7-bit address)
  * I2C Standard Speed: 100kHz
  ******************************************************************************
  */

#ifndef __RX8025T_H
#define __RX8025T_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "i2c.h"

/* Exported types ------------------------------------------------------------*/

/**
  * @brief  时间结构定义
  */
typedef struct
{
    uint8_t seconds;    ///< Seconds: 0-59
    uint8_t minutes;    ///< Minutes: 0-59
    uint8_t hours;      ///< Hours: 0-23 (24-hour format)
} RX8025T_Time;

/**
  * @brief  日期结构定义
  */
typedef struct
{
    uint8_t year;       ///< Year: 0-99 (2000-2099)
    uint8_t month;      ///< Month: 1-12
    uint8_t day;        ///< Day: 1-31
    uint8_t weekday;    ///< Weekday: 0-6 (0=Sunday, 1=Monday, ..., 6=Saturday)
} RX8025T_Date;

/**
  * @brief  日期和时间结构定义
  */
typedef struct
{
    RX8025T_Date date;   ///< Date part
    RX8025T_Time time;   ///< Time part
} RX8025T_DateTime;

/* 直接访问的替代结构 */
typedef struct
{
    uint8_t seconds;    ///< Seconds: 0-59
    uint8_t minutes;    ///< Minutes: 0-59
    uint8_t hours;      ///< Hours: 0-23 (24-hour format)
    uint8_t weekday;    ///< Weekday: 0-6 (0=Sunday, 1=Monday, ..., 6=Saturday)
    uint8_t day;        ///< Day: 1-31
    uint8_t month;      ///< Month: 1-12
    uint8_t year;       ///< Year: 0-99 (2000-2099)
} RX8025T_DateTimeCompact;

/* 导出常量 --------------------------------------------------------*/
/* 此驱动程序中没有导出的常量 */

/* 导出的宏 ------------------------------------------------------------*/
/* 此驱动程序中没有导出的宏 */

/* 导出函数原型 ---------------------------------------------*/

/**
  * @brief  初始化 RX-8025T 实时时钟
  * @note   此功能检查通信，清除低电压标志，并确保实时时钟正在运行
  * @retval HAL 状态
  */
HAL_StatusTypeDef RX8025T_Init(void);

/**
  * @brief  Read current time from RX-8025T
  * @param  time: Pointer to RX8025T_Time structure to store the time
  * @retval HAL status
  */
HAL_StatusTypeDef RX8025T_GetTime(RX8025T_Time *time);

/**
  * @brief  Read current date from RX-8025T
  * @param  date: Pointer to RX8025T_Date structure to store the date
  * @retval HAL status
  */
HAL_StatusTypeDef RX8025T_GetDate(RX8025T_Date *date);

/**
  * @brief  Read complete date and time from RX-8025T
  * @param  datetime: Pointer to RX8025T_DateTimeCompact structure to store the date and time
  * @retval HAL status
  */
HAL_StatusTypeDef RX8025T_GetDateTime(RX8025T_DateTimeCompact *datetime);

/**
  * @brief  Set time to RX-8025T
  * @param  time: Pointer to RX8025T_Time structure containing the time to set
  * @retval HAL status
  */
HAL_StatusTypeDef RX8025T_SetTime(RX8025T_Time *time);

/**
  * @brief  Set date to RX-8025T
  * @param  date: Pointer to RX8025T_Date structure containing the date to set
  * @retval HAL status
  */
HAL_StatusTypeDef RX8025T_SetDate(RX8025T_Date *date);

/**
  * @brief  Set complete date and time to RX-8025T
  * @param  datetime: Pointer to RX8025T_DateTimeCompact structure containing the date and time to set
  * @retval HAL status
  */
HAL_StatusTypeDef RX8025T_SetDateTime(RX8025T_DateTimeCompact *datetime);

/**
  * @brief  Get formatted date and time string
  * @param  buffer: Buffer to store the formatted string
  * @param  size: Size of the buffer (minimum 20 bytes recommended)
  * @retval HAL status
  * @note   Format: "2026-03-12 09:18:30"
  */
HAL_StatusTypeDef RX8025T_GetDateTimeString(char *buffer, uint16_t size);

/**
  * @brief  Get formatted date string
  * @param  buffer: Buffer to store the formatted string
  * @param  size: Size of the buffer (minimum 11 bytes recommended)
  * @retval HAL status
  * @note   Format: "2026-03-12"
  */
HAL_StatusTypeDef RX8025T_GetDateString(char *buffer, uint16_t size);

/**
  * @brief  Get formatted time string
  * @param  buffer: Buffer to store the formatted string
  * @param  size: Size of the buffer (minimum 9 bytes recommended)
  * @retval HAL status
  * @note   Format: "09:18:30"
  */
HAL_StatusTypeDef RX8025T_GetTimeString(char *buffer, uint16_t size);

/**
  * @brief  Get weekday name string
  * @param  weekday: Weekday number (0-6, 0=Sunday)
  * @param  buffer: Buffer to store the weekday name
  * @param  size: Size of the buffer (minimum 10 bytes recommended)
  * @retval HAL status
  */
HAL_StatusTypeDef RX8025T_GetWeekdayName(uint8_t weekday, char *buffer, uint16_t size);

#ifdef __cplusplus
}
#endif

#endif /* __RX8025T_H */
