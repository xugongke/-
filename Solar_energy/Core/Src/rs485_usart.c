#include "rs485_usart.h"
#include "main.h"
#include "usart.h"
#include <stdio.h>  // 用于printf打印日志

/* ===================== 接收缓冲区定义 ===================== */
uint8_t USART6_RX_BUF[RS485_RX_BUFFER_SIZE] = {0};
uint8_t UART4_RX_BUF[RS485_RX_BUFFER_SIZE] = {0};
uint8_t UART7_RX_BUF[RS485_RX_BUFFER_SIZE] = {0};
uint8_t UART8_RX_BUF[RS485_RX_BUFFER_SIZE] = {0};

/* ===================== 接收长度计数 ===================== */
uint16_t USART6_RX_CNT = 0;
uint16_t UART4_RX_CNT = 0;
uint16_t UART7_RX_CNT = 0;
uint16_t UART8_RX_CNT = 0;

/* ===================== USART6 纯中断初始化 ===================== */
void RS485_USART6_Init(void)
{
    // RS485设置为接收模式（不变）
    HAL_GPIO_WritePin(RS485_CTRL1_GPIO_Port, RS485_CTRL1_Pin, GPIO_PIN_RESET);
    
    // 启动【纯单字节中断接收】（核心替换）
    if (HAL_UART_Receive_IT(&huart6, &USART6_RX_BUF[USART6_RX_CNT], 1) != HAL_OK)
    {
        printf("USART6 中断接收启动失败\r\n");
        return;
    }

    printf("USART6(RS485_CTRL1) 纯中断初始化成功\r\n");
}

/* ===================== UART4 纯中断初始化 ===================== */
void RS485_UART4_Init(void)
{
    HAL_GPIO_WritePin(RS485_CTRL4_GPIO_Port, RS485_CTRL4_Pin, GPIO_PIN_RESET);

    if (HAL_UART_Receive_IT(&huart4, &UART4_RX_BUF[UART4_RX_CNT], 1) != HAL_OK)
    {
        printf("UART4 中断接收启动失败\r\n");
        return;
    }

    printf("UART4(RS485_CTRL4) 纯中断初始化成功\r\n");
}

/* ===================== UART7 纯中断初始化 ===================== */
void RS485_UART7_Init(void)
{
    HAL_GPIO_WritePin(RS485_CTRL5_GPIO_Port, RS485_CTRL5_Pin, GPIO_PIN_RESET);

    if (HAL_UART_Receive_IT(&huart7, &UART7_RX_BUF[UART7_RX_CNT], 1) != HAL_OK)
    {
        printf("UART7 中断接收启动失败\r\n");
        return;
    }

    printf("UART7(RS485_CTRL5) 纯中断初始化成功\r\n");
}

/* ===================== UART8 纯中断初始化 ===================== */
void RS485_UART8_Init(void)
{
    HAL_GPIO_WritePin(RS485_CTRL3_GPIO_Port, RS485_CTRL3_Pin, GPIO_PIN_RESET);

    if (HAL_UART_Receive_IT(&huart8, &UART8_RX_BUF[UART8_RX_CNT], 1) != HAL_OK)
    {
        printf("UART8 中断接收启动失败\r\n");
        return;
    }

    printf("UART8(RS485_CTRL3) 纯中断初始化成功\r\n");
}

/* ===================== 串口中断接收回调函数（核心！） ===================== */
// 每收到1个字节，自动进入这个函数
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    /*---------- USART6 中断处理 ----------*/
    if(huart->Instance == USART6)
    {
				printf("USART6_RX_BUF[%d]:%#x\r\n",USART6_RX_CNT,USART6_RX_BUF[USART6_RX_CNT]);
        // 接收长度+1
        USART6_RX_CNT++;
        // 缓冲区满则清零（防止溢出）
        if(USART6_RX_CNT >= RS485_RX_BUFFER_SIZE)
        {
            USART6_RX_CNT = 0;
        }
        // 重新启动中断，接收下一个字节（必须写！实现连续接收）
        HAL_UART_Receive_IT(&huart6, &USART6_RX_BUF[USART6_RX_CNT], 1);
    }

    /*---------- UART4 中断处理 ----------*/
    if(huart->Instance == UART4)
    {
        UART4_RX_CNT++;
        if(UART4_RX_CNT >= RS485_RX_BUFFER_SIZE) UART4_RX_CNT = 0;
        HAL_UART_Receive_IT(&huart4, &UART4_RX_BUF[UART4_RX_CNT], 1);
    }

    /*---------- UART7 中断处理 ----------*/
    if(huart->Instance == UART7)
    {
        UART7_RX_CNT++;
        if(UART7_RX_CNT >= RS485_RX_BUFFER_SIZE) UART7_RX_CNT = 0;
        HAL_UART_Receive_IT(&huart7, &UART7_RX_BUF[UART7_RX_CNT], 1);
    }

    /*---------- UART8 中断处理 ----------*/
    if(huart->Instance == UART8)
    {
        UART8_RX_CNT++;
        if(UART8_RX_CNT >= RS485_RX_BUFFER_SIZE) UART8_RX_CNT = 0;
        HAL_UART_Receive_IT(&huart8, &UART8_RX_BUF[UART8_RX_CNT], 1);
    }
}


