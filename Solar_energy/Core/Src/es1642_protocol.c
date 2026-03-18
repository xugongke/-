#include "es1642_protocol.h"
#include <string.h>

/* ============================================================
 *                     内部静态函数声明
 * ============================================================ */
static void es1642_write_le16(uint8_t *buf, uint16_t value);
static uint16_t es1642_read_le16(const uint8_t *buf);
static uint32_t es1642_read_le24(const uint8_t *buf);
static int16_t es1642_sign_extend_9bit(uint16_t value);
static void es1642_parser_reset_only(ES1642_RxParser *parser);
static void es1642_report_error(ES1642_Handle *handle, int32_t err_code);
static void es1642_calc_check(const uint8_t *buf, uint16_t data_len, uint8_t *csum, uint8_t *cxor);
static int32_t es1642_prepare_tx_frame(ES1642_Handle *handle, uint8_t ctrl, uint8_t cmd, uint16_t data_len);
static int32_t es1642_send_prepared_frame(ES1642_Handle *handle, uint16_t data_len);
static int32_t es1642_parse_basic_response(const ES1642_Frame *frame, uint8_t expect_cmd, uint16_t expect_len);
static int32_t es1642_parse_remote_common(const ES1642_Frame *frame,
                                          uint8_t expect_cmd,
                                          uint16_t local_payload_len,
                                          uint8_t src_addr[ES1642_ADDR_LEN],
                                          const uint8_t **payload);

/* ============================================================
 *                     内部静态函数实现
 * ============================================================ */

/**
 * @brief 向缓冲区写入小端序16位整数
 * @param buf 缓冲区指针
 * @param value 要写入的16位值
 */
static void es1642_write_le16(uint8_t *buf, uint16_t value)
{
    buf[0] = (uint8_t)(value & 0xFFu);
    buf[1] = (uint8_t)((value >> 8) & 0xFFu);
}

/**
 * @brief 从缓冲区读取小端序16位整数
 * @param buf 缓冲区指针
 * @return 读取的16位值
 */
static uint16_t es1642_read_le16(const uint8_t *buf)
{
    return (uint16_t)((uint16_t)buf[0] | ((uint16_t)buf[1] << 8));
}

/**
 * @brief 从缓冲区读取小端序24位整数
 * @param buf 缓冲区指针
 * @return 读取的24位值
 */
static uint32_t es1642_read_le24(const uint8_t *buf)
{
    return ((uint32_t)buf[0]) |
           ((uint32_t)buf[1] << 8) |
           ((uint32_t)buf[2] << 16);
}

/**
 * @brief 9位符号扩展
 * @param value 要扩展的16位值（只使用低9位）
 * @return 扩展后的16位有符号值
 */
static int16_t es1642_sign_extend_9bit(uint16_t value)
{
    value &= 0x01FFu;
    if ((value & 0x0100u) != 0u)
    {
        value |= 0xFE00u;
    }
    return (int16_t)value;
}

/**
 * @brief 重置接收解析器状态
 * @param parser 解析器指针
 */
static void es1642_parser_reset_only(ES1642_RxParser *parser)
{
    if (parser == NULL)
    {
        return;
    }

    parser->state = ES1642_RX_WAIT_HEAD;
    parser->data_index = 0u;
    parser->sum = 0u;
    parser->xorv = 0u;
}

/**
 * @brief 报告错误
 * @param handle ES1642句柄
 * @param err_code 错误码
 */
static void es1642_report_error(ES1642_Handle *handle, int32_t err_code)
{
    if ((handle != NULL) && (handle->on_error != NULL))
    {
        handle->on_error(handle, err_code);
    }
}

/**
 * @brief 计算协议规定的校验和
 * @param buf 帧缓冲区指针
 * @param data_len 数据长度
 * @param csum 输出参数，算术和校验值
 * @param cxor 输出参数，异或校验值
 */
static void es1642_calc_check(const uint8_t *buf, uint16_t data_len, uint8_t *csum, uint8_t *cxor)
{
    uint16_t i;
    uint8_t sum_val;
    uint8_t xor_val;
    uint16_t end_index;

    sum_val = 0u;
    xor_val = 0u;

    /*
     * buf 指向完整帧起始地址，buf[0] 为 0x79，
     * 需要参与校验的是 buf[1] ~ buf[4 + data_len]
     */
    end_index = (uint16_t)(4u + data_len);
    for (i = 1u; i <= end_index; i++)
    {
        sum_val = (uint8_t)(sum_val + buf[i]);
        xor_val = (uint8_t)(xor_val ^ buf[i]);
    }

    *csum = sum_val;
    *cxor = xor_val;
}

/**
 * @brief 准备发送帧
 * @param handle ES1642句柄
 * @param ctrl 控制域
 * @param cmd 命令
 * @param data_len 数据长度
 * @return 操作结果
 */
static int32_t es1642_prepare_tx_frame(ES1642_Handle *handle, uint8_t ctrl, uint8_t cmd, uint16_t data_len)
{
    if ((handle == NULL) || (handle->send == NULL))
    {
        return ES1642_RES_ERR_PARAM;
    }

    if (data_len > ES1642_MAX_DATA_LEN)
    {
        return ES1642_RES_ERR_DATA_TOO_LONG;
    }

    handle->tx_buf[0] = ES1642_FRAME_HEAD;
    es1642_write_le16(&handle->tx_buf[1], data_len);
    handle->tx_buf[3] = ctrl;
    handle->tx_buf[4] = cmd;

    return ES1642_RES_OK;
}

/**
 * @brief 发送已准备好的帧
 * @param handle ES1642句柄
 * @param data_len 数据长度
 * @return 操作结果
 */
static int32_t es1642_send_prepared_frame(ES1642_Handle *handle, uint16_t data_len)
{
    uint16_t frame_len;
    uint8_t csum;
    uint8_t cxor;
    int32_t ret;

    if ((handle == NULL) || (handle->send == NULL))
    {
        return ES1642_RES_ERR_PARAM;
    }

    es1642_calc_check(handle->tx_buf, data_len, &csum, &cxor);

    handle->tx_buf[5u + data_len] = csum;
    handle->tx_buf[6u + data_len] = cxor;

    frame_len = (uint16_t)(ES1642_FRAME_FIXED_LEN + data_len);
    ret = handle->send(handle->tx_buf, frame_len, handle->user_arg);
    if (ret < 0)
    {
        return ret;
    }

    return ES1642_RES_OK;
}

/**
 * @brief 解析基本响应帧
 * @param frame 接收到的帧指针
 * @param expect_cmd 期望的命令
 * @param expect_len 期望的数据长度
 * @return 操作结果
 */
static int32_t es1642_parse_basic_response(const ES1642_Frame *frame, uint8_t expect_cmd, uint16_t expect_len)
{
    if (frame == NULL)
    {
        return ES1642_RES_ERR_PARAM;
    }

    if (ES1642_IsExceptionFrame(frame) != 0u)
    {
        return ES1642_RES_ERR_EXCEPTION;
    }

    if (frame->cmd != expect_cmd)
    {
        return ES1642_RES_ERR_CMD;
    }

    if (frame->len != expect_len)
    {
        return ES1642_RES_ERR_LENGTH;
    }

    return ES1642_RES_OK;
}

/**
 * @brief 解析远程响应帧的通用部分
 * @param frame 接收到的帧指针
 * @param expect_cmd 期望的命令
 * @param local_payload_len 本地负载长度
 * @param src_addr 输出参数，源地址
 * @param payload 输出参数，负载指针
 * @return 操作结果
 */
static int32_t es1642_parse_remote_common(const ES1642_Frame *frame,
                                          uint8_t expect_cmd,
                                          uint16_t local_payload_len,
                                          uint8_t src_addr[ES1642_ADDR_LEN],
                                          const uint8_t **payload)
{
    if ((frame == NULL) || (src_addr == NULL) || (payload == NULL))
    {
        return ES1642_RES_ERR_PARAM;
    }

    if (ES1642_IsExceptionFrame(frame) != 0u)
    {
        return ES1642_RES_ERR_EXCEPTION;
    }

    if (frame->cmd != expect_cmd)
    {
        return ES1642_RES_ERR_CMD;
    }

    if (frame->len != (uint16_t)(ES1642_ADDR_LEN + local_payload_len))
    {
        return ES1642_RES_ERR_LENGTH;
    }

    memcpy(src_addr, &frame->data[0], ES1642_ADDR_LEN);
    *payload = &frame->data[ES1642_ADDR_LEN];

    return ES1642_RES_OK;
}

/* ============================================================
 *                     对外基础接口实现
 * ============================================================ */

/**
 * @brief 初始化ES1642协议处理句柄
 * @param handle ES1642句柄指针
 * @param send_func 发送函数指针
 * @param user_arg 用户参数，会传递给发送函数
 */
void ES1642_Init(ES1642_Handle *handle, ES1642_SendFunc send_func, void *user_arg)
{
    if (handle == NULL)
    {
        return;
    }

    memset(handle, 0, sizeof(ES1642_Handle));
    handle->send = send_func;
    handle->user_arg = user_arg;
    es1642_parser_reset_only(&handle->parser);
}

/**
 * @brief 注册帧接收回调函数
 * @param handle ES1642句柄指针
 * @param cb 回调函数指针
 */
void ES1642_RegisterFrameCallback(ES1642_Handle *handle, ES1642_FrameCallback cb)
{
    if (handle == NULL)
    {
        return;
    }

    handle->on_frame = cb;
}

/**
 * @brief 注册错误回调函数
 * @param handle ES1642句柄指针
 * @param cb 回调函数指针
 */
void ES1642_RegisterErrorCallback(ES1642_Handle *handle, ES1642_ErrorCallback cb)
{
    if (handle == NULL)
    {
        return;
    }

    handle->on_error = cb;
}

/**
 * @brief 重置接收解析器
 * @param handle ES1642句柄指针
 */
void ES1642_ResetParser(ES1642_Handle *handle)
{
    if (handle == NULL)
    {
        return;
    }

    es1642_parser_reset_only(&handle->parser);
}

/**
 * @brief 按字节接收状态机
 * @param handle ES1642句柄指针
 * @param byte_val 接收到的字节值
 * @return 操作结果
 * 
 * 使用建议：
 * 1. 串口每收到一个字节就调用一次；
 * 2. 返回 ES1642_RES_FRAME_OK 表示刚刚收到了一个完整有效帧；
 * 3. 完整帧内容保存在 handle->parser.frame 中，在下一次收到新帧前有效；
 * 4. 如注册了 on_frame 回调，会在收到完整帧时自动回调。
 */
int32_t ES1642_InputByte(ES1642_Handle *handle, uint8_t byte_val)
{
    ES1642_RxParser *p;

    if (handle == NULL)
    {
        return ES1642_RES_ERR_PARAM;
    }

    p = &handle->parser;

    switch (p->state)
    {
        case ES1642_RX_WAIT_HEAD:
        {
            if (byte_val == ES1642_FRAME_HEAD)
            {
                p->frame.head = byte_val;
                p->frame.len = 0u;
                p->frame.ctrl = 0u;
                p->frame.cmd = 0u;
                p->frame.csum = 0u;
                p->frame.cxor = 0u;
                p->data_index = 0u;
                p->sum = 0u;
                p->xorv = 0u;
                p->state = ES1642_RX_WAIT_LEN_L;
            }
            return ES1642_RES_IN_PROGRESS;
        }

        case ES1642_RX_WAIT_LEN_L:
        {
            p->frame.len = byte_val;
            p->sum = (uint8_t)(p->sum + byte_val);
            p->xorv = (uint8_t)(p->xorv ^ byte_val);
            p->state = ES1642_RX_WAIT_LEN_H;
            return ES1642_RES_IN_PROGRESS;
        }

        case ES1642_RX_WAIT_LEN_H:
        {
            p->frame.len |= (uint16_t)((uint16_t)byte_val << 8);
            p->sum = (uint8_t)(p->sum + byte_val);
            p->xorv = (uint8_t)(p->xorv ^ byte_val);

            if (p->frame.len > ES1642_MAX_DATA_LEN)
            {
                es1642_report_error(handle, ES1642_RES_ERR_DATA_TOO_LONG);
                es1642_parser_reset_only(p);
                return ES1642_RES_ERR_DATA_TOO_LONG;
            }

            p->state = ES1642_RX_WAIT_CTRL;
            return ES1642_RES_IN_PROGRESS;
        }

        case ES1642_RX_WAIT_CTRL:
        {
            p->frame.ctrl = byte_val;
            p->sum = (uint8_t)(p->sum + byte_val);
            p->xorv = (uint8_t)(p->xorv ^ byte_val);
            p->state = ES1642_RX_WAIT_CMD;
            return ES1642_RES_IN_PROGRESS;
        }

        case ES1642_RX_WAIT_CMD:
        {
            p->frame.cmd = byte_val;
            p->sum = (uint8_t)(p->sum + byte_val);
            p->xorv = (uint8_t)(p->xorv ^ byte_val);

            if (p->frame.len == 0u)
            {
                p->state = ES1642_RX_WAIT_CSUM;
            }
            else
            {
                p->data_index = 0u;
                p->state = ES1642_RX_WAIT_DATA;
            }
            return ES1642_RES_IN_PROGRESS;
        }

        case ES1642_RX_WAIT_DATA:
        {
            p->frame.data[p->data_index] = byte_val;
            p->data_index++;
            p->sum = (uint8_t)(p->sum + byte_val);
            p->xorv = (uint8_t)(p->xorv ^ byte_val);

            if (p->data_index >= p->frame.len)
            {
                p->state = ES1642_RX_WAIT_CSUM;
            }
            return ES1642_RES_IN_PROGRESS;
        }

        case ES1642_RX_WAIT_CSUM:
        {
            p->frame.csum = byte_val;
            if (p->frame.csum != p->sum)
            {
                es1642_report_error(handle, ES1642_RES_ERR_CHECKSUM);
                es1642_parser_reset_only(p);
                return ES1642_RES_ERR_CHECKSUM;
            }

            p->state = ES1642_RX_WAIT_CXOR;
            return ES1642_RES_IN_PROGRESS;
        }

        case ES1642_RX_WAIT_CXOR:
        {
            p->frame.cxor = byte_val;
            if (p->frame.cxor != p->xorv)
            {
                es1642_report_error(handle, ES1642_RES_ERR_XOR);
                es1642_parser_reset_only(p);
                return ES1642_RES_ERR_XOR;
            }

            if (handle->on_frame != NULL)
            {
                handle->on_frame(handle, &p->frame);
            }

            es1642_parser_reset_only(p);
            return ES1642_RES_FRAME_OK;
        }

        default:
        {
            es1642_parser_reset_only(p);
            return ES1642_RES_ERR_FRAME;
        }
    }
}

/**
 * @brief 按缓冲区接收数据
 * @param handle ES1642句柄指针
 * @param buf 缓冲区指针
 * @param len 缓冲区长度
 * @return 操作结果
 */
int32_t ES1642_InputBuffer(ES1642_Handle *handle, const uint8_t *buf, uint16_t len)
{
    uint16_t i;
    int32_t ret;
    int32_t last_ret;

    if ((handle == NULL) || (buf == NULL))
    {
        return ES1642_RES_ERR_PARAM;
    }

    last_ret = ES1642_RES_IN_PROGRESS;
    for (i = 0u; i < len; i++)
    {
        ret = ES1642_InputByte(handle, buf[i]);
        if (ret < 0)
        {
            last_ret = ret;
        }
        else if (ret == ES1642_RES_FRAME_OK)
        {
            last_ret = ES1642_RES_FRAME_OK;
        }
    }

    return last_ret;
}

/**
 * @brief 构建协议帧
 * @param ctrl 控制域
 * @param cmd 命令
 * @param data 数据指针
 * @param data_len 数据长度
 * @param out_buf 输出缓冲区指针
 * @param out_buf_size 输出缓冲区大小
 * @param out_frame_len 输出参数，帧长度
 * @return 操作结果
 */
int32_t ES1642_BuildFrame(uint8_t ctrl,
                          uint8_t cmd,
                          const uint8_t *data,
                          uint16_t data_len,
                          uint8_t *out_buf,
                          uint16_t out_buf_size,
                          uint16_t *out_frame_len)
{
    uint16_t need_len;
    uint8_t csum;
    uint8_t cxor;

    if ((out_buf == NULL) || (out_frame_len == NULL))
    {
        return ES1642_RES_ERR_PARAM;
    }

    if ((data_len > 0u) && (data == NULL))
    {
        return ES1642_RES_ERR_PARAM;
    }

    if (data_len > ES1642_MAX_DATA_LEN)
    {
        return ES1642_RES_ERR_DATA_TOO_LONG;
    }

    need_len = (uint16_t)(ES1642_FRAME_FIXED_LEN + data_len);
    if (out_buf_size < need_len)
    {
        return ES1642_RES_ERR_BUF_SMALL;
    }

    out_buf[0] = ES1642_FRAME_HEAD;
    es1642_write_le16(&out_buf[1], data_len);
    out_buf[3] = ctrl;
    out_buf[4] = cmd;

    if (data_len > 0u)
    {
        memcpy(&out_buf[5], data, data_len);
    }

    es1642_calc_check(out_buf, data_len, &csum, &cxor);
    out_buf[5u + data_len] = csum;
    out_buf[6u + data_len] = cxor;

    *out_frame_len = need_len;
    return ES1642_RES_OK;
}

/**
 * @brief 发送协议帧
 * @param handle ES1642句柄指针
 * @param ctrl 控制域
 * @param cmd 命令
 * @param data 数据指针
 * @param data_len 数据长度
 * @return 操作结果
 */
int32_t ES1642_SendFrame(ES1642_Handle *handle,
                         uint8_t ctrl,
                         uint8_t cmd,
                         const uint8_t *data,
                         uint16_t data_len)
{
    int32_t ret;

    if ((handle == NULL) || (handle->send == NULL))
    {
        return ES1642_RES_ERR_PARAM;
    }

    if ((data_len > 0u) && (data == NULL))
    {
        return ES1642_RES_ERR_PARAM;
    }

    ret = es1642_prepare_tx_frame(handle, ctrl, cmd, data_len);
    if (ret != ES1642_RES_OK)
    {
        return ret;
    }

    if (data_len > 0u)
    {
        memcpy(&handle->tx_buf[5], data, data_len);
    }

    return es1642_send_prepared_frame(handle, data_len);
}

/**
 * @brief 发送确认帧
 * @param handle ES1642句柄指针
 * @param cmd 命令
 * @return 操作结果
 */
int32_t ES1642_SendAck(ES1642_Handle *handle, uint8_t cmd)
{
    return ES1642_SendFrame(handle, ES1642_CTRL_DEFAULT_ACK, cmd, NULL, 0u);
}

/* ============================================================
 *                     控制域辅助函数
 * ============================================================ */

/**
 * @brief 构建发送数据控制域
 * @param relay_depth 中继深度
 * @return 控制域值
 */
uint16_t ES1642_MakeSendDataCtrl(uint8_t relay_depth)
{
    return (uint16_t)(((uint16_t)(relay_depth & 0x0Fu)) << 12);
}

/**
 * @brief 构建搜索控制域
 * @param depth 搜索深度
 * @param search_rule 搜索规则
 * @return 控制域值
 */
uint16_t ES1642_MakeSearchCtrl(uint8_t depth, uint8_t search_rule)
{
    return (uint16_t)((((uint16_t)(depth & 0x0Fu)) << 8) |
                      (((uint16_t)(search_rule & 0x07u)) << 12));
}

/**
 * @brief 构建响应搜索控制域
 * @param participate 是否参与
 * @return 控制域值
 */
uint16_t ES1642_MakeRespondSearchCtrl(uint8_t participate)
{
    return (uint16_t)(((uint16_t)(participate ? 1u : 0u)) << 8);
}

/* ============================================================
 *                     常用命令发送实现
 * ============================================================ */

/**
 * @brief 重启命令
 * @param handle ES1642句柄指针
 * @return 操作结果
 */
int32_t ES1642_CmdReboot(ES1642_Handle *handle)
{
    return ES1642_SendFrame(handle, ES1642_CTRL_DEFAULT_REQ, ES1642_CMD_REBOOT, NULL, 0u);
}

/**
 * @brief 读取版本命令
 * @param handle ES1642句柄指针
 * @return 操作结果
 */
int32_t ES1642_CmdReadVersion(ES1642_Handle *handle)
{
    return ES1642_SendFrame(handle, ES1642_CTRL_DEFAULT_REQ, ES1642_CMD_READ_VERSION, NULL, 0u);
}

/**
 * @brief 读取MAC地址命令
 * @param handle ES1642句柄指针
 * @return 操作结果
 */
int32_t ES1642_CmdReadMac(ES1642_Handle *handle)
{
    return ES1642_SendFrame(handle, ES1642_CTRL_DEFAULT_REQ, ES1642_CMD_READ_MAC, NULL, 0u);
}

/**
 * @brief 读取通信地址命令
 * @param handle ES1642句柄指针
 * @return 操作结果
 */
int32_t ES1642_CmdReadCommAddr(ES1642_Handle *handle)
{
    return ES1642_SendFrame(handle, ES1642_CTRL_DEFAULT_REQ, ES1642_CMD_READ_COMM_ADDR, NULL, 0u);
}

/**
 * @brief 设置通信地址命令
 * @param handle ES1642句柄指针
 * @param addr 地址指针
 * @return 操作结果
 */
int32_t ES1642_CmdSetCommAddr(ES1642_Handle *handle, const uint8_t addr[ES1642_ADDR_LEN])
{
    if (addr == NULL)
    {
        return ES1642_RES_ERR_PARAM;
    }

    return ES1642_SendFrame(handle, ES1642_CTRL_DEFAULT_REQ, ES1642_CMD_SET_COMM_ADDR, addr, ES1642_ADDR_LEN);
}

/**
 * @brief 读取网络参数命令
 * @param handle ES1642句柄指针
 * @return 操作结果
 */
int32_t ES1642_CmdReadNetParam(ES1642_Handle *handle)
{
    return ES1642_SendFrame(handle, ES1642_CTRL_DEFAULT_REQ, ES1642_CMD_READ_NET_PARAM, NULL, 0u);
}

/**
 * @brief 设置网络参数命令
 * @param handle ES1642句柄指针
 * @param relay_depth 中继深度
 * @return 操作结果
 */
int32_t ES1642_CmdSetNetParam(ES1642_Handle *handle, uint8_t relay_depth)
{
    uint8_t data[2];

    data[0] = relay_depth;
    data[1] = 0x00u; /* RSV */

    return ES1642_SendFrame(handle, ES1642_CTRL_DEFAULT_REQ, ES1642_CMD_SET_NET_PARAM, data, 2u);
}

/**
 * @brief 发送用户数据命令
 * @param handle ES1642句柄指针
 * @param prm 是否主动发送
 * @param relay_depth 中继深度
 * @param dst_addr 目的地址指针
 * @param user_data 用户数据指针
 * @param user_data_len 用户数据长度
 * @return 操作结果
 */
int32_t ES1642_CmdSendUserData(ES1642_Handle *handle,
                               uint8_t prm,
                               uint8_t relay_depth,
                               const uint8_t dst_addr[ES1642_ADDR_LEN],
                               const uint8_t *user_data,
                               uint16_t user_data_len)
{
    uint16_t data_len;
    uint16_t data_ctrl;
    uint8_t ctrl;
    int32_t ret;

    if ((handle == NULL) || (dst_addr == NULL))
    {
        return ES1642_RES_ERR_PARAM;
    }

    if ((user_data_len > 0u) && (user_data == NULL))
    {
        return ES1642_RES_ERR_PARAM;
    }

    data_len = (uint16_t)(2u + ES1642_ADDR_LEN + 2u + user_data_len);
    ret = es1642_prepare_tx_frame(handle,
                                  (prm != 0u) ? ES1642_CTRL_DATA_ACTIVE : ES1642_CTRL_DATA_RESPONSE,
                                  ES1642_CMD_SEND_DATA,
                                  data_len);
    if (ret != ES1642_RES_OK)
    {
        return ret;
    }

    data_ctrl = ES1642_MakeSendDataCtrl(relay_depth);
    es1642_write_le16(&handle->tx_buf[5], data_ctrl);
    memcpy(&handle->tx_buf[7], dst_addr, ES1642_ADDR_LEN);
    es1642_write_le16(&handle->tx_buf[13], user_data_len);

    if (user_data_len > 0u)
    {
        memcpy(&handle->tx_buf[15], user_data, user_data_len);
    }

    ctrl = (prm != 0u) ? ES1642_CTRL_DATA_ACTIVE : ES1642_CTRL_DATA_RESPONSE;
    handle->tx_buf[3] = ctrl;

    return es1642_send_prepared_frame(handle, data_len);
}

/**
 * @brief 开始搜索命令
 * @param handle ES1642句柄指针
 * @param depth 搜索深度
 * @param search_rule 搜索规则
 * @param attribute 属性指针
 * @param attribute_len 属性长度
 * @return 操作结果
 */
int32_t ES1642_CmdStartSearch(ES1642_Handle *handle,
                              uint8_t depth,
                              uint8_t search_rule,
                              const uint8_t *attribute,
                              uint8_t attribute_len)
{
    uint16_t data_len;
    uint16_t data_ctrl;
    int32_t ret;

    if ((attribute_len > 0u) && (attribute == NULL))
    {
        return ES1642_RES_ERR_PARAM;
    }

    data_len = (uint16_t)(2u + 1u + attribute_len);
    ret = es1642_prepare_tx_frame(handle, ES1642_CTRL_DEFAULT_REQ, ES1642_CMD_START_SEARCH, data_len);
    if (ret != ES1642_RES_OK)
    {
        return ret;
    }

    data_ctrl = ES1642_MakeSearchCtrl(depth, search_rule);
    es1642_write_le16(&handle->tx_buf[5], data_ctrl);
    handle->tx_buf[7] = attribute_len;

    if (attribute_len > 0u)
    {
        memcpy(&handle->tx_buf[8], attribute, attribute_len);
    }

    return es1642_send_prepared_frame(handle, data_len);
}

/**
 * @brief 停止搜索命令
 * @param handle ES1642句柄指针
 * @return 操作结果
 */
int32_t ES1642_CmdStopSearch(ES1642_Handle *handle)
{
    return ES1642_SendFrame(handle, ES1642_CTRL_DEFAULT_REQ, ES1642_CMD_STOP_SEARCH, NULL, 0u);
}

/**
 * @brief 响应搜索命令
 * @param handle ES1642句柄指针
 * @param participate 是否参与
 * @param src_addr 源地址指针
 * @param task_id 任务ID
 * @param attribute 属性指针
 * @param attribute_len 属性长度
 * @return 操作结果
 */
int32_t ES1642_CmdRespondSearch(ES1642_Handle *handle,
                                uint8_t participate,
                                const uint8_t src_addr[ES1642_ADDR_LEN],
                                uint8_t task_id,
                                const uint8_t *attribute,
                                uint8_t attribute_len)
{
    uint16_t data_len;
    uint16_t data_ctrl;
    int32_t ret;

    if ((handle == NULL) || (src_addr == NULL))
    {
        return ES1642_RES_ERR_PARAM;
    }

    if (participate == 0u)
    {
        attribute_len = 0u;
        attribute = NULL;
    }
    else if ((attribute_len > 0u) && (attribute == NULL))
    {
        return ES1642_RES_ERR_PARAM;
    }

    data_len = (uint16_t)(2u + ES1642_ADDR_LEN + 1u + 1u + attribute_len);
    ret = es1642_prepare_tx_frame(handle, ES1642_CTRL_DEFAULT_REQ, ES1642_CMD_RESPOND_SEARCH, data_len);
    if (ret != ES1642_RES_OK)
    {
        return ret;
    }

    data_ctrl = ES1642_MakeRespondSearchCtrl(participate);
    es1642_write_le16(&handle->tx_buf[5], data_ctrl);
    memcpy(&handle->tx_buf[7], src_addr, ES1642_ADDR_LEN);
    handle->tx_buf[13] = task_id;
    handle->tx_buf[14] = attribute_len;

    if (attribute_len > 0u)
    {
        memcpy(&handle->tx_buf[15], attribute, attribute_len);
    }

    return es1642_send_prepared_frame(handle, data_len);
}

/**
 * @brief 设置网络PSK命令
 * @param handle ES1642句柄指针
 * @param dst_addr 目的地址指针
 * @param new_psk 新PSK指针
 * @return 操作结果
 */
int32_t ES1642_CmdSetNetworkPsk(ES1642_Handle *handle,
                                const uint8_t dst_addr[ES1642_ADDR_LEN],
                                const uint8_t new_psk[ES1642_PSK_SET_LEN])
{
    uint16_t data_ctrl;
    int32_t ret;
    uint16_t data_len;

    if ((handle == NULL) || (dst_addr == NULL) || (new_psk == NULL))
    {
        return ES1642_RES_ERR_PARAM;
    }

    data_len = (uint16_t)(2u + ES1642_ADDR_LEN + 1u + 1u + ES1642_PSK_SET_LEN);
    ret = es1642_prepare_tx_frame(handle, ES1642_CTRL_DEFAULT_REQ, ES1642_CMD_SET_NETWORK_PSK, data_len);
    if (ret != ES1642_RES_OK)
    {
        return ret;
    }

    data_ctrl = 0x0044u;
    es1642_write_le16(&handle->tx_buf[5], data_ctrl);
    memcpy(&handle->tx_buf[7], dst_addr, ES1642_ADDR_LEN);
    handle->tx_buf[13] = 0x00u; /* Old Psk Len 固定为 0 */
    handle->tx_buf[14] = ES1642_PSK_SET_LEN;
    memcpy(&handle->tx_buf[15], new_psk, ES1642_PSK_SET_LEN);

    return es1642_send_prepared_frame(handle, data_len);
}

/**
 * @brief 清除网络PSK命令
 * @param handle ES1642句柄指针
 * @param dst_addr 目的地址指针
 * @return 操作结果
 */
int32_t ES1642_CmdClearNetworkPsk(ES1642_Handle *handle,
                                  const uint8_t dst_addr[ES1642_ADDR_LEN])
{
    uint16_t data_ctrl;
    int32_t ret;
    uint16_t data_len;

    if ((handle == NULL) || (dst_addr == NULL))
    {
        return ES1642_RES_ERR_PARAM;
    }

    /*
     * 按协议：清除网络口令时 New Psk Len 只要"不是 08H"即可。
     * 这里采用最简单方式：New Psk Len = 0，不附加 New Psk 数据。
     */
    data_len = (uint16_t)(2u + ES1642_ADDR_LEN + 1u + 1u);
    ret = es1642_prepare_tx_frame(handle, ES1642_CTRL_DEFAULT_REQ, ES1642_CMD_SET_NETWORK_PSK, data_len);
    if (ret != ES1642_RES_OK)
    {
        return ret;
    }

    data_ctrl = 0x0044u;
    es1642_write_le16(&handle->tx_buf[5], data_ctrl);
    memcpy(&handle->tx_buf[7], dst_addr, ES1642_ADDR_LEN);
    handle->tx_buf[13] = 0x00u; /* Old Psk Len */
    handle->tx_buf[14] = 0x00u; /* New Psk Len != 0x08 即可 */

    return es1642_send_prepared_frame(handle, data_len);
}

/**
 * @brief 远程读取版本命令
 * @param handle ES1642句柄指针
 * @param dst_addr 目的地址指针
 * @return 操作结果
 */
int32_t ES1642_CmdRemoteReadVersion(ES1642_Handle *handle,
                                    const uint8_t dst_addr[ES1642_ADDR_LEN])
{
    uint8_t data[1 + ES1642_ADDR_LEN];

    if (dst_addr == NULL)
    {
        return ES1642_RES_ERR_PARAM;
    }

    data[0] = 0x00u; /* Data Ctrl 固定 00H */
    memcpy(&data[1], dst_addr, ES1642_ADDR_LEN);

    return ES1642_SendFrame(handle, ES1642_CTRL_DEFAULT_REQ, ES1642_CMD_REMOTE_READ_VERSION, data, sizeof(data));
}

/**
 * @brief 远程读取MAC地址命令
 * @param handle ES1642句柄指针
 * @param dst_addr 目的地址指针
 * @return 操作结果
 */
int32_t ES1642_CmdRemoteReadMac(ES1642_Handle *handle,
                                const uint8_t dst_addr[ES1642_ADDR_LEN])
{
    uint8_t data[1 + ES1642_ADDR_LEN];

    if (dst_addr == NULL)
    {
        return ES1642_RES_ERR_PARAM;
    }

    data[0] = 0x00u; /* Data Ctrl 固定 00H */
    memcpy(&data[1], dst_addr, ES1642_ADDR_LEN);

    return ES1642_SendFrame(handle, ES1642_CTRL_DEFAULT_REQ, ES1642_CMD_REMOTE_READ_MAC, data, sizeof(data));
}

/**
 * @brief 远程读取网络参数命令
 * @param handle ES1642句柄指针
 * @param dst_addr 目的地址指针
 * @return 操作结果
 */
int32_t ES1642_CmdRemoteReadNetParam(ES1642_Handle *handle,
                                     const uint8_t dst_addr[ES1642_ADDR_LEN])
{
    uint8_t data[1 + ES1642_ADDR_LEN];

    if (dst_addr == NULL)
    {
        return ES1642_RES_ERR_PARAM;
    }

    data[0] = 0x00u; /* Data Ctrl 固定 00H */
    memcpy(&data[1], dst_addr, ES1642_ADDR_LEN);

    return ES1642_SendFrame(handle, ES1642_CTRL_DEFAULT_REQ, ES1642_CMD_REMOTE_READ_NET_PARAM, data, sizeof(data));
}

/* ============================================================
 *                     收包解析辅助函数实现
 * ============================================================ */

/**
 * @brief 判断是否为异常帧
 * @param frame 帧指针
 * @return 1是异常帧，0不是异常帧
 */
uint8_t ES1642_IsExceptionFrame(const ES1642_Frame *frame)
{
    if (frame == NULL)
    {
        return 0u;
    }

    return ES1642_CTRL_IS_EXCEPTION(frame->ctrl);
}

/**
 * @brief 解析异常帧
 * @param frame 帧指针
 * @param status 输出参数，状态
 * @return 操作结果
 */
int32_t ES1642_ParseException(const ES1642_Frame *frame, uint8_t *status)
{
    if ((frame == NULL) || (status == NULL))
    {
        return ES1642_RES_ERR_PARAM;
    }

    if (ES1642_IsExceptionFrame(frame) == 0u)
    {
        return ES1642_RES_ERR_FRAME;
    }

    if (frame->len != 1u)
    {
        return ES1642_RES_ERR_LENGTH;
    }

    *status = frame->data[0];
    return ES1642_RES_OK;
}

/**
 * @brief 解析重启响应
 * @param frame 帧指针
 * @param state 输出参数，状态
 * @return 操作结果
 */
int32_t ES1642_ParseRebootRsp(const ES1642_Frame *frame, uint8_t *state)
{
    int32_t ret;

    if (state == NULL)
    {
        return ES1642_RES_ERR_PARAM;
    }

    ret = es1642_parse_basic_response(frame, ES1642_CMD_REBOOT, 2u);
    if (ret != ES1642_RES_OK)
    {
        return ret;
    }

    *state = frame->data[0];
    return ES1642_RES_OK;
}

/**
 * @brief 解析版本响应
 * @param frame 帧指针
 * @param info 输出参数，版本信息
 * @return 操作结果
 */
int32_t ES1642_ParseVersionRsp(const ES1642_Frame *frame, ES1642_VersionInfo *info)
{
    int32_t ret;

    if (info == NULL)
    {
        return ES1642_RES_ERR_PARAM;
    }

    ret = es1642_parse_basic_response(frame, ES1642_CMD_READ_VERSION, 7u);
    if (ret != ES1642_RES_OK)
    {
        return ret;
    }

    info->vendor_id = es1642_read_le16(&frame->data[0]);
    info->chip_type = es1642_read_le16(&frame->data[2]);
    info->product_info = frame->data[4];
    info->version_bcd = es1642_read_le16(&frame->data[5]);

    return ES1642_RES_OK;
}

/**
 * @brief 解析读取MAC地址响应
 * @param frame 帧指针
 * @param mac 输出参数，MAC地址
 * @return 操作结果
 */
int32_t ES1642_ParseReadMacRsp(const ES1642_Frame *frame, uint8_t mac[ES1642_ADDR_LEN])
{
    int32_t ret;

    if (mac == NULL)
    {
        return ES1642_RES_ERR_PARAM;
    }

    ret = es1642_parse_basic_response(frame, ES1642_CMD_READ_MAC, ES1642_ADDR_LEN);
    if (ret != ES1642_RES_OK)
    {
        return ret;
    }

    memcpy(mac, frame->data, ES1642_ADDR_LEN);
    return ES1642_RES_OK;
}

/**
 * @brief 解析读取通信地址响应
 * @param frame 帧指针
 * @param addr 输出参数，地址
 * @return 操作结果
 */
int32_t ES1642_ParseReadCommAddrRsp(const ES1642_Frame *frame, uint8_t addr[ES1642_ADDR_LEN])
{
    int32_t ret;

    if (addr == NULL)
    {
        return ES1642_RES_ERR_PARAM;
    }

    ret = es1642_parse_basic_response(frame, ES1642_CMD_READ_COMM_ADDR, ES1642_ADDR_LEN);
    if (ret != ES1642_RES_OK)
    {
        return ret;
    }

    memcpy(addr, frame->data, ES1642_ADDR_LEN);
    return ES1642_RES_OK;
}

/**
 * @brief 解析读取网络参数响应
 * @param frame 帧指针
 * @param info 输出参数，网络参数信息
 * @return 操作结果
 */
int32_t ES1642_ParseReadNetParamRsp(const ES1642_Frame *frame, ES1642_NetParamInfo *info)
{
    int32_t ret;

    if (info == NULL)
    {
        return ES1642_RES_ERR_PARAM;
    }

    ret = es1642_parse_basic_response(frame, ES1642_CMD_READ_NET_PARAM, 7u);
    if (ret != ES1642_RES_OK)
    {
        return ret;
    }

    info->relay_depth = frame->data[0];
    memcpy(info->network_psk, &frame->data[1], 4u);
    info->rsv0 = frame->data[5];
    info->rsv1 = frame->data[6];

    return ES1642_RES_OK;
}

/**
 * @brief 解析接收数据指示
 * @param frame 帧指针
 * @param ind 输出参数，接收数据指示
 * @return 操作结果
 */
int32_t ES1642_ParseRecvDataInd(const ES1642_Frame *frame, ES1642_RxDataInd *ind)
{
    uint32_t ctrl24;
    uint16_t user_data_len;

    if ((frame == NULL) || (ind == NULL))
    {
        return ES1642_RES_ERR_PARAM;
    }

    if (ES1642_IsExceptionFrame(frame) != 0u)
    {
        return ES1642_RES_ERR_EXCEPTION;
    }

    if (frame->cmd != ES1642_CMD_RECV_DATA)
    {
        return ES1642_RES_ERR_CMD;
    }

    if (frame->len < 11u)
    {
        return ES1642_RES_ERR_LENGTH;
    }

    ctrl24 = es1642_read_le24(&frame->data[0]);
    user_data_len = es1642_read_le16(&frame->data[9]);

    if (frame->len != (uint16_t)(11u + user_data_len))
    {
        return ES1642_RES_ERR_LENGTH;
    }

    ind->data_ctrl_raw = ctrl24;
    ind->relay_depth = (uint8_t)((ctrl24 >> 8) & 0x0Fu);
    ind->rssi = es1642_sign_extend_9bit((uint16_t)((ctrl24 >> 15) & 0x01FFu));
    memcpy(ind->src_addr, &frame->data[3], ES1642_ADDR_LEN);
    ind->user_data_len = user_data_len;
    ind->user_data = &frame->data[11];

    return ES1642_RES_OK;
}

/**
 * @brief 解析搜索结果指示
 * @param frame 帧指针
 * @param ind 输出参数，搜索结果指示
 * @return 操作结果
 */
int32_t ES1642_ParseSearchResultInd(const ES1642_Frame *frame, ES1642_SearchResultInd *ind)
{
    uint16_t dev_ctrl;
    uint8_t attribute_len;

    if ((frame == NULL) || (ind == NULL))
    {
        return ES1642_RES_ERR_PARAM;
    }

    if (ES1642_IsExceptionFrame(frame) != 0u)
    {
        return ES1642_RES_ERR_EXCEPTION;
    }

    if (frame->cmd != ES1642_CMD_REPORT_SEARCH_RESULT)
    {
        return ES1642_RES_ERR_CMD;
    }

    if (frame->len < 9u)
    {
        return ES1642_RES_ERR_LENGTH;
    }

    dev_ctrl = es1642_read_le16(&frame->data[6]);
    attribute_len = frame->data[8];

    if (frame->len != (uint16_t)(9u + attribute_len))
    {
        return ES1642_RES_ERR_LENGTH;
    }

    memcpy(ind->dev_addr, &frame->data[0], ES1642_ADDR_LEN);
    ind->dev_ctrl_raw = dev_ctrl;
    ind->net_state = (uint8_t)((dev_ctrl >> 4) & 0x0Fu);
    ind->attribute_len = attribute_len;
    ind->attribute = &frame->data[9];

    return ES1642_RES_OK;
}

/**
 * @brief 解析搜索通知
 * @param frame 帧指针
 * @param notify_info 输出参数，搜索通知信息
 * @return 操作结果
 */
int32_t ES1642_ParseSearchNotify(const ES1642_Frame *frame, ES1642_SearchNotify *notify_info)
{
    uint16_t data_ctrl;
    uint8_t attribute_len;

    if ((frame == NULL) || (notify_info == NULL))
    {
        return ES1642_RES_ERR_PARAM;
    }

    if (ES1642_IsExceptionFrame(frame) != 0u)
    {
        return ES1642_RES_ERR_EXCEPTION;
    }

    if (frame->cmd != ES1642_CMD_NOTIFY_SEARCH)
    {
        return ES1642_RES_ERR_CMD;
    }

    if (frame->len < 10u)
    {
        return ES1642_RES_ERR_LENGTH;
    }

    data_ctrl = es1642_read_le16(&frame->data[0]);
    attribute_len = frame->data[9];

    if (frame->len != (uint16_t)(10u + attribute_len))
    {
        return ES1642_RES_ERR_LENGTH;
    }

    notify_info->data_ctrl_raw = data_ctrl;
    memcpy(notify_info->src_addr, &frame->data[2], ES1642_ADDR_LEN);
    notify_info->task_id = frame->data[8];
    notify_info->attribute_len = attribute_len;
    notify_info->attribute = &frame->data[10];

    return ES1642_RES_OK;
}

/**
 * @brief 解析PSK通知
 * @param frame 帧指针
 * @param notify_info 输出参数，PSK通知信息
 * @return 操作结果
 */
int32_t ES1642_ParsePskNotify(const ES1642_Frame *frame, ES1642_PskNotify *notify_info)
{
    uint16_t data_ctrl;

    if ((frame == NULL) || (notify_info == NULL))
    {
        return ES1642_RES_ERR_PARAM;
    }

    if (ES1642_IsExceptionFrame(frame) != 0u)
    {
        return ES1642_RES_ERR_EXCEPTION;
    }

    if (frame->cmd != ES1642_CMD_NOTIFY_SET_PSK)
    {
        return ES1642_RES_ERR_CMD;
    }

    if (frame->len != 8u)
    {
        return ES1642_RES_ERR_LENGTH;
    }

    data_ctrl = es1642_read_le16(&frame->data[0]);
    notify_info->data_ctrl_raw = data_ctrl;
    memcpy(notify_info->src_addr, &frame->data[2], ES1642_ADDR_LEN);
    notify_info->op = (uint8_t)((data_ctrl >> 12) & 0x03u);

    return ES1642_RES_OK;
}

/**
 * @brief 解析PSK结果指示
 * @param frame 帧指针
 * @param ind 输出参数，PSK结果指示
 * @return 操作结果
 */
int32_t ES1642_ParsePskResultInd(const ES1642_Frame *frame, ES1642_PskResultInd *ind)
{
    if ((frame == NULL) || (ind == NULL))
    {
        return ES1642_RES_ERR_PARAM;
    }

    if (ES1642_IsExceptionFrame(frame) != 0u)
    {
        return ES1642_RES_ERR_EXCEPTION;
    }

    if (frame->cmd != ES1642_CMD_REPORT_SET_PSK_RESULT)
    {
        return ES1642_RES_ERR_CMD;
    }

    if (frame->len != 9u)
    {
        return ES1642_RES_ERR_LENGTH;
    }

    ind->data_ctrl_raw = es1642_read_le16(&frame->data[0]);
    memcpy(ind->src_addr, &frame->data[2], ES1642_ADDR_LEN);
    ind->state = frame->data[8];

    return ES1642_RES_OK;
}

/**
 * @brief 解析远程版本响应
 * @param frame 帧指针
 * @param src_addr 输出参数，源地址
 * @param info 输出参数，版本信息
 * @return 操作结果
 */
int32_t ES1642_ParseRemoteVersionRsp(const ES1642_Frame *frame,
                                     uint8_t src_addr[ES1642_ADDR_LEN],
                                     ES1642_VersionInfo *info)
{
    const uint8_t *payload;
    int32_t ret;

    if (info == NULL)
    {
        return ES1642_RES_ERR_PARAM;
    }

    ret = es1642_parse_remote_common(frame,
                                     ES1642_CMD_REMOTE_READ_VERSION,
                                     7u,
                                     src_addr,
                                     &payload);
    if (ret != ES1642_RES_OK)
    {
        return ret;
    }

    info->vendor_id = es1642_read_le16(&payload[0]);
    info->chip_type = es1642_read_le16(&payload[2]);
    info->product_info = payload[4];
    info->version_bcd = es1642_read_le16(&payload[5]);

    return ES1642_RES_OK;
}

/**
 * @brief 解析远程MAC地址响应
 * @param frame 帧指针
 * @param src_addr 输出参数，源地址
 * @param mac 输出参数，MAC地址
 * @return 操作结果
 */
int32_t ES1642_ParseRemoteReadMacRsp(const ES1642_Frame *frame,
                                     uint8_t src_addr[ES1642_ADDR_LEN],
                                     uint8_t mac[ES1642_ADDR_LEN])
{
    const uint8_t *payload;
    int32_t ret;

    if ((src_addr == NULL) || (mac == NULL))
    {
        return ES1642_RES_ERR_PARAM;
    }

    ret = es1642_parse_remote_common(frame,
                                     ES1642_CMD_REMOTE_READ_MAC,
                                     ES1642_ADDR_LEN,
                                     src_addr,
                                     &payload);
    if (ret != ES1642_RES_OK)
    {
        return ret;
    }

    memcpy(mac, payload, ES1642_ADDR_LEN);
    return ES1642_RES_OK;
}

/**
 * @brief 解析远程网络参数响应
 * @param frame 帧指针
 * @param src_addr 输出参数，源地址
 * @param info 输出参数，网络参数信息
 * @return 操作结果
 */
int32_t ES1642_ParseRemoteReadNetParamRsp(const ES1642_Frame *frame,
                                          uint8_t src_addr[ES1642_ADDR_LEN],
                                          ES1642_NetParamInfo *info)
{
    const uint8_t *payload;
    int32_t ret;

    if ((src_addr == NULL) || (info == NULL))
    {
        return ES1642_RES_ERR_PARAM;
    }

    ret = es1642_parse_remote_common(frame,
                                     ES1642_CMD_REMOTE_READ_NET_PARAM,
                                     7u,
                                     src_addr,
                                     &payload);
    if (ret != ES1642_RES_OK)
    {
        return ret;
    }

    info->relay_depth = payload[0];
    memcpy(info->network_psk, &payload[1], 4u);
    info->rsv0 = payload[5];
    info->rsv1 = payload[6];

    return ES1642_RES_OK;
}

