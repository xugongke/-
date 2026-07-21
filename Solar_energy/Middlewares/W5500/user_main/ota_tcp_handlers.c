/**
  ******************************************************************************
  * @file    ota_tcp_handlers.c
  * @brief   OTA升级 TCP 命令处理函数
  *
  * @details 本文件实现OTA升级相关的TCP命令处理函数, 是上位机与设备
  *          OTA升级模块之间的网络接口层. 每个函数对应一个OTA TCP命令:
  *
  *            - CMD_OTA_BEGIN  : 开始升级, 通知设备擦除备用Bank准备接收
  *            - CMD_OTA_DATA   : 传输一包固件数据
  *            - CMD_OTA_END    : 结束传输, 触发CRC校验和Bank切换
  *            - CMD_OTA_STATUS : 查询当前升级状态
  *
  *          统一处理流程: 解析TCP帧 -> 校验参数 -> 调用 ota_upgrade.c 中
  *          对应API -> 根据返回值回复 RESPONSE(成功) 或 ERROR(失败)
  ******************************************************************************
  */
#include "tcp_cmd_handler.h"
#include "ota_upgrade.h"
#include "user_main.h"
#include <stdio.h>

/**
 * @brief  处理 CMD_OTA_BEGIN 命令 (开始OTA升级)
 * @note   解析上位机下发的固件总大小和CRC32, 调用 OTA_Begin() 擦除备用Bank.
 *         成功回复 RESPONSE(0x00), 失败回复 ERROR(错误码).
 * @param  data  TCP帧负载 (指向 OtaBeginReq_t 结构)
 * @param  len   负载长度
 */
void tcp_resp_ota_begin(const uint8_t *data, uint16_t len)
{
    /* 参数长度校验: 至少要包含 OtaBeginReq_t (8字节) */
    if (len < sizeof(OtaBeginReq_t)) { tcp_send_error(ERR_PARAM_INVALID); return; }
    /* 解析开始升级请求 (固件总大小 + CRC32) */
    const OtaBeginReq_t *req = (const OtaBeginReq_t *)data;
    /* 调用OTA核心: 擦除备用Bank, 进入接收状态 */
    int rc = OTA_Begin(req->total_size, req->fw_crc32);
    if (rc == OTA_OK) {
        /* 成功: 回复1字节 0x00 表示开始成功 */
        uint8_t resp = 0x00;
        tcp_send_frame(FRAME_TYPE_RESPONSE, CMD_OTA_BEGIN, &resp, 1);
    } else {
        /* 失败: 回复4字节负数错误码 (见 ota_err_t) */
        int32_t code = (int32_t)rc;
        tcp_send_frame(FRAME_TYPE_ERROR, CMD_OTA_BEGIN, (uint8_t *)&code, 4);
    }
}

/**
 * @brief  处理 CMD_OTA_DATA 命令 (接收一包固件数据)
 * @note   解析数据包头(序号+长度)和负载数据, 调用 OTA_ReceiveChunk() 写入Flash.
 *         数据包必须按序号顺序到达, 否则返回 OTA_ERR_SEQ.
 * @param  data  TCP帧负载 (OtaDataHeader_t 头 + 实际固件数据)
 * @param  len   负载总长度
 */
void tcp_resp_ota_data(const uint8_t *data, uint16_t len)
{
    /* 参数长度校验: 至少要包含数据包头 OtaDataHeader_t (4字节) */
    if (len < sizeof(OtaDataHeader_t)) { tcp_send_error(ERR_PARAM_INVALID); return; }
    /* 解析数据包头: 包序号 seq + 本包数据长度 data_len */
    const OtaDataHeader_t *hdr = (const OtaDataHeader_t *)data;
    /* 负载数据紧跟在包头之后 */
    const uint8_t *payload = data + sizeof(OtaDataHeader_t);
    /* 二次校验: 实际长度必须等于包头声明长度 (防止帧错位/截断) */
    if ((uint32_t)len != (uint32_t)(sizeof(OtaDataHeader_t) + hdr->data_len)) {
        tcp_send_error(ERR_PARAM_INVALID); return;
    }
    /* 调用OTA核心: 将本包数据写入备用Bank (内部会校验序号连续性) */
    int rc = OTA_ReceiveChunk(hdr->seq, payload, hdr->data_len);
    if (rc == OTA_OK) {
        /* 成功: 回复1字节 0x00, 上位机收到后发送下一包 */
        uint8_t resp = 0x00;
        tcp_send_frame(FRAME_TYPE_RESPONSE, CMD_OTA_DATA, &resp, 1);
    } else {
        /* 失败: 回复4字节错误码 (序号错乱/溢出/Flash写失败等) */
        int32_t code = (int32_t)rc;
        tcp_send_frame(FRAME_TYPE_ERROR, CMD_OTA_DATA, (uint8_t *)&code, 4);
    }
}

/**
 * @brief  处理 CMD_OTA_END 命令 (结束升级并触发Bank切换)
 * @note   二次确认CRC32后, 调用 OTA_End() 执行:
 *         大小校验 -> CRC32硬件校验 -> 写trial标志 -> 切换Bank并复位.
 *         注意: 校验通过后设备会立即复位, 因此 OTA_OK 这一路通常来不及
 *         把响应帧发出去, 上位机主要靠重连后的 CMD_OTA_STATUS 判断结果.
 * @param  data  TCP帧负载 (指向 OtaEndReq_t, 仅含二次确认CRC32)
 * @param  len   负载长度
 */
void tcp_resp_ota_end(const uint8_t *data, uint16_t len)
{
    /* 参数长度校验: 至少要包含 OtaEndReq_t (4字节) */
    if (len < sizeof(OtaEndReq_t)) { tcp_send_error(ERR_PARAM_INVALID); return; }
    /* 解析结束请求: 上位机再次下发的CRC32, 用于二次确认 */
    const OtaEndReq_t *req = (const OtaEndReq_t *)data;
    /* 调用OTA核心: 校验+切换Bank (成功则复位, 不会真正返回) */
    int rc = OTA_End(req->fw_crc32);
    if (rc == OTA_OK) {
        /* 校验通过, 已发起Bank切换 (复位前可能来不及发送此帧) */
        uint8_t resp = 0x00;
        tcp_send_frame(FRAME_TYPE_RESPONSE, CMD_OTA_END, &resp, 1);
    } else {
        /* 失败: 回复错误码 (大小不符/CRC校验失败等), 升级中止 */
        int32_t code = (int32_t)rc;
        tcp_send_frame(FRAME_TYPE_ERROR, CMD_OTA_END, (uint8_t *)&code, 4);
    }
}

/**
 * @brief  处理 CMD_OTA_STATUS 命令 (查询当前升级状态)
 * @note   上位机可通过此命令轮询升级进度, 用于显示进度条/故障诊断.
 *         返回当前Bank、升级状态、已接收字节数、固件总大小、版本号等.
 */
void tcp_resp_ota_status(void)
{
    OtaStatusResp_t status;
    /* 收集当前升级状态 (Bank号/状态/进度/版本等信息) */
    OTA_GetStatus(&status);
    /* 将状态结构体作为响应负载整体发送给上位机 */
    tcp_send_frame(FRAME_TYPE_RESPONSE, CMD_OTA_STATUS,
                   (uint8_t *)&status, sizeof(status));
}