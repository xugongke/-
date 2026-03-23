#include "user_main.h"
#include <stdio.h>
#include <stdint.h>
#include "wizchip_conf.h"
#include "wiz_interface.h"
#include "loopback.h"

#include "cmsis_os.h"
#include "main.h"

/*wizchip->STM32 Hardware Pin define*/
//	wizchip_SCS    --->     STM32_GPIOD7
//	wizchip_SCLK	 --->     STM32_GPIOB13
//  wizchip_MISO	 --->     STM32_GPIOB14
//	wizchip_MOSI	 --->     STM32_GPIOB15
//	wizchip_RESET	 --->     STM32_GPIOD8
//	wizchip_INT    --->     STM32_GPIOD9

/* Define network information */
wiz_NetInfo default_net_info = {
    .mac = {0x00, 0x08, 0xdc, 0x12, 0x22, 0x12},
    .ip = {192, 168, 1, 212},
    .gw = {192, 168, 1, 1},
    .sn = {255, 255, 255, 0},
    .dns = {8, 8, 8, 8},
    .dhcp = NETINFO_DHCP // dhcp get ip
    //.dhcp = NETINFO_STATIC  //static ip
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
//  printf("wizchip TCP Server example\r\n");

//  /* wizchip init */
//  wizchip_initialize();

//  /* set network information */
//  network_init(ethernet_buf, &default_net_info);

//  /* Enable keepalive,Parameter 2 is the keep alive time, with a unit of 5 seconds */
//  setSn_KPALVTR(SOCKET_ID, 6); // 30s keepalive
//	
//  while (1)
//	{
//			loopback_tcps(SOCKET_ID, ethernet_buf, local_port);
//	}
//}
void W5500_Task(void *argument)
{
  /* USER CODE BEGIN W5500Taskfun */
  printf("wizchip TCP Server example\r\n");

  /* wizchip init */
  wizchip_initialize();

  /* set network information */
  network_init(ethernet_buf, &default_net_info);

  /* Enable keepalive,Parameter 2 is the keep alive time, with a unit of 5 seconds */
  setSn_KPALVTR(SOCKET_ID, 6); // 30s keepalive
  /* Infinite loop */
  for(;;)
  {
		loopback_tcps(SOCKET_ID, ethernet_buf, local_port);
		osDelay(1);
  }
  /* USER CODE END W5500Taskfun */
}

