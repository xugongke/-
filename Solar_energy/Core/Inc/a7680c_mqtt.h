#ifndef __A7680C_MQTT_H
#define __A7680C_MQTT_H

#include "a7680c_at.h"

/* MQTT初始化 */
uint8_t A7680C_MQTT_Start(void);

/* MQTT连接 */
uint8_t A7680C_MQTT_Connect(char *client,char *user,char *pass);

/* MQTT发布 */
uint8_t A7680C_MQTT_Publish(char *topic,char *msg);

#endif
