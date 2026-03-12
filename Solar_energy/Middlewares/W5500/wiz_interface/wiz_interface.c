#include "wiz_interface.h"
#include "wiz_platform.h"
#include "wizchip_conf.h"
#include "dhcp.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define W5500_VERSION 0x04

struct wiz_timer
{
    void (*func)(void);     // 计时器触发时要执行的函数的回调函数指针
    uint32_t trigger_time;  // 触发时间（毫秒）
    uint32_t count_time;    // 当前计数值（毫秒）
    struct wiz_timer *next; // 下一个计时器节点
};

struct wiz_timer *wiz_timer_head = NULL;
volatile uint32_t wiz_delay_ms_count = 0;

/**
 * @brief 创建 wiz_timer 节点
 * @param func :计时器触发时要执行的函数的回调函数指针
 * @param time :触发时间，通常以毫秒为单位
 *
 * @return 成功创建时返回指向 wiz_timer 节点的指针，否则返回 NULL
 */
static struct wiz_timer *create_wiz_timer_node(void (*func)(void), uint32_t time)
{
    struct wiz_timer *newNode = (struct wiz_timer *)malloc(sizeof(struct wiz_timer));
    if (newNode == NULL)
    {
        printf("Memory allocation failed.\n");
        return NULL;
    }
    newNode->func = func;
    newNode->trigger_time = time;
    newNode->count_time = 0;
    newNode->next = NULL;
    return newNode;
}

/**
 * @brief 在定时器链表中添加一个新的定时器节点
 * @param func 回调函数指针，当定时器时间达到时被调用
 * @param 时间 触发时间（毫秒）
 */
void wiz_add_timer(void (*func)(void), uint32_t time)
{
    struct wiz_timer *newNode = create_wiz_timer_node(func, time);
    if (wiz_timer_head == NULL)
    {
        wiz_timer_head = newNode;
        return;
    }
    struct wiz_timer *temp = wiz_timer_head;
    while (temp->next != NULL)
    {
        temp = temp->next;
    }
    temp->next = newNode;
}

/**
 * @brief 删除指定回调函数的定时器
 * @param 函数回调指针
 * @return none
 */
void wiz_delete_timer(void (*func)(void))
{
    struct wiz_timer *temp = wiz_timer_head;
    struct wiz_timer *prev = NULL;

    if (temp != NULL && temp->func == func)
    {
        wiz_timer_head = temp->next;
        free(temp);
        return;
    }

    while (temp != NULL && temp->func != func)
    {
        prev = temp;
        temp = temp->next;
    }

    if (temp == NULL)
        return;

    prev->next = temp->next;
    free(temp);
}

/**
 * @brief 向导定时器事件处理程序
 *
 * 你必须将此功能添加到你的1毫秒定时器中断
 *
 */
void wiz_timer_handler(void)
{

    wiz_delay_ms_count++;
    struct wiz_timer *temp = wiz_timer_head;
    while (temp != NULL)
    {
        temp->count_time++;
        if (temp->count_time >= temp->trigger_time)
        {
            temp->count_time = 0;
            temp->func();
        }
        temp = temp->next;
    }
}

/**
 * @brief 以毫秒为单位的延迟函数
 * @param nms :Delay Time
 */
void wiz_user_delay_ms(uint32_t nms)
{
    wiz_delay_ms_count = 0;
    while (wiz_delay_ms_count < nms)
    {
    }
}

/**
 * @brief 检查 WIZCHIP 版本
 */
void wizchip_version_check(void)
{
    uint8_t error_count = 0;
    while (1)
    {
        wiz_user_delay_ms(1000);
        if (getVERSIONR() != W5500_VERSION)
        {
            error_count++;
            if (error_count > 5)
            {
                printf("错误，W5500 版本是 0x%02x, 但是读取 W5500 版本值 = 0x%02x\r\n", W5500_VERSION, getVERSIONR());
                while (1)
                    ;
            }
        }
        else
        {
            break;
        }
    }
}

/**
 * @brief 打印物理层信息
 */
void wiz_print_phy_info(void)
{
    uint8_t get_phy_conf;
    get_phy_conf = getPHYCFGR();
    printf("当前的Mbtis速度: %dMbps\r\n", get_phy_conf & 0x02 ? 100 : 10);
    printf("当前双工模式 : %s\r\n", get_phy_conf & 0x04 ? "Full-Duplex" : "Half-Duplex");
}

/**
 * @brief 以太网链路检测
 */
void wiz_phy_link_check(void)
{
    uint8_t phy_link_status;
    do
    {
        wiz_user_delay_ms(1000);
        ctlwizchip(CW_GET_PHYLINK, (void *)&phy_link_status);
        if (phy_link_status == PHY_LINK_ON)
        {
            printf("PHY 已连接\r\n");
            wiz_print_phy_info();
        }
        else
        {
            printf("PHY未连接\r\n");
        }
    } while (phy_link_status == PHY_LINK_OFF);
}

/**
 * @brief   wizchip 初始化函数
 * @param   none
 * @return  none
 */
void wizchip_initialize(void)
{
    /* 启用定时器中断 */
    wiz_tim_irq_enable();

    /* 寄存器 wizchip SPI */
    wizchip_spi_cb_reg();

    /* 重置wizchip */
    wizchip_reset();

    /* 读取版本寄存器 */
    wizchip_version_check();

    /* 检查PHY链路状态，导致PHY正常启动 */
    wiz_phy_link_check();
}

/**
 * @brief   打印网络信息
 * @param   none
 * @return  none
 */
void print_network_information(void)
{
    wiz_NetInfo net_info;
    wizchip_getnetinfo(&net_info); // 获取芯片配置信息

    if (net_info.dhcp == NETINFO_DHCP)
    {
        printf("====================================================================================================\r\n");
        printf(" %s 网络配置 : DHCP\r\n\r\n", _WIZCHIP_ID_);
    }
    else
    {
        printf("====================================================================================================\r\n");
        printf(" %s 网络配置 : static\r\n\r\n", _WIZCHIP_ID_);
    }

    printf(" MAC         : %02X:%02X:%02X:%02X:%02X:%02X\r\n", net_info.mac[0], net_info.mac[1], net_info.mac[2], net_info.mac[3], net_info.mac[4], net_info.mac[5]);
    printf(" IP          : %d.%d.%d.%d\r\n", net_info.ip[0], net_info.ip[1], net_info.ip[2], net_info.ip[3]);
    printf(" 子网掩码 	 : %d.%d.%d.%d\r\n", net_info.sn[0], net_info.sn[1], net_info.sn[2], net_info.sn[3]);
    printf(" 网关        : %d.%d.%d.%d\r\n", net_info.gw[0], net_info.gw[1], net_info.gw[2], net_info.gw[3]);
    printf(" DNS         : %d.%d.%d.%d\r\n", net_info.dns[0], net_info.dns[1], net_info.dns[2], net_info.dns[3]);
    printf("====================================================================================================\r\n\r\n");
}

/**
 * @brief DHCP 过程
 * @param sn :套接字号
 * @param buffer :套接字缓冲区
 */
static uint8_t wiz_dhcp_process(uint8_t sn, uint8_t *buffer)
{
    wiz_NetInfo conf_info;
    uint8_t dhcp_run_flag = 1;
    uint8_t dhcp_ok_flag = 0;
    /* 注册 DHCP_time_handler 到 1 秒定时器 */
    wiz_add_timer(DHCP_time_handler, 1000);
    DHCP_init(sn, buffer);
    printf("DHCP running\r\n");
    while (1)
    {
        switch (DHCP_run()) // 执行 DHCP 客户端
        {
        case DHCP_IP_LEASED: // DHCP 成功获取网络信息
        {
            if (dhcp_ok_flag == 0)
            {
                dhcp_ok_flag = 1;
                dhcp_run_flag = 0;
            }
            break;
        }
        case DHCP_FAILED:
        {
            dhcp_run_flag = 0;
            break;
        }
        }
        if (dhcp_run_flag == 0)
        {
            printf("DHCP %s!\r\n", dhcp_ok_flag ? "成功" : "失败");
            DHCP_stop();

            /*DHCP 获取成功，取消注册 DHCP_time_handler*/
            wiz_delete_timer(DHCP_time_handler);

            if (dhcp_ok_flag)
            {
                getIPfromDHCP(conf_info.ip);
                getGWfromDHCP(conf_info.gw);
                getSNfromDHCP(conf_info.sn);
                getDNSfromDHCP(conf_info.dns);
                conf_info.dhcp = NETINFO_DHCP;
                getSHAR(conf_info.mac);
                wizchip_setnetinfo(&conf_info); // 将网络信息更新为通过 DHCP 获取的网络信息
                return 1;
            }
            return 0;
        }
    }
}

/**
 * @brief   设置网络信息
 *
 * 首先确定是否使用 DHCP。如果使用 DHCP，首先通过 DHCP 获取互联网协议地址。
 * 当 DHCP 失败时，使用静态 IP 配置网络信息。如果使用静态 IP，则直接配置网络信息
 *
 * @param   sn: socketid
 * @param   ethernet_buff:
 * @param   net_info: 网络信息结构
 * @return  none
 */
void network_init(uint8_t *ethernet_buff, wiz_NetInfo *conf_info)
{
    int ret;
    wizchip_setnetinfo(conf_info); // 配置网络信息
    if (conf_info->dhcp == NETINFO_DHCP)//判断是否使用DHCP
    {
        ret = wiz_dhcp_process(0, ethernet_buff);
        if (ret == 0)
        {
            conf_info->dhcp = NETINFO_STATIC;
            wizchip_setnetinfo(conf_info);
        }
    }
    print_network_information();
}
