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
#include "fatfs.h"
#include "lvgl.h"
#include "gui_guider.h"

/* 上次选中的用户编号，用于从详情页返回时恢复焦点 */
uint8_t s_last_user_no = 0;

/* 用户详情页UI展示缓存数组，与 device_list[] 索引一一对应 */
user_detail_cache_t user_detail_cache[MAX_DEVICES] = {0};

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

    if(fs_mutex) osMutexAcquire(fs_mutex, osWaitForever);

    /* 尝试打开文件（仅读，不创建） */
    res = f_open(&SDFile, filepath, FA_OPEN_EXISTING | FA_READ);
    if (res == FR_OK)
    {
        house_info_t info;
        parse_addr((uint8_t *)dev_addr, &info);
        printf("用户数据文件已存在: %s (楼栋%d 单元%d 房间%d)\r\n",
                        filepath, info.building, info.unit, info.room);
        /* 文件已存在，直接关闭跳过 */
        f_close(&SDFile);
        if(fs_mutex) osMutexRelease(fs_mutex);
        return 0;
    }

    /* 文件不存在，创建新文件并写入默认数据 */
    res = f_open(&SDFile, filepath, FA_CREATE_NEW | FA_WRITE);
    if (res != FR_OK)
    {
        /* 可能被并发创建了，再试一次读取 */
        res = f_open(&SDFile, filepath, FA_OPEN_EXISTING | FA_READ);
        if (res == FR_OK)
        {
            f_close(&SDFile);
            if(fs_mutex) osMutexRelease(fs_mutex);
            return 0;
        }
        printf("用户数据文件创建失败: %s, err=%d\r\n", filepath, res);
        if(fs_mutex) osMutexRelease(fs_mutex);
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
    default_data.last_energy_read    = 0;     /* 搜索前暂存的上次读取到的从机累计用电量 */
    default_data.half_day_energy_wh = 0.0f;  /* 搜索前暂存的半日累积用电量0Wh */
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
        //数据最新更新事件等于创建时间
        default_data.update_time = default_data.create_time;
    }

    /* 写入文件 */
    res = f_write(&SDFile, &default_data, sizeof(default_data), &bw);
    f_close(&SDFile);
    if(fs_mutex) osMutexRelease(fs_mutex);

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

    if(fs_mutex) osMutexAcquire(fs_mutex, osWaitForever);
    res = f_open(&SDFile, filepath, FA_OPEN_EXISTING | FA_READ);
    if (res != FR_OK)
    {
        if(fs_mutex) osMutexRelease(fs_mutex);
        return -3;
    }

    res = f_read(&SDFile, data, sizeof(user_data_file_t), &br);
    f_close(&SDFile);
    if(fs_mutex) osMutexRelease(fs_mutex);

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
//将RAM中的用户数据写入SD卡
int write_user_data(const uint8_t *dev_addr, user_data_file_t *data)
{
    FRESULT res;
    char filepath[40];
    UINT bw;

    if (dev_addr == NULL || data == NULL)
    {
        return -1;
    }

    if (build_filepath(dev_addr, filepath) != 0)
    {
        return -2;
    }

    if(fs_mutex) osMutexAcquire(fs_mutex, osWaitForever);
    /* 打开已存在的文件并覆盖写入 */
    res = f_open(&SDFile, filepath, FA_OPEN_EXISTING | FA_WRITE);
    if (res != FR_OK)
    {
        if(fs_mutex) osMutexRelease(fs_mutex);
        return -3;
    }

    res = f_write(&SDFile, data, sizeof(user_data_file_t), &bw);
    f_close(&SDFile);
    if(fs_mutex) osMutexRelease(fs_mutex);

    if (res != FR_OK || bw != sizeof(user_data_file_t))
    {
        return -3;
    }

    return 0;
}

/* ================== 缓存管理 ================== */

/**
 * @brief  从SD卡加载所有已入网设备的用电量数据到RAM缓存
 * @note   只在上电初始化后调用
 *         仅加载已入网设备(valid==1)的数据，跳过未入网设备
 */
void user_detail_cache_init(void)
{
    uint16_t loaded = 0;

    /* 先清空整个缓存 */
    memset(user_detail_cache, 0, sizeof(user_detail_cache));

    for (uint16_t i = 0; i < device_count; i++)
    {
        /* 跳过未入网的设备 */
        if (device_list[i].state.bits.valid == 0) continue;

        /* 从SD卡读取用户数据文件 */
        user_data_file_t file_data;
        if (read_user_data(device_list[i].addr, &file_data) == 0)
        {
            /* 读取成功，填充缓存 */
            user_detail_cache[i].daily_energy   = file_data.daily_energy;
            user_detail_cache[i].monthly_energy = file_data.monthly_energy;
            user_detail_cache[i].annual_energy  = file_data.annual_energy;
            user_detail_cache[i].total_energy   = file_data.total_energy;
            memcpy(user_detail_cache[i].weekly_energy, file_data.weekly_energy,
                   sizeof(file_data.weekly_energy));
            memcpy(&user_detail_cache[i].update_time, &file_data.update_time,
                   sizeof(file_data.update_time));
            loaded++;
        }
    }

    printf("用户详情缓存初始化完成: 已加载%d/%u个设备\r\n", loaded, device_count);
}

/* ================== UI回调函数实现 ================== */

/**
 * @brief  用户列表项点击回调函数
 * @note   user_data传入device_list索引(uint32_t)
 *         点击后切换到用户详情页，直接从RAM缓存读取用电量数据显示在表格中
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

    /* 从RAM缓存读取用户数据（不再每次读SD卡） */
    user_detail_cache_t *cache = &user_detail_cache[idx];

    /* 无动画直接切换到用户详情页
     * is_clean=false: 不在回调内清理旧屏幕(避免在子对象回调中删除父对象导致白屏)
     * auto_del=true: 由LVGL在屏幕切换完成后自动释放旧屏幕 */
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

    /* 检查缓存是否有有效数据（total_energy非0或update_time非全0表示有数据） */
    if (cache->total_energy != 0.0f ||
        cache->update_time.year != 0 || cache->update_time.month != 0)
    {
        /* 缓存有效，显示日/月/年/总累积用电量 (2行x4列表格) */
        snprintf(buf, sizeof(buf), "%.2f", cache->daily_energy);
        lv_table_set_cell_value(guider_ui.screen_user_detail_table_1, 1, 0, buf);

        snprintf(buf, sizeof(buf), "%.2f", cache->monthly_energy);
        lv_table_set_cell_value(guider_ui.screen_user_detail_table_1, 1, 1, buf);

        snprintf(buf, sizeof(buf), "%.2f", cache->annual_energy);
        lv_table_set_cell_value(guider_ui.screen_user_detail_table_1, 1, 2, buf);

        snprintf(buf, sizeof(buf), "%.2f", cache->total_energy);
        lv_table_set_cell_value(guider_ui.screen_user_detail_table_1, 1, 3, buf);

        snprintf(buf, sizeof(buf), "20%02d年%02d月%02d日 %02d:%02d:%02d  ",
        cache->update_time.year, cache->update_time.month, cache->update_time.day,
        cache->update_time.hours, cache->update_time.minutes, cache->update_time.seconds);

        lv_label_set_text(guider_ui.screen_user_detail_label_time, buf);

        /* 填充七日用电量柱形图数据 */
        if (guider_ui.screen_user_detail_chart != NULL)
        {
            lv_chart_series_t *ser = lv_chart_get_series_next(guider_ui.screen_user_detail_chart, NULL);
            if (ser != NULL)
            {
                /* 找出七日中的最大值，用于动态调整Y轴范围 */
                float max_val = 0.0f;
                for (int i = 0; i < 7; i++)
                {
                    if (cache->weekly_energy[i] > max_val)
                        max_val = cache->weekly_energy[i];
                }

                /* 设置Y轴范围: 最大值留20%余量，最小范围0~5(即50) */
                int16_t y_max = (int16_t)(max_val * 10.0f * 1.2f);  /* kWh * 10, 留20%余量 */
                if (y_max < 50) y_max = 50;  /* 最小显示范围 0~5.0 kWh */
                lv_chart_set_range(guider_ui.screen_user_detail_chart, LV_CHART_AXIS_PRIMARY_Y, 0, y_max);

                /* 填充数据: weekly_energy[0]=7天前...[6]=最新(当天)，乘10转整数 */
                for (int i = 0; i < 7; i++)
                {
                    int16_t val = (int16_t)(cache->weekly_energy[i] * 10.0f);
                    ser->y_points[i] = val;
                }
                lv_chart_refresh(guider_ui.screen_user_detail_chart);
            }
        }
    }
    else
    {
        /* 缓存无数据（设备未入网或SD卡无文件），显示默认/错误信息 */
        lv_table_set_cell_value(guider_ui.screen_user_detail_table_1, 1, 0, "--");
        lv_table_set_cell_value(guider_ui.screen_user_detail_table_1, 1, 1, "--");
        lv_table_set_cell_value(guider_ui.screen_user_detail_table_1, 1, 2, "--");
        lv_table_set_cell_value(guider_ui.screen_user_detail_table_1, 1, 3, "--");
    }
}
