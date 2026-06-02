#ifndef A7680C_NETMGR_H
#define A7680C_NETMGR_H

#include "main.h"

/**
 * @brief  网络就绪标志（供 Weather_Task 和 DevicePoll_Task 判断）
 *         1=网络已初始化且正常, 0=网络未就绪/异常
 */
extern uint8_t simcard_ready;

/**
 * @brief  网络管理器初始化（主循环开始前调用一次）
 * @param  weather_timer: 天气定时器句柄, 初始化成功后自动启动
 */
void A7680C_NetManager_Init(void *weather_timer);

/**
 * @brief  网络管理器周期处理（在主循环中每秒调用一次）
 * @retval 建议的延时时间(ms), 传给 osDelay()
 *         - 1000: 正常运行
 *         - 5000: 初始化/重启失败, 等待后重试
 *         - 0  : 初始化刚完成, 立即进入下一轮
 *
 * @note   内部自动维护状态机:
 *         need_init=1 → 执行全量初始化(ATE0→网络→时间→HTTP→MQTT)
 *         need_init=0 → 执行健康检查(CPIN+CGATT每秒, CLBS每30秒)
 */
uint32_t A7680C_NetManager_Process(void);

#endif /* A7680C_NETMGR_H */
