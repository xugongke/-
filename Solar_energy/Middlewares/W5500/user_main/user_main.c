#include "user_main.h"
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "wizchip_conf.h"
#include "wiz_interface.h"
#include "interrupt.h"
#include "socket.h"

#include "cmsis_os.h"
#include "stream_buffer.h"
#include "main.h"

/* ==================== 网络配置 ==================== */
wiz_NetInfo default_net_info = {
    .mac = {0x00, 0x08, 0xdc, 0x12, 0x22, 0x12},
    .ip = {192, 168, 1, 139},
    .gw = {192, 168, 1, 1},
    .sn = {255, 255, 255, 0},
    .dns = {8, 8, 8, 8},
    .dhcp = NETINFO_DHCP
};

static uint16_t local_port = 8080;
static uint8_t ethernet_buf[ETHERNET_BUF_MAX_SIZE] = {0};

/* ==================== 流缓冲区 ==================== */
StreamBufferHandle_t tcp_stream_buf = NULL;

/* ==================== 帧解析器 ==================== */
static FrameParser_t frame_parser;

/* ==================== 外部变量 (interrupt.c中定义) ==================== */
extern volatile uint8_t g_tcp_connected;

/* ================================================================
 *  帧解析状态机实现
 * ================================================================ */

void frame_parser_init(FrameParser_t *parser)
{
    memset(parser, 0, sizeof(FrameParser_t));
    parser->state = FRAME_STATE_WAIT_HEADER;
}

void frame_parser_reset(FrameParser_t *parser)
{
    parser->state = FRAME_STATE_WAIT_HEADER;
    parser->header_idx = 0;
    parser->length_idx = 0;
    parser->data_len = 0;
    parser->data_index = 0;
    parser->tail_idx = 0;
}

/**
 * @brief   帧解析状态机 - 逐字节消费流数据
 *
 * 协议格式: [0xAA][0xAA][LEN_H][LEN_L][DATA...][0x0D][0x0A]
 */
static void frame_parser_process_byte(FrameParser_t *parser, uint8_t byte)
{
    switch (parser->state)
    {
        /* ---- 状态1: 等待帧头 ---- */
        case FRAME_STATE_WAIT_HEADER:
        {
            uint8_t header[FRAME_HEADER_LEN] = {FRAME_HEADER_0, FRAME_HEADER_1};

            if (byte == header[parser->header_idx])
            {
                parser->header_idx++;
                if (parser->header_idx >= FRAME_HEADER_LEN)
                {
                    /* 帧头匹配完成，进入长度字段接收 */
                    parser->state = FRAME_STATE_WAIT_LENGTH;
                    parser->length_idx = 0;
                }
            }
            else
            {
                /* 帧头不匹配，重新搜索 */
                parser->header_idx = 0;
                /* 优化：如果当前字节恰好是帧头第1字节，不要丢弃 */
                if (byte == header[0])
                {
                    parser->header_idx = 1;
                }
            }
            break;
        }

        /* ---- 状态2: 等待数据长度字段 ---- */
        case FRAME_STATE_WAIT_LENGTH:
        {
            parser->length_buf[parser->length_idx++] = byte;

            if (parser->length_idx >= FRAME_LENGTH_LEN)
            {
                /* 大端序: 第1字节是高字节 */
                parser->data_len = ((uint16_t)parser->length_buf[0] << 8) |
                                   ((uint16_t)parser->length_buf[1]);

                /* 合法性检查 */
                if (parser->data_len == 0 || parser->data_len > FRAME_MAX_DATA_LEN)
                {
                    /* 长度异常，丢弃当前帧，重新搜索帧头 */
                    frame_parser_reset(parser);
                    return;
                }

                parser->state = FRAME_STATE_WAIT_DATA;
                parser->data_index = 0;
            }
            break;
        }

        /* ---- 状态3: 等待数据域 ---- */
        case FRAME_STATE_WAIT_DATA:
        {
            parser->data_buf[parser->data_index++] = byte;

            if (parser->data_index >= parser->data_len)
            {
                /* 数据域接收完成，进入帧尾接收 */
                parser->state = FRAME_STATE_WAIT_TAIL;
                parser->tail_idx = 0;
            }
            break;
        }

        /* ---- 状态4: 等待帧尾 ---- */
        case FRAME_STATE_WAIT_TAIL:
        {
            uint8_t tail[FRAME_TAIL_LEN] = {FRAME_TAIL_0, FRAME_TAIL_1};

            if (byte == tail[parser->tail_idx])
            {
                parser->tail_idx++;
                if (parser->tail_idx >= FRAME_TAIL_LEN)
                {
                    /* ★ 一帧完整数据接收成功！★ */
                    tcp_frame_handler(parser->data_buf, parser->data_len);

                    /* 重置解析器，准备接收下一帧 */
                    frame_parser_reset(parser);
                }
            }
            else
            {
                /* 帧尾校验失败，丢弃当前帧 */
                frame_parser_reset(parser);
                /* 检查当前字节是否是新帧的帧头 */
                if (byte == FRAME_HEADER_0)
                {
                    parser->header_idx = 1;
                }
            }
            break;
        }

        default:
            frame_parser_reset(parser);
            break;
    }
}

void frame_parser_feed(FrameParser_t *parser, const uint8_t *data, uint16_t len)
{
    for (uint16_t i = 0; i < len; i++)
    {
        frame_parser_process_byte(parser, data[i]);
    }
}

/* ================================================================
 *  用户回调 - 在此实现命令分发逻辑
 * ================================================================ */

/**
 * @brief   帧接收完成回调
 * @param   data: 数据域指针 (已去除帧头、长度、帧尾)
 * @param   len:  数据域长度
 * @note    根据你的实际协议，在这里解析数据域中的命令码并执行对应操作
 *
 * 示例协议数据域格式 (请替换为你实际的协议):
 *   data[0] = 命令码
 *   data[1..] = 参数
 */
void tcp_frame_handler(const uint8_t *data, uint16_t len)
{
    /* TODO: 根据实际协议解析命令并执行
     *
     * 示例:
     *   uint8_t cmd = data[0];
     *   switch(cmd)
     *   {
     *       case 0x01:  // 查询设备列表
     *           tcp_send_device_list(SOCKET_ID);
     *           break;
     *       case 0x02:  // 绑定设备
     *           tcp_handle_bind(SOCKET_ID, data, len);
     *           break;
     *       ...
     *   }
     */
}

/**
 * @brief   按帧协议格式组装并发送数据
 * @param   data: 数据域指针
 * @param   len:  数据域长度
 */
void tcp_send_frame(const uint8_t *data, uint16_t len)
{
    if (!g_tcp_connected)
        return;

    static uint8_t send_buf[FRAME_HEADER_LEN + FRAME_LENGTH_LEN + FRAME_MAX_DATA_LEN + FRAME_TAIL_LEN];
    uint16_t idx = 0;

    /* 帧头 */
    send_buf[idx++] = FRAME_HEADER_0;
    send_buf[idx++] = FRAME_HEADER_1;

    /* 数据长度 (大端序) */
    send_buf[idx++] = (uint8_t)(len >> 8);
    send_buf[idx++] = (uint8_t)(len & 0xFF);

    /* 数据域 */
    memcpy(&send_buf[idx], data, len);
    idx += len;

    /* 帧尾 */
    send_buf[idx++] = FRAME_TAIL_0;
    send_buf[idx++] = FRAME_TAIL_1;

    send(SOCKET_ID, send_buf, idx);
}

/* ================================================================
 *  W5500 主任务
 * ================================================================ */

void W5500_Task(void *argument)
{
    /* wizchip 初始化 */
    printf("W5500 stream buffer + frame parser example\r\n");
    wizchip_initialize();

    /* 设置网络信息 */
    network_init(ethernet_buf, &default_net_info);
    setSIMR(0xff);
    setSn_IMR(SOCKET_ID, 0x0f);

    /* 创建流缓冲区 */
    tcp_stream_buf = xStreamBufferCreate(TCP_STREAM_BUF_SIZE, 1);
    if (tcp_stream_buf == NULL)
    {
        printf("TCP stream buffer create failed!\r\n");
        Error_Handler();
    }

    /* 初始化帧解析器 */
    frame_parser_init(&frame_parser);

    /* Infinite loop */
    for (;;)
    {
        uint8_t recv_buf[128];
        size_t received_bytes = xStreamBufferReceive(tcp_stream_buf,
                                                     recv_buf,
                                                     sizeof(recv_buf),
                                                     pdMS_TO_TICKS(1000));

        if (received_bytes > 0)
        {
            /* 喂给帧解析状态机 */
            frame_parser_feed(&frame_parser, recv_buf, (uint16_t)received_bytes);
        }

        /* 处理TCP连接状态管理 (放在主循环中以非阻塞方式运行) */
        loopback_tcps_interrupt(SOCKET_ID, ethernet_buf, local_port);
    }
}
