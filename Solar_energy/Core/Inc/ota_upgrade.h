/**
  ******************************************************************************
  * @file    ota_upgrade.h
  * @brief   双Bank固件升级管理器 (APP直擦直写 + CRC32校验 + 自动回滚)
  *
  * 设计思路:
  *   1. APP通过TCP接收固件数据, 直接写入备用Bank
  *   2. 硬件CRC32校验整个备用Bank
  *   3. 校验通过: 写RTC备份寄存器(trial标志) → BANK_SwitchAndReset()
  *   4. 新Bank启动: main()开头检查trial标志
  *      - 若trial计数超限 → 自动切回旧Bank (回滚)
  *      - 若正常启动 → FreeRTOS运行数秒后清除trial标志 (确认)
  *   5. 若新固件崩溃 → IWDG超时复位 → trial计数+1 → 最终回滚
  *
  * Flash布局 (STM32F429IGT6 双Bank模式 DB1M=1):
  *   Bank 1: 0x08000000 - 0x0807FFFF (512KB, 8个扇区)
  *   Bank 2: 0x08080000 - 0x080FFFFF (512KB, 8个扇区)
  ******************************************************************************
  */
#ifndef __OTA_UPGRADE_H
#define __OTA_UPGRADE_H

#include "stdint.h"

/* ==================== 错误码 ==================== */
typedef enum {
    OTA_OK                =  0,   /**< 成功 */
    OTA_ERR_SEQ           = -1,   /**< 包序号不连续 */
    OTA_ERR_OVERFLOW      = -2,   /**< 超出Bank容量 */
    OTA_ERR_FLASH         = -3,   /**< Flash擦写失败 */
    OTA_ERR_SIZE_MISMATCH = -4,   /**< 总大小与已接收不一致 */
    OTA_ERR_CRC           = -5,   /**< CRC32校验失败 */
    OTA_ERR_BUSY          = -6,   /**< 正在升级中, 拒绝重复开始 */
    OTA_ERR_NOT_DUAL_BANK = -7,   /**< 不在双Bank模式 */
} ota_err_t;

/* ==================== 升级状态 ==================== */
typedef enum {
    OTA_STATE_IDLE      = 0,   /**< 空闲 */
    OTA_STATE_RECEIVING = 1,   /**< 接收中 */
    OTA_STATE_VERIFYING = 2,   /**< 校验中 */
    OTA_STATE_DONE      = 3,   /**< 已完成(等待复位) */
} ota_state_t;

/* ==================== TCP协议数据结构 ==================== */
#pragma pack(push, 1)

/** CMD_OTA_BEGIN 请求 (8字节) */
typedef struct {
    uint32_t total_size;   /**< 固件总大小(字节) */
    uint32_t fw_crc32;     /**< 固件CRC32 (STM32硬件CRC算法) */
} OtaBeginReq_t;

/** CMD_OTA_DATA 请求头 (4字节, 后跟data[]) */
typedef struct {
    uint16_t seq;          /**< 包序号, 从0递增 */
    uint16_t data_len;     /**< 本包数据长度(字节) */
    /* 紧跟 uint8_t data[data_len] */
} OtaDataHeader_t;

/** CMD_OTA_END 请求 (4字节) */
typedef struct {
    uint32_t fw_crc32;     /**< 二次确认CRC32 */
} OtaEndReq_t;

/** CMD_OTA_STATUS 响应 */
typedef struct {
    uint8_t  active_bank;     /**< 当前运行Bank: 1或2 */
    uint8_t  ota_state;       /**< 升级状态 (ota_state_t) */
    uint16_t trial_count;     /**< 试启动计数(回滚诊断用) */
    uint32_t received_bytes;  /**< 已接收字节数 */
    uint32_t total_bytes;     /**< 固件总大小 */
    uint32_t standby_bank;    /**< 备用Bank基地址 */
} OtaStatusResp_t;

#pragma pack(pop)

/* ==================== RTC备份寄存器定义 ==================== */
#define RTC_BKP_DR_OTA_MAGIC    RTC_BKP_DR0   /**< 魔数寄存器 */
#define RTC_BKP_DR_OTA_TRIALS   RTC_BKP_DR1   /**< 试启动计数器 */
#define RTC_BKP_DR_OTA_SIZE     RTC_BKP_DR2   /**< 固件大小(诊断用) */
#define RTC_BKP_DR_OTA_CRC      RTC_BKP_DR3   /**< 固件CRC32(诊断用) */

#define OTA_MAGIC_PENDING       0xB0070A7AU   /**< 升级待确认魔数 */
#define OTA_MAGIC_CONFIRMED     0x00000000U   /**< 已确认/空闲 */
#define OTA_MAX_TRIAL_BOOTS     3             /**< 最大试启动次数 */

/* ==================== 升级进行中标志 ==================== */
/* DevicePoll_Task 等任务检查此标志, 为1时暂停非关键业务 */
extern volatile uint8_t g_ota_in_progress;

/* ==================== 公共API ==================== */

/**
 * @brief  初始化OTA状态 (在main.c的USER CODE BEGIN 2中调用)
 * @note   仅重置RAM状态, 不影响RTC备份寄存器
 */
void OTA_Init(void);

/**
 * @brief  开机回滚检查 (在main()最开头, 外设初始化后立即调用)
 * @note   读取RTC备份寄存器, 若trial计数超限则自动切回旧Bank
 *         必须在MX_RTC_Init()之后调用
 */
void OTA_BootCheck(void);

/**
 * @brief  确认升级成功 (在FreeRTOS稳定运行数秒后调用)
 * @note   清除RTC备份寄存器的trial标志, 标记新固件为"已确认"
 */
void OTA_ConfirmIfPending(void);

/**
 * @brief  喂独立看门狗 (IWDG)
 * @note   在FreeRTOS任务中周期调用
 */
void OTA_FeedWatchdog(void);

/**
 * @brief  开始升级 (CMD_OTA_BEGIN 处理函数)
 *         擦除整个备用Bank(8个扇区), 初始化接收状态
 * @param  total_size  固件总大小(字节)
 * @param  crc32       上位机计算的STM32硬件CRC32
 * @return OTA_OK成功; 负数见ota_err_t
 */
int OTA_Begin(uint32_t total_size, uint32_t crc32);

/**
 * @brief  接收一包升级数据 (CMD_OTA_DATA 处理函数)
 * @param  seq   包序号(从0递增, 必须连续)
 * @param  data  数据指针
 * @param  len   数据长度
 * @return OTA_OK成功; 负数见ota_err_t
 */
int OTA_ReceiveChunk(uint16_t seq, const uint8_t *data, uint16_t len);

/**
 * @brief  执行升级 (CMD_OTA_END 处理函数)
 *         校验CRC32 → 写trial标志 → BANK_SwitchAndReset()
 * @param  crc32  二次确认CRC32
 * @return OTA_OK成功(一般不返回, 会复位); 负数见ota_err_t
 */
int OTA_End(uint32_t crc32);

/**
 * @brief  获取当前升级状态 (CMD_OTA_STATUS 响应)
 * @param  out  输出状态结构体
 */
void OTA_GetStatus(OtaStatusResp_t *out);

#endif /* __OTA_UPGRADE_H */
