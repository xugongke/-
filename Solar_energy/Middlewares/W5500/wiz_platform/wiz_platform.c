#include "wiz_platform.h"
#include "wizchip_conf.h"
#include "main.h"
#include "wiz_interface.h"
#include <stdio.h>
#include <stdint.h>
#include "FreeRTOS.h"
#include "semphr.h"

extern SPI_HandleTypeDef hspi1;
extern UART_HandleTypeDef huart1;
extern TIM_HandleTypeDef htim2;

/* W5500中断信号量 (user_main.c中创建) */
extern SemaphoreHandle_t w5500_int_sem;

/* ==================== DMA 相关定义 ==================== */

/* SPI DMA完成信号量: DMA传输完成后在中断中释放, 任务中获取 */
static SemaphoreHandle_t spi_dma_sem = NULL;

/* SPI互斥锁: 保护W5500 SPI操作的原子性(CS拉低→传输→CS拉高) */
static SemaphoreHandle_t spi_mutex = NULL;

/* DMA小数据量阈值: ≤此值用阻塞轮询, >此值用DMA传输 */
#define SPI_DMA_THRESHOLD 4

/**
 * @brief   初始化SPI DMA所需的信号量和互斥锁
 * @note    必须在FreeRTOS调度器启动后、W5500初始化前调用
 *          (由 W5500_Task 在 wizchip_initialize() 之前调用)
 */
void wiz_spi_dma_init(void)
{
    if (spi_dma_sem == NULL)
        spi_dma_sem = xSemaphoreCreateBinary();
    if (spi_mutex == NULL)
        spi_mutex = xSemaphoreCreateMutex();
}

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

/* ==================== 临界区保护 (互斥锁) ==================== */

/**
 * @brief   进入SPI临界区
 * @note    使用FreeRTOS互斥锁, 而非taskENTER_CRITICAL()
 *          因为taskENTER_CRITICAL()会关闭中断, 导致DMA完成中断无法触发,
 *          xSemaphoreTake(spi_dma_sem)会永久阻塞→死锁。
 *          互斥锁只阻塞任务调度, 中断仍然可以响应。
 */
static void wizchip_cris_enter(void)
{
    if (spi_mutex != NULL)
        xSemaphoreTake(spi_mutex, portMAX_DELAY);
}

/**
 * @brief   退出SPI临界区
 */
static void wizchip_cris_exit(void)
{
    if (spi_mutex != NULL)
        xSemaphoreGive(spi_mutex);
}

/* ==================== SPI 单字节读写 (保持阻塞) ==================== */

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

/* ==================== SPI 批量读写 (DMA优化) ==================== */

/**
 * @brief   从wizchip写入SPI缓冲区 (DMA优化)
 * @param   buf:write buff (必须位于DMA可访问内存区域 0x20000000)
 * @param   len:write len
 * @return  none
 * @note    len ≤ SPI_DMA_THRESHOLD 时使用阻塞轮询 (DMA启动开销 > 传输时间)
 *          len > SPI_DMA_THRESHOLD 时使用DMA传输, CPU在传输期间被释放
 */
void wizchip_write_buff(uint8_t *buf, uint16_t len)
{
    if (len <= SPI_DMA_THRESHOLD)
    {
        /* 小数据量直接阻塞, DMA启动开销反而更大 */
        HAL_SPI_Transmit(&hspi1, buf, len, 0xffff);
    }
    else
    {
        /* 清除可能的OVR标志, 防止TX-only DMA时RX端溢出产生虚假错误中断 */
        __HAL_SPI_CLEAR_OVRFLAG(&hspi1);
        hspi1.ErrorCode = HAL_SPI_ERROR_NONE;

        if (HAL_SPI_Transmit_DMA(&hspi1, buf, len) == HAL_OK)
        {
            /* 阻塞等待DMA发送完成 (在中断回调中释放信号量) */
            xSemaphoreTake(spi_dma_sem, portMAX_DELAY);
        }
        else
        {
            /* DMA启动失败(SPI忙或错误), 回退到阻塞方式 */
            HAL_SPI_Transmit(&hspi1, buf, len, 0xffff);
        }
    }
}

/**
 * @brief   从wizchip读取SPI缓冲区 (DMA优化)
 * @param   buf:read buff (必须位于DMA可访问内存区域 0x20000000)
 * @param   len:read len
 * @return  none
 * @note    len ≤ SPI_DMA_THRESHOLD 时使用阻塞轮询
 *          len > SPI_DMA_THRESHOLD 时使用DMA传输
 *          HAL_SPI_Receive_DMA在全双工主模式下内部自动发送dummy数据产生时钟
 */
void wizchip_read_buff(uint8_t *buf, uint16_t len)
{
    if (len <= SPI_DMA_THRESHOLD)
    {
        HAL_SPI_Receive(&hspi1, buf, len, 0xffff);
    }
    else
    {
        if (HAL_SPI_Receive_DMA(&hspi1, buf, len) == HAL_OK)
        {
            /* 阻塞等待DMA接收完成 (在中断回调中释放信号量) */
            xSemaphoreTake(spi_dma_sem, portMAX_DELAY);
        }
        else
        {
            /* DMA启动失败, 回退到阻塞方式 */
            HAL_SPI_Receive(&hspi1, buf, len, 0xffff);
        }
    }
}

/* ==================== SPI DMA中断回调 ==================== */

/**
 * @brief   DMA完成时释放信号量的内部辅助函数
 * @note    从DMA中断上下文调用, 使用FromISR版本
 */
static void spi_dma_give_semaphore(void)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xSemaphoreGiveFromISR(spi_dma_sem, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

/**
 * @brief   SPI DMA发送完成回调 (HAL自动调用, 在DMA TC中断上下文)
 */
void HAL_SPI_TxCpltCallback(SPI_HandleTypeDef *hspi)
{
    if (hspi->Instance == SPI1)
        spi_dma_give_semaphore();
}

/**
 * @brief   SPI DMA接收完成回调 (HAL自动调用, 在DMA TC中断上下文)
 */
void HAL_SPI_RxCpltCallback(SPI_HandleTypeDef *hspi)
{
    if (hspi->Instance == SPI1)
        spi_dma_give_semaphore();
}

/**
 * @brief   SPI DMA全双工收发完成回调
 * @note    某些HAL版本中HAL_SPI_Receive_DMA内部会走TransmitReceive路径,
 *          此时由TxRxCplt回调通知完成
 */
void HAL_SPI_TxRxCpltCallback(SPI_HandleTypeDef *hspi)
{
    if (hspi->Instance == SPI1)
        spi_dma_give_semaphore();
}

/**
 * @brief   SPI错误回调
 * @note    TX-only DMA在全双工SPI上会产生OVR(Overrun), 因为RX端无人读取。
 *          这里静默清除OVR, 不影响正常流程。
 */
void HAL_SPI_ErrorCallback(SPI_HandleTypeDef *hspi)
{
    if (hspi->Instance == SPI1)
    {
        __HAL_SPI_CLEAR_OVRFLAG(hspi);
        hspi->ErrorCode = HAL_SPI_ERROR_NONE;
    }
}

/* ==================== W5500 硬件复位 ==================== */

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

/* ==================== W5500 回调注册 ==================== */

/**
 * @brief   wizchip SPI 回调注册
 * @param   none
 * @return  none
 * @note    注册CS控制、SPI读写、批量读写、临界区保护回调
 */
void wizchip_spi_cb_reg(void)
{
    reg_wizchip_cs_cbfunc(wizchip_select, wizchip_deselect);
    reg_wizchip_spi_cbfunc(wizchip_read_byte, wizchip_write_byte);
    reg_wizchip_spiburst_cbfunc(wizchip_read_buff, wizchip_write_buff);
    reg_wizchip_cris_cbfunc(wizchip_cris_enter, wizchip_cris_exit);
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
        /*
         * 注意: 不需要在这里调用 __HAL_GPIO_EXTI_CLEAR_IT(GPIO_Pin),
         * 因为W5500的INTn引脚是低电平有效, 只要在tcp_client_process()中
         * 正确清除Sn_IR和IR寄存器, INTn引脚就会自动恢复高电平, EXTI中断自然消除。
         *
         * 如果Sn_IR未被清除, INTn持续为低, 调用__HAL_GPIO_EXTI_CLEAR_IT也无效,
         * EXTI会立即再次触发, 导致中断风暴。
         */

        /* 释放信号量, 唤醒W5500任务处理中断事件 */
        if (w5500_int_sem != NULL)
        {
            BaseType_t xHigherPriorityTaskWoken = pdFALSE;
            xSemaphoreGiveFromISR(w5500_int_sem, &xHigherPriorityTaskWoken);
            portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
        }
    }
}