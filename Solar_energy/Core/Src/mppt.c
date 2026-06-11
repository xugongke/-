/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    mppt.c
  * @brief   太阳能最大功率点跟踪 (MPPT) - 扰动观察法 (P&O) 实现
  * @note    基于离散负载调节, 通过增减加热管数量寻找最大功率点
  *
  *          核心公式:
  *            P = V_bus? × N_active / R_heater
  *            其中 R_heater = 8Ω (单台热水器直流加热管电阻)
  *
  *          P&O 算法流程:
  *            1. 测量当前电压 → 计算功率 P_old
  *            2. 扰动: 增加/减少 1 台加热器
  *            3. 等待系统稳定
  *            4. 再次测量 → 计算功率 P_new
  *            5. 比较 P_new 和 P_old, 决定下一步方向
  *
  *          与传统 MPPT 的区别:
  *            - 传统: 连续调节 DC-DC 占空比
  *            - 本方案: 离散调节负载数量 (N = 1, 2, 3, ...)
  *            - 优点: 无需电流传感器, 零硬件成本
  *            - 缺点: 粒度粗 (最小步长=1台), 追踪速度慢
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "mppt.h"
#include "main.h"             /* stm32f4xx_hal.h → IRQn_Type 定义 */
#include "battery.h"          /* Solar_GetVoltage() */
#include "device_manager.h"   /* device_list, device_count, device_ctrl_heater */
#include "cmsis_os.h"         /* osDelay, xTaskGetTickCount */
#include "stdio.h"

/* ========================== 全局实例 ========================== */

mppt_data_t g_mppt = {0};

/* ========================== 私有变量 ========================== */

/** 等待状态计数器 (秒) */
static uint8_t wait_counter = 0;

/** 发电量累计的时间戳 (上次累计的时刻, 单位: 秒) */
static uint32_t energy_last_tick = 0;

/** 发电量累计初始化标志 (防止 energy_last_tick==0 时跳过第一次累计) */
static uint8_t energy_initialized = 0;

/** 发电量累计间隔 (秒) */
#define ENERGY_ACCUM_INTERVAL_SEC   5

/* ========================== 私有函数 ========================== */

/**
 * @brief  统计当前实际开启的加热器数量
 * @retval 正在加热的设备数
 */
static uint8_t count_active_heaters(void)
{
    uint8_t count = 0;
    for (uint16_t i = 0; i < device_count; i++)
    {
        if (device_list[i].state.bits.valid &&
            device_list[i].state.bits.dc_heating)
        {
            count++;
        }
    }
    return count;
}

/**
 * @brief  统计在线设备数量 (已入网 + 无通信故障)
 * @retval 在线设备数
 */
static uint8_t count_online_devices(void)
{
    uint8_t count = 0;
    for (uint16_t i = 0; i < device_count; i++)
    {
        if (device_list[i].state.bits.valid &&
            !device_list[i].state.bits.comm_err)
        {
            count++;
        }
    }
    return count;
}

/**
 * @brief  累计发电量
 * @note   在 MEASURE 和 OBSERVE 状态时调用
 *         使用梯形积分: E += P × Δt
 */
static void accumulate_energy(void)
{
    /* 累计发电量 (每隔 ENERGY_ACCUM_INTERVAL_SEC 秒累计一次) */
    uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS / 1000; /* 当前秒数 */
    if (!energy_initialized)
    {
        energy_last_tick = now;
        energy_initialized = 1;
        return;
    }

    if (now - energy_last_tick >= ENERGY_ACCUM_INTERVAL_SEC)
    {
        float power = g_mppt.power;
        if (power > 0.0f)
        {
            /* power 单位 W, 间隔单位 秒, 转换为 Wh */
            g_mppt.energy_wh += power * (float)(now - energy_last_tick) / 3600.0f;
        }
        energy_last_tick = now;
    }
}

/* ========================== 公共函数 ========================== */

/**
 * @brief  MPPT 模块初始化
 */
void MPPT_Init(void)
{
    g_mppt.state = MPPT_STATE_INIT;
    g_mppt.n_active = 0;
    g_mppt.n_online = 0;
    g_mppt.direction = MPPT_DIR_INIT;
    g_mppt.stable_count = 0;
    g_mppt.voltage = 0.0f;
    g_mppt.power = 0.0f;
    g_mppt.power_prev = 0.0f;
    g_mppt.power_max = 0.0f;
    g_mppt.n_at_max_power = 0;
    g_mppt.energy_wh = 0.0f;
    g_mppt.cycle_count = 0;
    g_mppt.enabled = 1;  /* 默认使能 */
    wait_counter = 0;
    energy_last_tick = 0;
    energy_initialized = 0;

    printf("MPPT: 模块初始化完成, 加热管电阻=%.1fΩ\r\n", MPPT_HEATER_RESISTANCE);
}

/**
 * @brief  MPPT 任务函数 - 状态机驱动
 * @note   每次 FreeRTOS 任务循环调用一次, 内部根据状态执行不同操作
 */
void MPPT_Task(void)
{
    /* MPPT 未使能时跳过 */
    if (!g_mppt.enabled)
    {
        return;
    }

    switch (g_mppt.state)
    {
        /*----------------------------------------------------------
         * INIT: 初始化
         * 检查在线设备数和总线电压, 决定是否启动 MPPT
         *----------------------------------------------------------*/
        case MPPT_STATE_INIT:
        {
            g_mppt.n_online = count_online_devices();
            g_mppt.voltage = Solar_GetVoltage();

            printf("MPPT INIT: 在线=%d, 电压=%.1fV\r\n",
                   g_mppt.n_online, g_mppt.voltage);

            /* 无在线设备 → 禁用 */
            if (g_mppt.n_online == 0)
            {
                printf("MPPT: 无在线设备, 进入DISABLED\r\n");
                g_mppt.state = MPPT_STATE_DISABLED;
                break;
            }

            /* 电压太低 → 等待光照 */
            if (g_mppt.voltage < MPPT_VOLTAGE_MIN)
            {
                printf("MPPT: 电压%.1fV < 阈值%.1fV, 等待光照\r\n",
                       g_mppt.voltage, MPPT_VOLTAGE_MIN);
                g_mppt.state = MPPT_STATE_DISABLED;
                break;
            }

            /* 条件满足: 先关闭所有加热管, 从 N=0 开始测量 */
            g_mppt.n_active = count_active_heaters();

            /* 如果当前没有开启任何加热管, 先开1台作为起点 */
            if (g_mppt.n_active == 0)
            {
                g_mppt.n_active = MPPT_SetActiveHeaters(1);
                if (g_mppt.n_active == 0)
                {
                    printf("MPPT: 无法开启任何加热器, DISABLED\r\n");
                    g_mppt.state = MPPT_STATE_DISABLED;
                    break;
                }
                /* 等待开启后电压稳定 */
                wait_counter = 0;
                g_mppt.state = MPPT_STATE_WAIT;
                printf("MPPT: 初始开启%d台, 等待稳定\r\n", g_mppt.n_active);
            }
            else
            {
                /* 已有加热管在运行, 直接进入测量 */
                g_mppt.state = MPPT_STATE_MEASURE;
            }
            break;
        }

        /*----------------------------------------------------------
         * DISABLED: 禁用状态
         * 等待条件恢复 (有设备在线 + 电压足够)
         *----------------------------------------------------------*/
        case MPPT_STATE_DISABLED:
        {
            g_mppt.n_online = count_online_devices();
            g_mppt.voltage = Solar_GetVoltage();

            /* 条件恢复 → 回到 INIT */
            if (g_mppt.n_online > 0 && g_mppt.voltage >= MPPT_VOLTAGE_MIN)
            {
                printf("MPPT: 条件恢复(在线=%d, V=%.1fV), 重新初始化\r\n",
                       g_mppt.n_online, g_mppt.voltage);
                g_mppt.state = MPPT_STATE_INIT;
            }
            break;
        }

        /*----------------------------------------------------------
         * MEASURE: 测量当前状态
         * 读取电压, 计算功率, 记录为 P_prev
         *----------------------------------------------------------*/
        case MPPT_STATE_MEASURE:
        {
            /* 更新在线设备数 (可能有设备掉线) */
            g_mppt.n_online = count_online_devices();

            /* 读取当前总线电压 */
            g_mppt.voltage = Solar_GetVoltage();

            /* 电压过低 → 暂停 MPPT */
            if (g_mppt.voltage < MPPT_VOLTAGE_MIN)
            {
                printf("MPPT: 电压%.1fV过低, 暂停跟踪\r\n", g_mppt.voltage);
                g_mppt.state = MPPT_STATE_DISABLED;
                break;
            }

            /* 计算当前功率 */
            g_mppt.power = MPPT_CalcPower(g_mppt.voltage, g_mppt.n_active);
            g_mppt.power_prev = g_mppt.power;

            /* 累计发电量 */
            accumulate_energy();

            printf("MPPT MEASURE: V=%.1fV, N=%d, P=%.1fW\r\n",
                   g_mppt.voltage, g_mppt.n_active, g_mppt.power);

            /* 记录最大功率 */
            if (g_mppt.power > g_mppt.power_max)
            {
                g_mppt.power_max = g_mppt.power;
                g_mppt.n_at_max_power = g_mppt.n_active;
            }

            /* 进入扰动 */
            g_mppt.state = MPPT_STATE_PERTURB;
            break;
        }

        /*----------------------------------------------------------
         * PERTURB: 执行扰动
         * 根据当前方向决定增加还是减少1台加热器
         *----------------------------------------------------------*/
        case MPPT_STATE_PERTURB:
        {
            uint8_t target = g_mppt.n_active;

            /* 根据方向计算目标数量 */
            if (g_mppt.direction == MPPT_DIR_INIT ||
                g_mppt.direction == MPPT_DIR_INCREASE)
            {
                target = g_mppt.n_active + 1;
            }
            else
            {
                target = g_mppt.n_active - 1;
            }

            /* 边界检查 */
            if (target < 1)
            {
                target = 1;
                /* 已在最小值, 反向 */
                g_mppt.direction = MPPT_DIR_INCREASE;
            }
            if (target > g_mppt.n_online)
            {
                target = g_mppt.n_online;
                /* 已在最大值, 反向 */
                g_mppt.direction = MPPT_DIR_DECREASE;
            }

            /* 如果 target 和当前相同, 说明已到边界且方向也改了, 无需扰动 */
            if (target == g_mppt.n_active &&
                g_mppt.direction != MPPT_DIR_INIT)
            {
                /* 两边都到头了, 回到测量维持当前状态 */
                printf("MPPT: 已在边界(N=%d), 维持当前\r\n", g_mppt.n_active);
                g_mppt.state = MPPT_STATE_MEASURE;
                break;
            }

            printf("MPPT PERTURB: %d台 → %d台 (方向=%s)\r\n",
                   g_mppt.n_active, target,
                   g_mppt.direction == MPPT_DIR_INCREASE ? "增加" : "减少");

            /* 执行加热器控制 */
            uint8_t actual = MPPT_SetActiveHeaters(target);

            if (actual != target)
            {
                printf("MPPT: 实际开启%d台(目标%d台), 调整计划\r\n", actual, target);
            }

            g_mppt.n_active = actual;

            /* 进入等待, 让总线电压稳定 */
            wait_counter = 0;
            g_mppt.state = MPPT_STATE_WAIT;
            break;
        }

        /*----------------------------------------------------------
         * WAIT: 等待系统稳定
         * 切换加热管后需要等待一段时间再测量
         *----------------------------------------------------------*/
        case MPPT_STATE_WAIT:
        {
            wait_counter++;

            if (wait_counter >= MPPT_SETTLE_TIME_SEC)
            {
                /* 稳定时间到, 进入观察 */
                g_mppt.state = MPPT_STATE_OBSERVE;
                printf("MPPT: 稳定等待完成, 开始观察\r\n");
            }
            /* 等待期间也累计发电量 (使用上次的功率值) */
            else
            {
                accumulate_energy();
            }
            break;
        }

        /*----------------------------------------------------------
         * OBSERVE: 观察扰动结果
         * 读取新电压, 计算新功率
         *----------------------------------------------------------*/
        case MPPT_STATE_OBSERVE:
        {
            /* 读取新的总线电压 */
            g_mppt.voltage = Solar_GetVoltage();

            /* 电压过低 → 暂停 */
            if (g_mppt.voltage < MPPT_VOLTAGE_MIN)
            {
                printf("MPPT: 观察期电压%.1fV过低, 暂停\r\n", g_mppt.voltage);
                g_mppt.state = MPPT_STATE_DISABLED;
                break;
            }

            /* 计算新功率 */
            g_mppt.power = MPPT_CalcPower(g_mppt.voltage, g_mppt.n_active);

            /* 累计发电量 */
            accumulate_energy();

            printf("MPPT OBSERVE: V=%.1fV, N=%d, P=%.1fW (之前=%.1fW, Δ=%.1fW)\r\n",
                   g_mppt.voltage, g_mppt.n_active, g_mppt.power,
                   g_mppt.power_prev, g_mppt.power - g_mppt.power_prev);

            /* 记录最大功率 */
            if (g_mppt.power > g_mppt.power_max)
            {
                g_mppt.power_max = g_mppt.power;
                g_mppt.n_at_max_power = g_mppt.n_active;
            }

            g_mppt.cycle_count++;

            /* 进入决策 */
            g_mppt.state = MPPT_STATE_DECIDE;
            break;
        }

        /*----------------------------------------------------------
         * DECIDE: 决策下一步
         * 比较 P_new 和 P_old, 决定扰动方向
         *----------------------------------------------------------*/
        case MPPT_STATE_DECIDE:
        {
            float delta_p = g_mppt.power - g_mppt.power_prev;

            if (delta_p > MPPT_POWER_THRESHOLD)
            {
                /* 功率增加 → 方向正确, 继续同方向 */
                printf("MPPT DECIDE: 功率增加%.1fW, 继续%s方向\r\n",
                       delta_p,
                       g_mppt.direction == MPPT_DIR_INCREASE ? "增加" : "减少");

                g_mppt.stable_count = 0;
                /* direction 保持不变 */
            }
            else if (delta_p < -MPPT_POWER_THRESHOLD)
            {
                /* 功率下降 → 方向错误, 反向 */
                if (g_mppt.direction == MPPT_DIR_INCREASE)
                {
                    g_mppt.direction = MPPT_DIR_DECREASE;
                }
                else
                {
                    g_mppt.direction = MPPT_DIR_INCREASE;
                }

                printf("MPPT DECIDE: 功率下降%.1fW, 反向(→%s)\r\n",
                       -delta_p,
                       g_mppt.direction == MPPT_DIR_INCREASE ? "增加" : "减少");

                g_mppt.stable_count = 0;
            }
            else
            {
                /* 功率变化不大 → 可能已在 MPP 附近 */
                g_mppt.stable_count++;
                printf("MPPT DECIDE: 功率变化%.1fW(阈值±%.1fW), 稳定计数=%d/%d\r\n",
                       delta_p, MPPT_POWER_THRESHOLD,
                       g_mppt.stable_count, MPPT_STABLE_COUNT_MAX);

                /* 交替扰动方向, 在 MPP 附近小幅振荡 */
                if (g_mppt.direction == MPPT_DIR_INCREASE)
                {
                    g_mppt.direction = MPPT_DIR_DECREASE;
                }
                else
                {
                    g_mppt.direction = MPPT_DIR_INCREASE;
                }
            }

            /* 回到扰动, 继续下一轮 */
            g_mppt.state = MPPT_STATE_PERTURB;
            break;
        }

        default:
            g_mppt.state = MPPT_STATE_INIT;
            break;
    }
}

/**
 * @brief  计算估算功率
 * @param  voltage: 总线电压 (V)
 * @param  n_heaters: 开启的加热器数量
 * @retval 估算功率 (W)
 */
float MPPT_CalcPower(float voltage, uint8_t n_heaters)
{
    if (n_heaters == 0 || voltage <= 0.0f) return 0.0f;
    return voltage * voltage * (float)n_heaters / MPPT_HEATER_RESISTANCE;
}

/**
 * @brief  使能/禁用 MPPT 功能
 */
void MPPT_Enable(uint8_t enable)
{
    g_mppt.enabled = enable;
    if (!enable)
    {
        printf("MPPT: 已禁用\r\n");
    }
    else
    {
        printf("MPPT: 已使能, 重新初始化\r\n");
        g_mppt.state = MPPT_STATE_INIT;
    }
}

/**
 * @brief  重置日发电量
 */
void MPPT_ResetDailyEnergy(void)
{
    g_mppt.energy_wh = 0.0f;
    g_mppt.power_max = 0.0f;
    g_mppt.cycle_count = 0;
    energy_last_tick = 0;
    energy_initialized = 0;
    printf("MPPT: 日发电量已重置\r\n");
}

/**
 * @brief  获取日发电量 (kWh)
 */
float MPPT_GetDailyEnergy_kWh(void)
{
    return g_mppt.energy_wh / 1000.0f;
}

/**
 * @brief  获取当前估算功率 (W)
 */
float MPPT_GetPower(void)
{
    return g_mppt.power;
}

/**
 * @brief  强制重新搜索 MPP
 */
void MPPT_ForceRescan(void)
{
    printf("MPPT: 强制重新搜索\r\n");
    g_mppt.power_max = 0.0f;
    g_mppt.n_at_max_power = 0;
    g_mppt.stable_count = 0;
    g_mppt.direction = MPPT_DIR_INIT;
    g_mppt.state = MPPT_STATE_INIT;
}

/**
 * @brief  开启指定数量的加热器
 * @param  target_count: 目标开启数量
 * @retval 实际成功控制的数量
 *
 * 策略:
 *   - 需要开启更多: 按设备列表顺序开启 (温度低的优先更优, 但需要排序)
 *   - 需要关闭一些: 按设备列表逆序关闭
 *   - 简化实现: 按列表顺序依次控制
 */
uint8_t MPPT_SetActiveHeaters(uint8_t target_count)
{
    uint8_t actually_active = 0;
    uint8_t controlled = 0;

    /* 先统计当前状态 */
    for (uint16_t i = 0; i < device_count && controlled < 20; i++)
    {
        /* 只处理在线设备 */
        if (!device_list[i].state.bits.valid) continue;
        if (device_list[i].state.bits.comm_err) continue;

        uint8_t is_heating = device_list[i].state.bits.dc_heating;

        if (actually_active < target_count)
        {
            /* 需要开启 */
            if (!is_heating)
            {
                int ret = device_ctrl_heater((int)i, 1);
                if (ret == 0)
                {
                    device_list[i].state.bits.dc_heating = 1;
                    actually_active++;
                }
                else
                {
                    printf("MPPT: 设备[%d]开启失败(ret=%d)\r\n", i, ret);
                }
            }
            else
            {
                /* 已经在加热 */
                actually_active++;
            }
        }
        else
        {
            /* 需要关闭 */
            if (is_heating)
            {
                int ret = device_ctrl_heater((int)i, 0);
                if (ret == 0)
                {
                    device_list[i].state.bits.dc_heating = 0;
                }
                else
                {
                    printf("MPPT: 设备[%d]关闭失败(ret=%d)\r\n", i, ret);
                    actually_active++;  /* 关闭失败, 仍然算激活 */
                }
            }
        }
        controlled++;
    }

    printf("MPPT_SetActiveHeaters: 目标=%d, 实际=%d\r\n", target_count, actually_active);
    return actually_active;
}
