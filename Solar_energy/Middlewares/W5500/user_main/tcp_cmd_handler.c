/**
 * @file    tcp_cmd_handler.c
 * @brief   TCP命令处理 - 帧协议命令分发与响应实现
 */

#include "tcp_cmd_handler.h"
#include "user_main.h"
#include "device_manager.h"
#include "user_data_manager.h"
#include "mppt.h"
#include "host_comm.h"
#include "socket.h"
#include "cmsis_os.h"
#include <stdio.h>
#include <string.h>

/* ==================== 外部变量 ==================== */
extern volatile uint8_t g_tcp_connected;

/* ==================== 全局帧数据缓冲区 ==================== */
uint8_t g_frame_data_buf[FRAME_MAX_DATA_LEN];  /* 组帧数据缓冲区，供分包发送函数共用 */

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
        printf("Bind failed: ret=%d, err=0x%02X\r\n", ret, err);
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
    tcp_set_search_socket(TCP_SOCKET_ID);

    uint8_t resp[1] = { 0x00 };
    tcp_send_frame(FRAME_TYPE_RESPONSE, CMD_START_SEARCH, resp, 1);

    printf("Search started\r\n");
}

/**
 * @brief   处理停止搜索设备命令
 *
 * 请求数据域: [CMD=0x04]
 * 成功响应:   [CMD=0x04][0x00=成功]
 */
void tcp_resp_stop_search(void)
{
    tcp_clear_search_socket();

    uint8_t resp[1] = { 0x00 };
    tcp_send_frame(FRAME_TYPE_RESPONSE, CMD_STOP_SEARCH, resp, 1);

    printf("Search stopped\r\n");
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
 * 每个用户数据占 sizeof(user_detail_cache_t)=50 字节, 每包最多10个用户
 * 数据域最大 = 1(seq) + 2(total) + 2(count) + 10*50 = 505 ≤ 512
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

        /* 计算数据域长度: 1(seq) + 2(total_user) + 2(pack_count) + count*50 */
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
