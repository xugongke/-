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
 * @brief   向PC端发送搜索到的单个设备信息
 * @param   mac: MAC地址(6字节)
 * @param   addr: 通信地址(6字节)
 * @param   valid: 入网状态(0=未入网,1=已入网)
 */
void tcp_send_search_device(const uint8_t mac[6], const uint8_t addr[6], uint8_t valid);

/**
 * @brief   向PC端发送搜索完成通知
 */
void tcp_send_search_done(void);

/**
 * @brief   设置当前TCP连接的socket编号
 * @param   sn: socket编号
 */
void tcp_set_search_socket(uint8_t sn);

/**
 * @brief   清除当前TCP连接的socket编号（断开时调用）
 */
void tcp_clear_search_socket(void);

#endif
