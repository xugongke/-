/**
 * @file    user_data_manager.c
 * @brief   用户数据文件管理模块实现
 * @note    在SD卡的USER目录下，以"楼栋_单元_房间.bin"为文件名，
 *          为每个用户创建独立的二进制数据文件。
 */
#include "user_data_manager.h"
#include "device_manager.h"
#include <string.h>
#include <stdio.h>
#include "ff.h"
#include "lvgl.h"
#include "gui_guider.h"

/* 上次选中的用户编号，用于从详情页返回时恢复焦点 */
uint8_t s_last_user_no = 0;

/* ================== 内部函数声明 ================== */

/**
 * @brief  根据通信地址生成文件路径: "0:/USER/楼栋_单元_房间.bin"
 * @param  dev_addr: 通信地址（6字节）
 * @param  path: 输出路径缓冲区（至少40字节）
 * @retval 0: 成功, -1: 地址无效
 */
static int build_filepath(const uint8_t *dev_addr, char *path)
{
    house_info_t info;
    parse_addr((uint8_t *)dev_addr, &info);

    /* 检查地址是否全0（未设置） */
    if (info.building == 0 && info.unit == 0 && info.room == 0)
    {
        return -1;
    }

    snprintf(path, 40, "%s/%d_%d_%04d.bin",
             USER_DATA_DIR, info.building, info.unit, info.room);

    return 0;
}

/**
 * @brief  确保USER目录存在，不存在则创建
 * @retval FR_OK: 成功, 其他: FatFS错误码
 */
static FRESULT ensure_user_dir(void)
{
    FRESULT res;
    static FILINFO fno;

    /* 用 f_stat 检查目录是否存在 */
    res = f_stat(USER_DATA_DIR, &fno);
    if (res == FR_OK)
    {
        return FR_OK;  /* 目录已存在 */
    }

    /* 目录不存在，尝试创建 */
    res = f_mkdir(USER_DATA_DIR);
    if (res == FR_OK)
    {
        printf("USER数据目录创建成功: %s\r\n", USER_DATA_DIR);
    }
    else if (res == FR_EXIST)
    {
        res = FR_OK;  /* 可能被其他任务抢先创建了 */
    }
    else
    {
        printf("USER数据目录创建失败: %d\r\n", res);
    }

    return res;
}

/* ================== 对外接口实现 ================== */

int ensure_user_data_file(const uint8_t *dev_addr, const uint8_t *mac)
{
    static FIL file;
    FRESULT res;
    char filepath[40];
    UINT bw;

    if (dev_addr == NULL || mac == NULL)
    {
        return -1;
    }

    /* 生成文件路径 */
    if (build_filepath(dev_addr, filepath) != 0)
    {
        printf("用户数据: 通信地址无效(全0)\r\n");
        return -2;
    }

    /* 确保USER目录存在 */
    if (ensure_user_dir() != FR_OK)
    {
        return -3;
    }

    /* 尝试打开文件（仅读，不创建） */
    res = f_open(&file, filepath, FA_OPEN_EXISTING | FA_READ);
    if (res == FR_OK)
    {
				house_info_t info;
				parse_addr((uint8_t *)dev_addr, &info);
				printf("用户数据文件已存在: %s (楼栋%d 单元%d 房间%d)\r\n",
							 filepath, info.building, info.unit, info.room);
        /* 文件已存在，直接关闭跳过 */
        f_close(&file);
        return 0;
    }

    /* 文件不存在，创建新文件并写入默认数据 */
    res = f_open(&file, filepath, FA_CREATE_NEW | FA_WRITE);
    if (res != FR_OK)
    {
        /* 可能被并发创建了，再试一次读取 */
        res = f_open(&file, filepath, FA_OPEN_EXISTING | FA_READ);
        if (res == FR_OK)
        {
            f_close(&file);
            return 0;
        }
        printf("用户数据文件创建失败: %s, err=%d\r\n", filepath, res);
        return -3;
    }

    /* 构造默认用户数据 */
    house_info_t info;
    user_data_file_t default_data;
    RX8025T_DateTimeCompact rtc_now;

    parse_addr((uint8_t *)dev_addr, &info);

    memset(&default_data, 0, sizeof(default_data));
    default_data.magic          = USER_DATA_MAGIC;
    default_data.version        = USER_DATA_VERSION;
    default_data.reserved1      = 0;
    memcpy(default_data.mac, mac, 6);
    default_data.building       = info.building;
    default_data.unit           = info.unit;
    default_data.room           = info.room;
    default_data.temperature    = 25.0f;     /* 默认温度25℃ - 内部使用 */
    default_data.power          = 0.0f;      /* 默认功率0W - 内部使用 */
    default_data.daily_energy   = 0.0f;      /* 日累积用电量0kWh */
    default_data.monthly_energy = 0.0f;      /* 月累积用电量0kWh */
    default_data.annual_energy  = 0.0f;      /* 年累积用电量0kWh */
    default_data.total_energy   = 0.0f;      /* 总累积用电量0kWh */

    /* 读取RTC时间作为创建时间和更新时间 */
    if (RX8025T_GetDateTime(&rtc_now) == HAL_OK)
    {
        default_data.create_time.year    = rtc_now.year;
        default_data.create_time.month   = rtc_now.month;
        default_data.create_time.day     = rtc_now.day;
        default_data.create_time.hours   = rtc_now.hours;
        default_data.create_time.minutes = rtc_now.minutes;
        default_data.create_time.seconds = rtc_now.seconds;
        default_data.update_time = default_data.create_time;
    }

    /* 写入文件 */
    res = f_write(&file, &default_data, sizeof(default_data), &bw);
    f_close(&file);

    if (res != FR_OK || bw != sizeof(default_data))
    {
        printf("用户数据文件写入失败: %s\r\n", filepath);
        /* 写入失败，删除损坏文件 */
        f_unlink(filepath);
        return -3;
    }

    printf("用户数据文件创建成功: %s (楼栋%d 单元%d 房间%d)\r\n",
           filepath, info.building, info.unit, info.room);

    return 0;
}

int read_user_data(const uint8_t *dev_addr, user_data_file_t *data)
{
    static FIL file;
    FRESULT res;
    char filepath[40];
    UINT br;

    if (dev_addr == NULL || data == NULL)
    {
        return -1;
    }

    if (build_filepath(dev_addr, filepath) != 0)
    {
        return -2;
    }

    res = f_open(&file, filepath, FA_OPEN_EXISTING | FA_READ);
    if (res != FR_OK)
    {
        return -3;
    }

    res = f_read(&file, data, sizeof(user_data_file_t), &br);
    f_close(&file);

    if (res != FR_OK || br != sizeof(user_data_file_t))
    {
        return -3;
    }

    /* 校验魔数 */
    if (data->magic != USER_DATA_MAGIC)
    {
        printf("用户数据文件校验失败: %s\r\n", filepath);
        return -3;
    }

    return 0;
}

int write_user_data(const uint8_t *dev_addr, user_data_file_t *data)
{
    static FIL file;
    FRESULT res;
    char filepath[40];
    UINT bw;
    RX8025T_DateTimeCompact rtc_now;

    if (dev_addr == NULL || data == NULL)
    {
        return -1;
    }

    if (build_filepath(dev_addr, filepath) != 0)
    {
        return -2;
    }

    /* 读取RTC时间作为更新时间 */
    if (RX8025T_GetDateTime(&rtc_now) == HAL_OK)
    {
        data->update_time.year    = rtc_now.year;
        data->update_time.month   = rtc_now.month;
        data->update_time.day     = rtc_now.day;
        data->update_time.hours   = rtc_now.hours;
        data->update_time.minutes = rtc_now.minutes;
        data->update_time.seconds = rtc_now.seconds;
    }

    /* 打开已存在的文件并覆盖写入 */
    res = f_open(&file, filepath, FA_OPEN_EXISTING | FA_WRITE);
    if (res != FR_OK)
    {
        return -3;
    }

    res = f_write(&file, data, sizeof(user_data_file_t), &bw);
    f_close(&file);

    if (res != FR_OK || bw != sizeof(user_data_file_t))
    {
        return -3;
    }

    return 0;
}

/* ================== UI回调函数实现 ================== */

/**
 * @brief  用户列表项点击回调函数
 * @note   user_data传入device_list索引(uint32_t)
 *         点击后切换到用户详情页，并从SD卡读取温度/功率/用电量显示在表格中
 */
void user_list_item_event_handler(lv_event_t *e)
{
    uint32_t idx = (uint32_t)(uintptr_t)lv_event_get_user_data(e);
    char buf[64];

    /* 记住编号，用于返回列表页时恢复焦点 */
    s_last_user_no = (uint8_t)idx;

    /* 索引有效性检查 */
    if (idx >= device_count)
    {
        printf("用户列表回调: 索引%lu超出范围(共%u)\r\n", (unsigned long)idx, device_count);
        return;
    }

    /* 读取用户数据文件 */
    user_data_file_t user_data;
    int ret = read_user_data(device_list[idx].addr, &user_data);

    /* 切换到用户详情页 */
    ui_load_scr_animation(&guider_ui, &guider_ui.screen_user_detail,
                          guider_ui.screen_user_detail_del,
                          &guider_ui.screen_user_list_del,
                          setup_scr_screen_user_detail,
                          LV_SCR_LOAD_ANIM_NONE, 10, 10, true, true);

    /* 设置用户地址标签: "X楼 X单元 XXXX" */
    house_info_t house;
    parse_addr(device_list[idx].addr, &house);
    snprintf(buf, sizeof(buf), "%d楼 %d单元 %04d", house.building, house.unit, house.room);
    lv_label_set_text(guider_ui.screen_user_detail_label_user, buf);

    if (ret == 0)
    {
        /* 文件读取成功，显示日/月/年累积用电量 */
        snprintf(buf, sizeof(buf), "%.2f kWh ", user_data.daily_energy);
        lv_table_set_cell_value(guider_ui.screen_user_detail_table_1, 1, 1, buf);

        snprintf(buf, sizeof(buf), "%.2f kWh ", user_data.monthly_energy);
        lv_table_set_cell_value(guider_ui.screen_user_detail_table_1, 2, 1, buf);

        snprintf(buf, sizeof(buf), "%.2f kWh ", user_data.annual_energy);
        lv_table_set_cell_value(guider_ui.screen_user_detail_table_1, 3, 1, buf);
			
				snprintf(buf, sizeof(buf), "20%02d年%02d月%02d日 %02d时%02d分%02d秒  ",
				user_data.update_time.year,user_data.update_time.month,user_data.update_time.day,user_data.update_time.hours,user_data.update_time.minutes,user_data.update_time.seconds);
				
				lv_label_set_text(guider_ui.screen_user_detail_label_time,buf);
    }
    else
    {
        /* 文件不存在或读取失败，显示默认/错误信息 */
        lv_table_set_cell_value(guider_ui.screen_user_detail_table_1, 1, 1, "-- kWh ");
        lv_table_set_cell_value(guider_ui.screen_user_detail_table_1, 2, 1, "-- kWh ");
        lv_table_set_cell_value(guider_ui.screen_user_detail_table_1, 3, 1, "-- kWh ");
    }
}
