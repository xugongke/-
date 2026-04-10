#ifndef __RS485_USART_H
#define __RS485_USART_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f4xx_hal.h"
#include "FreeRTOS.h"
#include "queue.h"

/* ===================== 接收缓冲区宏定义 ===================== */
#define RS485_RX_BUFFER_SIZE    256
#define RS485_UART_QUEUE_SIZE   20  // 队列深度，可根据实际情况调整

/* ===================== 串口数据结构体 ===================== */
typedef struct {
    UART_HandleTypeDef *huart;          // 串口句柄指针
    uint8_t *rx_buffer;                 // 接收缓冲区指针
    uint16_t rx_length;                 // 接收到的字节数
} RS485_UART_Data_t;

/* ===================== 接收缓冲区声明 ===================== */
extern uint8_t USART1_RX_BUF[RS485_RX_BUFFER_SIZE];
extern uint8_t USART6_RX_BUF[RS485_RX_BUFFER_SIZE];
extern uint8_t UART4_RX_BUF[RS485_RX_BUFFER_SIZE];
extern uint8_t UART7_RX_BUF[RS485_RX_BUFFER_SIZE];
extern uint8_t UART8_RX_BUF[RS485_RX_BUFFER_SIZE];

/* ===================== 数据结构体声明 ===================== */
extern RS485_UART_Data_t USART1_Data;
extern RS485_UART_Data_t USART6_Data;
extern RS485_UART_Data_t UART4_Data;
extern RS485_UART_Data_t UART7_Data;
extern RS485_UART_Data_t UART8_Data;

/* ===================== 队列句柄声明 ===================== */
extern QueueHandle_t xRS485_UART_Queue;

/* ===================== 初始化函数声明 ===================== */
void RS485_USART_Init_All(void);         // 初始化所有串口和队列
void RS485_USART1_Init(void);
void RS485_USART6_Init(void);
void RS485_UART4_Init(void);
void RS485_UART7_Init(void);
void RS485_UART8_Init(void);

#ifdef __cplusplus
}
#endif

#endif
