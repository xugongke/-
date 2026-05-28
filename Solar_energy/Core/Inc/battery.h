/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    battery.h
  * @brief   电池电压采集与电量估算模块
  * @note    使用 ADC3 IN9 (PF3) 采集 VBAT 电压
  *          适用于两节 3.7V 锂电池并联 (额定电压 3.7V, 满充 4.2V)
  ******************************************************************************
  */
/* USER CODE END Header */

#ifndef __BATTERY_H__
#define __BATTERY_H__

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* USER CODE BEGIN Includes */
#include "lvgl.h"
/* USER CODE END Includes */

/* ========================== 硬件参数配置 ========================== */

/**
 * @brief ADC 参考电压 (STM32F429 VREF+ 通常为 3.3V)
 */
#define BATTERY_ADC_VREF            3.3f

/**
 * @brief ADC 分辨率位数 (12位 ADC, 0~4095)
 */
#define BATTERY_ADC_RESOLUTION      4096.0f

/**
 * @brief 分压电阻比: VBAT = ADC引脚电压 × 分压比
 * @note  如果使用两个等值电阻分压 (如 10kΩ + 10kΩ), 则分压比为 2.0
 *        如果没有分压电路, 设为 1.0
 *        如果使用 10kΩ 和 20kΩ 分压, 则 VBAT = Vadc × (10+20)/10 = 3.0
 *        请根据实际硬件电路修改此值
 */
#define BATTERY_VOLTAGE_DIVIDER     2.0f

/**
 * @brief ADC 多次采样取平均的采样次数
 */
#define BATTERY_ADC_SAMPLE_COUNT    16

/**
 * @brief 电池电量估算相关参数
 * @note  锂电池 (3.7V 额定) 的典型电压范围:
 *        满充: 4.20V
 *        额定: 3.70V
 *        放电截止: 3.00V
 */
#define BATTERY_VOLTAGE_MAX         4.20f   /* 满充电压 */
#define BATTERY_VOLTAGE_MIN         3.30f   /* 放电截止电压 */
#define BATTERY_VOLTAGE_FULL        4.1f   /* 100% 对应电压 */
#define BATTERY_VOLTAGE_EMPTY       3.30f   /* 0% 对应电压 */

/* ========================== 数据结构定义 ========================== */

/**
 * @brief 电池状态信息结构体
 */
typedef struct {
    float    voltage;       /* 实时电池电压 (V) */
    uint8_t  percentage;    /* 剩余电量百分比 (0~100%) */
    uint8_t  is_charging;   /* 充电状态: 0=未充电, 1=正在充电, 2=充电完成 */
    uint16_t adc_raw;       /* ADC 原始采样值 */
    float    adc_voltage;   /* ADC 引脚电压 (V) */
} Battery_Info_t;

/* ========================== 函数原型 ========================== */

/**
 * @brief  电池管理模块初始化
 * @note   内部调用 ADC 校准, 应在 MX_ADC3_Init() 之后调用
 */
void Battery_Init(void);

/**
 * @brief  获取电池信息 (电压、电量百分比、充电状态)
 * @param  info: 电池信息输出结构体指针
 */
void Battery_GetInfo(Battery_Info_t *info);

/**
 * @brief  仅获取电池电压
 * @retval 电池电压 (V), 如 3.85
 */
float Battery_GetVoltage(void);

/**
 * @brief  仅获取电池电量百分比
 * @retval 电量百分比 (0~100)
 */
uint8_t Battery_GetPercentage(void);

/**
 * @brief  获取充电状态
 * @retval 0=未充电, 1=正在充电, 2=充电完成(待机)
 */
uint8_t Battery_GetChargeStatus(void);

/* USER CODE BEGIN Prototypes */

/**
 * @brief  LVGL电池电量指示器初始化 (在custom_init中调用)
 * @param  parent: 父容器对象 (通常为 screen_user_home)
 */
void Battery_Widget_Init(lv_obj_t *parent);

/**
 * @brief  LVGL电池电量指示器周期更新 (由lv_timer回调)
 * @note   内部直接采集ADC并更新UI, 无需单独调用Battery_UpdateCache
 */
void Battery_Widget_Update(void);

/**
 * @brief  LVGL信号强度指示器初始化 (与电池指示器一起初始化)
 * @param  parent: 父容器对象 (通常为 screen_user_home)
 */
void Signal_Widget_Init(lv_obj_t *parent);

/**
 * @brief  LVGL信号强度指示器更新
 */
void Signal_Widget_Update(void);

/**
 * @brief  全局信号强度等级 (由freertos任务写入, LVGL定时器读取)
 *         -1=无信号/未知, 0=极弱, 1=弱, 2=中, 3=强, 4=满格
 */
extern volatile int8_t g_signal_level;

/**
 * @brief  将CSQ RSSI值映射到信号格数
 * @param  rssi: AT+CSQ返回的RSSI值 (0~31, 99=未知)
 * @retval -1=未知/无信号, 0=极弱, 1=弱, 2=中, 3=强, 4=满格
 */
int8_t Signal_GetLevel(int32_t rssi);

/* ========================== 太阳能直流总线电压 ========================== */

/**
 * @brief  获取太阳能直流总线电压 (V)
 * @note   内部直接采集ADC1_IN9 (PB1), 分压比48倍
 * @retval 电压值, 如 48.5
 */
float Solar_GetVoltage(void);

/**
 * @brief  更新home页太阳能卡片的电压显示 (由LVGL定时器调用)
 * @note   内部直接采集ADC并更新UI, 无需单独调用Solar_UpdateCache
 */
void Solar_Widget_Update(void);

/* USER CODE END Prototypes */

#ifdef __cplusplus
}
#endif

#endif /* __BATTERY_H__ */
