/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * File Name          : freertos.c
  * Description        : Code for freertos applications
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

/* Includes ------------------------------------------------------------------*/
#include "FreeRTOS.h"
#include "task.h"
#include "main.h"
#include "cmsis_os.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "stdio.h"
#include "message_buffer.h"
#include "lvgl.h"
#include "lv_port_disp.h"
#include "lv_port_indev.h"
#include "lv_port_fs.h"
#include "key.h"
#include "tpad.h"
#include "gui_guider.h"           // Gui Guider 生成的界面和控件的声明
#include "events_init.h"          // Gui Guider 生成的初始化事件、回调函数
#include "sdio.h"
#include "fatfs.h"
#include "custom.h"
#include "sdcard.h"
#include "user_status.h"
#include "a7680c.h"
#include "a7680c_at.h"
#include "a7680c_mqtt.h"
#include "rx8025t_example.h"
#include "es1642.h"
#include "es1642_usage_guide.h"
lv_ui  guider_ui;                     // 定义 界面对象
extern lv_group_t * g_keypad_group;		//声明全局group
extern SemaphoreHandle_t w5500_int_semaphore;
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
MessageBufferHandle_t uart2Message;//消息缓冲区句柄
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */
// 注意: 本函数需要在f_mount()执行后再调用，因为CubeMX生成的FatFs代码, 会在f_mount()函数内对SD卡进行初始化
void SDCardInfo(void)
{
    HAL_SD_CardInfoTypeDef pCardInfo = {0};                    // SD卡信息结构体
    uint8_t status = HAL_SD_GetCardState(&hsd);                // SD卡状态标志值
    if (status == HAL_SD_CARD_TRANSFER)
    {
        HAL_SD_GetCardInfo(&hsd, &pCardInfo);                  // 获取 SD 卡的信息
        printf("\r\n");
        printf("*** 获取SD卡信息 *** \r\n");
        printf("卡类型：%d \r\n", pCardInfo.CardType);         // 类型返回：0-SDSC、1-SDHC/SDXC、3-SECURED
        printf("卡版本：%d \r\n", pCardInfo.CardVersion);      // 版本返回：0-CARD_V1、1-CARD_V2
        printf("块数量：%d \r\n", pCardInfo.BlockNbr);         // 可用的块数量
        printf("块大小：%d \r\n", pCardInfo.BlockSize);        // 每个块的大小; 单位：字节
        printf("卡容量：%lluMB \r\n", ((uint64_t)pCardInfo.BlockSize * pCardInfo.BlockNbr) / 1024 / 1024);  // 计算卡的容量; 单位：GB
    }
		else
		{
			printf("SD卡通信失败\r\n");
		}
}

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN Variables */

/* USER CODE END Variables */
/* Definitions for defaultTask */
osThreadId_t defaultTaskHandle;
const osThreadAttr_t defaultTask_attributes = {
  .name = "defaultTask",
  .stack_size = 128 * 4,
  .priority = (osPriority_t) osPriorityLow2,
};
/* Definitions for LVGLTask */
osThreadId_t LVGLTaskHandle;
const osThreadAttr_t LVGLTask_attributes = {
  .name = "LVGLTask",
  .stack_size = 1024 * 4,
  .priority = (osPriority_t) osPriorityHigh,
};
/* Definitions for A7680CTask */
osThreadId_t A7680CTaskHandle;
const osThreadAttr_t A7680CTask_attributes = {
  .name = "A7680CTask",
  .stack_size = 512 * 4,
  .priority = (osPriority_t) osPriorityLow1,
};
/* Definitions for W5500Task */
osThreadId_t W5500TaskHandle;
const osThreadAttr_t W5500Task_attributes = {
  .name = "W5500Task",
  .stack_size = 512 * 4,
  .priority = (osPriority_t) osPriorityLow4,
};
/* Definitions for ES1642Task */
osThreadId_t ES1642TaskHandle;
const osThreadAttr_t ES1642Task_attributes = {
  .name = "ES1642Task",
  .stack_size = 512 * 4,
  .priority = (osPriority_t) osPriorityLow,
};

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */

/* USER CODE END FunctionPrototypes */

void StartDefaultTask(void *argument);
void lvgl_task(void *argument);
void A7680C_Task(void *argument);
extern void W5500_Task(void *argument);
void ES1642_Task(void *argument);

void MX_FREERTOS_Init(void); /* (MISRA C 2004 rule 8.1) */

/* Hook prototypes */
void vApplicationTickHook(void);

/* USER CODE BEGIN 3 */
__weak void vApplicationTickHook( void )
{
   /* This function will be called by each tick interrupt if
   configUSE_TICK_HOOK is set to 1 in FreeRTOSConfig.h. User code can be
   added here, but the tick hook is called from an interrupt context, so
   code must not attempt to block, and only the interrupt safe FreeRTOS API
   functions can be used (those that end in FromISR()). */
}
/* USER CODE END 3 */

/**
  * @brief  FreeRTOS initialization
  * @param  None
  * @retval None
  */
void MX_FREERTOS_Init(void) {
  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* USER CODE BEGIN RTOS_MUTEX */
  /* add mutexes, ... */
  /* USER CODE END RTOS_MUTEX */

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  /* Create W5500 interrupt semaphore for task synchronization */
  w5500_int_semaphore = xSemaphoreCreateBinary();
  if (w5500_int_semaphore == NULL)
  {
    Error_Handler();
  }
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
  /* start timers, add new ones, ... */
  /* USER CODE END RTOS_TIMERS */

  /* USER CODE BEGIN RTOS_QUEUES */
  /* add queues, ... */
	uart2Message = xMessageBufferCreate(ES1642_RX_BUF_SIZE);
	if(uart2Message == NULL)
	{
			// 创建失败（内存不足）
			Error_Handler();
	}
  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* creation of defaultTask */
  defaultTaskHandle = osThreadNew(StartDefaultTask, NULL, &defaultTask_attributes);

  /* creation of LVGLTask */
  LVGLTaskHandle = osThreadNew(lvgl_task, NULL, &LVGLTask_attributes);

  /* creation of A7680CTask */
  A7680CTaskHandle = osThreadNew(A7680C_Task, NULL, &A7680CTask_attributes);

  /* creation of W5500Task */
  W5500TaskHandle = osThreadNew(W5500_Task, NULL, &W5500Task_attributes);

  /* creation of ES1642Task */
  ES1642TaskHandle = osThreadNew(ES1642_Task, NULL, &ES1642Task_attributes);

  /* USER CODE BEGIN RTOS_THREADS */
  /* add threads, ... */
  /* USER CODE END RTOS_THREADS */

  /* USER CODE BEGIN RTOS_EVENTS */
  /* add events, ... */
  /* USER CODE END RTOS_EVENTS */

}

/* USER CODE BEGIN Header_StartDefaultTask */
/**
  * @brief  Function implementing the defaultTask thread.
  * @param  argument: Not used
  * @retval None
  */
/* USER CODE END Header_StartDefaultTask */
void StartDefaultTask(void *argument)
{
  /* USER CODE BEGIN StartDefaultTask */
  /* Infinite loop */
  for(;;)
  {
		HAL_GPIO_TogglePin(LED1_GPIO_Port, LED1_Pin);
		HAL_GPIO_TogglePin(LED2_GPIO_Port, LED2_Pin);
    osDelay(1000);
  }
  /* USER CODE END StartDefaultTask */
}

/* USER CODE BEGIN Header_lvgl_task */
/**
* @brief Function implementing the LVGLTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_lvgl_task */
void lvgl_task(void *argument)
{
  /* USER CODE BEGIN lvgl_task */
	lv_init();  						/*初始化LVGL图形库*/
	g_keypad_group = lv_group_create();//给全局group分配空间
	lv_port_disp_init();		/* lvgl显示接口初始化,放在lv_init()的后面 */	
	lv_port_indev_init();   /* lvgl输入接口初始化,放在lv_init()的后面 */	
	lv_port_fs_init();   		/* lvgl文件系统接口初始化,放在lv_init()的后面 */	
//	SDCardInfo();//读取SD卡基本信息,在挂载之后执行
	printf("LVGL Version: %d.%d.%d\r\n", LVGL_VERSION_MAJOR, LVGL_VERSION_MINOR, LVGL_VERSION_PATCH);
//	if(f_mkdir("0:/SYS") == FR_OK)
//	{
//		printf("SYS创建成功\r\n");
//	}
//	if(f_mkdir("0:/LOG") == FR_OK)
//	{
//		printf("LOG创建成功\r\n");
//	}
//	if(f_mkdir("0:/USER") == FR_OK)
//	{
//		printf("USER创建成功\r\n");
//	}
//	for(uint32_t i = 1;i <=50;i++)
//	{
//		Create_der(i);
//		Write_temperature(i);
//		Write_power(i);
//		Write_yongpower(i);
//	}
	
	/* USER CODE END 2 */
	//自己设计的图形窗口
	setup_ui(&guider_ui);           // 初始化 UI
	events_init(&guider_ui);       // 初始化 事件
	custom_init(&guider_ui);			// 你自己的逻辑
  /* Infinite loop */
  for(;;)
  {
		lv_timer_handler();  // 处理 LVGL 任务
		osDelay(5);
  }
  /* USER CODE END lvgl_task */
}

/* USER CODE BEGIN Header_A7680C_Task */
/**
* @brief Function implementing the A7680CTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_A7680C_Task */
void A7680C_Task(void *argument)
{
  /* USER CODE BEGIN A7680C_Task */
//	A7680C_Init();
//	if(A7680C_Test())
//	{
//			printf("A7680C通信正常\r\n");
//	}
//	else
//	{
//		printf("A7680C通信失败\r\n");
//	}
	if (RX8025T_InitAndDisplay() == HAL_OK) 
	{
//		printf("外部RTC时钟通信成功\r\n");
	}
	else
	{
		printf("外部RTC时钟通信失败\r\n");
	}
  /* Infinite loop */
  for(;;)
  {
		RX8025T_Task();
		osDelay(1);
  }
  /* USER CODE END A7680C_Task */
}

/* USER CODE BEGIN Header_ES1642_Task */
/**
* @brief Function implementing the ES1642Task thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_ES1642_Task */
void ES1642_Task(void *argument)
{
  /* USER CODE BEGIN ES1642_Task */
	static uint8_t buf[ES1642_MAX_FRAME_LEN];
	// 初始化ES1642模块
	if (ES1642_InitModule() != 0)
	{
			Error_Handler();
	}
  /* Infinite loop */
  for(;;)
  {
		//等待消息缓冲区有消息包，然后读取一包数据,xMessageBufferReceive会释放cpu
		size_t n = xMessageBufferReceive(uart2Message, buf, sizeof(buf), portMAX_DELAY);
//		printf("接收协议帧:");
//		for(int i = 0;i < n;i++)
//		{
//			printf("%#x ",buf[i]);
//		}
//		printf("\r\n");
		if(n)
		{
				ES1642_ProcessCompleteFrame(&g_es1642_handle,buf,n);
		}
  }
  /* USER CODE END ES1642_Task */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */

/* USER CODE END Application */

