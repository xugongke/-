#ifndef __WIZ_PLATFORM_H__
#define __WIZ_PLATFORM_H__

#include <stdint.h>

/**
 * @brief   hardware reset wizchip
 * @param   none
 * @return  none
 */
void wizchip_reset(void);

/**
 * @brief   Register the WIZCHIP SPI callback function
 * @param   none
 * @return  none
 */
void wizchip_spi_cb_reg(void);

/**
 * @brief   初始化SPI DMA所需的信号量和互斥锁
 * @note    必须在FreeRTOS调度器启动后、W5500初始化前调用
 */
void wiz_spi_dma_init(void);

/**
 * @brief   Turn on wiz timer interrupt
 * @param   none
 * @return  none
 */
void wiz_tim_irq_enable(void);

/**
 * @brief   Turn off wiz timer interrupt
 * @param   none
 * @return  none
 */
void wiz_tim_irq_disable(void);
#endif
