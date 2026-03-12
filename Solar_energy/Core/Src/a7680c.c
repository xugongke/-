#include "a7680c.h"

/*=============================
变量定义
=============================*/

/* 接收数据缓冲区 */
uint8_t a7680c_rx_buf[A7680C_RX_BUF_SIZE];

/* 接收到的数据长度 */
uint16_t a7680c_rx_len = 0;

/* DMA接收缓冲区 */
static uint8_t dma_rx_buf[A7680C_RX_BUF_SIZE];


/**
 * @brief  A7680C初始化
 * @note   启动DMA接收
 */
void A7680C_Init(void)
{
    A7680C_UART_DMA_Start();
}


/**
 * @brief  启动UART DMA接收
 */
void A7680C_UART_DMA_Start(void)
{
    /* 启动DMA接收 */
    HAL_UART_Receive_DMA(&huart3, dma_rx_buf, A7680C_RX_BUF_SIZE);

    /* 使能IDLE中断 */
    __HAL_UART_ENABLE_IT(&huart3, UART_IT_IDLE);
}


/**
 * @brief  发送数据到A7680C
 */
void A7680C_Send(char *data)
{
    HAL_UART_Transmit_DMA(&huart3, (uint8_t*)data, strlen(data));
}


/**
 * @brief  USART IDLE中断处理
 * @note   用于判断一帧数据接收完成
 */
void A7680C_IDLE_IRQHandler(void)
{
    /* 判断是否发生IDLE中断 */
    if(__HAL_UART_GET_FLAG(&huart3, UART_FLAG_IDLE))
    {
        /* 清除IDLE标志 */
        __HAL_UART_CLEAR_IDLEFLAG(&huart3);

        /* 停止DMA接收 */
        HAL_UART_DMAStop(&huart3);

        /* 计算当前接收到的数据长度 */
        a7680c_rx_len = A7680C_RX_BUF_SIZE - __HAL_DMA_GET_COUNTER(huart3.hdmarx);

        /* 将DMA缓冲区数据复制到接收缓冲区 */
        memcpy(a7680c_rx_buf, dma_rx_buf, a7680c_rx_len);

        /* 清空DMA缓冲区 */
        memset(dma_rx_buf, 0, A7680C_RX_BUF_SIZE);

        /* 重新启动DMA接收 */
        HAL_UART_Receive_DMA(&huart3, dma_rx_buf, A7680C_RX_BUF_SIZE);
    }
}
