#include "device_manager.h"
#include <string.h>
#include <stdio.h>
#include "es1642.h"
#include "es1642_usage_guide.h"
#include "user_data_manager.h"
#include "a7680c_mqtt.h"
#include "cmsis_os.h"

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

// 标志：设备是否有变化（用于减少SD写入）
static uint8_t device_changed = 0;

// 上位机忙碌标志: 1=上位机正在操作, 轮询暂停
volatile uint8_t g_host_busy = 0;

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
		if(net_state == ES1642_NET_STATE_SAME_NETWORK)//如果已入网，代表之前设置过通信地址,valid设为1
		{
			memcpy(device_list[device_count].mac, mac, 6);
			memcpy(device_list[device_count].addr, addr, 6);
			device_list[device_count].valid = 1;
		}
		else//没入网那就先添加进设备表，等待入网
		{
			memcpy(device_list[device_count].mac, mac, 6);
			memcpy(device_list[device_count].addr, addr, 6);
			device_list[device_count].valid = 0;
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
		
		if(device_list[index].valid == 0)//如果是没入网，那就进行入网，入网了才能正常通信,下面的发送修改通信地址命令从机才能接收到
		{
				ret = ES1642_SetPsk(device_list[index].addr, new_psk);
				if (ret == 0)
				{
						printf("从机入网成功\r\n");
						device_list[index].valid = 1;
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
				cmd_buf[0] = SLAVE_CMD_SET_ADDR;       // 命令: 修改通信地址
				cmd_buf[1] = ES1642_ADDR_LEN;           // 数据长度: 6
				memcpy(&cmd_buf[2], addr, ES1642_ADDR_LEN); // 数据: 新通信地址
				ret = ES1642_SendUserData(device_list[index].addr, cmd_buf, sizeof(cmd_buf), 0, &response);

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
	memset(device_list,0,sizeof(device_list));
	device_count = 0;
}

// ================== 保存设备表 ==================
FRESULT save_devices(void)
{
    static FIL file;
    FRESULT res;
    UINT bw;

    // 没有变化就不写SD卡（减少磨损）
    if (!device_changed)
        return FR_OK;
		//不管文件是否存在，都直接创建新文件；如果已存在，就清空重写
    res = f_open(&file, DEVICE_FILE, FA_CREATE_ALWAYS | FA_WRITE);
    if (res != FR_OK)
    {
        printf("打开文件失败\r\n");
        return res;
    }

    res = f_write(&file, device_list, device_count * sizeof(device_t), &bw);

    if (res == FR_OK)
    {
        printf("保存成功，设备数:%d\r\n", device_count);
        device_changed = 0;
    }

    f_close(&file);
    return res;
}

// ================== 加载设备表 ==================
FRESULT load_devices(void)
{
    static FIL file;
    FRESULT res;
    UINT br;
	//如果文件存在就打开，如果文件不存在就创建，但是并没有读写权限，需要加上
    res = f_open(&file, DEVICE_FILE, FA_OPEN_ALWAYS | FA_READ);
    if (res != FR_OK)
    {
        printf("设备表文件打开失败\r\n");
        device_count = 0;
        return FR_NO_FILE;
    }

    res = f_read(&file, device_list, sizeof(device_list), &br);
    if (res != FR_OK)
    {
        printf("设备表文档f_read失败\r\n");
    }

    f_close(&file);
		//br是实际读取到的字节数,也就是用户真实的数量
    device_count = br / sizeof(device_t);

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
int device_ctrl_heater(uint8_t *addr, uint8_t heater_on)
{
    es1642_response_t response;
    uint8_t cmd_buf[3];  /* [cmd][data_len][data] */
    int ret;

    if (addr == NULL) { return -1; }

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
    ret = ES1642_SendUserData(addr, cmd_buf, sizeof(cmd_buf), 0, &response);

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

// ================== 读取从机状态(扩展版,返回解析数据) ==================

/**
 * @brief 读取从机状态并通过结构体返回解析后的数据
 * @param addr   从机通信地址 (6字节)
 * @param status 输出: 解析后的状态数据
 * @return 0=成功, -1=参数错误, -2=从机超时, -3=响应异常, -4=发送失败
 *
 * 从机响应: [cmd=0x04][data_len=0x04][temp][vol_lo][vol_hi][state]
 *   temp:  温度 (int8_t, 单位℃)
 *   vol:   电压 (uint16_t小端, 单位V)
 *   state: 状态字 (bit1=直流加热, bit7=电源反接)
 */
int device_read_status_ex(uint8_t *addr, device_status_t *status)
{
    es1642_response_t response;
    uint8_t cmd_buf[2];  /* [cmd][data_len] */
    int ret;

    if (addr == NULL || status == NULL) { return -1; }

    /* 初始化输出结构体 */
    memset(status, 0, sizeof(device_status_t));

    /* 组装读取命令: 命令字+数据长度(0) */
    cmd_buf[0] = SLAVE_CMD_READ_STATUS;
    cmd_buf[1] = 0x00;  /* 无附加数据 */

    /* 发送 */
    ret = ES1642_SendUserData(addr, cmd_buf, sizeof(cmd_buf), 0, &response);

    if (ret == 0)
    {
        /* 检查响应: [cmd=0x04][len=0x04][temp][vol_lo][vol_hi][state] */
        if (response.data_len >= 6 &&
            response.data[0] == SLAVE_CMD_READ_STATUS &&
            response.data[1] == 0x04)
        {
            status->temperature  = (int8_t)response.data[2];
            status->input_voltage = (uint16_t)response.data[3] | ((uint16_t)response.data[4] << 8);
            uint8_t state_byte   = response.data[5];

            /* bit1 = 直流加热, bit7 = 电源反接 */
            status->dc_heating    = (state_byte & 0x02) ? 1 : 0;
            status->power_reverse = (state_byte & 0x80) ? 1 : 0;
            status->dry_burn_err  = (state_byte & 0x40) ? 1 : 0;
            status->relay_err     = (state_byte & 0x20) ? 1 : 0;

            printf("从机状态: 温度=%d℃, 电压=%dV, 直流加热=%s, 电源反接=%s，干烧错误=%s，继电器控制错误=%s\r\n",
                   status->temperature, status->input_voltage,
                   status->dc_heating ? "是 " : "否 ",
                   status->power_reverse ? "是 " : "否 ",
                   status->dry_burn_err ? "是 " : "否 ",
                   status->relay_err ? "是 " : "否 ");
            return 0;
        }
        else
        {
            printf("从机状态读取失败, 响应异常\r\n");
            return -3;
        }
    }
    else if (ret == -2)
    {
        printf("从机状态读取超时\r\n");
        return -2;
    }
    else
    {
        printf("发送读取状态命令失败\r\n");
        return -4;
    }
}

// ================== 轮询所有从机状态并通过MQTT上报 ==================

/**
 * @brief 轮询所有有效设备的状态并通过MQTT上报
 * @note  每个设备单独发布一条MQTT消息，topic格式: solar/status/楼栋_单元_房间
 *        JSON payload: {"t":25,"v":220,"dc":0,"pr":0}
 *        读取失败的设备也会上报（带ok字段标识）
 */
void device_poll_all_status(void)
{
    if (device_count == 0) return;

    /* 搜索设备期间不进行轮询，避免干扰搜索 */
    if (g_es1642_searching)
    {
        printf("正在搜索设备，跳过本次轮询\r\n");
        return;
    }

    /* 上位机正在操作时暂停轮询，避免干扰入网等操作 */
    if (g_host_busy)
    {
        printf("上位机正在操作，跳过本次轮询\r\n");
        return;
    }

    char topic[48];
    char payload[128];
    house_info_t house;
    device_status_t status;

    printf("开始轮询%d个设备状态...\r\n", device_count);

    for (uint16_t i = 0; i < device_count; i++)
    {
        /* 跳过未入网的设备 */
        if (device_list[i].valid != 1) continue;

        /* 解析通信地址 */
        parse_addr(device_list[i].addr, &house);

        /* 构建MQTT topic: solar/status/楼栋_单元_房间号 */
        snprintf(topic, sizeof(topic), "solar/status/%d_%d_%04d",
                 house.building, house.unit, house.room);

        /* 读取从机状态 */
        int ret = device_read_status_ex(device_list[i].addr, &status);

        if (ret == 0)
        {
            /* 读取成功 */
            snprintf(payload, sizeof(payload),
                     "{\"t\":%d,\"v\":%d,\"dc\":%d,\"pr\":%d,\"ok\":1}",
                     status.temperature, status.input_voltage,
                     status.dc_heating, status.power_reverse);
        }
        else
        {
            /* 读取失败 */
            snprintf(payload, sizeof(payload),
                     "{\"t\":0,\"v\":0,\"dc\":0,\"pr\":0,\"ok\":0,\"err\":%d}",
                     -ret);
        }

        /* 发布MQTT消息 */
        uint8_t mqtt_ret = A7680C_MQTT_Publish(topic, payload);
        if (mqtt_ret != AT_RESULT_OK)
        {
            printf("MQTT发布失败: %s\r\n", topic);
        }

        /* 设备间间隔200ms，避免载波通信冲突 */
        osDelay(200);
    }

    printf("设备轮询完成\r\n");
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
				if(device_list[i].valid != 1)
				{
					printf("未入网 ");
				}
				else if(device_list[i].valid == 1)
				{
					printf("已入网 ");
				}

        printf("\r\n");
    }
}
