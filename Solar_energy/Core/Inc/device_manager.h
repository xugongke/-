#ifndef __DEVICE_MANAGER_H
#define __DEVICE_MANAGER_H

#include "stdint.h"
#include "ff.h"   // FATFS

// ================== 配置参数 ==================
#define MAX_DEVICES 256      // 最大设备数量
#define DEVICE_FILE "0:/devices.bin"   // 存储文件名

// ================== 设备状态位图共用体 ==================
typedef union {
    uint8_t byte;  // 整字节访问
    struct {
        uint8_t valid        : 1;  // bit0: 有效/入网标志 (1=已入网, 0=未入网)
        uint8_t dc_heating   : 1;  // bit1: 直流加热 (1=正在加热, 0=未加热)
        uint8_t comm_err     : 1;  // bit2: 通信异常 (1=异常, 0=正常)
        uint8_t reserved1    : 1;  // bit3: 保留
        uint8_t reserved2    : 1;  // bit4: 保留
        uint8_t relay_err    : 1;  // bit5: 继电器控制异常 (1=异常, 0=正常)
        uint8_t temp_err     : 1;  // bit6: 温度异常 (1=异常, 0=正常)
        uint8_t power_reverse: 1;  // bit7: 电源反接 (1=反接, 0=正常)
    } bits;
} device_state_t;

// ================== 设备结构体 ==================
typedef struct
{
    uint8_t mac[6];            // MAC地址（唯一标识）
    uint8_t addr[6];           // 通信地址（可修改）
    int8_t   temperature;       /**< 温度 (℃) ，不会马上保存到SD卡 */
    uint16_t input_voltage;     /**< 输入电压 (V) ，不会马上保存到SD卡 */
    device_state_t state;      // 状态位图 (共用体, 可按位域或整字节访问 ，不会马上保存到SD卡)
    uint8_t comm_fail_cnt;     // 通信失败次数计数 ，不会马上保存到SD卡
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
int update_device(uint8_t *mac, uint8_t *addr);

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
/**
 * @brief 生成通信地址（户号编码）
 */
void make_addr(uint8_t *addr,
               uint8_t building,
               uint8_t unit,
               uint16_t room);

/**
 * @brief 控制从机启动/停止加热
 * @param dev_index 设备在device_list中的下标
 * @param heater_on 1=启动加热, 0=停止加热
 * @return 0=成功, -1=参数错误, -2=从机超时, -3=从机拒绝, -4=发送失败
 */
int device_ctrl_heater(int dev_index, uint8_t heater_on);


/**
 * @brief 读取从机状态并通过结构体返回解析后的数据
 * @param dev_index 设备在device_list中的下标
 * @param status 输出: 解析后的状态数据
 * @return 0=成功, -1=参数错误, -2=从机超时, -3=响应异常, -4=发送失败
 */
int device_read_status_ex(int dev_index);

/**
 * @brief 轮询所有有效设备状态并通过MQTT上报
 */
void device_poll_all_status(void);

/* 上位机忙碌标志: 1=上位机正在操作(TCP已连接/RS485绑定中), 轮询暂停 */
extern volatile uint8_t g_host_busy;

#endif
