#include "a7680c_at.h"
#include "a7680c.h"
#include "main.h"
#include "rx8025t.h"
#include "rx8025t_example.h"
/* AT测试 */
uint8_t A7680C_Test(void)
{
    return A7680C_SendAT("AT\r\n","OK",1000,NULL);
}
/* 查询模块信息 */ 
uint8_t A7680C_SendCmdWaitAck(uint8_t* data_buff)
{
    return A7680C_SendAT("ATI\r\n","OK",500,data_buff);
}
/* 启用命令回显 */ 
uint8_t A7680C_SendATE1(void)
{
    return A7680C_SendAT("ATE1\r\n","OK",200,NULL);
}
/* 关闭命令回显 */ 
uint8_t A7680C_SendATE0(void)
{
    return A7680C_SendAT("ATE0\r\n","OK",200,NULL);
}
/* 保存设置 */ 
uint8_t A7680C_SendATW(void)
{
    return A7680C_SendAT("AT&W\r\n","OK",500,NULL);
}
/* 重启模块 */ 
uint8_t A7680C_SendAT_CFUN(void)
{
    return A7680C_SendAT("AT+CFUN=1,1\r\n",NULL,500,NULL);
}
/*
 * 查询 SIM 卡状态
 * 作用：
 * 1. 检查 SIM 卡是否已经插好、识别正常
 * 2. 判断模块是否已经可以使用蜂窝网络
 *
 * 常见返回：
 * +CPIN: READY    表示 SIM 卡正常
 *
 * 说明：
 * 如果这里不正常，后面的附网、拨号、NTP 校时都无法进行
 */
uint8_t A7680C_SendAT_CPIN(void)
{
    return A7680C_SendAT("AT+CPIN?\r\n", "OK", 500,NULL);
}

/*
 * 查询信号强度
 * 作用：
 * 1. 检查当前蜂窝网络信号是否正常
 * 2. 判断模块当前所在位置是否具备联网条件
 *
 * 常见返回：
 * +CSQ: <rssi>,<ber>
 *
 * 说明：
 * rssi 越大，信号一般越好
 * 如果信号太差，可能导致附网失败、NTP 校时失败
 */
uint8_t A7680C_SendAT_CSQ(uint8_t* data_buff)
{
    return A7680C_SendAT("AT+CSQ\r\n", "OK", 500,data_buff);
}

/*
 * 附着分组域网络
 * 作用：
 * 1. 让模块接入移动数据网络
 * 2. 为后续 PDP 激活、访问 NTP 服务器做准备
 *
 * 说明：
 * 只有成功附着网络后，模块才可以进行数据通信
 * 这个命令相当于“打开移动数据能力”
 */
uint8_t A7680C_SendAT_CGATT_SET(void)
{
    return A7680C_SendAT("AT+CGATT=1\r\n", "OK", 3000,NULL);
}

/*
 * 查询当前分组域附着状态
 * 作用：
 * 1. 检查模块是否已经成功附着到移动数据网络
 * 2. 验证上一步 AT+CGATT=1 是否生效
 *
 * 常见返回：
 * +CGATT: 1   表示已附着
 * +CGATT: 0   表示未附着
 *
 * 说明：
 * 如果这里不是 1，后面的 NTP 校时一般不会成功
 */
uint8_t A7680C_SendAT_CGATT_READ(void)
{
    return A7680C_SendAT("AT+CGATT?\r\n", "OK", 500,NULL);
}

/*
 * 配置 PDP 上下文
 * 作用：
 * 1. 设置数据业务使用的 PDP 类型和 APN
 * 2. 告诉模块通过哪个运营商接入点上网
 *
 * 参数说明：
 * 1        -> PDP 上下文编号
 * "IP"     -> 使用 IPv4
 * "CMNET"  -> APN，需根据运营商修改
 *
 * 说明：
 * 如果 APN 配错，模块可能附网成功，但仍然无法正常访问网络
 * 中国移动常见 APN 为 CMNET
 * 中国联通、中国电信需要改成各自对应 APN
 */
uint8_t A7680C_SendAT_CGDCONT(void)
{
    return A7680C_SendAT("AT+CGDCONT=1,\"IP\",\"CMNET\"\r\n", "OK", 500,NULL);
}

/*
 * 启用时区自动更新
 * 作用：
 * 1. 允许模块根据网络信息自动更新时区
 * 2. 避免时间同步成功后时区不正确
 *
 * 参数说明：
 * 1 -> 开启自动时区更新
 *
 * 说明：
 * 这个命令不是 NTP 校时的核心命令，但建议开启
 * 这样在某些网络环境下，模块能自动修正时区信息
 */
uint8_t A7680C_SendAT_CTzu(void)
{
    return A7680C_SendAT("AT+CTZU=1\r\n", "OK", 500,NULL);
}

/*
 * 配置 NTP 功能
 * 作用：
 * 1. 开启或初始化模块的 NTP 时间同步功能
 * 2. 为后续执行 AT+CNTP 做准备
 *
 * 参数说明：
 * 1 -> 使能 NTP 相关配置
 *
 * 说明：
 * 这是进行 NTP 校时前的准备命令
 * 不同固件版本对这个命令的细节可能略有差异
 */
uint8_t A7680C_SendAT_CNTPCFG(void)
{
    return A7680C_SendAT("AT+CHTPCFG=\"CID\",1\r\n", "OK", 500,NULL);
}

/*
 * 设置 NTP 服务器和时区
 * 作用：
 * 1. 指定模块向哪个 NTP 服务器获取当前标准时间
 * 2. 设置本地时区为北京时间
 *
 * 参数说明：
 * "ntp1.aliyun.com" -> NTP 服务器地址
 * 32                -> 时区，单位是 15 分钟
 *
 * 时区换算：
 * 32 * 15 分钟 = 480 分钟 = UTC+8
 * 也就是北京时间
 *
 * 说明：
 * 如果这个服务器访问不通，可以换成别的 NTP 服务器
 * 例如：
 * "time.google.com"
 * "ntp.aliyun.com"
 * "cn.ntp.org.cn"
 */
uint8_t A7680C_SendAT_CNTP_SERVER(void)
{
    return A7680C_SendAT("AT+CNTP=\"ntp1.aliyun.com\",32\r\n", "OK", 1000,NULL);
}

/*
 * 执行 NTP 校时
 * 作用：
 * 1. 让模块主动连接前面设置好的 NTP 服务器
 * 2. 从网络获取当前标准时间
 * 3. 用获取到的时间更新模块内部 RTC
 *
 * 常见返回：
 * +CNTP: 0    表示校时成功
 * +CNTP: 其他 表示校时失败
 *
 * 说明：
 * 这是“获取网络当前时间”的核心命令
 * 执行成功后，再用 AT+CCLK? 读取，时间才会准确
 * 因为访问网络服务器需要时间，所以超时时间要设置得长一些
 */
uint8_t A7680C_SendAT_CNTP(void)
{
    return A7680C_SendAT("AT+CNTP\r\n", "+CNTP:", 10000,NULL);
}
/*
 * 读取模块当前 RTC 时间
 * 作用：
 * 1. 查询模块当前内部时钟的值
 * 2. 检查 NTP 校时之后，RTC 是否已经被更新为正确时间
 *
 * 常见返回：
 * +CCLK: "yy/MM/dd,hh:mm:ss+zz"
 *
 * 说明：
 * 这个命令本身不是去“联网获取时间”
 * 它只是把模块当前保存的 RTC 时间读出来
 *
 * 也就是说：
 * 先执行 AT+CNTP 完成网络校时
 * 再执行 AT+CCLK? 才能读到准确时间
 *
 * 如果你读到：
 * +CCLK: "70/01/01,00:14:01+00"
 * 这通常表示 RTC 还没有被正确校时
 */
uint8_t A7680C_SendAT_CCLK(uint8_t* data_buff)
{
    return A7680C_SendAT("AT+CCLK?\r\n", "OK", 500,data_buff);
}
//解析时间戳函数
uint8_t A7680C_ParseCCLK(const uint8_t *at_parse_buf, RX8025T_DateTimeCompact *time)
{
    int yy, MM, dd, hh, mm, ss, tz;
    const char *p;

    if (at_parse_buf == NULL || time == NULL)
    {
        return 0;
    }

    /* 查找 +CCLK: 这一段的起始位置 */
    p = strstr((const char *)at_parse_buf, "+CCLK:");
    if (p == NULL)
    {
        return 0;
    }

    /*
     * 解析格式：
     * +CCLK: "26/03/27,08:57:29+32"
     *
     * %2d -> 年
     * %2d -> 月
     * %2d -> 日
     * %2d -> 时
     * %2d -> 分
     * %2d -> 秒
     * %d  -> 时区
     */
    if (sscanf(p, "+CCLK: \"%2d/%2d/%2d,%2d:%2d:%2d%d\"",
               &yy, &MM, &dd, &hh, &mm, &ss, &tz) != 7)
    {
        return 0;
    }

    /* 注意A7680C 返回的是两位年份，后面要进行处理 */
    time->year     = (uint8_t)yy;
    time->month    = (uint8_t)MM;
    time->day      = (uint8_t)dd;
    time->hours     = (uint8_t)hh;
    time->minutes   = (uint8_t)mm;
    time->seconds   = (uint8_t)ss;

    return 1;
}
// 解析CSQ信号强度函数
uint8_t A7680C_ParseCSQ(const uint8_t *at_parse_buf, int32_t *rssi, int32_t *ber)
{
    const char *p;

    // 空指针检查
    if (at_parse_buf == NULL || rssi == NULL || ber == NULL)
    {
        return 0;
    }

    /* 查找 +CSQ: 这一段的起始位置 */
    p = strstr((const char *)at_parse_buf, "+CSQ:");
    if (p == NULL)
    {
        return 0;
    }

    /*
     * 解析格式：
     * +CSQ: 28,99
     *
     * %d -> RSSI 信号强度（0-31，99=未知）
     * %d -> 信道误码率
     */
    if (sscanf(p, "+CSQ: %d,%d", rssi, ber) != 2)
    {
        return 0;
    }

    return 1;
}
/**
 * @brief  解析 +CLBS 经纬度
 * @param  buf: 存放AT指令返回的缓冲区（at_parse_buf）
 * @retval 解析后的经纬度
 */
CLBS_PosTypeDef A7680C_ParseCLBS(char *buf)
{
    CLBS_PosTypeDef pos = {0};

    if (buf == NULL) {
        return pos;
    }

    // 查找 +CLBS:
    char *p = strstr(buf, "+CLBS:");
    if (p == NULL) {
        return pos;
    }

    // 跳过 "+CLBS:" 字符串
    p += strlen("+CLBS:");

    // 格式：+CLBS: 0,lat,lng,xxx
    // 直接按格式解析 4 个值，取中间两个就是经纬度
    int flag = 0;
    int xxx = 0;
    sscanf(p, "%d,%f,%f,%d", &flag, &pos.latitude, &pos.longitude, &xxx);

    return pos;
}

/*
 * 获取网络时间，并返回失败发生在哪一步
 * 适合调试阶段使用
 */
at_result_t A7680C_GetNetworkTime_Debug(uint8_t *step)
{
    at_result_t ret;

    if (step == NULL)
    {
        return AT_RESULT_ERROR;
    }

    *step = 1;
    ret = A7680C_SendAT_CPIN();
    if (ret != AT_RESULT_OK) return ret;

    *step = 3;
    ret = A7680C_SendAT_CGATT_SET();
    if (ret != AT_RESULT_OK) return ret;

    *step = 4;
    ret = A7680C_SendAT_CGATT_READ();
    if (ret != AT_RESULT_OK) return ret;

    *step = 5;
    ret = A7680C_SendAT_CGDCONT();
    if (ret != AT_RESULT_OK) return ret;

    *step = 6;
    ret = A7680C_SendAT_CTzu();
    if (ret != AT_RESULT_OK) return ret;

    *step = 7;
    ret = A7680C_SendAT_CNTPCFG();
    if (ret != AT_RESULT_OK) return ret;

    *step = 8;
    ret = A7680C_SendAT_CNTP_SERVER();
    if (ret != AT_RESULT_OK) return ret;

    *step = 9;
    ret = A7680C_SendAT_CNTP();
    if (ret != AT_RESULT_OK) return ret;

    *step = 10;
		uint8_t time_buff[128];
    ret = A7680C_SendAT_CCLK(time_buff);
    if (ret != AT_RESULT_OK)
		{
			return ret;
		}
		else
		{
			RX8025T_DateTimeCompact DateTim;
			A7680C_ParseCCLK(time_buff,&DateTim);//解析获取网络时间
			if(RX8025T_SetDateTime(&DateTim) != HAL_OK)//把解析获得的网络时间更新到RTC芯片中
			{
				return AT_RESULT_ERROR;
			}
		}
    *step = 0;
    return AT_RESULT_OK;
}

