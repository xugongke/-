#include "interrupt.h"
#include "socket.h"
#include "wiz_interface.h"
#include <stdio.h>
#include <string.h>
#include "main.h"
#include "stream_buffer.h"
#include "host_comm.h"
#include "device_manager.h"

#define ETHERNET_BUF_MAX_SIZE (1024 * 2)

#if (_WIZCHIP_ == W5500)
#define IR_SOCK(ch) (0x01 << ch)
#endif

enum SN_STATUS
{
    closed_status = 0,
    ready_status,
    connected_status,
};

static uint8_t I_STATUS[_WIZCHIP_SOCK_NUM_] = {0};
static uint8_t ch_status[_WIZCHIP_SOCK_NUM_] = {0};

/* TCP连接状态标志 (供其他模块查询) */
volatile uint8_t g_tcp_connected = 0;

/* 外部流缓冲区句柄 (user_main.c中创建) */
extern StreamBufferHandle_t tcp_stream_buf;

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

/**
 * @brief   TCP服务器连接状态管理 + 原始数据接收
 * @param   sn:   socket编号
 * @param   buf:  接收缓冲区 (仅用于断开时读取残留数据)
 * @param   port: 监听端口
 * @note    接收到的数据直接送入流缓冲区，不在此处做协议解析
 */
void loopback_tcps_interrupt(uint8_t sn, uint8_t *buf, uint16_t port)
{
    uint16_t len = 0;
    uint8_t destip[4];
    uint16_t destport;

    /* ---- Socket关闭处理 ---- */
    if (I_STATUS[sn] == SOCK_CLOSED)
    {
        if (!ch_status[sn])
        {
            printf("%d:TCP server start\r\n", sn);
            ch_status[sn] = ready_status;

            if (socket(sn, Sn_MR_TCP, port, 0x00) != sn)
            {
                ch_status[sn] = closed_status;
            }
            else
            {
                printf("%d:Socket opened\r\n", sn);
                listen(sn);
                printf("%d:Listen, TCP server loopback, port [%d]\r\n", sn, port);
            }
        }
    }

    /* ---- 新连接 ---- */
    if (I_STATUS[sn] & Sn_IR_CON)
    {
        getSn_DIPR(sn, destip);
        destport = getSn_DPORT(sn);
        printf("%d:Connected - %d.%d.%d.%d : %d\r\n",
               sn, destip[0], destip[1], destip[2], destip[3], destport);

        ch_status[sn] = connected_status;
        g_tcp_connected = 1;
        g_host_busy = 1;
        I_STATUS[sn] &= ~(Sn_IR_CON);
    }

    /* ---- 断开连接 ---- */
    if (I_STATUS[sn] & Sn_IR_DISCON)
    {
        printf("%d:套接字已断开\r\n", sn);
        g_tcp_connected = 0;
        g_host_busy = 0;
        tcp_clear_search_socket();

        /* 读取并丢弃RX缓冲区中的残留数据 */
        if ((getSn_RX_RSR(sn)) > 0)
        {
            len = getSn_RX_RSR(sn);
            if (len > ETHERNET_BUF_MAX_SIZE)
                len = ETHERNET_BUF_MAX_SIZE;
            recv(sn, buf, len);
        }
        disconnect(sn);
        ch_status[sn] = closed_status;
        I_STATUS[sn] &= ~(Sn_IR_DISCON);
    }

    /* ---- 数据接收: 仅转发原始字节到流缓冲区 ---- */
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

        if ((getSn_RX_RSR(sn)) > 0)
        {
            len = getSn_RX_RSR(sn);
            if (len > ETHERNET_BUF_MAX_SIZE)
                len = ETHERNET_BUF_MAX_SIZE;
            len = recv(sn, buf, len);

            /* 将原始字节流送入Stream Buffer */
            if (tcp_stream_buf != NULL && len > 0)
            {
                xStreamBufferSend(tcp_stream_buf, buf, len, 0);
            }
        }
    }

    /* ---- 发送完成 ---- */
    if (I_STATUS[sn] & Sn_IR_SENDOK)
    {
        I_STATUS[sn] &= ~(Sn_IR_SENDOK);
    }
}
