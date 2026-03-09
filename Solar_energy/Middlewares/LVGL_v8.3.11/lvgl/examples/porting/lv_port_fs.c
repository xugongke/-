/**
 * @file lv_port_fs.c
 *
 */

/*Copy this file as "lv_port_fs.c" and set this value to "1" to enable content*/
#if 1

/*********************
 *      INCLUDES
 *********************/
#include "lv_port_fs.h"
#include "lvgl.h"

#include "stdio.h" //FATFS头文件
#include "ff.h" //FATFS头文件
/*********************
 *      DEFINES
 *********************/

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *  STATIC PROTOTYPES
 **********************/
static void fs_init(void);

static void * fs_open(lv_fs_drv_t * drv, const char * path, lv_fs_mode_t mode);
static lv_fs_res_t fs_close(lv_fs_drv_t * drv, void * file_p);
static lv_fs_res_t fs_read(lv_fs_drv_t * drv, void * file_p, void * buf, uint32_t btr, uint32_t * br);
static lv_fs_res_t fs_write(lv_fs_drv_t * drv, void * file_p, const void * buf, uint32_t btw, uint32_t * bw);
static lv_fs_res_t fs_seek(lv_fs_drv_t * drv, void * file_p, uint32_t pos, lv_fs_whence_t whence);
static lv_fs_res_t fs_size(lv_fs_drv_t * drv, void * file_p, uint32_t * size_p);
static lv_fs_res_t fs_tell(lv_fs_drv_t * drv, void * file_p, uint32_t * pos_p);

static void * fs_dir_open(lv_fs_drv_t * drv, const char * path);
static lv_fs_res_t fs_dir_read(lv_fs_drv_t * drv, void * rddir_p, char * fn);
static lv_fs_res_t fs_dir_close(lv_fs_drv_t * drv, void * rddir_p);

/**********************
 *  STATIC VARIABLES
 **********************/

/**********************
 * GLOBAL PROTOTYPES
 **********************/

/**********************
 *      MACROS
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

void lv_port_fs_init(void)
{
    /*----------------------------------------------------
     * Initialize your storage device and File System
     * -------------------------------------------------*/
    fs_init();

    /*---------------------------------------------------
     * Register the file system interface in LVGL
     *--------------------------------------------------*/

    /*Add a simple drive to open images*/
    static lv_fs_drv_t fs_drv;
    lv_fs_drv_init(&fs_drv);

    /*Set up fields...*/
    fs_drv.letter = 'S';
    fs_drv.open_cb = fs_open;
    fs_drv.close_cb = fs_close;
    fs_drv.read_cb = fs_read;
    fs_drv.write_cb = fs_write;
    fs_drv.seek_cb = fs_seek;
    fs_drv.tell_cb = fs_tell;

    fs_drv.dir_close_cb = fs_dir_close;
    fs_drv.dir_open_cb = fs_dir_open;
    fs_drv.dir_read_cb = fs_dir_read;

    lv_fs_drv_register(&fs_drv);
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

/*Initialize your Storage device and File system.*/
static void fs_init(void)
{
    /*E.g. for FatFS initialize the SD card and FatFS itself*/

    /*You code here*/
		static FATFS myFatFs;   // 文件系统对象
		// 挂载 FATFS
    FRESULT f_res = f_mount(&myFatFs, "0:", 1);
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
        if (myFatFs.fs_type == 0x03)                                      // FAT32; 1-FAT12、2-FAT16、3-FAT32、4-exFat
            printf("SD卡已有文件系统：FAT32\n");
        if (myFatFs.fs_type == 0x04)                                      // exFAT; 1-FAT12、2-FAT16、3-FAT32、4-exFat
            printf("SD卡已有文件系统：exFAT\n");                         
        printf("FatFs 挂载成功 \r\n");                                    // 挂载成功
    }
}

/**
 * Open a file
 * @param drv       pointer to a driver where this function belongs
 * @param path      path to the file beginning with the driver letter (e.g. S:/folder/file.txt)
 * @param mode      read: FS_MODE_RD, write: FS_MODE_WR, both: FS_MODE_RD | FS_MODE_WR
 * @return          a file descriptor or NULL on error
 */
 
static void * fs_open(lv_fs_drv_t * drv, const char * path, lv_fs_mode_t mode)
{
  lv_fs_res_t res = LV_FS_RES_NOT_IMP;
	BYTE fatfs_mode = 0;
	/*把LVGL的打开文件的模式和FatFs一一对应好*/
	//FA_OPEN_ALWAYS 如果文件存在，则打开；否则，创建一个新文件。
	if(mode == LV_FS_MODE_WR) fatfs_mode = FA_WRITE | FA_OPEN_ALWAYS;
	else if(mode == LV_FS_MODE_RD) fatfs_mode = FA_READ | FA_OPEN_ALWAYS;
	else if(mode == (LV_FS_MODE_WR | LV_FS_MODE_RD)) fatfs_mode = FA_READ | FA_WRITE | FA_OPEN_ALWAYS;

	/* LVGL 传进来的path已经没有盘符，比如 "test.txt" ,我们需要把盘符补充上去，然后才能给f_open使用*/
	char full_path[256];
	lv_snprintf(full_path, 256, "0:%s", path);  // 注意 path 已经是 "/xxx"
	//printf("fs_open full_path=%s \n",full_path);
	FIL * f = lv_mem_alloc(sizeof(FIL));   // 为每个文件分配空间
	
  if(f == NULL)
	{
		printf("lv_mem_alloc 失败 ! (f == NULL)\r\n");
		return NULL;
	}
	FRESULT fr = f_open(f, full_path, fatfs_mode);
	if(fr == FR_OK) 
	{
		printf("fs_open full_path=%s  OK !\n",full_path);
		return f;
  }
	else 
	{
		printf("f_open 失败: %d, full_path=%s\n", fr, full_path);
		lv_mem_free(f);
		return NULL;
  }
}
 

/**
 * Close an opened file
 * @param drv       pointer to a driver where this function belongs
 * @param file_p    pointer to a file_t variable. (opened with fs_open)
 * @return          LV_FS_RES_OK: no error or  any error from @lv_fs_res_t enum
 */
static lv_fs_res_t fs_close(lv_fs_drv_t * drv, void * file_p)
{
		LV_UNUSED(drv);
		if(FR_OK == f_close((FIL*)file_p)) 
		{
			lv_mem_free(file_p);
			return LV_FS_RES_OK;
		}
		else
		{
			printf("调用fs_close失败\r\n");
			lv_mem_free(file_p);
			return LV_FS_RES_UNKNOWN;
		}
}

/**
 * Read data from an opened file
 * @param drv       pointer to a driver where this function belongs
 * @param file_p    pointer to a file_t variable.
 * @param buf       pointer to a memory block where to store the read data
 * @param btr       number of Bytes To Read
 * @param br        the real number of read bytes (Byte Read)
 * @return          LV_FS_RES_OK: no error or  any error from @lv_fs_res_t enum
 */
static lv_fs_res_t fs_read(lv_fs_drv_t * drv, void * file_p, void * buf, uint32_t btr, uint32_t * br)
{
    /*Add your code here*/
		LV_UNUSED(drv);
 
    FRESULT res = f_read(file_p, buf, btr, (UINT *)br);
    if(res == FR_OK) return LV_FS_RES_OK;
    else return LV_FS_RES_UNKNOWN;
}

/**
 * Write into a file
 * @param drv       pointer to a driver where this function belongs
 * @param file_p    pointer to a file_t variable
 * @param buf       pointer to a buffer with the bytes to write
 * @param btw       Bytes To Write
 * @param bw        the number of real written bytes (Bytes Written). NULL if unused.
 * @return          LV_FS_RES_OK: no error or  any error from @lv_fs_res_t enum
 */
static lv_fs_res_t fs_write(lv_fs_drv_t * drv, void * file_p, const void * buf, uint32_t btw, uint32_t * bw)
{
    /*Add your code here*/
		LV_UNUSED(drv);
    FRESULT res = f_write(file_p, buf, btw, (UINT *)bw);
    if(res == FR_OK) return LV_FS_RES_OK;
    else return LV_FS_RES_UNKNOWN;
}

/**
 * Set the read write pointer. Also expand the file size if necessary.
 * @param drv       pointer to a driver where this function belongs
 * @param file_p    pointer to a file_t variable. (opened with fs_open )
 * @param pos       the new position of read write pointer
 * @param whence    tells from where to interpret the `pos`. See @lv_fs_whence_t
 * @return          LV_FS_RES_OK: no error or  any error from @lv_fs_res_t enum
 */
static lv_fs_res_t fs_seek(lv_fs_drv_t * drv, void * file_p, uint32_t pos, lv_fs_whence_t whence)
{
    /*Add your code here*/
	LV_UNUSED(drv);
    switch(whence) {
        case LV_FS_SEEK_SET:
            f_lseek(file_p, pos);
            break;
        case LV_FS_SEEK_CUR:
            f_lseek(file_p, f_tell((FIL *)file_p) + pos);
            break;
        case LV_FS_SEEK_END:
            f_lseek(file_p, f_size((FIL *)file_p) + pos);
            break;
        default:
            break;
    }
    return LV_FS_RES_OK;
}
/**
 * Give the position of the read write pointer
 * @param drv       pointer to a driver where this function belongs
 * @param file_p    pointer to a file_t variable.
 * @param pos_p     pointer to to store the result
 * @return          LV_FS_RES_OK: no error or  any error from @lv_fs_res_t enum
 */
static lv_fs_res_t fs_tell(lv_fs_drv_t * drv, void * file_p, uint32_t * pos_p)
{
    /*Add your code here*/
	LV_UNUSED(drv);
    *pos_p = f_tell((FIL *)file_p);
    return LV_FS_RES_OK;
}

/**
 * Initialize a 'lv_fs_dir_t' variable for directory reading
 * @param drv       pointer to a driver where this function belongs
 * @param path      path to a directory
 * @return          pointer to the directory read descriptor or NULL on error
 */
static void * fs_dir_open(lv_fs_drv_t * drv, const char * path)
{
	 LV_UNUSED(drv);
	DIR * d = lv_mem_alloc(sizeof(DIR));
	if(d == NULL) return NULL;
	
	/*Make the path relative to the current directory (the projects root folder)*/
	char real_path[256];
	lv_snprintf(real_path, 256, "0:%s", path);
	FRESULT res = f_opendir(d, real_path);
	if(res != FR_OK) {
			lv_mem_free(d);
			d = NULL;
	}
	return d;
}

/**
 * Read the next filename form a directory.
 * The name of the directories will begin with '/'
 * @param drv       pointer to a driver where this function belongs
 * @param rddir_p   pointer to an initialized 'lv_fs_dir_t' variable
 * @param fn        pointer to a buffer to store the filename
 * @return          LV_FS_RES_OK: no error or  any error from @lv_fs_res_t enum
 */
static lv_fs_res_t fs_dir_read(lv_fs_drv_t * drv, void * rddir_p, char * fn)
{
	LV_UNUSED(drv);
    FRESULT res;
    FILINFO fno;
    fn[0] = '\0';
    do {
        res = f_readdir(rddir_p, &fno);
        if(res != FR_OK) return LV_FS_RES_UNKNOWN;
 
        if(fno.fattrib & AM_DIR) {
            fn[0] = '/';
            strcpy(&fn[1], fno.fname);
        }
        else strcpy(fn, fno.fname);
 
    } while(strcmp(fn, "/.") == 0 || strcmp(fn, "/..") == 0);
 
    return LV_FS_RES_OK;
}

/**
 * Close the directory reading
 * @param drv       pointer to a driver where this function belongs
 * @param rddir_p   pointer to an initialized 'lv_fs_dir_t' variable
 * @return          LV_FS_RES_OK: no error or  any error from @lv_fs_res_t enum
 */
static lv_fs_res_t fs_dir_close(lv_fs_drv_t * drv, void * rddir_p)
{
	LV_UNUSED(drv);
	if(FR_OK == f_closedir(rddir_p)) 
	{
		lv_mem_free(rddir_p);
		return LV_FS_RES_OK;
	}
    else
	{
		lv_mem_free(rddir_p);
		return LV_FS_RES_UNKNOWN;
	}
}

#else /*Enable this file at the top*/

/*This dummy typedef exists purely to silence -Wpedantic.*/
typedef int keep_pedantic_happy;
#endif
