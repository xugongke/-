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
#include "sdcard.h"
#include "user_status.h"
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
  .stack_size = 128 * 4,
  .priority = (osPriority_t) osPriorityLow7,
};
/* Definitions for LVGLTask */
osThreadId_t LVGLTaskHandle;
const osThreadAttr_t LVGLTask_attributes = {
  .name = "LVGLTask",
  .stack_size = 1024 * 4,
  .priority = (osPriority_t) osPriorityHigh7,
};
/* Definitions for A7680CTask */
osThreadId_t A7680CTaskHandle;
const osThreadAttr_t A7680CTask_attributes = {
  .name = "A7680CTask",
  .stack_size = 512 * 4,
  .priority = (osPriority_t) osPriorityLow4,
};
/* Definitions for W5500Task */
osThreadId_t W5500TaskHandle;
const osThreadAttr_t W5500Task_attributes = {
  .name = "W5500Task",
  .stack_size = 512 * 4,
  .priority = (osPriority_t) osPriorityLow,
};
/* Definitions for ES1642Task */
osThreadId_t ES1642TaskHandle;
const osThreadAttr_t ES1642Task_attributes = {
  .name = "ES1642Task",
  .stack_size = 512 * 4,
  .priority = (osPriority_t) osPriorityLow5,
};
/* Definitions for RTCTask */
osThreadId_t RTCTaskHandle;
const osThreadAttr_t RTCTask_attributes = {
  .name = "RTCTask",
  .stack_size = 512 * 4,
  .priority = (osPriority_t) osPriorityLow3,
};
/* Definitions for WeatherTask */
osThreadId_t WeatherTaskHandle;
const osThreadAttr_t WeatherTask_attributes = {
  .name = "WeatherTask",
  .stack_size = 512 * 4,
  .priority = (osPriority_t) osPriorityLow2,
};
/* Definitions for RS485UARTProces */
osThreadId_t RS485UARTProcesHandle;
const osThreadAttr_t RS485UARTProces_attributes = {
  .name = "RS485UARTProces",
  .stack_size = 256 * 4,
  .priority = (osPriority_t) osPriorityLow,
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
void Callback01(void *argument);

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

  /* Create the semaphores(s) */
  /* creation of at_sem */
  at_semHandle = osSemaphoreNew(1, 0, &at_sem_attributes);

  /* creation of at_mutex */
  at_mutexHandle = osSemaphoreNew(1, 1, &at_mutex_attributes);

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

  /* USER CODE BEGIN RTOS_THREADS */
  /* add threads, ... */
  /* USER CODE END RTOS_THREADS */

  /* USER CODE BEGIN RTOS_EVENTS */
  /* add events, ... */
	osTimerStart(weatherTimerHandle, 900000);//每15分钟调用一次获取天气函数
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
	int32_t rssi,ber,i = 0;
  /* Infinite loop */
  for(;;)
  {
		HAL_GPIO_TogglePin(LED1_GPIO_Port, LED1_Pin);
		HAL_GPIO_TogglePin(LED2_GPIO_Port, LED2_Pin);
		if(i >= 10)//如果重启过模块,就要重新关闭回显
		{
			if(A7680C_SendATE0() == AT_RESULT_OK)
			{
				i = 0;
			}
		}
		//不管在哪个页面都不停的检测sim卡是否正常插入，是否正常通信
		if(lv_obj_is_valid(guider_ui.screen_user_home_line_1) && lv_obj_is_valid(guider_ui.screen_user_home_line_3) && lv_obj_is_valid(guider_ui.screen_user_home_label_3))
		{
			//只有跳转到home界面创建了信号线段组件才开始设置图形显示,虽然现在home页面设置为不删除了，但是还是判断一下比较好
			if(A7680C_SendAT_CPIN() == AT_RESULT_OK)//检测SIM卡是否插好
			{
				uint8_t Signal_buff[32];
				if(A7680C_SendAT_CSQ(Signal_buff) == AT_RESULT_OK)//如果查询信号强度成功
				{
					//隐藏标签
					lv_obj_add_flag(guider_ui.screen_user_home_label_3, LV_OBJ_FLAG_HIDDEN);
					A7680C_ParseCSQ(Signal_buff,&rssi,&ber);//解析读取到的信号强度
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
			else//检测不到sim卡
			{
				lv_obj_set_style_line_color(guider_ui.screen_user_home_line_1, lv_color_hex(0x757575), LV_PART_MAIN|LV_STATE_DEFAULT);
				lv_obj_set_style_line_color(guider_ui.screen_user_home_line_2, lv_color_hex(0x757575), LV_PART_MAIN|LV_STATE_DEFAULT);
				lv_obj_set_style_line_color(guider_ui.screen_user_home_line_3, lv_color_hex(0x757575), LV_PART_MAIN|LV_STATE_DEFAULT);
				//显示标签
				lv_obj_clear_flag(guider_ui.screen_user_home_label_3, LV_OBJ_FLAG_HIDDEN);
				i++;
				if(i == 10)
				{
					A7680C_SendAT_CFUN();//重启模块
				}
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
//	for(uint32_t i = 0;i < 50;i++)
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
	uint8_t recv_buf[256];
	A7680C_SendATE0();//关闭回显
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
		size_t n = xMessageBufferReceive(uart2Message, buf, sizeof(buf), portMAX_DELAY);
		if(n)
		{
				ES1642_ProcessCompleteFrame(&g_es1642_handle,buf,n);
		}
  }
  /* USER CODE END ES1642_Task */
}

/* USER CODE BEGIN Header_RTC_Task */
/**
* @brief Function implementing the RTCTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_RTC_Task */
void RTC_Task(void *argument)
{
  /* USER CODE BEGIN RTC_Task */
	uint8_t step;
	if (RX8025T_InitAndDisplay() == HAL_OK) 
	{
//		printf("外部RTC时钟通信成功\r\n");
	}
	else
	{
		printf("外部RTC时钟通信失败\r\n");
	}
	//更新网络时间
	osDelay(4000);
	at_result_t ret = A7680C_GetNetworkTime_Debug(&step);
	A7680C_HTTP_Init();//初始化HTTP
	if (ret == AT_RESULT_OK)
	{
			printf("更新网络时间到RTC芯片成功\r\n");
			//更新完成网络时间后再跳转到home界面
			ui_load_scr_animation(&guider_ui, &guider_ui.screen_user_home, guider_ui.screen_user_home_del, &guider_ui.screen_user_list_del, setup_scr_screen_user_home, LV_SCR_LOAD_ANIM_NONE, 10, 10, true, true);
	}
	else
	{
			printf("step:%d出现错误\r\n",step);
			//更新网络时间错误也要跳转
			ui_load_scr_animation(&guider_ui, &guider_ui.screen_user_home, guider_ui.screen_user_home_del, &guider_ui.screen_user_list_del, setup_scr_screen_user_home, LV_SCR_LOAD_ANIM_NONE, 10, 10, true, true);
	}
  /* Infinite loop */
  for(;;)
  {
		RX8025T_Task();
    osDelay(10);
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
	WeatherCurrent_t weather_data = {0};//存储天气代码的结构体
	
  /* Infinite loop */
  for(;;)
  {
		//等待 0x01 标志位，阻塞等待
    osThreadFlagsWait(0x01, osFlagsWaitAny, osWaitForever);
		
		A7680C_SendAT("AT+CLBS=1\r\n", "CLBS", 5000,jwd_buff);//读取经纬度
		pos = A7680C_ParseCLBS((char*)jwd_buff);//解析经纬度
	
		A7680C_HTTP_GetWeatherData(pos.latitude,pos.longitude,&weather_data);//读取天气代码
		const char* Weather_buff = Weather_GetShortDesc(weather_data.weather_code);//将天气代码翻译成中文
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
  }
  /* USER CODE END Weather_Task */
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

