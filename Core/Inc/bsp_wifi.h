/* ============================================================================
 * bsp_wifi.h  -  ESP8266 驱动（非阻塞状态机 + 透传模式）
 *
 * 本工程的接线：
 *   USART3  <->  ESP8266     （PB10 TX / PB11 RX，115200 8N1）
 *   USART6  <->  PC / ST-Link VCP 调试台（PG14 TX / PG9 RX）
 * ==========================================================================*/

#ifndef __BSP_WIFI_H__
#define __BSP_WIFI_H__

#include "usart.h"
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

/* ---------------------------------------------------------------- 缓冲区 */
/* ESP8266 串口用的一整块缓冲，由 HAL_UARTEx_RxEventCallback 填。
 * 大小 WIFI_RX_BUF_SIZE 定义在 usart.h 里（usart.c 也要用同一个值）。 */
extern uint8_t  USART3_RxBuff[BSP_WIFI_RX_BUF_SIZE];
extern uint16_t USART3_RxCounter;          /* 16 位：缓冲区是 1024 字节，8 位装不下 */

#define WiFi_RX_BUF     USART3_RxBuff      /* 别名，wifi.c 里用这个名字 */
#define WiFi_RxCounter  USART3_RxCounter






/* ---------------------------------------------------- 非阻塞状态机 */
/* 命名规律：
 *   SEND_xxx —— 负责发指令，发完立刻切到对应的 WAIT_xxx
 *   WAIT_xxx —— 负责看缓冲区有没有想要的回应，以及判超时
 * 两个分开是为了「发完就能返回」，不在原地等。 */
typedef enum {
    WIFI_IDLE = 0,          /* 空闲：上一轮彻底失败后在这里喘息 1 秒 */

    WIFI_SEND_RST,          /* 发 AT+RST */
    WIFI_WAIT_RST,          /* 等 "ready" */
    WIFI_SEND_CWMODE,       /* 发 AT+CWMODE=1 */
    WIFI_WAIT_CWMODE,       /* 等 "OK" */
    WIFI_SEND_AUTOCONN,     /* 发 AT+CWAUTOCONN=0 */
    WIFI_WAIT_AUTOCONN,     /* 等 "OK" */
    WIFI_SEND_JAP,          /* 发 AT+CWJAP="ssid","pwd" */
    WIFI_WAIT_JAP,          /* 等 "OK" / "WIFI GOT IP"，超时给 30 秒 */
    WIFI_SEND_MUX,          /* 发 AT+CIPMUX=0 */
    WIFI_WAIT_MUX,          /* 等 "OK" */
    WIFI_SEND_MODE,         /* 发 AT+CIPMODE=1 */
    WIFI_WAIT_MODE,         /* 等 "OK" */
    WIFI_SEND_START,        /* 发 AT+CIPSTART="TCP","ip",port */
    WIFI_WAIT_START,        /* 等 "CONNECT" / "OK" */
    WIFI_SEND_SEND,         /* 发 AT+CIPSEND（不带数字） */
    WIFI_WAIT_SEND,         /* 等 ">" 提示符 */

    WIFI_READY              /* 已连上，透传中 */
} WifiState_t;

/* ------------------------------------------------------ 新增 API */
/* 主循环每圈调一次。绝不阻塞，最坏几微秒。 */
void    Wifi_Task(void);

/* 从接收缓冲区取一帧，取完立刻清。
 * 返回 1 = 有数据（out 里是 \0 结尾的字符串）；0 = 没数据。 */
uint8_t Wifi_TakeFrame(char *out, uint16_t out_size);



/* ------------------------------------------------------------ 串口映射 */
/* ESP8266 走 USART3，调试台走 USART6。这两个宏是全文的关键：
   写错一个，数据就跑到错的串口上，现象是「有输出但对方没收到」。 */
#define ESP8266_USART(fmt, ...)  USART_printf(&huart3, fmt, ##__VA_ARGS__)
#define u1_USART(fmt, ...)       USART_printf(&huart6, fmt, ##__VA_ARGS__)

/* --------------------------------------------------------------- WiFi / TCP */
#define ID          "bt"                 /* 路由器 SSID */
#define PASSWORD    "00000000"           /* 路由器密码 */

#define ServerIP    "129.211.183.41"     /* 课堂调试台 IP，会变，以 8090 页面为准 */
#define ServerPort  9015                 /* TCP 端口，同样以页面为准 */

/* -------------------------------------------------------------------- API */
void USART_printf(UART_HandleTypeDef *huart, const char *fmt, ...);

void ESP8266_Clear(void);                                     /* 清接收缓冲区 */
int  WIFI_SendData(const char *data);                         /* 透传发数据 */

extern int connect_flag;    /* 1 = TCP 已建立（同时当锁用，防重发 AT） */

#endif /* __BSP_WIFI_H__ */

