
/**
 * @file    es1642.c
 * @brief   ES1642 模块底层通信驱动实现
 * @author  OpenAI
 * @date    2026-03-13
 * 
 * 本文件实现了ES1642无线模块的协议层驱动，包括：
 * 1. 数据的序列化和反序列化（大小端转换）
 * 2. 校验和计算（CSUM和CXOR）
 * 3. 帧的封装和解析
 * 4. 流式接收处理
 * 5. 各类命令的发送和接收处理
 * 
 * 使用说明：
 * - 本驱动只负责协议层，不直接操作硬件
 * - 硬件操作通过回调函数由用户实现
 * - 支持阻塞、DMA、中断等多种传输方式
 */

#include "es1642.h"
#include <stdio.h>
#include <string.h>

/* ========================= 内部工具函数 ========================= */

/**
 * @brief  将16位数值以小端序写入缓冲区
 * @param  buf: 目标缓冲区指针
 * @param  value: 要写入的16位数值
 * @retval None
 * @note   小端序：低字节在前，高字节在后
 *         例如：0x1234 -> buf[0]=0x34, buf[1]=0x12
 */
static void es1642_put_le16(uint8_t *buf, uint16_t value)
{
    buf[0] = (uint8_t)(value & 0xFFU);   /* 低字节 */
    buf[1] = (uint8_t)((value >> 8) & 0xFFU);  /* 高字节 */
}

/**
 * @brief  从缓冲区读取16位小端序数值
 * @param  buf: 源缓冲区指针
 * @retval 读取到的16位数值
 * @note   小端序：低字节在前，高字节在后
 *         例如：buf[0]=0x34, buf[1]=0x12 -> 返回0x1234
 */
static uint16_t es1642_get_le16(const uint8_t *buf)
{
    return (uint16_t)((uint16_t)buf[0] | ((uint16_t)buf[1] << 8));
}

/**
 * @brief  从缓冲区读取24位小端序数值
 * @param  buf: 源缓冲区指针
 * @retval 读取到的24位数值
 * @note   小端序：最低字节在前，最高字节在后
 *         例如：buf[0]=0x34, buf[1]=0x12, buf[2]=0x01 -> 返回0x011234
 */
static uint32_t es1642_get_le24(const uint8_t *buf)
{
    return ((uint32_t)buf[0]) |          /* 字节0：最低8位 */
           ((uint32_t)buf[1] << 8) |     /* 字节1：中间8位 */
           ((uint32_t)buf[2] << 16);     /* 字节2：最高8位 */
}

/**
 * @brief  计算校验和（CSUM和CXOR）
 * @param  buf: 数据缓冲区指针
 * @param  len: 数据长度
 * @param  csum: 输出参数，CSUM校验和（所有字节相加）
 * @param  cxor: 输出参数，CXOR校验和（所有字节异或）
 * @retval None
 * @note   CSUM: 累加和校验，用于检测数据传输错误
 *         CXOR: 异或校验，用于增强数据完整性检查
 */
static void es1642_calc_checksum(const uint8_t *buf, uint16_t len, uint8_t *csum, uint8_t *cxor)
{
    uint16_t i;
    uint8_t sum = 0U;  /* 累加和 */
    uint8_t x = 0U;    /* 异或值 */

    for (i = 0U; i < len; ++i)
    {
        sum = (uint8_t)(sum + buf[i]);  /* 累加 */
        x ^= buf[i];                    /* 异或 */
    }

    if (csum != NULL)
    {
        *csum = sum;  /* 输出CSUM */
    }

    if (cxor != NULL)
    {
        *cxor = x;  /* 输出CXOR */
    }
}

/**
 * @brief  将9位有符号数进行符号扩展
 * @param  value: 9位数值（低9位有效）
 * @retval 符号扩展后的16位有符号数
 * @note   用于处理RSSI信号强度等9位有符号数据
 *         例如：0x1FF (-1) 扩展后为 0xFFFF (-1)
 */
static int16_t es1642_sign_extend_9bit(uint16_t value)
{
    value &= 0x01FFU;  /* 只保留低9位 */

    /* 如果第9位是1（负数），进行符号扩展 */
    if ((value & 0x0100U) != 0U)
    {
        value |= 0xFE00U;  /* 高位置1，扩展为负数 */
    }

    return (int16_t)value;
}

/**
 * @brief  检查帧是否为正常命令帧
 * @param  frame: 帧结构体指针
 * @param  expect_cmd: 期望的指令字
 * @retval 状态码
 * @note   检查内容包括：
 *         1. 帧指针是否有效
 *         2. 指令字是否匹配
 *         3. 是否不是异常帧
 */
static es1642_status_t es1642_expect_normal_cmd(const es1642_frame_t *frame, uint8_t expect_cmd)
{
    if (frame == NULL)
    {
        return ES1642_STATUS_ERROR_PARAM;
    }

    if (frame->cmd != expect_cmd)
    {
        return ES1642_STATUS_ERROR_CMD_MISMATCH;  /* 指令字不匹配 */
    }

    if (frame->is_exception)
    {
        return ES1642_STATUS_ERROR_FRAME_IS_EXCEPTION;  /* 是异常帧 */
    }

    return ES1642_STATUS_OK;
}

/**
 * @brief  从字节数组解析版本信息
 * @param  data: 原始数据指针
 * @param  version: 输出参数，版本信息结构体
 * @retval None
 * @note   数据格式：
 *         [0-1] 厂商标识 (2字节, 小端)
 *         [2-3] 芯片类型 (2字节, 小端)
 *         [4]   产品信息 (1字节)
 *         [5-6] 版本号 (2字节, 小端, BCD格式)
 */
static void es1642_parse_version_bytes(const uint8_t *data, es1642_version_t *version)
{
    version->vendor_id = es1642_get_le16(&data[0]);    /* 厂商标识 */
    version->chip_type = es1642_get_le16(&data[2]);    /* 芯片类型 */
    version->product_info = data[4];                   /* 产品信息 */
    version->version_bcd = es1642_get_le16(&data[5]);  /* 版本号 */
}

/**
 * @brief  远程命令的通用发送函数
 * @param  handle: ES1642驱动句柄
 * @param  cmd: 指令字
 * @param  dst_addr: 目标设备地址（6字节）
 * @retval 状态码
 * @note   用于发送远程读取命令（远程读取版本、MAC、网络参数）
 *         数据格式：DataCtrl(1字节) + DstAddr(6字节)
 */
static es1642_status_t es1642_send_remote_common(es1642_handle_t *handle,
                                                 uint8_t cmd,
                                                 const uint8_t dst_addr[ES1642_ADDR_LEN])
{
    uint8_t payload[1U + ES1642_ADDR_LEN];  /* 数据载荷缓冲区 */

    if (dst_addr == NULL)
    {
        return ES1642_STATUS_ERROR_PARAM;
    }

    payload[0] = 0x00U; /* 远程调试命令 Data Ctrl 固定为 00H */
    (void)memcpy(&payload[1], dst_addr, ES1642_ADDR_LEN);  /* 拷贝目标地址 */

    return ES1642_SendFrame(handle,
                            ES1642_MakeDeviceRequestCtrl(),
                            cmd,
                            payload,
                            (uint16_t)sizeof(payload));
}

/* ========================= 对外基础接口 ========================= */
/**
 * @brief  生成设备请求控制字节
 * @retval 控制字节值（默认为0x58）
 * @note   用于发起命令请求
 */
uint8_t ES1642_MakeDeviceRequestCtrl(void)
{
    return ES1642_CTRL_DEVICE_REQUEST;
}

/**
 * @brief  生成设备应答控制字节
 * @retval 控制字节值（默认为0x18）
 * @note   用于回复命令请求
 */
uint8_t ES1642_MakeDeviceReplyCtrl(void)
{
    return ES1642_CTRL_DEVICE_REPLY;
}

/**
 * @brief  生成设备异常应答控制字节
 * @retval 控制字节值（默认为0xB8）
 * @note   用于返回异常应答，设置了RESPOND位
 */
uint8_t ES1642_MakeDeviceExceptionCtrl(void)
{
    return (uint8_t)(ES1642_CTRL_DEVICE_REPLY | ES1642_CTRL_BIT_RESPOND);
}

/**
 * @brief  生成发送数据的控制字节
 * @param  prm: 是否为请求模式（true=请求，false=应答）
 * @retval 控制字节值
 * @note   根据prm参数返回请求或应答控制字节
 */
uint8_t ES1642_MakeSendDataCtrlByte(bool prm)
{
    return prm ? ES1642_MakeDeviceRequestCtrl() : ES1642_MakeDeviceReplyCtrl();
}

/**
 * @brief  生成发送数据的DataCtrl字段
 * @param  relay_depth: 中继深度（0-15，0表示自动）
 * @retval DataCtrl字段值（16位）
 * @note   中继深度位于高4位（bit12-15）
 *         0表示不指定跳数，由模块自动决定
 */
uint16_t ES1642_MakeTxDataCtrl(uint8_t relay_depth)
{
    /* 0 表示不指定跳数，由模块自动决定 */
    relay_depth &= 0x0FU;  /* 限制在0-15范围内 */
    return (uint16_t)((uint16_t)relay_depth << 12);  /* 移到高4位 */
}

/**
 * @brief  生成搜索命令的DataCtrl字段
 * @param  depth: 搜索深度（0-15，0自动转为15）
 * @param  rule: 搜索规则
 * @retval DataCtrl字段值（16位）
 * @note   数据格式：
 *         bit15-12: 搜索规则（0-7）
 *         bit11-8:  搜索深度（0-15）
 *         bit7-0:   保留
 */
uint16_t ES1642_MakeSearchCtrl(uint8_t depth, es1642_search_rule_t rule)
{
    /* 协议规定：深度 0 按 15 处理 */
    if (depth == 0U)
    {
        depth = 15U;  /* 0表示最大深度 */
    }

    depth &= 0x0FU;  /* 限制在0-15范围内 */

    return (uint16_t)(((uint16_t)depth << 8) | (((uint16_t)rule & 0x07U) << 12));
}

/**
 * @brief  生成搜索应答的DataCtrl字段
 * @param  participate: 是否参与搜索
 * @retval DataCtrl字段值（16位）
 * @note   participate=true时返回0x0100，否则返回0x0000
 */
uint16_t ES1642_MakeSearchReplyCtrl(bool participate)
{
    return participate ? 0x0100U : 0x0000U;
}

/**
 * @brief  判断地址是否为广播地址
 * @param  addr: 地址数组（6字节）
 * @retval true=是广播地址，false=不是广播地址
 * @note   广播地址为全0xFF（FF:FF:FF:FF:FF:FF）
 */
bool ES1642_IsBroadcastAddr(const uint8_t addr[ES1642_ADDR_LEN])
{
    uint8_t i;

    if (addr == NULL)
    {
        return false;
    }

    /* 检查每个字节是否为0xFF */
    for (i = 0U; i < ES1642_ADDR_LEN; ++i)
    {
        if (addr[i] != 0xFFU)
        {
            return false;  /* 只要有一个字节不是0xFF，就不是广播地址 */
        }
    }

    return true;
}

/**
 * @brief  设置地址为广播地址
 * @param  addr: 地址数组（6字节）
 * @retval None
 * @note   将地址设置为全0xFF
 */
void ES1642_SetBroadcastAddr(uint8_t addr[ES1642_ADDR_LEN])
{
    uint8_t i;

    if (addr == NULL)
    {
        return;
    }

    /* 将每个字节设置为0xFF */
    for (i = 0U; i < ES1642_ADDR_LEN; ++i)
    {
        addr[i] = 0xFFU;
    }
}

/**
 * @brief  拷贝地址
 * @param  dst: 目标地址数组（6字节）
 * @param  src: 源地址数组（6字节）
 * @retval None
 * @note   使用memcpy实现6字节地址拷贝
 */
void ES1642_CopyAddr(uint8_t dst[ES1642_ADDR_LEN], const uint8_t src[ES1642_ADDR_LEN])
{
    if ((dst == NULL) || (src == NULL))
    {
        return;
    }

    (void)memcpy(dst, src, ES1642_ADDR_LEN);  /* 拷贝6字节 */
}

/**
 * @brief  获取状态码的字符串描述
 * @param  status: 状态码
 * @retval 状态字符串指针
 * @note   用于调试和错误信息打印
 */
const char *ES1642_StatusString(es1642_status_t status)
{
    switch (status)
    {
        case ES1642_STATUS_OK: return "成功\r\n";
        case ES1642_STATUS_IN_PROGRESS: return "接收中\r\n";
        case ES1642_STATUS_FRAME_READY: return "收到完整帧\r\n";
        case ES1642_STATUS_ERROR_PARAM: return "参数错误\r\n";
        case ES1642_STATUS_ERROR_NO_TX_PORT: return "未注册发送回调\r\n";
        case ES1642_STATUS_ERROR_BUFFER_TOO_SMALL: return "缓存空间不足\r\n";
        case ES1642_STATUS_ERROR_BAD_HEAD: return "帧头错误\r\n";
        case ES1642_STATUS_ERROR_BAD_LENGTH: return "长度错误\r\n";
        case ES1642_STATUS_ERROR_DATA_TOO_LONG: return "数据长度过长\r\n";
        case ES1642_STATUS_ERROR_CHECKSUM: return "校验错误\r\n";
        case ES1642_STATUS_ERROR_CMD_MISMATCH: return "指令字不匹配\r\n";
        case ES1642_STATUS_ERROR_FRAME_IS_EXCEPTION: return "异常应答帧\r\n";
        case ES1642_STATUS_ERROR_NOT_EXCEPTION_FRAME: return "不是异常应答帧\r\n";
        case ES1642_STATUS_ERROR_PAYLOAD_LENGTH: return "Data 长度与协议不符\r\n";
        case ES1642_STATUS_ERROR_TX_FAIL: return "发送失败\r\n";
        default: return "未知状态\r\n";
    }
}

/**
 * @brief  获取异常码的字符串描述
 * @param  exception_code: 异常码
 * @retval 异常字符串指针
 * @note   用于调试和错误信息打印
 */
const char *ES1642_ExceptionString(uint8_t exception_code)
{
    switch (exception_code)
    {
        case ES1642_EXCEPTION_BAD_FORMAT: return "错误的格式\r\n";
        case ES1642_EXCEPTION_BAD_DATA_UNIT: return "错误的数据单元\r\n";
        case ES1642_EXCEPTION_BAD_LENGTH: return "错误的长度\r\n";
        case ES1642_EXCEPTION_INVALID_CMD: return "指令字无效\r\n";
        case ES1642_EXCEPTION_NO_RAM: return "RAM 空间不足\r\n";
        case ES1642_EXCEPTION_BAD_STATE: return "错误的状态\r\n";
        default: return "保留/未知异常码\r\n";
    }
}

/* ========================= 帧封装 / 发送 ========================= */

/**
 * @brief  构建完整的协议帧
 * @param  ctrl: 控制字节
 * @param  cmd: 指令字
 * @param  data: 数据载荷指针（可为NULL）
 * @param  data_len: 数据载荷长度
 * @param  out_frame: 输出帧缓冲区
 * @param  out_size: 输出缓冲区大小
 * @param  out_frame_len: 输出参数，实际帧长度
 * @retval 状态码
 * @note   帧格式：
 *         [0]   帧头 (0x79)
 *         [1-2] 数据长度 (2字节, 小端)
 *         [3]   控制字节
 *         [4]   指令字
 *         [5...] 数据载荷 (data_len字节)
 *         [N-1] CSUM校验和
 *         [N]   CXOR校验和
 */
es1642_status_t ES1642_BuildFrame(uint8_t ctrl,
                                  uint8_t cmd,
                                  const uint8_t *data,
                                  uint16_t data_len,
                                  uint8_t *out_frame,
                                  uint16_t out_size,
                                  uint16_t *out_frame_len)
{
    uint8_t csum;
    uint8_t cxor;
    uint16_t total_len;

    if ((out_frame == NULL) || (out_frame_len == NULL))
    {
        return ES1642_STATUS_ERROR_PARAM;
    }

    if ((data_len > 0U) && (data == NULL))
    {
        return ES1642_STATUS_ERROR_PARAM;
    }

    if (data_len > ES1642_MAX_DATA_LEN)
    {
        return ES1642_STATUS_ERROR_DATA_TOO_LONG;
    }

    total_len = (uint16_t)(ES1642_MIN_FRAME_LEN + data_len);

    if (out_size < total_len)
    {
        return ES1642_STATUS_ERROR_BUFFER_TOO_SMALL;
    }

    out_frame[0] = ES1642_FRAME_HEAD;
    es1642_put_le16(&out_frame[1], data_len);
    out_frame[3] = ctrl;
    out_frame[4] = cmd;

    if (data_len > 0U)
    {
        (void)memcpy(&out_frame[5], data, data_len);
    }

    es1642_calc_checksum(&out_frame[1], (uint16_t)(data_len + 4U), &csum, &cxor);

    out_frame[(uint16_t)(5U + data_len)] = csum;
    out_frame[(uint16_t)(6U + data_len)] = cxor;
    *out_frame_len = total_len;

    return ES1642_STATUS_OK;
}

/**
 * @brief  发送协议帧
 * @param  handle: ES1642驱动句柄
 * @param  ctrl: 控制字节
 * @param  cmd: 指令字
 * @param  data: 数据载荷指针（可为NULL）
 * @param  data_len: 数据载荷长度
 * @retval 状态码
 * @note   工作流程：
 *         1. 调用ES1642_BuildFrame构建完整帧
 *         2. 通过回调函数将帧发送出去
 *         3. 支持阻塞、DMA、中断等多种发送方式
 */
es1642_status_t ES1642_SendFrame(es1642_handle_t *handle,
                                 uint8_t ctrl,
                                 uint8_t cmd,
                                 const uint8_t *data,
                                 uint16_t data_len)
{
    uint8_t frame_buf[ES1642_MAX_FRAME_LEN];  /* 帧缓冲区 */
    uint16_t frame_len = 0U;                   /* 实际帧长度 */
    int32_t send_len;                          /* 发送的字节数 */
    es1642_status_t status;

    if (handle == NULL)
    {
        return ES1642_STATUS_ERROR_PARAM;
    }

    /* 检查是否注册了发送回调 */
    if (handle->write == NULL)
    {
        return ES1642_STATUS_ERROR_NO_TX_PORT;
    }

    /* 构建协议帧 */
    status = ES1642_BuildFrame(ctrl,
                               cmd,
                               data,
                               data_len,
                               frame_buf,
                               (uint16_t)sizeof(frame_buf),
                               &frame_len);
    if (status != ES1642_STATUS_OK)
    {
        return status;
    }
    /* 通过回调函数发送数据 */
    send_len = handle->write(frame_buf, frame_len, handle->user_arg);

    /* 检查是否全部发送成功 */
    if (send_len != (int32_t)frame_len)
    {
        return ES1642_STATUS_ERROR_TX_FAIL;
    }

    return ES1642_STATUS_OK;
}

/* ========================= 帧解析 / 流式接收 ========================= */

/**
 * @brief  解析完整的协议帧
 * @param  raw_frame: 原始帧数据指针
 * @param  frame_len: 帧长度
 * @param  frame: 输出参数，解析后的帧结构体
 * @retval 状态码
 * @note   解析步骤：
 *         1. 检查帧头是否为0x79
 *         2. 读取数据长度
 *         3. 检查帧长度是否匹配
 *         4. 计算并验证校验和（CSUM和CXOR）
 *         5. 填充帧结构体各个字段
 */
es1642_status_t ES1642_ParseFrame(const uint8_t *raw_frame,
                                  uint16_t frame_len,
                                  es1642_frame_t *frame)
{
    uint16_t data_len;
    uint8_t csum;
    uint8_t cxor;

    if ((raw_frame == NULL) || (frame == NULL))
    {
        return ES1642_STATUS_ERROR_PARAM;
    }

    if (frame_len < ES1642_MIN_FRAME_LEN)
    {
        return ES1642_STATUS_ERROR_BAD_LENGTH;
    }

    if (raw_frame[0] != ES1642_FRAME_HEAD)
    {
        return ES1642_STATUS_ERROR_BAD_HEAD;
    }

    data_len = es1642_get_le16(&raw_frame[1]);

    if (data_len > ES1642_MAX_DATA_LEN)
    {
        return ES1642_STATUS_ERROR_DATA_TOO_LONG;
    }

    if (frame_len != (uint16_t)(ES1642_MIN_FRAME_LEN + data_len))
    {
        return ES1642_STATUS_ERROR_BAD_LENGTH;
    }

    es1642_calc_checksum(&raw_frame[1], (uint16_t)(data_len + 4U), &csum, &cxor);

    if ((csum != raw_frame[(uint16_t)(5U + data_len)]) ||
        (cxor != raw_frame[(uint16_t)(6U + data_len)]))
    {
        return ES1642_STATUS_ERROR_CHECKSUM;
    }

    frame->header = raw_frame[0];
    frame->data_len = data_len;
    frame->ctrl = raw_frame[3];
    frame->cmd = raw_frame[4];
    frame->data = (data_len > 0U) ? &raw_frame[5] : NULL;
    frame->csum = raw_frame[(uint16_t)(5U + data_len)];
    frame->cxor = raw_frame[(uint16_t)(6U + data_len)];
    frame->prm = ((raw_frame[3] & ES1642_CTRL_BIT_PRM) != 0U);
    frame->is_exception = ((raw_frame[3] & ES1642_CTRL_BIT_RESPOND) != 0U);
    frame->exception_code = (frame->is_exception && (data_len > 0U))
                          ? raw_frame[(uint16_t)(4U + data_len)]
                          : 0xFFU;

    return ES1642_STATUS_OK;
}

/**
 * @brief  处理一帧完整的原始帧数据（适用于 DMA/消息缓冲接收一次性得到完整帧的场景）
 * @param  handle: ES1642 驱动句柄指针
 * @param  frame_buf: 指向完整原始帧的缓冲区（从帧头开始）
 * @param  frame_len: 缓冲区长度（应为完整帧长度）
 * @retval ES1642_STATUS_FRAME_READY: 成功解析并触发 on_frame 回调
 * @retval ES1642_STATUS_ERROR_PARAM: 参数错误
 * @retval ES1642_STATUS_ERROR_BAD_HEAD: 帧头错误
 * @retval ES1642_STATUS_ERROR_BAD_LENGTH: 长度字段与实际长度不匹配
 * @retval ES1642_STATUS_ERROR_DATA_TOO_LONG: 声明的数据长度超出最大值
 * @retval ES1642_STATUS_ERROR_CHECKSUM: 校验失败
 * @retval 其它错误码: 解析过程中的其它错误
 * @note   该函数等价于对一个完整帧调用 ES1642_ParseFrame，然后根据解析结果
 *         触发 `handle->port.on_frame` 或 `handle->port.on_error` 回调。
 */
es1642_status_t ES1642_ProcessCompleteFrame(es1642_handle_t *handle,
                                            const uint8_t *frame_buf,
                                            uint16_t frame_len)
{
    es1642_frame_t frame;
    es1642_status_t status;

    if ((handle == NULL) || (frame_buf == NULL))
    {
        return ES1642_STATUS_ERROR_PARAM;
    }

    /* 解析传入的完整帧 */
    status = ES1642_ParseFrame(frame_buf, frame_len, &frame);

    if (status == ES1642_STATUS_OK)
    {
        if (handle->on_frame != NULL)
        {
            handle->on_frame(handle, &frame, handle->user_arg);
        }

        return ES1642_STATUS_FRAME_READY;
    }

    /* 解析失败时通知错误回调（如果有）并返回错误码 */
    if (handle->on_error != NULL)
    {
        handle->on_error(handle, status, handle->user_arg);
    }

    /* 不改变 RX 状态，调用者可根据需要决定是否重置 */
    return status;
}

/* ========================= 命令发送接口 ========================= */

/**
 * @brief  发送重启命令
 * @param  handle: ES1642 驱动句柄指针
 * @retval ES1642_STATUS_OK: 发送成功
 * @retval ES1642_STATUS_ERROR_PARAM: 参数错误（handle 为 NULL）
 * @retval ES1642_STATUS_ERROR_NO_TX_PORT: 未注册发送回调
 * @retval ES1642_STATUS_ERROR_TX_FAIL: 发送失败
 * @note   该函数构建一个空数据载荷的重启请求帧并通过注册的写回调发送
 */
es1642_status_t ES1642_SendReboot(es1642_handle_t *handle)
{
    return ES1642_SendFrame(handle,
                            ES1642_MakeDeviceRequestCtrl(),
                            ES1642_CMD_REBOOT,
                            NULL,
                            0U);
}

/**
 * @brief  请求读取模块版本信息
 * @param  handle: ES1642 驱动句柄指针
 * @retval ES1642_STATUS_OK: 发送成功
 * @retval ES1642_STATUS_ERROR_PARAM: 参数错误
 * @retval ES1642_STATUS_ERROR_NO_TX_PORT: 未注册发送回调
 * @retval ES1642_STATUS_ERROR_TX_FAIL: 发送失败
 */
es1642_status_t ES1642_SendReadVersion(es1642_handle_t *handle)
{
    return ES1642_SendFrame(handle,
                            ES1642_MakeDeviceRequestCtrl(),
                            ES1642_CMD_READ_VERSION,
                            NULL,
                            0U);
}

/**
 * @brief  请求读取本地模块 MAC 地址
 * @param  handle: ES1642 驱动句柄指针
 * @retval ES1642_STATUS_OK: 发送成功
 * @retval ES1642_STATUS_ERROR_PARAM: 参数错误
 * @retval ES1642_STATUS_ERROR_NO_TX_PORT: 未注册发送回调
 * @retval ES1642_STATUS_ERROR_TX_FAIL: 发送失败
 */
es1642_status_t ES1642_SendReadMac(es1642_handle_t *handle)
{
    return ES1642_SendFrame(handle,
                            ES1642_MakeDeviceRequestCtrl(),
                            ES1642_CMD_READ_MAC,
                            NULL,
                            0U);
}

/**
 * @brief  请求读取模块自身地址（6 字节）
 * @param  handle: ES1642 驱动句柄指针
 * @retval ES1642_STATUS_OK: 发送成功
 * @retval ES1642_STATUS_ERROR_PARAM: 参数错误
 * @retval ES1642_STATUS_ERROR_NO_TX_PORT: 未注册发送回调
 * @retval ES1642_STATUS_ERROR_TX_FAIL: 发送失败
 */
es1642_status_t ES1642_SendReadAddr(es1642_handle_t *handle)
{
    return ES1642_SendFrame(handle,
                            ES1642_MakeDeviceRequestCtrl(),
                            ES1642_CMD_READ_ADDR,
                            NULL,
                            0U);
}

/**
 * @brief  设置模块地址
 * @param  handle: ES1642 驱动句柄指针
 * @param  addr: 要设置的地址，数组长度为 ES1642_ADDR_LEN（6）
 * @retval ES1642_STATUS_OK: 发送成功
 * @retval ES1642_STATUS_ERROR_PARAM: 参数错误（handle 或 addr 为 NULL）
 * @retval ES1642_STATUS_ERROR_NO_TX_PORT: 未注册发送回调
 * @retval ES1642_STATUS_ERROR_TX_FAIL: 发送失败
 */
es1642_status_t ES1642_SendSetAddr(es1642_handle_t *handle,
                                   const uint8_t addr[ES1642_ADDR_LEN])
{
    if (addr == NULL)
    {
        return ES1642_STATUS_ERROR_PARAM;
    }

    return ES1642_SendFrame(handle,
                            ES1642_MakeDeviceRequestCtrl(),
                            ES1642_CMD_SET_ADDR,
                            addr,
                            ES1642_ADDR_LEN);
}

/**
 * @brief  请求读取网络参数（中继深度、网络密钥等）
 * @param  handle: ES1642 驱动句柄指针
 * @retval ES1642_STATUS_OK: 发送成功
 * @retval ES1642_STATUS_ERROR_PARAM: 参数错误
 * @retval ES1642_STATUS_ERROR_NO_TX_PORT: 未注册发送回调
 * @retval ES1642_STATUS_ERROR_TX_FAIL: 发送失败
 */
es1642_status_t ES1642_SendReadNetParam(es1642_handle_t *handle)
{
    return ES1642_SendFrame(handle,
                            ES1642_MakeDeviceRequestCtrl(),
                            ES1642_CMD_READ_NET_PARAM,
                            NULL,
                            0U);
}

/**
 * @brief  设置网络参数（当前仅支持设置中继深度）
 * @param  handle: ES1642 驱动句柄指针
 * @param  relay_depth: 中继深度（0-255，协议中通常只使用低位）
 * @retval ES1642_STATUS_OK: 发送成功
 * @retval ES1642_STATUS_ERROR_PARAM: 参数错误
 * @retval ES1642_STATUS_ERROR_NO_TX_PORT: 未注册发送回调
 * @retval ES1642_STATUS_ERROR_TX_FAIL: 发送失败
 */
es1642_status_t ES1642_SendSetNetParam(es1642_handle_t *handle,
                                       uint8_t relay_depth)
{
    uint8_t payload[2];

    payload[0] = relay_depth;
    payload[1] = 0x00U; /* RSV */

    return ES1642_SendFrame(handle,
                            ES1642_MakeDeviceRequestCtrl(),
                            ES1642_CMD_SET_NET_PARAM,
                            payload,
                            (uint16_t)sizeof(payload));
}

/**
 * @brief  发送用户数据到目标地址
 * @param  handle: ES1642 驱动句柄指针
 * @param  dst_addr: 目标地址（6 字节）
 * @param  user_data: 用户数据指针
 * @param  user_data_len: 用户数据长度
 * @param  relay_depth: 中继深度（0-15）
 * @param  prm: 是否为请求模式（true = 请求，false = 应答）
 * @retval ES1642_STATUS_OK: 发送成功
 * @retval ES1642_STATUS_ERROR_PARAM: 参数错误（例如 dst_addr 为 NULL 或 user_data_len>0 但 user_data 为 NULL）
 * @retval ES1642_STATUS_ERROR_DATA_TOO_LONG: 总载荷超长
 * @retval ES1642_STATUS_ERROR_NO_TX_PORT: 未注册发送回调
 * @retval ES1642_STATUS_ERROR_TX_FAIL: 发送失败
 * @note   载荷格式：DataCtrl(2) + DstAddr(6) + UserLen(2) + UserData
 */
es1642_status_t ES1642_SendData(es1642_handle_t *handle,
                                const uint8_t dst_addr[ES1642_ADDR_LEN],
                                const uint8_t *user_data,
                                uint16_t user_data_len,
                                uint8_t relay_depth,
                                bool prm)
{
    uint8_t payload[ES1642_MAX_DATA_LEN];
    uint16_t data_ctrl;
    uint16_t payload_len;

    if (dst_addr == NULL)
    {
        return ES1642_STATUS_ERROR_PARAM;
    }

    if ((user_data_len > 0U) && (user_data == NULL))
    {
        return ES1642_STATUS_ERROR_PARAM;
    }

    payload_len = (uint16_t)(ES1642_SEND_DATA_FIXED_LEN + user_data_len);

    if (payload_len > ES1642_MAX_DATA_LEN)
    {
        return ES1642_STATUS_ERROR_DATA_TOO_LONG;
    }

    data_ctrl = ES1642_MakeTxDataCtrl(relay_depth);

    es1642_put_le16(&payload[0], data_ctrl);
    (void)memcpy(&payload[2], dst_addr, ES1642_ADDR_LEN);
    es1642_put_le16(&payload[8], user_data_len);

    if (user_data_len > 0U)
    {
        (void)memcpy(&payload[10], user_data, user_data_len);
    }

    return ES1642_SendFrame(handle,
                            ES1642_MakeSendDataCtrlByte(prm),
                            ES1642_CMD_SEND_DATA,
                            payload,
                            payload_len);
}

/**
 * @brief  发送开始搜索命令
 * @param  handle: ES1642 驱动句柄指针
 * @param  depth: 搜索深度（0 表示自动，协议中 0 表示 15）
 * @param  rule: 搜索规则（枚举类型）
 * @param  attribute: 搜索属性指针（可选）
 * @param  attribute_len: 属性长度
 * @retval ES1642_STATUS_OK: 发送成功
 * @retval ES1642_STATUS_ERROR_PARAM: 参数错误
 * @retval ES1642_STATUS_ERROR_DATA_TOO_LONG: 载荷过长
 */
es1642_status_t ES1642_SendStartSearch(es1642_handle_t *handle,
                                       uint8_t depth,
                                       es1642_search_rule_t rule,
                                       const uint8_t *attribute,
                                       uint8_t attribute_len)
{
    uint8_t payload[ES1642_MAX_DATA_LEN];
    uint16_t data_ctrl;
    uint16_t payload_len;

    if ((attribute_len > 0U) && (attribute == NULL))
    {
        return ES1642_STATUS_ERROR_PARAM;
    }

    payload_len = (uint16_t)(3U + attribute_len);

    if (payload_len > ES1642_MAX_DATA_LEN)
    {
        return ES1642_STATUS_ERROR_DATA_TOO_LONG;
    }

    data_ctrl = ES1642_MakeSearchCtrl(depth, rule);

    es1642_put_le16(&payload[0], data_ctrl);
    payload[2] = attribute_len;

    if (attribute_len > 0U)
    {
        (void)memcpy(&payload[3], attribute, attribute_len);
    }

    return ES1642_SendFrame(handle,
                            ES1642_MakeDeviceRequestCtrl(),
                            ES1642_CMD_START_SEARCH,
                            payload,
                            payload_len);
}

/**
 * @brief  发送停止搜索命令
 * @param  handle: ES1642 驱动句柄指针
 * @retval ES1642_STATUS_OK: 发送成功
 * @retval ES1642_STATUS_ERROR_PARAM: 参数错误
 */
es1642_status_t ES1642_SendStopSearch(es1642_handle_t *handle)
{
    return ES1642_SendFrame(handle,
                            ES1642_MakeDeviceRequestCtrl(),
                            ES1642_CMD_STOP_SEARCH,
                            NULL,
                            0U);
}

/**
 * @brief  发送搜索应答（回复发起搜索的设备）
 * @param  handle: ES1642 驱动句柄指针
 * @param  src_addr: 源设备地址（6 字节）——被回复的设备地址
 * @param  task_id: 搜索任务 ID
 * @param  participate: 是否参与（true 参与并可附加 attribute）
 * @param  attribute: 附加属性指针（可选）
 * @param  attribute_len: 属性长度
 * @retval ES1642_STATUS_OK: 发送成功
 * @retval ES1642_STATUS_ERROR_PARAM: 参数错误
 * @retval ES1642_STATUS_ERROR_DATA_TOO_LONG: 载荷过长
 */
es1642_status_t ES1642_SendSearchReply(es1642_handle_t *handle,
                                       const uint8_t src_addr[ES1642_ADDR_LEN],
                                       uint8_t task_id,
                                       bool participate,
                                       const uint8_t *attribute,
                                       uint8_t attribute_len)
{
    uint8_t payload[ES1642_MAX_DATA_LEN];
    uint16_t data_ctrl;
    uint16_t payload_len;

    if (src_addr == NULL)
    {
        return ES1642_STATUS_ERROR_PARAM;
    }

    if (!participate)
    {
        attribute = NULL;
        attribute_len = 0U;
    }
    else if ((attribute_len > 0U) && (attribute == NULL))
    {
        return ES1642_STATUS_ERROR_PARAM;
    }

    payload_len = (uint16_t)(10U + attribute_len);

    if (payload_len > ES1642_MAX_DATA_LEN)
    {
        return ES1642_STATUS_ERROR_DATA_TOO_LONG;
    }

    data_ctrl = ES1642_MakeSearchReplyCtrl(participate);

    es1642_put_le16(&payload[0], data_ctrl);
    (void)memcpy(&payload[2], src_addr, ES1642_ADDR_LEN);
    payload[8] = task_id;
    payload[9] = attribute_len;

    if (attribute_len > 0U)
    {
        (void)memcpy(&payload[10], attribute, attribute_len);
    }

    return ES1642_SendFrame(handle,
                            ES1642_MakeDeviceReplyCtrl(),
                            ES1642_CMD_REPLY_SEARCH,
                            payload,
                            payload_len);
}

/**
 * @brief  设置目标设备的 PSK（预共享密钥）
 * @param  handle: ES1642 驱动句柄指针
 * @param  dst_addr: 目标设备地址（6 字节）
 * @param  new_psk: 新 PSK 指针（可选）
 * @param  new_psk_len: 新 PSK 长度
 * @retval ES1642_STATUS_OK: 发送成功
 * @retval ES1642_STATUS_ERROR_PARAM: 参数错误
 * @retval ES1642_STATUS_ERROR_DATA_TOO_LONG: 载荷过长
 */
es1642_status_t ES1642_SendSetPsk(es1642_handle_t *handle,
                                  const uint8_t dst_addr[ES1642_ADDR_LEN],
                                  const uint8_t *new_psk,
                                  uint8_t new_psk_len)
{
    uint8_t payload[ES1642_MAX_DATA_LEN];
    uint16_t payload_len;

    if (dst_addr == NULL)
    {
        return ES1642_STATUS_ERROR_PARAM;
    }

    if ((new_psk_len > 0U) && (new_psk == NULL))
    {
        return ES1642_STATUS_ERROR_PARAM;
    }

    payload_len = (uint16_t)(10U + new_psk_len);

    if (payload_len > ES1642_MAX_DATA_LEN)
    {
        return ES1642_STATUS_ERROR_DATA_TOO_LONG;
    }

    /* Data Ctrl 固定 0x0044 */
    es1642_put_le16(&payload[0], 0x0044U);
    (void)memcpy(&payload[2], dst_addr, ES1642_ADDR_LEN);
    payload[8] = 0x00U;              /* Old Psk Len 固定为 0 */
    payload[9] = new_psk_len;        /* New Psk Len */

    if (new_psk_len > 0U)
    {
        (void)memcpy(&payload[10], new_psk, new_psk_len);
    }

    return ES1642_SendFrame(handle,
                            ES1642_MakeDeviceRequestCtrl(),
                            ES1642_CMD_SET_PSK,
                            payload,
                            payload_len);
}

/**
 * @brief  发送空应答（仅控制字与指令，无数据）
 * @param  handle: ES1642 驱动句柄指针
 * @param  cmd: 指令字，被应答的命令
 * @retval ES1642_STATUS_OK: 发送成功
 */
es1642_status_t ES1642_SendAckEmpty(es1642_handle_t *handle, uint8_t cmd)
{
    return ES1642_SendFrame(handle,
                            ES1642_MakeDeviceReplyCtrl(),
                            cmd,
                            NULL,
                            0U);
}

/**
 * @brief  发送异常应答帧
 * @param  handle: ES1642 驱动句柄指针
 * @param  cmd: 原始指令字
 * @param  exception_code: 异常码
 * @retval ES1642_STATUS_OK: 发送成功
 */
es1642_status_t ES1642_SendException(es1642_handle_t *handle,
                                     uint8_t cmd,
                                     uint8_t exception_code)
{
    return ES1642_SendFrame(handle,
                            ES1642_MakeDeviceExceptionCtrl(),
                            cmd,
                            &exception_code,
                            1U);
}

/**
 * @brief  发送远程读取版本命令（针对远端设备）
 * @param  handle: ES1642 驱动句柄指针
 * @param  dst_addr: 目标设备地址（6 字节）
 * @retval ES1642_STATUS_OK: 发送成功
 */
es1642_status_t ES1642_SendRemoteReadVersion(es1642_handle_t *handle,
                                             const uint8_t dst_addr[ES1642_ADDR_LEN])
{
    return es1642_send_remote_common(handle, ES1642_CMD_REMOTE_READ_VERSION, dst_addr);
}

/**
 * @brief  发送远程读取 MAC 命令
 * @param  handle: ES1642 驱动句柄指针
 * @param  dst_addr: 目标设备地址（6 字节）
 * @retval ES1642_STATUS_OK: 发送成功
 */
es1642_status_t ES1642_SendRemoteReadMac(es1642_handle_t *handle,
                                         const uint8_t dst_addr[ES1642_ADDR_LEN])
{
    return es1642_send_remote_common(handle, ES1642_CMD_REMOTE_READ_MAC, dst_addr);
}

/**
 * @brief  发送远程读取网络参数命令
 * @param  handle: ES1642 驱动句柄指针
 * @param  dst_addr: 目标设备地址（6 字节）
 * @retval ES1642_STATUS_OK: 发送成功
 */
es1642_status_t ES1642_SendRemoteReadNetParam(es1642_handle_t *handle,
                                              const uint8_t dst_addr[ES1642_ADDR_LEN])
{
    return es1642_send_remote_common(handle, ES1642_CMD_REMOTE_READ_NET_PARAM, dst_addr);
}

/* ========================= 命令解码接口 ========================= */

/**
 * @brief  解码空载荷响应（用于只需确认成功/失败的命令）
 * @param  frame: 解析后的帧指针
 * @param  expect_cmd: 期望的指令字
 * @retval ES1642_STATUS_OK: 命令正常且没有载荷
 * @retval ES1642_STATUS_ERROR_PAYLOAD_LENGTH: 载荷长度不为 0
 * @retval 其它错误码: 参见 es1642_expect_normal_cmd 返回值
 */
es1642_status_t ES1642_DecodeEmptyResponse(const es1642_frame_t *frame, uint8_t expect_cmd)
{
    es1642_status_t status = es1642_expect_normal_cmd(frame, expect_cmd);

    if (status != ES1642_STATUS_OK)
    {
        return status;
    }

    return (frame->data_len == 0U) ? ES1642_STATUS_OK : ES1642_STATUS_ERROR_PAYLOAD_LENGTH;
}

/**
 * @brief  解码重启命令的响应
 * @param  frame: 解析后的帧指针
 * @param  state: 输出参数，设备状态（1 字节）
 * @param  rsv: 输出参数，保留字节（1 字节）
 * @retval ES1642_STATUS_OK: 解码成功
 * @retval ES1642_STATUS_ERROR_PARAM: 参数错误
 * @retval ES1642_STATUS_ERROR_PAYLOAD_LENGTH: 载荷长度不为 2
 * @retval 其它错误码: 指令字不匹配或异常帧等
 */
es1642_status_t ES1642_DecodeRebootResponse(const es1642_frame_t *frame,
                                            uint8_t *state,
                                            uint8_t *rsv)
{
    es1642_status_t status = es1642_expect_normal_cmd(frame, ES1642_CMD_REBOOT);

    if ((status != ES1642_STATUS_OK) || (state == NULL) || (rsv == NULL))
    {
        return (status != ES1642_STATUS_OK) ? status : ES1642_STATUS_ERROR_PARAM;
    }

    if (frame->data_len != 2U)
    {
        return ES1642_STATUS_ERROR_PAYLOAD_LENGTH;
    }

    *state = frame->data[0];
    *rsv = frame->data[1];

    return ES1642_STATUS_OK;
}

/**
 * @brief  解码读取版本信息的响应
 * @param  frame: 解析后的帧指针
 * @param  version: 输出参数，版本信息结构体指针
 * @retval ES1642_STATUS_OK: 解码成功
 * @retval ES1642_STATUS_ERROR_PARAM: 参数错误
 * @retval ES1642_STATUS_ERROR_PAYLOAD_LENGTH: 载荷长度不为 7
 */
es1642_status_t ES1642_DecodeVersion(const es1642_frame_t *frame,
                                     es1642_version_t *version)
{
    es1642_status_t status = es1642_expect_normal_cmd(frame, ES1642_CMD_READ_VERSION);

    if ((status != ES1642_STATUS_OK) || (version == NULL))
    {
        return (status != ES1642_STATUS_OK) ? status : ES1642_STATUS_ERROR_PARAM;
    }

    if (frame->data_len != 7U)
    {
        return ES1642_STATUS_ERROR_PAYLOAD_LENGTH;
    }

    es1642_parse_version_bytes(frame->data, version);
    return ES1642_STATUS_OK;
}

/**
 * @brief  解码读取 MAC 响应
 * @param  frame: 解析后的帧指针
 * @param  mac: 输出缓冲，长度为 ES1642_ADDR_LEN（6）
 * @retval ES1642_STATUS_OK: 解码成功
 * @retval ES1642_STATUS_ERROR_PARAM: 参数错误
 * @retval ES1642_STATUS_ERROR_PAYLOAD_LENGTH: 载荷长度与地址长度不符
 */
es1642_status_t ES1642_DecodeMac(const es1642_frame_t *frame,
                                 uint8_t mac[ES1642_ADDR_LEN])
{
    es1642_status_t status = es1642_expect_normal_cmd(frame, ES1642_CMD_READ_MAC);

    if ((status != ES1642_STATUS_OK) || (mac == NULL))
    {
        return (status != ES1642_STATUS_OK) ? status : ES1642_STATUS_ERROR_PARAM;
    }

    if (frame->data_len != ES1642_ADDR_LEN)
    {
        return ES1642_STATUS_ERROR_PAYLOAD_LENGTH;
    }

    (void)memcpy(mac, frame->data, ES1642_ADDR_LEN);
    return ES1642_STATUS_OK;
}

/**
 * @brief  解码读取模块地址响应
 * @param  frame: 解析后的帧指针
 * @param  addr: 输出缓冲，长度为 ES1642_ADDR_LEN（6）
 * @retval ES1642_STATUS_OK: 解码成功
 * @retval ES1642_STATUS_ERROR_PARAM: 参数错误
 * @retval ES1642_STATUS_ERROR_PAYLOAD_LENGTH: 载荷长度不符
 */
es1642_status_t ES1642_DecodeAddr(const es1642_frame_t *frame,
                                  uint8_t addr[ES1642_ADDR_LEN])
{
    es1642_status_t status = es1642_expect_normal_cmd(frame, ES1642_CMD_READ_ADDR);

    if ((status != ES1642_STATUS_OK) || (addr == NULL))
    {
        return (status != ES1642_STATUS_OK) ? status : ES1642_STATUS_ERROR_PARAM;
    }

    if (frame->data_len != ES1642_ADDR_LEN)
    {
        return ES1642_STATUS_ERROR_PAYLOAD_LENGTH;
    }

    (void)memcpy(addr, frame->data, ES1642_ADDR_LEN);
    return ES1642_STATUS_OK;
}

/**
 * @brief  解码网络参数响应
 * @param  frame: 解析后的帧指针
 * @param  net_param: 输出参数，网络参数结构体指针
 * @retval ES1642_STATUS_OK: 解码成功
 * @retval ES1642_STATUS_ERROR_PARAM: 参数错误
 * @retval ES1642_STATUS_ERROR_PAYLOAD_LENGTH: 载荷长度不为 7
 */
es1642_status_t ES1642_DecodeNetParam(const es1642_frame_t *frame,
                                      es1642_net_param_t *net_param)
{
    es1642_status_t status = es1642_expect_normal_cmd(frame, ES1642_CMD_READ_NET_PARAM);

    if ((status != ES1642_STATUS_OK) || (net_param == NULL))
    {
        return (status != ES1642_STATUS_OK) ? status : ES1642_STATUS_ERROR_PARAM;
    }

    if (frame->data_len != 7U)
    {
        return ES1642_STATUS_ERROR_PAYLOAD_LENGTH;
    }

    net_param->relay_depth = frame->data[0];
    (void)memcpy(net_param->network_key, &frame->data[1], ES1642_LOCAL_NET_KEY_LEN);
    net_param->rsv1 = frame->data[5];
    net_param->rsv2 = frame->data[6];

    return ES1642_STATUS_OK;
}

/**
 * @brief  解码接收到的数据帧（RECV_DATA）
 * @param  frame: 解析后的帧指针
 * @param  recv_data: 输出参数，接收数据结构体指针
 * @retval ES1642_STATUS_OK: 解码成功
 * @retval ES1642_STATUS_ERROR_PARAM: 参数错误
 * @retval ES1642_STATUS_ERROR_PAYLOAD_LENGTH: 载荷长度不正确
 * @note   解码后填写源地址、原始控制字段、RSSI、用户数据指针与长度
 */
es1642_status_t ES1642_DecodeRecvData(const es1642_frame_t *frame,
                                      es1642_recv_data_t *recv_data)
{
    uint32_t raw_data_ctrl;
    uint16_t user_data_len;
    es1642_status_t status = es1642_expect_normal_cmd(frame, ES1642_CMD_RECV_DATA);

    if ((status != ES1642_STATUS_OK) || (recv_data == NULL))
    {
        return (status != ES1642_STATUS_OK) ? status : ES1642_STATUS_ERROR_PARAM;
    }

    if (frame->data_len < ES1642_RECV_DATA_FIXED_LEN)
    {
        return ES1642_STATUS_ERROR_PAYLOAD_LENGTH;
    }

    raw_data_ctrl = es1642_get_le24(&frame->data[0]);
    user_data_len = es1642_get_le16(&frame->data[9]);

    if (frame->data_len != (uint16_t)(ES1642_RECV_DATA_FIXED_LEN + user_data_len))
    {
        return ES1642_STATUS_ERROR_PAYLOAD_LENGTH;
    }

    recv_data->raw_data_ctrl = raw_data_ctrl;
    recv_data->relay_depth = (uint8_t)((raw_data_ctrl >> 8) & 0x0FU);
    recv_data->rssi = es1642_sign_extend_9bit((uint16_t)((raw_data_ctrl >> 15) & 0x01FFU));
    (void)memcpy(recv_data->src_addr, &frame->data[3], ES1642_ADDR_LEN);
    recv_data->user_data_len = user_data_len;
    recv_data->user_data = (user_data_len > 0U) ? &frame->data[11] : NULL;

    return ES1642_STATUS_OK;
}

/**
 * @brief  解码搜索结果报告
 * @param  frame: 解析后的帧指针
 * @param  result: 输出参数，搜索结果结构体指针
 * @retval ES1642_STATUS_OK: 解码成功
 * @retval ES1642_STATUS_ERROR_PARAM: 参数错误
 * @retval ES1642_STATUS_ERROR_PAYLOAD_LENGTH: 载荷长度不正确
//这个命令数据手册有误，在addr前面还有1字节的设备数量
 */
es1642_status_t ES1642_DecodeSearchResult(const es1642_frame_t *frame,
                                          es1642_search_result_t *result)
{
    uint8_t attribute_len;
    es1642_status_t status = es1642_expect_normal_cmd(frame, ES1642_CMD_REPORT_SEARCH_RESULT);

    if ((status != ES1642_STATUS_OK) || (result == NULL))
    {
        return (status != ES1642_STATUS_OK) ? status : ES1642_STATUS_ERROR_PARAM;
    }

    if (frame->data_len < 10U)
    {
        return ES1642_STATUS_ERROR_PAYLOAD_LENGTH;
    }
		
    attribute_len = frame->data[9];
		
    if (frame->data_len != (uint16_t)(10U + attribute_len))
    {
        return ES1642_STATUS_ERROR_PAYLOAD_LENGTH;
    }

		result->count = frame->data[0];
    (void)memcpy(result->dev_addr, &frame->data[1], ES1642_ADDR_LEN);
    result->raw_dev_ctrl = es1642_get_le16(&frame->data[7]);
    result->net_state = (uint8_t)((result->raw_dev_ctrl >> 4) & 0x0FU);
    result->attribute_len = attribute_len;
    result->attribute = (attribute_len > 0U) ? &frame->data[10] : NULL;

    return ES1642_STATUS_OK;
}

/**
 * @brief  解码搜索通知帧
 * @param  frame: 解析后的帧指针
 * @param  notify: 输出参数，搜索通知结构体指针
 * @retval ES1642_STATUS_OK: 解码成功
 * @retval ES1642_STATUS_ERROR_PARAM: 参数错误
 * @retval ES1642_STATUS_ERROR_PAYLOAD_LENGTH: 载荷长度不正确
 */
es1642_status_t ES1642_DecodeSearchNotify(const es1642_frame_t *frame,
                                          es1642_search_notify_t *notify)
{
    uint8_t attribute_len;
    es1642_status_t status = es1642_expect_normal_cmd(frame, ES1642_CMD_NOTIFY_SEARCH);

    if ((status != ES1642_STATUS_OK) || (notify == NULL))
    {
        return (status != ES1642_STATUS_OK) ? status : ES1642_STATUS_ERROR_PARAM;
    }

    if (frame->data_len < 10U)
    {
        return ES1642_STATUS_ERROR_PAYLOAD_LENGTH;
    }

    attribute_len = frame->data[9];

    if (frame->data_len != (uint16_t)(10U + attribute_len))
    {
        return ES1642_STATUS_ERROR_PAYLOAD_LENGTH;
    }

    notify->raw_data_ctrl = es1642_get_le16(&frame->data[0]);
    (void)memcpy(notify->src_addr, &frame->data[2], ES1642_ADDR_LEN);
    notify->task_id = frame->data[8];
    notify->attribute_len = attribute_len;
    notify->attribute = (attribute_len > 0U) ? &frame->data[10] : NULL;

    return ES1642_STATUS_OK;
}

/**
 * @brief  解码 PSK 变更通知
 * @param  frame: 解析后的帧指针
 * @param  notify: 输出参数，PSK 通知结构体指针
 * @retval ES1642_STATUS_OK: 解码成功
 * @retval ES1642_STATUS_ERROR_PARAM: 参数错误
 * @retval ES1642_STATUS_ERROR_PAYLOAD_LENGTH: 载荷长度不为 8
 */
es1642_status_t ES1642_DecodePskNotify(const es1642_frame_t *frame,
                                       es1642_psk_notify_t *notify)
{
    es1642_status_t status = es1642_expect_normal_cmd(frame, ES1642_CMD_NOTIFY_PSK);

    if ((status != ES1642_STATUS_OK) || (notify == NULL))
    {
        return (status != ES1642_STATUS_OK) ? status : ES1642_STATUS_ERROR_PARAM;
    }

    if (frame->data_len != 8U)
    {
        return ES1642_STATUS_ERROR_PAYLOAD_LENGTH;
    }

    notify->raw_data_ctrl = es1642_get_le16(&frame->data[0]);
    (void)memcpy(notify->src_addr, &frame->data[2], ES1642_ADDR_LEN);
    notify->op = (uint8_t)((notify->raw_data_ctrl >> 12) & 0x03U);

    return ES1642_STATUS_OK;
}

/**
 * @brief  解码 PSK 操作结果报告
 * @param  frame: 解析后的帧指针
 * @param  result: 输出参数，PSK 结果结构体指针
 * @retval ES1642_STATUS_OK: 解码成功
 * @retval ES1642_STATUS_ERROR_PARAM: 参数错误
 * @retval ES1642_STATUS_ERROR_PAYLOAD_LENGTH: 载荷长度不为 9
 */
es1642_status_t ES1642_DecodePskResult(const es1642_frame_t *frame,
                                       es1642_psk_result_t *result)
{
    es1642_status_t status = es1642_expect_normal_cmd(frame, ES1642_CMD_REPORT_PSK_RESULT);

    if ((status != ES1642_STATUS_OK) || (result == NULL))
    {
        return (status != ES1642_STATUS_OK) ? status : ES1642_STATUS_ERROR_PARAM;
    }

    if (frame->data_len != 9U)
    {
        return ES1642_STATUS_ERROR_PAYLOAD_LENGTH;
    }

    result->raw_data_ctrl = es1642_get_le16(&frame->data[0]);
    (void)memcpy(result->src_addr, &frame->data[2], ES1642_ADDR_LEN);
    result->state = frame->data[8];

    return ES1642_STATUS_OK;
}

/**
 * @brief  解码远程设备的版本信息响应
 * @param  frame: 解析后的帧指针
 * @param  version: 输出参数，远程版本信息结构体指针
 * @retval ES1642_STATUS_OK: 解码成功
 * @retval ES1642_STATUS_ERROR_PARAM: 参数错误
 * @retval ES1642_STATUS_ERROR_PAYLOAD_LENGTH: 载荷长度不为 13
 */
es1642_status_t ES1642_DecodeRemoteVersion(const es1642_frame_t *frame,
                                           es1642_remote_version_t *version)
{
    es1642_status_t status = es1642_expect_normal_cmd(frame, ES1642_CMD_REMOTE_READ_VERSION);

    if ((status != ES1642_STATUS_OK) || (version == NULL))
    {
        return (status != ES1642_STATUS_OK) ? status : ES1642_STATUS_ERROR_PARAM;
    }

    if (frame->data_len != 13U)
    {
        return ES1642_STATUS_ERROR_PAYLOAD_LENGTH;
    }

    (void)memcpy(version->src_addr, &frame->data[0], ES1642_ADDR_LEN);
    es1642_parse_version_bytes(&frame->data[6], &version->version);

    return ES1642_STATUS_OK;
}

/**
 * @brief  解码远程设备的 MAC 响应
 * @param  frame: 解析后的帧指针
 * @param  mac: 输出参数，远程 MAC 结构体指针
 * @retval ES1642_STATUS_OK: 解码成功
 * @retval ES1642_STATUS_ERROR_PARAM: 参数错误
 * @retval ES1642_STATUS_ERROR_PAYLOAD_LENGTH: 载荷长度不为 12
 */
es1642_status_t ES1642_DecodeRemoteMac(const es1642_frame_t *frame,
                                       es1642_remote_mac_t *mac)
{
    es1642_status_t status = es1642_expect_normal_cmd(frame, ES1642_CMD_REMOTE_READ_MAC);

    if ((status != ES1642_STATUS_OK) || (mac == NULL))
    {
        return (status != ES1642_STATUS_OK) ? status : ES1642_STATUS_ERROR_PARAM;
    }

    if (frame->data_len != 12U)
    {
        return ES1642_STATUS_ERROR_PAYLOAD_LENGTH;
    }

    (void)memcpy(mac->src_addr, &frame->data[0], ES1642_ADDR_LEN);
    (void)memcpy(mac->mac, &frame->data[6], ES1642_ADDR_LEN);

    return ES1642_STATUS_OK;
}

/**
 * @brief  解码远程设备的网络参数响应
 * @param  frame: 解析后的帧指针
 * @param  net_param: 输出参数，远程网络参数结构体指针
 * @retval ES1642_STATUS_OK: 解码成功
 * @retval ES1642_STATUS_ERROR_PARAM: 参数错误
 * @retval ES1642_STATUS_ERROR_PAYLOAD_LENGTH: 载荷长度不为 13
 */
es1642_status_t ES1642_DecodeRemoteNetParam(const es1642_frame_t *frame,
                                            es1642_remote_net_param_t *net_param)
{
    es1642_status_t status = es1642_expect_normal_cmd(frame, ES1642_CMD_REMOTE_READ_NET_PARAM);

    if ((status != ES1642_STATUS_OK) || (net_param == NULL))
    {
        return (status != ES1642_STATUS_OK) ? status : ES1642_STATUS_ERROR_PARAM;
    }

    if (frame->data_len != 13U)
    {
        return ES1642_STATUS_ERROR_PAYLOAD_LENGTH;
    }

    (void)memcpy(net_param->src_addr, &frame->data[0], ES1642_ADDR_LEN);
    net_param->net_param.relay_depth = frame->data[6];
    (void)memcpy(net_param->net_param.network_key, &frame->data[7], ES1642_LOCAL_NET_KEY_LEN);
    net_param->net_param.rsv1 = frame->data[11];
    net_param->net_param.rsv2 = frame->data[12];

    return ES1642_STATUS_OK;
}

/**
 * @brief  解码本地异常应答（模块本地返回的异常）
 * @param  frame: 解析后的帧指针
 * @param  exception_code: 输出参数，异常码
 * @retval ES1642_STATUS_OK: 解码成功
 * @retval ES1642_STATUS_ERROR_PARAM: 参数错误
 * @retval ES1642_STATUS_ERROR_NOT_EXCEPTION_FRAME: 不是异常帧
 * @retval ES1642_STATUS_ERROR_PAYLOAD_LENGTH: 载荷长度不为 1
 */
es1642_status_t ES1642_DecodeLocalException(const es1642_frame_t *frame,
                                            uint8_t *exception_code)
{
    if ((frame == NULL) || (exception_code == NULL))
    {
        return ES1642_STATUS_ERROR_PARAM;
    }

    if (!frame->is_exception)
    {
        return ES1642_STATUS_ERROR_NOT_EXCEPTION_FRAME;
    }

    if (frame->data_len != 1U)
    {
        return ES1642_STATUS_ERROR_PAYLOAD_LENGTH;
    }

    *exception_code = frame->data[0];
    return ES1642_STATUS_OK;
}

/**
 * @brief  解码远程异常应答（远端设备返回的异常）
 * @param  frame: 解析后的帧指针
 * @param  src_addr: 输出参数，产生异常的远端设备地址（6 字节）
 * @param  exception_code: 输出参数，异常码
 * @retval ES1642_STATUS_OK: 解码成功
 * @retval ES1642_STATUS_ERROR_PARAM: 参数错误
 * @retval ES1642_STATUS_ERROR_NOT_EXCEPTION_FRAME: 不是异常帧
 * @retval ES1642_STATUS_ERROR_PAYLOAD_LENGTH: 载荷长度不为 7
 */
es1642_status_t ES1642_DecodeRemoteException(const es1642_frame_t *frame,
                                             uint8_t src_addr[ES1642_ADDR_LEN],
                                             uint8_t *exception_code)
{
    if ((frame == NULL) || (src_addr == NULL) || (exception_code == NULL))
    {
        return ES1642_STATUS_ERROR_PARAM;
    }

    if (!frame->is_exception)
    {
        return ES1642_STATUS_ERROR_NOT_EXCEPTION_FRAME;
    }

    if (frame->data_len != 7U)
    {
        return ES1642_STATUS_ERROR_PAYLOAD_LENGTH;
    }

    (void)memcpy(src_addr, &frame->data[0], ES1642_ADDR_LEN);
    *exception_code = frame->data[6];

    return ES1642_STATUS_OK;
}
