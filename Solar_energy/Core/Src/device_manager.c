#include "device_manager.h"
#include <string.h>
#include <stdio.h>
#include "es1642.h"
#include "es1642_usage_guide.h"
#include "user_data_manager.h"
#include "a7680c_mqtt.h"
#include "cmsis_os.h"
#include "fatfs.h"
#include "mppt.h"

// ================== 从机命令定义 ==================
#define SLAVE_CMD_SET_ADDR    0x01  // 修改通信地址命令
#define SLAVE_CMD_HEATER_ON   0x02  // 启动加热
#define SLAVE_CMD_HEATER_OFF  0x03  // 停止加热
#define SLAVE_CMD_READ_STATUS 0x04  // 读取从机状态(温度+电压+状态)

// ================== 从机响应结果码 ==================
#define SLAVE_RESULT_OK       0x01  // 操作成功
#define SLAVE_RESULT_FAIL     0x00  // 操作失败

// ================== 全局变量 ==================
device_t device_list[MAX_DEVICES];
uint16_t device_count = 0;

/* 设备管理模式标志: 1=处于管理模式(从机轮询暂停), 0=正常轮询
 * 由上位机CMD_DEVICE_MANAGE_MODE(0x02)命令设置, TCP断开时自动清零 */
volatile uint8_t g_device_manage_mode = 0;

// 运行时临时数据 (daily_energy_wh 已并入 device_t; last_energy_read 仍为独立数组)
uint32_t last_energy_read[MAX_DEVICES];  // 各设备上次从从机读取的累计用电量(Wh), 差值计算用

// 标志：设备是否有变化（用于减少SD写入）
static uint8_t device_changed = 0;

/* 告警预扫描全局数据 */
alert_stats_t g_alert_stats = {0};
alert_item_t  g_alert_items[ALERT_MAX_ITEMS] = {0};

/**
 * @brief  扫描所有设备状态, 统计告警并填充 g_alert_stats 和 g_alert_items
 * @note   在 device_poll_all_status() 之后调用, 不涉及LVGL API
 */
void alert_scan_devices(void)
{
    alert_stats_t stats = {0};
    uint8_t DC_count = 0;
    int item_idx = 0;

    for (uint16_t i = 0; i < device_count; i++)
    {
        /* 只统计已入网设备 */
        if (device_list[i].state.bits.valid == 0) continue;

        /* 通信异常 (bit2) */
        if (device_list[i].state.bits.comm_err)
        {
            stats.comm_cnt++;
            stats.err_total++;
            if (item_idx < ALERT_MAX_ITEMS)
            {
                g_alert_items[item_idx].dev_idx = i;
                g_alert_items[item_idx].type = ALERT_COMM_FAIL;
                item_idx++;
            }
        }

        /* 继电器异常 (bit5) */
        if (device_list[i].state.bits.relay_err)
        {
            stats.relay_cnt++;
            stats.err_total++;
            if (item_idx < ALERT_MAX_ITEMS)
            {
                g_alert_items[item_idx].dev_idx = i;
                g_alert_items[item_idx].type = ALERT_RELAY_ERR;
                item_idx++;
            }
        }

        /* 温度异常 (bit6) */
        if (device_list[i].state.bits.temp_err)
        {
            stats.temp_cnt++;
            stats.err_total++;
            if (item_idx < ALERT_MAX_ITEMS)
            {
                g_alert_items[item_idx].dev_idx = i;
                g_alert_items[item_idx].type = ALERT_TEMP_ERR;
                item_idx++;
            }
        }

        /* 电源反接 (bit7) */
        if (device_list[i].state.bits.power_reverse)
        {
            stats.power_cnt++;
            stats.err_total++;
            if (item_idx < ALERT_MAX_ITEMS)
            {
                g_alert_items[item_idx].dev_idx = i;
                g_alert_items[item_idx].type = ALERT_POWER_REVERSE;
                item_idx++;
            }
        }

        /* 统计正在加热的设备数量 (bit1) */
        if (device_list[i].state.bits.dc_heating)
        {
            DC_count++;
        }
    }

    stats.item_count = item_idx;
    g_alert_stats = stats;
    g_mppt.n_active = DC_count; /* 更新MPPT模块的正在加热设备数量 */
}

// ================== 初始化 ==================
void device_manager_init(void)
{
    // 上电时加载设备表
    if (load_devices() != FR_OK)
    {
        device_count = 0;
    }
}

// ================== 根据mac地址查找设备 ==================
int find_device_by_mac(uint8_t *mac)
{
    for (int i = 0; i < device_count; i++)
    {
        if (memcmp(device_list[i].mac, mac, 6) == 0)
        {
            return i;
        }
    }
    return -1;
}

// ================== 搜索设备时调用的添加设备函数，搜索设备前清空设备表 ==================
void add_device(uint8_t *mac, uint8_t *addr,uint8_t net_state)
{
		if (device_count >= MAX_DEVICES)
		{
				printf("设备已满，无法添加\r\n");
				return;
		}
		if(net_state == ES1642_NET_STATE_SAME_NETWORK)//如果已入网，代表之前设置过通信地址,bit0置1
		{
			memcpy(device_list[device_count].mac, mac, 6);
			memcpy(device_list[device_count].addr, addr, 6);
            //设备状态默认有效且已入网，其他状态位默认0，防止列表中之前的状态位影响到新添加的设备
			device_list[device_count].state.byte = 0x01;  // bit0=1: 已入网
			device_list[device_count].comm_fail_cnt = 0;
		}
		else//没入网那就先添加进设备表，等待入网
		{
			memcpy(device_list[device_count].mac, mac, 6);
			memcpy(device_list[device_count].addr, addr, 6);
			device_list[device_count].state.byte = 0x00;
			device_list[device_count].comm_fail_cnt = 0;
		}

		device_count++;
		device_changed = 1;
}

// ================== 电脑软件发送过来绑定命令时更新设备表中的通信地址 ==================
int update_device(uint8_t *mac, uint8_t *addr)
{
    int index = find_device_by_mac(mac);//查找小程序发来的要绑定的设备在设备表中的位置
		const uint8_t new_psk[ES1642_SET_PSK_LEN] = {0x11,0x12,0x13,0x14,0x15,0x16,0x17,0x18};//统一网络口令
		es1642_response_t response; // 用于接收从机响应数据
		int ret;
		
    if (index < 0) {return 1;}//设备列表不存在，请重新搜索设备
		
		if(device_list[index].state.bits.valid == 0)//如果是没入网，那就进行入网，入网了才能正常通信,下面的发送修改通信地址命令从机才能接收到
		{
            ret = ES1642_SetPsk(index, new_psk);
            if (ret == 0)
            {
                    printf("从机入网成功\r\n");
                    device_list[index].state.bits.valid = 1;
                    device_changed = 1;
            }
            else if (ret == -2)
            {
                    printf("从机入网响应超时\r\n");
                    return 5; // 入网超时
            }
            else if (ret == -3)
            {
                    printf("从机入网失败\r\n");
                    return 6; // 入网失败
            }
            else
            {
                    printf("发送入网命令失败\r\n");
                    return 7;
            }
		}

		// MAC已存在 → 判断通信地址是否变化，如果通信地址发生变化，就给从机发送设置通信地址命令
		if (memcmp(device_list[index].addr, addr, 6) != 0)//通信地址发生变化就更新一下列表,并给从机发送修改通信地址命令
		{
            /* 给从机发送命令：[cmd][data_len][data...]
                * cmd = 0x01 (修改通信地址), data_len = 6, data = 6字节新通信地址
                * 注意：这里用 ES1642_ADDR_LEN(6) 而不是 sizeof(addr)，因为 addr 是指针，sizeof 指针为 4
                */
            uint8_t cmd_buf[2 + ES1642_ADDR_LEN]; // cmd(1) + len(1) + data(6) = 8字节
            RX8025T_Time current_time;
            cmd_buf[0] = SLAVE_CMD_SET_ADDR;       // 命令: 修改通信地址
            cmd_buf[1] = ES1642_ADDR_LEN;           // 数据长度: 6
            RX8025T_GetTime(&current_time); // 获取当前时分秒，放到通信地址最后面
            addr[4] = current_time.minutes;
            addr[5] = current_time.seconds;
            memcpy(&cmd_buf[2], addr, ES1642_ADDR_LEN); // 数据: 新通信地址
            ret = ES1642_SendUserData(index, cmd_buf, sizeof(cmd_buf), 0, &response);

            if (ret == 0)
            {
                /* 从机响应格式: [cmd][data_len][data]
                    * cmd=0x01(修改地址), data_len=0x01, data=0x01(成功)/0x00(失败)
                    * 总共3字节
                    */
                if (response.data_len >= 3 &&
                        response.data[0] == SLAVE_CMD_SET_ADDR &&
                        response.data[1] == 0x01 &&
                        response.data[2] == SLAVE_RESULT_OK)
                {
                    printf("从机修改通信地址成功, 响应长度=%d\r\n", response.data_len);
                    memcpy(device_list[index].addr, addr, 6); // 通信地址修改成功，更新设备表
                    device_changed = 1;
                    /* 为新的通信地址创建数据文件,这里后面要加一个重复绑定判断 */
                    ensure_user_data_file(device_list[index].addr, device_list[index].mac);
                    /*把新的通信地址对应的文件更新到RAM*/
                    user_data_file_t file_data;
                    if (read_user_data(device_list[index].addr, &file_data) == 0)
                    {
                        /* 读取成功，填充缓存 */
                        user_detail_cache[index].unit = file_data.unit;
                        user_detail_cache[index].room = file_data.room;
                        user_detail_cache[index].daily_energy   = file_data.daily_energy;
                        user_detail_cache[index].monthly_energy = file_data.monthly_energy;
                        user_detail_cache[index].annual_energy  = file_data.annual_energy;
                        user_detail_cache[index].total_energy   = file_data.total_energy;
                        memcpy(user_detail_cache[index].weekly_energy, file_data.weekly_energy,
                            sizeof(file_data.weekly_energy));
                        memcpy(&user_detail_cache[index].update_time, &file_data.update_time,
                            sizeof(file_data.update_time));
                    }
                }
                else
                {
                    printf("从机修改通信地址失败\r\n");
                    return 2;
                }

            }
            else if (ret == -2)
            {
                /* 从机响应超时，通信地址可能没有修改成功 */
                printf("从机修改通信地址超时，请检查从机状态\r\n");
                return 3; // 从机响应超时
            }
            else
            {
                /* 发送失败 */
                printf("发送修改通信地址命令失败\r\n");
                return 4;
            }
		}
		return 0;
}



// ================== 清空设备表 ==================
void Clear_devices(void)
{
	memset(device_list,0,sizeof(device_list));  // daily_energy_wh 现为 device_t 字段, 随之一起清零
	memset(last_energy_read,0,sizeof(last_energy_read));
	memset(user_detail_cache,0,sizeof(user_detail_cache));
	device_count = 0;
}

// ================== 保存设备表 ==================
FRESULT save_devices(void)
{
    FRESULT res;
    UINT bw;

    // 没有变化就不写SD卡（减少磨损）
    if (!device_changed)
        return FR_OK;

    if(fs_mutex) osMutexAcquire(fs_mutex, osWaitForever);
		//不管文件是否存在，都直接创建新文件；如果已存在，就清空重写
    res = f_open(&SDFile, DEVICE_FILE, FA_CREATE_ALWAYS | FA_WRITE);
    if (res != FR_OK)
    {
        printf("打开文件失败\r\n");
        return res;
    }

    res = f_write(&SDFile, device_list, device_count * sizeof(device_t), &bw);

    if (res == FR_OK)
    {
        printf("保存成功，设备数:%d\r\n", device_count);
        device_changed = 0;
    }

    f_close(&SDFile);
    if(fs_mutex) osMutexRelease(fs_mutex);
    return res;
}

// ================== 加载设备表 ==================
FRESULT load_devices(void)
{
    FRESULT res;
    UINT br;

    if(fs_mutex) osMutexAcquire(fs_mutex, osWaitForever);
	//如果文件存在就打开，如果文件不存在就创建，但是并没有写权限
    res = f_open(&SDFile, DEVICE_FILE, FA_OPEN_ALWAYS | FA_READ);
    if (res != FR_OK)
    {
        printf("设备表文件打开失败\r\n");
        device_count = 0;
        return FR_NO_FILE;
    }

    res = f_read(&SDFile, device_list, sizeof(device_list), &br);
    if (res != FR_OK)
    {
        printf("设备表文档f_read失败\r\n");
    }

    f_close(&SDFile);
    if(fs_mutex) osMutexRelease(fs_mutex);
		//br是实际读取到的字节数,也就是用户真实的数量
    device_count = br / sizeof(device_t);

    /* daily_energy_wh 是运行时当日累积量, 不应跨重启携带(load_devices读出的是SD上的陈旧值), 清零 */
    for (uint16_t i = 0; i < device_count; i++)
    {
        device_list[i].daily_energy_wh = 0;
    }

//    for (int i = 0; i < device_count; i++)
//    {
//        // 初始化运行时临时错误数据
//        device_list[i].state.bits.comm_err = 1;
//        device_list[i].state.bits.relay_err = 1;
//        device_list[i].state.bits.temp_err = 1;
//        device_list[i].state.bits.power_reverse = 1;
//    }

    printf("加载设备数量:%d\r\n", device_count);

    return FR_OK;
}

/**
 * @brief 解析通信地址，获取楼栋、单元、房号
 * @param addr 6字节通信地址
 * @param info 输出结构体
 */
void parse_addr(uint8_t *addr, house_info_t *info)
{
    if (addr == NULL || info == NULL)
        return;

    info->building = addr[0];  // 楼栋号
    info->unit     = addr[1];  // 单元号

    // 房号（高字节在前）
    info->room = (addr[2] << 8) | addr[3];
}

/**
 * @brief 生成通信地址（户号编码）
 */
void make_addr(uint8_t *addr,
               uint8_t building,
               uint8_t unit,
               uint16_t room)
{
    if (!addr) return;

    addr[0] = building;
    addr[1] = unit;

    addr[2] = (room >> 8) & 0xFF;  // 高字节
    addr[3] = room & 0xFF;         // 低字节

    addr[4] = 0x00; // 预留
    addr[5] = 0x00; // 预留
}

// ================== 控制从机加热 ==================

/**
 * @brief 控制从机启动/停止加热
 * @param addr   从机通信地址 (6字节)
 * @param heater_on  1=启动加热, 0=停止加热
 * @return 0=成功, -1=参数错误, -2=从机超时, -3=从机拒绝, -4=发送失败
 *
 * 发送格式: [cmd][data_len][data]
 *   启动加热: cmd=0x02, data_len=0x01, data=0x01
 *   停止加热: cmd=0x03, data_len=0x01, data=0x00
 *
 * 从机响应: [cmd][data_len][result]
 *   result: 0x01=成功, 0x00=失败
 */
int device_ctrl_heater(int dev_index, uint8_t heater_on)
{
    es1642_response_t response;
    uint8_t cmd_buf[3];  /* [cmd][data_len][data] */
    int ret;

    if (dev_index < 0 || dev_index >= device_count) { return -1; }

    /* 组装命令帧 */
    if (heater_on)
    {
        cmd_buf[0] = SLAVE_CMD_HEATER_ON;   /* 命令字: 启动加热 */
        cmd_buf[1] = 0x01;                   /* 数据长度: 1字节 */
        cmd_buf[2] = 0x01;                   /* 数据: 1=启动 */
    }
    else
    {
        cmd_buf[0] = SLAVE_CMD_HEATER_OFF;  /* 命令字: 停止加热 */
        cmd_buf[1] = 0x01;                   /* 数据长度: 1字节 */
        cmd_buf[2] = 0x00;                   /* 数据: 0=停止 */
    }

    /* 通过ES1642载波发送给从机 */
    ret = ES1642_SendUserData(dev_index, cmd_buf, sizeof(cmd_buf), 0, &response);

    if (ret == 0)
    {
        /* 检查从机响应: [cmd][len][result] */
        uint8_t expected_cmd = heater_on ? SLAVE_CMD_HEATER_ON : SLAVE_CMD_HEATER_OFF;

        if (response.data_len >= 3 &&
            response.data[0] == expected_cmd &&
            response.data[1] == 0x01 &&
            response.data[2] == SLAVE_RESULT_OK)
        {
            printf("从机加热控制成功: %s\r\n", heater_on ? "启动加热" : "停止加热");
            return 0;
        }
        else
        {
            printf("从机加热控制失败, 响应异常\r\n");
            return -3;  /* 从机拒绝或响应异常 */
        }
    }
    else if (ret == -2)
    {
        printf("从机加热控制超时\r\n");
        return -2;  /* 从机响应超时 */
    }
    else
    {
        printf("发送加热控制命令失败\r\n");
        return -4;  /* 发送失败 */
    }
}
// ================== 每日零点：将RAM中累积电量写入SD卡 ==================

/**
 * @brief 将RAM中累积的daily_energy_wh写入各用户数据文件，并清零数组
 * @note  在每天零点调用（由RTC_Task检测日期变化触发）
 *        将 daily_energy_wh[i] 累加到 user_data_file_t 的日/月/年/总用电量
 *        同时处理日/月/年重置逻辑
 */
void daily_energy_flush_to_sd(void)
{
    if (device_count == 0) return;

    /* 获取当前RTC时间 */
    RX8025T_DateTimeCompact rtc_now;
    if (RX8025T_GetDateTime(&rtc_now) != HAL_OK)
    {
        printf("零点结算失败: 无法获取RTC时间\r\n");
        return;
    }

    printf("零点结算：将RAM中日累积电量写入SD卡... (当前: %02d/%02d %02d:%02d:%02d)\r\n",
           rtc_now.month, rtc_now.day, rtc_now.hours, rtc_now.minutes, rtc_now.seconds);

    /* 清零太阳能发电量RAM累积, 随后在循环中逐台累加各从机kWh */
    g_mppt.energy_kwh = 0.0f;

    for (uint16_t i = 0; i < device_count; i++)
    {
        /* 跳过未入网的设备 */
        if (device_list[i].state.bits.valid == 0) continue;

        /* 累加太阳能发电量: 使用与用户数据相同的kWh计算方式, 保证发电量=Σ用电量 */
        g_mppt.energy_kwh += device_list[i].daily_energy_wh / 1000.0f;

        /* 从SD卡读取用户数据文件 */
        user_data_file_t user_data;
        int ret = read_user_data(device_list[i].addr, &user_data);
        if (ret == 0)
        {
            float energy_kwh = device_list[i].daily_energy_wh / 1000.0f;  /* Wh → kWh */
            /* 检查是否需要重置日用电量（日期变化时重置） */
            if (user_data.last_reset_day != rtc_now.day)
            {
                printf("  设备[%d] 日用电量重置 (上次: %02d, 当前: %02d)\r\n", 
                       i, user_data.last_reset_day, rtc_now.day);
                user_data.daily_energy = energy_kwh;
                user_data.last_reset_day = rtc_now.day;
                user_data.half_day_energy_wh = 0;

                /* 累加到各维度用电量 */
                user_data.monthly_energy += energy_kwh;
                user_data.annual_energy  += energy_kwh;
                user_data.total_energy   += energy_kwh;

                /* 更新七日用电量数组：整体左移，[6]放入当日用电量 */
                memmove(&user_data.weekly_energy[0], &user_data.weekly_energy[1],
                        6 * sizeof(float));  /* [0]=[1], [1]=[2], ... [5]=[6] */
                user_data.weekly_energy[6] = user_data.daily_energy;  /* 最新一天 */

                house_info_t house;
                parse_addr(device_list[i].addr, &house);
                printf("  设备[%d_%d_%04d] 本日用电: %d Wh, 本日用电: %.3f kWh\r\n",
                    house.building, house.unit, house.room,
                    device_list[i].daily_energy_wh, user_data.daily_energy);
            }

            /* 检查是否需要重置月用电量（月份变化时重置） */
            if (user_data.last_reset_mon != rtc_now.month)
            {
                uint8_t old_mon = user_data.last_reset_mon;  /* 保存旧月份，用于跨年判断 */
                printf("  设备[%d] 月用电量重置 (上次: %02d, 当前: %02d)\r\n", 
                       i, old_mon, rtc_now.month);
                user_data.monthly_energy = energy_kwh;
                user_data.last_reset_mon = rtc_now.month;

                /* 跨年检查：如果月份变为1月且上次记录不是1月，说明是新的一年 */
                if (rtc_now.month == 1 && old_mon != 1)
                {
                    printf("  设备[%d] 年用电量重置\r\n", i);
                    user_data.annual_energy = energy_kwh;
                }
            }
            /* 在写入SD卡之前更新时间 */
            user_data.update_time.year    = rtc_now.year;
            user_data.update_time.month   = rtc_now.month;
            user_data.update_time.day     = rtc_now.day;
            user_data.update_time.hours   = rtc_now.hours;
            user_data.update_time.minutes = rtc_now.minutes;
            user_data.update_time.seconds = rtc_now.seconds;

            /* 同步更新RAM缓存，这样就不用再读取一遍sd卡了 */
            user_detail_cache[i].unit = user_data.unit;
            user_detail_cache[i].room = user_data.room;
            user_detail_cache[i].daily_energy   = user_data.daily_energy;
            user_detail_cache[i].monthly_energy = user_data.monthly_energy;
            user_detail_cache[i].annual_energy  = user_data.annual_energy;
            user_detail_cache[i].total_energy   = user_data.total_energy;
            memcpy(user_detail_cache[i].weekly_energy, user_data.weekly_energy,
                sizeof(user_data.weekly_energy));
            /* 更新RAM时间 */
            memcpy(&user_detail_cache[i].update_time, &user_data.update_time,
                   sizeof(user_data.update_time));

            /* 写回SD卡 */
            write_user_data(device_list[i].addr, &user_data);
        }
        /* 清零该设备的RAM日累积 */
        device_list[i].daily_energy_wh = 0;
    }

    printf("零点结算完成\r\n");
}

// ================== 搜索前：暂存当日累积电量到SD卡 ==================

/**
 * @brief 搜索前把RAM中的 daily_energy_wh 和 last_energy_read 暂存到各用户数据文件
 * @note  在 ES1642_CMD_START_SEARCH 收到模块确认后、Clear_devices() 之前调用。
 *        只更新 user_data_file_t 中的 half_day_energy_wh 和 last_energy_read 两个字段，
 *        其它用电量字段(daily/monthly/annual/total/weekly)保持不动，避免破坏已结算数据。
 *        仅处理已入网设备(valid==1)，按"通信地址(楼栋_单元_房间)"匹配文件。
 */
void save_energy_before_search(void)
{
    if (device_count == 0) return;

    printf("搜索前暂存: 将RAM中日累积电量写入用户数据文件... (设备数:%d)\r\n", device_count);

    for (uint16_t i = 0; i < device_count; i++)
    {
        /* 跳过未入网设备 */
        if (device_list[i].state.bits.valid == 0) continue;

        user_data_file_t user_data;
        if (read_user_data(device_list[i].addr, &user_data) == 0)
        {
            /* 只更新暂存字段，其它字段不动 */
            user_data.half_day_energy_wh = device_list[i].daily_energy_wh;
            user_data.last_energy_read   = last_energy_read[i];
            write_user_data(device_list[i].addr, &user_data);
        }
        else
        {
            printf("  设备[%d] 用户数据文件读取失败，跳过暂存\r\n", i);
        }
    }

    printf("搜索前暂存完成\r\n");
}

// ================== 搜索后：恢复当日累积电量并重建缓存 ==================

/**
 * @brief 搜索结束后从用户数据文件恢复 daily_energy_wh / last_energy_read，并重建缓存
 * @note  在 ES1642_CMD_STOP_SEARCH 的 if/else 之后统一调用：
 *          - device_count>0: 使用刚搜索到的新设备表
 *          - device_count==0: device_manager_init() 已重新加载旧设备表
 *        把文件中的 half_day_energy_wh 回填到 daily_energy_wh[i]、
 *        last_energy_read 回填到 last_energy_read[i]；
 *        随后将文件中的这两个暂存字段清零并写回，避免下次上电/搜索读到陈旧快照。
 *        最后调用 user_detail_cache_init() 重建用户详情页缓存。
 *        仅处理已入网设备(valid==1)。
 */
void restore_energy_after_search(void)
{
    if (device_count == 0)
    {
        /* 无设备，仍需重建(清空)缓存，保证状态一致 */
        user_detail_cache_init();
        return;
    }

    printf("搜索后恢复: 从用户数据文件恢复日累积电量... (设备数:%d)\r\n", device_count);

    for (uint16_t i = 0; i < device_count; i++)
    {
        /* 跳过未入网设备 */
        if (device_list[i].state.bits.valid == 0) continue;

        user_data_file_t user_data;
        if (read_user_data(device_list[i].addr, &user_data) == 0)
        {
            /* 把暂存值恢复到RAM */
            device_list[i].daily_energy_wh  = user_data.half_day_energy_wh;
            last_energy_read[i] = user_data.last_energy_read;

            /* 清零文件中的暂存字段并写回，避免陈旧快照残留 */
            user_data.half_day_energy_wh = 0;
            user_data.last_energy_read   = 0;
            write_user_data(device_list[i].addr, &user_data);
        }
        else
        {
            /* 文件不存在/读取失败，保持RAM为0(Clear_devices已清零)，新设备按基准处理 */
            device_list[i].daily_energy_wh  = 0;
            last_energy_read[i] = 0;
        }
    }

    /* 重建用户详情页缓存(仅加载已入网设备) */
    user_detail_cache_init();

    printf("搜索后恢复完成\r\n");
}

// ================== 打印设备列表 ==================
void print_device_list(void)
{
    printf("设备总数: %d\r\n", device_count);

    for (int i = 0; i < device_count; i++)
    {
        printf("[%d] MAC:", i);

        for (int j = 0; j < 6; j++)
        {
            printf("%02X", device_list[i].mac[j]);
            if (j < 5) printf(":");
        }

        printf("  ADDR:");

        for (int j = 0; j < 6; j++)
        {
            printf("%02X", device_list[i].addr[j]);
            if (j < 5) printf(":");
        }
				printf("  网络状态:");
				if(device_list[i].state.bits.valid == 0)
				{
					printf("未入网 ");
				}
				else
				{
					printf("已入网 ");
				}

        printf("\r\n");
    }
}

// ================== 根据通信地址查找设备(MPPT异步回调用) ==================
int find_device_by_addr(const uint8_t *addr)
{
    for (int i = 0; i < device_count; i++)
    {
        if (memcmp(device_list[i].addr, addr, 6) == 0)
        {
            return i;
        }
    }
    return -1;
}

/* ================== 通信错误金丝雀探测(恢复) ================== */

/**
 * @brief  每轮探测1台 comm_err 设备用于通信恢复(主要解决早晨载波恢复)
 * @note   主机电压≠从机端电压(线损随距离/负载变化), 无法用主机电压判断某台从机
 *         当前能否通信, 唯一可靠的可达性测试就是"试一下"。因此每轮只挑1台
 *         comm_err 设备发0x04(最多1×10s超时), 避免对全部 comm_err 设备逐台超时
 *         拖慢轮询周期(夜间全员comm_err时也只多花10s)。
 *         一旦这台响应成功→载波恢复→把【所有】设备的 comm_err 清零
 *         (comm_fail_cnt保留, 实现"1-strike"快速重新隔离); 本轮随后的正常轮询
 *         会立即重新轮询它们。仍不可达的远端从机会在下一次超时后被重新标记。
 *         金丝雀按游标轮询选取, 不解析响应数据(由本轮正常轮询刷新)。
 */
static void comm_err_canary_probe(void)
{
    static uint16_t cursor = 0;                  /* 轮询游标 */
    uint8_t read_cmd[2] = {SLAVE_CMD_READ_STATUS, 0x00};

    if (device_count == 0) return;

    /* 从游标处开始找下一台 comm_err 设备 */
    int target = -1;
    for (uint16_t n = 0; n < device_count; n++)
    {
        uint16_t i = (uint16_t)((cursor + n) % device_count);
        if (device_list[i].state.bits.valid && device_list[i].state.bits.comm_err)
        {
            target = (int)i;
            break;
        }
    }
    /* 游标前进一步(无论是否找到, 保持轮询节奏) */
    cursor = (uint16_t)((cursor + 1) % device_count);

    if (target < 0) return;   /* 没有 comm_err 设备, 白天稳定期零开销 */

    /* 对这台发0x04探测(成功则 ES1642_SendUserData 已清它自己的 comm_err/comm_fail_cnt) */
    es1642_response_t response;
    int ret = ES1642_SendUserData(target, read_cmd, 2, 0, &response);

    if (ret == 0 && response.data_len >= 10 &&
        response.data[0] == SLAVE_CMD_READ_STATUS &&
        response.data[1] == 0x08)
    {
        /* 金丝雀恢复 → 全员清除 comm_err。comm_fail_cnt保留(其它设备≥3),
         * 故它们若下次轮询再超时会立即(1-strike)被重新标记 comm_err。 */
        printf("金丝雀: 设备[%d]恢复通信, 全员清除comm_err\r\n", target);
        for (uint16_t k = 0; k < device_count; k++)
        {
            if (device_list[k].state.bits.valid)
            {
                device_list[k].state.bits.comm_err = 0;
            }
        }
    }
    /* 探测失败(超时): 什么都不做, 游标已前进, 下轮换一台再试 */
}

// ================== MPPT 采集+控制一体化 ==================
void device_poll_and_control_all(void)
{
    if (device_count == 0) return;
    if (g_es1642_searching)
    {
        printf("正在搜索设备，跳过本次轮询\r\n");
        return;
    }

    /* 金丝雀探测: 每轮挑1台 comm_err 设备发0x04; 一旦恢复→全员清 comm_err。
     * 放在正常轮询前, 本轮正常轮询会立即重新轮询刚恢复的设备。
     * 主机电压≠从机电压(线损+负载), 无法靠主机电压判断可达性, 只能"试一下"。 */
    comm_err_canary_probe();

    /* ===== ① 慢速采集阶段：逐台发0x04等ACK ===== */
    uint8_t read_cmd[2] = {SLAVE_CMD_READ_STATUS, 0x00};
    printf("MPPT采集: 开始轮询%d台设备...\r\n", device_count);

    for (uint16_t i = 0; i < device_count; i++)
    {
        if(g_device_manage_mode == 1 || g_es1642_searching == 1)
        {
            printf("设备管理模式或搜索模式下，跳过轮询\r\n");
            return;  /* 搜索/管理模式下不轮询 */
        }
        if (!device_list[i].state.bits.valid) continue;
        if (device_list[i].state.bits.comm_err) continue;

        es1642_response_t response;
        int ret = ES1642_SendUserData((int)i, read_cmd, 2, 0, &response);
        
        if (ret == 0 && response.data_len >= 10 &&
            response.data[0] == SLAVE_CMD_READ_STATUS &&
            response.data[1] == 0x08)
        {
            /* 解析温度/电压/状态 */
            device_list[i].temperature = (int8_t)response.data[2];
            device_list[i].input_voltage = (uint16_t)response.data[3] | ((uint16_t)response.data[4] << 8);
            uint8_t st = response.data[5];
            device_list[i].state.bits.dc_heating    = (st & 0x02) ? 1 : 0;
            device_list[i].state.bits.power_reverse = (st & 0x80) ? 1 : 0;
            device_list[i].state.bits.temp_err      = (st & 0x40) ? 1 : 0;
            device_list[i].state.bits.relay_err     = (st & 0x20) ? 1 : 0;

            /* 解析从机累积用电量 (uint32_t 小端, 永不清零) */
            uint32_t energy_accum = (uint32_t)response.data[6]
                                  | ((uint32_t)response.data[7] << 8)
                                  | ((uint32_t)response.data[8] << 16)
                                  | ((uint32_t)response.data[9] << 24);

            /* 用电量差值计算 */
            uint32_t delta;
            uint32_t last_old = last_energy_read[i];  /* 保存旧值用于诊断打印 */
            if (last_energy_read[i] == 0) {
                delta = 0;  /* 首次/搜索后, 建立基准不累加 */
            } else if (energy_accum < last_energy_read[i]) {
                delta = energy_accum;  /* 从机重启/清零 */
            } else {
                delta = energy_accum - last_energy_read[i];  /* 正常差值 */
            }
            device_list[i].daily_energy_wh += delta;
            last_energy_read[i] = energy_accum;
            printf("用户%d: 日累计=%dWh (accum=%d, last=%d, delta=%d)\r\n",
                   device_list[i].addr[3], device_list[i].daily_energy_wh,
                   energy_accum, last_old, delta);
						
//														char topic[48];
//														char payload[128];
//														house_info_t house;
//														
//														/* 解析通信地址 */
//														parse_addr(device_list[i].addr, &house);

//														/* 构建MQTT topic: solar/status/楼栋_单元_房间号 */
//														snprintf(topic, sizeof(topic), "solar/status/%d_%d_%04d",
//																		 house.building, house.unit, house.room);
//														if (ret == 0)
//														{
//																/* 读取成功 */
//																snprintf(payload, sizeof(payload),
//																				 "{\"t\":%d,\"v\":%d,\"dc\":%d,\"pr\":%d,\"ok\":1}",
//																				 device_list[i].temperature, device_list[i].input_voltage,
//																				 device_list[i].state.bits.dc_heating, device_list[i].state.bits.power_reverse);
//														}
//														else
//														{
//																/* 读取失败 */
//																snprintf(payload, sizeof(payload),
//																				 "{\"t\":0,\"v\":0,\"dc\":0,\"pr\":0,\"ok\":0,\"err\":%d}",
//																				 -ret);
//														}

//														/* 发布MQTT消息(带自动重连保护) */
//														uint8_t mqtt_ret = A7680C_MQTT_Publish_Safe(topic, payload);
//														if (mqtt_ret != AT_RESULT_OK)
//														{
//																printf("MQTT发布失败: %s\r\n", topic);
//														}
        }
        osDelay(250);  /* 设备间间隔250ms，避免载波通信冲突 */
    }
    printf("MPPT采集完成\r\n");

    /* ===== 告警扫描（在控制阶段前执行，基于采集到的真实状态，不受控制阶段临时relay_err标记影响） ===== */
    alert_scan_devices();

    /* ===== ② 快速控制阶段：MPPT P&O 控制闭环 ===== */
    MPPT_ControlLoop();
}
