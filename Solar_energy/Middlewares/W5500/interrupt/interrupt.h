#ifndef __INTERRUPT_H__
#define __INTERRUPT_H__

#include "stdint.h"

/**
 * @brief   Determine the interrupt type and store the value in I STATUS
 * @param   none
 * @return  none
 */
void wizchip_ISR(void);

/**
 * @brief   TCP server interrupt mode loopback
 * @param   none
 * @return  none
 */
void loopback_tcps_interrupt(uint8_t sn, uint8_t *buf, uint16_t port);

/**
 * @brief   将搜索到的设备信息存入待发送队列（中断安全）
 * @param   mac: MAC地址(6字节)
 * @param   addr: 通信地址(6字节)
 * @param   valid: 入网状态(0=未入网,1=已入网)
 * @note    由 es1642_on_frame_received 的 REPORT_SEARCH_RESULT 分支调用
 *          实际TCP发送在 tcp_process_search_pending() 中完成
 */
void tcp_send_search_device(const uint8_t mac[6], const uint8_t addr[6], uint8_t valid);

/**
 * @brief   标记搜索完成，待发送队列中会追加SEARCH_DONE
 * @note    实际TCP发送在 tcp_process_search_pending() 中完成
 */
void tcp_send_search_done(void);

/**
 * @brief   设置当前TCP连接的socket编号（供搜索结果推送使用）
 * @param   sn: socket编号
 */
void tcp_set_search_socket(uint8_t sn);

/**
 * @brief   清除当前TCP连接的socket编号（断开时调用）
 */
void tcp_clear_search_socket(void);

/**
 * @brief   在W5500主循环中调用，处理待发送的搜索设备数据
 * @note    必须在 loopback_tcps_interrupt() 所在的同一任务中周期调用
 */
void tcp_process_search_pending(void);

#endif
