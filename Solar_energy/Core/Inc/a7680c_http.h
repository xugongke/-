#ifndef A7680C_HTTP_H
#define A7680C_HTTP_H

#include "a7680c_at.h"
#include <stdint.h>
#include <string.h>

// HTTP 状态码定义
typedef enum {
    HTTP_STATUS_CONTINUE = 100,
    HTTP_STATUS_OK = 200,
    HTTP_STATUS_CREATED = 201,
    HTTP_STATUS_ACCEPTED = 202,
    HTTP_STATUS_NO_CONTENT = 204,
    HTTP_STATUS_BAD_REQUEST = 400,
    HTTP_STATUS_UNAUTHORIZED = 401,
    HTTP_STATUS_FORBIDDEN = 403,
    HTTP_STATUS_NOT_FOUND = 404,
    HTTP_STATUS_INTERNAL_SERVER_ERROR = 500,
    HTTP_STATUS_BAD_GATEWAY = 502,
    HTTP_STATUS_SERVICE_UNAVAILABLE = 503,
    HTTP_STATUS_GATEWAY_TIMEOUT = 504,
    HTTP_ERROR_NETWORK = 601,
    HTTP_ERROR_NO_MEMORY = 602,
    HTTP_ERROR_DNS = 603,
    HTTP_ERROR_STACK_BUSY = 604,
    HTTP_ERROR_CONNECTION_CLOSED = 704,
    HTTP_ERROR_TIMEOUT = 705,
    HTTP_ERROR_SEND_RECV_FAILED = 706,
    HTTP_ERROR_FILE_NOT_FOUND = 707,
    HTTP_ERROR_INVALID_PARAM = 708,
    HTTP_ERROR_NETWORK_ERROR = 709,
    HTTP_ERROR_SSL_SESSION_FAILED = 710,
    HTTP_ERROR_WRONG_STATE = 711,
    HTTP_ERROR_CREATE_SOCKET_FAILED = 712,
    HTTP_ERROR_GET_DNS_FAILED = 713,
    HTTP_ERROR_CONNECT_SOCKET_FAILED = 714,
    HTTP_ERROR_HANDSHAKE_FAILED = 715,
    HTTP_ERROR_CLOSE_SOCKET_FAILED = 716,
    HTTP_ERROR_NO_NETWORK_ERROR = 717,
    HTTP_ERROR_SEND_TIMEOUT = 718,
    HTTP_ERROR_CA_MISSED = 719,
    HTTP_ERROR_UNKNOWN = 702
} http_status_t;

// HTTP 方法定义
typedef enum {
    HTTP_METHOD_GET = 0,
    HTTP_METHOD_POST = 1,
    HTTP_METHOD_HEAD = 2,
    HTTP_METHOD_DELETE = 3,
    HTTP_METHOD_PUT = 4,
    HTTP_METHOD_PATCH = 5
} http_method_t;

// HTTP 配置参数
typedef struct {
    char url[256];                // URL 地址
    uint32_t connect_timeout;     // 连接超时 (秒)
    uint32_t receive_timeout;     // 接收超时 (秒)
    char content_type[64];        // Content-Type
    char accept_type[64];         // Accept-Type
    uint8_t ssl_cfg_id;           // SSL 配置 ID
    char user_data[256];          // 自定义头部信息
} http_config_t;

// HTTP 响应数据
typedef struct {
    uint8_t *data;               // 响应数据指针
    uint32_t data_len;           // 响应数据长度
    http_status_t status_code;   // HTTP 状态码
    uint8_t is_chunked;          // 是否分块传输
} http_response_t;

// 函数原型声明


// 天气信息结构体
typedef struct {
    float latitude;
    float longitude;
    int   weather_code;
		int   is_day;   
} WeatherCurrent_t;

// 获取天气数据的函数
void A7680C_HTTP_GetWeatherData(float latitude, float longitude, WeatherCurrent_t *weather_data);
const char* Weather_GetShortDesc(int code);
uint8_t A7680C_HTTP_Init(void);
#endif // A7680C_HTTP_H
