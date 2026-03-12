#ifndef __A7680C_AT_H
#define __A7680C_AT_H

#include "a7680c.h"

/*=============================
AT指令接口
=============================*/

/* 发送AT并等待应答 */
uint8_t A7680C_SendAT(char *cmd,char *ack,uint32_t timeout);

/* AT测试 */
uint8_t A7680C_Test(void);

/* 检测SIM卡 */
uint8_t A7680C_CheckSIM(void);

/* 查询信号强度 */
uint8_t A7680C_CheckSignal(void);

/* 网络附着 */
uint8_t A7680C_CheckNetwork(void);

uint8_t A7680C_SendCmdWaitAck(void);
uint8_t A7680C_SendATE1(void);
#endif
