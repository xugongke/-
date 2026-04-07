#ifndef __RS485_USART_H
#define __RS485_USART_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f4xx_hal.h"

/* ===================== 接收缓冲区宏定义 ===================== */
#define RS485_RX_BUFFER_SIZE    256  

/* ===================== 接收缓冲区声明 ===================== */
extern uint8_t USART6_RX_BUF[RS485_RX_BUFFER_SIZE];
extern uint8_t UART4_RX_BUF[RS485_RX_BUFFER_SIZE];
extern uint8_t UART7_RX_BUF[RS485_RX_BUFFER_SIZE];
extern uint8_t UART8_RX_BUF[RS485_RX_BUFFER_SIZE];

/* ===================== 接收长度计数（中断用） ===================== */
extern uint16_t USART6_RX_CNT;
extern uint16_t UART4_RX_CNT;
extern uint16_t UART7_RX_CNT;
extern uint16_t UART8_RX_CNT;

/* ===================== 初始化函数声明 ===================== */
void RS485_USART6_Init(void);
void RS485_UART4_Init(void);
void RS485_UART7_Init(void);
void RS485_UART8_Init(void);

#ifdef __cplusplus
}
#endif

#endif

