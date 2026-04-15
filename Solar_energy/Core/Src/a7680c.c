#include "a7680c.h"
#include "usart.h"
/*=============================
变量定义
=============================*/
at_result_t at_result;
char *keyword = NULL;

/* DMA接收数据缓冲区 */
uint8_t a7680c_rx_buf[UART_RX_DMA_SIZE];

uint8_t at_parse_buf[AT_PARSE_BUFFER_SIZE];//流式拼接缓冲区，接收到的完整的4G数据存储在这里面
uint16_t at_index = 0;//记录缓冲区下标

/* 接收到的完整帧总长度 */
uint16_t a7680c_rx_len = 0;

//通知函数（接收任务调用）
void at_notify_result(at_result_t result)
{
    at_result = result;
    osSemaphoreRelease(at_semHandle);
}
/**
 * @brief  A7680C初始化
 * @note   启动DMA接收
 */
void A7680C_Init(void)
{
    /* 启动串口DMA+空闲中断接收 */
    if (HAL_UARTEx_ReceiveToIdle_DMA(&huart3, a7680c_rx_buf, sizeof(a7680c_rx_buf)) != HAL_OK)
    {
        printf("A7680C启动UART3 DMA接收失败\r\n");
        return;
    }
		__HAL_DMA_DISABLE_IT(huart3.hdmarx, DMA_IT_HT); // 关闭半传中断，避免重复回调
		
    
    printf("A7680C模块初始化成功\r\n");
}


/**
 * @brief  发送数据到A7680C
 */
void A7680C_Send(char *data)
{
    HAL_UART_Transmit_DMA(&huart3, (uint8_t*)data, strlen(data));
}

/**
 * @brief  发送AT指令并等待返回（多任务安全，互斥调用）
 * @param  cmd AT指令
 * @param  ack 期望返回字符串
 * @param  timeout 超时时间 ms
 * @retval 1 成功
 *         0 失败/超时
 */
uint8_t A7680C_SendAT(char *cmd, char *ack, uint32_t timeout,uint8_t* data)
{
    // ==============================================
    // 【第一步：互斥锁 —— 保证同一时刻只有一个人调用A7680C_Send】
    // ==============================================
    if (osSemaphoreAcquire(at_mutexHandle, osWaitForever) != osOK)
    {
        return 0; // 获取不到锁，直接退出（不会进入业务）
    }

    // 清空接收缓冲区
    at_index = 0;
    keyword = ack;  // 设置本次要等待的关键字

    // ==============================================
    // 【第二步：清空残留信号量】
    // 因为这个信号量是接收任务释放的，必须清空历史残留
    // ==============================================
    while (osSemaphoreAcquire(at_semHandle, 0) == osOK);

    // ==============================================
    // 【第三步：发送AT指令】
    // ==============================================
    A7680C_Send(cmd);

    // ==============================================
    // 【第四步：等待接收任务释放信号】
    // ==============================================
    uint8_t ret = 0;
    if (osSemaphoreAcquire(at_semHandle, pdMS_TO_TICKS(timeout)) == osOK)
    {
        // 收到响应
        ret = at_result;
				if(ret == AT_RESULT_ERROR)
				{
					printf("%s",at_parse_buf);
				}
				else
				{
					if(data != NULL)
					{//接收到的完整帧保存到了data中，不用担心at_parse_buf被覆盖导致数据丢失了
						memcpy(data, at_parse_buf, a7680c_rx_len + 1);//加一是因为最后还有个字符串结束符号
					}
				}
    }
    else
    {
        printf("超时未接收到响应\r\n");
        ret = AT_RESULT_TIMEOUT;
    }

    // ==============================================
    // 【第五步：释放互斥锁 —— 允许下一个任务调用】
    // ==============================================
    osSemaphoreRelease(at_mutexHandle);

    return ret;
}
/**
 * @brief  查找指定字符
 */
int at_find(const char *key)
{
    return (strstr((char *)at_parse_buf, key) != NULL);
}
/**
* @brief  流式拼接处理函数，现在只处理的单片机主动发送AT命令的情况，还没有处理云平台主动发送命令的情况
 */
void at_process_data(uint8_t *data, uint16_t len)
{
		if (data == NULL || len == 0) return;
	
    if (len >= AT_PARSE_BUFFER_SIZE) {
        len = AT_PARSE_BUFFER_SIZE - 1;
        at_index = 0;
    } else if (at_index + len >= AT_PARSE_BUFFER_SIZE) {
        at_index = 0;
    }

    memcpy(&at_parse_buf[at_index], data, len);
    at_index += len;
    at_parse_buf[at_index] = '\0';

    // 判断响应
    if (at_find(keyword))
    {
				a7680c_rx_len = at_index;
        at_notify_result(AT_RESULT_OK);
        at_index = 0;
    }
    else if (at_find("ERROR"))
    {
				a7680c_rx_len = at_index;
				at_notify_result(AT_RESULT_ERROR);
				at_index = 0;
    }
}

