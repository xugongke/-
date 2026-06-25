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
#include "string.h"
#include "ff.h"
#include "fatfs.h"
#include "rx8025t.h"

/* ========================== 全局实例 ========================== */

mppt_data_t g_mppt = {0};
solar_energy_data_t g_solar_energy = {0};

/* MPPT 主动控制 */
uint32_t heating_seconds[MAX_DEVICES] = {0};  /* 每台累计加热秒数(公平性排序用) */
static   uint8_t mppt_first_round = 1;        /* 首轮标志(采集基准不扰动) */

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
            !device_list[i].state.bits.comm_err &&    /* 排除通信异常(状态过期不可信) */
            !device_list[i].state.bits.relay_err &&   /* 排除继电器故障(无法执行控制) */
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

/* ========================== 公共函数 ========================== */
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
/* ========================== 太阳能发电量管理 ========================== */

/**
 * @brief  从SD卡加载太阳能发电量数据到RAM；若文件不存在则创建默认文件
 * @note   在文件系统初始化完成后调用 (lvgl_task中)
 */
void solar_energy_init(void)
{
    FRESULT res;
    UINT br;

    if(fs_mutex) osMutexAcquire(fs_mutex, osWaitForever);

    /* 尝试打开已存在的文件 */
    res = f_open(&SDFile, SOLAR_ENERGY_FILE, FA_OPEN_EXISTING | FA_READ);
    if (res == FR_OK)
    {
        /* 文件存在，读取数据 */
        res = f_read(&SDFile, &g_solar_energy, sizeof(solar_energy_data_t), &br);
        f_close(&SDFile);
        if(fs_mutex) osMutexRelease(fs_mutex);

        if (res == FR_OK && br == sizeof(solar_energy_data_t) &&
            g_solar_energy.magic == SOLAR_ENERGY_MAGIC)
        {
            printf("太阳能发电量文件加载成功: 日=%.3f, 月=%.3f, 年=%.3f, 总=%.3f kWh\r\n",
                   g_solar_energy.daily_generation_kwh,
                   g_solar_energy.monthly_generation_kwh,
                   g_solar_energy.annual_generation_kwh,
                   g_solar_energy.total_generation_kwh);
            return;
        }
        printf("太阳能发电量文件校验失败，创建新文件\r\n");
    }
    else
    {
        if(fs_mutex) osMutexRelease(fs_mutex);
        printf("太阳能发电量文件不存在，创建新文件\r\n");
    }

    /* 文件不存在或校验失败，创建默认数据 */
    memset(&g_solar_energy, 0, sizeof(solar_energy_data_t));
    g_solar_energy.magic = SOLAR_ENERGY_MAGIC;
    g_solar_energy.version = SOLAR_ENERGY_VERSION;

    /* 读取当前RTC时间作为初始重置时间 */
    RX8025T_DateTimeCompact rtc_now;
    if (RX8025T_GetDateTime(&rtc_now) == HAL_OK)
    {
        g_solar_energy.last_reset_day = rtc_now.day;
        g_solar_energy.last_reset_mon = rtc_now.month;
    }

    /* 写入SD卡 */
    solar_energy_save();
    printf("太阳能发电量文件创建成功\r\n");
}

/**
 * @brief  将RAM中的太阳能发电量数据写入SD卡（清空写）
 */
void solar_energy_save(void)
{
    FRESULT res;
    UINT bw;

    if(fs_mutex) osMutexAcquire(fs_mutex, osWaitForever);

    res = f_open(&SDFile, SOLAR_ENERGY_FILE, FA_CREATE_ALWAYS | FA_WRITE);
    if (res != FR_OK)
    {
        printf("太阳能发电量文件打开失败: %d\r\n", res);
        if(fs_mutex) osMutexRelease(fs_mutex);
        return;
    }

    res = f_write(&SDFile, &g_solar_energy, sizeof(solar_energy_data_t), &bw);
    f_close(&SDFile);
    if(fs_mutex) osMutexRelease(fs_mutex);

    if (res != FR_OK || bw != sizeof(solar_energy_data_t))
    {
        printf("太阳能发电量文件写入失败\r\n");
    }
}

/**
 * @brief  处理太阳能发电量的日/月/年重置逻辑，将g_mppt.energy_wh累加到g_solar_energy
 * @note   在daily_energy_flush_to_sd中调用（零点结算时）
 */
void solar_energy_flush(void)
{
    /* 获取当前RTC时间 */
    RX8025T_DateTimeCompact rtc_now;
    if (RX8025T_GetDateTime(&rtc_now) != HAL_OK)
    {
        printf("太阳能发电量结算失败: 无法获取RTC时间\r\n");
        return;
    }

    float energy_kwh = g_mppt.energy_wh / 1000.0f;  /* Wh → kWh */

    printf("太阳能发电量结算: 当前RAM累积=%dWh (%.3fkWh)\r\n", g_mppt.energy_wh, energy_kwh);

    /* 检查是否需要重置日发电量（日期变化时重置） */
    if (g_solar_energy.last_reset_day != rtc_now.day)
    {
        printf("太阳能 日发电量重置 (上次: %02d, 当前: %02d)\r\n",
               g_solar_energy.last_reset_day, rtc_now.day);

        /* 先把昨天剩余的累积电量计入各维度 */
        g_solar_energy.monthly_generation_kwh += energy_kwh;
        g_solar_energy.annual_generation_kwh  += energy_kwh;
        g_solar_energy.total_generation_kwh   += energy_kwh;

        /* 更新15日发电量数组：整体左移，[14]放入当日发电量 */
        memmove(&g_solar_energy.history_daily[0], &g_solar_energy.history_daily[1],
                (SOLAR_HISTORY_DAYS - 1) * sizeof(float));
        g_solar_energy.history_daily[SOLAR_HISTORY_DAYS - 1] = g_solar_energy.daily_generation_kwh;

        /* 日发电量重置为今天的累积 */
        g_solar_energy.daily_generation_kwh = energy_kwh;
        g_solar_energy.last_reset_day = rtc_now.day;
    }

    /* 检查是否需要重置月发电量（月份变化时重置） */
    if (g_solar_energy.last_reset_mon != rtc_now.month)
    {
        uint8_t old_mon = g_solar_energy.last_reset_mon;
        printf("太阳能 月发电量重置 (上次: %02d, 当前: %02d)\r\n", old_mon, rtc_now.month);

        g_solar_energy.monthly_generation_kwh = energy_kwh;
        g_solar_energy.last_reset_mon = rtc_now.month;

        /* 跨年检查：如果月份变为1月且上次记录不是1月，说明是新的一年 */
        if (rtc_now.month == 1 && old_mon != 1)
        {
            printf("太阳能 年发电量重置\r\n");
            g_solar_energy.annual_generation_kwh = energy_kwh;
        }
    }

    /* 清零RAM中的日累积 */
    g_mppt.energy_wh = 0.0f;

    /* 更新时间戳 */
    g_solar_energy.update_time.year    = rtc_now.year;
    g_solar_energy.update_time.month   = rtc_now.month;
    g_solar_energy.update_time.day     = rtc_now.day;
    g_solar_energy.update_time.hours   = rtc_now.hours;
    g_solar_energy.update_time.minutes = rtc_now.minutes;
    g_solar_energy.update_time.seconds = rtc_now.seconds;

    /* 写入SD卡 */
    solar_energy_save();

    printf("太阳能发电量结算完成: 日=%.3f, 月=%.3f, 年=%.3f, 总=%.3f kWh\r\n",
           g_solar_energy.daily_generation_kwh,
           g_solar_energy.monthly_generation_kwh,
           g_solar_energy.annual_generation_kwh,
           g_solar_energy.total_generation_kwh);
}

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

/* ========================== MPPT 主动控制 (异步架构) ========================== */

/**
 * @brief  统计可调度设备数 (在线 且 温度<75 C)
 */
static uint8_t count_schedulable_devices(void)
{
    uint8_t count = 0;
    for (uint16_t i = 0; i < device_count; i++)
    {
        if (device_list[i].state.bits.valid &&
            !device_list[i].state.bits.comm_err &&    /* 排除通信异常 */
            !device_list[i].state.bits.relay_err &&   /* 排除继电器故障 */
            device_list[i].temperature < 75)
        {
            count++;
        }
    }
    return count;
}

/**
 * @brief  选1台设备开启 (温度最低+加热时长最少的OFF设备)
 * @retval >=0: 设备索引, -1: 无可开设备
 */
static int mppt_select_one_to_enable(void)
{
    int best = -1;
    float best_score = 1e9f;
    for (int i = 0; i < device_count; i++)
    {
        if (device_list[i].state.bits.valid &&
            !device_list[i].state.bits.comm_err &&
            !device_list[i].state.bits.relay_err &&
            device_list[i].temperature < 75 &&
            !device_list[i].state.bits.dc_heating)  /* 当前OFF */
        {
            float score = (float)device_list[i].temperature
                        + (float)heating_seconds[i] / 100.0f;
            if (score < best_score) { best_score = score; best = i; }
        }
    }
    return best;
}

/**
 * @brief  选1台设备关闭 (温度最高+加热时长最多的ON设备)
 * @retval >=0: 设备索引, -1: 无可关设备
 */
static int mppt_select_one_to_disable(void)
{
    int best = -1;
    float best_score = -1e9f;
    for (int i = 0; i < device_count; i++)
    {
        if (device_list[i].state.bits.valid &&
            !device_list[i].state.bits.comm_err &&
            device_list[i].state.bits.dc_heating)  /* 当前ON */
        {
            float score = (float)device_list[i].temperature
                        + (float)heating_seconds[i] / 100.0f;
            if (score > best_score) { best_score = score; best = i; }
        }
    }
    return best;
}

/**
 * @brief  MPPT P&O 控制闭环 (采集阶段之后调用)
 * @note   快速循环: ±1台→发0x02/0.03等ACK→等3秒→读电压→P&O判断
 *         功率增加继续搜索, 功率下降撤回最后一步并退出
 *         继电器不能频繁通断, 找到MPP后立即停止
 */
void MPPT_ControlLoop(void)
{
    float v = Solar_GetVoltage();
    g_mppt.voltage = v;
    g_mppt.n_active = count_active_heaters();
    g_mppt.n_online = count_online_devices();

    /* 载波断链或无设备, 不控制 */
    if (v < 28.0f || g_mppt.n_online == 0) return;

    /* 首轮: 建立功率基准, 不扰动 */
    if (mppt_first_round)
    {
        mppt_first_round = 0;
        g_mppt.power_prev = MPPT_CalcPower(v, g_mppt.n_active);
        g_mppt.direction = MPPT_DIR_DECREASE;  /* 从机默认全开, 首次应尝试减载 */
        printf("MPPT首轮: V=%.1fV, 当前开启加热的数量=%d, P=%.1fW, 建立基准\r\n",
               v, g_mppt.n_active, g_mppt.power_prev);
        return;
    }

    uint8_t n_schedulable = count_schedulable_devices();
    if (n_schedulable == 0) return;

    printf("MPPT控制开始: 当前开启的加热器数量=%d, 扰动方向=%d\r\n", g_mppt.n_active, g_mppt.direction);

    /* P&O 快速控制循环 */
    while (1)
    {
        int idx;

        /* ① 选管 + 发命令 (±1台) */
        if (g_mppt.direction == MPPT_DIR_INCREASE)
        {
            idx = mppt_select_one_to_enable();
            printf("MPPT: 选中设备[%d]开启\r\n", idx);
            if (idx < 0)
            {
                printf("MPPT: 无可开启设备, 退出控制循环\r\n");
                /* 反向方向, 下一轮采集后使用 */
                g_mppt.direction = (g_mppt.direction == MPPT_DIR_INCREASE)
                                ? MPPT_DIR_DECREASE : MPPT_DIR_INCREASE;
                break;  /* 无可开设备, 退出 */
            } 
            if (device_ctrl_heater(idx, 1) != 0) {
                device_list[idx].state.bits.relay_err = 1;  /* 标记跳过, 下轮采集纠正 */
                continue;
            }
            device_list[idx].state.bits.dc_heating = 1;
            g_mppt.n_active++;
        }
        else
        {
            idx = mppt_select_one_to_disable();
            printf("MPPT: 选中设备[%d]关闭\r\n", idx);
            if (idx < 0)
            {
                printf("MPPT: 无可关闭设备, 退出控制循环\r\n");
                /* 反向方向, 下一轮采集后使用 */
                g_mppt.direction = (g_mppt.direction == MPPT_DIR_INCREASE)
                                ? MPPT_DIR_DECREASE : MPPT_DIR_INCREASE;
                break;  /* 无可关设备, 退出 */
            }
            if (device_ctrl_heater(idx, 0) != 0) {
                device_list[idx].state.bits.relay_err = 1;
                continue;
            }
            device_list[idx].state.bits.dc_heating = 0;
            g_mppt.n_active--;
        }

        /* ② 等继电器动作 */
        osDelay(3000);

        /* ③ 读电压算功率 */
        v = Solar_GetVoltage();
        g_mppt.voltage = v;
        g_mppt.power = MPPT_CalcPower(v, g_mppt.n_active);

        /* ④ P&O 判断 */
        float delta_p = g_mppt.power - g_mppt.power_prev;
        g_mppt.cycle_count++;

        printf("MPPT控制: V=%.1fV, N=%d, P=%.1fW(ΔP=%.1fW)\r\n",
               v, g_mppt.n_active, g_mppt.power, delta_p);

        if (delta_p > MPPT_POWER_THRESHOLD)
        {
            /* 功率增加, 方向正确, 继续搜索 */
            g_mppt.power_prev = g_mppt.power;
        }
        else
        {
            /* 功率下降或不变 → 找到MPP, 撤回最后一步 */
            printf("MPPT: 功率未增加, 撤回最后一步, 退出控制\r\n");
            if (g_mppt.direction == MPPT_DIR_INCREASE)
            {
                if (device_ctrl_heater(idx, 0) == 0)
                    device_list[idx].state.bits.dc_heating = 0;
                g_mppt.n_active--;
            }
            else
            {
                if (device_ctrl_heater(idx, 1) == 0)
                    device_list[idx].state.bits.dc_heating = 1;
                g_mppt.n_active++;
            }
            /* 反向方向, 下一轮采集后使用 */
            g_mppt.direction = (g_mppt.direction == MPPT_DIR_INCREASE)
                             ? MPPT_DIR_DECREASE : MPPT_DIR_INCREASE;
            break;  /* 退出控制循环 */
        }
    }

    printf("MPPT控制完成: 当前开启的加热器数量=%d\r\n", g_mppt.n_active);
}
