/**
  ******************************************************************************
  * @file    a7680c_netmgr.c
  * @brief   A7680C 网络管理器 — 统一管理模块初始化、健康监控、信号采集
  * @note    从 freertos.c 中抽取, 供 StartDefaultTask 每秒调用一次
  *
  *          状态机:
  *          ┌─────────────┐  need_init=1  ┌──────────────────────┐
  *          │  全量初始化  │ ───────────→ │  运行中(健康检查)     │
  *          │  ATE0→网络   │              │  每秒: CPIN+CGATT+CSQ │
  *          │  →时间→HTTP  │              │  每30秒: CLBS          │
  *          │  →MQTT       │              │  异常10次: need_init=1 │
  *          └─────────────┘              └──────────────────────┘
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "a7680c_netmgr.h"
#include "a7680c_at.h"
#include "a7680c_mqtt.h"
#include "a7680c_http.h"
#include "battery.h"
#include "cmsis_os.h"
#include "stdio.h"

/* ========================== 私有变量 ========================== */

/** 网络就绪标志（供 Weather_Task 和 DevicePoll_Task 读取） */
uint8_t simcard_ready = 0;

/** 是否需要全量初始化 */
static uint8_t need_init = 1;

/** 网络异常连续失败计数 */
static int32_t fail_count = 0;

/** 上网能力周期检查计数器（每30秒CLBS一次） */
static uint16_t online_check_count = 0;

/** 天气软件定时器句柄（初始化成功后启动） */
static osTimerId_t weather_timer = NULL;

/* ========================== 私有函数 ========================== */

/**
  * @brief  全量初始化 A7680C 模块
  * @retval 建议延时(ms): 0=成功, 5000=失败需等待
  * @note   依次执行: ATE0 → 等待网络就绪 → 同步时间 → HTTP → MQTT
  */
static uint32_t netmgr_full_init(void)
{
    simcard_ready = 0;
    fail_count = 0;
    printf("========== A7680C全量初始化开始 ==========\r\n");

    /* ---- 第1步: 关闭回显 ---- */
    uint8_t ate0_ok = 0;
    for (uint8_t retry = 0; retry < 30; retry++)
    {
        if (A7680C_SendATE0() == AT_RESULT_OK)
        {
            ate0_ok = 1;
            break;
        }
        osDelay(1000);
    }
    if (!ate0_ok)
    {
        printf("警告: 30s内未能关闭回显\r\n");
        A7680C_SendAT_CFUN();
        return 5000;
    }

    /* ---- 第2步: 轮询等待网络就绪 (最多60秒) ---- */
    uint8_t net_ok = 0;
    for (uint8_t retry = 0; retry < 60; retry++)
    {
        if (A7680C_CheckNetworkReady() == AT_RESULT_OK)
        {
            net_ok = 1;
            break;
        }
        osDelay(1000);
    }

    if (!net_ok)
    {
        printf("60s内网络未就绪,稍后重试\r\n");
        A7680C_SendAT_CFUN();
        return 5000;
    }

    /* ---- 第3步: 同步网络时间到RTC芯片 ---- */
    uint8_t step;
    if (A7680C_GetNetworkTime_Debug(&step) == AT_RESULT_OK)
    {
        printf("网络时间同步成功\r\n");
    }
    else
    {
        printf("网络时间同步失败, step:%d\r\n", step);
    }

    /* ---- 第4步: 初始化HTTP ---- */
    A7680C_HTTP_Init();

    /* ---- 第5步: 初始化MQTT ---- */
    printf("初始化MQTT连接...\r\n");
    if (A7680C_MQTT_Start() != AT_RESULT_OK)
    {
        printf("MQTT START失败, 5秒后重试\r\n");
        osDelay(5000);
        A7680C_MQTT_Start();
    }
    if (A7680C_MQTT_Connect(MQTT_CLIENT_ID, NULL, NULL) != AT_RESULT_OK)
    {
        printf("MQTT CONNECT失败, 10秒后重试\r\n");
        osDelay(10000);
        A7680C_MQTT_Connect(MQTT_CLIENT_ID, NULL, NULL);
    }

    /* ---- 初始化完成 ---- */
    simcard_ready = 1;
    need_init = 0;

    /* 启动天气定时器（1秒后首次触发） */
    if (weather_timer != NULL)
    {
        osTimerStart(weather_timer, 1000);
    }

    printf("========== A7680C全量初始化完成 ==========\r\n");
    return 0;
}

/**
  * @brief  网络健康检查 + 信号采集（每秒调用一次）
  * @retval 建议延时(ms): 1000=正常, 5000=模块已重启需等待
  */
static uint32_t netmgr_health_check(void)
{
    /* 检测SIM卡 + 网络附着状态 */
    uint8_t sim_ok = A7680C_SendAT_CPIN();
    uint8_t net_ok = 0;

    if (sim_ok == AT_RESULT_OK)
    {
        net_ok = A7680C_SendAT("AT+CGATT?\r\n", "+CGATT: 1", 2000, NULL);
    }

    if (sim_ok == AT_RESULT_OK && net_ok == AT_RESULT_OK)
    {
        /* 网络基本正常 */
        fail_count = 0;

        /* 周期性验证上网能力（每30秒用CLBS检查一次） */
        online_check_count++;
        if (online_check_count >= 30)
        {
            online_check_count = 0;
            if (A7680C_SendAT("AT+CLBS=1\r\n", "+CLBS: 0", 5000, NULL) != AT_RESULT_OK)
            {
                printf("CLBS失败,SIM卡可能欠费无法上网\r\n");
                simcard_ready = 0;
            }
            else
            {
                if (simcard_ready == 0)
                {
                    printf("CLBS成功,上网能力恢复\r\n");
                    simcard_ready = 1;
                }
            }
        }

        /* 读取CSQ并更新全局信号等级 (LVGL定时器会自动刷新UI) */
        uint8_t signal_buf[32];
        int32_t rssi, ber;
        if (A7680C_SendAT_CSQ(signal_buf) == AT_RESULT_OK)
        {
            A7680C_ParseCSQ(signal_buf, &rssi, &ber);
            g_signal_level = Signal_GetLevel(rssi);
        }
    }
    else
    {
        /* 网络异常：累计失败次数 */
        fail_count++;
        printf("网络异常(SIM=%d,CGATT=%d, fail=%d/10)\r\n", sim_ok, net_ok, fail_count);

        g_signal_level = -1;  /* 无信号, LVGL定时器会显示X */

        if (fail_count >= 10)
        {
            printf("网络连续10次异常,重启模块并重新初始化\r\n");
            simcard_ready = 0;
            A7680C_SendAT_CFUN();  /* 重启模块 */
            need_init = 1;         /* 触发全量重新初始化 */
            return 5000;
        }
    }

    return 1000;
}

/* ========================== 公共函数 ========================== */

/**
  * @brief  网络管理器初始化
  * @param  weather_timer: 天气软件定时器句柄
  */
void A7680C_NetManager_Init(void *weather_timer_ptr)
{
    weather_timer = (osTimerId_t)weather_timer_ptr;
}

/**
  * @brief  网络管理器周期处理（在主循环中调用）
  * @retval 建议的延时时间(ms)
  */
uint32_t A7680C_NetManager_Process(void)
{
    if (need_init)
    {
        uint32_t delay = netmgr_full_init();
        /* full_init 成功时 need_init=0, 失败时 need_init 保持=1 */
        if (need_init)
        {
            /* 初始化失败, 保持标志, 返回等待时间 */
            return delay;
        }
        /* 初始化成功, 不延时, 立即进入下一轮开始健康检查 */
        return 0;
    }

    return netmgr_health_check();
}
