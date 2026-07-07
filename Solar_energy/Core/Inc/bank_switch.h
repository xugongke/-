/**
  ******************************************************************************
  * @file    bank_switch.h
  * @brief   STM32F429IGT6 双Bank切换验证模块
  * @note    Flash布局 (双Bank模式 DB1M=1):
  *          Bank 1 (物理): 0x08000000 - 0x0807FFFF (512KB)
  *          Bank 2 (物理): 0x08080000 - 0x080FFFFF (512KB)
  *
  *          Bank交换机制 (BFB2位):
  *          BFB2=0: Bank1映射到0x08000000, Bank2映射到0x08080000 (默认)
  *          BFB2=1: Bank2映射到0x08000000, Bank1映射到0x08080000 (交换)
  *
  *          关键点: 交换后，当前运行的Bank始终从0x08000000可见，
  *          所以同一份固件可以运行在任意Bank，无需修改地址。
  ******************************************************************************
  */

#ifndef __BANK_SWITCH_H
#define __BANK_SWITCH_H

#include "stm32f4xx.h"
#include <stdint.h>

/* Bank地址定义 */
#define BANK1_BASE_ADDR    0x08000000UL  /* Bank 1 基地址 (512KB) */
#define BANK2_BASE_ADDR    0x08080000UL  /* Bank 2 基地址 (512KB) */
#define BANK_SIZE          0x00080000UL  /* 每个Bank 512KB */

/*
 * Option Bytes 相关位 (FLASH_OPTCR) - 直接使用 CMSIS 定义
 * BFB2  : bit 4  (0x00000010) - Boot From Bank 2
 * DB1M  : bit 30 (0x40000000) - Dual-Bank Mode for 1MB devices
 * OPTSTRT: bit 1 (0x00000002) - Option start
 *
 * 注意: BFB2 和 DB1M 已在 stm32f429xx.h 中定义
 */

/**
 * @brief  获取当前激活的Bank编号
 * @retval 1: 当前从Bank 1启动 (BFB2=0)
 * @retval 2: 当前从Bank 2启动 (BFB2=1)
 */
uint8_t BANK_GetActiveBank(void);

/**
 * @brief  检查是否处于双Bank模式
 * @retval 1: 双Bank模式已启用 (DB1M=1)
 * @retval 0: 单Bank模式 (DB1M=0)
 */
uint8_t BANK_IsDualBankMode(void);

/**
 * @brief  启用双Bank模式 (设置DB1M=1) 并执行系统复位
 * @note   此操作会修改Option Bytes的DB1M位 (bit 30)
 *         需要系统复位后生效
 *         ⚠️ 注意：操作期间不要断电！
 * @retval 0: 成功（系统会复位，不会返回）
 *         1: 失败
 */
uint8_t BANK_EnableDualBank(void);

/**
 * @brief  切换Bank并执行系统复位
 * @note   切换后MCU会自动复位，从另一个Bank启动
 * @retval 0: 成功发起切换（不会返回，因为会复位）
 *         1: 失败（不在双Bank模式）
 */
uint8_t BANK_SwitchAndReset(void);

/**
 * @brief  设置Bank模式 (DB1M + BFB2) 并执行系统复位
 * @param  dual_bank: 1=双Bank(DB1M=1), 0=单Bank(DB1M=0)
 * @param  bfb2: 1=从Bank2启动, 0=从Bank1启动
 * @note   不会返回 (复位后从头执行). 用于OTA临时切单Bank擦除.
 */
uint8_t BANK_SetBankMode(uint8_t dual_bank, uint8_t bfb2);

/**
 * @brief  打印当前Bank信息（通过串口）
 * @param  huart: UART句柄指针
 */
void BANK_PrintInfo(void);

/**
 * @brief  通过串口命令处理Bank切换
 * @param  huart: UART句柄指针
 * @param  cmd: 接收到的命令字符串
 */
void BANK_ProcessCommand(const char *cmd);

#endif /* __BANK_SWITCH_H */
