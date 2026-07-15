#include "user_main.h"
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "wizchip_conf.h"
#include "wiz_interface.h"
#include "wiz_platform.h"
#include "socket.h"

#include "cmsis_os.h"
#include "stream_buffer.h"
#include "semphr.h"
#include "main.h"
#include "tcp_cmd_handler.h"
#include "device_manager.h"
#include "fatfs.h"
#include "gui_guider.h"
#include "es1642_usage_guide.h"

/* ==================== 网络配置 ==================== */
wiz_NetInfo default_net_info = {
    .mac = {0x00, 0x08, 0xdc, 0x12, 0x22, 0x12},
    .ip = {192, 168, 1, 139},
    .gw = {192, 168, 1, 1},
    .sn = {255, 255, 255, 0},
    .dns = {8, 8, 8, 8},
    .dhcp = NETINFO_DHCP
};

/**
 * @brief 根据STM32唯一ID(UID)生成本地管理MAC地址
 * @note  每片芯片的96位UID不同, 保证多台主机MAC互不相同, 从而:
 *          1) DHCP 服务器会按不同MAC分配不同IP, 不会再IP冲突;
 *          2) 局域网内二层地址不冲突.
 *        MAC[0]=0x02 表示"本地管理单播地址"(LAM), 无需向IEEE购买.
 *        注意: 之前 default_net_info.mac 写死成同一个值, 两台主机烧同一固件
 *        就会MAC相同 -> DHCP分到同一个IP -> TCP连接被互相RST(10054).
 *
 *        === 如何获取/验证真实 UID ===
 *        方法1 (代码内): 本函数通过 HAL_GetUIDw0/1/2() 读取并 printf 打印,
 *                       串口终端即可看到 "UID = XXXXXXXX-XXXXXXXX-XXXXXXXX".
 *        方法2 (ST-Link): 用 STM32CubeProgrammer 连接芯片,
 *                        在 Memory 视图读取地址 0x1FFF7A10 处的 12 字节.
 *        方法3 (IDE调试): Keil/CubeIDE 调试时在 Watch 窗口添加表达式
 *                        *(uint32_t*)0x1FFF7A10 即可查看.
 *
 *        UID 地址说明 (STM32F4 全系列统一):
 *          UID_BASE = 0x1FFF7A10 (定义在 stm32f4xx.h, 由 CMSIS 提供)
 *          Word0 @ 0x1FFF7A10: [31:16]=Wafer_X/Y坐标, [15:8]=Wafer编号
 *          Word1 @ 0x1FFF7A14: Lot编号 (ASCII)
 *          Word2 @ 0x1FFF7A18: Lot编号续 + 其他
 *
 *        MAC 生成策略: 将 96 位 UID 的全部 12 字节通过 XOR 折叠到 5 字节,
 *        相比只取低位字节, 能更充分地利用 UID 信息, 避免同批次芯片 MAC 重复.
 */
static void generate_mac_from_uid(uint8_t *mac)
{
    /* 使用 HAL 库接口读取 96 位唯一 ID (底层即读 UID_BASE=0x1FFF7A10) */
    uint32_t uid0 = HAL_GetUIDw0();
    uint32_t uid1 = HAL_GetUIDw1();
    uint32_t uid2 = HAL_GetUIDw2();

    /* 将 12 字节 UID 通过 XOR 折叠到 5 字节, 充分利用全部 96 位信息 */
    uint8_t b[12];
    b[0]  = (uint8_t)(uid0);        b[1]  = (uint8_t)(uid0 >> 8);
    b[2]  = (uint8_t)(uid0 >> 16);  b[3]  = (uint8_t)(uid0 >> 24);
    b[4]  = (uint8_t)(uid1);        b[5]  = (uint8_t)(uid1 >> 8);
    b[6]  = (uint8_t)(uid1 >> 16);  b[7]  = (uint8_t)(uid1 >> 24);
    b[8]  = (uint8_t)(uid2);        b[9]  = (uint8_t)(uid2 >> 8);
    b[10] = (uint8_t)(uid2 >> 16);  b[11] = (uint8_t)(uid2 >> 24);

    mac[0] = 0x02U;  /* 本地管理地址 (LAM) + 单播 */
    mac[1] = b[0] ^ b[4] ^ b[8];
    mac[2] = b[1] ^ b[5] ^ b[9];
    mac[3] = b[2] ^ b[6] ^ b[10];
    mac[4] = b[3] ^ b[7] ^ b[11];
    mac[5] = b[0] ^ b[5] ^ b[10];  /* 交叉混合, 进一步增加区分度 */
}

/* 上位机服务器IP和端口 */
uint8_t  server_ip[4] = {192, 168, 6, 196};
uint16_t server_port  = 22222;

/* DMA收发缓冲区必须放在0x20000000的AHB SRAM区域, DMA控制器才能访问 */
__attribute__((section("RW_IRAM1")))
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
 * 协议格式: [0x0D][TYPE][CMD][LEN_L][LEN_H][DATA...][CRC_L][CRC_H][0x0E]
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

        /* ---- 状态4: 等待数据长度 (2字节, 小端序) ---- */
        case FRAME_STATE_WAIT_LENGTH:
        {
            parser->length_buf[parser->length_idx++] = byte;

            if (parser->length_idx >= FRAME_LENGTH_LEN)
            {
                /* len low + len high (little-endian) */
                parser->data_len = (uint16_t)parser->length_buf[0] |
                                   ((uint16_t)parser->length_buf[1] << 8);

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
                uint8_t crc_src[5 + FRAME_MAX_DATA_LEN];
                uint16_t crc_src_len = 0;
                crc_src[crc_src_len++] = FRAME_HEADER;
                crc_src[crc_src_len++] = parser->frame_type;
                crc_src[crc_src_len++] = parser->frame_cmd;
                crc_src[crc_src_len++] = parser->length_buf[0];
                crc_src[crc_src_len++] = parser->length_buf[1];
                if (parser->data_len > 0)
                {
                    memcpy(&crc_src[crc_src_len], parser->data_buf, parser->data_len);
                    crc_src_len += parser->data_len;
                }
                uint16_t calc_crc = crc16_ccitt(crc_src, crc_src_len);
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
    buf[idx++] = (uint8_t)(len & 0xFF);        /* len low */
    buf[idx++] = (uint8_t)((len >> 8) & 0xFF); /* len high */

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

    /* 发送缓冲区也经DMA传输, 必须放在DMA可访问内存区域 */
    __attribute__((section("RW_IRAM1")))
    static uint8_t send_buf[1 + FRAME_TYPE_LEN + FRAME_CMD_LEN + FRAME_LENGTH_LEN +
                            FRAME_MAX_DATA_LEN + FRAME_CRC_LEN + 1];

    uint16_t frame_len = build_frame(send_buf, type, cmd, data, len);
    int lenl = send(TCP_SOCKET_ID, send_buf, frame_len);
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
    printf("TCP 连接到 %d.%d.%d.%d:%d...\r\n",
           server_ip[0], server_ip[1], server_ip[2], server_ip[3], server_port);

    /* 连接尝试期间关闭Socket中断, 防止连接失败时产生中断风暴 */
    setSn_IMR(TCP_SOCKET_ID, 0x00);

    /* 先关闭socket */
    close(TCP_SOCKET_ID);

    /* 打开TCP socket */
    if (socket(TCP_SOCKET_ID, Sn_MR_TCP, 0, 0x00) != TCP_SOCKET_ID)
    {
        printf("套接字打开失败\r\n");
        return -1;
    }

    /* 连接服务器 */
    if (connect(TCP_SOCKET_ID, server_ip, server_port) != SOCK_OK)
    {
        printf("连接失败\r\n");
        close(TCP_SOCKET_ID);
        return -1;
    }

    g_device_manage_mode = 0;//建立连接时退出管理模式, 以便从机轮询继续工作

    printf("TCP 已连接\r\n");
    g_tcp_connected = 1;

    /* 启用TCP Keepalive: 30秒间隔(值=6, 单位5秒), 自动检测服务器非正常断开 */
    setSn_KPALVTR(TCP_SOCKET_ID, 6);
    /* 连接成功, 开启Socket中断 (RECV | DISCON | CON | TIMEOUT) */
    setSn_IMR(TCP_SOCKET_ID, Sn_IR_RECV | Sn_IR_DISCON | Sn_IR_CON | Sn_IR_TIMEOUT);

    return 0;
}

/* ================================================================
 *  TCP客户端事件处理
 * ================================================================ */

/**
 * @brief   处理Socket的中断事件
 * @note    在每次主循环中被调用, 负责清除中断标志并处理连接状态
 */
static void tcp_client_process(void)
{
    uint16_t len;

    /* 清除Socket中断标志, 但保留SENDOK给send()函数自己处理 */
    uint8_t sir = getSn_IR(TCP_SOCKET_ID);
    if (sir)
    {
        setSn_IR(TCP_SOCKET_ID, sir & ~Sn_IR_SENDOK);  /* 清除除SENDOK外的所有标志 */
    }

    /* 清除W5500公共中断寄存器 */
    uint8_t ir = getIR();
    if (ir)
    {
        setIR(ir);  /* 写1清除 */
    }

    /* 检查连接状态 */
    uint8_t sr = getSn_SR(TCP_SOCKET_ID);

    if (sr == SOCK_ESTABLISHED)
    {
        if (!g_tcp_connected)
        {
            g_tcp_connected = 1;
            /* 启用TCP Keepalive: 30秒间隔(值=6, 单位5秒) */
            setSn_KPALVTR(TCP_SOCKET_ID, 6);
            /* 连接恢复, 开启Socket中断 (含TIMEOUT) */
            setSn_IMR(TCP_SOCKET_ID, Sn_IR_RECV | Sn_IR_DISCON | Sn_IR_CON | Sn_IR_TIMEOUT);
            printf("TCP 已连接\r\n");
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
        g_device_manage_mode = 0;  /* TCP断开, 自动退出管理模式, 恢复从机轮询 */
        /* 断开连接, 关闭Socket中断防止中断风暴 */
        setSn_IMR(TCP_SOCKET_ID, 0x00);
        printf("TCP 客户端：关闭等待，正在断开连接\r\n");
    }
    else if (sr == SOCK_CLOSED)
    {
        if (g_tcp_connected)
        {
            g_tcp_connected = 0;
            g_device_manage_mode = 0;  /* TCP断开, 自动退出管理模式, 恢复从机轮询 */
            /* 断开连接, 关闭Socket中断防止中断风暴 */
            setSn_IMR(TCP_SOCKET_ID, 0x00);
            printf("TCP 客户端：已断开连接\r\n");
        }
        /* 不在此处重连, 由主循环处理 */
    }
}

/* ================================================================
 *  W5500 主任务
 * ================================================================ */

/**
 * @brief   获取当前服务器IP和端口
 */
void tcp_get_server_addr(uint8_t ip[4], uint16_t *port)
{
    memcpy(ip, server_ip, 4);
    *port = server_port;
}

/**
 * @brief   设置新的服务器IP和端口
 * @note    设置后会在 W5500_Task 主循环中自动触发断线重连
 */
void tcp_set_server_addr(const uint8_t ip[4], uint16_t port)
{
    memcpy(server_ip, ip, 4);
    server_port = port;
    //只有在已经建立连接的时候才触发断线重连
    if(g_tcp_connected)
    {
        /* 触发断线重连 */
        disconnect(TCP_SOCKET_ID);
        g_tcp_connected = 0;
        g_device_manage_mode = 0;  /* 主动断开重连, 自动退出管理模式 */
        printf("TCP 客户端：服务器地址已更新，正在重连...\r\n");
    }
}

/* ================================================================
 *  TCP配置持久化 (TF卡)
 *  文件路径: "0:/tcp_config.ini"
 *  格式: "ip=xxx.xxx.xxx.xxx\nport=xxxxx\n"
 * ================================================================ */

#define TCP_CONFIG_FILE  "0:/tcp_config.ini"

/**
 * @brief   将当前 server_ip 和 server_port 保存到TF卡
 * @return  0=成功, -1=失败
 */
int tcp_config_save(void)
{
    FRESULT res;
    UINT bytes_written;
    char buf[64];

    if(fs_mutex) osMutexAcquire(fs_mutex, osWaitForever);

    /* 创建/覆盖文件 */
    res = f_open(&SDFile, TCP_CONFIG_FILE, FA_WRITE | FA_CREATE_ALWAYS);
    if (res != FR_OK) {
        printf("tcp_config_save: 打开文件失败 (%d)\r\n", res);
        if(fs_mutex) osMutexRelease(fs_mutex);
        return -1;
    }

    /* 写入IP */
    snprintf(buf, sizeof(buf), "ip=%d.%d.%d.%d\n",
                server_ip[0], server_ip[1], server_ip[2], server_ip[3]);
    res = f_write(&SDFile, buf, strlen(buf), &bytes_written);

    /* 写入Port */
    snprintf(buf, sizeof(buf), "port=%d\n", server_port);
    res |= f_write(&SDFile, buf, strlen(buf), &bytes_written);

    f_close(&SDFile);
    if(fs_mutex) osMutexRelease(fs_mutex);

    if (res != FR_OK) {
        printf("tcp_config_save: 写入失败 (%d)\r\n", res);
        return -1;
    }

    printf("tcp_config_save: 保存成功 %d.%d.%d.%d:%d\r\n",
           server_ip[0], server_ip[1], server_ip[2], server_ip[3], server_port);
    return 0;
}

/**
 * @brief   从TF卡读取 server_ip 和 server_port
 * @return  0=成功, -1=失败(使用默认值)
 */
int tcp_config_load(void)
{
    FRESULT res;
    char line[64];
    int found_ip = 0, found_port = 0;

    if(fs_mutex) osMutexAcquire(fs_mutex, osWaitForever);

    /* 打开文件 */
    res = f_open(&SDFile, TCP_CONFIG_FILE, FA_READ);
    if (res != FR_OK) {
        printf("tcp_config_load: 文件不存在 (%d), 使用默认配置\r\n", res);
        if(fs_mutex) osMutexRelease(fs_mutex);
        return -1;
    }

    /* 逐行读取并解析 */
    while (f_gets(line, sizeof(line), &SDFile) != NULL)
    {
        if (strncmp(line, "ip=", 3) == 0)
        {
            /* 解析IP: ip=xxx.xxx.xxx.xxx */
            uint8_t ip[4] = {0};
            uint8_t octet = 0, val = 0, has_val = 0;
            for (int i = 3; i <= (int)strlen(line) && octet < 4; i++) {
                if (line[i] >= '0' && line[i] <= '9') {
                    val = val * 10 + (line[i] - '0');
                    has_val = 1;
                } else {
                    if (has_val) {
                        ip[octet++] = val;
                        val = 0;
                        has_val = 0;
                    }
                }
            }
            if (octet == 4) {
                memcpy(server_ip, ip, 4);
                found_ip = 1;
            }
        }
        else if (strncmp(line, "port=", 5) == 0)
        {
            /* 解析Port: port=xxxxx */
            uint16_t port = 0;
            for (int i = 5; i < (int)strlen(line); i++) {
                if (line[i] >= '0' && line[i] <= '9') {
                    port = port * 10 + (line[i] - '0');
                }
            }
            if (port > 0) {
                server_port = port;
                found_port = 1;
            }
        }
    }

    f_close(&SDFile);
    if(fs_mutex) osMutexRelease(fs_mutex);

    if (found_ip && found_port) {
        printf("tcp_config_load: 读取成功 %d.%d.%d.%d:%d\r\n",
               server_ip[0], server_ip[1], server_ip[2], server_ip[3], server_port);
        lv_snprintf(server_ip_buf, sizeof(server_ip_buf), "%d.%d.%d.%d ",server_ip[0], server_ip[1], server_ip[2], server_ip[3]);
        lv_snprintf(server_port_buf, sizeof(server_port_buf), "%d ",server_port);

        if(lv_obj_is_valid(guider_ui.screen_user_home_label_ip))
        {
            lv_label_set_text(guider_ui.screen_user_home_label_ip, server_ip_buf);
            lv_label_set_text(guider_ui.screen_user_home_label_port, server_port_buf);
        }
        return 0;
    } else {
        printf("tcp_config_load: 解析不完整 (ip=%d,port=%d), 使用默认配置\r\n",
               found_ip, found_port);
        return -1;
    }
}

void W5500_Task(void *argument)
{
    osDelay(8000);//等待es1642收到帧头错误
    /* 初始化SPI DMA所需的信号量和互斥锁 (必须在wizchip初始化前) */
    wiz_spi_dma_init();

    /* wizchip 初始化 */
    printf("W5500 TCP client\r\n");
    wizchip_initialize();

    /* 用芯片UID生成全网唯一MAC, 覆盖写死的默认MAC, 避免多台主机DHCP分配到相同IP */
    generate_mac_from_uid(default_net_info.mac);
    printf("MAC = %02X:%02X:%02X:%02X:%02X:%02X\r\n",
           default_net_info.mac[0], default_net_info.mac[1], default_net_info.mac[2],
           default_net_info.mac[3], default_net_info.mac[4], default_net_info.mac[5]);
    /* 设置网络信息 */
    network_init(ethernet_buf, &default_net_info);

    /* 仅使能Socket 0的SIMR掩码 */
    setSIMR(0x01 << TCP_SOCKET_ID);
    /* Socket中断掩码初始化为关闭, 连接成功后按需开启 */
    setSn_IMR(TCP_SOCKET_ID, 0x00);
    /* 清除可能残留的中断标志 */
    setIR(getIR());
    setSn_IR(TCP_SOCKET_ID, 0x1F);

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

        /* PHY链路状态检测: 网线拔掉时主动断开TCP连接
         * (解决网线拔掉期间服务器关闭导致连接半开、永远无法重连的问题) */
        if (wizphy_getphylink() == PHY_LINK_OFF)
        {
            if (g_tcp_connected)
            {
                disconnect(TCP_SOCKET_ID);
                g_tcp_connected = 0;
                g_device_manage_mode = 0;  /* 自动退出管理模式 */
                setSn_IMR(TCP_SOCKET_ID, 0x00);  /* 关闭Socket中断 */
                printf("网线已断开, 主动断开TCP连接\r\n");
            }
            continue;  /* 网线断开期间不处理其他事件, 等待网线恢复 */
        }

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
