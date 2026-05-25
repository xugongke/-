/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    battery.c
  * @brief   电池电压采集与电量估算模块实现
  * @note    使用 ADC3 IN9 (PF3) 采集 VBAT 电压
  *          适用于两节 3.7V 锂电池并联 (额定电压 3.7V, 满充 4.2V)
  *
  *          硬件连接:
  *            - VBAT --> 分压电阻 --> PF3 (ADC3_IN9)
  *            - CHRG_Pin (PH3): 充电指示, 低电平有效(正在充电)
  *            - STDBY_Pin (PH4): 充电完成指示, 低电平有效(充电完成)
  *
  *          电量估算采用锂电池开路电压(OCV)与放电深度(DOD)的对应关系,
  *          使用分段线性插值查表法, 比简单的线性映射更准确。
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "battery.h"
#include "adc.h"
#include "main.h"

/* USER CODE BEGIN Includes */
#include "gui_guider.h"
/* USER CODE END Includes */

/* ========================== 私有变量 ========================== */

extern ADC_HandleTypeDef hadc3;

/* ========================== 锂电池 OCV-SOC 查找表 ========================== */

/**
 * @brief 锂电池 (3.7V额定) 开路电压 vs SOC 映射表
 * @note  数据来源于典型 18650 锂电池放电曲线 (0.2C 放电率)
 *        两节并联不影响电压, 仅增加容量, 因此电压-SOC关系不变
 *
 *        表格按电压从低到高排列, 用于线性插值计算
 */
typedef struct {
    float voltage;   /* 开路电压 (V) */
    uint8_t soc;     /* 对应 SOC (%) */
} VoltageSOC_t;

/* 典型锂电池放电曲线查找表 (电压从低到高) */
static const VoltageSOC_t soc_table[] = {
    { 3.00f,  0  },    /* 放电截止 */
    { 3.10f,  5  },
    { 3.20f,  10 },
    { 3.30f,  15 },
    { 3.40f,  20 },
    { 3.50f,  25 },
    { 3.55f,  30 },
    { 3.60f,  35 },
    { 3.65f,  40 },
    { 3.68f,  45 },
    { 3.70f,  50 },    /* 额定电压附近 */
    { 3.72f,  55 },
    { 3.75f,  60 },
    { 3.78f,  65 },
    { 3.82f,  70 },
    { 3.87f,  75 },
    { 3.92f,  80 },
    { 3.98f,  85 },
    { 4.03f,  90 },
    { 4.08f,  95 },
    { 4.15f,  100},    /* 满充 */
};

/** 查找表项数 */
#define SOC_TABLE_SIZE  (sizeof(soc_table) / sizeof(soc_table[0]))

/* ========================== 私有函数 ========================== */

/**
 * @brief  ADC 多次采样取平均值
 * @retval 平均后的 ADC 原始值 (12位, 0~4095)
 */
static uint16_t Battery_ReadADC_Average(void)
{
    uint32_t sum = 0;
    uint16_t valid_count = 0;

    for (uint16_t i = 0; i < BATTERY_ADC_SAMPLE_COUNT; i++)
    {
        /* 启动 ADC 转换 */
        if (HAL_ADC_Start(&hadc3) != HAL_OK)
        {
            continue;
        }

        /* 等待转换完成, 超时 100ms */
        if (HAL_ADC_PollForConversion(&hadc3, 100) == HAL_OK)
        {
            sum += HAL_ADC_GetValue(&hadc3);
            valid_count++;
        }

        /* 停止 ADC */
        HAL_ADC_Stop(&hadc3);
    }

    if (valid_count == 0)
    {
        return 0;
    }

    return (uint16_t)(sum / valid_count);
}

/**
 * @brief  将 ADC 原始值转换为实际电池电压
 * @param  adc_raw: ADC 采样原始值
 * @retval 电池实际电压 (V)
 */
static float Battery_ADC_ToVoltage(uint16_t adc_raw)
{
    /* 先计算 ADC 引脚电压: Vadc = (adc_raw / 4096) × VREF */
    float adc_voltage = ((float)adc_raw / BATTERY_ADC_RESOLUTION) * BATTERY_ADC_VREF;

    /* 再根据分压比还原实际电池电压: VBAT = Vadc × 分压比 */
    float battery_voltage = adc_voltage * BATTERY_VOLTAGE_DIVIDER;

    return battery_voltage;
}

/**
 * @brief  通过查找表 + 线性插值计算 SOC
 * @param  voltage: 当前电池电压 (V)
 * @retval 电量百分比 (0~100)
 */
static uint8_t Battery_CalcSOC(float voltage)
{
    /* 电压低于最低值, 返回 0% */
    if (voltage <= soc_table[0].voltage)
    {
        return 0;
    }

    /* 电压高于最高值, 返回 100% */
    if (voltage >= soc_table[SOC_TABLE_SIZE - 1].voltage)
    {
        return 100;
    }

    /* 在查找表中进行线性插值 */
    for (uint16_t i = 0; i < SOC_TABLE_SIZE - 1; i++)
    {
        if (voltage >= soc_table[i].voltage && voltage < soc_table[i + 1].voltage)
        {
            /* 线性插值: soc = soc_low + (v - v_low) / (v_high - v_low) × (soc_high - soc_low) */
            float ratio = (voltage - soc_table[i].voltage) /
                          (soc_table[i + 1].voltage - soc_table[i].voltage);

            float soc = soc_table[i].soc + ratio * (soc_table[i + 1].soc - soc_table[i].soc);

            /* 限制范围并四舍五入 */
            if (soc < 0.0f)   soc = 0.0f;
            if (soc > 100.0f) soc = 100.0f;

            return (uint8_t)(soc + 0.5f);
        }
    }

    return 0;
}

/**
 * @brief  读取充电状态引脚
 * @retval 0=未充电, 1=正在充电, 2=充电完成(待机)
 * @note   典型充电 IC (如 TP4056):
 *         CHRG 引脚: 低电平 = 正在充电
 *         STDBY 引脚: 低电平 = 充电完成
 *         两者都为高电平 = 未充电 (或无电池)
 */
static uint8_t Battery_ReadChargeStatus(void)
{
    GPIO_PinState chrg_pin  = HAL_GPIO_ReadPin(CHRG_GPIO_Port, CHRG_Pin);
    GPIO_PinState stdby_pin = HAL_GPIO_ReadPin(STDBY_GPIO_Port, STDBY_Pin);
	
    /* CHRG 低电平: 正在充电 */
    if (chrg_pin == GPIO_PIN_RESET && stdby_pin == GPIO_PIN_SET)
    {
        return 1;
    }

    /* STDBY 低电平: 充电完成 */
    if (stdby_pin == GPIO_PIN_RESET && chrg_pin == GPIO_PIN_SET)
    {
        return 2;
    }

    /* 都为低电平: 未充电 */
    return 0;
}

/* ========================== 公共函数 ========================== */

/**
 * @brief  电池管理模块初始化
 * @note   应在 MX_ADC3_Init() 之后调用
 */
void Battery_Init(void)
{
    /* STM32F4xx ADC 校准在出厂时已完成, 无需运行时校准 */
    /* 通过多次采样取平均来保证精度 */
}

/**
 * @brief  获取电池完整信息
 * @param  info: 电池信息输出结构体指针
 */
void Battery_GetInfo(Battery_Info_t *info)
{
    if (info == NULL)
    {
        return;
    }

    /* 读取 ADC 平均值 */
    info->adc_raw = Battery_ReadADC_Average();

    /* 计算 ADC 引脚电压 */
    info->adc_voltage = ((float)info->adc_raw / BATTERY_ADC_RESOLUTION) * BATTERY_ADC_VREF;

    /* 计算实际电池电压 */
    info->voltage = Battery_ADC_ToVoltage(info->adc_raw);

    /* 计算电量百分比 */
    info->percentage = Battery_CalcSOC(info->voltage);

    /* 读取充电状态 */
    info->is_charging = Battery_ReadChargeStatus();
}

/**
 * @brief  仅获取电池电压
 * @retval 电池电压 (V)
 */
float Battery_GetVoltage(void)
{
    uint16_t adc_raw = Battery_ReadADC_Average();
    return Battery_ADC_ToVoltage(adc_raw);
}

/**
 * @brief  仅获取电池电量百分比
 * @retval 电量百分比 (0~100)
 */
uint8_t Battery_GetPercentage(void)
{
    float voltage = Battery_GetVoltage();
    return Battery_CalcSOC(voltage);
}

/**
 * @brief  获取充电状态
 * @retval 0=未充电, 1=正在充电, 2=充电完成(待机)
 */
uint8_t Battery_GetChargeStatus(void)
{
    return Battery_ReadChargeStatus();
}

/* USER CODE BEGIN 1 */

/* ========================== LVGL 状态栏组件 ========================== */

/* 全局信号强度等级 (freertos任务写, LVGL定时器读) */
volatile int8_t g_signal_level = -1;  /* 默认无信号 */

/* 电池组件对象句柄 */
static struct {
    lv_obj_t *cont;        /* 电池外框容器 */
    lv_obj_t *fill;        /* 电池填充条 */
    lv_obj_t *cap;         /* 电池正极帽 */
    lv_obj_t *label_pct;   /* 百分比文字 */
    lv_obj_t *label_chg;   /* 充电图标 */
} battery_widget;

/* 信号组件对象句柄 */
static struct {
    lv_obj_t *cont;        /* 信号容器 */
    lv_obj_t *bars[4];     /* 4根信号条 (从矮到高) */
    lv_obj_t *label_x;     /* 无信号X标记 */
} signal_widget;

/* LVGL周期更新定时器 (电池+信号共用) */
static lv_timer_t *status_bar_timer = NULL;

/* 电池数据缓存 (由非LVGL任务通过 Battery_UpdateCache() 写入,
 * LVGL定时器通过 Battery_Widget_Update() 只读缓存) */
static volatile uint8_t  cached_battery_pct = 100;
static volatile uint8_t  cached_charge_status = 0;

/**
 * @brief  将CSQ RSSI值映射到信号格数
 */
int8_t Signal_GetLevel(int32_t rssi)
{
    if (rssi >= 28 && rssi <= 31) return 4;   /* 满格信号 */
    if (rssi >= 24 && rssi <= 27) return 3;   /* 强信号 */
    if (rssi >= 17 && rssi <= 23) return 2;   /* 中等信号 */
    if (rssi >= 10 && rssi <= 16) return 1;   /* 弱信号 */
    if (rssi == 99)              return -1;   /* 未知/无信号 */
    return 0;  /* rssi 0~9: 极弱信号 */
}

/**
 * @brief  更新电池数据缓存 (由非LVGL任务周期调用, 如StartDefaultTask)
 * @note   在非LVGL任务中执行阻塞的ADC采集, 将结果存入缓存,
 *         LVGL定时器回调只读缓存, 不做阻塞操作, 避免与其他任务的
 *         LVGL API调用产生竞态条件导致TLSF堆损坏。
 */
void Battery_UpdateCache(void)
{
    uint16_t adc_raw = Battery_ReadADC_Average();
    float voltage = Battery_ADC_ToVoltage(adc_raw);
    cached_battery_pct = Battery_CalcSOC(voltage);
    cached_charge_status = Battery_ReadChargeStatus();
}

/**
 * @brief  LVGL定时器回调 - 周期更新电池+信号UI显示
 * @note   只读取缓存数据更新UI, 不做阻塞ADC采集
 */
static void status_bar_timer_cb(lv_timer_t *timer)
{
    (void)timer;

    /* 如果 home 页面不是当前活跃屏幕, 跳过更新 */
    if (guider_ui.screen_user_home == NULL ||
        lv_scr_act() != guider_ui.screen_user_home)
    {
        return;
    }

    Battery_Widget_Update();
    Signal_Widget_Update();
}

/**
 * @brief  创建电池指示器UI组件
 * @param  parent: 父对象 (screen_user_home)
 *
 *  布局示意 (左上角):
 *  ┌─────────────────────┐
 *  │ 🔋 75%  ┃          │
 *  └─────────────────────┘
 *
 *  电池图标 + 百分比文字, 位于屏幕左上角 (信号格的对称位置)
 *  充电时显示闪电图标 ⚡
 */
void Battery_Widget_Init(lv_obj_t *parent)
{
    if (parent == NULL) return;

    /* ---- 1. 整体容器 (仅包含电池图标) ---- */
    battery_widget.cont = lv_obj_create(parent);
    lv_obj_remove_style_all(battery_widget.cont);
    lv_obj_set_size(battery_widget.cont, 68, 28);
    lv_obj_set_pos(battery_widget.cont, 3, 6);
    lv_obj_clear_flag(battery_widget.cont, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(battery_widget.cont, 0, 0);

    /* ---- 2. 电池外框 (加大以容纳百分比文字) ---- */
    lv_obj_t *bat_body = lv_obj_create(battery_widget.cont);
    lv_obj_remove_style_all(bat_body);
    lv_obj_set_size(bat_body, 46, 20);
    lv_obj_set_pos(bat_body, 2, 4);
    lv_obj_set_style_radius(bat_body, 3, 0);
    lv_obj_set_style_border_width(bat_body, 2, 0);
    lv_obj_set_style_border_color(bat_body, lv_color_hex(0x333333), 0);
    lv_obj_set_style_border_opa(bat_body, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(bat_body, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_bg_opa(bat_body, LV_OPA_TRANSP, 0);
    lv_obj_clear_flag(bat_body, LV_OBJ_FLAG_SCROLLABLE);

    /* ---- 3. 电池正极帽 (右侧小凸起) ---- */
    battery_widget.cap = lv_obj_create(battery_widget.cont);
    lv_obj_remove_style_all(battery_widget.cap);
    lv_obj_set_size(battery_widget.cap, 3, 10);
    lv_obj_set_pos(battery_widget.cap, 48, 9);
    lv_obj_set_style_radius(battery_widget.cap, 1, 0);
    lv_obj_set_style_bg_color(battery_widget.cap, lv_color_hex(0x333333), 0);
    lv_obj_set_style_bg_opa(battery_widget.cap, LV_OPA_COVER, 0);
    lv_obj_clear_flag(battery_widget.cap, LV_OBJ_FLAG_SCROLLABLE);

    /* ---- 4. 电池填充条 (显示电量百分比) ---- */
    battery_widget.fill = lv_obj_create(bat_body);
    lv_obj_remove_style_all(battery_widget.fill);
    lv_obj_set_size(battery_widget.fill, 38, 12);
    lv_obj_align(battery_widget.fill, LV_ALIGN_LEFT_MID, 2, 0);
    lv_obj_set_style_radius(battery_widget.fill, 1, 0);
    lv_obj_set_style_bg_color(battery_widget.fill, lv_color_hex(0x4CAF50), 0);
    lv_obj_set_style_bg_opa(battery_widget.fill, LV_OPA_COVER, 0);
    lv_obj_clear_flag(battery_widget.fill, LV_OBJ_FLAG_SCROLLABLE);

    /* ---- 5. 百分比文字 (在电池内部居中显示) ---- */
    battery_widget.label_pct = lv_label_create(bat_body);
    lv_label_set_text(battery_widget.label_pct, "100%");
    lv_obj_set_style_text_color(battery_widget.label_pct, lv_color_hex(0x333333), 0);
    lv_obj_set_style_text_font(battery_widget.label_pct, &lv_font_montserratMedium_12, 0);
    lv_obj_align(battery_widget.label_pct, LV_ALIGN_CENTER, 0, 0);

    /* ---- 6. 充电图标 (默认隐藏, 在电池右侧) ---- */
    battery_widget.label_chg = lv_label_create(battery_widget.cont);
    lv_label_set_text(battery_widget.label_chg, LV_SYMBOL_CHARGE);
    lv_obj_set_style_text_color(battery_widget.label_chg, lv_color_hex(0xFF9800), 0);
    lv_obj_set_style_text_font(battery_widget.label_chg, &lv_font_montserratMedium_12, 0);
    lv_obj_set_style_bg_opa(battery_widget.label_chg, 0, 0);
    lv_obj_set_pos(battery_widget.label_chg, 54, 8);
    lv_obj_add_flag(battery_widget.label_chg, LV_OBJ_FLAG_HIDDEN);

    /* ---- 7. 创建LVGL定时器, 每10秒更新电池+信号状态 ---- */
    /* 先删除旧定时器, 防止每次重建 home 页面时定时器累积 */
    if (status_bar_timer != NULL)
    {
        lv_timer_del(status_bar_timer);
        status_bar_timer = NULL;
    }
    status_bar_timer = lv_timer_create(status_bar_timer_cb, 10000, NULL);
//    lv_timer_ready(status_bar_timer);  /* 立即触发第一次更新 */
		Battery_Widget_Update();
}

/**
 * @brief  更新电池指示器显示
 */
void Battery_Widget_Update(void)
{
    /* 安全检查: 确保所有 widget 对象仍然有效 */
    if (battery_widget.fill == NULL ||
        battery_widget.cont == NULL ||
        battery_widget.label_pct == NULL ||
        battery_widget.label_chg == NULL)
    {
        return;
    }
    if (!lv_obj_is_valid(battery_widget.cont) ||
        !lv_obj_is_valid(battery_widget.fill) ||
        !lv_obj_is_valid(battery_widget.label_pct) ||
        !lv_obj_is_valid(battery_widget.label_chg))
    {
        return;
    }

    /* 从缓存读取电池数据 (缓存由 Battery_UpdateCache() 在非LVGL任务中更新) */
    uint8_t pct = cached_battery_pct;
    uint8_t charge_status = cached_charge_status;

    /* ---- 更新填充条宽度 (电池内部可用宽度约20px) ---- */
    if (pct > 100) pct = 100;
    lv_coord_t fill_w = (lv_coord_t)((float)pct * 38 / 100);
    if (fill_w < 2 && pct > 0) fill_w = 2;  /* 至少显示一点 */
    lv_obj_set_width(battery_widget.fill, fill_w);

    /* ---- 根据电量百分比设置颜色 ---- */
    lv_color_t fill_color;
    if (pct > 50)
    {
        fill_color = lv_color_hex(0x4CAF50);  /* 绿色 */
    }
    else if (pct > 20)
    {
        fill_color = lv_color_hex(0xFF9800);  /* 橙色 */
    }
    else
    {
        fill_color = lv_color_hex(0xF44336);  /* 红色 */
    }
    lv_obj_set_style_bg_color(battery_widget.fill, fill_color, 0);

    /* ---- 更新百分比文字 ---- */
    char buf[8];
    snprintf(buf, sizeof(buf), "%d%%", pct);
    lv_label_set_text(battery_widget.label_pct, buf);

    /* ---- 更新充电图标 ---- */
    if (charge_status == 1)
    {
        /* 正在充电 - 显示闪电图标 */
        lv_obj_clear_flag(battery_widget.label_chg, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_style_text_color(battery_widget.label_chg, lv_color_hex(0xFF9800), 0);
    }
    else if (charge_status == 2)
    {
        /* 充电完成 - 显示满电标识 */
        lv_obj_clear_flag(battery_widget.label_chg, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_style_text_color(battery_widget.label_chg, lv_color_hex(0x4CAF50), 0);
    }
    else
    {
        /* 未充电 - 隐藏图标 */
        lv_obj_add_flag(battery_widget.label_chg, LV_OBJ_FLAG_HIDDEN);
    }
}

/**
 * @brief  创建手机风格4格信号强度指示器
 * @param  parent: 父对象 (screen_user_home)
 *
 *  布局示意 (右上角):
 *
 *  ┌───────────────────────┐
 *  │  ▐  ▐▌  ▐▌▌  ▐▌▌▌   │   ← 4格信号条, 底部对齐, 逐级升高
 *  └───────────────────────┘
 *
 *  信号等级: -1=无信号(显示X), 0~4=点亮的格数
 *  颜色: 绿色(0x00cc66)激活, 灰色(0xbbbbbb)未激活
 */
void Signal_Widget_Init(lv_obj_t *parent)
{
    if (parent == NULL) return;

    /* 容器 */
    signal_widget.cont = lv_obj_create(parent);
    lv_obj_remove_style_all(signal_widget.cont);
    lv_obj_set_size(signal_widget.cont, 40, 26);
    lv_obj_set_pos(signal_widget.cont, 436, 9);
    lv_obj_clear_flag(signal_widget.cont, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(signal_widget.cont, 0, 0);

    /* 4根信号条: 从左到右逐渐增高, 底部对齐 */
    /* 容器高26px, 4根条高度: 5, 10, 15, 20, 宽度: 5px, 间距: 2px */
    static const lv_coord_t bar_h[4] = {5, 10, 15, 20};
    static const lv_coord_t bar_w = 5;
    static const lv_coord_t gap = 2;
    lv_coord_t total_w = 4 * bar_w + 3 * gap;  /* = 26 */
    lv_coord_t start_x = (40 - total_w) / 2;    /* 居中 = 7 */

    for (uint8_t i = 0; i < 4; i++)
    {
        signal_widget.bars[i] = lv_obj_create(signal_widget.cont);
        lv_obj_remove_style_all(signal_widget.bars[i]);
        lv_obj_set_size(signal_widget.bars[i], bar_w, bar_h[i]);
        lv_coord_t x = start_x + i * (bar_w + gap);
        lv_coord_t y = 24 - bar_h[i];  /* 底部对齐 (容器高26, 留2px padding) */
        lv_obj_set_pos(signal_widget.bars[i], x, y);
        lv_obj_set_style_radius(signal_widget.bars[i], 1, 0);
        lv_obj_set_style_bg_color(signal_widget.bars[i], lv_color_hex(0xbbbbbb), 0);
        lv_obj_set_style_bg_opa(signal_widget.bars[i], LV_OPA_COVER, 0);
        lv_obj_clear_flag(signal_widget.bars[i], LV_OBJ_FLAG_SCROLLABLE);
    }

    /* 无信号X标记 (默认隐藏) */
    signal_widget.label_x = lv_label_create(signal_widget.cont);
    lv_label_set_text(signal_widget.label_x, LV_SYMBOL_CLOSE);
    lv_obj_set_style_text_color(signal_widget.label_x, lv_color_hex(0xF44336), 0);
    lv_obj_set_style_text_font(signal_widget.label_x, &lv_font_montserratMedium_12, 0);
    lv_obj_align(signal_widget.label_x, LV_ALIGN_CENTER, 0, 0);
    lv_obj_add_flag(signal_widget.label_x, LV_OBJ_FLAG_HIDDEN);
}

/**
 * @brief  更新信号强度显示 (由LVGL定时器调用)
 */
void Signal_Widget_Update(void)
{
    if (signal_widget.cont == NULL || !lv_obj_is_valid(signal_widget.cont)) return;

    int8_t level = g_signal_level;
    const lv_color_t color_on  = lv_color_hex(0x00cc66);   /* 激活: 绿色 */
    const lv_color_t color_off = lv_color_hex(0xbbbbbb);   /* 未激活: 灰色 */

    if (level < 0)
    {
        /* 无信号: 全灰 + 显示X */
        for (uint8_t i = 0; i < 4; i++)
        {
            if (lv_obj_is_valid(signal_widget.bars[i]))
                lv_obj_set_style_bg_color(signal_widget.bars[i], color_off, 0);
        }
        lv_obj_clear_flag(signal_widget.label_x, LV_OBJ_FLAG_HIDDEN);
    }
    else
    {
        /* 有信号: 根据level点亮对应格数 */
        lv_obj_add_flag(signal_widget.label_x, LV_OBJ_FLAG_HIDDEN);
        for (uint8_t i = 0; i < 4; i++)
        {
            if (!lv_obj_is_valid(signal_widget.bars[i])) continue;
            lv_color_t c = (i < level) ? color_on : color_off;
            lv_obj_set_style_bg_color(signal_widget.bars[i], c, 0);
        }
    }
}

/* USER CODE END 1 */
