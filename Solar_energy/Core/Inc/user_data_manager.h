/**
 * @file    user_data_manager.h
 * @brief   用户数据文件管理模块
 * @note    在SD卡的USER目录下，以"楼栋_单元_房间"为文件名，
 *          为每个用户创建独立的数据文件，保存热水器温度、功率、用电量等信息。
 */
#ifndef __USER_DATA_MANAGER_H
#define __USER_DATA_MANAGER_H

#include "stdint.h"
#include "rx8025t.h"
#include "lvgl.h"

/* ================== 配置参数 ================== */
#define USER_DATA_DIR       "0:/USER"
#define USER_DATA_MAGIC     0x55444154  /* "UDAT" */
#define USER_DATA_VERSION   1

/* ================== 用户数据文件结构体 ================== */
#pragma pack(push, 1)

/* 紧凑时间戳，与RX8025T_DateTimeCompact对应，共6字节 */
typedef struct {
    uint8_t year;            /* 年: 0-99 (2000-2099) */
    uint8_t month;           /* 月: 1-12 */
    uint8_t day;             /* 日: 1-31 */
    uint8_t hours;           /* 时: 0-23 */
    uint8_t minutes;         /* 分: 0-59 */
    uint8_t seconds;         /* 秒: 0-59 */
} user_data_timestamp_t;

typedef struct {
    uint32_t magic;          /* 魔数 USER_DATA_MAGIC */
    uint8_t  version;        /* 数据版本号 */
    uint8_t  reserved1;      /* 预留对齐 */
    uint8_t  mac[6];         /* MAC地址 */
    uint8_t  building;       /* 楼栋号 */
    uint8_t  unit;           /* 单元号 */
    uint16_t room;           /* 房间号 */
    float    temperature;    /* 热水器当前温度(℃) - 内部使用 */
    float    power;          /* 当前功率(W) - 内部使用 */
    float    daily_energy;   /* 日累积用电量(kWh) - 当日0点重置 */
    float    monthly_energy; /* 月累积用电量(kWh) - 每月1号重置 */
    float    annual_energy;  /* 年累积用电量(kWh) - 每年1月1日重置 */
    float    total_energy;   /* 总累积用电量(kWh) - 永不重置 */
    uint8_t  last_reset_day; /* 上次日用电量重置时的日期(1-31) */
    uint8_t  last_reset_mon; /* 上次月用电量重置时的月份(1-12) */
    uint8_t  reserved2[2];   /* 预留对齐 */
    user_data_timestamp_t create_time;  /* 数据创建时间 */
    user_data_timestamp_t update_time;  /* 数据最后更新时间 */
} user_data_file_t;
#pragma pack(pop)

/* ================== 接口函数 ================== */

/**
 * @brief  确保用户数据文件存在，不存在则创建（带默认值）
 * @param  dev_addr: 通信地址（6字节），用于解析楼栋/单元/房间作为文件名
 * @param  mac: MAC地址（6字节），存储在文件中
 * @retval 0: 成功(文件已存在或新创建), 负数: 错误
 *         -1: 参数错误
 *         -2: 通信地址无效(全0)
 *         -3: 文件操作失败
 */
int ensure_user_data_file(const uint8_t *dev_addr, const uint8_t *mac);

/**
 * @brief  读取用户数据文件
 * @param  dev_addr: 通信地址（6字节）
 * @param  data: 输出用户数据
 * @retval 0: 成功, 负数: 错误
 */
int read_user_data(const uint8_t *dev_addr, user_data_file_t *data);

/**
 * @brief  写入（更新）用户数据文件
 * @param  dev_addr: 通信地址（6字节）
 * @param  data: 用户数据
 * @retval 0: 成功, 负数: 错误
 */
int write_user_data(const uint8_t *dev_addr, user_data_file_t *data);

/* ================== UI回调相关 ================== */

extern uint8_t s_last_user_no;  /* 上次选中的用户编号，用于返回时恢复焦点 */

/**
 * @brief  用户列表项点击回调函数
 * @note   点击后切换到用户详情页，并从SD卡读取该用户的温度/功率/用电量显示
 *         user_data传入的是device_list中的索引(uint32_t)
 */
void user_list_item_event_handler(lv_event_t *e);

#endif /* __USER_DATA_MANAGER_H */
