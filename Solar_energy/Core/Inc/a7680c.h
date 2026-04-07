#ifndef __A7680C_H
#define __A7680C_H

#include "stm32f4xx_hal.h"
#include "main.h"
#include <string.h>
#include <stdio.h>

/*=============================
A7680C串口接收缓冲区大小
=============================*/
#define UART_RX_DMA_SIZE        256
#define AT_MSG_BUFFER_SIZE      4096
#define AT_PARSE_BUFFER_SIZE    2048


/*=============================
外部变量声明
=============================*/

/* USART3句柄（在usart.c中定义） */
extern UART_HandleTypeDef huart3;

/* A7680C接收缓冲区 */
extern uint8_t a7680c_rx_buf[UART_RX_DMA_SIZE];

extern uint8_t at_parse_buf[AT_PARSE_BUFFER_SIZE];

extern uint16_t at_index;//记录缓冲区下标

extern osSemaphoreId_t at_semHandle;//构建完整帧后释放这个信号量

extern osSemaphoreId_t at_mutexHandle;//保证同一时刻只运行一个send函数

typedef enum {
    AT_RESULT_NONE = 0,
    AT_RESULT_OK,
    AT_RESULT_ERROR,
    AT_RESULT_TIMEOUT
} at_result_t;

extern at_result_t at_result;
/*=============================
函数声明
=============================*/

/**
 * @brief  A7680C模块初始化
 * @note   启动DMA接收和IDLE中断
 */
void A7680C_Init(void);


/**
 * @brief  发送数据到A7680C模块
 * @param  data 要发送的字符串
 */
void A7680C_Send(char *data);


/**
 * @brief  USART3 IDLE中断处理函数
 * @note   在USART3中断函数中调用
 */
void A7680C_IDLE_IRQHandler(void);

void at_process_data(uint8_t *data, uint16_t len);

/* 发送AT并等待应答 */
uint8_t A7680C_SendAT(char *cmd, char *ack, uint32_t timeout,uint8_t* data);

#endif
