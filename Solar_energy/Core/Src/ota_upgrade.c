/**
  ******************************************************************************
  * @file    ota_upgrade.c
  * @brief   Dual-bank firmware upgrade manager (STM32F429IGT6)
  *
  * OTA擦除策略 (解决双Bank模式下SNB 8-15不能擦除的硬件限制):
  *   - 单Bank模式(DB1M=0): 擦 sector 8-11 (SNB 8-11), 标准 HAL 即可
  *   - 双Bank+BFB2=1: 备用区=物理Bank1(SNB 0-7), SNB 0-7 始终能擦!
  *   - 双Bank+BFB2=0: 上电时切回单Bank模式(为下次OTA做准备)
  *
  * 交替更新流程:
  *   第1次: 单Bank擦sector8-11 → 写 → SetBankMode(1,1) → Bank2启动
  *   第2次: BFB2=1擦SNB0-7 → 写 → SwitchAndReset → Bank1启动 → 上电切单Bank
  *   第3次: 同第1次...
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
static uint32_t    s_standby_addr = 0;

/* 备用区CPU地址始终是 0x08080000:
 *   单Bank: 0x08080000 = sector 8-11
 *   BFB2=0: 0x08080000 = 物理Bank2
 *   BFB2=1: 0x08080000 = 物理Bank1 (地址翻转) */
static uint32_t get_standby_bank_addr(void)
{
    return BANK2_BASE_ADDR;  /* 0x08080000 */
}

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

/* ==================== 擦除 ==================== */

/**
 * @brief 单Bank模式下擦除 sector 8-11 (标准 HAL, 可靠)
 */
static int erase_single_bank(void)
{
    FLASH_EraseInitTypeDef erase;
    uint32_t sector_error;

    erase.TypeErase    = FLASH_TYPEERASE_SECTORS;
    erase.Banks        = FLASH_BANK_1;
    erase.Sector       = FLASH_SECTOR_8;
    erase.NbSectors    = 4;  /* sector 8,9,10,11 = 4×128KB = 512KB */
    erase.VoltageRange = FLASH_VOLTAGE_RANGE_3;

    HAL_FLASH_Unlock();
    __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_OPERR | FLASH_FLAG_WRPERR |
                           FLASH_FLAG_PGAERR | FLASH_FLAG_PGPERR | FLASH_FLAG_PGSERR);
    HAL_StatusTypeDef status = HAL_FLASHEx_Erase(&erase, &sector_error);
    HAL_FLASH_Lock();

    if (status != HAL_OK)
    {
        printf("[OTA] Single-bank erase FAILED status=%d err=0x%lX\r\n", status, sector_error);
        return -1;
    }
    printf("[OTA] Single-bank erase OK (sector 8-11)\r\n");
    return 0;
}

/**
 * @brief 双Bank+BFB2=1 模式下擦除物理Bank1 (SNB 0-7, 共8个扇区=512KB)
 * @note  SNB 0-7 始终能擦除(不管BFB2). 关中断+一次性写CR确保STRT生效.
 */
static int erase_dual_bank_bfb2(void)
{
    printf("[OTA] Dual-bank erase: SNB 0-7 (physical Bank1 = standby)\r\n");

    /* 解锁 Flash */
    if (FLASH->CR & FLASH_CR_LOCK)
    {
        FLASH->KEYR = 0x45670123U;
        FLASH->KEYR = 0xCDEF89ABU;
    }
    if (FLASH->CR & FLASH_CR_LOCK)
    {
        printf("[OTA] Flash unlock FAILED\r\n");
        return -1;
    }

    FLASH->SR = 0x0000C3FFU;  /* 清除所有标志 */

    uint32_t primask = __get_PRIMASK();
    __disable_irq();

    /* 逐 sector 擦除 (sector 0-7, 每个16K/64K/128K, 共512KB) */
    for (uint32_t sector = 0; sector <= 7; sector++)
    {
        FLASH->CR &= ~(FLASH_CR_PSIZE | FLASH_CR_SNB | FLASH_CR_SER |
                       FLASH_CR_MER | FLASH_CR_STRT);
        FLASH->CR = FLASH_PSIZE_WORD | FLASH_CR_SER |
                    (sector << FLASH_CR_SNB_Pos) | FLASH_CR_STRT;

        uint32_t to = 4000000U;
        while ((FLASH->SR & FLASH_SR_BSY) && (to-- > 0)) { }

        uint32_t sr = FLASH->SR;
        FLASH->CR &= ~(FLASH_CR_SER | FLASH_CR_SNB | FLASH_CR_STRT);
        FLASH->SR = sr;

        if (!primask) { __enable_irq(); OTA_FeedWatchdog(); __disable_irq(); }

        if (sr & (FLASH_SR_WRPERR | FLASH_SR_PGSERR))
        {
            printf("[OTA] sector %lu erase ERROR SR=0x%08lX\r\n",
                   (unsigned long)sector, (unsigned long)sr);
            if (!primask) __enable_irq();
            FLASH->CR |= FLASH_CR_LOCK;
            return -1;
        }
    }

    if (!primask) __enable_irq();
    FLASH->CR |= FLASH_CR_LOCK;

    printf("[OTA] Dual-bank erase OK (SNB 0-7)\r\n");
    return 0;
}

/**
 * @brief 擦除备用区 (根据当前Bank模式选择策略)
 */
static int erase_standby_bank(void)
{
    if (!BANK_IsDualBankMode())
    {
        /* 单Bank: 擦 sector 8-11 */
        return erase_single_bank();
    }
    else
    {
        /* 双Bank+BFB2=1: 擦 SNB 0-7 (物理Bank1=备用区) */
        return erase_dual_bank_bfb2();
    }
}

/* ==================== 编程 ==================== */

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

/* ==================== CRC32 ==================== */

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

/* ==================== OTA 状态机 ==================== */

void OTA_Init(void)
{
    s_state           = OTA_STATE_IDLE;
    s_next_seq        = 0;
    s_write_off       = 0;
    s_total_size      = 0;
    s_expected_crc    = 0;
    s_standby_addr    = 0;
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

/**
 * @brief 上电时检测Bank模式, 确保处于可OTA的状态
 * @note  在 main() 初始化后调用. 如果双Bank+BFB2=0(刚从Bank1启动),
 *        切换到单Bank模式, 为下次OTA做准备.
 *        应在 main() 的 USER CODE BEGIN 2 中, RTC初始化之后调用.
 */
void OTA_PowerOnCheck(void)
{
    enable_backup_access();

    /* 如果待确认(magic=PENDING), 保持双Bank以便回滚, 不切单Bank */
    uint32_t magic = read_bkp(RTC_BKP_DR_OTA_MAGIC);
    if (magic == OTA_MAGIC_PENDING)
        return;

    /* 已确认/初始状态: 如果双Bank+BFB2=0, 切单Bank以便下次OTA */
    if (BANK_IsDualBankMode() && BANK_GetActiveBank() == 1)
    {
        printf("[OTA] PowerOn: dual-bank BFB2=0 -> switch to single-bank\r\n");
        HAL_Delay(50);
        BANK_SetBankMode(0, 0);  /* 不返回 */
    }
}

void OTA_ConfirmIfPending(void)
{
    uint32_t magic = read_bkp(RTC_BKP_DR_OTA_MAGIC);
    if (magic == OTA_MAGIC_PENDING)
    {
        printf("[OTA] Firmware confirmed\r\n");
        write_bkp(RTC_BKP_DR_OTA_MAGIC, OTA_MAGIC_CONFIRMED);
        write_bkp(RTC_BKP_DR_OTA_TRIALS, 0);

        /* 确认后, 如果双Bank+BFB2=0, 切单Bank为下次OTA做准备 */
        if (BANK_IsDualBankMode() && BANK_GetActiveBank() == 1)
        {
            printf("[OTA] Confirmed -> switch to single-bank\r\n");
            HAL_Delay(50);
            BANK_SetBankMode(0, 0);  /* 不返回 */
        }
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

    if (total_size == 0 || total_size > BANK_SIZE)
        return OTA_ERR_OVERFLOW;

    s_total_size   = total_size;
    s_expected_crc = crc32;
    s_standby_addr = get_standby_bank_addr();
    s_next_seq     = 0;
    s_write_off    = 0;

    printf("[OTA] BEGIN size=%lu crc=0x%08lX standby=0x%08lX %s\r\n",
           total_size, crc32, s_standby_addr,
           BANK_IsDualBankMode() ? "(dual-bank BFB2=1)" : "(single-bank)");

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

    /* 根据 Bank 模式选择切换方式 */
    if (!BANK_IsDualBankMode())
    {
        /* 单Bank -> 双Bank + BFB2=1 (从Bank2启动新固件) */
        printf("[OTA] Switching: single-bank -> dual-bank BFB2=1\r\n");
        BANK_SetBankMode(1, 1);  /* 不返回 */
    }
    else
    {
        /* 双Bank BFB2=1 -> BFB2=0 (从Bank1启动新固件) */
        printf("[OTA] Switching: BFB2=1 -> BFB2=0\r\n");
        BANK_SwitchAndReset();  /* 不返回 */
    }

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
    out->standby_bank   = get_standby_bank_addr();
}