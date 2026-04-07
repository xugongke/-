/**
  ******************************************************************************
  * @file    rx8025t_example.h
  * @brief   RX-8025T RTC example header file
  ******************************************************************************
  * @attention
  * This header provides function prototypes for RX-8025T RTC examples
  ******************************************************************************
  */

#ifndef __RX8025T_EXAMPLE_H
#define __RX8025T_EXAMPLE_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "rx8025t.h"
/* Exported functions prototypes ---------------------------------------------*/

/**
  * @brief  Update time display on LCD
  * @retval HAL status
  */
HAL_StatusTypeDef RX8025T_UpdateTimeDisplay(void);

/**
  * @brief  Initialize RTC and display system
  * @retval HAL status
  */
HAL_StatusTypeDef RX8025T_InitAndDisplay(void);

/**
  * @brief  RTC task for periodic update (call this in main loop or timer)
  * @retval HAL status
  */
HAL_StatusTypeDef RX8025T_Task(void);

/**
  * @brief  Display formatted date and time on specific position
  * @param  x: X position
  * @param  y: Y position
  * @param  color: Text color
  * @retval HAL status
  */
HAL_StatusTypeDef RX8025T_DisplayDateTime(uint16_t x, uint16_t y, uint16_t color);

/**
  * @brief  Display simple time on specific position
  * @param  x: X position
  * @param  y: Y position
  * @param  color: Text color
  * @retval HAL status
  */
HAL_StatusTypeDef RX8025T_DisplayTime(uint16_t x, uint16_t y, uint16_t color);

/**
  * @brief  Display simple date on specific position
  * @param  x: X position
  * @param  y: Y position
  * @param  color: Text color
  * @retval HAL status
  */
HAL_StatusTypeDef RX8025T_DisplayDate(uint16_t x, uint16_t y, uint16_t color);

/**
  * @brief  Simple test function for RX-8025T
  * @retval HAL status
  */
HAL_StatusTypeDef RX8025T_Test(void);

#ifdef __cplusplus
}
#endif

#endif /* __RX8025T_EXAMPLE_H */
