/**
 * @file    host_comm.c
 * @brief   上位机通信接口 - TCP(W5500网口) 和 RS485(USART6串口) 统一管理
 *
 * 本文件集中管理所有与上位机通信相关的函数，包括：
 * 1. TCP版本上位机通信函数（通过W5500网口）- socket send/recv
 * 2. RS485版本上位机通信函数（通过USART6串口）- HAL_UART_Transmit + 方向控制
 * 3. 共用的辅助函数（MAC/地址字符串转换等）
 */

#include "host_comm.h"
#include "main.h"
#include "usart.h"
#include "socket.h"
#include "device_manager.h"
#include "es1642_usage_guide.h"
#include "cmsis_os.h"
#include <stdio.h>
#include <string.h>

/* ===================== 共用辅助函数 ===================== */

void host_mac_to_string(const uint8_t mac[6], char *out)
{
    sprintf(out, "%02X:%02X:%02X:%02X:%02X:%02X",
            mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

void host_addr_to_string(const uint8_t addr[6], char *out)
{
    sprintf(out, "%02X:%02X:%02X:%02X:%02X:%02X",
            addr[0], addr[1], addr[2], addr[3], addr[4], addr[5]);
}

int host_parse_mac_string(const char *str, uint8_t mac[6])
{
    unsigned int tmp[6];
    if (sscanf(str, "%2x:%2x:%2x:%2x:%2x:%2x",
               &tmp[0], &tmp[1], &tmp[2], &tmp[3], &tmp[4], &tmp[5]) != 6)
    {
        return -1;
    }
    for (int i = 0; i < 6; i++)
    {
        mac[i] = (uint8_t)tmp[i];
    }
    return 0;
}

/* ================================================================ */
/* ===================== TCP上位机通信函数 ========================= */
/* ================================================================ */

/* 搜索设备时保存当前TCP连接的socket编号，用于实时推送搜索结果 */
static int8_t g_tcp_search_sn = -1;

/* -------------------- 搜索socket管理 -------------------- */

void tcp_set_search_socket(uint8_t sn)
{
    g_tcp_search_sn = (int8_t)sn;
}

void tcp_clear_search_socket(void)
{
    g_tcp_search_sn = -1;
}

/* -------------------- TCP发送设备列表 -------------------- */

void tcp_send_device_list(uint8_t sn)
{
    char line[128];
    char mac_str[32];
    char addr_str[32];

    sprintf(line, "COUNT,%u\n", device_count);
    send(sn, (uint8_t *)line, strlen(line));

    for (uint16_t i = 0; i < device_count; i++)
    {
        host_mac_to_string(device_list[i].mac, mac_str);
        host_addr_to_string(device_list[i].addr, addr_str);
        sprintf(line, "DEV,%s,%s,%u\n", mac_str, addr_str, device_list[i].valid);
        send(sn, (uint8_t *)line, strlen(line));
    }
    send(sn, (uint8_t *)"END\n", sizeof("END\n"));
}

/* -------------------- TCP处理绑定命令 -------------------- */

void tcp_handle_bind_cmd(uint8_t sn, const char *cmd)
{
    /* 命令格式: BIND,AA:BB:CC:DD:EE:FF,2,1,301 */
    char mac_str[32] = {0};
    int building = 0;
    int unit = 0;
    int room = 0;
    uint8_t mac[6];
    uint8_t addr[6];

    if (sscanf(cmd, "BIND,%31[^,],%d,%d,%d", mac_str, &building, &unit, &room) != 4)
    {
        send(sn, (uint8_t *)"ERR,命令格式错误\n", sizeof("ERR,命令格式错误\n"));
        return;
    }

    if (host_parse_mac_string(mac_str, mac) != 0)
    {
        send(sn, (uint8_t *)"ERR,MAC格式错误\n", sizeof("ERR,MAC格式错误\n"));
        return;
    }

    /* 将用户住址转换成通信地址 */
    make_addr(addr, (uint8_t)building, (uint8_t)unit, (uint16_t)room);

    /* 判断通信地址是否改变，并设置通信地址后入网 */
    int ret = update_device(mac, addr);
    switch (ret)
    {
        case 1:
            send(sn, (uint8_t *)"设备列表中没有该设备，请重新搜索设备\r\n",
                 sizeof("设备列表中没有该设备，请重新搜索设备\r\n"));
            break;
        case 2:
            send(sn, (uint8_t *)"从机es1642模块损坏，请更换模块\r\n",
                 sizeof("从机es1642模块损坏，请更换模块\r\n"));
            break;
        case 3:
            send(sn, (uint8_t *)"从机修改通信地址超时，请检查从机状态\r\n",
                 sizeof("从机修改通信地址超时，请检查从机状态\r\n"));
            break;
        case 4:
            send(sn, (uint8_t *)"发送修改通信地址命令失败\r\n",
                 sizeof("发送修改通信地址命令失败\r\n"));
            break;
        case 5:
            send(sn, (uint8_t *)"从机入网响应超时\r\n",
                 sizeof("从机入网响应超时\r\n"));
            break;
        case 6:
            send(sn, (uint8_t *)"从机入网失败\r\n",
                 sizeof("从机入网失败\r\n"));
            break;
        case 7:
            send(sn, (uint8_t *)"发送入网命令失败\r\n",
                 sizeof("发送入网命令失败\r\n"));
            break;
        default:
            break;
    }

    save_devices();
    send(sn, (uint8_t *)"OK\r\n", sizeof("OK\r\n"));
}

/* -------------------- TCP搜索结果推送 -------------------- */

void tcp_send_search_device(const uint8_t mac[6], const uint8_t addr[6], uint8_t valid)
{
    char line[128];
    char mac_str[32];
    char addr_str[32];

    if (g_tcp_search_sn < 0)
    {
        return;  /* 没有活跃的TCP连接 */
    }

    host_mac_to_string(mac, mac_str);
    host_addr_to_string(addr, addr_str);
    sprintf(line, "SEARCH_DEV,%s,%s,%u\n", mac_str, addr_str, valid);
    send((uint8_t)g_tcp_search_sn, (uint8_t *)line, strlen(line));
}

void tcp_send_search_ok(void)
{
    if (g_tcp_search_sn < 0)
    {
        return;
    }

    send((uint8_t)g_tcp_search_sn, (uint8_t *)"OK\r\n", strlen("OK\r\n"));
}

void tcp_send_search_done(void)
{
    if (g_tcp_search_sn < 0)
    {
        return;
    }
    send((uint8_t)g_tcp_search_sn, (uint8_t *)"SEARCH_DONE\n", strlen("SEARCH_DONE\n"));
    tcp_clear_search_socket();
}

/* ================================================================ */
/* ==================== RS485上位机通信函数 ======================== */
/* ================================================================ */

/* RS485搜索活跃标志 */
static uint8_t g_rs485_search_active = 0;

/* -------------------- RS485基础发送函数 -------------------- */

void rs485_usart6_send(const uint8_t *data, uint16_t len)
{
    /* 切换RS485为发送模式 */
    HAL_GPIO_WritePin(RS485_CTRL1_GPIO_Port, RS485_CTRL1_Pin, GPIO_PIN_SET);

    /* 等待方向切换稳定 */
    osDelay(1);

    /* 发送数据 */
    HAL_UART_Transmit(&huart6, (uint8_t *)data, len, HAL_MAX_DELAY);

    /* 等待发送完成 */
    osDelay(1);

    /* 切换RS485回接收模式 */
    HAL_GPIO_WritePin(RS485_CTRL1_GPIO_Port, RS485_CTRL1_Pin, GPIO_PIN_RESET);
}

/* -------------------- RS485发送设备列表 -------------------- */

void rs485_send_device_list(void)
{
    char line[128];
    char mac_str[32];
    char addr_str[32];

    sprintf(line, "COUNT,%u\n", device_count);
    rs485_usart6_send((const uint8_t *)line, strlen(line));

    for (uint16_t i = 0; i < device_count; i++)
    {
        host_mac_to_string(device_list[i].mac, mac_str);
        host_addr_to_string(device_list[i].addr, addr_str);
        sprintf(line, "DEV,%s,%s,%u\n", mac_str, addr_str, device_list[i].valid);
        rs485_usart6_send((const uint8_t *)line, strlen(line));
    }
    rs485_usart6_send((const uint8_t *)"END\n", strlen("END\n"));
}

/* -------------------- RS485处理绑定命令 -------------------- */

void rs485_handle_bind_cmd(const char *cmd)
{
    /* 命令格式: BIND,AA:BB:CC:DD:EE:FF,2,1,301 */
    char mac_str[32] = {0};
    int building = 0;
    int unit = 0;
    int room = 0;
    uint8_t mac[6];
    uint8_t addr[6];

    if (sscanf(cmd, "BIND,%31[^,],%d,%d,%d", mac_str, &building, &unit, &room) != 4)
    {
        rs485_usart6_send((const uint8_t *)"ERR,命令格式错误\n", strlen("ERR,命令格式错误\n"));
        return;
    }

    if (host_parse_mac_string(mac_str, mac) != 0)
    {
        rs485_usart6_send((const uint8_t *)"ERR,MAC格式错误\n", strlen("ERR,MAC格式错误\n"));
        return;
    }

    /* 将用户住址转换成通信地址 */
    make_addr(addr, (uint8_t)building, (uint8_t)unit, (uint16_t)room);

    /* 判断通信地址是否改变，并设置通信地址后入网 */
    int ret = update_device(mac, addr);
    switch (ret)
    {
        case 1:
            rs485_usart6_send((const uint8_t *)"ERR,设备列表中没有该设备，请重新搜索设备\n",
                             strlen("ERR,设备列表中没有该设备，请重新搜索设备\n"));
            break;
        case 2:
            rs485_usart6_send((const uint8_t *)"ERR,从机es1642模块损坏，请更换模块\n",
                             strlen("ERR,从机es1642模块损坏，请更换模块\n"));
            break;
        case 3:
            rs485_usart6_send((const uint8_t *)"ERR,从机修改通信地址超时，请检查从机状态\n",
                             strlen("ERR,从机修改通信地址超时，请检查从机状态\n"));
            break;
        case 4:
            rs485_usart6_send((const uint8_t *)"ERR,发送修改通信地址命令失败\n",
                             strlen("ERR,发送修改通信地址命令失败\n"));
            break;
        case 5:
            rs485_usart6_send((const uint8_t *)"ERR,从机入网响应超时\n",
                             strlen("ERR,从机入网响应超时\n"));
            break;
        case 6:
            rs485_usart6_send((const uint8_t *)"ERR,从机入网失败\n",
                             strlen("ERR,从机入网失败\n"));
            break;
        case 7:
            rs485_usart6_send((const uint8_t *)"ERR,发送入网命令失败\n",
                             strlen("ERR,发送入网命令失败\n"));
            break;
        default:
            break;
    }

    save_devices();
    rs485_usart6_send((const uint8_t *)"OK\r\n", strlen("OK\r\n"));
}

/* -------------------- RS485搜索状态管理 -------------------- */

void rs485_set_search_active(void)
{
    g_rs485_search_active = 1;
}

void rs485_clear_search_active(void)
{
    g_rs485_search_active = 0;
}

/* -------------------- RS485搜索结果推送 -------------------- */

void rs485_send_search_device(const uint8_t mac[6], const uint8_t addr[6], uint8_t valid)
{
    char line[128];
    char mac_str[32];
    char addr_str[32];

    if (!g_rs485_search_active)
    {
        return;  /* RS485没有活跃的搜索请求 */
    }

    host_mac_to_string(mac, mac_str);
    host_addr_to_string(addr, addr_str);
    sprintf(line, "SEARCH_DEV,%s,%s,%u\n", mac_str, addr_str, valid);
    rs485_usart6_send((const uint8_t *)line, strlen(line));
}

void rs485_send_search_ok(void)
{
    if (!g_rs485_search_active)
    {
        return;
    }

    rs485_usart6_send((const uint8_t *)"OK\r\n", strlen("OK\r\n"));
}

void rs485_send_search_done(void)
{
    if (!g_rs485_search_active)
    {
        return;
    }

    rs485_usart6_send((const uint8_t *)"SEARCH_DONE\n", strlen("SEARCH_DONE\n"));
    rs485_clear_search_active();
}
