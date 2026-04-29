#include "rs485_usart.h"
#include "main.h"
#include "usart.h"
#include <stdio.h>
#include <string.h>

#include "cmsis_os.h"
#include "host_comm.h"
#include "es1642_usage_guide.h"

/* ===================== 接收缓冲区定义 ===================== */
__attribute__((section("RW_IRAM1")))// 放到普通 SRAM（0x20000000）
uint8_t USART1_RX_BUF[RS485_RX_BUFFER_SIZE] = {0};
__attribute__((section("RW_IRAM1")))// 放到普通 SRAM（0x20000000）
uint8_t USART6_RX_BUF[RS485_RX_BUFFER_SIZE] = {0};
__attribute__((section("RW_IRAM1")))// 放到普通 SRAM（0x20000000）
uint8_t UART4_RX_BUF[RS485_RX_BUFFER_SIZE] = {0};
__attribute__((section("RW_IRAM1")))// 放到普通 SRAM（0x20000000）
uint8_t UART7_RX_BUF[RS485_RX_BUFFER_SIZE] = {0};
__attribute__((section("RW_IRAM1")))// 放到普通 SRAM（0x20000000）
uint8_t UART8_RX_BUF[RS485_RX_BUFFER_SIZE] = {0};

/* ===================== 数据结构体实例定义 ===================== */
RS485_UART_Data_t USART1_Data = {0};
RS485_UART_Data_t USART6_Data = {0};
RS485_UART_Data_t UART4_Data = {0};
RS485_UART_Data_t UART7_Data = {0};
RS485_UART_Data_t UART8_Data = {0};

/* ===================== 队列句柄定义 ===================== */
QueueHandle_t xRS485_UART_Queue = NULL;

/* ===================== 初始化所有串口和队列 ===================== */
void RS485_USART_Init_All(void)
{
    // 创建队列用于传递串口数据指针
    xRS485_UART_Queue = xQueueCreate(RS485_UART_QUEUE_SIZE, sizeof(RS485_UART_Data_t*));
    
    if (xRS485_UART_Queue == NULL)
    {
        printf("RS485_UART_Queue 创建失败！\r\n");
        return;
    }
    
    printf("RS485_UART_Queue 创建成功，深度: %d\r\n", RS485_UART_QUEUE_SIZE);
    
    // 初始化各个串口
    RS485_USART1_Init();
    RS485_USART6_Init();
    RS485_UART4_Init();
    RS485_UART7_Init();
    RS485_UART8_Init();
}

/* ===================== USART1 DMA+空闲中断初始化 ===================== */
void RS485_USART1_Init(void)
{
    // 初始化数据结构体
    USART1_Data.huart = &huart1;
    USART1_Data.rx_buffer = USART1_RX_BUF;
    USART1_Data.rx_length = 0;
    
    // RS485设置为接收模式
    HAL_GPIO_WritePin(RS485_CTRL2_GPIO_Port, RS485_CTRL2_Pin, GPIO_PIN_RESET);
    
    // 启动DMA+空闲中断接收
    if (HAL_UARTEx_ReceiveToIdle_DMA(&huart1, USART1_RX_BUF, RS485_RX_BUFFER_SIZE) != HAL_OK)
    {
        printf("USART1 DMA接收启动失败\r\n");
        return;
    }
		__HAL_DMA_DISABLE_IT(huart1.hdmarx, DMA_IT_HT); // 关闭半传中断，避免重复回调
    
}

/* ===================== USART6 DMA+空闲中断初始化 ===================== */
void RS485_USART6_Init(void)
{
    // 初始化数据结构体
    USART6_Data.huart = &huart6;
    USART6_Data.rx_buffer = USART6_RX_BUF;
    USART6_Data.rx_length = 0;
    
    // RS485设置为接收模式
    HAL_GPIO_WritePin(RS485_CTRL1_GPIO_Port, RS485_CTRL1_Pin, GPIO_PIN_RESET);
    
    // 启动DMA+空闲中断接收
    if (HAL_UARTEx_ReceiveToIdle_DMA(&huart6, USART6_RX_BUF, RS485_RX_BUFFER_SIZE) != HAL_OK)
    {
        printf("USART6 DMA接收启动失败\r\n");
        return;
    }
    __HAL_DMA_DISABLE_IT(huart6.hdmarx, DMA_IT_HT); // 关闭半传中断，避免重复回调
}

/* ===================== UART4 DMA+空闲中断初始化 ===================== */
void RS485_UART4_Init(void)
{
    // 初始化数据结构体
    UART4_Data.huart = &huart4;
    UART4_Data.rx_buffer = UART4_RX_BUF;
    UART4_Data.rx_length = 0;
    
    // RS485设置为接收模式
    HAL_GPIO_WritePin(RS485_CTRL4_GPIO_Port, RS485_CTRL4_Pin, GPIO_PIN_RESET);

    // 启动DMA+空闲中断接收
    if (HAL_UARTEx_ReceiveToIdle_DMA(&huart4, UART4_RX_BUF, RS485_RX_BUFFER_SIZE) != HAL_OK)
    {
        printf("UART4 DMA接收启动失败\r\n");
        return;
    }
    __HAL_DMA_DISABLE_IT(huart4.hdmarx, DMA_IT_HT); // 关闭半传中断，避免重复回调
}

/* ===================== UART7 DMA+空闲中断初始化 ===================== */
void RS485_UART7_Init(void)
{
    // 初始化数据结构体
    UART7_Data.huart = &huart7;
    UART7_Data.rx_buffer = UART7_RX_BUF;
    UART7_Data.rx_length = 0;
    
    // RS485设置为接收模式
    HAL_GPIO_WritePin(RS485_CTRL5_GPIO_Port, RS485_CTRL5_Pin, GPIO_PIN_RESET);

    // 启动DMA+空闲中断接收
    if (HAL_UARTEx_ReceiveToIdle_DMA(&huart7, UART7_RX_BUF, RS485_RX_BUFFER_SIZE) != HAL_OK)
    {
        printf("UART7 DMA接收启动失败\r\n");
        return;
    }
    __HAL_DMA_DISABLE_IT(huart7.hdmarx, DMA_IT_HT); // 关闭半传中断，避免重复回调
}

/* ===================== UART8 DMA+空闲中断初始化 ===================== */
void RS485_UART8_Init(void)
{
    // 初始化数据结构体
    UART8_Data.huart = &huart8;
    UART8_Data.rx_buffer = UART8_RX_BUF;
    UART8_Data.rx_length = 0;
    
    // RS485设置为接收模式
    HAL_GPIO_WritePin(RS485_CTRL3_GPIO_Port, RS485_CTRL3_Pin, GPIO_PIN_RESET);

    // 启动DMA+空闲中断接收
    if (HAL_UARTEx_ReceiveToIdle_DMA(&huart8, UART8_RX_BUF, RS485_RX_BUFFER_SIZE) != HAL_OK)
    {
        printf("UART8 DMA接收启动失败\r\n");
        return;
    }
    __HAL_DMA_DISABLE_IT(huart8.hdmarx, DMA_IT_HT); // 关闭半传中断，避免重复回调
}




/* ===================== 数据处理函数 ===================== */
void Process_USART1_Data(uint8_t *data, uint16_t len)
{
    printf("========== USART1 收到数据 (长度: %d) ==========\r\n", len);
    
    // 打印接收到的数据（十六进制格式）
    for (uint16_t i = 0; i < len; i++)
    {
        printf("%02X ", data[i]);
        if ((i + 1) % 16 == 0) printf("\r\n");
    }
    printf("\r\n");
    
    // TODO: 在这里添加USART1的数据解析逻辑
    // 例如：Modbus协议解析、自定义协议解析等
}

void Process_USART6_Data(uint8_t *data, uint16_t len)
{
    printf("========== USART6 收到数据 (长度: %d) ==========\r\n", len);
    /* 命令解析 - 与TCP命令格式保持一致 */

    if (strncmp((char *)data, "LIST", 4) == 0)
    {
        /* 读取设备列表 */
        rs485_send_device_list();
        return;
    }

    if (strncmp((char *)data, "BIND,", 5) == 0)
    {
        /* 修改通信地址并入网 */
        rs485_handle_bind_cmd((const char *)data);
        return;
    }

    if (strncmp((char *)data, "SEARCH_START", 12) == 0)
    {
        /* 启动设备搜索 */
        rs485_set_search_active();
        int ret = ES1642_StartSearch(0, ES1642_SEARCH_RULE_ALL);  /* depth=0(自动), rule=搜索所有设备 */
				if(ret != 0)
				{
					rs485_usart6_send((const uint8_t *)"给模块发送启动搜索命令失败\n", strlen("给模块发送启动搜索命令失败\n"));
				}
        /* OK在es1642_on_frame_received中收到ES1642响应后发送 */
        return;
    }

    if (strncmp((char *)data, "SEARCH_STOP", 11) == 0)
    {
        /* 停止设备搜索 */
        int ret = ES1642_StopSearch();
				if(ret != 0)
				{
					rs485_usart6_send((const uint8_t *)"给模块发送停止搜索命令失败\n", strlen("给模块发送停止搜索命令失败\n"));
				}
        /* SEARCH_DONE在es1642_on_frame_received中收到ES1642响应后发送 */
        return;
    }

    /* 未知命令 */
    rs485_usart6_send((const uint8_t *)"ERR,未知命令\n", strlen("ERR,未知命令\n"));
}

void Process_UART4_Data(uint8_t *data, uint16_t len)
{
    printf("========== UART4 收到数据 (长度: %d) ==========\r\n", len);
    
    for (uint16_t i = 0; i < len; i++)
    {
        printf("%02X ", data[i]);
        if ((i + 1) % 16 == 0) printf("\r\n");
    }
    printf("\r\n");
    
    // TODO: 在这里添加UART4的数据解析逻辑
}

void Process_UART7_Data(uint8_t *data, uint16_t len)
{
    printf("========== UART7 收到数据 (长度: %d) ==========\r\n", len);
    
    for (uint16_t i = 0; i < len; i++)
    {
        printf("%02X ", data[i]);
        if ((i + 1) % 16 == 0) printf("\r\n");
    }
    printf("\r\n");
    
    // TODO: 在这里添加UART7的数据解析逻辑
}

void Process_UART8_Data(uint8_t *data, uint16_t len)
{
    printf("========== UART8 收到数据 (长度: %d) ==========\r\n", len);
    
    for (uint16_t i = 0; i < len; i++)
    {
        printf("%02X ", data[i]);
        if ((i + 1) % 16 == 0) printf("\r\n");
    }
    printf("\r\n");
    
    // TODO: 在这里添加UART8的数据解析逻辑
}

/* ===================== 多串口数据处理任务（核心！） ===================== */
void RS485_UART_ProcessTask(void *pvParameters)
{
    RS485_UART_Data_t *p_uart_data;
    
    printf("RS485_UART_ProcessTask 启动成功！\r\n");
    printf("等待串口数据...\r\n\r\n");
    
    for(;;)
    {
        // 从队列接收数据指针
        // 阻塞等待，portMAX_DELAY 表示无限等待
        if (xQueueReceive(xRS485_UART_Queue, &p_uart_data, portMAX_DELAY) == pdTRUE)
        {
            // 根据串口号分发到不同的处理函数
            if (p_uart_data->huart->Instance == USART1)
            {
                Process_USART1_Data(p_uart_data->rx_buffer, p_uart_data->rx_length);
            }
            else if (p_uart_data->huart->Instance == USART6)
            {
                Process_USART6_Data(p_uart_data->rx_buffer, p_uart_data->rx_length);
            }
            else if (p_uart_data->huart->Instance == UART4)
            {
                Process_UART4_Data(p_uart_data->rx_buffer, p_uart_data->rx_length);
            }
            else if (p_uart_data->huart->Instance == UART7)
            {
                Process_UART7_Data(p_uart_data->rx_buffer, p_uart_data->rx_length);
            }
            else if (p_uart_data->huart->Instance == UART8)
            {
                Process_UART8_Data(p_uart_data->rx_buffer, p_uart_data->rx_length);
            }
            else
            {
                printf("未知串口数据！\r\n");
            }
        }
    }
}
