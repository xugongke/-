#ifndef __A7680C_H
#define __A7680C_H

#include "stm32f4xx_hal.h"
#include <string.h>
#include <stdio.h>

/*=============================
A7680C串口接收缓冲区大小
=============================*/
#define A7680C_RX_BUF_SIZE 1024


/*=============================
外部变量声明
=============================*/

/* USART3句柄（在usart.c中定义） */
extern UART_HandleTypeDef huart3;

/* A7680C接收缓冲区 */
extern uint8_t a7680c_rx_buf[A7680C_RX_BUF_SIZE];

/* 当前接收到的数据长度 */
extern uint16_t a7680c_rx_len;


/*=============================
函数声明
=============================*/

/**
 * @brief  A7680C模块初始化
 * @note   启动DMA接收和IDLE中断
 */
void A7680C_Init(void);


/**
 * @brief  启动UART DMA接收
 */
void A7680C_UART_DMA_Start(void);


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

#endif
