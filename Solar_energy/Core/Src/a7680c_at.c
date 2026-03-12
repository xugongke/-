#include "a7680c_at.h"

/**
 * @brief  发送AT指令并等待返回
 * @param  cmd AT指令
 * @param  ack 期望返回字符串
 * @param  timeout 超时时间
 * @retval 1 成功
 *         0 失败
 */
uint8_t A7680C_SendAT(char *cmd,char *ack,uint32_t timeout)
{
    uint32_t tick = HAL_GetTick();

    /* 清空接收缓冲区 */
    memset(a7680c_rx_buf,0,A7680C_RX_BUF_SIZE);

    /* 发送AT指令 */
    A7680C_Send(cmd);

    /* 等待返回 */
    while(HAL_GetTick()-tick < timeout)
    {
        if(strstr((char*)a7680c_rx_buf,ack))
        {
						printf("%s\r\n",a7680c_rx_buf);
            return 1;
        }
    }
    return 0;
}


/* AT测试 */
uint8_t A7680C_Test(void)
{
    return A7680C_SendAT("AT\r\n","OK",1000);
}


/* 查询SIM卡 */
uint8_t A7680C_CheckSIM(void)
{
    return A7680C_SendAT("AT+CPIN?\r\n","READY",2000);
}


/* 查询信号 */
uint8_t A7680C_CheckSignal(void)
{
    return A7680C_SendAT("AT+CSQ\r\n","OK",2000);
}


/* 查询网络附着 */
uint8_t A7680C_CheckNetwork(void)
{
    return A7680C_SendAT("AT+CGATT?\r\n","1",3000);
}
/* 查询模块信息 */ 
uint8_t A7680C_SendCmdWaitAck(void)
{
    return A7680C_SendAT("ATI\r\n","OK",1000);
}
/* 启用命令回显 */ 
uint8_t A7680C_SendATE1(void)
{
    return A7680C_SendAT("ATE1\r\n","OK",1000);
}

