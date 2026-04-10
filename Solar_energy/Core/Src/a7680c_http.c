#include "a7680c_http.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "a7680c.h"

// 默认 HTTP 配置
static http_config_t g_http_config = {
    .url = "",
    .connect_timeout = 120,
    .receive_timeout = 20,
    .content_type = "application/json",
    .accept_type = "application/json",
    .ssl_cfg_id = 0,
    .user_data = ""
};

// 初始化 HTTP 服务，这里填写1是因为前面在初始化PDP时候写的1
uint8_t A7680C_HTTP_Init(void)
{
    return A7680C_SendAT("AT+HTTPINIT=1\r\n", "OK", 120000,NULL);
}

// 终止 HTTP 服务
uint8_t A7680C_HTTP_Term(void)
{
    return A7680C_SendAT("AT+HTTPTERM=1\r\n", "OK", 60000,NULL);
}

// 设置URL参数
uint8_t A7680C_HTTP_SetConfig(void)
{
    uint8_t result;
		char cmd[512];
    
    // 设置 URL
    if (strlen(g_http_config.url) > 0) 
		{
        snprintf(cmd, sizeof(cmd), "AT+HTTPPARA=\"URL\",\"%s\"\r\n", g_http_config.url);
        result = A7680C_SendAT(cmd, "OK", 5000,NULL);
        if (result != AT_RESULT_OK) 
				{
            return result;
        }
    }  
    return AT_RESULT_OK;
}

// 发送GET请求,会读取到网页返回的字节数
uint8_t A7680C_HTTP_GET(uint8_t* data_buff)
{
		return A7680C_SendAT("AT+HTTPACTION=0\r\n", "HTTPACTION", 60000,data_buff);
}

//读取响应头
uint8_t A7680C_HTTP_HTTPHEAD(uint8_t* data_buff)
{
	return A7680C_SendAT("AT+HTTPHEAD\r\n", "OK", 60000,data_buff);
}

// 读取 HTTP 响应信息
uint8_t A7680C_HTTP_ReadResponse(uint32_t offset, uint32_t length,uint8_t* data_buff)
{
    char cmd[64];
    
    snprintf(cmd, sizeof(cmd), "AT+HTTPREAD=%d,%d\r\n", offset, length);
    return A7680C_SendAT(cmd, "HTTPREAD: 0", 60000,data_buff);
}
// 输入：包含 +HTTPACTION: 0,200,274 的字符串
// 输出：解析出来的 data_len（274）
uint32_t A7680C_ParseHttpActionDataLen(char *str)
{
    if (str == NULL) return 0;

    // 找 "+HTTPACTION:"
    char *p = strstr(str, "+HTTPACTION:");
    if (p == NULL) return 0;

    // 跳过 "+HTTPACTION:" 这串字符
    p += strlen("+HTTPACTION:");

    int method = 0;
    int status = 0;
    uint32_t data_len = 0;

    // 按格式 0,200,274 直接解析三个数字
    sscanf(p, "%d,%d,%d", &method, &status, &data_len);

    return data_len;
}

//解析天气JSON
WeatherCurrent_t Weather_ParseCurrent(const char *json)
{
    WeatherCurrent_t w = {0};
    
    // 1. 解析纬度
		char *lat = strstr(json, "\"latitude\":");
    if (lat)
		{
			sscanf(lat, "\"latitude\":%f,", &w.latitude);
		}
		else
		{
			printf("纬度解析失败\r\n");
			return w;
		}

    // 2. 解析经度
    char *lon = strstr(lat, "\"longitude\":");
    if (lon)
		{
			sscanf(lon, "\"longitude\":%f,", &w.longitude);
		}
		else
		{
			printf("经度解析失败\r\n");
			return w;
		}

    // 3. 解析天气代码
    char *current = strstr(json, "\"current\":{");
		if (!current)
		{
				printf("current字段不存在\r\n");
				return w;
		}

    // 在 current 内部找 weather_code
    char *p = strstr(current, "\"weather_code\":");
    if (p) 
		{
        sscanf(p, "\"weather_code\":%d", &w.weather_code);
    }
		else
		{
			printf("天气解析失败\r\n");
			return w;
		}
		
		char *day = strstr(current, "\"is_day\":");
		if (day)
		{
				sscanf(day, "\"is_day\":%d", &w.is_day);
		}
		else
		{
			printf("是否白天解析失败\r\n");
			return w;
		}

    return w;
}

// 获取天气数据
void A7680C_HTTP_GetWeatherData(float latitude, float longitude, WeatherCurrent_t *weather_data)
{
		uint8_t len_buff[32];
		uint8_t Weather_buff[512];
    if (weather_data == NULL) {//判读天气数据结构体是否为NULL
				printf("天气结构体为NULL\r\n");
        return;
    }
    
    // 构建 URL	
    snprintf(g_http_config.url, sizeof(g_http_config.url), "https://api.open-meteo.com/v1/forecast?latitude=%.4f&longitude=%.4f&current=weather_code,is_day&timezone=auto&forecast_days=1", 
             latitude, longitude);

		A7680C_HTTP_SetConfig();//设置URL
		A7680C_HTTP_GET(len_buff);//发送GET请求,此时网络中的数据就已经存储到了A7680C中	
		uint32_t len = A7680C_ParseHttpActionDataLen((char *)len_buff);//把网页获取到的字符串长度解析出来

		A7680C_HTTP_ReadResponse(0,len,Weather_buff);//一次读取len字节数据
		printf("%s",Weather_buff);
		*weather_data = Weather_ParseCurrent((const char *)Weather_buff);//解析JSON
}
//将天气代码翻译成中文
const char* Weather_GetShortDesc(int code)
{
    switch(code)
    {
        case 0:  	return "晴天 ";
        case 1:		return "晴转多云 ";
        case 2:		return "多云 ";
        case 3:  	return "阴天 ";
        case 45:	return "雾 ";
        case 48: 	return "霾 ";
        case 51:
        case 53:
        case 55: 
				case 56:
				case 57:
        case 61:	return "小雨 ";
        case 63:	return "中雨 ";
        case 65: 	return "大雨 ";
				case 66: 	return "冻雨 ";
				case 67: 	return "冻雨 ";
        case 71:	return "小雪 ";
        case 73:	return "中雪 ";
        case 75:	return "大雪 ";
				case 77: 	return "雪粒 ";
        case 80:
        case 81:
        case 82: 	return "阵雨 ";
        case 85:
        case 86: 	return "阵雪 ";
				case 95: 
				case 96: 	return "雷暴 ";
				case 99: 	return "冰雹 ";
        default: 	return "未知 ";
    }
}

