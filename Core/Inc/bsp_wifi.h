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
/* AT 回显和透传数据是两股流，必须分两块缓冲。
 * 只用一块的后果（实测复现过）：
 *   1) 状态机还在等 CWJAP 的 OK，服务器推来的业务数据一帧就把 "WIFI GOT IP" 覆盖了
 *   2) 进了透传，AT 的回显 "AT+CIPSEND \r\n >" 也混进业务数据交给解析器
 *
 *   USART3_RxBuff   <- AT 阶段用（阻塞式收发），透传阶段不再往里写
 *   g_wifi_line[]   <- 透传阶段用，ISR 按 '\n' 切行塞进环形队列，主循环取走
 *
 * 接收不管哪个阶段都只挂一次 uart3，靠 g_wifi_passthru 分流。 */
extern uint8_t  USART3_RxBuff[BSP_WIFI_RX_BUF_SIZE];
extern uint16_t USART3_RxCounter;          /* 16 位：缓冲区是 1024 字节，8 位装不下 */

/* 透传期间 ISR 往哪塞数据。1 = 进业务环形队列，0 = 进 USART3_RxBuff。 */
extern volatile uint8_t g_wifi_passthru;

/* 透传下行行缓冲 / 队列，ISR 与主循环共用，定义在 usart.c */
#define WIFI_LINE_MAX     96              /* 单行上限，超了截断（含 '\n'） */
#define WIFI_LINE_SLOTS   4               /* 环形队列槽数 */

/* ISR 回调：usart.c 的 HAL_UARTEx_RxEventCallback 调，用户不要直接调 */
void Wifi_RxChunk(const uint8_t *data, uint16_t len);

/* 业务行环形队列（定义在 usart.c，Wifi_RxChunk 写、Wifi_TakeFrame 读） */
extern char              s_line[WIFI_LINE_SLOTS][WIFI_LINE_MAX];
extern volatile uint8_t  s_head;      /* ISR 写指针 */
extern volatile uint8_t  s_tail;      /* 主循环读指针 */
extern volatile uint8_t  s_len;       /* ISR 正在拼的当前行长度 */
extern volatile uint32_t s_overflow;  /* 队列满丢弃行数，调试用 */







/* ---------------------------------------------------- 非阻塞状态机 */
/* 命名规律：
 *   SEND_xxx —— 负责发指令，发完立刻切到对应的 WAIT_xxx
 *   WAIT_xxx —— 负责看缓冲区有没有想要的回应，以及判超时
 * 两个分开是为了「发完就能返回」，不在原地等。
 *
 * 为什么要 SEND/WAIT 分开：只有把等待搬到主循环里，WWDG 才喂得上。
 * 快指令（OK 几毫秒就回）用 Wifi_AtCmd 在原地等，省状态；
 * 慢指令（CWJAP 最长 30 秒、CIPSTART 10 秒）必须拆开，绝不原地等。 */
typedef enum {
    WIFI_IDLE = 0,          /* 空闲：上一轮彻底失败后在这里喘息 1 秒 */

    WIFI_SEND_RST,          /* 发 AT+RST */
    WIFI_WAIT_RST,          /* 等 "ready"（慢，拆开） */

    WIFI_SEND_CWMODE,       /* AT+CWMODE=1，用 Wifi_AtCmd 原地等 OK */
    WIFI_SEND_AUTOCONN,     /* AT+CWAUTOCONN=0，同上 */
    WIFI_SEND_JAP,          /* 发 AT+CWJAP="ssid","pwd" */
    WIFI_WAIT_JAP,          /* 等 "OK" / "WIFI GOT IP"，超时给 30 秒 */
    WIFI_SEND_MUX,          /* AT+CIPMUX=0，用 Wifi_AtCmd 原地等 OK */
    WIFI_SEND_MODE,         /* AT+CIPMODE=1，同上 */
    WIFI_SEND_START,        /* 发 AT+CIPSTART="TCP","ip",port */
    WIFI_WAIT_START,        /* 等 "CONNECT" / "OK"，超时 10 秒 */
    WIFI_SEND_SEND,         /* 发 AT+CIPSEND（不带数字） */
    WIFI_WAIT_SEND,         /* 等 ">" 提示符 */

    WIFI_READY              /* 已连上，透传中 */
} WifiState_t;

/* ------------------------------------------------------ 新增 API */
/* 主循环每圈调一次。绝不阻塞，最坏几微秒。 */
void    Wifi_Task(void);

/* 从业务行队列取一行，行尾 CR/LF 已剥掉。
 * 返回 1 = 取到（out 以 0 结尾）；0 = 队列空。 */
uint8_t Wifi_TakeFrame(char *out, uint16_t out_size);

/* 清空业务行队列。握手的 SEND_xxx 阶段调，防上一轮的残行 */
void    Wifi_FlushRx(void);




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

void ESP8266_Clear(void);                                     /* 清 AT 阶段缓冲区 */

/* 发一条 AT 指令，然后在 AT 缓冲区里等 expect。
 *
 * 为什么不用 strstr 一把梭：strstr 是子串搜索，"WIFI DISCONNECT" 里就含 "CONNECT"，
 * 判 TCP 建链会误命中（实测复现过）。这里做两个加强：
 *   ① 发之前清缓冲，断言命令回显已经出现 —— 保证看到的回应属于**这一条**指令
 *   ② 回应按行切，每行头部剥掉 CRLF 再用 strcmp 整行比对（不做子串搜索）
 * 返回 0 = 命中，1 = 超时。*/
uint8_t Wifi_AtCmd(const char *cmd, const char *expect, uint32_t timeout_ms);

int  WIFI_SendData(const char *data);                         /* 透传发数据 */

extern int connect_flag;    /* 1 = TCP 已建立（同时当锁用，防重发 AT） */

#endif /* __BSP_WIFI_H__ */

