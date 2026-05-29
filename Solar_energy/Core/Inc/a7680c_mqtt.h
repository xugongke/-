#ifndef __A7680C_MQTT_H
#define __A7680C_MQTT_H

#include "a7680c_at.h"

/* ================== MQTT服务器配置 ================== */
#define MQTT_BROKER_ADDR    "broker.emqx.io"
#define MQTT_BROKER_PORT    1883
#define MQTT_CLIENT_ID      "solar_dev_01"
/* ==================================================== */

/* MQTT连接状态标志(供其他任务检查) */
extern volatile uint8_t g_mqtt_connected;

/* MQTT EX 启动服务 */
uint8_t A7680C_MQTT_Start(void);

/* MQTT EX 连接服务器 */
uint8_t A7680C_MQTT_Connect(char *client, char *user, char *pass);

/* MQTT EX 发布消息 */
uint8_t A7680C_MQTT_Publish(char *topic, char *msg);

/* MQTT EX 带自动重连的发布消息 */
uint8_t A7680C_MQTT_Publish_Safe(char *topic, char *msg);

/* MQTT EX 断开连接 */
uint8_t A7680C_MQTT_Disconnect(void);

/* MQTT EX 停止服务 */
uint8_t A7680C_MQTT_Stop(void);

/* MQTT完整重连流程(Stop→Start→Connect) */
uint8_t A7680C_MQTT_Reconnect(void);

#endif
