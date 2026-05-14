#ifndef __A7680C_MQTT_H
#define __A7680C_MQTT_H

#include "a7680c_at.h"

/* ================== MQTT服务器配置 ================== */
#define MQTT_BROKER_ADDR    "broker.emqx.io"
#define MQTT_BROKER_PORT    1883
#define MQTT_CLIENT_ID      "solar_dev_01"
/* ==================================================== */

/* MQTT EX 启动服务 */
uint8_t A7680C_MQTT_Start(void);

/* MQTT EX 连接服务器 */
uint8_t A7680C_MQTT_Connect(char *client, char *user, char *pass);

/* MQTT EX 发布消息 */
uint8_t A7680C_MQTT_Publish(char *topic, char *msg);

/* MQTT EX 断开连接 */
uint8_t A7680C_MQTT_Disconnect(void);

/* MQTT EX 停止服务 */
uint8_t A7680C_MQTT_Stop(void);

#endif
