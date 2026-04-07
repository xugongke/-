#include "user_status.h"
#include "string.h"
#include "stdio.h"
#include "lvgl.h"
#include "gui_guider.h"
#include "sdcard.h"
#include "es1642_usage_guide.h"
#include "key.h" // Added for key handling
#include "main.h" // For HAL_GetTick
#include "a7680c_at.h"
#include "a7680c_http.h"
static USER_STATUS_FILE user_file;

uint8_t dirty_flag = 0;

static const char *file_main = "S:/SYS/user_status.bin";			//主状态文件
static const char *file_bak  = "S:/SYS/user_status_bak.bin";	//备份文件（掉电保护）
static const char *log_file  = "0:/SYS/user_log.txt";					//上线下线日志


//CRC计算
static uint32_t CRC32(uint8_t *buf,uint32_t len)
{
    uint32_t crc=0xffffffff;

    for(uint32_t i=0;i<len;i++)
    {
        crc^=buf[i];

        for(uint8_t j=0;j<8;j++)
        {
            if(crc&1)
                crc=(crc>>1)^0xedb88320;
            else
                crc>>=1;
        }
    }

    return crc;
}
//用户状态读取
uint8_t USER_IsOnline(uint8_t user)
{
    if(user<1 || user>USER_MAX) return 0;

    uint8_t byte=(user-1)/8;
    uint8_t bit =(user-1)%8;

    return (user_file.bitmap[byte]>>bit)&1;
}
//用户上线
void USER_SetOnline(uint8_t user)
{
    uint8_t byte=(user-1)/8;
    uint8_t bit =(user-1)%8;

    if(USER_IsOnline(user)) return;

    user_file.bitmap[byte]|=(1<<bit);

    dirty_flag=1;

    USER_LOG(user,"ONLINE");
}
//用户离线
void USER_SetOffline(uint8_t user)
{
    uint8_t byte=(user-1)/8;
    uint8_t bit =(user-1)%8;

    if(!USER_IsOnline(user)) return;

    user_file.bitmap[byte]&=~(1<<bit);

    dirty_flag=1;

    USER_LOG(user,"OFFLINE");
}
//日志记录（带RTC时间）
void USER_LOG(uint8_t user,char *event)
{
    FIL file;
    UINT bw;

    char buf[64];

//    RTC_TimeTypeDef time;
//    RTC_DateTypeDef date;

//    HAL_RTC_GetTime(&hrtc,&time,RTC_FORMAT_BIN);
//    HAL_RTC_GetDate(&hrtc,&date,RTC_FORMAT_BIN);

//    sprintf(buf,
//    "%02d-%02d-%02d %02d:%02d:%02d USER%02d %s\r\n",
//    2000+date.Year,
//    date.Month,
//    date.Date,
//    time.Hours,
//    time.Minutes,
//    time.Seconds,
//    user,
//    event);

    sprintf(buf,
    "%02d-%02d-%02d %02d:%02d:%02d USER%02d %s\r\n",
    2000+26,
    3,
    5,
    9,
    25,
    30,
    user,
    event);
		//FA_OPEN_APPEND，如果文件存在，直接打开，如果文件不存在，则创建，并在文本末尾添加新内容
    if(f_open(&file,log_file,FA_OPEN_APPEND|FA_WRITE)==FR_OK)
    {
        f_write(&file,buf,strlen(buf),&bw);
        f_close(&file);
    }
		else
		{
			printf("user_log.txt文件打开失败\r\n");
		}
}
//保存状态到SD卡
void USER_STATUS_Save(void)
{
    lv_fs_file_t file;
    uint32_t bw;//记录写入字节数的变量

    user_file.magic=USER_STATUS_MAGIC;

    user_file.version++;

    user_file.crc=CRC32((uint8_t*)&user_file,
    sizeof(USER_STATUS_FILE)-4);

    if(lv_fs_open(&file,file_bak,LV_FS_MODE_WR)==LV_FS_RES_OK)
    {
        lv_fs_write(&file,&user_file,sizeof(USER_STATUS_FILE),&bw);
        lv_fs_close(&file);
    }
		else
		{
			printf("user_status_bak.bin打开失败\r\n");
		}

    if(lv_fs_open(&file,file_main,LV_FS_MODE_WR)==FR_OK)
    {
        lv_fs_write(&file,&user_file,sizeof(USER_STATUS_FILE),&bw);
        lv_fs_close(&file);
    }
		else
		{
			printf("user_status.bin打开失败\r\n");
		}

    dirty_flag=0;
}
//系统初始化
void USER_STATUS_Init(void)
{
    FIL file;
    UINT br;

    if(f_open(&file,file_main,FA_READ)==FR_OK)
    {
        f_read(&file,&user_file,sizeof(USER_STATUS_FILE),&br);

        f_close(&file);

        uint32_t crc=CRC32((uint8_t*)&user_file,sizeof(USER_STATUS_FILE)-4);

        if(user_file.magic!=USER_STATUS_MAGIC || crc!=user_file.crc)
        {
            memset(&user_file,0,sizeof(user_file));
        }
    }
    else
    {
        memset(&user_file,0,sizeof(user_file));
    }
}


void screen_user_list_item_event_handler(lv_event_t *e)
{
    /* user_data 里放 user_no（1~50）获取用户编号 */
    uint32_t user_no = (uint32_t)(uintptr_t)lv_event_get_user_data(e);

    lv_event_code_t code = lv_event_get_code(e);
    switch (code) {
    case LV_EVENT_KEY:
    {
        uint32_t key = lv_event_get_key(e);
        if(key == LV_KEY_LEFT)//左键，用户离线
        {
					ES1642_ReadAddr();
//					USER_SetOffline(user_no);//用户离线
        }
				
        if(key == LV_KEY_RIGHT)//右键，用户上线
        {
					printf("开始搜索全部设备\r\n");
					ES1642_StartSearch(0,ES1642_SEARCH_RULE_ALL);
//					USER_SetOnline(user_no);//用户上线
        }
				
        if(key == LV_KEY_UP)
        {
						WeatherCurrent_t weather_data;
						uint8_t jwd_buff[64];
					
						A7680C_SendAT("AT+CLBS=1\r\n", "CLBS", 5000,jwd_buff);//读取经纬度
						CLBS_PosTypeDef pos = A7680C_ParseCLBS((char*)jwd_buff);//解析经纬度
					
						A7680C_HTTP_GetWeatherData(pos.latitude,pos.longitude,&weather_data);//读取天气代码
						const char* Weather_buff = Weather_GetShortDesc(weather_data.weather_code);//将天气代码翻译成中文
						printf("%s\r\n",Weather_buff);
        }
				
        if(key == LV_KEY_DOWN)
        {
					
        }
				
        if(key == LV_KEY_ESC)//ESC键，返回首页
        {
					 s_last_user_no = 0;//回到首页，重置用户列表焦点
					 ui_load_scr_animation(&guider_ui, &guider_ui.screen_user_home, guider_ui.screen_user_home_del, &guider_ui.screen_user_list_del, setup_scr_screen_user_home, LV_SCR_LOAD_ANIM_NONE, 10, 10, true, true);
        }

        break;
    }
    default:
        break;
    }
}
