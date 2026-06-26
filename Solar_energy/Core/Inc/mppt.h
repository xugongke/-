/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    mppt.h
  * @brief   太阳能最大功率点跟踪 (MPPT) 模块 - 扰动观察法 (P&O)
  * @note    基于离散负载调节的 MPPT 实现
  *
  *          原理:
  *            功率 P = V? × N / R_heater (N=开启的加热器数量)
  *            通过增减 N 来寻找最大功率点
  *
  *          硬件前提:
  *            - 主机通过 ADC1_IN9 (PB1) 采集直流总线电压
  *            - 主机通过 ES1642 载波模块控制从机加热管开关
  *            - 加热管为纯电阻负载 (8Ω)
  *
  *          状态机:
  *            INIT → MEASURE → PERTURB → WAIT → OBSERVE → DECIDE → PERTURB
  *                                                        ↓
  *                    (光照不足时) ←──────────────── DISABLED
  ******************************************************************************
  */
/* USER CODE END Header */

#ifndef __MPPT_H__
#define __MPPT_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "stdint.h"
#include "user_data_manager.h"  /* for user_data_timestamp_t */

/* ========================== 硬件参数配置 ========================== */

/**
 * @brief 单台热水器加热管电阻 (Ω)
 * @note  硬件工程师确认: 直流加热管视为恒定 8Ω 电阻
 */
#define MPPT_HEATER_RESISTANCE      14.06f  /* 150V/1600W = 14.06Ω */

/**
 * @brief 直流总线最低工作电压 (V)
 * @note  低于此电压认为无有效光照, MPPT 进入 DISABLED 状态
 *        设备工作电压范围 30~150V, 取 25V 作为阈值(留余量)
 */
#define MPPT_VOLTAGE_MIN            25.0f

/**
 * @brief 直流总线最高安全电压 (V)
 * @note  超过此电压应减少负载以防过压
 */
#define MPPT_VOLTAGE_MAX            155.0f

/**
 * @brief P&O 扰动后系统稳定等待时间 (秒)
 * @note  切换加热管后总线电压需要一定时间稳定
 *        取决于太阳能板电容效应和线路电感
 *        建议 8~15 秒
 */
#define MPPT_SETTLE_TIME_SEC        10

/**
 * @brief MPPT 初始扰动方向
 * @note  0=初始状态(首次向增加方向), 1=增加, 2=减少
 */
#define MPPT_DIR_INIT               0
#define MPPT_DIR_INCREASE           1
#define MPPT_DIR_DECREASE           2

/**
 * @brief 功率变化判定阈值 (W)
 * @note  功率变化小于此值视为"不变", 避免因测量噪声导致频繁切换
 */
#define MPPT_POWER_THRESHOLD        5.0f

/**
 * @brief 连续无效扰动计数上限
 * @note  连续在此阈值内功率无明显变化, 认为已到达 MPP 附近
 *        进入慢速模式, 延长扰动间隔
 */
#define MPPT_STABLE_COUNT_MAX       3

/**
 * @brief MPPT 任务周期 (毫秒)
 * @note  状态机每次循环的间隔, 非扰动周期
 *        扰动周期 = MPPT_SETTLE_TIME_SEC + 控制命令耗时
 */
#define MPPT_TASK_PERIOD_MS         1000

/* ========================== 太阳能发电量数据结构 ========================== */

#define SOLAR_ENERGY_FILE    "0:/solar_energy.bin"
#define SOLAR_ENERGY_MAGIC   0x534F4C41  /* "SOLA" */
#define SOLAR_ENERGY_VERSION 1
#define SOLAR_HISTORY_DAYS   15

#pragma pack(push, 1)
typedef struct {
    uint32_t magic;                              /* 魔数校验 */
    uint8_t  version;                            /* 版本号 */
    float    daily_generation_kwh;               /* 日发电量(kWh) - 当日0点重置 */
    float    monthly_generation_kwh;             /* 月发电量(kWh) - 每月1号重置 */
    float    annual_generation_kwh;              /* 年发电量(kWh) - 每年1月1日重置 */
    float    total_generation_kwh;               /* 总发电量(kWh) - 永不重置 */
    float    history_daily[SOLAR_HISTORY_DAYS];  /* 近15日发电量(kWh), [0]=15天前...[14]=最新(当天) */
    uint8_t  last_reset_day;                     /* 上次日发电量重置时的日期(1-31) */
    uint8_t  last_reset_mon;                     /* 上次月发电量重置时的月份(1-12) */
    user_data_timestamp_t update_time;           /* 数据最后更新时间 */
    uint8_t  reserved[2];                        /* 预留对齐 */
} solar_energy_data_t;
#pragma pack(pop)

extern solar_energy_data_t g_solar_energy;  /* 太阳能发电量数据(全局RAM缓存) */

/**
 * @brief  从SD卡加载太阳能发电量数据到RAM；若文件不存在则创建默认文件
 * @note   在文件系统初始化完成后调用 (lvgl_task中)
 */
void solar_energy_init(void);

/**
 * @brief  将RAM中的太阳能发电量数据写入SD卡（清空写）
 */
void solar_energy_save(void);

/**
 * @brief  处理太阳能发电量的日/月/年重置逻辑，将g_mppt.energy_wh累加到g_solar_energy
 * @note   在daily_energy_flush_to_sd中调用（零点结算时）
 */
void solar_energy_flush(void);

/* ========================== 数据结构定义 ========================== */

/**
 * @brief MPPT 状态机枚举
 */
typedef enum {
    MPPT_STATE_INIT = 0,    /* 初始化: 确定当前在线设备数和电压 */
    MPPT_STATE_DISABLED,    /* 禁用: 光照不足或无设备 */
    MPPT_STATE_MEASURE,     /* 测量: 读取当前电压, 计算功率 */
    MPPT_STATE_PERTURB,     /* 扰动: 发送加热管开关命令 */
    MPPT_STATE_WAIT,        /* 等待: 等待系统稳定 */
    MPPT_STATE_OBSERVE,     /* 观察: 读取新电压, 计算新功率 */
    MPPT_STATE_DECIDE,      /* 决策: 比较功率, 决定下一步方向 */
} mppt_state_t;

/**
 * @brief MPPT 运行数据结构体 (用于调试和UI显示)
 */
typedef struct {
    /* 当前状态 */
    mppt_state_t state;         /* 状态机当前状态 */
    uint8_t  n_active;          /* 当前开启的加热器数量 */
    uint8_t  n_online;          /* 在线设备总数 */
    uint8_t  direction;         /* 当前扰动方向: 0=init, 1=增加, 2=减少 */
    uint8_t  stable_count;      /* 连续稳定计数 (功率变化<阈值) */

    /* 功率数据 */
    float voltage;              /* 最近一次总线电压 (V) */
    float power;                /* 当前估算功率 (W) */
    float power_prev;           /* 扰动前功率 (W) */
    float power_max;            /* 观测到的最大功率 (W) */
    uint8_t n_at_max_power;     /* 最大功率对应的加热器数 */

    /* 累计发电量 */
    uint32_t energy_wh;            /* 日发电量累计 (Wh) */

    /* 统计 */
    uint32_t cycle_count;       /* P&O 循环计数 */
    uint8_t  enabled;           /* MPPT 使能标志: 0=禁用, 1=启用 */
} mppt_data_t;

/* ========================== 全局变量 ========================== */

extern mppt_data_t g_mppt;

/* ========================== 函数原型 ========================== */

/**
 * @brief  使能/禁用 MPPT 功能
 * @param  enable: 1=使能, 0=禁用
 */
void MPPT_Enable(uint8_t enable);

/**
 * @brief  计算估算功率
 * @param  voltage: 总线电压 (V)
 * @param  n_heaters: 开启的加热器数量
 * @retval 估算功率 (W)
 */
float MPPT_CalcPower(float voltage, uint8_t n_heaters);

/**
 * @brief  获取日发电量
 * @retval 日发电量 (kWh)
 */
float MPPT_GetDailyEnergy_kWh(void);

/**
 * @brief  获取当前估算功率
 * @retval 当前功率 (W)
 */
float MPPT_GetPower(void);

/**
 * @brief  强制重新搜索 MPP (光照剧变时调用)
 * @note   重置状态机到 INIT, 从1台开始重新搜索
 */
void MPPT_ForceRescan(void);

/* ========================== MPPT 主动控制接口 ========================== */

extern uint32_t heating_seconds[MAX_DEVICES];    /* 每台设备累计加热秒数(公平性排序用) */

/**
 * @brief  MPPT P&O 控制闭环 (在采集阶段之后调用)
 * @note   快速控制循环: ±1台→发0x02/0.03等ACK→等3秒→读电压→P&O判断
 *         功率增加继续搜索, 功率下降撤回最后一步并退出
 *         选管基于采集阶段的温度数据(温度排序+加热时长公平)
 */
void MPPT_ControlLoop(void);

#ifdef __cplusplus
}
#endif

#endif /* __MPPT_H__ */
