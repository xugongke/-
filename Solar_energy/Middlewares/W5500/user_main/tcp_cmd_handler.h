/**
 * @file    tcp_cmd_handler.h
 * @brief   TCP命令处理 - 帧协议命令分发与响应
 *
 * 帧类型定义:
 *   0x00 = 请求帧 (上位机 → 单片机)
 *   0x01 = 响应帧 (单片机 → 上位机)
 *   0x02 = 错误帧 (单片机 → 上位机)
 *
 * 命令字定义:
 *   0x03 = 读取设备列表
 *   0x04 = 绑定设备
 *   0x05 = 开始搜索设备
 *   0x06 = 停止搜索设备
 *   0x07 = 读取用户用电量数据
 *   0x08 = 读取太阳能板发电量
 */
#ifndef __TCP_CMD_HANDLER_H
#define __TCP_CMD_HANDLER_H

#include "stdint.h"

/* ==================== 帧类型 ==================== */
#define FRAME_TYPE_REQUEST  0x00   /* 请求帧: 上位机 → 单片机 */
#define FRAME_TYPE_RESPONSE 0x01   /* 响应帧: 单片机 → 上位机 */
#define FRAME_TYPE_ERROR    0x02   /* 错误帧: 单片机 → 上位机 */

/* ==================== 命令码 ==================== */
#define CMD_GET_DEVICE_LIST  0x03  /* 读取设备列表 */
#define CMD_BIND_DEVICE      0x04  /* 绑定设备 */
#define CMD_START_SEARCH     0x05  /* 开始搜索设备 */
#define CMD_STOP_SEARCH      0x06  /* 停止搜索设备 */
#define CMD_GET_USER_DATA    0x07  /* 读取用户用电量数据 */
#define CMD_GET_SOLAR_ENERGY 0x08  /* 读取太阳能板发电量 */

/* ==================== 错误码 ==================== */
#define ERR_INVALID_CMD      0x01  /* 无效命令 */
#define ERR_PARAM_INVALID    0x02  /* 参数错误 */
#define ERR_DEVICE_NOT_FOUND 0x03  /* 设备未找到 */
#define ERR_ES1642_DAMAGED   0x04  /* 从机模块损坏 */
#define ERR_ADDR_TIMEOUT     0x05  /* 修改地址超时 */
#define ERR_NET_TIMEOUT      0x06  /* 入网超时 */
#define ERR_NET_FAILED       0x07  /* 入网失败 */
#define ERR_SEND_FAILED      0x08  /* 发送失败 */

/* ==================== 分包配置 ==================== */
#define DEVICE_PER_PACKET    28    /* 读取设备列表每包最大设备数 (512/18≈28) */
#define USER_DATA_PER_PACKET 10    /* 每包最大用户数据数 (512-5)/50≈10) */

/**
 * @brief   处理接收到的完整帧
 * @param   type: 帧类型 (应为 FRAME_TYPE_REQUEST)
 * @param   cmd:  命令字
 * @param   data: 数据域指针
 * @param   len:  数据域长度
 */
void tcp_dispatch_frame(uint8_t type, uint8_t cmd, const uint8_t *data, uint16_t len);

/**
 * @brief   发送错误帧
 * @param   error_code: 错误码
 */
void tcp_send_error(uint8_t error_code);

/**
 * @brief   分包发送设备列表 (响应CMD_GET_DEVICE_LIST)
 */
void tcp_resp_device_list(void);

/**
 * @brief   处理绑定设备命令 (CMD_BIND_DEVICE)
 * @param   data: 数据域 (MAC[6] + 楼栋[1] + 单元[1] + 房号[2])
 * @param   len:  数据域长度 (应为10)
 */
void tcp_resp_bind_device(const uint8_t *data, uint16_t len);

/**
 * @brief   处理开始搜索设备命令 (CMD_START_SEARCH)
 */
void tcp_resp_start_search(void);

/**
 * @brief   处理停止搜索设备命令 (CMD_STOP_SEARCH)
 */
void tcp_resp_stop_search(void);

/**
 * @brief   分包发送用户用电量数据 (响应CMD_GET_USER_DATA)
 */
void tcp_resp_user_data(void);

/**
 * @brief   发送太阳能板发电量数据 (响应CMD_GET_SOLAR_ENERGY)
 */
void tcp_resp_solar_energy(void);

#endif /* __TCP_CMD_HANDLER_H */

