#include "lvgl.h"
#include "sdcard.h"
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "ff.h"


/* 读一个文本文件到 out（最多 out_len-1 字节），并做 '\r' '\n' 尾部裁剪 */
static bool fs_read_text_file(const char *path, char *out, size_t out_len)
{
    if(!path || !out || out_len < 2) return false;
    out[0] = '\0';

    lv_fs_file_t f;
    lv_fs_res_t res = lv_fs_open(&f, path, LV_FS_MODE_RD);
    if(res != LV_FS_RES_OK) {
        return false;
    }

    uint32_t br = 0;
    res = lv_fs_read(&f, out, (uint32_t)(out_len - 1), &br);
    lv_fs_close(&f);

    if(res != LV_FS_RES_OK) {
        out[0] = '\0';
        return false;
    }

    out[br] = '\0';

    /* 去掉末尾的换行/回车/空白 */
    while(br > 0) {
        char c = out[br - 1];
        if(c == '\n' || c == '\r' || c == ' ' || c == '\t') {
            out[--br] = '\0';
        } else {
            break;
        }
    }

    /* 可选：处理 UTF-8 BOM（0xEF 0xBB 0xBF） */
    if((unsigned char)out[0] == 0xEF &&
       (unsigned char)out[1] == 0xBB &&
       (unsigned char)out[2] == 0xBF) {
        memmove(out, out + 3, strlen(out + 3) + 1);
    }

    return true;
}

/* 你 GUI-Guider 里通常会有一个全局的 lv_ui guider_ui; 
   这里假设你能拿到 ui 指针（推荐做法是传进来） */
void ui_load_user_detail(lv_ui *ui, uint32_t user_no)
{
    if(!ui) return;

    char path[64];
		char user[64];
    char dat[64];
    char moon[64];
    char year[64];
		lv_snprintf(user, sizeof(user), "用户：%lu", (unsigned long)user_no);

    /* data.txt */
    lv_snprintf(path, sizeof(path), "S:/USER/%lu/data.txt", (unsigned long)user_no);
    if(!fs_read_text_file(path, dat, sizeof(dat))) {
        lv_snprintf(dat, sizeof(dat), "N/A");
    }

    /* moon.txt */
    lv_snprintf(path, sizeof(path), "S:/USER/%lu/moon.txt", (unsigned long)user_no);
    if(!fs_read_text_file(path, moon, sizeof(moon))) {
        lv_snprintf(moon, sizeof(moon), "N/A");
    }

    /* year.txt */
    lv_snprintf(path, sizeof(path), "S:/USER/%lu/year.txt", (unsigned long)user_no);
    if(!fs_read_text_file(path, year, sizeof(year))) {
        lv_snprintf(year, sizeof(year), "N/A");
    }
		//切换页面函数，该函数是自动生成的
		ui_load_scr_animation(&guider_ui, &guider_ui.screen_user_detail, guider_ui.screen_user_detail_del, &guider_ui.screen_user_list_del, setup_scr_screen_user_detail, LV_SCR_LOAD_ANIM_NONE, 10, 10, true, true);

		/* 刷新 UI 文本 */
		lv_label_set_text(ui->screen_user_detail_label_user, user);
    lv_table_set_cell_value(ui->screen_user_detail_table_1,1,1,dat);
    lv_table_set_cell_value(ui->screen_user_detail_table_1,2,1,moon);
    lv_table_set_cell_value(ui->screen_user_detail_table_1,3,1,year);
}
/* 全局或静态变量，保存上次的编号 */
uint8_t s_last_user_no = 0; /* 默认选中第 0 个 */
//用户按键统一回调函数
void user_list_item_event_handler(lv_event_t *e)
{
    /* user_data 里放 user_no（1~50）获取用户编号 */
    uint32_t user_no = (uint32_t)(uintptr_t)lv_event_get_user_data(e);
	
		/* 记住编号 */
    s_last_user_no = user_no;

    /* 如果你工程里有全局 guider_ui，就直接用它 */
    ui_load_user_detail(&guider_ui, user_no);
}

void sdcard_write(uint32_t i)
{
    static FIL myFile;                                                    // 文件对象; 这个结构体占用570字节，有点大，需用static修饰(存放在全局数据区), 避免stack溢出
    static FRESULT f_res;                                                 // 文件操作结果
    static uint32_t num;                                                  // 文件实际成功读写的字节数
    char aWriteBuf[64];      // 要写入的数据
		char path[64];
	
		lv_snprintf(path, sizeof(path), "0:/USER/%lu/data.txt", (unsigned long)i);
		lv_snprintf(aWriteBuf, sizeof(aWriteBuf), "%d℃ ", (unsigned long)i*10);
	
    f_res = f_open(&myFile, path, FA_OPEN_ALWAYS | FA_WRITE);   // 打开文件; 参数：要操作的文件对象、路径和文件名称、打开模式;
    if (f_res == FR_OK)
    {
        f_res = f_write(&myFile, aWriteBuf, sizeof(aWriteBuf), &num);     // 向文件内写入数据; 参数：文件对象、数据缓存、申请写入的字节数、实际写入的字节数
        if (f_res == FR_OK)
        {
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


//		lv_snprintf(path, sizeof(path), "0:/USER/%lu", (unsigned long)i);
//		f_res = f_mkdir(path);
//		if(f_res == FR_OK)
//		{
//			printf("%d创建成功\r\n",i);
//		}
//		else
//		{
//			printf("%d创建失败,path:%s\r\n",i,path);
//		}
}
void Create_der(uint32_t i)
{
    static FRESULT f_res;                                                 // 文件操作结果                                           // 文件实际成功读写的字节数
		char path[64];
	
		lv_snprintf(path, sizeof(path), "0:/USER/%lu", (unsigned long)i);
		f_res = f_mkdir(path);
		if(f_res == FR_OK)
		{
			printf("%d创建成功\r\n",i);
		}
		else
		{
			printf("%d创建失败,path:%s\r\n",i,path);
		}
}
void Write_temperature(uint32_t i)
{
    static FIL myFile;                                                    // 文件对象; 这个结构体占用570字节，有点大，需用static修饰(存放在全局数据区), 避免stack溢出
    static FRESULT f_res;                                                 // 文件操作结果
    static uint32_t num;                                                  // 文件实际成功读写的字节数
    char aWriteBuf[64];      // 要写入的数据
		char path[64];
	
		lv_snprintf(path, sizeof(path), "0:/USER/%lu/data.txt", (unsigned long)i);
		lv_snprintf(aWriteBuf, sizeof(aWriteBuf), "%d℃ ", (unsigned long)i*10);
	
    f_res = f_open(&myFile, path, FA_OPEN_ALWAYS | FA_WRITE);   // 打开文件; 参数：要操作的文件对象、路径和文件名称、打开模式;
    if (f_res == FR_OK)
    {
        f_res = f_write(&myFile, aWriteBuf, sizeof(aWriteBuf), &num);     // 向文件内写入数据; 参数：文件对象、数据缓存、申请写入的字节数、实际写入的字节数
        if (f_res == FR_OK)
        {
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
}
void Write_power(uint32_t i)
{
    static FIL myFile;                                                    // 文件对象; 这个结构体占用570字节，有点大，需用static修饰(存放在全局数据区), 避免stack溢出
    static FRESULT f_res;                                                 // 文件操作结果
    static uint32_t num;                                                  // 文件实际成功读写的字节数
    char aWriteBuf[64];      // 要写入的数据
		char path[64];
	
		lv_snprintf(path, sizeof(path), "0:/USER/%lu/moon.txt", (unsigned long)i);
		lv_snprintf(aWriteBuf, sizeof(aWriteBuf), "%dkW ", (unsigned long)i*20);
	
    f_res = f_open(&myFile, path, FA_OPEN_ALWAYS | FA_WRITE);   // 打开文件; 参数：要操作的文件对象、路径和文件名称、打开模式;
    if (f_res == FR_OK)
    {
        f_res = f_write(&myFile, aWriteBuf, sizeof(aWriteBuf), &num);     // 向文件内写入数据; 参数：文件对象、数据缓存、申请写入的字节数、实际写入的字节数
        if (f_res == FR_OK)
        {
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
}
void Write_yongpower(uint32_t i)
{
    static FIL myFile;                                                    // 文件对象; 这个结构体占用570字节，有点大，需用static修饰(存放在全局数据区), 避免stack溢出
    static FRESULT f_res;                                                 // 文件操作结果
    static uint32_t num;                                                  // 文件实际成功读写的字节数
    char aWriteBuf[64];      // 要写入的数据
		char path[64];
	
		lv_snprintf(path, sizeof(path), "0:/USER/%lu/year.txt", (unsigned long)i);
		lv_snprintf(aWriteBuf, sizeof(aWriteBuf), "%dkWh ", (unsigned long)i*30);
	
    f_res = f_open(&myFile, path, FA_OPEN_ALWAYS | FA_WRITE);   // 打开文件; 参数：要操作的文件对象、路径和文件名称、打开模式;
    if (f_res == FR_OK)
    {
        f_res = f_write(&myFile, aWriteBuf, sizeof(aWriteBuf), &num);     // 向文件内写入数据; 参数：文件对象、数据缓存、申请写入的字节数、实际写入的字节数
        if (f_res == FR_OK)
        {
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
}


