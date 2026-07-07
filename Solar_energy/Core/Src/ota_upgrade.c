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

static ota_state_t s_state        = OTA_STATE_IDLE;
static uint16_t    s_next_seq     = 0;
static uint32_t    s_write_off    = 0;
static uint32_t    s_total_size   = 0;
static uint32_t    s_expected_crc = 0;
/* 备用区地址固定为0x08080000:
 * 双Bank模式下, Bank交换只影响运行区映射(0x08000000指向当前Bank),
 * 备用区的映射地址始终是0x08080000, 所以固件写入和CRC读取都从0x08080000开始. */
static uint32_t    s_standby_addr = BANK2_BASE_ADDR;  /* 0x08080000 */

static void enable_backup_access(void)
{
    __HAL_RCC_PWR_CLK_ENABLE();
    HAL_PWR_EnableBkUpAccess();
}

static uint32_t read_bkp(uint32_t reg)
{
    return HAL_RTCEx_BKUPRead(&hrtc, reg);
}

static void write_bkp(uint32_t reg, uint32_t val)
{
    HAL_RTCEx_BKUPWrite(&hrtc, reg, val);
}

static int erase_standby_bank(void)
{
    FLASH_EraseInitTypeDef erase;
    uint32_t sector_error;
    uint8_t  active = BANK_GetActiveBank();

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

static int write_flash(uint32_t offset, const uint8_t *data, uint16_t len)
{
    uint32_t addr = s_standby_addr + offset;
    HAL_FLASH_Unlock();
    for (uint32_t i = 0; i < len; i++)
    {
        if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_BYTE,
                               addr + i, (uint64_t)data[i]) != HAL_OK)
        {
            printf("[OTA] Flash write FAILED at %lu\r\n", offset + i);
            HAL_FLASH_Lock();
            return -1;
        }
    }
    HAL_FLASH_Lock();
    return 0;
}

static uint32_t compute_hw_crc32(uint32_t len)
{
    uint32_t full_words  = len / 4;
    uint32_t remain      = len % 4;
    uint32_t total_words = full_words + (remain > 0 ? 1 : 0);
    const uint32_t *p = (const uint32_t *)s_standby_addr;

    __HAL_CRC_DR_RESET(&hcrc);

    for (uint32_t i = 0; i < total_words; i++)
    {
        uint32_t word;
        if (i < full_words)
        {
            word = p[i];
        }
        else
        {
            uint8_t buf[4] = {0xFF, 0xFF, 0xFF, 0xFF};
            const uint8_t *src = (const uint8_t *)&p[i];
            memcpy(buf, src, remain);
            memcpy(&word, buf, 4);
        }
        hcrc.Instance->DR = word;
    }
    return hcrc.Instance->DR;
}

void OTA_Init(void)
{
    s_state           = OTA_STATE_IDLE;
    s_next_seq        = 0;
    s_write_off       = 0;
    s_total_size      = 0;
    s_expected_crc    = 0;
    g_ota_in_progress = 0;
}

void OTA_BootCheck(void)
{
    enable_backup_access();

    uint32_t magic = read_bkp(RTC_BKP_DR_OTA_MAGIC);
    if (magic != OTA_MAGIC_PENDING)
        return;

    uint32_t trials = read_bkp(RTC_BKP_DR_OTA_TRIALS);
    trials++;
    write_bkp(RTC_BKP_DR_OTA_TRIALS, trials);

    printf("[OTA] TRIAL BOOT #%lu/%d (Bank %d)\r\n",
           trials, OTA_MAX_TRIAL_BOOTS, BANK_GetActiveBank());

    if (trials > OTA_MAX_TRIAL_BOOTS)
    {
        printf("[OTA] ROLLBACK: switching back!\r\n");
        write_bkp(RTC_BKP_DR_OTA_MAGIC, OTA_MAGIC_CONFIRMED);
        write_bkp(RTC_BKP_DR_OTA_TRIALS, 0);
        HAL_Delay(100);
        BANK_SwitchAndReset();
    }
}

void OTA_ConfirmIfPending(void)
{
    uint32_t magic = read_bkp(RTC_BKP_DR_OTA_MAGIC);
    if (magic == OTA_MAGIC_PENDING)
    {
        printf("[OTA] 固件已确认\r\n");
        write_bkp(RTC_BKP_DR_OTA_MAGIC, OTA_MAGIC_CONFIRMED);
        write_bkp(RTC_BKP_DR_OTA_TRIALS, 0);
    }
}

void OTA_FeedWatchdog(void)
{
    HAL_IWDG_Refresh(&hiwdg);
}

int OTA_Begin(uint32_t total_size, uint32_t crc32)
{
    if (s_state != OTA_STATE_IDLE)
        return OTA_ERR_BUSY;

    if (!BANK_IsDualBankMode())
        return OTA_ERR_NOT_DUAL_BANK;

    if (total_size == 0 || total_size > BANK_SIZE)
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
    if (s_state != OTA_STATE_RECEIVING)
        return OTA_ERR_BUSY;
    if (!data || len == 0)
        return OTA_ERR_FLASH;
    if (seq != s_next_seq)
        return OTA_ERR_SEQ;
    if (s_write_off + len > s_total_size)
        return OTA_ERR_OVERFLOW;

    if (write_flash(s_write_off, data, len) != 0)
        return OTA_ERR_FLASH;

    s_write_off += len;
    s_next_seq++;

    if ((seq % 50) == 0)
        printf("[OTA] seq=%u %lu/%lu\r\n", seq, s_write_off, s_total_size);

    return OTA_OK;
}

int OTA_End(uint32_t crc32)
{
    if (s_state != OTA_STATE_RECEIVING)
        return OTA_ERR_BUSY;

    s_state = OTA_STATE_VERIFYING;

    if (s_write_off != s_total_size)
    {
        s_state = OTA_STATE_IDLE;
        g_ota_in_progress = 0;
        return OTA_ERR_SIZE_MISMATCH;
    }

    if (crc32 != s_expected_crc)
    {
        s_state = OTA_STATE_IDLE;
        g_ota_in_progress = 0;
        return OTA_ERR_CRC;
    }

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

    enable_backup_access();
    write_bkp(RTC_BKP_DR_OTA_MAGIC, OTA_MAGIC_PENDING);
    write_bkp(RTC_BKP_DR_OTA_TRIALS, 0);
    write_bkp(RTC_BKP_DR_OTA_SIZE, s_total_size);
    write_bkp(RTC_BKP_DR_OTA_CRC, s_expected_crc);

    s_state = OTA_STATE_DONE;
    g_ota_in_progress = 0;
    HAL_Delay(100);
    BANK_SwitchAndReset();

    return OTA_OK;
}

void OTA_GetStatus(OtaStatusResp_t *out)
{
    if (!out) return;
    out->active_bank    = BANK_GetActiveBank();
    out->ota_state      = (uint8_t)s_state;
    out->trial_count    = (uint16_t)read_bkp(RTC_BKP_DR_OTA_TRIALS);
    out->received_bytes = s_write_off;
    out->total_bytes    = s_total_size;
    out->standby_bank   = BANK2_BASE_ADDR;  /* 备用区地址固定 0x08080000 */
}
