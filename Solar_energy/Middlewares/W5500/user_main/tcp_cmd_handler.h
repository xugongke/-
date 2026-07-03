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
#define CMD_DEVICE_MANAGE_MODE 0x02  /* 设备管理模式 (进入/退出) */
#define CMD_GET_DEVICE_LIST  0x03  /* 读取设备列表 */
#define CMD_BIND_DEVICE      0x04  /* 绑定设备 */
#define CMD_START_SEARCH     0x05  /* 开始搜索设备 */
#define CMD_STOP_SEARCH      0x06  /* 停止搜索设备 */
#define CMD_GET_USER_DATA    0x07  /* 读取用户用电量数据 */
#define CMD_GET_SOLAR_ENERGY 0x08  /* 读取太阳能板发电量 */
#define CMD_SET_REMARK       0x09  /* 设置备注信息 */
#define CMD_GET_REMARK       0x0A  /* 读取备注信息 */
#define CMD_SEARCH_RESULT    0x0B  /* 搜索结果推送 (单片机→上位机) */
#define CMD_SET_BUILDING     0x0C  /* 设置楼栋号 */
#define CMD_GET_HOST_INFO    0x0D  /* 读取主机信息(MAC+楼栋号) */

/* ==================== 错误码 ==================== */
#define ERR_INVALID_CMD      0x01  /* 无效命令 */
#define ERR_PARAM_INVALID    0x02  /* 参数错误 */
#define ERR_DEVICE_NOT_FOUND 0x03  /* 设备未找到 */
#define ERR_ES1642_DAMAGED   0x04  /* 单片机串口损坏或者从机模块损坏 */
#define ERR_ADDR_TIMEOUT     0x05  /* 修改地址超时 */
#define ERR_NET_TIMEOUT      0x06  /* 入网超时 */
#define ERR_NET_FAILED       0x07  /* 入网失败 */
#define ERR_SEND_FAILED      0x08  /* 发送失败 */

/* ==================== 分包配置 ==================== */
#define DEVICE_PER_PACKET    24    /* 读取设备列表每包最大设备数 (512-5)/sizeof(device_t=21)≈24 */
#define USER_DATA_PER_PACKET 10    /* 读取从机用电量每包最大用户数据数 (512-5)/50≈10) */

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
 * @brief   处理设备管理模式命令 (CMD_DEVICE_MANAGE_MODE)
 * @param   data: 数据域 (1字节: 0x01=进入管理模式, 0x00=退出管理模式)
 * @param   len:  数据域长度 (应为1)
 * @note    进入管理模式后, DevicePoll_Task 的从机轮询暂停;
 *          退出后恢复。TCP断开时自动退出管理模式。
 */
void tcp_resp_device_manage_mode(const uint8_t *data, uint16_t len);

/**
 * @brief   处理开始搜索设备命令 (CMD_START_SEARCH)
 */
void tcp_resp_start_search(void);
/**
 * @brief   处理开始搜索设备命令 (成功)
 */
void tcp_resp_start_search_ok(void);

/**
 * @brief   处理停止搜索设备命令 (CMD_STOP_SEARCH)
 */
void tcp_resp_stop_search(void);

/**
 * @brief   处理停止搜索设备命令 (成功)
 */
void tcp_resp_stop_search_ok(void);

/**
 * @brief   分包发送用户用电量数据 (响应CMD_GET_USER_DATA)
 */
void tcp_resp_user_data(void);

/**
 * @brief   发送太阳能板发电量数据 (响应CMD_GET_SOLAR_ENERGY)
 */
void tcp_resp_solar_energy(void);

/**
 * @brief   处理设置备注信息命令 (CMD_SET_REMARK)
 * @param   data: 数据域 (备注信息文本, 最多256字节)
 * @param   len:  数据域长度
 * @note    保存到SD卡并更新RAM缓冲区
 */
void tcp_resp_set_remark(const uint8_t *data, uint16_t len);

/**
 * @brief   处理读取备注信息命令 (CMD_GET_REMARK)
 * @note    将RAM中的备注信息打包成响应帧发送给上位机
 */
void tcp_resp_get_remark(void);

/**
 * @brief   开机时从SD卡加载备注信息到RAM
 * @note    在文件系统初始化完成后调用
 */
void remark_info_load(void);

/**
 * @brief   向上位机推送单个搜索到的设备信息 (CMD_SEARCH_RESULT)
 * @param   mac:       MAC地址(6字节)
 * @param   addr:      通信地址(6字节)
 * @param   net_state: 网络状态 (0=未入网, 1=已入网)
 * @note    由 ES1642_CMD_REPORT_SEARCH_RESULT 回调调用, 每搜到一个设备推送一帧
 *          帧类型=RESPONSE, 数据域=[MAC(6)][ADDR(6)][net_state(1)]=13字节
 */
void tcp_send_search_result(const uint8_t mac[6], const uint8_t addr[6], uint8_t net_state);

/**
 * @brief   处理设置楼栋号命令 (CMD_SET_BUILDING)
 * @param   data: 数据域 (1字节楼栋号)
 * @param   len:  数据域长度
 * @note    保存到SD卡, 开机时自动加载
 */
void tcp_resp_set_building(const uint8_t *data, uint16_t len);

/**
 * @brief   处理读取主机信息命令 (CMD_GET_HOST_INFO)
 * @note    返回主机MAC地址(6B)+楼栋号(1B)=7字节
 */
void tcp_resp_get_host_info(void);

/**
 * @brief   开机时从SD卡加载楼栋号到RAM
 * @note    在文件系统初始化完成后调用
 */
void building_no_load(void);

#endif /* __TCP_CMD_HANDLER_H */

