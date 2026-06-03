/**
  ******************************************************************************
  * @file    bank_switch.c
  * @brief   STM32F429IGT6 双Bank切换验证实现
  ******************************************************************************
  */

#include "bank_switch.h"
#include "usart.h"
#include "stdio.h"
#include "string.h"

/* ========== 公共函数实现 ========== */

/**
 * @brief  检查是否处于双Bank模式
 * @note   读取 FLASH_OPTCR 的 DB1M 位 (bit30)
 *         DB1M=1 表示 1MB Flash 被配置为双Bank模式 (2x512KB)
 */
uint8_t BANK_IsDualBankMode(void)
{
    return (FLASH->OPTCR & FLASH_OPTCR_DB1M) ? 1 : 0;
}

/**
 * @brief  获取当前激活的Bank编号
 * @note   读取 FLASH_OPTCR 的 BFB2 位 (bit 4)
 *         BFB2=0: 从Bank 1启动
 *         BFB2=1: 从Bank 2启动
 *
 *         重要：Bank交换后，当前运行Bank始终映射到 0x08000000，
 *         所以代码不需要修改向量表地址。
 */
uint8_t BANK_GetActiveBank(void)
{
    return (FLASH->OPTCR & FLASH_OPTCR_BFB2) ? 2 : 1;
}

/**
 * @brief  启用双Bank模式 (设置DB1M=1) 并执行系统复位
 * @note   此操作修改 Option Bytes 的 DB1M 位 (bit 30)
 *         将 1MB Flash 从单Bank模式切换为双Bank模式 (2x512KB)
 *
 *         ⚠️ 重要警告:
 *         1. 操作期间绝对不能断电！
 *         2. 启用双Bank后，Bank1只保留512KB (0x08000000-0x0807FFFF)
 *            如果当前固件超过512KB，超出的部分会丢失！
 *         3. 需要确保当前编译的scatter file限制了512KB
 *
 * @retval 0: 成功（系统会复位，不会返回）
 *         1: 失败
 */
uint8_t BANK_EnableDualBank(void)
{
    uint32_t optcr_val;

    /* 如果已经在双Bank模式，不需要操作 */
    if (BANK_IsDualBankMode())
    {
        return 1;
    }

    /* 禁用中断（Option Bytes编程期间Flash不可读，中断会导致Bus Fault） */
    __disable_irq();

    /* 清除SR中残余错误标志 */
    FLASH->SR = FLASH->SR;

    /* 确保Flash空闲 */
    while (FLASH->SR & FLASH_SR_BSY) {}

    /* 解锁Flash控制器 */
    FLASH->KEYR = FLASH_KEY1;
    FLASH->KEYR = FLASH_KEY2;

    /* 解锁Option Bytes */
    FLASH->OPTKEYR = FLASH_OPT_KEY1;
    FLASH->OPTKEYR = FLASH_OPT_KEY2;

    /* 等待解锁完成 */
    while (FLASH->SR & FLASH_SR_BSY) {}

    /* 读取当前OPTCR值，设置DB1M位 */
    optcr_val = FLASH->OPTCR;
    optcr_val |= FLASH_OPTCR_DB1M;       /* 设置DB1M=1 (双Bank模式) */
    optcr_val &= ~FLASH_OPTCR_OPTLOCK;   /* 确保OPTLOCK=0 */

    /* 先写入新值，再触发编程 */
    FLASH->OPTCR = optcr_val;
    FLASH->OPTCR = optcr_val | FLASH_OPTCR_OPTSTRT;

    /* 等待编程完成 */
    while (FLASH->SR & FLASH_SR_BSY) {}

    /* 检查错误 */
    if (FLASH->SR & (FLASH_SR_PGSERR | FLASH_SR_PGPERR | FLASH_SR_PGAERR | FLASH_SR_WRPERR))
    {
        FLASH->OPTCR |= FLASH_OPTCR_OPTLOCK;
        FLASH->CR |= FLASH_CR_LOCK;
        return 1;
    }

    /* 锁定 */
    FLASH->OPTCR |= FLASH_OPTCR_OPTLOCK;
    FLASH->CR |= FLASH_CR_LOCK;

    /* 忙等延时后复位（中断已禁用，不能用HAL_Delay） */
    for (volatile uint32_t d = 0; d < 5000000; d++) { __NOP(); }
    NVIC_SystemReset();

    return 0;
}

/**
 * @brief  切换Bank并执行系统复位
 * @note   翻转BFB2位 (bit 4)，触发Option Bytes编程，然后系统复位。
 *         BFB2位的变化在系统复位后生效。
 *         复位后MCU自动从交换后的Bank启动。
 *
 *         关键点：
 *         - 操作前必须禁用中断（Flash编程期间不可读）
 *         - 编程前清除SR残余错误标志（否则会误判编程失败）
 *         - 复位前使用忙等延时（中断已禁用，不能用HAL_Delay）
 *
 * @retval 0: 成功（系统会复位，不会返回）
 *         1: 失败
 */
uint8_t BANK_SwitchAndReset(void)
{
    uint32_t optcr_before, optcr_after;

    /* 检查是否在双Bank模式 */
    if (!BANK_IsDualBankMode())
    {
        printf("[BANK] Not in dual-bank mode!\r\n");
        return 1;
    }

    /* ★ 禁用所有中断！
     * Option Bytes 编程期间 Flash 不可读，如果此时发生中断，
     * CPU 需要从 Flash 读取中断向量 → Bus Fault → 死机！
     */
    __disable_irq();

    /* 清除SR中所有残余错误标志（写1清零），避免误判编程失败 */
    FLASH->SR = FLASH->SR;

    /* 确保Flash空闲 */
    while (FLASH->SR & FLASH_SR_BSY) {}

    /* 解锁Flash控制器 */
    FLASH->KEYR = FLASH_KEY1;
    FLASH->KEYR = FLASH_KEY2;

    /* 解锁Option Bytes */
    FLASH->OPTKEYR = FLASH_OPT_KEY1;
    FLASH->OPTKEYR = FLASH_OPT_KEY2;

    /* 等待解锁完成 */
    while (FLASH->SR & FLASH_SR_BSY) {}

    /* 读取当前OPTCR值 */
    optcr_before = FLASH->OPTCR;

    /* 翻转BFB2位 */
    optcr_after = optcr_before ^ FLASH_OPTCR_BFB2;
    optcr_after &= ~(FLASH_OPTCR_OPTSTRT | FLASH_OPTCR_OPTLOCK);

    /* 先写入新的OPTCR值（不含OPTSTRT） */
    FLASH->OPTCR = optcr_after;

    /* 设置OPTSTRT触发编程 */
    FLASH->OPTCR = optcr_after | FLASH_OPTCR_OPTSTRT;

    /* 等待编程完成 */
    while (FLASH->SR & FLASH_SR_BSY) {}

    /* 检查是否有错误 */
    if (FLASH->SR & (FLASH_SR_PGSERR | FLASH_SR_PGPERR | FLASH_SR_PGAERR | FLASH_SR_WRPERR))
    {
        FLASH->OPTCR |= FLASH_OPTCR_OPTLOCK;
        FLASH->CR |= FLASH_CR_LOCK;
        return 1;
    }

    /* 锁定 */
    FLASH->OPTCR |= FLASH_OPTCR_OPTLOCK;
    FLASH->CR |= FLASH_CR_LOCK;

    /* 忙等延时（中断已禁用，不能用HAL_Delay） */
    for (volatile uint32_t d = 0; d < 5000000; d++) { __NOP(); }
    NVIC_SystemReset();

    return 0;
}

/**
 * @brief  打印当前Bank信息
 */
void BANK_PrintInfo(void)
{
    char buf[128];

    printf("\r\n====== STM32F429IGT6 Dual-Bank Info ======\r\n");

    /* 双Bank模式状态 */
    snprintf(buf, sizeof(buf), "Dual-Bank Mode : %s\r\n",
             BANK_IsDualBankMode() ? "ENABLED (DB1M=1)" : "DISABLED (DB1M=0)");
    printf("%s", buf);

    /* 当前OPTCR寄存器原始值 */
    snprintf(buf, sizeof(buf), "OPTCR Register: 0x%08X\r\n", FLASH->OPTCR);
    printf("%s", buf);

    /* 当前激活Bank */
    snprintf(buf, sizeof(buf), "Active Bank    : BANK %d (BFB2=%d)\r\n",
             BANK_GetActiveBank(),
             (FLASH->OPTCR & FLASH_OPTCR_BFB2) ? 1 : 0);
    printf("%s", buf);

    /* Bank地址映射 */
    if (BANK_GetActiveBank() == 1)
    {
        printf("Bank 1 @ 0x08000000 (Running HERE)\r\n");
        printf("Bank 2 @ 0x08080000 (Standby)\r\n");
    }
    else
    {
        printf("Bank 2 @ 0x08000000 (Running HERE, SWAPPED)\r\n");
        printf("Bank 1 @ 0x08080000 (Standby, SWAPPED)\r\n");
    }

    /* Flash总大小 */
    snprintf(buf, sizeof(buf), "Flash Total    : 1MB (2 x 512KB)\r\n");
    printf("%s", buf);

    /* 读取另一个Bank头部几个字（用于验证是否有有效固件） */
    uint32_t *other_bank_ptr = (uint32_t *)BANK2_BASE_ADDR;

    snprintf(buf, sizeof(buf), "Other Bank hdr : %08X %08X %08X %08X\r\n",
             other_bank_ptr[0], other_bank_ptr[1], other_bank_ptr[2], other_bank_ptr[3]);
    printf("%s", buf);

    /* 有效的固件头: 第一个字应该是初始栈指针，通常在 0x20000000-0x20040000 范围 */
    uint32_t initial_sp = other_bank_ptr[0];
    if (initial_sp >= 0x20000000UL && initial_sp <= 0x20040000UL)
    {
        printf("Other Bank FW  : VALID (valid SP detected)\r\n");
    }
    else
    {
        printf("Other Bank FW  : EMPTY or INVALID\r\n");
    }

    printf("===========================================\r\n");
    if (BANK_IsDualBankMode())
    {
        printf("Commands: \"BANK:INFO\" | \"BANK:SWITCH\"\r\n");
    }
    else
    {
        printf("Commands: \"BANK:INFO\" | \"BANK:ENABLEDB\"\r\n");
        printf("WARNING: DB1M=0! Send BANK:ENABLEDB to enable dual-bank\r\n");
    }
    printf("===========================================\r\n\r\n");
}

/**
 * @brief  通过串口命令处理Bank切换
 * @param  cmd: 命令字符串 (不区分大小写)
 *        "BANK:INFO"     - 打印Bank信息
 *        "BANK:SWITCH"   - 切换Bank并复位 (需双Bank模式)
 *        "BANK:ENABLEDB" - 启用双Bank模式并复位
 */
void BANK_ProcessCommand(const char *cmd)
{
    if (strstr(cmd, "BANK:INFO") != NULL || strstr(cmd, "bank:info") != NULL)
    {
        BANK_PrintInfo();
    }
    else if (strstr(cmd, "BANK:ENABLEDB") != NULL || strstr(cmd, "bank:enabledb") != NULL)
    {
        printf("Enabling Dual-Bank Mode (DB1M=1) ...\r\n");
        printf("WARNING: Do NOT power off during this operation!\r\n");

        /* 确保串口数据发送完毕 */
        HAL_Delay(50);

        /* 执行启用 (不会返回，系统会复位) */
        uint8_t ret = BANK_EnableDualBank();
        if (ret != 0)
        {
            printf("Failed! Already in Dual-Bank mode or error.\r\n");
        }
    }
    else if (strstr(cmd, "BANK:SWITCH") != NULL || strstr(cmd, "bank:switch") != NULL)
    {
        char buf[64];
        uint8_t current = BANK_GetActiveBank();

        snprintf(buf, sizeof(buf), "Switching from Bank %d -> Bank %d ...\r\n",
                 current, (current == 1) ? 2 : 1);
        printf("%s", buf);

        /* 确保串口数据发送完毕 */
        HAL_Delay(50);

        /* 执行切换 (不会返回) */
        BANK_SwitchAndReset();
    }
}
