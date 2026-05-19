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
#include "a7680c_at.h"
#include "a7680c_mqtt.h"
#include "rx8025t_example.h"
#include "es1642.h"
#include "es1642_usage_guide.h"
#include "device_manager.h"
#include "a7680c_http.h"
#include "rs485_usart.h"
lv_ui  guider_ui;                     // 定义 界面对象
extern lv_group_t * g_keypad_group;		//声明全局group
WeatherCurrent_t weather_data = {0};//存储天气代码的结构体
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
  .stack_size = 512 * 4,
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
  * @brief  A7680C模块管理任务（统一管理网络初始化、健康监控、信号显示）
  * @param  argument: Not used
  * @retval None
  *
  * 职责:
  * 1. 首次上电: 依次执行 ATE0→CheckNetworkReady→GetNetworkTime→HTTP_Init→MQTT连接
  * 2. 运行中: 每秒检测 CPIN+CGATT，网络异常累计10次则重启模块并重新初始化
  * 3. 更新UI信号强度条和天气文字
  */
/* 网络就绪标志（供Weather_Task和DevicePoll_Task使用） */
uint8_t simcard_ready = 0;
/* USER CODE END Header_StartDefaultTask */
void StartDefaultTask(void *argument)
{
  /* USER CODE BEGIN StartDefaultTask */
	int32_t rssi,ber;
	uint8_t Signal_buff[32];
	int32_t fail_count = 0;
	uint8_t need_init = 1;  /* 首次上电需要全量初始化 */
	uint16_t online_check_count = 0;  /* 上网能力周期检查计数器 */

  /* Infinite loop */
  for(;;)
  {
		HAL_GPIO_TogglePin(LED1_GPIO_Port, LED1_Pin);
		HAL_GPIO_TogglePin(LED2_GPIO_Port, LED2_Pin);

		/*======================================================
		 * 全量初始化（首次上电 或 模块重启后触发）
		 * 依次执行: ATE0 → 等待网络就绪 → 同步时间 → HTTP → MQTT
		 *======================================================*/
		if (need_init)
		{
			simcard_ready = 0;
			fail_count = 0;
			printf("========== A7680C全量初始化开始 ==========\r\n");

			/* 关闭回显（必须确保成功，否则回显会干扰后续AT指令解析）
			 * 模块刚上电时可能还未就绪，需要轮询等待 */
			uint8_t ate0_ok = 0;
			for (uint8_t ate_retry = 0; ate_retry < 30; ate_retry++)
			{
				if (A7680C_SendATE0() == AT_RESULT_OK)
				{
					ate0_ok = 1;
					printf("回显已关闭(第%d次尝试)\r\n", ate_retry + 1);
					break;
				}
				osDelay(1000);
			}
			if (!ate0_ok)
			{
				printf("警告: 30s内未能关闭回显\r\n");
			}

			/* 轮询等待网络就绪（最多60秒）
			 * CheckNetworkReady依次执行: CPIN→CGATT=1→CGATT?→CGDCONT
			 * 四步全部通过才说明模块可以正常上网 */
			uint8_t net_ok = 0;
			for (uint8_t retry = 0; retry < 60; retry++)
			{
				if (A7680C_CheckNetworkReady() == AT_RESULT_OK)
				{
					net_ok = 1;
					printf("网络就绪(第%d次尝试)\r\n", retry + 1);
					break;
				}
				printf("等待网络就绪...(%d/60)\r\n", retry + 1);
				osDelay(1000);
			}

			if (net_ok)
			{
				/* 同步网络时间到RTC芯片 */
				uint8_t step;
				if (A7680C_GetNetworkTime_Debug(&step) == AT_RESULT_OK)
				{
					printf("网络时间同步成功\r\n");
				}
				else
				{
					printf("网络时间同步失败, step:%d\r\n", step);
				}

				/* 初始化HTTP（用于天气获取） */
				A7680C_HTTP_Init();

				/* 初始化MQTT连接 */
				printf("初始化MQTT连接...\r\n");
				if (A7680C_MQTT_Start() != AT_RESULT_OK)
				{
					printf("MQTT START失败, 5秒后重试\r\n");
					osDelay(5000);
					A7680C_MQTT_Start();
				}
				if (A7680C_MQTT_Connect(MQTT_CLIENT_ID, NULL, NULL) != AT_RESULT_OK)
				{
					printf("MQTT CONNECT失败, 10秒后重试\r\n");
					osDelay(10000);
					A7680C_MQTT_Connect(MQTT_CLIENT_ID, NULL, NULL);
				}

				simcard_ready = 1;
				need_init = 0;
				
				osTimerStart(weatherTimerHandle, 1000);//从网络获取一次天气
				printf("========== A7680C全量初始化完成 ==========\r\n");
			}
			else
			{
				printf("60s内网络未就绪,稍后重试\r\n");
				simcard_ready = 0;
				need_init = 1;  /* 保持init标志，下次循环继续尝试 */
				A7680C_SendAT_CFUN();  /* 重启模块 */
				osDelay(5000);
				continue;
			}
		}

		/*======================================================
		 * UI就绪检测 + 信号强度显示 + 网络健康检查
		 *======================================================*/
		uint8_t ui_ready = lv_obj_is_valid(guider_ui.screen_user_home_line_1) &&
		                   lv_obj_is_valid(guider_ui.screen_user_home_line_3) &&
		                   lv_obj_is_valid(guider_ui.screen_user_home_label_3);

		if(ui_ready)
		{
			/* 检测SIM卡 + 网络附着状态 */
			uint8_t sim_ok = A7680C_SendAT_CPIN();
			uint8_t net_ok = 0;

			if(sim_ok == AT_RESULT_OK)
			{
				net_ok = A7680C_SendAT("AT+CGATT?\r\n", "+CGATT: 1", 2000, NULL);
			}

			if(sim_ok == AT_RESULT_OK && net_ok == AT_RESULT_OK)
			{
				/* 网络基本正常：更新信号强度显示 */
				fail_count = 0;

				/* 周期性验证上网能力（每30秒用CLBS检查一次）
				 * CPIN+CGATT在欠费卡下也能通过，但CLBS需要真正的数据通道
				 * CLBS失败说明SIM卡无法上网（欠费等），将simcard_ready置0 */
				online_check_count++;
				if(online_check_count >= 30)
				{
					online_check_count = 0;
					if(A7680C_SendAT("AT+CLBS=1\r\n", "+CLBS: 0", 5000, NULL) != AT_RESULT_OK)
					{
						printf("CLBS失败,SIM卡可能欠费无法上网\r\n");
						simcard_ready = 0;
						/* 不重启模块，只标记上网不可用，等下次CLBS成功再恢复 */
					}
					else
					{
						if(simcard_ready == 0)
						{
							printf("CLBS成功,上网能力恢复\r\n");
							simcard_ready = 1;
						}
					}
				}

				if(A7680C_SendAT_CSQ(Signal_buff) == AT_RESULT_OK)
				{
					lv_obj_add_flag(guider_ui.screen_user_home_label_3, LV_OBJ_FLAG_HIDDEN);
					A7680C_ParseCSQ(Signal_buff,&rssi,&ber);
					if(rssi >= 10 && rssi < 17)
					{
						lv_obj_set_style_line_color(guider_ui.screen_user_home_line_1, lv_color_hex(0x00ff86), LV_PART_MAIN|LV_STATE_DEFAULT);
						lv_obj_set_style_line_color(guider_ui.screen_user_home_line_2, lv_color_hex(0x757575), LV_PART_MAIN|LV_STATE_DEFAULT);
						lv_obj_set_style_line_color(guider_ui.screen_user_home_line_3, lv_color_hex(0x757575), LV_PART_MAIN|LV_STATE_DEFAULT);
					}
					else if(rssi >= 17 && rssi < 24)
					{
						lv_obj_set_style_line_color(guider_ui.screen_user_home_line_1, lv_color_hex(0x00ff86), LV_PART_MAIN|LV_STATE_DEFAULT);
						lv_obj_set_style_line_color(guider_ui.screen_user_home_line_2, lv_color_hex(0x00ff86), LV_PART_MAIN|LV_STATE_DEFAULT);
						lv_obj_set_style_line_color(guider_ui.screen_user_home_line_3, lv_color_hex(0x757575), LV_PART_MAIN|LV_STATE_DEFAULT);
					}
					else if(rssi >= 24 && rssi <= 31)
					{
						lv_obj_set_style_line_color(guider_ui.screen_user_home_line_1, lv_color_hex(0x00ff86), LV_PART_MAIN|LV_STATE_DEFAULT);
						lv_obj_set_style_line_color(guider_ui.screen_user_home_line_2, lv_color_hex(0x00ff86), LV_PART_MAIN|LV_STATE_DEFAULT);
						lv_obj_set_style_line_color(guider_ui.screen_user_home_line_3, lv_color_hex(0x00ff86), LV_PART_MAIN|LV_STATE_DEFAULT);
					}
					else if(rssi == 99)
					{
						lv_obj_set_style_line_color(guider_ui.screen_user_home_line_1, lv_color_hex(0x757575), LV_PART_MAIN|LV_STATE_DEFAULT);
						lv_obj_set_style_line_color(guider_ui.screen_user_home_line_2, lv_color_hex(0x757575), LV_PART_MAIN|LV_STATE_DEFAULT);
						lv_obj_set_style_line_color(guider_ui.screen_user_home_line_3, lv_color_hex(0x757575), LV_PART_MAIN|LV_STATE_DEFAULT);
					}
				}
			}
			else
			{
				/* 网络异常：累计失败次数 */
				fail_count++;
				printf("网络异常(SIM=%d,CGATT=%d, fail=%d/10)\r\n", sim_ok, net_ok, fail_count);

				lv_obj_set_style_line_color(guider_ui.screen_user_home_line_1, lv_color_hex(0x757575), LV_PART_MAIN|LV_STATE_DEFAULT);
				lv_obj_set_style_line_color(guider_ui.screen_user_home_line_2, lv_color_hex(0x757575), LV_PART_MAIN|LV_STATE_DEFAULT);
				lv_obj_set_style_line_color(guider_ui.screen_user_home_line_3, lv_color_hex(0x757575), LV_PART_MAIN|LV_STATE_DEFAULT);
				lv_obj_clear_flag(guider_ui.screen_user_home_label_3, LV_OBJ_FLAG_HIDDEN);

				if(fail_count >= 10)
				{
					printf("网络连续10次异常,重启模块并重新初始化\r\n");
					simcard_ready = 0;
					A7680C_SendAT_CFUN();  /* 重启模块 */
					need_init = 1;         /* 触发全量重新初始化 */
					osDelay(5000);         /* 等待模块重启 */
					continue;
				}
			}
		}

		/* 天气文字更新 */
		const char* Weather_buff = Weather_GetShortDesc(weather_data.weather_code);
		if(lv_obj_is_valid(guider_ui.screen_user_home_label_1) && lv_obj_is_valid(guider_ui.screen_user_home_label_2))
		{
				lv_label_set_text(guider_ui.screen_user_home_label_1, Weather_buff);
				if(weather_data.is_day == 1)
				{
					lv_label_set_text(guider_ui.screen_user_home_label_2,"白天 ");
				}
				else
				{
					lv_label_set_text(guider_ui.screen_user_home_label_2,"黑天 ");
				}
		}
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
	// 上电时加载设备表
	device_manager_init();
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
	if (RX8025T_InitAndDisplay() == HAL_OK) 
	{
//		printf("外部RTC时钟通信成功\r\n");
	}
	else
	{
		printf("外部RTC时钟通信失败\r\n");
	}

	/* 等待LVGL UI初始化完成（由lvgl_task发送0x02标志） */
	osThreadFlagsWait(0x02, osFlagsWaitAny, osWaitForever);
	printf("RTCTask: LVGL UI已就绪\r\n");
	osDelay(1500);

	/* 跳转到home界面 */
	ui_load_scr_animation(&guider_ui, &guider_ui.screen_user_home,
			guider_ui.screen_user_home_del, &guider_ui.screen_user_list_del,
			setup_scr_screen_user_home, LV_SCR_LOAD_ANIM_NONE, 10, 10, true, true);

  /* Infinite loop */
  for(;;)
  {
		RX8025T_Task();
    osDelay(100);
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
  /* Infinite loop */
  for(;;)
  {
    if(simcard_ready == 1)
    {
      device_poll_all_status();
    }
    osDelay(30000);  /* 每30秒轮询一次 */
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

