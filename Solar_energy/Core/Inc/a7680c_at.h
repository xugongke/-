#ifndef __A7680C_AT_H
#define __A7680C_AT_H

#include "a7680c.h"

// 解析结果结构体（存 纬度、经度）
typedef struct {
    float latitude;   // 纬度
    float longitude;  // 经度
} CLBS_PosTypeDef;


/*=============================
AT指令接口
=============================*/
/* AT测试 */
uint8_t A7680C_Test(void);

/* 检测SIM卡 */
uint8_t A7680C_CheckSIM(void);

/* 查询信号强度 */
uint8_t A7680C_CheckSignal(void);

/* 网络附着 */
uint8_t A7680C_CheckNetwork(void);

uint8_t A7680C_SendCmdWaitAck(uint8_t* data_buff);
uint8_t A7680C_SendATE1(void);
uint8_t A7680C_SendATE0(void);
uint8_t A7680C_SendATW(void);
uint8_t A7680C_SendAT_CFUN(void);
uint8_t A7680C_SendAT_CCLK(uint8_t* data_buff);
at_result_t A7680C_GetNetworkTime_Debug(uint8_t *step);
uint8_t A7680C_SendAT_CSQ(uint8_t* data_buff);
uint8_t A7680C_ParseCSQ(const uint8_t *at_parse_buf, int32_t *rssi, int32_t *ber);
uint8_t A7680C_SendAT_CPIN(void);
CLBS_PosTypeDef A7680C_ParseCLBS(char *buf);
#endif
