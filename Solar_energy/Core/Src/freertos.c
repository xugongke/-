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
#include "lvgl.h"
#include "lv_port_disp.h"
#include "lv_port_indev.h"
#include "lv_port_fs.h"
#include "lv_demo_widgets.h"
#include "lv_demo_music.h"
#include "lv_demo_benchmark.h"
#include "lv_demo_keypad_encoder.h"
#include "lv_demos.h"
#include "key.h"
#include "tpad.h"
#include "gui_guider.h"           // Gui Guider 生成的界面和控件的声明
#include "events_init.h"          // Gui Guider 生成的初始化事件、回调函数
#include "sdio.h"
#include "fatfs.h"
#include "dma2d.h"
#include "custom.h"
#include "sdcard.h"
#include "user_status.h"
lv_ui  guider_ui;                     // 声明 界面对象
extern lv_group_t * g_keypad_group;		//声明全局group
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

// SD卡的FatFS文件系统挂载、格式化、读写测试
void FatFsTest(void)
{
    static FATFS myFatFs;                                                 // FatFs 文件系统对象; 这个结构体占用598字节，有点大，需用static修饰(存放在全局数据区), 避免stack溢出
    static FIL myFile;                                                    // 文件对象; 这个结构体占用570字节，有点大，需用static修饰(存放在全局数据区), 避免stack溢出
    static FRESULT f_res;                                                 // 文件操作结果
    static uint32_t num;                                                  // 文件实际成功读写的字节数
    static uint8_t aReadData[1024] = {0};                                 // 读取缓冲区; 这个数组占用1024字节，需用static修饰(存放在全局数据区), 避免stack溢出
    static uint8_t aWriteBuf[] =  "测试; This is FatFs Test ! \r\n";      // 要写入的数据
 
    // 重要的延时：避免烧录期间的复位导致文件读写、格式化等错误
    HAL_Delay(1000);                                                      // 重要：稍作延时再开始读写测试; 避免有些仿真器烧录期间的多次复位，短暂运行了程序，导致下列读写数据不完整。
 
    // 1、挂载测试：在SD卡挂载文件系统
    printf("\r\n\r\n");
    printf("1、挂载 FatFs 测试 ****** \r\n");
    f_res = f_mount(&myFatFs, "0:", 1);                                   // 在SD卡上挂载文件系统; 参数：文件系统对象、驱动器路径、读写模式(0只读、1读写)
    if (f_res == FR_NO_FILESYSTEM)                                        // 检查是否已有文件系统，如果没有，就格式化创建创建文件系统
    {
        printf("SD卡没有文件系统，开始格式化…...\r\n");
        static uint8_t aMountBuffer[4096];                                // 格式化时所需的临时缓存; 块大小512的倍数; 值越大格式化越快, 如果内存不够，可改为512或者1024; 当需要在函数内定义这种大缓存时，要用static修饰，令缓存存放在全局数据区内，不然，可能会导致stack溢出。
        f_res = f_mkfs("0:",2, 0, aMountBuffer, sizeof(aMountBuffer));   // 格式化SD卡; 参数：驱动器、文件系统(0-自动\1-FAT12\2-FAT32\3-FAT32\4-exFat)、簇大小(0为自动选择)、格式化临时缓冲区、缓冲区大小; 格式化前必须先f_mount(x,x,1)挂载，即必须用读写方式挂载; 如果SD卡已格式化，f_mkfs()的第2个参数，不能用0自动，必须指定某个文件系统。
        if (f_res == FR_OK)                                               // 格式化 成功
        {
            printf("SD卡格式化：成功 \r\n");
            f_res = f_mount(NULL, "0:", 1);                               // 格式化后，先取消挂载
            f_res = f_mount(&myFatFs, "0:", 1);                           // 重新挂载
						printf("%#x\r\n",myFatFs.fs_type);
            if (f_res == FR_OK)
                printf("FatFs 挂载成功 \r\n");                            // 挂载成功
            else
                return;                                                   // 挂载失败，退出函数
        }
        else
        {
            printf("SD卡格式化：失败 \r\n");                              // 格式化 失败
            return;
        }
    }
    else if (f_res != FR_OK)                                              // 挂载异常
    {
        printf("FatFs 挂载异常: %d; 检查MX_SDIO_SD_Init()是否已修改1B\r", f_res);
        return;
    }
    else                                                                  // 挂载成功
    {
				printf("%#x\r\n",myFatFs.fs_type);
        if (myFatFs.fs_type == 0x03)                                      // FAT32; 1-FAT12、2-FAT16、3-FAT32、4-exFat
            printf("SD卡已有文件系统：FAT32\n");
        if (myFatFs.fs_type == 0x04)                                      // exFAT; 1-FAT12、2-FAT16、3-FAT32、4-exFat
            printf("SD卡已有文件系统：exFAT\n");                         
        printf("FatFs 挂载成功 \r\n");                                    // 挂载成功
    }
 
    // 2、写入测试：打开或创建文件，并写入数据
    printf("\r\n");
    printf("2、写入测试：打开或创建文件，并写入数据 ****** \r\n");
    f_res = f_open(&myFile, "0:text.txt", FA_CREATE_ALWAYS | FA_WRITE);   // 打开文件; 参数：要操作的文件对象、路径和文件名称、打开模式;
    if (f_res == FR_OK)
    {
        printf("打开文件 成功 \r\n");
        printf("写入测试\r\n");
        f_res = f_write(&myFile, aWriteBuf, sizeof(aWriteBuf), &num);     // 向文件内写入数据; 参数：文件对象、数据缓存、申请写入的字节数、实际写入的字节数
        if (f_res == FR_OK)
        {
            printf("写入成功  \r\n");
            printf("已写入字节数：%d \r\n", num);                         // printf 写入的字节数
            printf("已写入的数据：%s \r\n", aWriteBuf);                   // printf 写入的数据; 注意，这里以字符串方式显示，如果数据是非ASCII可显示范围，则无法显示
        }
        else
        {
            printf("写入失败 \r\n");                                      // 写入失败
            printf("错误编号： %d\r\n", f_res);                           // printf 错误编号
        }
        f_close(&myFile);                                                 // 不再读写，关闭文件
    }
    else
    {
        printf("打开文件 失败: %d\r\n", f_res);
    }
 
    // 3、读取测试：打开已有文件，读取其数据
    printf("3、读取测试：打开刚才的文件，读取其数据 ****** \r\n");
    f_res = f_open(&myFile, "0:text.txt", FA_OPEN_EXISTING | FA_READ);    // 打开文件; 参数：文件对象、路径和名称、操作模式; FA_OPEN_EXISTING：只打开已存在的文件; FA_READ: 以只读的方式打开文件
    if (f_res == FR_OK)
    {
        printf("打开文件 成功 \r\n");
        f_res = f_read(&myFile, aReadData, sizeof(aReadData), &num);      // 从文件中读取数据; 参数：文件对象、数据缓冲区、请求读取的最大字节数、实际读取的字节数
        if (f_res == FR_OK)
        {
            printf("读取数据 成功 \r\n");
            printf("已读取字节数：%d \r\n", num);                         // printf 实际读取的字节数
            printf("读取到的数据：%s\r\n", aReadData);                    // printf 实际数据; 注意，这里以字符串方式显示，如果数据是非ASCII可显示范围，则无法显示
        }
        else
        {
            printf("读取 失败  \r\n");                                    // printf 读取失败
            printf("错误编号：%d \r\n", f_res);                           // printf 错误编号
        }
    }
    else
    {
        printf("打开文件 失败 \r\n");                                     // printf 打开文件 失败
        printf("错误编号：%d\r\n", f_res);                                // printf 错误编号
    }
 
    f_close(&myFile);                                                     // 不再读写，关闭文件
    f_mount(NULL, "0:", 1);                                               // 不再使用文件系统，取消挂载文件系统
}
// 获取SD卡信息
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

// 从 SD 卡读取文本文件（UTF-8 编码）
static bool read_sd_text(const char *path, char *buf, uint32_t size)
{
    lv_fs_file_t file;
    lv_fs_res_t res = lv_fs_open(&file, path, LV_FS_MODE_RD);
    if (res != LV_FS_RES_OK) {
        printf("❌ 文件打开失败: %s (错误码=%d)\n", path, res);
        return false;
    }

    uint32_t br = 0;
    res = lv_fs_read(&file, buf, size - 1, &br);
    lv_fs_close(&file);
    
    if (res != LV_FS_RES_OK || br == 0) {
        printf("❌ 读取失败或文件为空 (读取=%lu)\n", br);
        return false;
    }
    buf[br] = '\0'; // 确保字符串结尾
    printf("✅ 读取成功: %lu 字节, 内容预览: %.20s...\n", br, buf);
    return true;
}

// 创建只读文本显示区域
void display_sd_text_file(void)
{
    // 1. 读取文件内容（缓冲区大小根据实际调整）
    #define TEXT_BUF_SIZE 8192
    char *text_buf = lv_mem_alloc(TEXT_BUF_SIZE);
    if (!text_buf) {
        LV_LOG_ERROR("内存分配失败!");
        return;
    }
    
    // 路径注意：驱动字母'S'需与 lv_port_fs_init 中 fs_drv.letter 一致
    if (!read_sd_text("S:/TXT/txt1.txt", text_buf, TEXT_BUF_SIZE)) { // 替换为你的文件名
        lv_mem_free(text_buf);
        return;
    }

    // 2. 创建只读文本区域（核心配置）
    lv_obj_t *ta = lv_textarea_create(lv_scr_act());
    lv_obj_set_size(ta, lv_pct(95), lv_pct(85));      // 占屏幕95%宽 x 85%高
    lv_obj_align(ta, LV_ALIGN_CENTER, 0, 0);
    
//    // 关键设置：只读 + 隐藏光标 + 禁用边框
//    lv_textarea_set_readonly(ta, true);                // 禁止编辑
//    lv_textarea_set_cursor_hidden(ta, true);           // 隐藏闪烁光标
//    lv_obj_set_style_border_width(ta, 0, LV_PART_MAIN); // 去掉边框（更像纯文本框）
    
    // 3. 设置文本（自动处理 UTF-8 中文）
    lv_textarea_set_text(ta, text_buf);
    
    // 4. 优化显示效果（可选但推荐）
    lv_obj_set_style_bg_color(ta, lv_color_white(), LV_PART_MAIN); // 白底
    lv_obj_set_style_text_color(ta, lv_color_black(), LV_PART_MAIN); // 黑字
    lv_obj_set_style_pad_all(ta, 10, LV_PART_MAIN);    // 内边距更舒适
    
    // 5. 释放内存
    lv_mem_free(text_buf);
    
    printf("✅ 文本已显示在屏幕中央（支持滚动查看全部内容）\n");
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
  .priority = (osPriority_t) osPriorityHigh,
};
/* Definitions for LVGLTask */
osThreadId_t LVGLTaskHandle;
const osThreadAttr_t LVGLTask_attributes = {
  .name = "LVGLTask",
  .stack_size = 2048 * 4,
  .priority = (osPriority_t) osPriorityLow,
};
/* Definitions for UserStatusTask */
osThreadId_t UserStatusTaskHandle;
const osThreadAttr_t UserStatusTask_attributes = {
  .name = "UserStatusTask",
  .stack_size = 512 * 4,
  .priority = (osPriority_t) osPriorityLow,
};

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */

/* USER CODE END FunctionPrototypes */

void StartDefaultTask(void *argument);
void lvgl_task(void *argument);
void USER_STATUS_Task(void *argument);

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
  /* add semaphores, ... */
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
  /* start timers, add new ones, ... */
  /* USER CODE END RTOS_TIMERS */

  /* USER CODE BEGIN RTOS_QUEUES */
  /* add queues, ... */
  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* creation of defaultTask */
  defaultTaskHandle = osThreadNew(StartDefaultTask, NULL, &defaultTask_attributes);

  /* creation of LVGLTask */
  LVGLTaskHandle = osThreadNew(lvgl_task, NULL, &LVGLTask_attributes);

  /* creation of UserStatusTask */
  UserStatusTaskHandle = osThreadNew(USER_STATUS_Task, NULL, &UserStatusTask_attributes);

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
//	FatFsTest();		//SD卡的FatFS文件系统挂载、格式化、读写测试
//	SDCardInfo();		//获取SD卡信息;注意:本函数要在f_mount()执行后再调用,只有在挂载时，才会调用 SD_initialize，进而调用 BSP_SD_Init()。
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
	SDCardInfo();//读取SD卡基本信息,在挂载之后执行
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
//	for(int i = 1;i <=50;i++)
//	{
//		sdcard_write(i);
//	}
	
//	lv_demo_widgets();
//	lv_demo_music();
//	lv_demo_benchmark();//刷新率测试
//	lv_demo_stress();
//	lv_demo_keypad_encoder();
	
	

	

	/* USER CODE END 2 */
	//自己设计的图形窗口
//	setup_ui(&guider_ui);           // 初始化 UI
//	events_init(&guider_ui);       // 初始化 事件
//	custom_init(&guider_ui);			// 你自己的逻辑
  /* Infinite loop */
  for(;;)
  {
		lv_timer_handler();  // 处理 LVGL 任务
		osDelay(5);
  }
  /* USER CODE END lvgl_task */
}

/* USER CODE BEGIN Header_USER_STATUS_Task */
/**
* @brief Function implementing the UserStatusTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_USER_STATUS_Task */
void USER_STATUS_Task(void *argument)
{
  /* USER CODE BEGIN USER_STATUS_Task */
//	USER_STATUS_Init();
  /* Infinite loop */
  for(;;)
  {
//    if(dirty_flag)
//    {
//        USER_STATUS_Save();//更新用户在线状态
//    }
    osDelay(1000);
  }
  /* USER CODE END USER_STATUS_Task */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */

/* USER CODE END Application */

