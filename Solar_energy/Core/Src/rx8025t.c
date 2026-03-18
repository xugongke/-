/**
  ******************************************************************************
  * @file    rx8025t.c
  * @brief   RX-8025T RTC driver implementation
  * @details This file provides firmware functions to manage the following 
  *          functionalities of the RX-8025T Real Time Clock:
  *           + Initialization
  *           + Time and Date reading/writing
  *           + Alarm functions
  *           + Configuration
  ******************************************************************************
  * @attention
  * RX-8025T I2C Address: 0x32 (7-bit address)
  * I2C Standard Speed: 100kHz
  ******************************************************************************
  */

#include "rx8025t.h"
#include <stdio.h>
#include "gui_guider.h"           // Gui Guider 生成的界面和控件的声明
/* 私有定义 */
#define RX8025T_I2C_ADDR    0x32    // 7位I2C地址

/* 寄存器地址 */
#define RX8025T_REG_SEC     0x00    // Seconds (00-59)
#define RX8025T_REG_MIN     0x01    // Minutes (00-59)
#define RX8025T_REG_HOUR    0x02    // Hours (00-23)
#define RX8025T_REG_WDAY    0x03    // Weekday (00-06, 0=Sunday)
#define RX8025T_REG_DAY     0x04    // Day (01-31)
#define RX8025T_REG_MONTH   0x05    // Month (01-12)
#define RX8025T_REG_YEAR    0x06    // Year (00-99)
#define RX8025T_REG_RES     0x07    // Reserved
#define RX8025T_REG_ALMIN   0x08    // Alarm Minute
#define RX8025T_REG_ALHOUR  0x09    // Alarm Hour
#define RX8025T_REG_ALWDAY  0x0A    // Alarm Weekday
#define RX8025T_REG_CNTA    0x0B    // Counter A
#define RX8025T_REG_CNTB    0x0C    // Counter B
#define RX8025T_REG_EXT     0x0D    // Extension register
#define RX8025T_REG_FLAG    0x0E    // Flag register
#define RX8025T_REG_CTRL    0x0F    // Control register

/* 标志寄存器位 */
#define RX8025T_FLAG_VLF    0x01    // Voltage Low Flag
#define RX8025T_FLAG_AF     0x02    // Alarm Flag
#define RX8025T_FLAG_TF    0x04    // Timer Flag
#define RX8025T_FLAG_UF     0x08    // Update Flag

/* 控制寄存器位 */
#define RX8025T_CTRL_TEST   0x01    // Test mode
#define RX8025T_CTRL_RESET  0x02    // System reset
#define RX8025T_CTRL_AIE    0x10    // Alarm Interrupt Enable
#define RX8025T_CTRL_TIE    0x20    // Timer Interrupt Enable
#define RX8025T_CTRL_UIE    0x40    // Update Interrupt Enable
#define RX8025T_CTRL_STOP   0x80    // Stop bit

/* 扩展寄存器位 */
#define RX8025T_EXT_TE      0x04    // Timer Enable
#define RX8025T_EXT_FSEL    0x08    // Frequency Select

/* 私有函数原型 */
static HAL_StatusTypeDef RX8025T_ReadRegister(uint8_t reg, uint8_t *data);
static HAL_StatusTypeDef RX8025T_WriteRegister(uint8_t reg, uint8_t data);
static uint8_t RX8025T_BCD2Dec(uint8_t bcd);
static uint8_t RX8025T_Dec2BCD(uint8_t dec);

/**
  * @brief  Initialize RX-8025T RTC
  * @retval HAL status
  */
HAL_StatusTypeDef RX8025T_Init(void)
{
    uint8_t data;
    HAL_StatusTypeDef status;

    // 检查通信
    status = RX8025T_ReadRegister(RX8025T_REG_SEC, &data);
    if (status != HAL_OK) {
        return status;
    }

    // 检查电压低标志
    status = RX8025T_ReadRegister(RX8025T_REG_FLAG, &data);
    if (status != HAL_OK) {
        return status;
    }

    if (data & RX8025T_FLAG_VLF) {
        // 清除电压低标志
        data &= ~RX8025T_FLAG_VLF;
        status = RX8025T_WriteRegister(RX8025T_REG_FLAG, data);
        if (status != HAL_OK) {
            return status;
        }
    }

    // 确保 RTC 正在运行（清除 STOP 位）
    status = RX8025T_ReadRegister(RX8025T_REG_CTRL, &data);
    if (status != HAL_OK) {
        return status;
    }

    if (data & RX8025T_CTRL_STOP) {
        data &= ~RX8025T_CTRL_STOP;
        status = RX8025T_WriteRegister(RX8025T_REG_CTRL, data);
        if (status != HAL_OK) {
            return status;
        }
    }

    return HAL_OK;
}

/**
  * @brief  从 RX-8025T 读取当前时间
  * @param  time: Pointer to RX8025T_Time structure
  * @retval HAL status
  */
HAL_StatusTypeDef RX8025T_GetTime(RX8025T_Time *time)
{
    uint8_t data[3];
    HAL_StatusTypeDef status;

    // Read seconds, minutes, hours
    status = HAL_I2C_Mem_Read(&hi2c2, RX8025T_I2C_ADDR << 1, RX8025T_REG_SEC,
                             I2C_MEMADD_SIZE_8BIT, data, 3, HAL_MAX_DELAY);
    if (status != HAL_OK) {
        return status;
    }

    // Convert BCD to decimal and mask 24-hour format bits
    time->seconds = RX8025T_BCD2Dec(data[0] & 0x7F);
    time->minutes = RX8025T_BCD2Dec(data[1] & 0x7F);
    time->hours = RX8025T_BCD2Dec(data[2] & 0x3F); // 24-hour format

    return HAL_OK;
}

/**
  * @brief  从 RX-8025T 读取当前日期
  * @param  date: Pointer to RX8025T_Date structure
  * @retval HAL status
  */
HAL_StatusTypeDef RX8025T_GetDate(RX8025T_Date *date)
{
    uint8_t data[4];
    HAL_StatusTypeDef status;

    // Read weekday, day, month, year
    status = HAL_I2C_Mem_Read(&hi2c2, RX8025T_I2C_ADDR << 1, RX8025T_REG_WDAY,
                             I2C_MEMADD_SIZE_8BIT, data, 4, HAL_MAX_DELAY);
    if (status != HAL_OK) {
        return status;
    }

    // Convert BCD to decimal
    date->weekday = data[0] & 0x07;
    date->day = RX8025T_BCD2Dec(data[1] & 0x3F);
    date->month = RX8025T_BCD2Dec(data[2] & 0x1F);
    date->year = RX8025T_BCD2Dec(data[3]);

    return HAL_OK;
}

/**
  * @brief  从 RX-8025T 读取完整日期和时间
  * @param  datetime: Pointer to RX8025T_DateTime structure
  * @retval HAL status
  */
HAL_StatusTypeDef RX8025T_GetDateTime(RX8025T_DateTimeCompact *datetime)
{
    uint8_t data[7];
    HAL_StatusTypeDef status;

    // Read all time registers
    status = HAL_I2C_Mem_Read(&hi2c2, RX8025T_I2C_ADDR << 1, RX8025T_REG_SEC,
                             I2C_MEMADD_SIZE_8BIT, data, 7, HAL_MAX_DELAY);
    if (status != HAL_OK) {
        return status;
    }

    // Convert BCD to decimal
    datetime->seconds = RX8025T_BCD2Dec(data[0] & 0x7F);
    datetime->minutes = RX8025T_BCD2Dec(data[1] & 0x7F);
    datetime->hours = RX8025T_BCD2Dec(data[2] & 0x3F);
    datetime->weekday = data[3] & 0x07;
    datetime->day = RX8025T_BCD2Dec(data[4] & 0x3F);
    datetime->month = RX8025T_BCD2Dec(data[5] & 0x1F);
    datetime->year = RX8025T_BCD2Dec(data[6]);

    return HAL_OK;
}

/**
  * @brief  Set time to RX-8025T
  * @param  time: Pointer to RX8025T_Time structure
  * @retval HAL status
  */
HAL_StatusTypeDef RX8025T_SetTime(RX8025T_Time *time)
{
    uint8_t data[3];

    // Convert decimal to BCD
    data[0] = RX8025T_Dec2BCD(time->seconds);
    data[1] = RX8025T_Dec2BCD(time->minutes);
    data[2] = RX8025T_Dec2BCD(time->hours);

    // Write time registers
    return HAL_I2C_Mem_Write(&hi2c2, RX8025T_I2C_ADDR << 1, RX8025T_REG_SEC,
                            I2C_MEMADD_SIZE_8BIT, data, 3, HAL_MAX_DELAY);
}

/**
  * @brief  Set date to RX-8025T
  * @param  date: Pointer to RX8025T_Date structure
  * @retval HAL status
  */
HAL_StatusTypeDef RX8025T_SetDate(RX8025T_Date *date)
{
    uint8_t data[4];

    // Convert decimal to BCD
    data[0] = date->weekday;
    data[1] = RX8025T_Dec2BCD(date->day);
    data[2] = RX8025T_Dec2BCD(date->month);
    data[3] = RX8025T_Dec2BCD(date->year);

    // Write date registers
    return HAL_I2C_Mem_Write(&hi2c2, RX8025T_I2C_ADDR << 1, RX8025T_REG_WDAY,
                            I2C_MEMADD_SIZE_8BIT, data, 4, HAL_MAX_DELAY);
}

/**
  * @brief  Set complete date and time to RX-8025T
  * @param  datetime: Pointer to RX8025T_DateTime structure
  * @retval HAL status
  */
HAL_StatusTypeDef RX8025T_SetDateTime(RX8025T_DateTimeCompact *datetime)
{
    uint8_t data[7];

    // Convert decimal to BCD
    data[0] = RX8025T_Dec2BCD(datetime->seconds);
    data[1] = RX8025T_Dec2BCD(datetime->minutes);
    data[2] = RX8025T_Dec2BCD(datetime->hours);
    data[3] = datetime->weekday;
    data[4] = RX8025T_Dec2BCD(datetime->day);
    data[5] = RX8025T_Dec2BCD(datetime->month);
    data[6] = RX8025T_Dec2BCD(datetime->year);

    // Write all time registers
    return HAL_I2C_Mem_Write(&hi2c2, RX8025T_I2C_ADDR << 1, RX8025T_REG_SEC,
                            I2C_MEMADD_SIZE_8BIT, data, 7, HAL_MAX_DELAY);
}

/**
  * @brief  Get formatted date and time string
  * @param  buffer: Buffer to store the formatted string
  * @param  size: Size of the buffer
  * @retval HAL status
  */
HAL_StatusTypeDef RX8025T_GetDateTimeString(char *buffer, uint16_t size)
{
    RX8025T_DateTimeCompact dt;
    HAL_StatusTypeDef status;

    status = RX8025T_GetDateTime(&dt);
    if (status != HAL_OK) {
        return status;
    }

    // Format: 2026-03-12 09:18:30
    snprintf(buffer, size, "20%02d-%02d-%02d %02d:%02d:%02d",
             dt.year, dt.month, dt.day,
             dt.hours, dt.minutes, dt.seconds);

    return HAL_OK;
}

/**
  * @brief  Get formatted date string
  * @param  buffer: Buffer to store the formatted string
  * @param  size: Size of the buffer
  * @retval HAL status
  */
HAL_StatusTypeDef RX8025T_GetDateString(char *buffer, uint16_t size)
{
    RX8025T_Date dt;
    HAL_StatusTypeDef status;

    status = RX8025T_GetDate(&dt);
    if (status != HAL_OK) {
        return status;
    }

    // Format: 2026-03-12
    snprintf(buffer, size, "20%02d-%02d-%02d", dt.year, dt.month, dt.day);

    return HAL_OK;
}

/**
  * @brief  Get formatted time string
  * @param  buffer: Buffer to store the formatted string
  * @param  size: Size of the buffer
  * @retval HAL status
  */
HAL_StatusTypeDef RX8025T_GetTimeString(char *buffer, uint16_t size)
{
    RX8025T_Time tm;
    HAL_StatusTypeDef status;

    status = RX8025T_GetTime(&tm);
    if (status != HAL_OK) {
        return status;
    }

    // Format: 09:18:30
    snprintf(buffer, size, "%02d:%02d:%02d", tm.hours, tm.minutes, tm.seconds);

    return HAL_OK;
}

/**
  * @brief  Get weekday name string
  * @param  weekday: Weekday number (0-6)
  * @param  buffer: Buffer to store the weekday name
  * @param  size: Size of the buffer
  * @retval HAL status
  */
HAL_StatusTypeDef RX8025T_GetWeekdayName(uint8_t weekday, char *buffer, uint16_t size)
{
    const char *weekdays[] = {"Sunday", "Monday", "Tuesday", "Wednesday",
                              "Thursday", "Friday", "Saturday"};

    if (weekday > 6) {
        weekday = 0;
    }

    lv_snprintf(buffer, size, "%s", weekdays[weekday]);
    return HAL_OK;
}

/* Private functions */

/**
  * @brief  从 RX-8025T 读取单个寄存器
  * @param  reg: 寄存器地址
  * @param  data: 用于存储读取数据的指针
  * @retval HAL status
  */
static HAL_StatusTypeDef RX8025T_ReadRegister(uint8_t reg, uint8_t *data)
{
    return HAL_I2C_Mem_Read(&hi2c2, RX8025T_I2C_ADDR << 1, reg,
                           I2C_MEMADD_SIZE_8BIT, data, 1, HAL_MAX_DELAY);
}

/**
  * @brief  Write single register to RX-8025T
  * @param  reg: Register address
  * @param  data: Data to write
  * @retval HAL status
  */
static HAL_StatusTypeDef RX8025T_WriteRegister(uint8_t reg, uint8_t data)
{
    return HAL_I2C_Mem_Write(&hi2c2, RX8025T_I2C_ADDR << 1, reg,
                            I2C_MEMADD_SIZE_8BIT, &data, 1, HAL_MAX_DELAY);
}

/**
  * @brief  Convert BCD to decimal
  * @param  bcd: BCD value
  * @retval Decimal value
  */
static uint8_t RX8025T_BCD2Dec(uint8_t bcd)
{
    return (uint8_t)((bcd >> 4) * 10 + (bcd & 0x0F));
}

/**
  * @brief  Convert decimal to BCD
  * @param  dec: Decimal value
  * @retval BCD value
  */
static uint8_t RX8025T_Dec2BCD(uint8_t dec)
{
    return (uint8_t)(((dec / 10) << 4) + (dec % 10));
}
