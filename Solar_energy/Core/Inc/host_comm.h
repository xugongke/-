/**
 * @file    host_comm.h
 * @brief   上位机通信接口 - TCP(W5500网口) 和 RS485(USART6串口) 统一管理
 *
 * 本文件集中管理所有与上位机通信相关的函数，包括：
 * 1. TCP版本上位机通信函数（通过W5500网口）
 * 2. RS485版本上位机通信函数（通过USART6串口）
 * 3. 共用的辅助函数（MAC/地址字符串转换等）
 */
#ifndef __HOST_COMM_H
#define __HOST_COMM_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stdint.h"

/* ===================== 共用辅助函数 ===================== */

/**
 * @brief  MAC地址转字符串 (格式: XX:XX:XX:XX:XX:XX)
 * @param  mac: MAC地址(6字节)
 * @param  out: 输出字符串缓冲区(至少18字节)
 */
void host_mac_to_string(const uint8_t mac[6], char *out);

/**
 * @brief  通信地址转字符串 (格式: XX:XX:XX:XX:XX:XX)
 * @param  addr: 通信地址(6字节)
 * @param  out: 输出字符串缓冲区(至少18字节)
 */
void host_addr_to_string(const uint8_t addr[6], char *out);

/**
 * @brief  解析MAC字符串 (格式: XX:XX:XX:XX:XX:XX)
 * @param  str: 输入字符串
 * @param  mac: 输出MAC地址(6字节)
 * @return 0:成功, -1:格式错误
 */
int host_parse_mac_string(const char *str, uint8_t mac[6]);

/* ===================== TCP上位机通信函数 (W5500网口) ===================== */

/**
 * @brief  通过TCP发送设备列表
 * @param  sn: socket编号
 */
void tcp_send_device_list(uint8_t sn);

/**
 * @brief  通过TCP处理绑定命令 (修改通信地址并入网)
 * @param  sn: socket编号
 * @param  cmd: 命令字符串 (格式: BIND,AA:BB:CC:DD:EE:FF,楼栋,单元,房号)
 */
void tcp_handle_bind_cmd(uint8_t sn, const char *cmd);

/**
 * @brief  设置当前TCP搜索用的socket编号
 * @param  sn: socket编号
 */
void tcp_set_search_socket(uint8_t sn);

/**
 * @brief  清除当前TCP搜索用的socket编号（断开时调用）
 */
void tcp_clear_search_socket(void);

/**
 * @brief  通过TCP推送搜索到的单个设备信息
 * @param  mac: MAC地址(6字节)
 * @param  addr: 通信地址(6字节)
 * @param  valid: 入网状态(0=未入网,1=已入网)
 */
void tcp_send_search_device(const uint8_t mac[6], const uint8_t addr[6], uint8_t valid);

/**
 * @brief  通过TCP发送搜索启动OK响应
 */
void tcp_send_search_ok(void);

/**
 * @brief  通过TCP发送搜索完成通知
 */
void tcp_send_search_done(void);

/* ===================== RS485上位机通信函数 (USART6串口) ===================== */

/**
 * @brief  通过USART6 RS485发送数据（自动控制收发方向）
 * @param  data: 数据指针
 * @param  len: 数据长度
 */
void rs485_usart6_send(const uint8_t *data, uint16_t len);

/**
 * @brief  通过RS485发送设备列表
 */
void rs485_send_device_list(void);

/**
 * @brief  通过RS485处理绑定命令 (修改通信地址并入网)
 * @param  cmd: 命令字符串 (格式: BIND,AA:BB:CC:DD:EE:FF,楼栋,单元,房号)
 */
void rs485_handle_bind_cmd(const char *cmd);

/**
 * @brief  设置RS485搜索活跃标志
 */
void rs485_set_search_active(void);

/**
 * @brief  清除RS485搜索活跃标志
 */
void rs485_clear_search_active(void);

/**
 * @brief  通过RS485推送搜索到的单个设备信息
 * @param  mac: MAC地址(6字节)
 * @param  addr: 通信地址(6字节)
 * @param  valid: 入网状态(0=未入网,1=已入网)
 */
void rs485_send_search_device(const uint8_t mac[6], const uint8_t addr[6], uint8_t valid);

/**
 * @brief  通过RS485发送搜索启动OK响应
 */
void rs485_send_search_ok(void);

/**
 * @brief  通过RS485发送搜索完成通知
 */
void rs485_send_search_done(void);

#ifdef __cplusplus
}
#endif

#endif /* __HOST_COMM_H */
