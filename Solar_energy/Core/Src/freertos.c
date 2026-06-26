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
#include "gui_guider.h"           // Gui Guider 生成的界面和控件的声明
#include "events_init.h"          // Gui Guider 生成的初始化事件、回调函数
#include "sdio.h"
#include "fatfs.h"
#include "custom.h"
#include "a7680c.h"
#include "a7680c_netmgr.h"
#include "a7680c_at.h"
#include "a7680c_mqtt.h"
#include "rx8025t_example.h"
#include "es1642.h"
#include "es1642_usage_guide.h"
#include "device_manager.h"
#include "user_data_manager.h"
#include "a7680c_http.h"
#include "rs485_usart.h"
#include "battery.h"
#include "mppt.h"
#include "user_main.h"
lv_ui  guider_ui;                     // 定义 界面对象
extern lv_group_t * g_keypad_group;		//声明全局group
WeatherCurrent_t weather_data = {0};//存储天气代码的结构体

/* LVGL互斥锁: 保护多任务对LVGL API的并发访问 */
osMutexId_t lvgl_mutex = NULL;
/* 文件系统互斥锁: 保护共享SDFile/SDFatFS的并发访问 */
osMutexId_t fs_mutex = NULL;
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
MessageBufferHandle_t uart2Message;//es1642消息缓冲区句柄
MessageBufferHandle_t uart3Message;//a7680c消息缓冲区句柄
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */
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
}
  /* USER CODE END StartDefaultTask */


/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN Variables */

/* USER CODE END Variables */
/* Definitions for defaultTask */
osThreadId_t defaultTaskHandle;
const osThreadAttr_t defaultTask_attributes = {
  .name = "defaultTask",
  .stack_size = 512 * 4,
  .priority = (osPriority_t) osPriorityBelowNormal3,
};
/* Definitions for LVGLTask */
osThreadId_t LVGLTaskHandle;
const osThreadAttr_t LVGLTask_attributes = {
  .name = "LVGLTask",
  .stack_size = 1024 * 4,
  .priority = (osPriority_t) osPriorityAboveNormal3,
};
/* Definitions for A7680CTask */
osThreadId_t A7680CTaskHandle;
const osThreadAttr_t A7680CTask_attributes = {
  .name = "A7680CTask",
  .stack_size = 512 * 4,
  .priority = (osPriority_t) osPriorityNormal3,
};
/* Definitions for W5500Task */
osThreadId_t W5500TaskHandle;
const osThreadAttr_t W5500Task_attributes = {
  .name = "W5500Task",
  .stack_size = 512 * 4,
  .priority = (osPriority_t) osPriorityBelowNormal1,
};
/* Definitions for ES1642Task */
osThreadId_t ES1642TaskHandle;
const osThreadAttr_t ES1642Task_attributes = {
  .name = "ES1642Task",
  .stack_size = 512 * 4,
  .priority = (osPriority_t) osPriorityNormal5,
};
/* Definitions for RTCTask */
osThreadId_t RTCTaskHandle;
const osThreadAttr_t RTCTask_attributes = {
  .name = "RTCTask",
  .stack_size = 512 * 4,
  .priority = (osPriority_t) osPriorityBelowNormal7,
};
/* Definitions for WeatherTask */
osThreadId_t WeatherTaskHandle;
const osThreadAttr_t WeatherTask_attributes = {
  .name = "WeatherTask",
  .stack_size = 512 * 4,
  .priority = (osPriority_t) osPriorityLow5,
};
/* Definitions for RS485UARTProces */
osThreadId_t RS485UARTProcesHandle;
const osThreadAttr_t RS485UARTProces_attributes = {
  .name = "RS485UARTProces",
  .stack_size = 512 * 4,
  .priority = (osPriority_t) osPriorityAboveNormal5,
};
/* Definitions for DevicePollTask */
osThreadId_t DevicePollTaskHandle;
const osThreadAttr_t DevicePollTask_attributes = {
  .name = "DevicePollTask",
  .stack_size = 640 * 4,
  .priority = (osPriority_t) osPriorityBelowNormal5,
};
/* Definitions for weatherTimer */
osTimerId_t weatherTimerHandle;
const osTimerAttr_t weatherTimer_attributes = {
  .name = "weatherTimer"
};
/* Definitions for at_sem */
osSemaphoreId_t at_semHandle;
const osSemaphoreAttr_t at_sem_attributes = {
  .name = "at_sem"
};
/* Definitions for at_mutex */
osSemaphoreId_t at_mutexHandle;
const osSemaphoreAttr_t at_mutex_attributes = {
  .name = "at_mutex"
};
/* Definitions for ES1642_send */
osSemaphoreId_t ES1642_sendHandle;
const osSemaphoreAttr_t ES1642_send_attributes = {
  .name = "ES1642_send"
};
/* Definitions for ES1642_mutex */
osSemaphoreId_t ES1642_mutexHandle;
const osSemaphoreAttr_t ES1642_mutex_attributes = {
  .name = "ES1642_mutex"
};

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */

/* USER CODE END FunctionPrototypes */

void StartDefaultTask(void *argument);
void lvgl_task(void *argument);
void A7680C_Task(void *argument);
extern void W5500_Task(void *argument);
void ES1642_Task(void *argument);
void RTC_Task(void *argument);
void Weather_Task(void *argument);
extern void RS485_UART_ProcessTask(void *argument);
void DevicePoll_Task(void *argument);
void Callback01(void *argument);

void MX_FREERTOS_Init(void); /* (MISRA C 2004 rule 8.1) */

/* Hook prototypes */
void vApplicationTickHook(void);

/* USER CODE BEGIN 3 */
__weak void vApplicationTickHook( void )
{
   /* This function will be called by each tick interrupt if
   configUSE_TICK_HOOK is set to 1 in FreeRTOSConfig.h. User code can be
   added here, but the code must not attempt to block, and only the interrupt safe FreeRTOS API
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
  /* 创建LVGL互斥锁 */
  lvgl_mutex = osMutexNew(NULL);
  /* 创建文件系统互斥锁 */
  fs_mutex = osMutexNew(NULL);
  /* USER CODE END RTOS_MUTEX */

  /* Create the semaphores(s) */
  /* creation of at_sem */
  at_semHandle = osSemaphoreNew(1, 0, &at_sem_attributes);

  /* creation of at_mutex */
  at_mutexHandle = osSemaphoreNew(1, 1, &at_mutex_attributes);

  /* creation of ES1642_send */
  ES1642_sendHandle = osSemaphoreNew(1, 0, &ES1642_send_attributes);

  /* creation of ES1642_mutex */
  ES1642_mutexHandle = osSemaphoreNew(1, 1, &ES1642_mutex_attributes);

  /* USER CODE BEGIN RTOS_SEMAPHORES */
	
  /* USER CODE END RTOS_SEMAPHORES */

  /* Create the timer(s) */
  /* creation of weatherTimer */
  weatherTimerHandle = osTimerNew(Callback01, osTimerOnce, NULL, &weatherTimer_attributes);

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
	uart3Message = xMessageBufferCreate(AT_MSG_BUFFER_SIZE);
	if(uart3Message == NULL)
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

  /* creation of RTCTask */
  RTCTaskHandle = osThreadNew(RTC_Task, NULL, &RTCTask_attributes);

  /* creation of WeatherTask */
  WeatherTaskHandle = osThreadNew(Weather_Task, NULL, &WeatherTask_attributes);

  /* creation of RS485UARTProces */
  RS485UARTProcesHandle = osThreadNew(RS485_UART_ProcessTask, NULL, &RS485UARTProces_attributes);

  /* creation of DevicePollTask */
  DevicePollTaskHandle = osThreadNew(DevicePoll_Task, NULL, &DevicePollTask_attributes);

  /* USER CODE BEGIN RTOS_THREADS */
  /* add threads, ... */
  /* USER CODE END RTOS_THREADS */

  /* USER CODE BEGIN RTOS_EVENTS */
  /* add events, ... */
  /* USER CODE END RTOS_EVENTS */

}

/* USER CODE BEGIN Header_StartDefaultTask */
/**
  * @brief  A7680C模块管理任务
  * @param  argument: Not used
  * @retval None
  * @note   网络初始化/健康检查逻辑已封装到 a7680c_netmgr.c
  */
/* USER CODE END Header_StartDefaultTask */
void StartDefaultTask(void *argument)
{
  /* USER CODE BEGIN StartDefaultTask */
	A7680C_NetManager_Init(weatherTimerHandle);

  /* Infinite loop */
  for(;;)
  {
		HAL_GPIO_TogglePin(LED1_GPIO_Port, LED1_Pin);
		HAL_GPIO_TogglePin(LED2_GPIO_Port, LED2_Pin);

		uint32_t delay = A7680C_NetManager_Process();
		if (delay > 0)
		{
			osDelay(delay);
		}
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
	printf("LVGL Version: %d.%d.%d\r\n", LVGL_VERSION_MAJOR, LVGL_VERSION_MINOR, LVGL_VERSION_PATCH);
	
	// 文件系统初始化完成之后读取设备表文件加载到RAM中
	device_manager_init();

	// 从SD卡加载所有已入网设备的用电量数据到RAM缓存（在UI显示之前完成）
	user_detail_cache_init();

	// 从SD卡加载太阳能发电量数据到RAM（文件系统初始化完成后）
	solar_energy_init();

	// 从TF卡读取TCP服务器IP/Port配置 (在W5500_Task连接之前完成)
	tcp_config_load();

	/* USER CODE END 2 */
	//自己设计的图形窗口
	setup_ui(&guider_ui);           // 初始化 UI
	events_init(&guider_ui);       // 初始化 事件
	custom_init(&guider_ui);			// 你自己的逻辑

	/* UI初始化完成，通知RTC_Task可以操作LVGL了 */
	osThreadFlagsSet(RTCTaskHandle, 0x02);
	printf("LVGL UI初始化完成\r\n");

  /* Infinite loop */
  for(;;)
  {
		if(lvgl_mutex) osMutexAcquire(lvgl_mutex, osWaitForever);
		lv_timer_handler();  // 处理 LVGL 任务
		if(lvgl_mutex) osMutexRelease(lvgl_mutex);
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
	uint8_t recv_buf[256];
	A7680C_Init();
  /* Infinite loop */
  for(;;)
  {
		size_t len = xMessageBufferReceive(uart3Message, recv_buf, sizeof(recv_buf), portMAX_DELAY);
		if(len > 0)
		{
				//调用流式拼接函数
				at_process_data(recv_buf, len);
		}
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
		//等待消息缓冲区有消息包，然后读取一包数据,xMessageBufferReceive会释放cpu,只要消息缓冲区有消息就会解除阻塞向下执行
		size_t n = xMessageBufferReceive(uart2Message, buf, sizeof(buf), pdMS_TO_TICKS(5000));
		if(n)
		{
				ES1642_ProcessCompleteFrame(&g_es1642_handle,buf,n);
		}
		else
		{
				// 超时，检查UART是否还活着
				if (huart2.RxState != HAL_UART_STATE_BUSY_RX)
				{
						printf("huart2DMA已死，重启DMA");
						// DMA接收已死，重新启动
						HAL_UARTEx_ReceiveToIdle_DMA(&huart2, g_es1642_rx_buf, sizeof(g_es1642_rx_buf));
						__HAL_DMA_DISABLE_IT(huart2.hdmarx, DMA_IT_HT);
				}
				continue;
		}
  }
  /* USER CODE END ES1642_Task */
}

/* USER CODE BEGIN Header_RTC_Task */
/**
* @brief Function implementing the RTCTask thread.
* @param argument: Not used
* @retval None
*
* 职责（精简版，AT模块管理已移至StartDefaultTask）:
* 1. 初始化外部RTC芯片RX8025T
* 2. 跳转到home界面
* 3. 启动天气定时器
* 4. 循环更新RTC时间显示
*/
/* USER CODE END Header_RTC_Task */
void RTC_Task(void *argument)
{
  /* USER CODE BEGIN RTC_Task */
	if (RX8025T_InitAndDisplay() != HAL_OK) printf("外部RTC时钟通信失败\r\n");

	/* 等待LVGL UI初始化完成（由lvgl_task发送0x02标志） */
	osThreadFlagsWait(0x02, osFlagsWaitAny, osWaitForever);
	printf("RTCTask: LVGL UI已就绪\r\n");
	osDelay(1500);

	/* 跳转到home界面 (获取互斥锁保护LVGL操作) */
	if(lvgl_mutex) osMutexAcquire(lvgl_mutex, osWaitForever);
	ui_load_scr_animation(&guider_ui, &guider_ui.screen_user_home,
			guider_ui.screen_user_home_del, &guider_ui.Startup_screen_del,
			setup_scr_screen_user_home, LV_SCR_LOAD_ANIM_NONE, 10, 10, true, true);
	if(lvgl_mutex) osMutexRelease(lvgl_mutex);

  /* Infinite loop */
  for(;;)
  {
		/* RX8025T_Task内部会调用LVGL API更新时间显示, 需要互斥锁保护 */
		if(lvgl_mutex) osMutexAcquire(lvgl_mutex, osWaitForever);
		RX8025T_Task();
		if(lvgl_mutex) osMutexRelease(lvgl_mutex);

		/* 更新电池和太阳能电压缓存 (ADC采集, 不涉及LVGL) */
		Battery_UpdateCache();
		Solar_UpdateCache();

		/* MPPT 扰动观察法状态机 (每秒执行一次) */
//		MPPT_Task();
    osDelay(1000);
  }
  /* USER CODE END RTC_Task */
}

/* USER CODE BEGIN Header_Weather_Task */
/**
* @brief Function implementing the WeatherTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_Weather_Task */
void Weather_Task(void *argument)
{
  /* USER CODE BEGIN Weather_Task */
	uint8_t jwd_buff[64];//存储经纬度字符串
	CLBS_PosTypeDef pos;
	
  /* Infinite loop */
  for(;;)
  {
		//等待 0x01 标志位，阻塞等待
    osThreadFlagsWait(0x01, osFlagsWaitAny, osWaitForever);
		if(simcard_ready == 1)
		{
			A7680C_SendAT("AT+CLBS=1\r\n", "+CLBS: 0", 5000,jwd_buff);//读取经纬度
			pos = A7680C_ParseCLBS((char*)jwd_buff);//解析经纬度
		
			A7680C_HTTP_GetWeatherData(pos.latitude,pos.longitude,&weather_data);//读取天气代码
		}
  }
  /* USER CODE END Weather_Task */
}

/* USER CODE BEGIN Header_DevicePoll_Task */
/**
* @brief Function implementing the DevicePollTask thread.
* @param argument: Not used
* @retval None
*
* 职责（精简版，MQTT初始化已移至StartDefaultTask）:
* 仅在simcard_ready=1时轮询设备状态并上报
*/
/* USER CODE END Header_DevicePoll_Task */
void DevicePoll_Task(void *argument)
{
  /* USER CODE BEGIN DevicePoll_Task */
  RX8025T_Date last_date = {0};  /* 记录上一次的日期，用于检测零点 */
  RX8025T_Date cur_date;

  /* 初始化：读取当前日期作为基准 */
  if (RX8025T_GetDate(&last_date) == HAL_OK)
  {
      printf("DevicePoll: 初始日期 20%02d-%02d-%02d\r\n",
             last_date.year, last_date.month, last_date.day);
  }
	osDelay(5000);//等待es1642收到帧头错误
  /* Infinite loop */
  for(;;)
  {
      if(g_es1642_searching == 0)  /* 仅在非搜索状态下执行 */
      {
        /* ① MPPT采集+控制一体化(慢速采集→告警扫描→快速P&O控制闭环) */
        device_poll_and_control_all();

        /* ② 更新太阳能发电量 = Σ各从机用电量 */
        uint32_t total_energy = 0;
        for (uint16_t i = 0; i < device_count; i++)
        {
            total_energy += daily_energy_wh[i];
        }
        g_mppt.energy_wh = total_energy;

        /* ⑤ 零点结算检测 */
        if (RX8025T_GetDate(&cur_date) == HAL_OK)
        {
            if (last_date.day != 0 && cur_date.day != last_date.day)
            {
                printf("检测到日期变化(20%02d-%02d-%02d → 20%02d-%02d-%02d)，执行零点结算\r\n",
                      last_date.year, last_date.month, last_date.day,
                      cur_date.year, cur_date.month, cur_date.day);
                daily_energy_flush_to_sd();  /* 将RAM中日累积电量写入SD卡 */
                solar_energy_flush();        /* 太阳能发电量结算并保存到SD卡 */
            }
            last_date = cur_date;
        }
      }
    osDelay(10000);  /* 10秒(控制+采集已在device_poll_and_control_all内完成) */
  }
  /* USER CODE END DevicePoll_Task */
}

/* Callback01 function */
void Callback01(void *argument)
{
  /* USER CODE BEGIN Callback01 */
	osThreadFlagsSet(WeatherTaskHandle, 0x01);
	osTimerStart(weatherTimerHandle,900000);//每15分钟调用一次获取天气函数
  /* USER CODE END Callback01 */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */
//freertos任务堆栈溢出错误回调函数
void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
    (void)xTask;
    printf("Stack overflow: %s\r\n", pcTaskName);
    taskDISABLE_INTERRUPTS();
    for(;;);
}
/* USER CODE END Application */

