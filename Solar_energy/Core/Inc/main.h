/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32f4xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "stm32f4xx.h"
#include "stdio.h"
#include "cmsis_os.h"
#include "FreeRTOS.h"  
#include "semphr.h"    
#include <stdbool.h>
#include <string.h>
/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define Key_Down_Pin GPIO_PIN_9
#define Key_Down_GPIO_Port GPIOF
#define Key_Right_Pin GPIO_PIN_10
#define Key_Right_GPIO_Port GPIOF
#define Key_UP_Pin GPIO_PIN_0
#define Key_UP_GPIO_Port GPIOC
#define Key_Left_Pin GPIO_PIN_1
#define Key_Left_GPIO_Port GPIOC
#define Key_Enter_Pin GPIO_PIN_2
#define Key_Enter_GPIO_Port GPIOC
#define Key_Return_Pin GPIO_PIN_3
#define Key_Return_GPIO_Port GPIOC
#define ES1642_RST_Pin GPIO_PIN_2
#define ES1642_RST_GPIO_Port GPIOH
#define CHRG_Pin GPIO_PIN_3
#define CHRG_GPIO_Port GPIOH
#define STDBY_Pin GPIO_PIN_4
#define STDBY_GPIO_Port GPIOH
#define W5500_CS_Pin GPIO_PIN_4
#define W5500_CS_GPIO_Port GPIOA
#define W5500_RST_Pin GPIO_PIN_4
#define W5500_RST_GPIO_Port GPIOC
#define W5500_INT_Pin GPIO_PIN_5
#define W5500_INT_GPIO_Port GPIOC
#define W5500_INT_EXTI_IRQn EXTI9_5_IRQn
#define Key2_Pin GPIO_PIN_11
#define Key2_GPIO_Port GPIOF
#define Key1_Pin GPIO_PIN_13
#define Key1_GPIO_Port GPIOF
#define LCD_RES_Pin GPIO_PIN_14
#define LCD_RES_GPIO_Port GPIOF
#define LCD_BL_Pin GPIO_PIN_12
#define LCD_BL_GPIO_Port GPIOD
#define LED1_Pin GPIO_PIN_6
#define LED1_GPIO_Port GPIOC
#define LED2_Pin GPIO_PIN_7
#define LED2_GPIO_Port GPIOC
#define RS485_CTRL2_Pin GPIO_PIN_5
#define RS485_CTRL2_GPIO_Port GPIOB

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
