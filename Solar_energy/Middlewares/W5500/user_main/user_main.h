#ifndef __USER_MAIN_H__
#define __USER_MAIN_H__

#include "stdint.h"

#define TCP_SOCKET_ID 0         /* TCP客户端 Socket */
#define ETHERNET_BUF_MAX_SIZE (128 * 2)
#define TCP_STREAM_BUF_SIZE   (256 * 2)  /* TCP流缓冲区大小 */

/* ==================== 帧协议配置 ====================
 *
 * 帧格式: [帧头][帧类型][命令字][数据长度][数据域][CRC16][帧尾]
 *   帧头:     0x0D (1字节)
 *   帧类型:   1字节 (0x00=请求, 0x01=响应, 0x02=错误)
 *   命令字:   1字节
 *   数据长度: 2字节 (小端序, 低字节在前)
 *   数据域:   N字节 (N = 数据长度)
 *   CRC16:   2字节 (小端序, CRC范围: 帧头+帧类型+命令字+数据长度+数据域)
 *   帧尾:     0x0E (1字节)
 */
#define FRAME_HEADER        0x0D         /* 帧头 */
#define FRAME_TAIL          0x0E         /* 帧尾 */
#define FRAME_TYPE_LEN      1            /* 帧类型长度 */
#define FRAME_CMD_LEN       1            /* 命令字长度 */
#define FRAME_LENGTH_LEN    2            /* 数据域长度字段字节数 */
#define FRAME_CRC_LEN       2            /* CRC16校验长度 */
#define FRAME_MAX_DATA_LEN  512          /* 数据域最大长度 */

/* ==================== 帧解析状态机 ==================== */
typedef enum {
    FRAME_STATE_WAIT_HEADER = 0,   /* 等待帧头 0x0D */
    FRAME_STATE_WAIT_TYPE,         /* 等待帧类型 */
    FRAME_STATE_WAIT_CMD,          /* 等待命令字 */
    FRAME_STATE_WAIT_LENGTH,       /* 等待数据长度 (2字节) */
    FRAME_STATE_WAIT_DATA,         /* 等待数据域 */
    FRAME_STATE_WAIT_CRC,          /* 等待CRC16校验 (2字节) */
    FRAME_STATE_WAIT_TAIL,         /* 等待帧尾 0x0E */
} FrameState_t;

typedef struct {
    FrameState_t state;
    uint8_t  frame_type;            /* 帧类型 */
    uint8_t  frame_cmd;             /* 命令字 */
    uint8_t  length_idx;            /* 长度字段接收索引 */
    uint16_t data_len;              /* 解析出的数据长度 */
    uint16_t data_index;            /* 数据域当前接收索引 */
    uint8_t  crc_idx;               /* CRC字段接收索引 */
    uint8_t  length_buf[FRAME_LENGTH_LEN];
    uint8_t  crc_buf[FRAME_CRC_LEN];
    uint8_t  data_buf[FRAME_MAX_DATA_LEN];
} FrameParser_t;

/* ==================== TCP服务器配置 ==================== */

/**
 * @brief   获取当前服务器IP和端口
 * @param   ip:   输出4字节IP地址
 * @param   port: 输出端口号
 */
void tcp_get_server_addr(uint8_t ip[4], uint16_t *port);

/**
 * @brief   设置新的服务器IP和端口, 并触发重连
 * @param   ip:   4字节IP地址
 * @param   port: 端口号
 */
void tcp_set_server_addr(const uint8_t ip[4], uint16_t port);

/**
 * @brief   将当前 server_ip 和 server_port 保存到TF卡
 * @return  0=成功, -1=失败
 */
int tcp_config_save(void);

/**
 * @brief   从TF卡读取 server_ip 和 server_port
 * @return  0=成功, -1=失败(使用默认值)
 */
int tcp_config_load(void);

/* ==================== 函数声明 ==================== */

/**
 * @brief   W5500任务函数
 */
void W5500_Task(void *argument);

/**
 * @brief   初始化帧解析器
 */
void frame_parser_init(FrameParser_t *parser);

/**
 * @brief   重置帧解析器
 */
void frame_parser_reset(FrameParser_t *parser);

/**
 * @brief   向帧解析器喂入数据
 * @param   parser: 解析器指针
 * @param   data:  数据指针
 * @param   len:   数据长度
 */
void frame_parser_feed(FrameParser_t *parser, const uint8_t *data, uint16_t len);

/**
 * @brief   帧接收完成回调 (用户在此函数中实现命令分发)
 * @param   type: 帧类型
 * @param   cmd:  命令字
 * @param   data: 数据域指针
 * @param   len:  数据域长度
 */
void tcp_frame_handler(uint8_t type, uint8_t cmd, const uint8_t *data, uint16_t len);

/**
 * @brief   按帧协议格式组装并发送数据到TCP客户端
 * @param   type: 帧类型
 * @param   cmd:  命令字
 * @param   data: 数据域指针
 * @param   len:  数据域长度
 */
void tcp_send_frame(uint8_t type, uint8_t cmd, const uint8_t *data, uint16_t len);

/**
 * @brief   CRC16-CCITT反射版本计算 (与上位机Qt版本一致)
 *          多项式0x1021, 初始值0x0000, 输入输出反射
 * @param   data: 数据指针
 * @param   len:  数据长度
 * @return  CRC16值
 */
uint16_t crc16_ccitt(const uint8_t *data, uint16_t len);

#endif
