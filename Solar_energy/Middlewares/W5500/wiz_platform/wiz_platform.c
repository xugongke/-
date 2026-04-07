#include "wiz_platform.h"
#include "wizchip_conf.h"
#include "main.h"
#include "wiz_interface.h"
#include <stdio.h>
#include <stdint.h>
#include "interrupt.h" 

extern SPI_HandleTypeDef hspi1;
extern UART_HandleTypeDef huart1;
extern TIM_HandleTypeDef htim2;

/**
 * @brief   SPI 选择 wizchip
 * @param   none
 * @return  none
 */
void wizchip_select(void)
{
    HAL_GPIO_WritePin(W5500_CS_GPIO_Port, W5500_CS_Pin, GPIO_PIN_RESET);
}

/**
 * @brief   SPI 取消选择 wizchip
 * @param   none
 * @return  none
 */
void wizchip_deselect(void)
{
    HAL_GPIO_WritePin(W5500_CS_GPIO_Port, W5500_CS_Pin, GPIO_PIN_SET);
}

/**
 * @brief   SPI 写 1 字节到 wizchip
 * @param   dat:1 byte data
 * @return  none
 */
void wizchip_write_byte(uint8_t dat)
{
    HAL_SPI_Transmit(&hspi1, &dat, 1, 0xffff);
}

/**
 * @brief   SPI 从 wizchip 读取 1 字节
 * @param   none
 * @return  1 byte data
 */
uint8_t wizchip_read_byte(void)
{
    uint8_t dat;
    HAL_SPI_Receive(&hspi1, &dat, 1, 0xffff);
    return dat;
}

/**
 * @brief   从wizchip写入SPI缓冲区
 * @param   buff:write buff
 * @param   len:write len
 * @return  none
 */
void wizchip_write_buff(uint8_t *buf, uint16_t len)
{
    HAL_SPI_Transmit(&hspi1, buf, len, 0xffff);
}

/**
 * @brief   从wizchip读取SPI缓冲区
 * @param   buff:read buff
 * @param   len:read len
 * @return  none
 */
void wizchip_read_buff(uint8_t *buf, uint16_t len)
{
    HAL_SPI_Receive(&hspi1, buf, len, 0xffff);
}

/**
 * @brief   硬件复位 Wizchip
 * @param   none
 * @return  none
 */
void wizchip_reset(void)
{
    HAL_GPIO_WritePin(W5500_RST_GPIO_Port, W5500_RST_Pin, GPIO_PIN_SET);
    wiz_user_delay_ms(10);
    HAL_GPIO_WritePin(W5500_RST_GPIO_Port, W5500_RST_Pin, GPIO_PIN_RESET);
    wiz_user_delay_ms(10);
    HAL_GPIO_WritePin(W5500_RST_GPIO_Port, W5500_RST_Pin, GPIO_PIN_SET);
    wiz_user_delay_ms(10);
}

/**
 * @brief   wizchip SPI 回调注册
 * @param   none
 * @return  none
 */
void wizchip_spi_cb_reg(void)
{
    reg_wizchip_cs_cbfunc(wizchip_select, wizchip_deselect);
    reg_wizchip_spi_cbfunc(wizchip_read_byte, wizchip_write_byte);
    reg_wizchip_spiburst_cbfunc(wizchip_read_buff, wizchip_write_buff);
}


/**
 * @brief   打开wiz定时器中断
 * @param   none
 * @return  none
 */
void wiz_tim_irq_enable(void)
{
    HAL_TIM_Base_Start_IT(&htim2);
}

/**
 * @brief   关闭wiz定时器中断
 * @param   none
 * @return  none
 */
void wiz_tim_irq_disable(void)
{
    HAL_TIM_Base_Stop_IT(&htim2);
}

/**
 * @brief   INTn GPIO 中断回调
 * @param   none
 * @return  none
 */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    if (GPIO_Pin == W5500_INT_Pin)
    {
        wizchip_ISR();
        __HAL_GPIO_EXTI_CLEAR_IT(GPIO_Pin);
    }
}

