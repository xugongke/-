/**
  ******************************************************************************
  * @file    ota_tcp_handlers.c
  * @brief   OTA TCP command handler functions
  ******************************************************************************
  */
#include "tcp_cmd_handler.h"
#include "ota_upgrade.h"
#include "user_main.h"
#include <stdio.h>

void tcp_resp_ota_begin(const uint8_t *data, uint16_t len)
{
    if (len < sizeof(OtaBeginReq_t)) { tcp_send_error(ERR_PARAM_INVALID); return; }
    const OtaBeginReq_t *req = (const OtaBeginReq_t *)data;
    int rc = OTA_Begin(req->total_size, req->fw_crc32);
    if (rc == OTA_OK) {
        uint8_t resp = 0x00;
        tcp_send_frame(FRAME_TYPE_RESPONSE, CMD_OTA_BEGIN, &resp, 1);
    } else {
        int32_t code = (int32_t)rc;
        tcp_send_frame(FRAME_TYPE_ERROR, CMD_OTA_BEGIN, (uint8_t *)&code, 4);
    }
}

void tcp_resp_ota_data(const uint8_t *data, uint16_t len)
{
    if (len < sizeof(OtaDataHeader_t)) { tcp_send_error(ERR_PARAM_INVALID); return; }
    const OtaDataHeader_t *hdr = (const OtaDataHeader_t *)data;
    const uint8_t *payload = data + sizeof(OtaDataHeader_t);
    if ((uint32_t)len != (uint32_t)(sizeof(OtaDataHeader_t) + hdr->data_len)) {
        tcp_send_error(ERR_PARAM_INVALID); return;
    }
    int rc = OTA_ReceiveChunk(hdr->seq, payload, hdr->data_len);
    if (rc == OTA_OK) {
        uint8_t resp = 0x00;
        tcp_send_frame(FRAME_TYPE_RESPONSE, CMD_OTA_DATA, &resp, 1);
    } else {
        int32_t code = (int32_t)rc;
        tcp_send_frame(FRAME_TYPE_ERROR, CMD_OTA_DATA, (uint8_t *)&code, 4);
    }
}

void tcp_resp_ota_end(const uint8_t *data, uint16_t len)
{
    if (len < sizeof(OtaEndReq_t)) { tcp_send_error(ERR_PARAM_INVALID); return; }
    const OtaEndReq_t *req = (const OtaEndReq_t *)data;
    int rc = OTA_End(req->fw_crc32);
    if (rc == OTA_OK) {
        uint8_t resp = 0x00;
        tcp_send_frame(FRAME_TYPE_RESPONSE, CMD_OTA_END, &resp, 1);
    } else {
        int32_t code = (int32_t)rc;
        tcp_send_frame(FRAME_TYPE_ERROR, CMD_OTA_END, (uint8_t *)&code, 4);
    }
}

void tcp_resp_ota_status(void)
{
    OtaStatusResp_t status;
    OTA_GetStatus(&status);
    tcp_send_frame(FRAME_TYPE_RESPONSE, CMD_OTA_STATUS,
                   (uint8_t *)&status, sizeof(status));
}
