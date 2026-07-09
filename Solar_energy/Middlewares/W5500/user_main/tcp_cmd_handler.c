/**
 * @file    tcp_cmd_handler.c
 * @brief   TCP命令处理 - 帧协议命令分发与响应实现
 */

#include "tcp_cmd_handler.h"
#include "user_main.h"
#include "device_manager.h"
#include "user_data_manager.h"
#include "es1642_usage_guide.h"
#include "mppt.h"
#include "host_comm.h"
#include "ota_upgrade.h"
#include "socket.h"
#include "cmsis_os.h"
#include "fatfs.h"
#include <stdio.h>
#include <string.h>

/* ==================== 外部变量 ==================== */
extern volatile uint8_t g_tcp_connected;
extern osMutexId_t fs_mutex;  /* 文件系统互斥锁(定义在freertos.c) */

/* ==================== 全局帧数据缓冲区 ==================== */
uint8_t g_frame_data_buf[FRAME_MAX_DATA_LEN];  /* 组帧数据缓冲区，供分包发送函数共用 */

/* ==================== 备注信息RAM缓冲区 ==================== */
static uint8_t g_remark_buf[256];  /* 备注信息最大256字节 */
static uint16_t g_remark_len = 0;  /* 备注信息实际长度 */

/* ==================== 楼栋号RAM缓冲区 ==================== */
static uint8_t g_building_no = 0;  /* 楼栋号(由上位机设置, 开机从SD卡加载) */

/* ==================== OTA处理函数前向声明 ==================== */
void tcp_resp_ota_begin(const uint8_t *data, uint16_t len);
void tcp_resp_ota_data(const uint8_t *data, uint16_t len);
void tcp_resp_ota_end(const uint8_t *data, uint16_t len);
void tcp_resp_ota_status(void);

/* ================================================================
 *  通用工具函数
 * ================================================================ */

/**
 * @brief   发送错误帧
 * @param   error_code: 错误码
 *
 * 错误帧格式: [0x0D][0x02][0x00][0x01][ERR_CODE][CRC_H][CRC_L][0x0E]
 */
void tcp_send_error(uint8_t error_code)
{
    tcp_send_frame(FRAME_TYPE_ERROR, 0x00, &error_code, 1);
}

/* ================================================================
 *  命令分发
 * ================================================================ */

/**
 * @brief   处理接收到的完整帧 (从 tcp_frame_handler 调用)
 * @param   type: 帧类型
 * @param   cmd:  命令字 (独立字段, 不在数据域中)
 * @param   data: 数据域指针
 * @param   len:  数据域长度
 */
void tcp_dispatch_frame(uint8_t type, uint8_t cmd, const uint8_t *data, uint16_t len)
{
    /* 只处理请求帧 */
    if (type != FRAME_TYPE_REQUEST)
    {
        printf("Unknown frame type: 0x%02X\r\n", type);
        return;
    }

    switch (cmd)
    {
        case CMD_DEVICE_MANAGE_MODE:
            /* 设备管理模式: 数据域 = [0x01=进入 / 0x00=退出] (1字节) */
            tcp_resp_device_manage_mode(data, len);
            break;

        case CMD_GET_DEVICE_LIST:
            /* 读取设备列表: 数据域为空 */
            tcp_resp_device_list();
            break;

        case CMD_BIND_DEVICE:
            /* 绑定设备: 数据域 = [MAC[6]][楼栋][单元][房号L][房号H] = 10字节 */
            tcp_resp_bind_device(data, len);
            break;

        case CMD_START_SEARCH:
            /* 开始搜索: 数据域为空 */
            tcp_resp_start_search();
            break;

        case CMD_STOP_SEARCH:
            /* 停止搜索: 数据域为空 */
            tcp_resp_stop_search();
            break;

        case CMD_GET_USER_DATA:
            /* 读取用户 用电量 数据: 数据域为空 */
            tcp_resp_user_data();
            break;

        case CMD_GET_SOLAR_ENERGY:
            /* 读取太阳能板 发电量 : 数据域为空 */
            tcp_resp_solar_energy();
            break;

        case CMD_SET_REMARK:
            /* 设置备注信息: 数据域 = 备注文本(最多256字节) */
            tcp_resp_set_remark(data, len);
            break;

        case CMD_GET_REMARK:
            /* 读取备注信息: 数据域为空 */
            tcp_resp_get_remark();
            break;

        case CMD_SET_BUILDING:
            /* 设置楼栋号: 数据域 = [楼栋号] (1字节) */
            tcp_resp_set_building(data, len);
            break;

        case CMD_GET_HOST_INFO:
            /* 读取主机信息: 数据域为空, 返回MAC+楼栋号 */
            tcp_resp_get_host_info();
            break;

        case CMD_OTA_BEGIN:
            tcp_resp_ota_begin(data, len);
            break;

        case CMD_OTA_DATA:
            tcp_resp_ota_data(data, len);
            break;

        case CMD_OTA_END:
            tcp_resp_ota_end(data, len);
            break;

        case CMD_OTA_STATUS:
            tcp_resp_ota_status();
            break;

        default:
            printf("未知命令: 0x%02X\r\n", cmd);
            tcp_send_error(ERR_INVALID_CMD);
            break;
    }
}

/* ================================================================
 *  命令实现
 * ================================================================ */

/**
 * @brief   分包发送设备列表
 *
 * 响应帧数据域格式 (CMD=0x03在帧头命令字字段, 不在数据域中):
 *   第1包 (包头):
 *     [0x00=首包标志][总设备数L][总设备数H]
 *     + [本包设备数L][本包设备数H]
 *     + device_t[0..N-1] 原始字节
 *
 *   后续包:
 *     [包序号(1~total)][总设备数L][总设备数H]
 *     + [本包设备数L][本包设备数H]
 *     + device_t[0..N-1] 原始字节
 *
 * 多字节字段均为小端序
 * 每个设备占 sizeof(device_t)=24 字节, 每包最多21个设备 (含 daily_energy_wh 字段)
 * 数据域最大 = 1(seq) + 2(total) + 2(count) + 21*24 = 509 ≤ 512
 */
void tcp_resp_device_list(void)
{
    if (!g_tcp_connected)
        return;

    uint16_t total_devices = device_count;
    if (total_devices == 0)
    {
        /* 空列表: 发送首包, 设备数为0 */
        uint8_t resp[5];
        resp[0] = 0x00;                              /* 首包标志 */
        resp[1] = (uint8_t)(total_devices & 0xFF);    /* 总设备数L */
        resp[2] = (uint8_t)(total_devices >> 8);      /* 总设备数H */
        resp[3] = 0x00;                               /* 本包设备数L */
        resp[4] = 0x00;                               /* 本包设备数H */
        tcp_send_frame(FRAME_TYPE_RESPONSE, CMD_GET_DEVICE_LIST, resp, 5);
        return;
    }

    /* 计算分包 */
    uint16_t dev_per_pack = DEVICE_PER_PACKET;
    uint8_t  total_packs  = (uint8_t)((total_devices + dev_per_pack - 1) / dev_per_pack);

    /* 分包发送 */
    uint16_t dev_offset = 0;
    uint16_t remain, count, data_len, idx;
    
    for (uint8_t seq = 0; seq < total_packs; seq++)
    {
        /* 本包设备数 */
        remain = total_devices - dev_offset;
        count  = (remain > dev_per_pack) ? dev_per_pack : remain;

        /* 计算数据域长度: 1(seq) + 2(total_dev) + 2(pack_count) + count*21 */
        data_len = 5 + (uint16_t)(count * sizeof(device_t));
        idx = 0;

        /* 包序号 (0=首包) */
        g_frame_data_buf[idx++] = seq;

        /* 总设备数 (小端序) */
        g_frame_data_buf[idx++] = (uint8_t)(total_devices & 0xFF);
        g_frame_data_buf[idx++] = (uint8_t)(total_devices >> 8);

        /* 本包设备数 (小端序) */
        g_frame_data_buf[idx++] = (uint8_t)(count & 0xFF);
        g_frame_data_buf[idx++] = (uint8_t)(count >> 8);

        /* 拷贝设备原始数据 */
        for (uint16_t d = 0; d < count; d++)
        {
            memcpy(&g_frame_data_buf[idx], &device_list[dev_offset + d], sizeof(device_t));
            idx += sizeof(device_t);
        }

        /* 发送这一包 */
        tcp_send_frame(FRAME_TYPE_RESPONSE, CMD_GET_DEVICE_LIST, g_frame_data_buf, data_len);

        dev_offset += count;

        /* 包间延时, 防止发送缓冲区溢出 */
        osDelay(2);
    }

    printf("Device list sent: %d devices, %d packets\r\n", total_devices, total_packs);
}

/**
 * @brief   处理绑定设备命令
 *
 * 请求数据域: [MAC[6]][楼栋][单元][房号L][房号H] (10字节, 房号小端序)
 * 成功响应:   数据域=[0x00=成功]
 * 失败响应:   错误帧 数据域=[ERR_CODE]
 */
void tcp_resp_bind_device(const uint8_t *data, uint16_t len)
{
    /* 数据域 = [MAC(6)] + [楼栋(1)] + [单元(1)] + [房号(2)] = 10 */
    if (len < 10)
    {
        tcp_send_error(ERR_PARAM_INVALID);
        return;
    }

    const uint8_t *mac      = &data[0];   /* MAC地址 */
    uint8_t  building       = data[6];    /* 楼栋 */
    uint8_t  unit           = data[7];    /* 单元 */
    uint16_t room           = (uint16_t)data[8] | ((uint16_t)data[9] << 8); /* 房号(小端序) */

    /* 将用户住址转换成通信地址 */
    uint8_t addr[6];
    make_addr(addr, building, unit, room);

    /* 更新设备 (修改地址 + 入网) */
    int ret = update_device((uint8_t *)mac, addr);

    if (ret == 0)
    {
        /* 成功 */
        uint8_t resp[1] = { 0x00 };
        tcp_send_frame(FRAME_TYPE_RESPONSE, CMD_BIND_DEVICE, resp, 1);

        /* 保存到SD卡 */
        save_devices();
        printf("Bind OK: MAC=%02X:%02X:%02X:%02X:%02X:%02X\r\n",
               mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    }
    else
    {
        /* 失败, 映射错误码 */
        uint8_t err;
        switch (ret)
        {
            case 1:  err = ERR_DEVICE_NOT_FOUND; break;
            case 2:  err = ERR_ES1642_DAMAGED;   break;
            case 3:  err = ERR_ADDR_TIMEOUT;     break;
            case 4:  err = ERR_SEND_FAILED;      break;
            case 5:  err = ERR_NET_TIMEOUT;      break;
            case 6:  err = ERR_NET_FAILED;       break;
            case 7:  err = ERR_SEND_FAILED;      break;
            default: err = ERR_PARAM_INVALID;    break;
        }

        uint8_t resp[1] = { err };
        tcp_send_frame(FRAME_TYPE_ERROR, CMD_BIND_DEVICE, resp, 1);
        printf("绑定失败: ret=%d, err=0x%02X\r\n", ret, err);
    }
}

/**
 * @brief   处理设备管理模式命令 (CMD_DEVICE_MANAGE_MODE)
 *
 * 请求数据域: [0x01=进入管理模式 / 0x00=退出管理模式] (1字节)
 *   - 进入管理模式: 暂停 DevicePoll_Task 的从机轮询, 便于搜索/绑定设备
 *   - 退出管理模式: 恢复从机轮询
 * 成功响应:   数据域=[0x00=成功]
 * 失败响应:   错误帧 数据域=[ERR_CODE]
 *
 * @note  g_device_manage_mode 与 g_es1642_searching 共同构成轮询门控:
 *        仅当两者均为0时, DevicePoll_Task 才执行从机轮询。
 *        TCP断开时 user_main.c 会自动将 g_device_manage_mode 清零。
 */
void tcp_resp_device_manage_mode(const uint8_t *data, uint16_t len)
{
    /* 数据域长度校验: 必须为1字节 */
    if (len < 1)
    {
        tcp_send_error(ERR_PARAM_INVALID);
        return;
    }

    uint8_t mode = data[0];

    if (mode == 0x01)
    {
        /* 进入设备管理模式: 暂停从机轮询 */
        g_device_manage_mode = 1;
        printf("进入设备管理模式, 从机轮询已暂停\r\n");
    }
    else if (mode == 0x00)
    {
        /* 退出设备管理模式: 恢复从机轮询 */
        g_device_manage_mode = 0;
        printf("退出设备管理模式, 从机轮询已恢复\r\n");
    }
    else
    {
        /* 非法参数 */
        tcp_send_error(ERR_PARAM_INVALID);
        printf("设备管理模式参数错误: 0x%02X\r\n", mode);
        return;
    }

    /* 成功响应 */
    uint8_t resp[1] = { 0x00 };
    tcp_send_frame(FRAME_TYPE_RESPONSE, CMD_DEVICE_MANAGE_MODE, resp, 1);
}

/**
 * @brief   处理开始搜索设备命令
 *
 * 请求数据域: [CMD=0x03]
 * 成功响应:   [CMD=0x03][0x00=成功]
 */
void tcp_resp_start_search(void)
{
    int ret = ES1642_StartSearch(0, ES1642_SEARCH_RULE_ALL);  /* depth=0(自动), rule=搜索所有设备 */
    if(ret != 0)
    {
        uint8_t resp[1] = { 0x01 };  /* 失败 */
        tcp_send_frame(FRAME_TYPE_RESPONSE, CMD_START_SEARCH, resp, 1);
    }
}
/**
 * @brief   处理开始搜索设备命令 (成功)
 *
 * 请求数据域: [CMD=0x03]
 * 成功响应:   [CMD=0x03][0x00=成功]
 */
void tcp_resp_start_search_ok(void)
{
    uint8_t resp[1] = { 0x00 }; /* 成功 */
    tcp_send_frame(FRAME_TYPE_RESPONSE, CMD_START_SEARCH, resp, 1);
}
/**
 * @brief   处理停止搜索设备命令
 *
 * 请求数据域: [CMD=0x04]
 * 成功响应:   [CMD=0x04][0x00=成功]
 */
void tcp_resp_stop_search(void)
{
    int ret = ES1642_StopSearch();
    if(ret != 0)
    {
        uint8_t resp[1] = { 0x01 };  /* 失败 */
        tcp_send_frame(FRAME_TYPE_RESPONSE, CMD_STOP_SEARCH, resp, 1);
    }
}
/**
 * @brief   处理停止搜索设备命令 (成功)
 *
 * 请求数据域: [CMD=0x04]
 * 成功响应:   [CMD=0x04][0x00=成功]
 */
void tcp_resp_stop_search_ok(void)
{
    uint8_t resp[1] = { 0x00 }; /* 成功 */
    tcp_send_frame(FRAME_TYPE_RESPONSE, CMD_STOP_SEARCH, resp, 1);
}

/**
 * @brief   分包发送用户用电量数据
 *
 * 响应帧数据域格式 (CMD=0x07在帧头命令字字段, 不在数据域中):
 *   第1包 (包头):
 *     [0x00=首包标志][总设备数L][总设备数H]
 *     + [本包设备数L][本包设备数H]
 *     + user_detail_cache_t[0..N-1] 原始字节
 *
 *   后续包:
 *     [包序号(1~total)][总设备数L][总设备数H]
 *     + [本包设备数L][本包设备数H]
 *     + user_detail_cache_t[0..N-1] 原始字节
 *
 * 多字节字段均为小端序
 * 每个用户数据占 sizeof(user_detail_cache_t)=53 字节, 每包最多9个用户
 * 数据域最大 = 1(seq) + 2(total) + 2(count) + 9*53 = 486 ≤ 512
 */
void tcp_resp_user_data(void)
{
    printf("分包发送用户用电量数据\r\n");
    if (!g_tcp_connected)
        return;

    uint16_t total_users = device_count;
    if (total_users == 0)
    {
        /* 空列表: 发送首包, 用户数为0 */
        uint8_t resp[5];
        resp[0] = 0x00;                              /* 首包标志 */
        resp[1] = (uint8_t)(total_users & 0xFF);     /* 总用户数L */
        resp[2] = (uint8_t)(total_users >> 8);       /* 总用户数H */
        resp[3] = 0x00;                               /* 本包用户数L */
        resp[4] = 0x00;                               /* 本包用户数H */
        tcp_send_frame(FRAME_TYPE_RESPONSE, CMD_GET_USER_DATA, resp, 5);
        return;
    }

    /* 计算分包 */
    uint16_t users_per_pack = USER_DATA_PER_PACKET;
    uint8_t  total_packs    = (uint8_t)((total_users + users_per_pack - 1) / users_per_pack);

    /* 分包发送 */
    uint16_t user_offset = 0;
    uint16_t remain, count, data_len, idx;
    
    for (uint8_t seq = 0; seq < total_packs; seq++)
    {
        /* 本包用户数 */
        remain = total_users - user_offset;
        count  = (remain > users_per_pack) ? users_per_pack : remain;

        /* 计算数据域长度: 1(seq) + 2(total_user) + 2(pack_count) + count*53 */
        data_len = 5 + (uint16_t)(count * sizeof(user_detail_cache_t));
        idx = 0;

        /* 包序号 (0=首包) */
        g_frame_data_buf[idx++] = seq;

        /* 总用户数 (小端序) */
        g_frame_data_buf[idx++] = (uint8_t)(total_users & 0xFF);
        g_frame_data_buf[idx++] = (uint8_t)(total_users >> 8);

        /* 本包用户数 (小端序) */
        g_frame_data_buf[idx++] = (uint8_t)(count & 0xFF);
        g_frame_data_buf[idx++] = (uint8_t)(count >> 8);

        /* 拷贝用户用电量原始数据 */
        for (uint16_t u = 0; u < count; u++)
        {
            memcpy(&g_frame_data_buf[idx], &user_detail_cache[user_offset + u], sizeof(user_detail_cache_t));
            idx += sizeof(user_detail_cache_t);
        }

        /* 发送这一包 */
        tcp_send_frame(FRAME_TYPE_RESPONSE, CMD_GET_USER_DATA, g_frame_data_buf, data_len);

        user_offset += count;

        /* 包间延时, 防止发送缓冲区溢出 */
        osDelay(2);
    }

    printf("User data sent: %d users, %d packets\r\n", total_users, total_packs);
}

/* ==================== 太阳能发电量响应结构体 ====================
 * 用于CMD_GET_SOLAR_ENERGY(0x08)响应帧数据域的打包, 紧凑无填充
 * 数据域布局(共82字节):
 *   [daily_generation_kwh   4B][monthly_generation_kwh 4B]
 *   [annual_generation_kwh  4B][total_generation_kwh   4B]
 *   [history_daily[15]    15*4=60B]
 *   [update_time             6B]
 */
#pragma pack(push, 1)
typedef struct {
    float    daily_generation_kwh;               /* 日发电量(kWh) */
    float    monthly_generation_kwh;             /* 月发电量(kWh) */
    float    annual_generation_kwh;              /* 年发电量(kWh) */
    float    total_generation_kwh;               /* 总发电量(kWh) */
    float    history_daily[SOLAR_HISTORY_DAYS];  /* 近15日发电量(kWh) */
    user_data_timestamp_t update_time;           /* 数据最后更新时间 */
} solar_energy_resp_t;
#pragma pack(pop)

/**
 * @brief   发送太阳能板发电量数据 (响应CMD_GET_SOLAR_ENERGY)
 *
 * 响应帧数据域格式 (CMD=0x08在帧头命令字字段, 不在数据域中):
 *   [daily_generation_kwh   4B][monthly_generation_kwh 4B]
 *   [annual_generation_kwh  4B][total_generation_kwh   4B]
 *   [history_daily[15]    15*4=60B]
 *   [update_time             6B]
 *
 * 多字节字段均为小端序 (ARM Cortex-M 默认小端)
 * 数据域长度 = sizeof(solar_energy_resp_t) = 82 字节 ≤ 512, 单帧发送
 */
void tcp_resp_solar_energy(void)
{
    if (!g_tcp_connected)
        return;

    /* 定义响应数据结构体, 从全局缓存中填充 */
    solar_energy_resp_t resp;

    resp.daily_generation_kwh   = g_solar_energy.daily_generation_kwh;
    resp.monthly_generation_kwh = g_solar_energy.monthly_generation_kwh;
    resp.annual_generation_kwh  = g_solar_energy.annual_generation_kwh;
    resp.total_generation_kwh   = g_solar_energy.total_generation_kwh;

    memcpy(resp.history_daily, g_solar_energy.history_daily, sizeof(resp.history_daily));
    memcpy(&resp.update_time, &g_solar_energy.update_time, sizeof(user_data_timestamp_t));

    /* 统一拷贝到帧数据缓冲区 */
    memcpy(g_frame_data_buf, &resp, sizeof(solar_energy_resp_t));

    /* 发送响应帧 (单帧, 无需分包) */
    tcp_send_frame(FRAME_TYPE_RESPONSE, CMD_GET_SOLAR_ENERGY,
                   g_frame_data_buf, sizeof(solar_energy_resp_t));

    printf("太阳能发电量: 日=%.3f, 月=%.3f, 年=%.3f, 总=%.3f kWh\r\n",
            resp.daily_generation_kwh,
            resp.monthly_generation_kwh,
            resp.annual_generation_kwh,
            resp.total_generation_kwh);
}

/* ================================================================
 *  备注信息管理 (CMD_SET_REMARK / CMD_GET_REMARK)
 * ================================================================ */

/**
 * @brief   将备注信息保存到SD卡 (内部函数)
 * @note    使用fs_mutex保护文件操作
 */
static void remark_info_save(void)
{
    FRESULT res;
    UINT bw;

    if(fs_mutex) osMutexAcquire(fs_mutex, osWaitForever);

    res = f_open(&SDFile, "0:/remark.txt", FA_CREATE_ALWAYS | FA_WRITE);
    if (res != FR_OK)
    {
        printf("备注信息: 打开文件失败 (%d)\r\n", res);
        if(fs_mutex) osMutexRelease(fs_mutex);
        return;
    }

    res = f_write(&SDFile, g_remark_buf, g_remark_len, &bw);
    f_close(&SDFile);

    if(fs_mutex) osMutexRelease(fs_mutex);

    if (res == FR_OK)
        printf("备注信息已保存到SD卡 (%d字节)\r\n", g_remark_len);
    else
        printf("备注信息: 写入失败 (%d)\r\n", res);
}

/**
 * @brief   开机时从SD卡加载备注信息到RAM
 * @note    在文件系统初始化完成后调用 (由lvgl_task调用)
 */
void remark_info_load(void)
{
    FRESULT res;
    UINT br;

    if(fs_mutex) osMutexAcquire(fs_mutex, osWaitForever);

    res = f_open(&SDFile, "0:/remark.txt", FA_READ);
    if (res != FR_OK)
    {
        /* 文件不存在, 备注为空 */
        g_remark_len = 0;
        printf("备注信息: 文件不存在, 使用空备注\r\n");
        if(fs_mutex) osMutexRelease(fs_mutex);
        return;
    }

    res = f_read(&SDFile, g_remark_buf, sizeof(g_remark_buf), &br);
    f_close(&SDFile);

    if(fs_mutex) osMutexRelease(fs_mutex);

    if (res == FR_OK)
    {
        g_remark_len = (uint16_t)br;
        printf("备注信息加载成功: %d字节\r\n", g_remark_len);
    }
    else
    {
        g_remark_len = 0;
        printf("备注信息: 读取失败 (%d)\r\n", res);
    }
}

/**
 * @brief   处理设置备注信息命令 (CMD_SET_REMARK)
 *
 * 请求数据域: 备注文本 (0~256字节, 空数据域表示清空备注)
 * 成功响应:   数据域=[0x00=成功]
 * 失败响应:   错误帧 数据域=[ERR_CODE]
 */
void tcp_resp_set_remark(const uint8_t *data, uint16_t len)
{
    /* 长度校验: 最大256字节 */
    if (len > 256)
    {
        tcp_send_error(ERR_PARAM_INVALID);
        printf("备注信息过长: %d字节 (最大256)\r\n", len);
        return;
    }

    /* 更新RAM缓冲区 */
    if (len > 0)
    {
        memcpy(g_remark_buf, data, len);
    }
    g_remark_len = len;

    /* 保存到SD卡 */
    remark_info_save();

    /* 成功响应 */
    uint8_t resp[1] = { 0x00 };
    tcp_send_frame(FRAME_TYPE_RESPONSE, CMD_SET_REMARK, resp, 1);

    printf("备注信息已更新: %d字节\r\n", g_remark_len);
}

/**
 * @brief   处理读取备注信息命令 (CMD_GET_REMARK)
 *
 * 请求数据域: 空
 * 响应数据域: 备注文本 (0~256字节, 长度=g_remark_len)
 */
void tcp_resp_get_remark(void)
{
    if (!g_tcp_connected)
        return;

    /* 将RAM中的备注信息作为响应数据域发送 */
    tcp_send_frame(FRAME_TYPE_RESPONSE, CMD_GET_REMARK, g_remark_buf, g_remark_len);

    printf("备注信息已发送: %d字节\r\n", g_remark_len);
}

/* ================================================================
 *  搜索结果推送 (CMD_SEARCH_RESULT)
 * ================================================================ */

/**
 * @brief   向上位机推送单个搜索到的设备信息 (CMD_SEARCH_RESULT)
 *
 * 由 ES1642_CMD_REPORT_SEARCH_RESULT 回调调用, 每搜到一个设备推送一帧。
 * 帧类型=RESPONSE, 数据域=[MAC(6)][ADDR(6)][net_state(1)]=13字节
 */
void tcp_send_search_result(const uint8_t mac[6], const uint8_t addr[6], uint8_t net_state)
{
    if (!g_tcp_connected)
        return;

    /* 数据域: MAC(6) + ADDR(6) + net_state(1) = 13字节 */
    uint8_t data[13];
    memcpy(&data[0], mac, 6);
    memcpy(&data[6], addr, 6);
    data[12] = net_state;

    tcp_send_frame(FRAME_TYPE_RESPONSE, CMD_SEARCH_RESULT, data, sizeof(data));

    printf("TCP推送搜索结果: MAC=%02X:%02X:%02X:%02X:%02X:%02X, net=%d\r\n",
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5], net_state);
}

/* ================================================================
 *  楼栋号管理 (CMD_SET_BUILDING / CMD_GET_HOST_INFO)
 * ================================================================ */

/**
 * @brief   将楼栋号保存到SD卡 (内部函数)
 */
static void building_no_save(void)
{
    FRESULT res;
    UINT bw;

    if(fs_mutex) osMutexAcquire(fs_mutex, osWaitForever);

    res = f_open(&SDFile, "0:/building.bin", FA_CREATE_ALWAYS | FA_WRITE);
    if (res != FR_OK)
    {
        printf("楼栋号: 打开文件失败 (%d)\r\n", res);
        if(fs_mutex) osMutexRelease(fs_mutex);
        return;
    }

    res = f_write(&SDFile, &g_building_no, 1, &bw);
    f_close(&SDFile);

    if(fs_mutex) osMutexRelease(fs_mutex);

    printf("楼栋号已保存: %d\r\n", g_building_no);
}

/**
 * @brief   开机时从SD卡加载楼栋号到RAM
 * @note    在文件系统初始化完成后调用
 */
void building_no_load(void)
{
    FRESULT res;
    UINT br;

    if(fs_mutex) osMutexAcquire(fs_mutex, osWaitForever);

    res = f_open(&SDFile, "0:/building.bin", FA_READ);
    if (res != FR_OK)
    {
        g_building_no = 0;
        printf("楼栋号: 文件不存在, 默认为0\r\n");
        if(fs_mutex) osMutexRelease(fs_mutex);
        return;
    }

    res = f_read(&SDFile, &g_building_no, 1, &br);
    f_close(&SDFile);

    if(fs_mutex) osMutexRelease(fs_mutex);

    printf("楼栋号加载成功: %d\r\n", g_building_no);
}

/**
 * @brief   处理设置楼栋号命令 (CMD_SET_BUILDING)
 *
 * 请求数据域: [楼栋号] (1字节)
 * 成功响应:   数据域=[0x00=成功]
 */
void tcp_resp_set_building(const uint8_t *data, uint16_t len)
{
    if (len < 1)
    {
        tcp_send_error(ERR_PARAM_INVALID);
        return;
    }

    g_building_no = data[0];
    building_no_save();

    uint8_t resp[1] = { 0x00 };
    tcp_send_frame(FRAME_TYPE_RESPONSE, CMD_SET_BUILDING, resp, 1);

    printf("楼栋号已设置: %d\r\n", g_building_no);
}

/**
 * @brief   处理读取主机信息命令 (CMD_GET_HOST_INFO)
 *
 * 请求数据域: 空
 * 响应数据域: [MAC(6)][楼栋号(1)] = 7字节
 */
void tcp_resp_get_host_info(void)
{
    if (!g_tcp_connected)
        return;

    /* 数据域: MAC(6) + 楼栋号(1) = 7字节 */
    uint8_t data[7];
    memcpy(&data[0], g_host_mac, 6);
    data[6] = g_building_no;

    tcp_send_frame(FRAME_TYPE_RESPONSE, CMD_GET_HOST_INFO, data, sizeof(data));

    printf("主机信息已发送: MAC=%02X:%02X:%02X:%02X:%02X:%02X, 楼栋=%d\r\n",
           g_host_mac[0], g_host_mac[1], g_host_mac[2],
           g_host_mac[3], g_host_mac[4], g_host_mac[5], g_building_no);
}
