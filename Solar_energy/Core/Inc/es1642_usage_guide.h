/**
 * @file    es1642_usage_guide.h
 * @brief   ES1642模块使用指南 - DMA+串口空闲中断实现示例
 * @author  Cline
 * @date    2026-03-16
 * 
 * 本文件提供ES1642模块的完整使用说明，包括：
 * 1. 模块初始化和配置
 * 2. DMA+串口空闲中断的数据收发实现
 * 3. 常用功能示例代码
 * 4. 错误处理和调试建议
 */

#ifndef ES1642_USAGE_GUIDE_H
#define ES1642_USAGE_GUIDE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "es1642.h"
#include "usart.h"
#include <stdio.h>
/* ======== 步骤1：定义全局变量和缓冲区 ======== */

/* ES1642驱动句柄，用来各种回调函数的指针 */
extern es1642_handle_t g_es1642_handle;

/* 串口接收缓冲区（DMA使用） */
#define ES1642_RX_BUF_SIZE  (ES1642_MAX_FRAME_LEN * 2)  /* 建议留有余量 */
extern uint8_t g_es1642_rx_buf[ES1642_MAX_FRAME_LEN];

/* 串口句柄（假设使用huart1） */
extern UART_HandleTypeDef huart1;

extern osSemaphoreId_t ES1642_mutexHandle;

extern osSemaphoreId_t ES1642_sendHandle;

/* ======== 步骤2：实现串口发送回调函数 ======== */

/**
 * @brief  串口发送回调函数
 * @param  data: 要发送的数据指针
 * @param  len: 数据长度
 * @param  user_arg: 用户参数（未使用）
 * @retval 发送的字节数（成功）或负数（失败）
 * @note   此函数由ES1642驱动内部调用，实现通过DMA发送数据
 */
static int32_t es1642_uart_write(const uint8_t *data, uint16_t len, void *user_arg);

/* ======== 步骤3：实现帧接收回调函数 ======== */

/**
 * @brief  帧接收完成回调函数
 * @param  handle: ES1642驱动句柄
 * @param  frame: 接收到的帧结构体
 * @param  user_arg: 用户参数（未使用）
 * @retval None
 * @note   当接收到完整帧时，驱动会调用此函数
 */
void es1642_on_frame_received(es1642_handle_t *handle, 
                                     const es1642_frame_t *frame, 
                                     void *user_arg);

/* ======== 步骤4：实现错误回调函数 ======== */

/**
 * @brief  错误回调函数
 * @param  handle: ES1642驱动句柄
 * @param  status: 错误状态码
 * @param  user_arg: 用户参数（未使用）
 * @retval None
 * @note   当发生错误时，驱动会调用此函数
 */
void es1642_on_error(es1642_handle_t *handle, 
                           es1642_status_t status, 
                           void *user_arg);


/* ======== 步骤6：实现模块初始化函数 ======== */

/**
 * @brief  初始化ES1642模块
 * @retval 0: 成功, -1: 失败
 */
int ES1642_InitModule(void);

/* ======== 步骤7：常用功能示例函数 ======== */

/**
 * @brief  读取模块版本信息
 * @retval 0: 成功, -1: 失败
 */
int ES1642_ReadVersion(void);

/**
 * @brief  读取模块MAC地址
 * @retval 0: 成功, -1: 失败
 */
int ES1642_ReadMac(void);

/**
 * @brief  读取模块地址
 * @retval 0: 成功, -1: 失败
 */
int ES1642_ReadAddr(void);

/**
 * @brief  发送数据到指定设备
 * @param  dst_addr: 目标地址（6字节）
 * @param  data: 要发送的数据
 * @param  len: 数据长度
 * @param  relay_depth: 中继深度（0表示自动）
 * @retval 0: 成功, -1: 失败
 */
int ES1642_SendUserData(const uint8_t dst_addr[ES1642_ADDR_LEN], 
                        const uint8_t *data, 
                        uint16_t len,
                        uint8_t relay_depth);

/**
 * @brief  发送广播数据
 * @param  data: 要发送的数据
 * @param  len: 数据长度
 * @retval 0: 成功, -1: 失败
 */
int ES1642_SendBroadcastData(const uint8_t *data, uint16_t len);

/**
 * @brief  启动设备搜索
 * @param  depth: 搜索深度（0=自动，1-15为具体深度）
 * @param  rule: 搜索规则
 * @retval 0: 成功, -1: 失败
 */
int ES1642_StartSearch(uint8_t depth, es1642_search_rule_t rule);

/**
 * @brief  停止设备搜索
 * @retval 0: 成功, -1: 失败
 */
int ES1642_StopSearch(void);

/**
 * @brief  设置模块地址
 * @param  addr: 要设置的地址（6字节）
 * @retval 0: 成功, -1: 失败
 */
int ES1642_SetModuleAddr(const uint8_t addr[ES1642_ADDR_LEN]);

/**
 * @brief  读取网络参数
 * @retval 0: 成功, -1: 失败
 */
int ES1642_ReadNetParam(void);

int ES1642_SendSearch(const uint8_t src_addr[ES1642_ADDR_LEN],
                          uint8_t task_id,
                          bool participate,
                          const uint8_t *attribute,
                          uint8_t attribute_len);
/**
 * @brief  设置PSK
 * @param  dst_addr: 目标地址（6字节）
 * @param  new_psk: 新的PSK指针（8字节）
 * @retval 0: 成功, -1: 失败
 */
int ES1642_SetPsk(const uint8_t dst_addr[ES1642_ADDR_LEN],
                 const uint8_t new_psk[ES1642_SET_PSK_LEN]);
													

#ifdef __cplusplus
}
#endif

#endif /* ES1642_USAGE_GUIDE_H */
