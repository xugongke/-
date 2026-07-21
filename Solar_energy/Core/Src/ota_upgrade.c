/**
  ******************************************************************************
  * @file    ota_upgrade.c
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
  * Flash扇区布局 (STM32F429IGT6 双Bank模式 DB1M=1):
  *   Bank 1: FLASH_SECTOR_0~7   (0x08000000-0x0807FFFF, 512KB)
  *     SECTOR_0~3: 16KB, SECTOR_4: 64KB, SECTOR_5~7: 128KB
  *   Bank 2: FLASH_SECTOR_12~19 (0x08080000-0x080FFFFF, 512KB)
  *     SECTOR_12~15: 16KB, SECTOR_16: 64KB, SECTOR_17~19: 128KB
  *
  *   关键: 双Bank模式下, 不管BFB2=0还是BFB2=1, 备用区的映射地址始终为
  *         0x08080000. HAL_FLASHEx_Erase可以正常擦除双Bank模式下的Flash,
  *         只需设置正确的Sector起始值和数量:
  *           运行Bank1(BFB2=0): 擦FLASH_SECTOR_12, NbSectors=8 (擦Bank2)
  *           运行Bank2(BFB2=1): 擦FLASH_SECTOR_0,  NbSectors=8 (擦Bank1)
  ******************************************************************************
  */
#include "ota_upgrade.h"
#include "bank_switch.h"
#include "stm32f4xx_hal.h"
#include "rtc.h"
#include "iwdg.h"
#include "crc.h"
#include "stm32f4xx_hal_flash.h"
#include "stm32f4xx_hal_flash_ex.h"
#include <string.h>
#include <stdio.h>

extern RTC_HandleTypeDef hrtc;
extern IWDG_HandleTypeDef hiwdg;
extern CRC_HandleTypeDef hcrc;

volatile uint8_t g_ota_in_progress = 0;

/* ===== OTA升级模块内部状态 (仅存在于RAM, 复位后清零) ===== */
static ota_state_t s_state        = OTA_STATE_IDLE;  /* 当前升级状态 */
static uint16_t    s_next_seq     = 0;               /* 期望接收的下一个包序号(连续校验) */
static uint32_t    s_write_off    = 0;               /* 备用Bank当前写入偏移(累计已接收字节) */
static uint32_t    s_total_size   = 0;               /* 本次升级固件总大小(字节) */
static uint32_t    s_expected_crc = 0;               /* 期望的固件CRC32 (由BEGIN下发) */
/* 备用区地址固定为0x08080000:
 * 双Bank模式下, Bank交换只影响运行区映射(0x08000000指向当前Bank),
 * 备用区的映射地址始终是0x08080000, 所以固件写入和CRC读取都从0x08080000开始. */
static uint32_t    s_standby_addr = BANK2_BASE_ADDR;  /* 0x08080000 */

/**
 * @brief  使能对RTC备份寄存器的写访问
 * @note   RTC备份寄存器在复位/掉电后仍能保持数据, 用于保存OTA的trial标志.
 *         写之前必须使能PWR时钟并打开备份域访问权限.
 */
static void enable_backup_access(void)
{
    __HAL_RCC_PWR_CLK_ENABLE();      /* 使能电源接口时钟 */
    HAL_PWR_EnableBkUpAccess();      /* 允许访问备份域 (清除DBP位保护) */
}

/**
 * @brief  读取一个RTC备份寄存器
 * @param  reg  备份寄存器编号 (如 RTC_BKP_DR0)
 * @retval 寄存器中的32位值
 */
static uint32_t read_bkp(uint32_t reg)
{
    return HAL_RTCEx_BKUPRead(&hrtc, reg);
}

/**
 * @brief  写入一个RTC备份寄存器
 * @param  reg  备份寄存器编号
 * @param  val  要写入的32位值
 */
static void write_bkp(uint32_t reg, uint32_t val)
{
    HAL_RTCEx_BKUPWrite(&hrtc, reg, val);
}

/**
 * @brief  擦除整个备用Bank (8个扇区, 共512KB)
 * @note   根据当前运行Bank确定要擦除的物理扇区:
 *           运行Bank1(BFB2=0): 备用区=Bank2, 擦 SECTOR_12~19
 *           运行Bank2(BFB2=1): 备用区=Bank1, 擦 SECTOR_0~7
 *         擦除前会清除Flash残留错误标志, 擦除过程中喂狗防止IWDG复位.
 * @retval 0: 成功; -1: 失败
 */
static int erase_standby_bank(void)
{
    FLASH_EraseInitTypeDef erase;
    uint32_t sector_error;
    uint8_t  active = BANK_GetActiveBank();   /* 当前运行Bank (1或2) */

    /* 根据当前运行Bank擦除备用Bank的全部8个扇区:
     *   运行Bank1(BFB2=0): 备用区=Bank2, 擦FLASH_SECTOR_12~19
     *   运行Bank2(BFB2=1): 备用区=Bank1, 擦FLASH_SECTOR_0~7
     */
    erase.TypeErase    = FLASH_TYPEERASE_SECTORS;
    erase.Banks        = (active == 1) ? FLASH_BANK_2 : FLASH_BANK_1;
    erase.Sector       = (active == 1) ? FLASH_SECTOR_12 : FLASH_SECTOR_0;
    erase.NbSectors    = 8;
    erase.VoltageRange = FLASH_VOLTAGE_RANGE_3;

    HAL_FLASH_Unlock();

    /* 清除可能残留的错误标志 */
    __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_OPERR | FLASH_FLAG_WRPERR |
                           FLASH_FLAG_PGAERR | FLASH_FLAG_PGPERR | FLASH_FLAG_PGSERR);

    OTA_FeedWatchdog();
    HAL_StatusTypeDef status = HAL_FLASHEx_Erase(&erase, &sector_error);
    OTA_FeedWatchdog();

    HAL_FLASH_Lock();

    if (status != HAL_OK)
    {
        printf("[OTA] Bank erase FAILED, status=%d, sector_error=0x%lX\r\n",
               status, sector_error);
        return -1;
    }
    printf("[OTA] Standby bank erased OK (active=Bank%d, sector %s)\r\n",
           active, (active == 1) ? "12~19" : "0~7");
    return 0;
}

/**
 * @brief  向备用Flash区写入一包数据 (按字节编程)
 * @note   写入地址 = 备用区基地址(0x08080000) + 偏移量.
 *         采用字节编程(FLASH_TYPEPROGRAM_BYTE)保证任意长度都能写入,
 *         无需考虑字节对齐问题 (代价是速度比字编程慢).
 * @param  offset  相对备用区基地址的偏移
 * @param  data    待写入数据
 * @param  len     数据长度
 * @retval 0: 成功; -1: 失败
 */
static int write_flash(uint32_t offset, const uint8_t *data, uint16_t len)
{
    uint32_t addr = s_standby_addr + offset;   /* 计算实际物理写入地址 */
    HAL_FLASH_Unlock();                        /* 解锁Flash控制器 */
    for (uint32_t i = 0; i < len; i++)
    {
        /* 按字节逐个编程, 任一字节失败即中止 */
        if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_BYTE,
                               addr + i, (uint64_t)data[i]) != HAL_OK)
        {
            printf("[OTA] Flash write FAILED at %lu\r\n", offset + i);
            HAL_FLASH_Lock();
            return -1;
        }
    }
    HAL_FLASH_Lock();                          /* 重新锁定Flash */
    return 0;
}

/**
 * @brief  使用STM32硬件CRC计算备用Bank中固件的CRC32
 * @note   硬件CRC要求按32位字(4字节)写入DR寄存器. 当固件长度不是4的
 *         整数倍时, 末尾不足一个字的字节需要补0xFF凑成完整字再计算
 *         (与上位机计算方式保持一致, 否则CRC不匹配).
 *         备用区中擦除后未写入的部分本来就是0xFF, 所以补0xFF符合实际.
 * @param  len  参与计算的固件长度(字节)
 * @retval 硬件计算出的CRC32值
 */
static uint32_t compute_hw_crc32(uint32_t len)
{
    uint32_t full_words  = len / 4;                 /* 完整的32位字个数 */
    uint32_t remain      = len % 4;                 /* 末尾不足一个字的剩余字节数 */
    uint32_t total_words = full_words + (remain > 0 ? 1 : 0);  /* 需要写入DR的总字数 */
    const uint32_t *p = (const uint32_t *)s_standby_addr;      /* 备用区起始地址 */

    __HAL_CRC_DR_RESET(&hcrc);                      /* 复位CRC计算单元 */

    for (uint32_t i = 0; i < total_words; i++)
    {
        uint32_t word;
        if (i < full_words)
        {
            word = p[i];                            /* 完整字: 直接读取 */
        }
        else
        {
            /* 末尾不足一个字: 用0xFF补齐到4字节, 再拷贝成32位字 */
            uint8_t buf[4] = {0xFF, 0xFF, 0xFF, 0xFF};
            const uint8_t *src = (const uint8_t *)&p[i];
            memcpy(buf, src, remain);               /* 拷贝剩余有效字节 */
            memcpy(&word, buf, 4);                  /* 组装成32位字 */
        }
        hcrc.Instance->DR = word;                   /* 写入DR, 硬件自动累加CRC */
    }
    return hcrc.Instance->DR;                       /* 读取最终CRC结果 */
}

/**
 * @brief  初始化OTA模块的RAM状态
 * @note   仅清零内存中的升级状态变量, 不影响RTC备份寄存器(回滚标志保留).
 *         应在 main() 的 USER CODE BEGIN 2 中调用一次.
 */
void OTA_Init(void)
{
    s_state           = OTA_STATE_IDLE;   /* 回到空闲态 */
    s_next_seq        = 0;                /* 序号从0开始 */
    s_write_off       = 0;                /* 写入偏移清零 */
    s_total_size      = 0;
    s_expected_crc    = 0;
    g_ota_in_progress = 0;                /* 清除"升级进行中"全局标志 */
}

/**
 * @brief  开机回滚检查 (自动判断新固件是否健康)
 * @note   必须在 main() 最开头、MX_RTC_Init() 之后调用. 工作机制:
 *           1. 读取RTC备份寄存器中的"升级待确认"魔数;
 *           2. 若魔数有效, 说明本次是从新Bank启动的"试启动", 计数+1;
 *           3. 若试启动次数超过 OTA_MAX_TRIAL_BOOTS, 认定新固件不可用,
 *              自动切回旧Bank (回滚); 否则继续运行, 等待 OTA_ConfirmIfPending 确认.
 *         若新固件反复崩溃 → IWDG复位 → 每次试启动计数+1 → 最终触发回滚.
 */
void OTA_BootCheck(void)
{
    enable_backup_access();                           /* 允许访问备份寄存器 */

    uint32_t magic = read_bkp(RTC_BKP_DR_OTA_MAGIC);  /* 读取升级标志 */
    if (magic != OTA_MAGIC_PENDING)                   /* 没有进行中的升级, 直接返回 */
        return;

    /* 此次启动属于"试启动", 计数累加 */
    uint32_t trials = read_bkp(RTC_BKP_DR_OTA_TRIALS);
    trials++;
    write_bkp(RTC_BKP_DR_OTA_TRIALS, trials);

    printf("[OTA] TRIAL BOOT #%lu/%d (Bank %d)\r\n",
           trials, OTA_MAX_TRIAL_BOOTS, BANK_GetActiveBank());

    /* 试启动次数超限: 判定新固件异常, 回滚到旧Bank */
    if (trials > OTA_MAX_TRIAL_BOOTS)
    {
        printf("[OTA] ROLLBACK: switching back!\r\n");
        write_bkp(RTC_BKP_DR_OTA_MAGIC, OTA_MAGIC_CONFIRMED);  /* 清除待确认标志 */
        write_bkp(RTC_BKP_DR_OTA_TRIALS, 0);                   /* 清零计数 */
        HAL_Delay(100);
        BANK_SwitchAndReset();                         /* 切回旧Bank并复位 */
    }
}

/**
 * @brief  确认升级成功 (新固件运行正常的"签字画押")
 * @note   在FreeRTOS稳定运行数秒后调用. 一旦确认, 清除待确认魔数和
 *         试启动计数, 表示新固件可靠, 后续复位不再触发回滚逻辑.
 */
void OTA_ConfirmIfPending(void)
{
    uint32_t magic = read_bkp(RTC_BKP_DR_OTA_MAGIC);
    if (magic == OTA_MAGIC_PENDING)        /* 仅当存在待确认标志时才处理 */
    {
        printf("[OTA] 固件已确认\r\n");
        write_bkp(RTC_BKP_DR_OTA_MAGIC, OTA_MAGIC_CONFIRMED);  /* 标记为已确认/空闲 */
        write_bkp(RTC_BKP_DR_OTA_TRIALS, 0);                   /* 清零试启动计数 */
    }
}

/**
 * @brief  喂独立看门狗 (IWDG)
 * @note   OTA擦写Flash、CRC计算耗时较长, 期间必须周期性喂狗,
 *         否则IWDG超时会触发意外复位, 导致升级中断或误判回滚.
 */
void OTA_FeedWatchdog(void)
{
    HAL_IWDG_Refresh(&hiwdg);   /* 刷新IWDG计数器, 防止超时复位 */
}

int OTA_Begin(uint32_t total_size, uint32_t crc32)
{
    /* 前置校验三连: 状态 / 双Bank模式 / 大小合法性 */
    if (s_state != OTA_STATE_IDLE)                 /* 已在升级中, 拒绝重复开始 */
        return OTA_ERR_BUSY;

    if (!BANK_IsDualBankMode())                    /* 必须先启用双Bank模式 */
        return OTA_ERR_NOT_DUAL_BANK;

    if (total_size == 0 || total_size > BANK_SIZE) /* 大小不能超过单Bank容量(512KB) */
        return OTA_ERR_OVERFLOW;

    s_total_size   = total_size;
    s_expected_crc = crc32;
    /* s_standby_addr 固定为 0x08080000, 不需要根据Bank动态计算 */
    s_next_seq     = 0;
    s_write_off    = 0;

    printf("[OTA] BEGIN size=%lu crc=0x%08lX standby=0x%08lX\r\n",
           total_size, crc32, s_standby_addr);

    OTA_FeedWatchdog();
    if (erase_standby_bank() != 0)
        return OTA_ERR_FLASH;
    OTA_FeedWatchdog();

    s_state = OTA_STATE_RECEIVING;
    g_ota_in_progress = 1;
    return OTA_OK;
}

int OTA_ReceiveChunk(uint16_t seq, const uint8_t *data, uint16_t len)
{
    /* 状态校验: 必须处于接收态 */
    if (s_state != OTA_STATE_RECEIVING)
        return OTA_ERR_BUSY;
    /* 参数校验: 数据指针非空且长度>0 */
    if (!data || len == 0)
        return OTA_ERR_FLASH;
    /* 序号连续性校验: 必须等于期望序号, 丢包/乱序直接报错 */
    if (seq != s_next_seq)
        return OTA_ERR_SEQ;
    /* 溢出校验: 累计写入不能超过固件总大小 */
    if (s_write_off + len > s_total_size)
        return OTA_ERR_OVERFLOW;

    /* 将本包数据写入备用Bank当前偏移处 */
    if (write_flash(s_write_off, data, len) != 0)
        return OTA_ERR_FLASH;

    s_write_off += len;     /* 累加已写入偏移 */
    s_next_seq++;           /* 期望序号+1, 等待下一包 */

    /* 每50包打印一次进度, 避免串口被刷爆 */
    if ((seq % 50) == 0)
        printf("[OTA] seq=%u %lu/%lu\r\n", seq, s_write_off, s_total_size);

    return OTA_OK;
}

int OTA_End(uint32_t crc32)
{
    if (s_state != OTA_STATE_RECEIVING)
        return OTA_ERR_BUSY;

    s_state = OTA_STATE_VERIFYING;   /* 进入校验态 */

    /* 1. 大小校验: 实际接收字节数必须等于声明的总大小 */
    if (s_write_off != s_total_size)
    {
        s_state = OTA_STATE_IDLE;
        g_ota_in_progress = 0;
        return OTA_ERR_SIZE_MISMATCH;
    }

    /* 2. CRC软校验: 上位机二次下发的CRC必须与BEGIN时一致 */
    if (crc32 != s_expected_crc)
    {
        s_state = OTA_STATE_IDLE;
        g_ota_in_progress = 0;
        return OTA_ERR_CRC;
    }

    /* 3. CRC硬校验: 用STM32硬件CRC重新计算整个备用Bank, 确保Flash数据正确 */
    OTA_FeedWatchdog();
    uint32_t calc_crc = compute_hw_crc32(s_total_size);

    if (calc_crc != s_expected_crc)
    {
        printf("[OTA] CRC FAILED calc=0x%08lX exp=0x%08lX\r\n", calc_crc, s_expected_crc);
        s_state = OTA_STATE_IDLE;
        g_ota_in_progress = 0;
        return OTA_ERR_CRC;
    }

    printf("[OTA] CRC OK, switching bank...\r\n");

    /* 4. 校验通过: 写入"待确认"标志和诊断信息到RTC备份寄存器.
     *    新Bank启动后由 OTA_BootCheck 计数, 由 OTA_ConfirmIfPending 确认. */
    enable_backup_access();
    write_bkp(RTC_BKP_DR_OTA_MAGIC, OTA_MAGIC_PENDING);  /* 标记升级待确认 */
    write_bkp(RTC_BKP_DR_OTA_TRIALS, 0);                 /* 试启动计数清零 */
    write_bkp(RTC_BKP_DR_OTA_SIZE, s_total_size);        /* 记录大小(诊断用) */
    write_bkp(RTC_BKP_DR_OTA_CRC, s_expected_crc);       /* 记录CRC(诊断用) */

    s_state = OTA_STATE_DONE;
    g_ota_in_progress = 0;
    HAL_Delay(100);
    BANK_SwitchAndReset();           /* 5. 切换到备用Bank并复位 (通常不返回) */

    return OTA_OK;
}

/**
 * @brief  获取当前OTA升级状态 (供 CMD_OTA_STATUS 上报给上位机)
 * @param  out  输出状态结构体指针 (调用方分配)
 */
void OTA_GetStatus(OtaStatusResp_t *out)
{
    if (!out) return;                                /* 空指针保护 */
    out->active_bank    = BANK_GetActiveBank();      /* 当前运行Bank: 1或2 */
    out->ota_state      = (uint8_t)s_state;          /* 升级状态机当前态 */
    out->trial_count    = (uint16_t)read_bkp(RTC_BKP_DR_OTA_TRIALS);  /* 试启动次数(回滚诊断) */
    out->received_bytes = s_write_off;               /* 已接收/写入字节数 (进度) */
    out->total_bytes    = s_total_size;              /* 固件总大小 */
    out->standby_bank   = BANK2_BASE_ADDR;  /* 备用区地址固定 0x08080000 */
    /* 拷贝固件版本号, 并保证以'\0'结尾 */
    strncpy(out->version, FIRMWARE_VERSION, sizeof(out->version) - 1);
    out->version[sizeof(out->version) - 1] = '\0';
}
