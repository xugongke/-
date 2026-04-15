#include "user_main.h"
#include <stdio.h>
#include <stdint.h>
#include "wizchip_conf.h"
#include "wiz_interface.h"
#include "interrupt.h"

#include "cmsis_os.h"
#include "main.h"

/*wizchip->STM32 Hardware Pin define*/
//	wizchip_SCS    --->     STM32_GPIOD7
//	wizchip_SCLK	 --->     STM32_GPIOB13
//  wizchip_MISO	 --->     STM32_GPIOB14
//	wizchip_MOSI	 --->     STM32_GPIOB15
//	wizchip_RESET	 --->     STM32_GPIOD8
//	wizchip_INT    --->     STM32_GPIOD9

/* network information */
wiz_NetInfo default_net_info = {
    .mac = {0x00, 0x08, 0xdc, 0x12, 0x22, 0x12},
    .ip = {192, 168, 1, 139},
    .gw = {192, 168, 1, 1},
    .sn = {255, 255, 255, 0},
    .dns = {8, 8, 8, 8},
//    .dhcp = NETINFO_DHCP // dhcp get ip
    .dhcp = NETINFO_STATIC  //static ip
};

uint16_t local_port = 8080;
static uint8_t ethernet_buf[ETHERNET_BUF_MAX_SIZE] = {0};

/**
 * @brief   User Run Program
 * @param   none
 * @return  none
 */
//void user_run(void)
//{
//  printf("wizchip interrupt example\r\n");

//  /* wizchip init */
//  wizchip_initialize();

//  /* set network information */
//  network_init(ethernet_buf, &default_net_info);
//  setSIMR(0xff); // enable all socket interrupt
//  setSn_IMR(SOCKET_ID, 0x0f);
//  while (1)
//  {
//    loopback_tcps_interrupt(SOCKET_ID, ethernet_buf, local_port);
//  }
//}
void W5500_Task(void *argument)
{
  /* USER CODE BEGIN W5500Taskfun */
  printf("wizchip interrupt example\r\n");

  /* wizchip init */
  wizchip_initialize();

  /* 设置网络信息 */
  network_init(ethernet_buf, &default_net_info);
  setSIMR(0xff); // 启用所有套接字中断
  setSn_IMR(SOCKET_ID, 0x0f);
  /* Infinite loop */
  for(;;)
  {
    loopback_tcps_interrupt(SOCKET_ID, ethernet_buf, local_port);
		osDelay(1);
  }
  /* USER CODE END W5500Taskfun */
}

