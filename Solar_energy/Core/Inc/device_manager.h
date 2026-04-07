#ifndef __DEVICE_MANAGER_H
#define __DEVICE_MANAGER_H

#include "stdint.h"
#include "ff.h"   // FATFS

// ================== 配置参数 ==================
#define MAX_DEVICES 256      // 最大设备数量
#define DEVICE_FILE "0:/LOG/devices.bin"   // 存储文件名

// ================== 设备结构体 ==================
typedef struct
{
    uint8_t mac[6];   // MAC地址（唯一标识）
    uint8_t addr[6];  // 通信地址（可修改）
    uint8_t valid;    // 有效标志：1=有效，0=无效
} device_t;
//通信地址解析结构体
typedef struct
{
    uint8_t building;   // 楼栋号
    uint8_t unit;       // 单元号
    uint16_t room;      // 房号
} house_info_t;

// ================== 全局变量 ==================
extern device_t device_list[MAX_DEVICES];
extern uint16_t device_count;

// ================== 接口函数 ==================

/**
 * @brief 初始化设备管理（从SD卡加载）
 */
void device_manager_init(void);

/**
 * @brief 根据MAC查找设备
 * @param mac MAC地址
 * @return >=0:索引  -1:未找到
 */
int find_device_by_mac(uint8_t *mac);

/**
 * @brief 添加或更新设备
 * @param mac MAC地址
 * @param addr 通信地址
 */
// ================== 搜索设备时调用的添加 设备函数 ，搜索设备前清空设备表 ==================
void add_device(uint8_t *mac, uint8_t *addr,uint8_t net_state);

// ================== 用户小程序发送过来绑定命令时 更新设备 表中的通信地址 ==================
void update_device(uint8_t *mac, uint8_t *addr);

/**
 * @brief 保存设备表到SD卡
 */
FRESULT save_devices(void);

/**
 * @brief 从SD卡加载设备表
 */
FRESULT load_devices(void);

void parse_addr(uint8_t *addr, house_info_t *info);

/**
 * @brief 打印设备列表（调试用）
 */
void print_device_list(void);
void Clear_devices(void);
#endif

