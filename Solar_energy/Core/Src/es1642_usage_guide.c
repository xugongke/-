/**
 * @file    es1642_usage_guide.c
 * @brief   ES1642模块使用指南 - DMA+串口空闲中断实现示例
 * @author  Cline
 * @date    2026-03-16
 * 
 * 本文件提供ES1642模块的完整使用说明，包括：
 * 1. 模块初始化和配置
 * 2. DMA+串口空闲中断的数据收发实现
 * 3. 常用功能示例代码
 * 4. 错误处理和调试建议
 */
#include "es1642.h"
#include "usart.h"
#include "es1642_usage_guide.h"
#include <stdio.h>
#include "ff.h"
#include "device_manager.h"
#include "interrupt.h"

/* ========================= 使用说明 ========================= */

/*
 * 一、ES1642模块驱动架构说明
 * =============================
 * 
 * 1. 分层设计：
 *    - 协议层：负责帧的封装、解析、校验（es1642.c/h）
 *    - 传输层：负责实际的数据收发，通过回调函数实现（用户实现）
 *    - 应用层：调用协议层接口完成具体业务功能（用户实现）
 * 
 * 2. 核心概念：
 *    - es1642_handle_t：驱动句柄，包含端口配置、接收缓冲区等
 *    - es1642_port_t：端口配置，包含发送回调、帧接收回调、错误回调
 *    - es1642_frame_t：帧结构体，解析后的帧信息
 * 
 * 3. 数据流向：
 *    发送：应用层 -> ES1642_SendXXX() -> write回调 -> 串口DMA发送
 *    接收：串口DMA接收 -> 空闲中断 -> InputBuffer回调 -> 解析帧 -> on_frame回调
 * 
 * 二、硬件连接要求
 * =================
 * 
 * 1. 串口配置：
 *    - 波特率：9600bps
 *    - 数据位：8位
 *    - 停止位：1位
 *    - 校验位：无
 * 
 * 2. DMA配置：
 *    - 发送：使用DMA1 StreamX ChannelX（根据实际硬件配置）
 *    - 接收：使用DMA1 StreamY ChannelY（根据实际硬件配置）
 *    - 接收缓冲区大小：建议至少ES1642_MAX_FRAME_LEN（519字节）
 * 
 * 3. 中断配置：
 *    - UART空闲中断：用于检测接收完成
 *    - DMA传输完成中断：用于发送完成（可选）
 */

/* ========================= 示例代码 ========================= */

/*
 * 三、完整的实现示例
 * ===================
 * 
 * 以下代码展示了如何使用DMA+串口空闲中断实现ES1642模块通信
 */

/* ======== 步骤1：定义全局变量和缓冲区 ======== */

/* ES1642驱动句柄 */
es1642_handle_t g_es1642_handle;

/* 串口接收缓冲区（DMA使用） */
__attribute__((section("RW_IRAM1")))// 放到普通 SRAM（0x20000000）
uint8_t g_es1642_rx_buf[ES1642_MAX_FRAME_LEN];
uint8_t* Current_addr = NULL;

/* ES1642 响应数据缓冲区
 * 由 es1642_on_frame_received 写入，由 ES1642_SendUserData 读取
 * 受 ES1642_mutexHandle 互斥锁保护，同一时刻只有一个调用者
 */
es1642_response_t g_es1642_response;

/* 当前等待的响应类型，用于区分信号量是由 RECV_DATA 还是 REPORT_PSK_RESULT 释放 */
es1642_wait_type_t g_es1642_wait_type = ES1642_WAIT_NONE;

/* PSK设置结果缓冲区 */
es1642_psk_result_response_t g_es1642_psk_result;

/* 串口句柄（假设使用huart2） */
extern UART_HandleTypeDef huart2;


/* ======== 步骤2：实现串口发送回调函数 ======== */

/**
 * @brief  串口发送回调函数
 * @param  data: 要发送的数据指针
 * @param  len: 数据长度
 * @param  user_arg: 用户参数（未使用）
 * @retval 发送的字节数（成功）或负数（失败）
 * @note   此函数由ES1642驱动内部调用，实现通过DMA发送数据
 */
static int32_t es1642_uart_write(const uint8_t *data, uint16_t len, void *user_arg)
{
    HAL_StatusTypeDef status;
    
    /* 参数检查 */
    if (data == NULL || len == 0)
    {
        return -1;
    }
    
    /* 使用中断方式发送数据（因为DMA不够用了）所以要保证给es1642模块发送的字节数要少，要不然会频繁占用CPU */
    status = HAL_UART_Transmit_IT(&huart2, (uint8_t *)data, len);
    
    if (status == HAL_OK)
    {
        return len;  /* 返回实际发送的字节数 */
    }
    else
    {
				printf("es1642发送命令失败\r\n");
        return -1;  /* 发送失败 */
    }
}

/* ======== 步骤3：实现帧接收回调函数 ======== */

/**
 * @brief  帧接收完成回调函数
 * @param  handle: ES1642驱动句柄
 * @param  frame: 接收到的帧结构体
 * @param  user_arg: 用户参数（未使用）
 * @retval None
 * @note   当接收到完整帧时，驱动会调用此函数
 */
void es1642_on_frame_received(es1642_handle_t *handle, 
                                     const es1642_frame_t *frame, 
                                     void *user_arg)
{
    es1642_status_t status;
    
    /* 根据指令字处理不同类型的响应 */
    switch (frame->cmd)
    {
        case ES1642_CMD_REBOOT:
        {
            uint8_t state, rsv;
            status = ES1642_DecodeRebootResponse(frame, &state, &rsv);
            if (status == ES1642_STATUS_OK)
            {
							if(state == 0x01)
							{
								printf("重启成功\r\n");
							}
							else
							{
								printf("重启失败\r\n");
							}
            }
            break;
        }
        
        case ES1642_CMD_READ_VERSION:
        {
            es1642_version_t version;
            status = ES1642_DecodeVersion(frame, &version);
            if (status == ES1642_STATUS_OK)
            {
                printf("版本信息: 厂商=0x%04X, 芯片=0x%04X, 版本=0x%04X\r\n",
                       version.vendor_id, version.chip_type, version.version_bcd);
            }
            break;
        }
        
        case ES1642_CMD_READ_MAC:
        {
            uint8_t mac[ES1642_ADDR_LEN];
            status = ES1642_DecodeMac(frame, mac);
            if (status == ES1642_STATUS_OK)
            {
                printf("MAC地址: %02X-%02X-%02X-%02X-%02X-%02X\r\n",
                       mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
            }
            break;
        }
        
        case ES1642_CMD_READ_ADDR:
        {
            uint8_t addr[ES1642_ADDR_LEN];
            status = ES1642_DecodeAddr(frame, addr);
            if (status == ES1642_STATUS_OK)
            {
                printf("模块地址: %02X:%02X:%02X:%02X:%02X:%02X\r\n",
                       addr[0], addr[1], addr[2], addr[3], addr[4], addr[5]);
            }
            break;
        }

        case ES1642_CMD_READ_NET_PARAM:
        {
            es1642_net_param_t net_param;
            status = ES1642_DecodeNetParam(frame, &net_param);
            if (status == ES1642_STATUS_OK)
            {
                printf("网络参数: 中继深度=%d, 口令=%02X%02X%02X%02X\r\n",
                       net_param.relay_depth,
                       net_param.network_key[0], net_param.network_key[1],
                       net_param.network_key[2], net_param.network_key[3]);
            }
            break;
        }

        case ES1642_CMD_SET_ADDR:
        {
            status = ES1642_DecodeEmptyResponse(frame, ES1642_CMD_SET_ADDR);
            if (status == ES1642_STATUS_OK)
            {
                printf("设置地址: OK\r\n");
            }
            break;
        }

        case ES1642_CMD_SET_NET_PARAM:
        {
            status = ES1642_DecodeEmptyResponse(frame, ES1642_CMD_SET_NET_PARAM);
            if (status == ES1642_STATUS_OK)
            {
                printf("设置网络参数: OK\r\n");
            }
            break;
        }

        case ES1642_CMD_SEND_DATA:
        {
            /* 发送命令的应答通常为空响应，按需处理 */
            status = ES1642_DecodeEmptyResponse(frame, ES1642_CMD_SEND_DATA);
            if (status == ES1642_STATUS_OK)
            {
                printf("发送数据命令已被模块接收\r\n");
            }
            break;
        }

        case ES1642_CMD_START_SEARCH:
				{
            status = ES1642_DecodeEmptyResponse(frame, ES1642_CMD_START_SEARCH);
            if (status == ES1642_STATUS_OK)
            {
                printf("启动搜索: OK\r\n");
            }
            break;
        }

        case ES1642_CMD_STOP_SEARCH:
        {
            status = ES1642_DecodeEmptyResponse(frame, ES1642_CMD_STOP_SEARCH);
            if (status == ES1642_STATUS_OK)
            {
                printf("停止搜索: OK\r\n");
								if(device_count > 0)
								{
									save_devices();//保存RAM中的设备表到SD卡中
									print_device_list();
								}
								else if(device_count == 0)
								{
									device_manager_init();//重新读取SD卡中的记录并加载到RAM中
								}
            }
            break;
        }

        case ES1642_CMD_REPORT_SEARCH_RESULT:
        {
            es1642_search_result_t result;
            status = ES1642_DecodeSearchResult(frame, &result);
            if (status == ES1642_STATUS_OK)
            {
							printf("设备数目:%d\r\n搜索到设备: 通信地址=%02X:%02X:%02X:%02X:%02X:%02X, "
                       "网络状态=%d\r\n",result.count,
                       result.dev_addr[0], result.dev_addr[1],
                       result.dev_addr[2], result.dev_addr[3],
                       result.dev_addr[4], result.dev_addr[5],
                       result.net_state);
							if(result.attribute_len == 6)
							{
								/* 添加更新设备表到RAM中 */
								add_device((uint8_t*)result.attribute, result.dev_addr, result.net_state);
								/* 实时推送搜索到的设备到PC上位机 */
								tcp_send_search_device((uint8_t*)result.attribute, result.dev_addr, result.net_state);
							}
							else
							{
								printf("MAC地址获取失败,result.attribute_len:%d\r\n",result.attribute_len);
							}
            }
            break;
        }

        case ES1642_CMD_NOTIFY_SEARCH:
        {
            es1642_search_notify_t notify;
            status = ES1642_DecodeSearchNotify(frame, &notify);
            if (status == ES1642_STATUS_OK)
            {
                printf("有设备在进行搜索，是否参加？: 源地址=%02X:%02X:%02X:%02X:%02X:%02X, 任务ID=%d, 属性长度=%d\r\n",
                       notify.src_addr[0], notify.src_addr[1], notify.src_addr[2],
                       notify.src_addr[3], notify.src_addr[4], notify.src_addr[5],
                       notify.task_id, notify.attribute_len);
											 ES1642_SendSearch(notify.src_addr,notify.task_id,1,NULL,0);//响应搜索
            }
            break;
        }

        case ES1642_CMD_REPLY_SEARCH:
        {
            status = ES1642_DecodeEmptyResponse(frame, ES1642_CMD_REPLY_SEARCH);
            if (status == ES1642_STATUS_OK)
            {
                printf("回复设备搜索 OK\r\n");
            }
            break;
        }

        case ES1642_CMD_SET_PSK:
        {
            /* 设备设置口令的应答通常为空响应 */
            status = ES1642_DecodeEmptyResponse(frame, ES1642_CMD_SET_PSK);
            if (status == ES1642_STATUS_OK)
            {
                printf("设置PSK: OK\r\n");
            }
            break;
        }

        case ES1642_CMD_NOTIFY_PSK:
        {
            es1642_psk_notify_t psk;
            status = ES1642_DecodePskNotify(frame, &psk);
            if (status == ES1642_STATUS_OK)
            {
                printf("PSK通知: 源地址=%02X:%02X:%02X:%02X:%02X:%02X, 操作=%02X\r\n",
                       psk.src_addr[0], psk.src_addr[1], psk.src_addr[2],
                       psk.src_addr[3], psk.src_addr[4], psk.src_addr[5],
                       psk.op);
            }
            break;
        }

        case ES1642_CMD_REPORT_PSK_RESULT:
        {
            es1642_psk_result_t result;
            status = ES1642_DecodePskResult(frame, &result);
            if (status == ES1642_STATUS_OK)
            {
                /* 如果当前正在等待PSK结果，保存数据并释放信号量 */
                if (g_es1642_wait_type == ES1642_WAIT_PSK_RESULT)
                {
                    memcpy(g_es1642_psk_result.src_addr, result.src_addr, ES1642_ADDR_LEN);
                    g_es1642_psk_result.state = result.state;
                    osSemaphoreRelease(ES1642_sendHandle);
                }
            }
            break;
        }

        case ES1642_CMD_REMOTE_READ_VERSION:
        {
            es1642_remote_version_t rver;
            status = ES1642_DecodeRemoteVersion(frame, &rver);
            if (status == ES1642_STATUS_OK)
            {
                printf("远程版本: 源地址=%02X:%02X:%02X:%02X:%02X:%02X, 厂商=0x%04X, 芯片=0x%04X, 版本=0x%04X\r\n",
                       rver.src_addr[0], rver.src_addr[1], rver.src_addr[2],
                       rver.src_addr[3], rver.src_addr[4], rver.src_addr[5],
                       rver.version.vendor_id, rver.version.chip_type, rver.version.version_bcd);
            }
            break;
        }

        case ES1642_CMD_REMOTE_READ_MAC:
        {
            es1642_remote_mac_t rmac;
            status = ES1642_DecodeRemoteMac(frame, &rmac);
            if (status == ES1642_STATUS_OK)
            {
                printf("远程MAC: 源地址=%02X:%02X:%02X:%02X:%02X:%02X, MAC=%02X:%02X:%02X:%02X:%02X:%02X\r\n",
                       rmac.src_addr[0], rmac.src_addr[1], rmac.src_addr[2],
                       rmac.src_addr[3], rmac.src_addr[4], rmac.src_addr[5],
                       rmac.mac[0], rmac.mac[1], rmac.mac[2], rmac.mac[3], rmac.mac[4], rmac.mac[5]);
            }
            break;
        }

        case ES1642_CMD_REMOTE_READ_NET_PARAM:
        {
            es1642_remote_net_param_t rnet;
            status = ES1642_DecodeRemoteNetParam(frame, &rnet);
            if (status == ES1642_STATUS_OK)
            {
                printf("远程网络参数: 源地址=%02X:%02X:%02X:%02X:%02X:%02X, 中继=%d\r\n",
                       rnet.src_addr[0], rnet.src_addr[1], rnet.src_addr[2],
                       rnet.src_addr[3], rnet.src_addr[4], rnet.src_addr[5],
                       rnet.net_param.relay_depth);
            }
            break;
        }
        
        case ES1642_CMD_RECV_DATA:
        {
            es1642_recv_data_t recv_data;
            status = ES1642_DecodeRecvData(frame, &recv_data);
            if (status == ES1642_STATUS_OK)
            {
                printf("收到数据: 源地址=%02X:%02X:%02X:%02X:%02X:%02X, "
                       "长度=%d, RSSI=%d, 中继深度:%d\r\n",
                       recv_data.src_addr[0], recv_data.src_addr[1],
                       recv_data.src_addr[2], recv_data.src_addr[3],
                       recv_data.src_addr[4], recv_data.src_addr[5],
                       recv_data.user_data_len, recv_data.rssi, recv_data.relay_depth);

								/* 将响应数据拷贝到全局响应缓冲区（供 ES1642_SendUserData 读取） */
								memcpy(g_es1642_response.src_addr, recv_data.src_addr, ES1642_ADDR_LEN);

								if (recv_data.user_data_len > 0 && recv_data.user_data != NULL && recv_data.user_data_len <= ES1642_RESP_MAX_LEN)
								{
										memcpy(g_es1642_response.data, recv_data.user_data, recv_data.user_data_len);
										g_es1642_response.data_len = recv_data.user_data_len;
								}
								else
								{
										g_es1642_response.data_len = 0;
								}
								osSemaphoreRelease(ES1642_sendHandle); /* 释放信号量，解除 ES1642_SendUserData 的阻塞 */
            }
            break;
        }
        
        default:
        {
            printf("收到未知指令: 0x%02X\r\n", frame->cmd);
            break;
        }
    }
    
    /* 检查是否为异常帧 */
    if (frame->is_exception)
    {
        uint8_t exception_code;
        status = ES1642_DecodeLocalException(frame, &exception_code);
        if (status == ES1642_STATUS_OK)
        {
            printf("模块返回异常: %s\r\n", ES1642_ExceptionString(exception_code));
        }
    }
}

/* ======== 步骤4：实现错误回调函数 ======== */

/**
 * @brief  错误回调函数
 * @param  handle: ES1642驱动句柄
 * @param  status: 错误状态码
 * @param  user_arg: 用户参数（未使用）
 * @retval None
 * @note   当发生错误时，驱动会调用此函数
 */
void es1642_on_error(es1642_handle_t *handle, 
                           es1642_status_t status, 
                           void *user_arg)
{
    printf("ES1642错误: %s\r\n", ES1642_StatusString(status));
}


/* ======== 步骤6：实现模块初始化函数 ======== */

/**
 * @brief  初始化ES1642模块
 * @retval 0: 成功, -1: 失败
 */
int ES1642_InitModule(void)
{ 
    /* 配置端口参数 */
    g_es1642_handle.write = es1642_uart_write;      /* 发送回调 */
    g_es1642_handle.on_frame = es1642_on_frame_received;  /* 帧接收回调 */
    g_es1642_handle.on_error = es1642_on_error;     /* 错误回调 */
    g_es1642_handle.user_arg = NULL;                /* 用户参数 */
    
    /* 启动串口DMA+空闲中断接收 */
    if (HAL_UARTEx_ReceiveToIdle_DMA(&huart2, g_es1642_rx_buf, sizeof(g_es1642_rx_buf)) != HAL_OK)
    {
        printf("ES1642启动UART2 DMA接收失败\r\n");
        return -1;
    }
		__HAL_DMA_DISABLE_IT(huart2.hdmarx, DMA_IT_HT); // 关闭半传中断，避免重复回调
    
    printf("ES1642模块初始化成功\r\n");
    
    return 0;
}

/* ======== 步骤7：常用功能示例函数 ======== */

/**
 * @brief  读取模块版本信息
 * @retval 0: 成功, -1: 失败
 */
int ES1642_ReadVersion(void)
{
    es1642_status_t status;
    
    printf("正在读取ES1642版本信息...\r\n");
    
    status = ES1642_SendReadVersion(&g_es1642_handle);
    
    if (status != ES1642_STATUS_OK)
    {
        printf("发送读取版本命令失败: %s\r\n", ES1642_StatusString(status));
        return -1;
    }
    
    return 0;
}

/**
 * @brief  读取模块MAC地址
 * @retval 0: 成功, -1: 失败
 */
int ES1642_ReadMac(void)
{
    es1642_status_t status;
    
    printf("正在读取ES1642 MAC地址...\r\n");
    
    status = ES1642_SendReadMac(&g_es1642_handle);
    
    if (status != ES1642_STATUS_OK)
    {
        printf("发送读取MAC命令失败: %s\r\n", ES1642_StatusString(status));
        return -1;
    }
    
    return 0;
}

/**
 * @brief  读取模块地址
 * @retval 0: 成功, -1: 失败
 */
int ES1642_ReadAddr(void)
{
    es1642_status_t status;
    
    printf("正在读取ES1642地址...\r\n");
    
    status = ES1642_SendReadAddr(&g_es1642_handle);
    
    if (status != ES1642_STATUS_OK)
    {
        printf("发送读取地址命令失败: %s\r\n", ES1642_StatusString(status));
        return -1;
    }
    
    return 0;
}

/**
 * @brief  发送数据到指定设备
 * @param  dst_addr: 目标地址（6字节）
 * @param  data: 要发送的数据
 * @param  len: 数据长度
 * @param  relay_depth: 中继深度（0表示自动）
 * @retval 0: 成功, -1: 失败
 */
int ES1642_SendUserData(const uint8_t dst_addr[ES1642_ADDR_LEN], 
                        const uint8_t *data, 
                        uint16_t len,
                        uint8_t relay_depth,
                        es1642_response_t *response)
{
    es1642_status_t status;

    if (dst_addr == NULL || (len > 0 && data == NULL))
    {
        printf("参数错误\r\n");
        return -1;
    }

    /* ==============================================
    【第一步：互斥锁 —— 保证同一时刻主机只和一个从机通信，避免总线冲突】
    ==============================================*/
    if (osSemaphoreAcquire(ES1642_mutexHandle, osWaitForever) != osOK)
    {
        return -1; /* 获取不到锁 */
    }

    /* ==============================================
    【第二步：清空残留信号量】
    因为这个信号量是接收回调释放的，必须清空历史残留
    ==============================================*/
    while (osSemaphoreAcquire(ES1642_sendHandle, 0) == osOK) {}

    /* ==============================================
    【第三步：清空响应缓冲区，设置等待类型和当前通信地址】
    ==============================================*/
    memset(&g_es1642_response, 0, sizeof(g_es1642_response));
    g_es1642_wait_type = ES1642_WAIT_RECV_DATA; /* 标记当前等待的是从机数据响应 */
    Current_addr = (uint8_t*)dst_addr; /* 保存现在正在通信的从机的通信地址 */

    /* ==============================================
    【第四步：给从机发送命令】
    ==============================================*/
    printf("正在发送数据到设备...\r\n");

    /* prm=false表示发送请求（非响应） */
    status = ES1642_SendData(&g_es1642_handle, dst_addr, data, len, relay_depth, false);

    if (status != ES1642_STATUS_OK)
    {
        printf("发送数据失败: %s\r\n", ES1642_StatusString(status));
        Current_addr = NULL;
        osSemaphoreRelease(ES1642_mutexHandle);
        return -1;
    }

    /* ==============================================
    【第五步：等待从机响应】
    ==============================================*/
    printf("数据发送成功,等待从机响应\r\n");

    if (osSemaphoreAcquire(ES1642_sendHandle, pdMS_TO_TICKS(2000)) == osOK)
    {
        printf("成功接收到从机的响应\r\n");

        /* 将响应数据拷贝给调用者（如果调用者需要） */
        if (response != NULL)
        {
            memcpy(response, &g_es1642_response, sizeof(es1642_response_t));
        }

        g_es1642_wait_type = ES1642_WAIT_NONE;
        Current_addr = NULL;
        osSemaphoreRelease(ES1642_mutexHandle);
        return 0;  /* 成功：发送成功且收到响应 */
    }
    else
    {
        printf("从机地址为%02X响应超时\r\n", dst_addr[0]);
        g_es1642_wait_type = ES1642_WAIT_NONE;
        Current_addr = NULL;
        osSemaphoreRelease(ES1642_mutexHandle);
        return -2; /* 响应超时 */
    }
}

/**
 * @brief  发送广播数据
 * @param  data: 要发送的数据
 * @param  len: 数据长度
 * @retval 0: 成功, -1: 失败
 */
int ES1642_SendBroadcastData(const uint8_t *data, uint16_t len)
{
    uint8_t broadcast_addr[ES1642_ADDR_LEN];
    
    /* 设置广播地址（全0xFF） */
    ES1642_SetBroadcastAddr(broadcast_addr);
    
    /* 发送广播数据，中继深度为0（自动），不需要响应 */
    return ES1642_SendUserData(broadcast_addr, data, len, 0, NULL);
}

/**
 * @brief  启动设备搜索
 * @param  depth: 搜索深度（0=自动，1-15为具体深度）
 * @param  rule: 搜索规则
 * @retval 0: 成功, -1: 失败
 */
int ES1642_StartSearch(uint8_t depth, es1642_search_rule_t rule)
{
    es1642_status_t status;
    
		Clear_devices();//清空设备表
    printf("正在启动设备搜索...\r\n");
    
    /* 启动搜索，不携带属性数据 */
    status = ES1642_SendStartSearch(&g_es1642_handle, depth, rule, NULL, 0);
    
    if (status != ES1642_STATUS_OK)
    {
        printf("启动搜索失败: %s\r\n", ES1642_StatusString(status));
        return -1;
    }
    
    return 0;
}

/**
 * @brief  停止设备搜索
 * @retval 0: 成功, -1: 失败
 */
int ES1642_StopSearch(void)
{
    es1642_status_t status;
    
    printf("正在停止设备搜索...\r\n");
    
    status = ES1642_SendStopSearch(&g_es1642_handle);
    
    if (status != ES1642_STATUS_OK)
    {
        printf("停止搜索失败: %s\r\n", ES1642_StatusString(status));
        return -1;
    }
    
    return 0;
}

/**
 * @brief  设置模块地址
 * @param  addr: 要设置的地址（6字节）
 * @retval 0: 成功, -1: 失败
 */
int ES1642_SetModuleAddr(const uint8_t addr[ES1642_ADDR_LEN])
{
    es1642_status_t status;
    
    if (addr == NULL)
    {
        printf("参数错误\r\n");
        return -1;
    }
    
    printf("正在设置模块地址...\r\n");
    
    status = ES1642_SendSetAddr(&g_es1642_handle, addr);
    
    if (status != ES1642_STATUS_OK)
    {
        printf("设置地址失败: %s\r\n", ES1642_StatusString(status));
        return -1;
    }
    
    return 0;
}

/**
 * @brief  读取网络参数
 * @retval 0: 成功, -1: 失败
 */
int ES1642_ReadNetParam(void)
{
    es1642_status_t status;
    
    printf("正在读取网络参数...\r\n");
    
    status = ES1642_SendReadNetParam(&g_es1642_handle);
    
    if (status != ES1642_STATUS_OK)
    {
        printf("读取网络参数失败: %s\r\n", ES1642_StatusString(status));
        return -1;
    }
    
    return 0;
}

/**
 * @brief  设置网络参数
 * @param  relay_depth: 中继深度
 * @retval 0: 成功, -1: 失败
 */
int ES1642_SetNetParam(uint8_t relay_depth)
{
    es1642_status_t status;
    
    printf("正在设置网络参数...\r\n");
    
    status = ES1642_SendSetNetParam(&g_es1642_handle, relay_depth);
    
    if (status != ES1642_STATUS_OK)
    {
        printf("设置网络参数失败: %s\r\n", ES1642_StatusString(status));
        return -1;
    }
    
    return 0;
}

/**
 * @brief  重启模块
 * @retval 0: 成功, -1: 失败
 */
int ES1642_RebootModule(void)
{
    es1642_status_t status;
    
    printf("正在重启ES1642模块...\r\n");
    
    status = ES1642_SendReboot(&g_es1642_handle);
    
    if (status != ES1642_STATUS_OK)
    {
        printf("发送重启命令失败: %s\r\n", ES1642_StatusString(status));
        return -1;
    }
    
    printf("重启命令已发送\r\n");
    return 0;
}

/**
 * @brief  发送搜索回复
 * @param  src_addr: 源地址（6字节）
 * @param  task_id: 任务ID
 * @param  participate: 是否参与搜索
 * @param  attribute: 属性数据指针（可为NULL）
 * @param  attribute_len: 属性数据长度
 * @retval 0: 成功, -1: 失败
 */
int ES1642_SendSearch(const uint8_t src_addr[ES1642_ADDR_LEN],
                          uint8_t task_id,
                          bool participate,
                          const uint8_t *attribute,
                          uint8_t attribute_len)
{
    es1642_status_t status;
    
    if (src_addr == NULL)
    {
        printf("参数错误\r\n");
        return -1;
    }
    
    printf("正在发送搜索回复...\r\n");
    
    status = ES1642_SendSearchReply(&g_es1642_handle, src_addr, task_id, 
                                   participate, attribute, attribute_len);
    
    if (status != ES1642_STATUS_OK)
    {
        printf("发送搜索回复失败: %s\r\n", ES1642_StatusString(status));
        return -1;
    }
    
    printf("搜索回复发送成功\r\n");
    return 0;
}

/**
 * @brief  设置PSK并等待从机入网结果
 * @param  dst_addr: 目标地址（6字节）
 * @param  new_psk: 新的PSK指针（8字节）
 * @param  result_state: 输出参数，PSK设置结果（0=失败, 1=成功），可为NULL
 * @retval 0: 命令发送成功且收到结果, -1: 发送失败, -2: 响应超时
 * @note   流程：发送SetPsk命令 → 模块回复空应答(SET_PSK) → 
 *         从机处理入网 → 从机上报结果(REPORT_PSK_RESULT, state=0失败/1成功)
 */
int ES1642_SetPsk(const uint8_t dst_addr[ES1642_ADDR_LEN],
                 const uint8_t new_psk[ES1642_SET_PSK_LEN])
{
    es1642_status_t status;

    if (dst_addr == NULL || new_psk == NULL)
    {
        printf("参数错误\r\n");
        return -1;
    }

    /* ==============================================
    【第一步：互斥锁 —— 保证同一时刻只有一个任务操作ES1642模块】
    ==============================================*/
    if (osSemaphoreAcquire(ES1642_mutexHandle, osWaitForever) != osOK)
    {
        return -1;
    }

    /* ==============================================
    【第二步：清空残留信号量】
    ==============================================*/
    while (osSemaphoreAcquire(ES1642_sendHandle, 0) == osOK) {}

    /* ==============================================
    【第三步：设置等待类型为PSK结果，保存目标地址】
    ==============================================*/
    g_es1642_wait_type = ES1642_WAIT_PSK_RESULT;
    Current_addr = (uint8_t*)dst_addr;
    memset(&g_es1642_psk_result, 0, sizeof(g_es1642_psk_result));

    /* ==============================================
    【第四步：发送SetPsk命令】
    ==============================================*/
    printf("正在发送设置PSK命令...\r\n");

    status = ES1642_SendSetPsk(&g_es1642_handle, dst_addr, new_psk, ES1642_SET_PSK_LEN);

    if (status != ES1642_STATUS_OK)
    {
        printf("发送设置PSK命令失败: %s\r\n", ES1642_StatusString(status));
        g_es1642_wait_type = ES1642_WAIT_NONE;
        Current_addr = NULL;
        osSemaphoreRelease(ES1642_mutexHandle);
        return -1;
    }

    /* ==============================================
    【第五步：等待从机上报PSK设置结果（REPORT_PSK_RESULT）】
    注意：中间会先收到模块的空应答(SET_PSK)，但那不会释放信号量
    只有收到 REPORT_PSK_RESULT 且 wait_type == WAIT_PSK_RESULT 时才释放信号量
    ==============================================*/
    printf("设置PSK命令已发送,等待从机入网结果...\r\n");

    if (osSemaphoreAcquire(ES1642_sendHandle, pdMS_TO_TICKS(5000)) == osOK)
    {
        printf("收到PSK设置结果: 地址=%02X:%02X:%02X:%02X:%02X:%02X, state=%d\r\n",
               g_es1642_psk_result.src_addr[0], g_es1642_psk_result.src_addr[1],
               g_es1642_psk_result.src_addr[2], g_es1642_psk_result.src_addr[3],
               g_es1642_psk_result.src_addr[4], g_es1642_psk_result.src_addr[5],
               g_es1642_psk_result.state);

        if (g_es1642_psk_result.state == 0x01)
        {
            printf("从机入网成功\r\n");
            g_es1642_wait_type = ES1642_WAIT_NONE;
            Current_addr = NULL;
            osSemaphoreRelease(ES1642_mutexHandle);
            return 0;  /* 入网成功 */
        }
        else
        {
            printf("从机入网失败, state=%d\r\n", g_es1642_psk_result.state);
            g_es1642_wait_type = ES1642_WAIT_NONE;
            Current_addr = NULL;
            osSemaphoreRelease(ES1642_mutexHandle);
            return -3; /* 入网失败 */
        }
    }
    else
    {
        printf("等待PSK设置结果超时\r\n");
        g_es1642_wait_type = ES1642_WAIT_NONE;
        Current_addr = NULL;
        osSemaphoreRelease(ES1642_mutexHandle);
        return -2; /* 响应超时 */
    }
}

/**
 * @brief  远程读取版本信息
 * @param  dst_addr: 目标地址（6字节）
 * @retval 0: 成功, -1: 失败
 */
int ES1642_RemoteReadVersion(const uint8_t dst_addr[ES1642_ADDR_LEN])
{
    es1642_status_t status;
    
    if (dst_addr == NULL)
    {
        printf("参数错误\r\n");
        return -1;
    }
    
    printf("正在远程读取版本信息...\r\n");
    
    status = ES1642_SendRemoteReadVersion(&g_es1642_handle, dst_addr);
    
    if (status != ES1642_STATUS_OK)
    {
        printf("发送远程读取版本命令失败: %s\r\n", ES1642_StatusString(status));
        return -1;
    }
    
    return 0;
}

/**
 * @brief  远程读取MAC地址
 * @param  dst_addr: 目标地址（6字节）
 * @retval 0: 成功, -1: 失败
 */
int ES1642_RemoteReadMac(const uint8_t dst_addr[ES1642_ADDR_LEN])
{
    es1642_status_t status;
    
    if (dst_addr == NULL)
    {
        printf("参数错误\r\n");
        return -1;
    }
    
    printf("正在远程读取MAC地址...\r\n");
    
    status = ES1642_SendRemoteReadMac(&g_es1642_handle, dst_addr);
    
    if (status != ES1642_STATUS_OK)
    {
        printf("发送远程读取MAC命令失败: %s\r\n", ES1642_StatusString(status));
        return -1;
    }
    
    return 0;
}

/**
 * @brief  远程读取网络参数
 * @param  dst_addr: 目标地址（6字节）
 * @retval 0: 成功, -1: 失败
 */
int ES1642_RemoteReadNetParam(const uint8_t dst_addr[ES1642_ADDR_LEN])
{
    es1642_status_t status;
    
    if (dst_addr == NULL)
    {
        printf("参数错误\r\n");
        return -1;
    }
    
    printf("正在远程读取网络参数...\r\n");
    
    status = ES1642_SendRemoteReadNetParam(&g_es1642_handle, dst_addr);
    
    if (status != ES1642_STATUS_OK)
    {
        printf("发送远程读取网络参数命令失败: %s\r\n", ES1642_StatusString(status));
        return -1;
    }
    
    return 0;
}

/**
 * @brief  发送空应答
 * @param  cmd: 要应答的指令字
 * @retval 0: 成功, -1: 失败
 */
int ES1642_SendAck(uint8_t cmd)
{
    es1642_status_t status;
    
    printf("正在发送应答...\r\n");
    
    status = ES1642_SendAckEmpty(&g_es1642_handle, cmd);
    
    if (status != ES1642_STATUS_OK)
    {
        printf("发送应答失败: %s\r\n", ES1642_StatusString(status));
        return -1;
    }
    
    printf("应答发送成功\r\n");
    return 0;
}

/**
 * @brief  发送异常应答
 * @param  cmd: 要应答的指令字
 * @param  exception_code: 异常码
 * @retval 0: 成功, -1: 失败
 */
int ES1642_SendExceptionResponse(uint8_t cmd, uint8_t exception_code)
{
    es1642_status_t status;
    
    printf("正在发送异常应答...\r\n");
    
    status = ES1642_SendException(&g_es1642_handle, cmd, exception_code);
    
    if (status != ES1642_STATUS_OK)
    {
        printf("发送异常应答失败: %s\r\n", ES1642_StatusString(status));
        return -1;
    }
    
    printf("异常应答发送成功\r\n");
    return 0;
}

/* ======== 步骤10：CubeMX配置要点 ======== */

/*
 * 1. UART配置：
 *    - 选择USART1
 *    - Baud Rate: 9600
 *    - Word Length: 8 Bits
 *    - Stop Bits: 1
 *    - Parity: None
 *    - NVIC Settings: 勾选USART1 global interrupt
 * 
 * 2. DMA配置：
 *    - 添加USART1_RX
 *      * Mode: Circular（循环模式）或 Normal（配合空闲中断使用Normal模式）
 *      * Data Width: Byte
 *      * Priority: High 或 Medium
 *    - 添加USART1_TX（可选）
 *      * Mode: Normal
 *      * Data Width: Byte
 *      * Priority: High 或 Medium
 * 
 * 3. 中断优先级：
 *    - USART1中断优先级应高于其他任务中断
 *    - 建议设置为抢占优先级1-2，子优先级0
 */

/* ========================= 调试建议 ========================= */

/*
 * 四、调试和排错建议
 * ===================
 * 
 * 1. 常见问题：
 *    - 无法接收到数据：检查串口配置、DMA配置、中断优先级
 *    - 校验和错误：检查硬件连接、波特率、是否有干扰
 *    - 缓冲区溢出：检查是否有大量数据同时到达，考虑增大缓冲区
 *    - 发送失败：检查串口是否被占用、DMA配置是否正确
 * 
 * 2. 调试方法：
 *    - 使用printf打印接收到的原始数据和解析结果
 *    - 使用逻辑分析仪或示波器观察串口信号
 *    - 逐步调试：先测试简单命令（如读版本），再测试复杂功能
 *    - 检查中断是否正常触发：在中断中设置断点
 * 
 * 3. 性能优化：
 *    - DMA接收缓冲区大小可以根据实际应用调整
 *    - 频繁的数据传输可以使用循环DMA模式
 *    - 在on_frame回调中避免耗时操作，可将数据保存到队列后处理
 */

/* ========================= 注意事项 ========================= */

/*
 * 五、重要注意事项
 * ================
 * 
 * 1. 指针有效期：
 *    - frame->data、recv_data.user_data等指针指向驱动内部缓冲区
 *    - 这些指针在回调函数返回后失效，如需长期保存请自行拷贝
 * 
 * 2. 线程安全：
 *    - 如果使用RTOS，需要注意在on_frame回调中避免长时间阻塞
 *    - 建议将接收到的数据放入队列，在任务中处理
 * 
 * 3. 超时处理：
 *    - 某些命令可能没有响应，建议添加超时机制
 *    - 可以使用HAL_GetTick()实现简单的超时检测
 * 
 * 4. 错误恢复：
 *    - 收到错误后，驱动会自动重置接收状态
 *    - 应用层可以根据需要实现重发机制
 */
