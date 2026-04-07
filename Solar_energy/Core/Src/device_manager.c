#include "device_manager.h"
#include <string.h>
#include <stdio.h>
#include "es1642.h"
#include "es1642_usage_guide.h"
// ================== 全局变量 ==================
device_t device_list[MAX_DEVICES];
uint16_t device_count = 0;

// 标志：设备是否有变化（用于减少SD写入）
static uint8_t device_changed = 0;

// ================== 初始化 ==================
void device_manager_init(void)
{
    // 上电时加载设备表
    if (load_devices() != FR_OK)
    {
        device_count = 0;
    }
}

// ================== 查找设备 ==================
int find_device_by_mac(uint8_t *mac)
{
    for (int i = 0; i < device_count; i++)
    {
        if (device_list[i].valid &&
            memcmp(device_list[i].mac, mac, 6) == 0)
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

// ================== 用户小程序发送过来绑定命令时更新设备表中的通信地址 ==================
void update_device(uint8_t *mac, uint8_t *addr)
{
    int index = find_device_by_mac(mac);//查找小程序发来的要绑定的设备在设备表中的位置
		const uint8_t new_psk[ES1642_SET_PSK_LEN] = {0x11,0x12,0x13,0x14,0x15,0x16,0x17,0x18};//统一网络口令
		
		if(device_list[index].valid == 0)//如果搜索结果是没入网，那就进行入网
		{
				ES1642_SetPsk(device_list[index].addr,new_psk);
		}

		// MAC已存在 → 判断通信地址是否变化
		if (memcmp(device_list[index].addr, addr, 6) != 0)//通信地址发生变化
		{
				memcpy(device_list[index].addr, addr, 6);
				device_list[index].valid = 1;
				device_changed = 1;
		}
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

    res = f_open(&file, DEVICE_FILE, FA_READ);
    if (res != FR_OK)
    {
        printf("设备文件不存在\r\n");
        device_count = 0;
        return FR_NO_FILE;
    }

    res = f_read(&file, device_list, sizeof(device_list), &br);

    f_close(&file);

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

