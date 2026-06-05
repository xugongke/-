#include "user_main.h"
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "wizchip_conf.h"
#include "wiz_interface.h"
#include "socket.h"

#include "cmsis_os.h"
#include "stream_buffer.h"
#include "semphr.h"
#include "main.h"
#include "tcp_cmd_handler.h"

/* ==================== 网络配置 ==================== */
wiz_NetInfo default_net_info = {
    .mac = {0x00, 0x08, 0xdc, 0x12, 0x22, 0x12},
    .ip = {192, 168, 1, 139},
    .gw = {192, 168, 1, 1},
    .sn = {255, 255, 255, 0},
    .dns = {8, 8, 8, 8},
    .dhcp = NETINFO_DHCP
};

/* 上位机服务器IP和端口 */
static uint8_t  server_ip[4] = {192, 168, 6, 196};
static uint16_t server_port  = 22222;

static uint8_t ethernet_buf[ETHERNET_BUF_MAX_SIZE] = {0};

/* ==================== 流缓冲区 ==================== */
StreamBufferHandle_t tcp_stream_buf = NULL;

/* ==================== W5500中断信号量 ==================== */
SemaphoreHandle_t w5500_int_sem = NULL;

/* ==================== 帧解析器 ==================== */
static FrameParser_t frame_parser;

/* ==================== 连接状态标志 ==================== */
volatile uint8_t g_tcp_connected = 0;

/* ================================================================
 *  CRC16-CCITT 软件实现 (反射版本, 与上位机一致)
 *  多项式: 0x1021
 *  初始值: 0x0000
 *  输入/输出反射: 是
 * ================================================================ */

/**
 * @brief   CRC16 反射查表 (CrctableCCITTRefl, 与上位机Qt版本完全一致)
 */
static const uint16_t crc16_table[256] = {
    0x0000, 0x1189, 0x2312, 0x329B, 0x4624, 0x57AD, 0x6536, 0x74BF,
    0x8C48, 0x9DC1, 0xAF5A, 0xBED3, 0xCA6C, 0xDBE5, 0xE97E, 0xF8F7,
    0x1081, 0x0108, 0x3393, 0x221A, 0x56A5, 0x472C, 0x75B7, 0x643E,
    0x9CC9, 0x8D40, 0xBFDB, 0xAE52, 0xDAED, 0xCB64, 0xF9FF, 0xE876,
    0x2102, 0x308B, 0x0210, 0x1399, 0x6726, 0x76AF, 0x4434, 0x55BD,
    0xAD4A, 0xBCC3, 0x8E58, 0x9FD1, 0xEB6E, 0xFAE7, 0xC87C, 0xD9F5,
    0x3183, 0x200A, 0x1291, 0x0318, 0x77A7, 0x662E, 0x54B5, 0x453C,
    0xBDCB, 0xAC42, 0x9ED9, 0x8F50, 0xFBEF, 0xEA66, 0xD8FD, 0xC974,
    0x4204, 0x538D, 0x6116, 0x709F, 0x0420, 0x15A9, 0x2732, 0x36BB,
    0xCE4C, 0xDFC5, 0xED5E, 0xFCD7, 0x8868, 0x99E1, 0xAB7A, 0xBAF3,
    0x5285, 0x430C, 0x7197, 0x601E, 0x14A1, 0x0528, 0x37B3, 0x263A,
    0xDECD, 0xCF44, 0xFDDF, 0xEC56, 0x98E9, 0x8960, 0xBBFB, 0xAA72,
    0x6306, 0x728F, 0x4014, 0x519D, 0x2522, 0x34AB, 0x0630, 0x17B9,
    0xEF4E, 0xFEC7, 0xCC5C, 0xDDD5, 0xA96A, 0xB8E3, 0x8A78, 0x9BF1,
    0x7387, 0x620E, 0x5095, 0x411C, 0x35A3, 0x242A, 0x16B1, 0x0738,
    0xFFCF, 0xEE46, 0xDCDD, 0xCD54, 0xB9EB, 0xA862, 0x9AF9, 0x8B70,
    0x8408, 0x9581, 0xA71A, 0xB693, 0xC22C, 0xD3A5, 0xE13E, 0xF0B7,
    0x0840, 0x19C9, 0x2B52, 0x3ADB, 0x4E64, 0x5FED, 0x6D76, 0x7CFF,
    0x9489, 0x8500, 0xB79B, 0xA612, 0xD2AD, 0xC324, 0xF1BF, 0xE036,
    0x18C1, 0x0948, 0x3BD3, 0x2A5A, 0x5EE5, 0x4F6C, 0x7DF7, 0x6C7E,
    0xA50A, 0xB483, 0x8618, 0x9791, 0xE32E, 0xF2A7, 0xC03C, 0xD1B5,
    0x2942, 0x38CB, 0x0A50, 0x1BD9, 0x6F66, 0x7EEF, 0x4C74, 0x5DFD,
    0xB58B, 0xA402, 0x9699, 0x8710, 0xF3AF, 0xE226, 0xD0BD, 0xC134,
    0x39C3, 0x284A, 0x1AD1, 0x0B58, 0x7FE7, 0x6E6E, 0x5CF5, 0x4D7C,
    0xC60C, 0xD785, 0xE51E, 0xF497, 0x8028, 0x91A1, 0xA33A, 0xB2B3,
    0x4A44, 0x5BCD, 0x6956, 0x78DF, 0x0C60, 0x1DE9, 0x2F72, 0x3EFB,
    0xD68D, 0xC704, 0xF59F, 0xE416, 0x90A9, 0x8120, 0xB3BB, 0xA232,
    0x5AC5, 0x4B4C, 0x79D7, 0x685E, 0x1CE1, 0x0D68, 0x3FF3, 0x2E7A,
    0xE70E, 0xF687, 0xC41C, 0xD595, 0xA12A, 0xB0A3, 0x8238, 0x93B1,
    0x6B46, 0x7ACF, 0x4854, 0x59DD, 0x2D62, 0x3CEB, 0x0E70, 0x1FF9,
    0xF78F, 0xE606, 0xD49D, 0xC514, 0xB1AB, 0xA022, 0x92B9, 0x8330,
    0x7BC7, 0x6A4E, 0x58D5, 0x495C, 0x3DE3, 0x2C6A, 0x1EF1, 0x0F78
};

/**
 * @brief   CRC16-CCITT 反射版本 (与上位机Qt版本完全一致)
 *          多项式0x1021, 初始值0x0000, 输入输出反射
 */
uint16_t crc16_ccitt(const uint8_t *data, uint16_t len)
{
    uint16_t crc = 0x0000;
    for (uint16_t i = 0; i < len; i++)
    {
        crc = (crc >> 8) ^ crc16_table[(crc ^ data[i]) & 0xFF];
    }
    return crc;
}

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
    parser->frame_type = 0;
    parser->frame_cmd = 0;
    parser->length_idx = 0;
    parser->data_len = 0;
    parser->data_index = 0;
    parser->crc_idx = 0;
}

/**
 * @brief   帧解析状态机 - 逐字节消费流数据
 *
 * 协议格式: [0x0D][TYPE][CMD][LEN_H][LEN_L][DATA...][CRC_H][CRC_L][0x0E]
 * CRC范围: 帧头 + TYPE + CMD + LEN_H + LEN_L + DATA...
 */
static void frame_parser_process_byte(FrameParser_t *parser, uint8_t byte)
{
    switch (parser->state)
    {
        /* ---- 状态1: 等待帧头 0x0D ---- */
        case FRAME_STATE_WAIT_HEADER:
        {
            if (byte == FRAME_HEADER)
            {
                parser->state = FRAME_STATE_WAIT_TYPE;
                parser->frame_type = 0;
                parser->frame_cmd = 0;
                parser->length_idx = 0;
                parser->data_len = 0;
                parser->data_index = 0;
                parser->crc_idx = 0;
            }
            break;
        }

        /* ---- 状态2: 等待帧类型 (1字节) ---- */
        case FRAME_STATE_WAIT_TYPE:
        {
            parser->frame_type = byte;
            parser->state = FRAME_STATE_WAIT_CMD;
            break;
        }

        /* ---- 状态3: 等待命令字 (1字节) ---- */
        case FRAME_STATE_WAIT_CMD:
        {
            parser->frame_cmd = byte;
            parser->state = FRAME_STATE_WAIT_LENGTH;
            parser->length_idx = 0;
            break;
        }

        /* ---- 状态4: 等待数据长度 (2字节, 大端序) ---- */
        case FRAME_STATE_WAIT_LENGTH:
        {
            parser->length_buf[parser->length_idx++] = byte;

            if (parser->length_idx >= FRAME_LENGTH_LEN)
            {
                parser->data_len = ((uint16_t)parser->length_buf[0] << 8) |
                                   ((uint16_t)parser->length_buf[1]);

                if (parser->data_len > FRAME_MAX_DATA_LEN)
                {
                    frame_parser_reset(parser);
                    return;
                }

                if (parser->data_len == 0)
                {
                    parser->state = FRAME_STATE_WAIT_CRC;
                    parser->crc_idx = 0;
                }
                else
                {
                    parser->state = FRAME_STATE_WAIT_DATA;
                    parser->data_index = 0;
                }
            }
            break;
        }

        /* ---- 状态5: 等待数据域 ---- */
        case FRAME_STATE_WAIT_DATA:
        {
            parser->data_buf[parser->data_index++] = byte;

            if (parser->data_index >= parser->data_len)
            {
                parser->state = FRAME_STATE_WAIT_CRC;
                parser->crc_idx = 0;
            }
            break;
        }

        /* ---- 状态6: 等待CRC16校验 (2字节, 小端序) ---- */
        case FRAME_STATE_WAIT_CRC:
        {
            parser->crc_buf[parser->crc_idx++] = byte;

            if (parser->crc_idx >= FRAME_CRC_LEN)
            {
                parser->state = FRAME_STATE_WAIT_TAIL;
            }
            break;
        }

        /* ---- 状态7: 等待帧尾 0x0E ---- */
        case FRAME_STATE_WAIT_TAIL:
        {
            if (byte == FRAME_TAIL)
            {
                /* CRC校验: 帧头 + 帧类型 + 命令字 + 长度 + 数据域 */
                uint16_t calc_crc = 0x0000;
                calc_crc = (calc_crc >> 8) ^ crc16_table[(calc_crc ^ FRAME_HEADER) & 0xFF];
                calc_crc = (calc_crc >> 8) ^ crc16_table[(calc_crc ^ parser->frame_type) & 0xFF];
                calc_crc = (calc_crc >> 8) ^ crc16_table[(calc_crc ^ parser->frame_cmd)  & 0xFF];
                calc_crc = (calc_crc >> 8) ^ crc16_table[(calc_crc ^ parser->length_buf[0]) & 0xFF];
                calc_crc = (calc_crc >> 8) ^ crc16_table[(calc_crc ^ parser->length_buf[1]) & 0xFF];
                for (uint16_t i = 0; i < parser->data_len; i++)
                {
                    calc_crc = (calc_crc >> 8) ^ crc16_table[(calc_crc ^ parser->data_buf[i]) & 0xFF];
                }
                /* CRC以小端序存储 */
                uint16_t recv_crc = (uint16_t)parser->crc_buf[0] |
                                    ((uint16_t)parser->crc_buf[1] << 8);

                if (calc_crc == recv_crc)
                {
                    tcp_frame_handler(parser->frame_type,
                                      parser->frame_cmd,
                                      parser->data_buf,
                                      parser->data_len);
                }
                else
                {
                    printf("CRC error: calc=0x%04X recv=0x%04X\r\n",
                           calc_crc, recv_crc);
                }
            }
            else
            {
                printf("Frame tail error: expected 0x%02X, got 0x%02X\r\n",
                       FRAME_TAIL, byte);
            }

            frame_parser_reset(parser);
            if (byte == FRAME_HEADER)
            {
                parser->state = FRAME_STATE_WAIT_TYPE;
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
 *  帧组装与发送
 * ================================================================ */

/**
 * @brief   按帧协议格式组装数据到指定缓冲区
 * @param   buf:  输出缓冲区
 * @param   type: 帧类型
 * @param   cmd:  命令字
 * @param   data: 数据域指针
 * @param   len:  数据域长度
 * @return  帧总长度
 */
static uint16_t build_frame(uint8_t *buf, uint8_t type, uint8_t cmd,
                            const uint8_t *data, uint16_t len)
{
    uint16_t idx = 0;

    buf[idx++] = FRAME_HEADER;
    buf[idx++] = type;
    buf[idx++] = cmd;
    buf[idx++] = (uint8_t)(len >> 8);
    buf[idx++] = (uint8_t)(len & 0xFF);

    if (len > 0 && data != NULL)
    {
        memcpy(&buf[idx], data, len);
        idx += len;
    }

    uint16_t crc = crc16_ccitt(&buf[0], idx);  /* CRC范围包含帧头 */
    buf[idx++] = (uint8_t)(crc & 0xFF);
    buf[idx++] = (uint8_t)((crc >> 8) & 0xFF);
    buf[idx++] = FRAME_TAIL;

    return idx;
}

/**
 * @brief   帧接收完成回调
 */
void tcp_frame_handler(uint8_t type, uint8_t cmd, const uint8_t *data, uint16_t len)
{
    tcp_dispatch_frame(type, cmd, data, len);
}

/**
 * @brief   按帧协议格式组装并发送数据到TCP服务器
 */
void tcp_send_frame(uint8_t type, uint8_t cmd, const uint8_t *data, uint16_t len)
{
    if (!g_tcp_connected)
        return;

    static uint8_t send_buf[1 + FRAME_TYPE_LEN + FRAME_CMD_LEN + FRAME_LENGTH_LEN +
                            FRAME_MAX_DATA_LEN + FRAME_CRC_LEN + 1];

    uint16_t frame_len = build_frame(send_buf, type, cmd, data, len);
    send(TCP_SOCKET_ID, send_buf, frame_len);
}

/* ================================================================
 *  TCP客户端连接管理
 * ================================================================ */

/**
 * @brief   尝试连接上位机TCP服务器
 * @return  0=成功, -1=失败
 */
static int tcp_client_connect(void)
{
    printf("TCP connecting to %d.%d.%d.%d:%d...\r\n",
           server_ip[0], server_ip[1], server_ip[2], server_ip[3], server_port);

    /* 先关闭socket */
    close(TCP_SOCKET_ID);

    /* 打开TCP socket */
    if (socket(TCP_SOCKET_ID, Sn_MR_TCP, 0, 0x00) != TCP_SOCKET_ID)
    {
        printf("Socket open failed\r\n");
        return -1;
    }

    /* 连接服务器 */
    if (connect(TCP_SOCKET_ID, server_ip, server_port) != SOCK_OK)
    {
        printf("Connect failed\r\n");
        close(TCP_SOCKET_ID);
        return -1;
    }

    printf("TCP connected\r\n");
    g_tcp_connected = 1;
    return 0;
}

/* ================================================================
 *  TCP客户端事件处理
 * ================================================================ */

/**
 * @brief   处理Socket 1的中断事件
 * @note    与loopback_tcps_interrupt类似, 但为客户端模式
 */
static void tcp_client_process(void)
{
    uint16_t len;

    /* 检查连接状态 */
    uint8_t sr = getSn_SR(TCP_SOCKET_ID);

    if (sr == SOCK_ESTABLISHED)
    {
        if (!g_tcp_connected)
        {
            g_tcp_connected = 1;
            printf("TCP client: connected\r\n");
        }

        /* 读取数据 */
        len = getSn_RX_RSR(TCP_SOCKET_ID);
        if (len > 0)
        {
            if (len > ETHERNET_BUF_MAX_SIZE)
                len = ETHERNET_BUF_MAX_SIZE;
            len = recv(TCP_SOCKET_ID, ethernet_buf, len);

            if (tcp_stream_buf != NULL && len > 0)
            {
                xStreamBufferSend(tcp_stream_buf, ethernet_buf, len, 0);
            }
        }
    }
    else if (sr == SOCK_CLOSE_WAIT)
    {
        disconnect(TCP_SOCKET_ID);
        g_tcp_connected = 0;
        printf("TCP client: close wait, disconnecting\r\n");
    }
    else if (sr == SOCK_CLOSED)
    {
        if (g_tcp_connected)
        {
            g_tcp_connected = 0;
            printf("TCP client: disconnected\r\n");
        }
        /* 不在此处重连, 由主循环处理 */
    }
}

/* ================================================================
 *  W5500 主任务
 * ================================================================ */

void W5500_Task(void *argument)
{
    /* wizchip 初始化 */
    printf("W5500 TCP client\r\n");
    wizchip_initialize();

    /* 设置网络信息 */
    network_init(ethernet_buf, &default_net_info);

    /* 启用Socket中断 */
    setSIMR(0xff);
    setSn_IMR(TCP_SOCKET_ID, 0x0f);

    /* 创建流缓冲区 */
    tcp_stream_buf = xStreamBufferCreate(TCP_STREAM_BUF_SIZE, 1);
    if (tcp_stream_buf == NULL)
    {
        printf("TCP stream buffer create failed!\r\n");
        Error_Handler();
    }

    /* 创建W5500中断信号量 (二值信号量, 初始为空) */
    w5500_int_sem = xSemaphoreCreateBinary();
    if (w5500_int_sem == NULL)
    {
        printf("W5500 interrupt semaphore create failed!\r\n");
        Error_Handler();
    }

    /* 初始化帧解析器 */
    frame_parser_init(&frame_parser);

    /* 尝试连接上位机服务器 */
    tcp_client_connect();

    /* Infinite loop */
    for (;;)
    {
        /* 阻塞等待W5500中断信号量, 超时3秒 (也用于断线重连检测) */
        xSemaphoreTake(w5500_int_sem, pdMS_TO_TICKS(3000));

        /* 处理TCP客户端事件 (连接/断开/数据接收) */
        tcp_client_process();

        /* 断线重连 */
        if (!g_tcp_connected)
        {
            tcp_client_connect();
        }

        /* 从Stream Buffer中取出数据, 喂给帧解析状态机 (非阻塞) */
        uint8_t recv_buf[128];
        size_t received_bytes;
        while ((received_bytes = xStreamBufferReceive(tcp_stream_buf,
                                                       recv_buf,
                                                       sizeof(recv_buf),
                                                       0)) > 0)
        {
            frame_parser_feed(&frame_parser, recv_buf, (uint16_t)received_bytes);
        }
    }
}
