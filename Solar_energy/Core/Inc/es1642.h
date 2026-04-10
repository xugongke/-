
/**
 * @file    es1642.h
 * @brief   ES1642 模块底层通信驱动头文件
 * @author  OpenAI
 * @date    2026-03-13
 *
 * 说明：
 * 1. 本驱动按《ES1642-Ⅳ模块接口协议》实现，适用于 STM32F429IGT6 等 MCU。
 * 2. 本文件只负责“模块协议层”的帧封装、帧解析、校验、常用命令打包与解包。
 * 3. UART 的具体发送方式（阻塞、DMA、中断）由用户通过回调函数接入。
 * 4. 0x14/0x15 的 User Data 为透传用户数据，具体的主从业务协议需由上层自行定义。
 */

#ifndef ES1642_H
#define ES1642_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* ========================= 基本常量 ========================= */

#define ES1642_UART_BAUDRATE         9600U
#define ES1642_FRAME_HEAD            0x79U
#define ES1642_ADDR_LEN              6U
#define ES1642_LOCAL_NET_KEY_LEN     4U   /* 读取网络参数(0DH)返回的网络口令长度 */
#define ES1642_SET_PSK_LEN           8U   /* 设置网络口令(1CH)时，协议建议的新口令长度 */

/* 协议中 Data 字段最大长度。
 * 若你的应用层 User Data 较大，可在编译前重定义本宏。 */
#ifndef ES1642_MAX_DATA_LEN
#define ES1642_MAX_DATA_LEN          512U
#endif

#define ES1642_MIN_FRAME_LEN         7U
#define ES1642_MAX_FRAME_LEN         (ES1642_MIN_FRAME_LEN + ES1642_MAX_DATA_LEN)//一帧数据最大长度

/* 0x14 发送数据时，Data 固定字段长度 = DataCtrl(2) + DstAddr(6) + UserLen(2) */
#define ES1642_SEND_DATA_FIXED_LEN   10U

/* 0x15 接收数据时，Data 固定字段长度 = DataCtrl(3) + SrcAddr(6) + UserLen(2) */
#define ES1642_RECV_DATA_FIXED_LEN   11U

/* ========================= Ctrl 控制位 ========================= */

#define ES1642_CTRL_BIT_PRM          0x40U
#define ES1642_CTRL_BIT_RESPOND      0x20U

/* 说明：
 * 协议正文只显式定义了 Ctrl 的 Prm / Respond 位，其他位标为 RSV。
 * 但附录 A 的示例报文实际使用了 0x58（设备下发）、0xB8（模块异常应答）的控制字节。
 * 为了尽量兼容实物模块，本驱动默认采用与示例一致的底值。
 * 若你现场测试发现模块允许使用其他 RSV 组合，可在编译前自行重定义下面这些宏。 */
#ifndef ES1642_CTRL_DEVICE_REQUEST
#define ES1642_CTRL_DEVICE_REQUEST       0x58U
#endif

#ifndef ES1642_CTRL_DEVICE_REPLY
#define ES1642_CTRL_DEVICE_REPLY         0x18U
#endif

#ifndef ES1642_CTRL_MODULE_NORMAL
#define ES1642_CTRL_MODULE_NORMAL        0x98U
#endif

#ifndef ES1642_CTRL_MODULE_EXCEPTION
#define ES1642_CTRL_MODULE_EXCEPTION     0xB8U
#endif

/* ========================= 指令字 ========================= */

typedef enum
{
		ES1642_CMD_REBOOT                 = 0x01,//模块重启,						单片机下发到模块
    ES1642_CMD_READ_VERSION           = 0x02,//读取 模块 版本信息,	单片机下发到模块
    ES1642_CMD_READ_MAC               = 0x03,//读取模块 MAC 地址,		单片机下发到模块
    ES1642_CMD_READ_ADDR              = 0x0B,//读取模块通信地址,		单片机下发到模块
    ES1642_CMD_SET_ADDR               = 0x0C,//设置模块通信地址,		单片机下发到模块
    ES1642_CMD_READ_NET_PARAM         = 0x0D,//读取 模块 网络参数,	单片机下发到模块
    ES1642_CMD_SET_NET_PARAM          = 0x0E,//设置 模块 网络参数,	单片机下发到模块

    ES1642_CMD_SEND_DATA              = 0x14,//发送数据,						单片机下发到模块
		ES1642_CMD_RECV_DATA              = 0x15,//接收数据,						模块回复给单片机
    ES1642_CMD_START_SEARCH           = 0x17,//发起设备搜索,				单片机下发到模块
    ES1642_CMD_STOP_SEARCH            = 0x18,//停止设备搜索,				单片机下发到模块
    ES1642_CMD_REPORT_SEARCH_RESULT   = 0x19,//上报搜索 结果,				模块回复给单片机
    ES1642_CMD_NOTIFY_SEARCH          = 0x1A,//通知 设备 搜索,			模块回复给单片机
    ES1642_CMD_REPLY_SEARCH           = 0x1B,//响应 设备 搜索,			单片机下发到模块
    ES1642_CMD_SET_PSK                = 0x1C,//设置网络口令,				单片机下发到模块
    ES1642_CMD_NOTIFY_PSK             = 0x1D,//通知网络口令设置,		模块回复给单片机
    ES1642_CMD_REPORT_PSK_RESULT      = 0x1F,//上报网络口令设置结果,模块回复给单片机

    ES1642_CMD_REMOTE_READ_VERSION    = 0x52,
    ES1642_CMD_REMOTE_READ_MAC        = 0x53,
    ES1642_CMD_REMOTE_READ_NET_PARAM  = 0x5D
} es1642_cmd_t;

/* ========================= 协议状态码 ========================= */

typedef enum
{
    ES1642_STATUS_OK = 0,                   /* 操作成功 */
    ES1642_STATUS_IN_PROGRESS = 1,          /* 流式接收中，帧尚未完整 */
    ES1642_STATUS_FRAME_READY = 2,          /* 流式接收完成一帧 */

    ES1642_STATUS_ERROR_PARAM = 0x100,      /* 参数错误 */
    ES1642_STATUS_ERROR_NO_TX_PORT,         /* 未注册发送回调 */
    ES1642_STATUS_ERROR_BUFFER_TOO_SMALL,   /* 输出缓存不够 */
    ES1642_STATUS_ERROR_BAD_HEAD,           /* 帧头错误 */
    ES1642_STATUS_ERROR_BAD_LENGTH,         /* 帧长度错误 */
    ES1642_STATUS_ERROR_DATA_TOO_LONG,      /* Data 字段超过本驱动配置上限 */
    ES1642_STATUS_ERROR_CHECKSUM,           /* CSUM/CXOR 校验失败 */
    ES1642_STATUS_ERROR_CMD_MISMATCH,       /* 指令字不匹配 */
    ES1642_STATUS_ERROR_FRAME_IS_EXCEPTION, /* 当前帧是异常应答帧 */
    ES1642_STATUS_ERROR_NOT_EXCEPTION_FRAME,/* 当前帧不是异常应答帧 */
    ES1642_STATUS_ERROR_PAYLOAD_LENGTH,     /* Data 字段长度与协议不符 */
    ES1642_STATUS_ERROR_TX_FAIL             /* 串口发送失败 */
} es1642_status_t;

/* ========================= 模块异常状态码 ========================= */

typedef enum
{
    ES1642_EXCEPTION_BAD_FORMAT    = 0x00, /* 错误的格式 */
    ES1642_EXCEPTION_BAD_DATA_UNIT = 0x01, /* 错误的数据单元 */
    ES1642_EXCEPTION_BAD_LENGTH    = 0x02, /* 错误的长度 */
    ES1642_EXCEPTION_INVALID_CMD   = 0x03, /* 指令字无效 */
    ES1642_EXCEPTION_NO_RAM        = 0x04, /* RAM 空间不足 */
    ES1642_EXCEPTION_BAD_STATE     = 0x05  /* 错误的状态 */
} es1642_exception_t;

/* ========================= 搜索相关枚举 ========================= */

typedef enum
{
    ES1642_SEARCH_RULE_ALL              = 0x00, /* 搜索所有设备 */
    ES1642_SEARCH_RULE_SAME_NETWORK     = 0x01, /* 搜索与本设备网络口令相同的设备 */
    ES1642_SEARCH_RULE_NOT_NETWORKED    = 0x02, /* 搜索未入网设备 */
    ES1642_SEARCH_RULE_OTHER_NETWORK    = 0x03  /* 搜索与本设备网络口令不同的设备 */
} es1642_search_rule_t;

typedef enum
{
    ES1642_NET_STATE_OFFLINE        = 0x00, /* 不在网 */
    ES1642_NET_STATE_SAME_NETWORK   = 0x01, /* 同属于自己网络 */
    ES1642_NET_STATE_OTHER_NETWORK  = 0x02, /* 属于其他网络 */
    ES1642_NET_STATE_UNKNOWN        = 0x03  /* 未知 */
} es1642_net_state_t;

typedef enum
{
    ES1642_SEARCH_REPLY_REJECT  = 0x00,     /* 不参与搜索 */
    ES1642_SEARCH_REPLY_ACCEPT  = 0x01      /* 参与搜索 */
} es1642_search_reply_state_t;

typedef enum
{
    ES1642_PSK_OP_CLEAR = 0x01,
    ES1642_PSK_OP_SET   = 0x02
} es1642_psk_op_t;

/* ========================= 基本结构体 ========================= */

typedef struct
{
    uint16_t vendor_id;     /* 厂商标识，协议中 ES = 0x4553 */
    uint16_t chip_type;     /* 芯片类型，协议中 1642 = 0x1642 */
    uint8_t  product_info;  /* 产品信息，固定 0x04 */
    uint16_t version_bcd;   /* 版本号，BCD 格式 */
} es1642_version_t;

typedef struct
{
    uint8_t relay_depth;                            /* 中继深度 */
    uint8_t network_key[ES1642_LOCAL_NET_KEY_LEN]; /* 读取到的网络口令(4字节) */
    uint8_t rsv1;
    uint8_t rsv2;
} es1642_net_param_t;

typedef struct
{
    uint8_t  header;           /* 帧头，固定 0x79 */
    uint16_t data_len;         /* Data 字段长度 */
    uint8_t  ctrl;             /* Ctrl 字段原始值 */
    uint8_t  cmd;              /* 指令字 */
    const uint8_t *data;       /* 指向原始 Data 数据区 */
    uint8_t  csum;             /* 原始 CSUM */
    uint8_t  cxor;             /* 原始 CXOR */

    bool     prm;              /* D6 位，仅对 0x14 有意义 */
    bool     is_exception;     /* D5 位，为 1 表示异常应答 */
    uint8_t  exception_code;   /* 若为异常帧，默认取 data 最后 1 字节 */
} es1642_frame_t;

typedef struct
{
    uint32_t raw_data_ctrl;                     /* 24 位有效 */
    uint8_t  relay_depth;                       /* 从 DataCtrl 中解析出的中继深度 */
    int16_t  rssi;                              /* 按协议位域解析出的 RSSI(接收信号强度指示，有符号整形数值) */
    uint8_t  src_addr[ES1642_ADDR_LEN];
    uint16_t user_data_len;
    const uint8_t *user_data;                  /* 指向原始帧缓存中的用户数据 */
} es1642_recv_data_t;

typedef struct
{
		uint8_t  count;
    uint8_t  dev_addr[ES1642_ADDR_LEN];
    uint16_t raw_dev_ctrl;
    uint8_t  net_state;
    uint8_t  attribute_len;
    const uint8_t *attribute;                  /* 指向原始帧缓存中的属性数据 */
} es1642_search_result_t;

typedef struct
{
    uint16_t raw_data_ctrl;
    uint8_t  src_addr[ES1642_ADDR_LEN];
    uint8_t  task_id;
    uint8_t  attribute_len;
    const uint8_t *attribute;                  /* 指向原始帧缓存中的属性数据 */
} es1642_search_notify_t;

typedef struct
{
    uint16_t raw_data_ctrl;
    uint8_t  src_addr[ES1642_ADDR_LEN];
    uint8_t  op;                               /* 01=清除，02=设置 */
} es1642_psk_notify_t;

typedef struct
{
    uint16_t raw_data_ctrl;
    uint8_t  src_addr[ES1642_ADDR_LEN];
    uint8_t  state;                            /* 00=失败，01=成功 */
} es1642_psk_result_t;

typedef struct
{
    uint8_t src_addr[ES1642_ADDR_LEN];
    es1642_version_t version;
} es1642_remote_version_t;

typedef struct
{
    uint8_t src_addr[ES1642_ADDR_LEN];
    uint8_t mac[ES1642_ADDR_LEN];
} es1642_remote_mac_t;

typedef struct
{
    uint8_t src_addr[ES1642_ADDR_LEN];
    es1642_net_param_t net_param;
} es1642_remote_net_param_t;

struct es1642_handle;
typedef struct es1642_handle es1642_handle_t;

/* 串口发送回调函数
 * 返回值：
 * >=0  : 实际发送字节数
 *  <0  : 发送失败 */
typedef int32_t (*es1642_write_fn_t)(const uint8_t *data, uint16_t len, void *user_arg);

/* 完整帧到达后的回调函数
 * 注意：frame->data / 解码结构体中的 user_data / attribute 指针都指向驱动内部 RX 缓冲。
 * 如果上层要长期保存，必须自行拷贝。 */
typedef void (*es1642_frame_cb_t)(es1642_handle_t *handle,
                                  const es1642_frame_t *frame,
                                  void *user_arg);

/* 解析或接收异常时的回调 */
typedef void (*es1642_error_cb_t)(es1642_handle_t *handle,
                                  es1642_status_t status,
                                  void *user_arg);

typedef struct
{
    es1642_write_fn_t write;
    es1642_frame_cb_t on_frame;
    es1642_error_cb_t on_error;
    void *user_arg;
} es1642_port_t;

struct es1642_handle
{
    es1642_port_t port;
};

/* ========================= 通用工具函数 ========================= */
uint8_t ES1642_MakeDeviceRequestCtrl(void);
uint8_t ES1642_MakeDeviceReplyCtrl(void);
uint8_t ES1642_MakeDeviceExceptionCtrl(void);
uint8_t ES1642_MakeSendDataCtrlByte(bool prm);

uint16_t ES1642_MakeTxDataCtrl(uint8_t relay_depth);
uint16_t ES1642_MakeSearchCtrl(uint8_t depth, es1642_search_rule_t rule);
uint16_t ES1642_MakeSearchReplyCtrl(bool participate);

bool ES1642_IsBroadcastAddr(const uint8_t addr[ES1642_ADDR_LEN]);
void ES1642_SetBroadcastAddr(uint8_t addr[ES1642_ADDR_LEN]);
void ES1642_CopyAddr(uint8_t dst[ES1642_ADDR_LEN], const uint8_t src[ES1642_ADDR_LEN]);

const char *ES1642_StatusString(es1642_status_t status);
const char *ES1642_ExceptionString(uint8_t exception_code);

/* ========================= 帧封装 / 解析 ========================= */

es1642_status_t ES1642_BuildFrame(uint8_t ctrl,
                                  uint8_t cmd,
                                  const uint8_t *data,
                                  uint16_t data_len,
                                  uint8_t *out_frame,
                                  uint16_t out_size,
                                  uint16_t *out_frame_len);

es1642_status_t ES1642_SendFrame(es1642_handle_t *handle,
                                 uint8_t ctrl,
                                 uint8_t cmd,
                                 const uint8_t *data,
                                 uint16_t data_len);

es1642_status_t ES1642_ParseFrame(const uint8_t *raw_frame,
                                  uint16_t frame_len,
                                  es1642_frame_t *frame);

es1642_status_t ES1642_ProcessCompleteFrame(es1642_handle_t *handle,
                                            const uint8_t *frame_buf,
                                            uint16_t frame_len);															 

/* ========================= 常用命令发送接口 ========================= */

es1642_status_t ES1642_SendReboot(es1642_handle_t *handle);
es1642_status_t ES1642_SendReadVersion(es1642_handle_t *handle);
es1642_status_t ES1642_SendReadMac(es1642_handle_t *handle);
es1642_status_t ES1642_SendReadAddr(es1642_handle_t *handle);
es1642_status_t ES1642_SendSetAddr(es1642_handle_t *handle,
                                   const uint8_t addr[ES1642_ADDR_LEN]);
es1642_status_t ES1642_SendReadNetParam(es1642_handle_t *handle);
es1642_status_t ES1642_SendSetNetParam(es1642_handle_t *handle,
                                       uint8_t relay_depth);

es1642_status_t ES1642_SendData(es1642_handle_t *handle,
                                const uint8_t dst_addr[ES1642_ADDR_LEN],
                                const uint8_t *user_data,
                                uint16_t user_data_len,
                                uint8_t relay_depth,
                                bool prm);

es1642_status_t ES1642_SendStartSearch(es1642_handle_t *handle,
                                       uint8_t depth,
                                       es1642_search_rule_t rule,
                                       const uint8_t *attribute,
                                       uint8_t attribute_len);

es1642_status_t ES1642_SendStopSearch(es1642_handle_t *handle);

es1642_status_t ES1642_SendSearchReply(es1642_handle_t *handle,
                                       const uint8_t src_addr[ES1642_ADDR_LEN],
                                       uint8_t task_id,
                                       bool participate,
                                       const uint8_t *attribute,
                                       uint8_t attribute_len);

es1642_status_t ES1642_SendSetPsk(es1642_handle_t *handle,
                                  const uint8_t dst_addr[ES1642_ADDR_LEN],
                                  const uint8_t *new_psk,
                                  uint8_t new_psk_len);

/* 设备对模块通知的普通应答，Data 为空。
 * 协议说明“不是必须”，但如果你的现场业务希望显式应答，可使用此接口。 */
es1642_status_t ES1642_SendAckEmpty(es1642_handle_t *handle, uint8_t cmd);

/* 设备主动回复异常应答。一般不常用，但为了协议完整性一并给出。 */
es1642_status_t ES1642_SendException(es1642_handle_t *handle,
                                     uint8_t cmd,
                                     uint8_t exception_code);

es1642_status_t ES1642_SendRemoteReadVersion(es1642_handle_t *handle,
                                             const uint8_t dst_addr[ES1642_ADDR_LEN]);
es1642_status_t ES1642_SendRemoteReadMac(es1642_handle_t *handle,
                                         const uint8_t dst_addr[ES1642_ADDR_LEN]);
es1642_status_t ES1642_SendRemoteReadNetParam(es1642_handle_t *handle,
                                              const uint8_t dst_addr[ES1642_ADDR_LEN]);

/* ========================= 常用命令解码接口 ========================= */

es1642_status_t ES1642_DecodeEmptyResponse(const es1642_frame_t *frame, uint8_t expect_cmd);
es1642_status_t ES1642_DecodeRebootResponse(const es1642_frame_t *frame,
                                            uint8_t *state,
                                            uint8_t *rsv);
es1642_status_t ES1642_DecodeVersion(const es1642_frame_t *frame,
                                     es1642_version_t *version);
es1642_status_t ES1642_DecodeMac(const es1642_frame_t *frame,
                                 uint8_t mac[ES1642_ADDR_LEN]);
es1642_status_t ES1642_DecodeAddr(const es1642_frame_t *frame,
                                  uint8_t addr[ES1642_ADDR_LEN]);
es1642_status_t ES1642_DecodeNetParam(const es1642_frame_t *frame,
                                      es1642_net_param_t *net_param);
es1642_status_t ES1642_DecodeRecvData(const es1642_frame_t *frame,
                                      es1642_recv_data_t *recv_data);
es1642_status_t ES1642_DecodeSearchResult(const es1642_frame_t *frame,
                                          es1642_search_result_t *result);
es1642_status_t ES1642_DecodeSearchNotify(const es1642_frame_t *frame,
                                          es1642_search_notify_t *notify);
es1642_status_t ES1642_DecodePskNotify(const es1642_frame_t *frame,
                                       es1642_psk_notify_t *notify);
es1642_status_t ES1642_DecodePskResult(const es1642_frame_t *frame,
                                       es1642_psk_result_t *result);

es1642_status_t ES1642_DecodeRemoteVersion(const es1642_frame_t *frame,
                                           es1642_remote_version_t *version);
es1642_status_t ES1642_DecodeRemoteMac(const es1642_frame_t *frame,
                                       es1642_remote_mac_t *mac);
es1642_status_t ES1642_DecodeRemoteNetParam(const es1642_frame_t *frame,
                                            es1642_remote_net_param_t *net_param);

/* 本地异常应答：Data = Status(1byte) */
es1642_status_t ES1642_DecodeLocalException(const es1642_frame_t *frame,
                                            uint8_t *exception_code);

/* 远程调试异常应答：
 * 协议附录 B 说明“远程应答增加源地址”，因此这里按 Data = SrcAddr(6) + Status(1) 解析。
 * 如果你实测模块返回的仍是单字节异常状态，请改用 ES1642_DecodeLocalException()。 */
es1642_status_t ES1642_DecodeRemoteException(const es1642_frame_t *frame,
                                             uint8_t src_addr[ES1642_ADDR_LEN],
                                             uint8_t *exception_code);

#ifdef __cplusplus
}
#endif

#endif /* ES1642_H */
