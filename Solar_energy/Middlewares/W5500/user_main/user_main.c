#include "user_main.h"
#include <stdio.h>
#include <stdint.h>
#include "wizchip_conf.h"
#include "wiz_interface.h"
#include "cmsis_os.h"
#include "main.h"

/*wizchip->STM32 Hardware Pin define*/
//	wizchip_SCS    --->     STM32_GPIOD7
//	wizchip_SCLK	 --->     STM32_GPIOB13
//  wizchip_MISO	 --->     STM32_GPIOB14
//	wizchip_MOSI	 --->     STM32_GPIOB15
//	wizchip_RESET	 --->     STM32_GPIOD8
//	wizchip_INT    --->     STM32_GPIOD9

/* 网络信息 */
wiz_NetInfo default_net_info = {
    .mac = {0x00, 0x08, 0xdc, 0x12, 0x22, 0x12},
    .ip = {192, 168, 1, 110},
    .gw = {192, 168, 1, 1},
    .sn = {255, 255, 255, 0},
    .dns = {8, 8, 8, 8},
    .dhcp = NETINFO_DHCP}; // DHCP 获取 IP 地址
uint8_t ethernet_buf[ETHERNET_BUF_MAX_SIZE] = {0};
/**
 * @brief   User Run Program
 * @param   none
 * @return  none
 */
void user_run(void)
{
	wiz_NetInfo net_info;
  printf("wizchip dhcp example\r\n");

  /* wizchip init */
  wizchip_initialize();

  /* First use DHCP to obtain the Internet Protocol Address, and use a static address if the maximum number of reconnections is exceeded */
  network_init(ethernet_buf, &default_net_info);
	
	wizchip_getnetinfo(&net_info);
  printf("please try ping %d.%d.%d.%d\r\n", net_info.ip[0], net_info.ip[1], net_info.ip[2], net_info.ip[3]);

  while (1)
  {
	}
}
void W5500_Task(void *argument)
{
  /* USER CODE BEGIN W5500Taskfun */
//	wiz_NetInfo net_info;
//  printf("wizchip DHCP 示例\r\n");

//  /* wizchip 初始化 */
//  wizchip_initialize();

//  /* 首先使用 DHCP 获取互联网协议地址，如果超过最大重连次数，则使用静态地址 */
//  network_init(ethernet_buf, &default_net_info);
//	
//	wizchip_getnetinfo(&net_info);
//  printf("请尝试 ping %d.%d.%d.%d\r\n", net_info.ip[0], net_info.ip[1], net_info.ip[2], net_info.ip[3]);
  /* Infinite loop */
  for(;;)
  {
		osDelay(1);
  }
  /* USER CODE END W5500Taskfun */
}


