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

static uint16_t tcp_port = 8080;
static uint8_t ethernet_buf[ETHERNET_BUF_MAX_SIZE] = {0};

/* ==================== 流缓冲区 ==================== */
StreamBufferHandle_t tcp_stream_buf = NULL;

/* ==================== W5500中断信号量 ==================== */
SemaphoreHandle_t w5500_int_sem = NULL;

/* ==================== 帧解析器 ==================== */
static FrameParser_t frame_parser;

/* ==================== UDP发现相关 ==================== */
static uint8_t udp_buf[64];
static uint16_t udp_remote_port;

/* ==================== 外部变量 (interrupt.c中定义) ==================== */
extern volatile uint8_t g_tcp_connected;

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
 * CRC范围: TYPE + CMD + LEN_H + LEN_L + DATA...
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
                /* 重置其他字段 */
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
                    /* 数据长度为0, 跳过数据域, 直接进入CRC接收 */
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
                /* 数据域接收完成, 进入CRC接收 */
                parser->state = FRAME_STATE_WAIT_CRC;
                parser->crc_idx = 0;
            }
            break;
        }

        /* ---- 状态6: 等待CRC16校验 (2字节, 大端序) ---- */
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
                /* ★ 帧尾匹配, 校验CRC ★ */

                /* 增量式CRC计算: 帧头 + 帧类型 + 命令字 + 长度 + 数据域 (与上位机一致) */
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
                /* CRC以小端序存储 (与上位机x86一致) */
                uint16_t recv_crc = (uint16_t)parser->crc_buf[0] |
                                    ((uint16_t)parser->crc_buf[1] << 8);

                if (calc_crc == recv_crc)
                {
                    /* CRC校验通过, 回调处理 */
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

            /* 无论成功失败, 重置解析器 */
            frame_parser_reset(parser);

            /* 如果当前字节恰好是新帧头 */
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
 *  用户回调 - 在此实现命令分发逻辑
 * ================================================================ */

/**
 * @brief   帧接收完成回调
 * @param   type: 帧类型
 * @param   cmd:  命令字
 * @param   data: 数据域指针
 * @param   len:  数据域长度
 */
void tcp_frame_handler(uint8_t type, uint8_t cmd, const uint8_t *data, uint16_t len)
{
    /* 命令分发: 交给 tcp_cmd_handler 处理 */
    tcp_dispatch_frame(type, cmd, data, len);
}

/**
 * @brief   按帧协议格式组装并发送数据
 * @param   type: 帧类型
 * @param   cmd:  命令字
 * @param   data: 数据域指针
 * @param   len:  数据域长度
 *
 * 发送格式: [0x0D][TYPE][CMD][LEN_H][LEN_L][DATA...][CRC_H][CRC_L][0x0E]
 */
/**
 * @brief   按帧协议格式组装数据到指定缓冲区 (内部共用)
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
 *  UDP设备发现处理
 * ================================================================ */

/**
 * @brief   检查是否为合法发现请求帧
 * @param   data: 收到的UDP数据
 * @param   len:  数据长度
 * @return  1=合法发现请求, 0=不是
 */
static int is_discover_frame(const uint8_t *data, uint16_t len)
{
    /* 完整帧长度: 1(头)+1(type)+1(cmd)+2(len)+2(crc)+1(tail) = 8字节 */
    if (len != 8)
        return 0;

    /* 检查帧头 */
    if (data[0] != FRAME_HEADER)
        return 0;

    /* 检查帧类型和命令字 */
    if (data[1] != FRAME_TYPE_REQUEST || data[2] != 0x01)
        return 0;

    /* 检查数据域长度为0 */
    if (data[3] != 0x00 || data[4] != 0x00)
        return 0;

    /* 检查CRC */
    uint16_t calc_crc = crc16_ccitt(&data[0], 5);  /* CRC范围: header+type+cmd+len */
    uint16_t recv_crc = (uint16_t)data[5] | ((uint16_t)data[6] << 8);
    if (calc_crc != recv_crc)
        return 0;

    /* 检查帧尾 */
    if (data[7] != FRAME_TAIL)
        return 0;

    return 1;
}

/**
 * @brief   处理UDP发现请求 (Socket 0)
 * @note    收到广播帧后, 验证CRC, 回复设备IP+TCP端口
 *
 *  响应数据域: [IP[4]][TCP端口H][TCP端口L] = 6字节
 */
void udp_discover_handler(void)
{
    uint16_t len;
    uint8_t remote_ip[4];
    uint16_t remote_port;

    len = getSn_RX_RSR(SOCKET_ID);
    if (len == 0)
        return;
    if (len > sizeof(udp_buf))
        len = sizeof(udp_buf);

    /* UDP接收: 获取发送方IP和端口 */
    len = recvfrom(SOCKET_ID, udp_buf, len, remote_ip, &remote_port);
    if (len == 0)
        return;

    printf("UDP recv from %d.%d.%d.%d:%d, len=%d\r\n",
           remote_ip[0], remote_ip[1], remote_ip[2], remote_ip[3], remote_port, len);

    /* 已建立TCP连接, 不再回复UDP广播 */
    if (g_tcp_connected)
        return;

    /* 验证是否为合法的设备发现帧 */
    if (!is_discover_frame(udp_buf, len))
        return;

    /* 组帧并发送UDP回复 (数据域长度为0) */
    uint8_t send_frame[32];
    uint16_t frame_len = build_frame(send_frame, FRAME_TYPE_RESPONSE,
                                     0x01, NULL, 0);

    sendto(SOCKET_ID, send_frame, frame_len, remote_ip, remote_port);
    printf("UDP discover reply sent\r\n");
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
    setSIMR(0xff);              /* 启用所有Socket中断 */
    setSn_IMR(SOCKET_ID, 0x0f);     /* Socket 0 (UDP) 中断 */
    setSn_IMR(TCP_SOCKET_ID, 0x0f); /* Socket 1 (TCP) 中断 */

    /* 初始化 Socket 0 为UDP (设备发现) */
    printf("Socket 0: UDP discover, port %d\r\n", UDP_DISCOVER_PORT);
    socket(SOCKET_ID, Sn_MR_UDP, UDP_DISCOVER_PORT, 0x00);

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

    /* Infinite loop
     *
     * 流程:
     * 1. 阻塞等待信号量 (EXTI中断释放)
     * 2. W5500中断发生 → wizchip_ISR()记录中断类型 → 释放信号量
     * 3. 任务被唤醒 → loopback_tcps_interrupt() 处理中断事件
     *    - 连接/断开事件: 管理TCP状态
     *    - 数据接收事件: recv() → 写入Stream Buffer
     * 4. 从Stream Buffer读取数据 → 喂给帧解析状态机 (超时0, 不阻塞)
     */
    for (;;)
    {
        /* 阻塞等待W5500中断信号量, 超时1秒 */
        xSemaphoreTake(w5500_int_sem, pdMS_TO_TICKS(1000));

        /* 处理 Socket 0: UDP设备发现 */
        udp_discover_handler();

        /* 处理 Socket 1: TCP服务器 (连接管理 + 数据接收 → 写入Stream Buffer) */
        loopback_tcps_interrupt(TCP_SOCKET_ID, ethernet_buf, tcp_port);

        /* 从Stream Buffer中取出数据, 喂给帧解析状态机 (非阻塞, 一次全部读完) */
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
