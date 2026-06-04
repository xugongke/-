#ifndef __USER_MAIN_H__
#define __USER_MAIN_H__

#include "stdint.h"

#define SOCKET_ID 0
#define ETHERNET_BUF_MAX_SIZE (1024 * 2)
#define TCP_STREAM_BUF_SIZE   (1024 * 4)  /* TCP流缓冲区大小 */

/* ==================== 帧协议配置 (根据实际协议修改) ==================== */
#define FRAME_HEADER_0       0xAA         /* 帧头第1字节 */
#define FRAME_HEADER_1       0x55         /* 帧头第2字节 */
#define FRAME_HEADER_LEN     2            /* 帧头长度 */
#define FRAME_LENGTH_LEN     2            /* 数据域长度字段字节数(大端序) */
#define FRAME_TAIL_0         0x0D         /* 帧尾第1字节 */
#define FRAME_TAIL_1         0x0A         /* 帧尾第2字节 */
#define FRAME_TAIL_LEN       2            /* 帧尾长度 */
#define FRAME_MAX_DATA_LEN   512          /* 数据域最大长度 */

/* ==================== loopback_tcps_interrupt 事件标志 ==================== */
#define TCPS_EVT_CONNECTED     0x01
#define TCPS_EVT_DISCONNECTED  0x02
#define TCPS_EVT_DATA_RECV     0x04

/* ==================== 帧解析状态机 ==================== */
typedef enum {
    FRAME_STATE_WAIT_HEADER = 0,   /* 等待帧头 */
    FRAME_STATE_WAIT_LENGTH,       /* 等待数据长度 */
    FRAME_STATE_WAIT_DATA,         /* 等待数据域 */
    FRAME_STATE_WAIT_TAIL,         /* 等待帧尾 */
} FrameState_t;

typedef struct {
    FrameState_t state;
    uint8_t  header_idx;           /* 帧头接收索引 */
    uint8_t  length_idx;           /* 长度字段接收索引 */
    uint16_t data_len;             /* 解析出的数据长度 */
    uint16_t data_index;           /* 数据域当前接收索引 */
    uint8_t  tail_idx;             /* 帧尾接收索引 */
    uint8_t  length_buf[FRAME_LENGTH_LEN];
    uint8_t  data_buf[FRAME_MAX_DATA_LEN];
} FrameParser_t;

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
 * @param   data: 数据域指针
 * @param   len:  数据域长度
 * @note    请根据实际协议在此函数中解析数据域并执行对应命令
 */
void tcp_frame_handler(const uint8_t *data, uint16_t len);

/**
 * @brief   按帧协议格式发送数据到TCP客户端
 * @param   data: 数据域指针
 * @param   len:  数据域长度
 */
void tcp_send_frame(const uint8_t *data, uint16_t len);

#endif
