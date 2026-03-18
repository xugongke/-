#ifndef __ES1642_PROTOCOL_H__
#define __ES1642_PROTOCOL_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>

/*
 * ES1642-Ⅳ 模块串口协议底层驱动
 *
 * 说明：
 * 1. 本文件只处理“模块串口协议”本身，不处理你的应用层业务协议。
 * 2. 串口底层发送由用户通过回调函数提供，适配 STM32 HAL / LL / 裸机均可。
 * 3. 接收建议在串口中断、DMA 空闲中断或任务轮询中，把收到的字节喂给 ES1642_InputByte()/ES1642_InputBuffer()。
 * 4. 本库默认按协议文档中的帧格式生成报文：79H + L(2字节小端) + Ctrl + Cmd + Data + CSUM + CXOR。
 * 5. 默认控制字采用协议示例中常见的设备侧取值：
 *      - 普通设备请求/正常应答：0x58
 *      - 发送数据(14H)“主动发起”：0x58
 *      - 发送数据(14H)“对其他设备指令的回应”：0x18
 *    如果现场模块对保留位有特殊要求，可直接调用 ES1642_SendFrame() 自定义 Ctrl。
 */

#ifndef ES1642_MAX_DATA_LEN
#define ES1642_MAX_DATA_LEN         512u
#endif

#define ES1642_FRAME_HEAD           0x79u
#define ES1642_ADDR_LEN             6u
#define ES1642_PSK_SET_LEN          8u
#define ES1642_FRAME_FIXED_LEN      7u      /* 帧头1 + 长度2 + Ctrl1 + Cmd1 + CSUM1 + CXOR1 */
#define ES1642_FRAME_MAX_LEN        (ES1642_FRAME_FIXED_LEN + ES1642_MAX_DATA_LEN)

/* 默认控制字（设备侧常用取值） */
#define ES1642_CTRL_DEFAULT_REQ     0x58u
#define ES1642_CTRL_DEFAULT_ACK     0x58u
#define ES1642_CTRL_DATA_ACTIVE     0x58u   /* 14H 主动发起 */
#define ES1642_CTRL_DATA_RESPONSE   0x18u   /* 14H 指令回应 */

/* 控制字位定义 */
#define ES1642_CTRL_PRM_MASK        0x40u
#define ES1642_CTRL_RESPOND_MASK    0x20u

/* 常用宏 */
#define ES1642_CTRL_IS_EXCEPTION(ctrl)   (((ctrl) & ES1642_CTRL_RESPOND_MASK) ? 1u : 0u)
#define ES1642_CTRL_GET_PRM(ctrl)        (((ctrl) & ES1642_CTRL_PRM_MASK) ? 1u : 0u)

/* 返回值定义 */
typedef enum
{
    ES1642_RES_OK                = 0,
    ES1642_RES_FRAME_OK          = 1,
    ES1642_RES_IN_PROGRESS       = 2,

    ES1642_RES_ERR_PARAM         = -1,
    ES1642_RES_ERR_BUF_SMALL     = -2,
    ES1642_RES_ERR_DATA_TOO_LONG = -3,
    ES1642_RES_ERR_SEND          = -4,
    ES1642_RES_ERR_CHECKSUM      = -5,
    ES1642_RES_ERR_XOR           = -6,
    ES1642_RES_ERR_FRAME         = -7,
    ES1642_RES_ERR_CMD           = -8,
    ES1642_RES_ERR_EXCEPTION     = -9,
    ES1642_RES_ERR_LENGTH        = -10
} ES1642_Result;

/* 指令字 */
typedef enum
{
    ES1642_CMD_REBOOT                = 0x01,
    ES1642_CMD_READ_VERSION          = 0x02,
    ES1642_CMD_READ_MAC              = 0x03,
    ES1642_CMD_READ_COMM_ADDR        = 0x0B,
    ES1642_CMD_SET_COMM_ADDR         = 0x0C,
    ES1642_CMD_READ_NET_PARAM        = 0x0D,
    ES1642_CMD_SET_NET_PARAM         = 0x0E,

    ES1642_CMD_SEND_DATA             = 0x14,
    ES1642_CMD_RECV_DATA             = 0x15,
    ES1642_CMD_START_SEARCH          = 0x17,
    ES1642_CMD_STOP_SEARCH           = 0x18,
    ES1642_CMD_REPORT_SEARCH_RESULT  = 0x19,
    ES1642_CMD_NOTIFY_SEARCH         = 0x1A,
    ES1642_CMD_RESPOND_SEARCH        = 0x1B,
    ES1642_CMD_SET_NETWORK_PSK       = 0x1C,
    ES1642_CMD_NOTIFY_SET_PSK        = 0x1D,
    ES1642_CMD_REPORT_SET_PSK_RESULT = 0x1F,

    ES1642_CMD_REMOTE_READ_VERSION   = 0x52,
    ES1642_CMD_REMOTE_READ_MAC       = 0x53,
    ES1642_CMD_REMOTE_READ_NET_PARAM = 0x5D
} ES1642_Cmd;

/* 异常状态码（附录 A） */
typedef enum
{
    ES1642_EXC_BAD_FORMAT    = 0x00,
    ES1642_EXC_BAD_DATA_UNIT = 0x01,
    ES1642_EXC_BAD_LENGTH    = 0x02,
    ES1642_EXC_BAD_CMD       = 0x03,
    ES1642_EXC_NO_RAM        = 0x04,
    ES1642_EXC_BAD_STATE     = 0x05
} ES1642_ExceptionCode;

/* 搜索规则（17H） */
typedef enum
{
    ES1642_SEARCH_ALL             = 0x00,
    ES1642_SEARCH_SAME_PSK        = 0x01,
    ES1642_SEARCH_NOT_IN_NETWORK  = 0x02,
    ES1642_SEARCH_OTHER_NETWORK   = 0x03
} ES1642_SearchRule;

/* 搜索结果网络状态（19H） */
typedef enum
{
    ES1642_NET_STATE_OFFLINE       = 0x00,
    ES1642_NET_STATE_SAME_NETWORK  = 0x01,
    ES1642_NET_STATE_OTHER_NETWORK = 0x02,
    ES1642_NET_STATE_UNKNOWN       = 0x03
} ES1642_NetState;

/* 网络口令操作类型（1DH） */
typedef enum
{
    ES1642_PSK_OP_CLEAR = 0x01,
    ES1642_PSK_OP_SET   = 0x02
} ES1642_PskOp;

/* 一帧完整报文 */
typedef struct
{
    uint8_t  head;
    uint16_t len;
    uint8_t  ctrl;
    uint8_t  cmd;
    uint8_t  data[ES1642_MAX_DATA_LEN];
    uint8_t  csum;
    uint8_t  cxor;
} ES1642_Frame;

/* 版本信息 */
typedef struct
{
    uint16_t vendor_id;      /* 文档示例：'ES' -> 0x4553 */
    uint16_t chip_type;      /* 文档示例：1642 -> 0x1642 */
    uint8_t  product_info;   /* 固定 0x04 */
    uint16_t version_bcd;    /* 2 字节 BCD 版本号 */
} ES1642_VersionInfo;

/* 网络参数 */
typedef struct
{
    uint8_t relay_depth;
    uint8_t network_psk[4];  /* 协议中读取网络参数返回 4 字节 */
    uint8_t rsv0;
    uint8_t rsv1;
} ES1642_NetParamInfo;

/* 15H 接收数据通知解析结果 */
typedef struct
{
    uint32_t      data_ctrl_raw;                     /* 24bit 原始控制域，低 24 位有效 */
    uint8_t       relay_depth;
    int16_t       rssi;
    uint8_t       src_addr[ES1642_ADDR_LEN];
    uint16_t      user_data_len;
    const uint8_t *user_data;
} ES1642_RxDataInd;

/* 19H 搜索结果上报解析结果 */
typedef struct
{
    uint8_t       dev_addr[ES1642_ADDR_LEN];
    uint16_t      dev_ctrl_raw;
    uint8_t       net_state;
    uint8_t       attribute_len;
    const uint8_t *attribute;
} ES1642_SearchResultInd;

/* 1AH 搜索通知解析结果 */
typedef struct
{
    uint16_t      data_ctrl_raw;
    uint8_t       src_addr[ES1642_ADDR_LEN];
    uint8_t       task_id;
    uint8_t       attribute_len;
    const uint8_t *attribute;
} ES1642_SearchNotify;

/* 1DH 网络口令变化通知解析结果 */
typedef struct
{
    uint16_t data_ctrl_raw;
    uint8_t  src_addr[ES1642_ADDR_LEN];
    uint8_t  op;
} ES1642_PskNotify;

/* 1FH 网络口令设置结果解析结果 */
typedef struct
{
    uint16_t data_ctrl_raw;
    uint8_t  src_addr[ES1642_ADDR_LEN];
    uint8_t  state;
} ES1642_PskResultInd;

/* 解析器状态 */
typedef enum
{
    ES1642_RX_WAIT_HEAD = 0,
    ES1642_RX_WAIT_LEN_L,
    ES1642_RX_WAIT_LEN_H,
    ES1642_RX_WAIT_CTRL,
    ES1642_RX_WAIT_CMD,
    ES1642_RX_WAIT_DATA,
    ES1642_RX_WAIT_CSUM,
    ES1642_RX_WAIT_CXOR
} ES1642_RxState;

/* 接收解析器 */
typedef struct
{
    ES1642_RxState state;
    uint16_t data_index;
    uint8_t  sum;
    uint8_t  xorv;
    ES1642_Frame frame;
} ES1642_RxParser;

struct ES1642_Handle;

/* 串口底层发送回调
 * 返回值约定：
 *   >=0 : 发送成功
 *   < 0 : 发送失败
 */
typedef int32_t (*ES1642_SendFunc)(const uint8_t *buf, uint16_t len, void *user_arg);

/* 收到完整有效帧后的回调 */
typedef void (*ES1642_FrameCallback)(struct ES1642_Handle *handle, const ES1642_Frame *frame);

/* 解析错误回调 */
typedef void (*ES1642_ErrorCallback)(struct ES1642_Handle *handle, int32_t err_code);

/* 驱动句柄 */
typedef struct ES1642_Handle
{
    ES1642_SendFunc      send;
    ES1642_FrameCallback on_frame;
    ES1642_ErrorCallback on_error;
    void                 *user_arg;

    ES1642_RxParser      parser;
    uint8_t              tx_buf[ES1642_FRAME_MAX_LEN];
} ES1642_Handle;

/* ========================= 基础接口 ========================= */

void ES1642_Init(ES1642_Handle *handle, ES1642_SendFunc send_func, void *user_arg);
void ES1642_RegisterFrameCallback(ES1642_Handle *handle, ES1642_FrameCallback cb);
void ES1642_RegisterErrorCallback(ES1642_Handle *handle, ES1642_ErrorCallback cb);
void ES1642_ResetParser(ES1642_Handle *handle);

int32_t ES1642_InputByte(ES1642_Handle *handle, uint8_t byte_val);
int32_t ES1642_InputBuffer(ES1642_Handle *handle, const uint8_t *buf, uint16_t len);

int32_t ES1642_BuildFrame(uint8_t ctrl,
                          uint8_t cmd,
                          const uint8_t *data,
                          uint16_t data_len,
                          uint8_t *out_buf,
                          uint16_t out_buf_size,
                          uint16_t *out_frame_len);

int32_t ES1642_SendFrame(ES1642_Handle *handle,
                         uint8_t ctrl,
                         uint8_t cmd,
                         const uint8_t *data,
                         uint16_t data_len);

/* 设备对模块通知的正常应答，通常为“同 Cmd、空 Data” */
int32_t ES1642_SendAck(ES1642_Handle *handle, uint8_t cmd);

/* ========================= 控制域辅助函数 ========================= */

/* 14H 发送数据的数据控制域：D12~D15 为中继深度 */
uint16_t ES1642_MakeSendDataCtrl(uint8_t relay_depth);

/* 17H 发起设备搜索的数据控制域：D8~D11 深度，D12~D14 搜索规则 */
uint16_t ES1642_MakeSearchCtrl(uint8_t depth, uint8_t search_rule);

/* 1BH 响应设备搜索的数据控制域：D8 为 State */
uint16_t ES1642_MakeRespondSearchCtrl(uint8_t participate);

/* ========================= 常用命令发送接口 ========================= */

int32_t ES1642_CmdReboot(ES1642_Handle *handle);
int32_t ES1642_CmdReadVersion(ES1642_Handle *handle);
int32_t ES1642_CmdReadMac(ES1642_Handle *handle);
int32_t ES1642_CmdReadCommAddr(ES1642_Handle *handle);
int32_t ES1642_CmdSetCommAddr(ES1642_Handle *handle, const uint8_t addr[ES1642_ADDR_LEN]);
int32_t ES1642_CmdReadNetParam(ES1642_Handle *handle);
int32_t ES1642_CmdSetNetParam(ES1642_Handle *handle, uint8_t relay_depth);

int32_t ES1642_CmdSendUserData(ES1642_Handle *handle,
                               uint8_t prm,
                               uint8_t relay_depth,
                               const uint8_t dst_addr[ES1642_ADDR_LEN],
                               const uint8_t *user_data,
                               uint16_t user_data_len);

int32_t ES1642_CmdStartSearch(ES1642_Handle *handle,
                              uint8_t depth,
                              uint8_t search_rule,
                              const uint8_t *attribute,
                              uint8_t attribute_len);

int32_t ES1642_CmdStopSearch(ES1642_Handle *handle);

int32_t ES1642_CmdRespondSearch(ES1642_Handle *handle,
                                uint8_t participate,
                                const uint8_t src_addr[ES1642_ADDR_LEN],
                                uint8_t task_id,
                                const uint8_t *attribute,
                                uint8_t attribute_len);

int32_t ES1642_CmdSetNetworkPsk(ES1642_Handle *handle,
                                const uint8_t dst_addr[ES1642_ADDR_LEN],
                                const uint8_t new_psk[ES1642_PSK_SET_LEN]);

int32_t ES1642_CmdClearNetworkPsk(ES1642_Handle *handle,
                                  const uint8_t dst_addr[ES1642_ADDR_LEN]);

int32_t ES1642_CmdRemoteReadVersion(ES1642_Handle *handle,
                                    const uint8_t dst_addr[ES1642_ADDR_LEN]);

int32_t ES1642_CmdRemoteReadMac(ES1642_Handle *handle,
                                const uint8_t dst_addr[ES1642_ADDR_LEN]);

int32_t ES1642_CmdRemoteReadNetParam(ES1642_Handle *handle,
                                     const uint8_t dst_addr[ES1642_ADDR_LEN]);

/* ========================= 收包解析辅助函数 ========================= */

uint8_t ES1642_IsExceptionFrame(const ES1642_Frame *frame);
int32_t ES1642_ParseException(const ES1642_Frame *frame, uint8_t *status);

int32_t ES1642_ParseRebootRsp(const ES1642_Frame *frame, uint8_t *state);
int32_t ES1642_ParseVersionRsp(const ES1642_Frame *frame, ES1642_VersionInfo *info);
int32_t ES1642_ParseReadMacRsp(const ES1642_Frame *frame, uint8_t mac[ES1642_ADDR_LEN]);
int32_t ES1642_ParseReadCommAddrRsp(const ES1642_Frame *frame, uint8_t addr[ES1642_ADDR_LEN]);
int32_t ES1642_ParseReadNetParamRsp(const ES1642_Frame *frame, ES1642_NetParamInfo *info);

int32_t ES1642_ParseRecvDataInd(const ES1642_Frame *frame, ES1642_RxDataInd *ind);
int32_t ES1642_ParseSearchResultInd(const ES1642_Frame *frame, ES1642_SearchResultInd *ind);
int32_t ES1642_ParseSearchNotify(const ES1642_Frame *frame, ES1642_SearchNotify *notify_info);
int32_t ES1642_ParsePskNotify(const ES1642_Frame *frame, ES1642_PskNotify *notify_info);
int32_t ES1642_ParsePskResultInd(const ES1642_Frame *frame, ES1642_PskResultInd *ind);

int32_t ES1642_ParseRemoteVersionRsp(const ES1642_Frame *frame,
                                     uint8_t src_addr[ES1642_ADDR_LEN],
                                     ES1642_VersionInfo *info);

int32_t ES1642_ParseRemoteReadMacRsp(const ES1642_Frame *frame,
                                     uint8_t src_addr[ES1642_ADDR_LEN],
                                     uint8_t mac[ES1642_ADDR_LEN]);

int32_t ES1642_ParseRemoteReadNetParamRsp(const ES1642_Frame *frame,
                                          uint8_t src_addr[ES1642_ADDR_LEN],
                                          ES1642_NetParamInfo *info);

#ifdef __cplusplus
}
#endif

#endif /* __ES1642_PROTOCOL_H__ */
