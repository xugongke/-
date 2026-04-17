#include "interrupt.h"
#include "socket.h" // Use socket
#include "wiz_interface.h"
#include <stdio.h>
#include "string.h"
#include "main.h"
#include "device_manager.h"

#define ETHERNET_BUF_MAX_SIZE (1024 * 2) // Send and receive cache size
#define INTERRUPT_DEBUG
#if (_WIZCHIP_ == W5500)
#define IR_SOCK(ch) (0x01 << ch) /**< check socket interrupt */
#endif

enum SN_STATUS
{
    closed_status = 0,
    ready_status,
    connected_status,
};
static uint8_t I_STATUS[_WIZCHIP_SOCK_NUM_];
static uint8_t ch_status[_WIZCHIP_SOCK_NUM_] = {0};
/**
 * @brief   确定中断类型并将值存储在 I_STATUS 中
 * @param   none
 * @return  none
 */
void wizchip_ISR(void)
{
    uint8_t SIR_val = 0;
    uint8_t tmp, sn;

    SIR_val = getSIR();
    if (SIR_val != 0xff)
    {
        setSIMR(0x00);
        for (sn = 0; sn < _WIZCHIP_SOCK_NUM_; sn++)
        {
            tmp = 0;
            if (SIR_val & IR_SOCK(sn))
            {
                tmp = getSn_IR(sn);
                I_STATUS[sn] |= tmp;
                tmp &= 0x0f;
                setSn_IR(sn, tmp);
            }
        }
        setSIMR(0xff);
    }
}
//MAC地址转换成字符串
static void mac_to_string(const uint8_t mac[6], char *out)
{
    sprintf(out, "%02X:%02X:%02X:%02X:%02X:%02X",
            mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}
//通信地址转换成字符串
static void addr_to_string(const uint8_t addr[6], char *out)
{
    sprintf(out, "%02X:%02X:%02X:%02X:%02X:%02X",
            addr[0], addr[1], addr[2], addr[3], addr[4], addr[5]);
}
//解析MAC字符串
static int parse_mac_string(const char *str, uint8_t mac[6])
{
    unsigned int tmp[6];
    if (sscanf(str, "%2x:%2x:%2x:%2x:%2x:%2x",
               &tmp[0], &tmp[1], &tmp[2], &tmp[3], &tmp[4], &tmp[5]) != 6)
    {
        return -1;
    }
    for (int i = 0; i < 6; i++)
    {
        mac[i] = (uint8_t)tmp[i];
    }
    return 0;
}
//把设备列表发送到app
void tcp_send_device_list(uint8_t sn)
{
    char line[128];
    char mac_str[32];
    char addr_str[32];

    sprintf(line, "COUNT,%u\n", device_count);
		send(sn, (uint8_t*)line, strlen(line));

    for (uint16_t i = 0; i < device_count; i++)
    {
        mac_to_string(device_list[i].mac, mac_str);
        addr_to_string(device_list[i].addr, addr_str);
        sprintf(line, "DEV,%s,%s,%u\n", mac_str, addr_str, device_list[i].valid);
        send(sn, (uint8_t*)line, strlen(line));
    }
		send(sn, (uint8_t*)"END\n", sizeof("END\n"));
}
//把用户住址转换成通信地址绑定到设备列表
void tcp_handle_bind_cmd(uint8_t sn,const char *cmd)
{
    // 命令格式: BIND,AA:BB:CC:DD:EE:FF,2,1,301
    char mac_str[32] = {0};
    int building = 0;
    int unit = 0;
    int room = 0;
    uint8_t mac[6];
    uint8_t addr[6];

    if (sscanf(cmd, "BIND,%31[^,],%d,%d,%d", mac_str, &building, &unit, &room) != 4)
    {
				send(sn, (uint8_t*)"ERR,命令格式错误\n", sizeof("ERR,命令格式错误\n"));
        return;
    }

    if (parse_mac_string(mac_str, mac) != 0)
    {
				send(sn, (uint8_t*)"ERR,MAC格式错误\n", sizeof("ERR,MAC格式错误\n"));
        return;
    }
		//生成通信地址
    make_addr(addr, (uint8_t)building, (uint8_t)unit, (uint16_t)room);
    update_device(mac, addr);//执行入网命令
    save_devices();
		send(sn, (uint8_t*)"OK\n", sizeof("OK\n"));
}

/**
 * @brief   TCP 服务器中断模式回环
 * @param   none
 * @return  none
 */
void loopback_tcps_interrupt(uint8_t sn, uint8_t *buf, uint16_t port)
{
    uint16_t len = 0;
    uint8_t destip[4];
    uint16_t destport;

    if (I_STATUS[sn] == SOCK_CLOSED)
    {

        if (!ch_status[sn])
        {
#ifdef INTERRUPT_DEBUG
            printf("%d:TCP server start\r\n", sn);
#endif
            ch_status[sn] = ready_status;

            if (socket(sn, Sn_MR_TCP, port, 0x00) != sn)
            {
                ch_status[sn] = closed_status;
            }
            else
            {
#ifdef INTERRUPT_DEBUG
                printf("%d:Socket opened\r\n", sn);
#endif
                listen(sn);
#ifdef INTERRUPT_DEBUG
                printf("%d:Listen, TCP server loopback, port [%d]\r\n", sn, port);
#endif
            }
        }
    }
    if (I_STATUS[sn] & Sn_IR_CON)
    {
        getSn_DIPR(sn, destip);
        destport = getSn_DPORT(sn);
#ifdef INTERRUPT_DEBUG
        printf("%d:Connected - %d.%d.%d.%d : %d\r\n", sn, destip[0], destip[1], destip[2], destip[3], destport);

#endif
        ch_status[sn] = connected_status;
        I_STATUS[sn] &= ~(Sn_IR_CON);
    }

    if (I_STATUS[sn] & Sn_IR_DISCON)
    {
        printf("%d:套接字已断开\r\n", sn);
        if ((getSn_RX_RSR(sn)) > 0)
        {
            len = getSn_RX_RSR(sn);

            if (len > ETHERNET_BUF_MAX_SIZE)
            {
                len = ETHERNET_BUF_MAX_SIZE;
            }
            recv(sn, buf, len);
            buf[len] = 0x00;
            printf("%d:recv data:%s\r\n", sn, buf);
            I_STATUS[sn] &= ~(Sn_IR_RECV);
        }
        disconnect(sn);
        ch_status[sn] = closed_status;
        I_STATUS[sn] &= ~(Sn_IR_DISCON);
    }

    if (I_STATUS[sn] & Sn_IR_RECV)
    {
#if (_WIZCHIP_ == W5100S)
        setIMR(0x00);
        setIMR2(0x00);
#elif (_WIZCHIP_ == W5500)
        setIMR(0x00);
#endif
        I_STATUS[sn] &= ~(Sn_IR_RECV);
#if (_WIZCHIP_ == W5100S)
        setIMR(0xff);
        setIMR2(0x01);
#elif (_WIZCHIP_ == W5500)
        setIMR(0xff);
#endif
        if ((getSn_RX_RSR(sn)) > 0)//正常通信时的实时数据接收
        {
            len = getSn_RX_RSR(sn);

            if (len > ETHERNET_BUF_MAX_SIZE)
            {
                len = ETHERNET_BUF_MAX_SIZE;
            }
            len = recv(sn, buf, len);
            buf[len] = 0x00;
            printf("%d:recv data:%s\r\n", sn, buf);
						
						
						if (strncmp((char*)buf, "LIST", 4) == 0)
						{
								tcp_send_device_list(sn);
								return;
						}
						if (strncmp((char*)buf, "BIND,", 5) == 0)
						{
								tcp_handle_bind_cmd(sn,(const char *)buf);
								return;
						}
        }
    }

    if (I_STATUS[sn] & Sn_IR_SENDOK)
    {
        I_STATUS[sn] &= ~(Sn_IR_SENDOK);
    }
}
