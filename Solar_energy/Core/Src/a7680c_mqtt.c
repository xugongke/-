#include "a7680c_mqtt.h"


/**
 * @brief  启动MQTT功能
 */
uint8_t A7680C_MQTT_Start(void)
{
    return A7680C_SendAT("AT+CMQTTSTART\r\n","OK",5000,NULL);
}


/**
 * @brief  MQTT服务器连接
 */
uint8_t A7680C_MQTT_Connect(char *client,char *user,char *pass)
{
    char cmd[128];

    /* 设置客户端ID */
    sprintf(cmd,"AT+CMQTTACCQ=0,\"%s\"\r\n",client);

    if(!A7680C_SendAT(cmd,"OK",2000,NULL))
        return 0;

    /* 连接服务器 */
    sprintf(cmd,"AT+CMQTTCONNECT=0,\"tcp://broker.emqx.io:1883\",60,1\r\n");

    return A7680C_SendAT(cmd,"OK",8000,NULL);
}


/**
 * @brief  MQTT发布消息
 */
uint8_t A7680C_MQTT_Publish(char *topic,char *msg)
{
    char cmd[128];

    sprintf(cmd,"AT+CMQTTTOPIC=0,%d\r\n",strlen(topic));

    if(!A7680C_SendAT(cmd,">",2000,NULL))
        return 0;

    A7680C_Send(topic);

    HAL_Delay(200);

    sprintf(cmd,"AT+CMQTTPAYLOAD=0,%d\r\n",strlen(msg));

    if(!A7680C_SendAT(cmd,">",2000,NULL))
        return 0;

    A7680C_Send(msg);

    return A7680C_SendAT("AT+CMQTTPUB=0,1,60\r\n","OK",5000,NULL);
}
