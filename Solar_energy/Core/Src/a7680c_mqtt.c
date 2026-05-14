#include "a7680c_mqtt.h"


/**
 * @brief  启动MQTT EX服务
 * @note   对应官方流程: AT+CMQTTSTART=1.这里写1是因为初始化PDP时写的1
 * @retval 1:成功 0:失败
 */
uint8_t A7680C_MQTT_Start(void)
{
    /* AT+CMQTTSTART -> 等待 +CMQTTSTART:0 */
    return A7680C_SendAT("AT+CMQTTSTART=1\r\n", "OK", 5000, NULL);
}


/**
 * @brief  MQTT EX连接服务器
 * @note   官方流程:
 *         1. AT+CMQTTACCQ=0,"client_id",0   获取客户端
 *         2. AT+CMQTTCFG="argtopic",0,1,1    配置topic参数模式
 *         3. AT+CMQTTCONNECT=0,"tcp://host:port",timeout,1
 * @param  client: 客户端ID字符串
 * @param  user:   用户名(保留,未使用)
 * @param  pass:   密码(保留,未使用)
 * @retval 1:成功 0:失败
 */
uint8_t A7680C_MQTT_Connect(char *client, char *user, char *pass)
{
    char cmd[160];

    /* 步骤1: 获取客户端 AT+CMQTTACCQ=0,"client_id",0 */
    snprintf(cmd, sizeof(cmd), "AT+CMQTTACCQ=0,\"%s\",0\r\n", client);
    if (A7680C_SendAT(cmd, "OK", 3000, NULL) != AT_RESULT_OK)
    {
        printf("MQTT: ACCQ失败\r\n");
        return 0;
    }

    /* 步骤2: 配置argtopic模式(1=topic作为AT命令参数传入) */
    if (A7680C_SendAT("AT+CMQTTCFG=\"argtopic\",0,1,1\r\n", "OK", 3000, NULL) != AT_RESULT_OK)
    {
        printf("MQTT: CFG失败\r\n");
        return 0;
    }

    /* 步骤3: 连接服务器 AT+CMQTTCONNECT=0,"tcp://host:port",timeout,clean_session */
    snprintf(cmd, sizeof(cmd), "AT+CMQTTCONNECT=0,\"tcp://%s:%d\",60,1\r\n",
             MQTT_BROKER_ADDR, MQTT_BROKER_PORT);
    if (A7680C_SendAT(cmd, "+CMQTTCONNECT: 0,0", 30000, NULL) != AT_RESULT_OK)
    {
        printf("MQTT: CONNECT失败\r\n");
        return 0;
    }

    printf("MQTT: 连接成功\r\n");
    return 1;
}


/**
 * @brief  MQTT EX发布消息
 * @note   官方流程(AT+CMQTTCFG="argtopic",0,1,1 模式):
 *         AT+CMQTTPUB=0,"topic",qos,length
 *         > (等待模块提示符)
 *         发送payload数据
 *         等待 +CMQTTPUB:0,0
 *
 *         为了避免竞态条件,在同一个互斥锁保护下完成payload发送和等待响应
 * @param  topic: 主题字符串
 * @param  msg:  消息payload(JSON字符串)
 * @retval 1:成功 0:失败
 */
uint8_t A7680C_MQTT_Publish(char *topic, char *msg)
{
    char cmd[256];
    uint16_t topic_len = strlen(topic);
    uint16_t msg_len = strlen(msg);

    /* 参数检查 */
    if (topic_len == 0 || topic_len > 500 || msg_len == 0 || msg_len > 1024)
    {
        printf("MQTT发布: 参数无效(topic=%u, msg=%u)\r\n", topic_len, msg_len);
        return 0;
    }

    /*
     * 步骤1: 发送PUB命令, 等待 ">" 提示符
     * 官方格式: AT+CMQTTPUB=<client_index>,"<topic>",<qos>,<req_length>
     */
    snprintf(cmd, sizeof(cmd), "AT+CMQTTPUB=0,\"%s\",1,%u\r\n", topic, msg_len);

    if (A7680C_SendAT(cmd, ">", 5000, NULL) != AT_RESULT_OK)
    {
        printf("MQTT发布: 等待'>'失败, topic=%s\r\n", topic);
        return 0;
    }

    /*
     * 步骤2: 在同一个互斥锁保护下发送payload并等待发布结果
     * 这样可以避免A7680C_SendAT("")重置缓冲区导致响应丢失
     */
    if (osSemaphoreAcquire(at_mutexHandle, osWaitForever) != osOK)
    {
        return 0;
    }

    /* 重置接收状态 */
    at_index = 0;
    keyword = "+CMQTTPUB: 0,0";
    while (osSemaphoreAcquire(at_semHandle, 0) == osOK);

    /* 发送payload数据 */
    A7680C_Send(msg);

    /* 等待发布结果: +CMQTTPUB:0,0 表示成功 */
    uint8_t ret = 0;
    if (osSemaphoreAcquire(at_semHandle, pdMS_TO_TICKS(10000)) == osOK)
    {
        if (at_result == AT_RESULT_OK)
        {
            ret = 1;
        }
        else
        {
            printf("MQTT发布失败: %s\r\n", topic);
        }
    }
    else
    {
				printf("模块响应的字符串为:%s",at_parse_buf);
        printf("MQTT发布超时: %s\r\n", topic);
    }

    osSemaphoreRelease(at_mutexHandle);
    return ret;
}


/**
 * @brief  MQTT EX断开连接
 * @note   AT+CMQTTDISC=0,120
 * @retval 1:成功 0:失败
 */
uint8_t A7680C_MQTT_Disconnect(void)
{
    return A7680C_SendAT("AT+CMQTTDISC=0,120\r\n", "+CMQTTDISC:0,0", 10000, NULL);
}


/**
 * @brief  MQTT EX停止服务
 * @note   AT+CMQTTSTOP
 * @retval 1:成功 0:失败
 */
uint8_t A7680C_MQTT_Stop(void)
{
    return A7680C_SendAT("AT+CMQTTSTOP\r\n", "+CMQTTSTOP:0", 5000, NULL);
}
