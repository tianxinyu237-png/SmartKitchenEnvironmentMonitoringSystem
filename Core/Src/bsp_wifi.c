
/* ============================================================================
 * bsp_wifi.c  -  ESP8266 驱动（非阻塞状态机 + 透传模式）
 *
 * 从老师给的阻塞版参考代码改写而来。原版是 while(1) + HAL_Delay 死等，
 * 跟 WWDG 49.93ms 超时互斥；现在改成主循环驱动的状态机，每圈只推进一步：
 *   - ESP8266 接的是 USART3（参考代码里误写成 USART5）。
 *   - 调试台是 USART6（参考代码用的是 USART1）。
 *   - 调试输出一律用英文字符串，中文含义写在行尾注释里。
 *     原因：本机这版 UartAssist 只有 ASCII / HEX，没有编码选项，
 *     中文会按 GBK 拆 UTF-8 的字节，显示成一堆鬼画符。
 *   - 接收不在这里挂钩子：main() 里挂一次，HAL_UARTEx_RxEventCallback
 *     (usart.c) 每帧自己重新挂，把数据填进 USART3_RxBuff。
 *     本文件只去轮询那块缓冲区，从不碰 UART 驱动。
 * ==========================================================================*/

#include "bsp_wifi.h"
#include "bsp_tick.h"      /* Tick_Expired —— 非阻塞状态机的时间判断靠它 */

int connect_flag = 0;              /* 1 = TCP 已建立            */




/* 透传模式下：传什么就发什么，不用 AT+CIPSEND=n、不用数长度。 */
int WIFI_SendData(const char *data)
{
    if (!connect_flag)                 /* 没连上就不发，避免数据被当 AT 指令解析 */
        return -1;
    ESP8266_USART("%s", data);
    return 0;
}


/* ============================================================
 * Wifi_Task() —— ESP8266 连接状态机（非阻塞版）
 *
 * 主循环每圈调一次。每次只做一件事，做完立刻返回。
 * 最坏执行时间：几十个机器周期。
 * ============================================================ */
void Wifi_Task(void)
{
    /* 这四个必须 static —— 要跨调用保持 */
    static WifiState_t st     = WIFI_IDLE;  /* 当前状态。初值 IDLE = 上电先等 1 秒 */
    static uint32_t    t_mark = 0;          /* 本步开始时刻(ms) */
    static uint8_t     retry  = 0;          /* 本步已重试次数 */
    static uint32_t    t_idle = 0;          /* 进 IDLE 的时刻 */

    switch (st)
    {
    /* ==================== 空闲喘息 ==================== */
    case WIFI_IDLE:
        if (Tick_Expired(&t_idle, 1000))    /* 等够 1 秒 */
        {
            retry = 0;
            st    = WIFI_SEND_RST;          /* 从头开始 */
        }
        break;

    /* ==================== 第 1 步：AT+RST ==================== */
    case WIFI_SEND_RST:
        ESP8266_Clear();                    /* ★ 发前必须清，防残留 "OK" 误判 */
        ESP8266_USART("AT+RST\r\n");        /* 发到 USART3 → ESP8266 */
        t_mark = HAL_GetTick();             /* 记起始时刻 */
        st     = WIFI_WAIT_RST;             /* 立刻回，不在这等 */
        break;

    case WIFI_WAIT_RST:
        if (strstr((char *)USART3_RxBuff, "ready"))
        {
            retry = 0;                      /* 本步成功，计数清零 */
            st    = WIFI_SEND_CWMODE;
        }
        else if (Tick_Expired(&t_mark, 5000))           /* 5 秒超时 */
        {
            if (++retry < 3) { st = WIFI_SEND_RST; }    /* 重试同一步 */
            else { st = WIFI_IDLE; t_idle = HAL_GetTick(); retry = 0; }
        }
        break;

    /* ==================== 第 2 步：CWMODE ==================== */
    case WIFI_SEND_CWMODE:
        ESP8266_Clear();
        ESP8266_USART("AT+CWMODE=1\r\n");   /* 1 = STA，当客户端连路由器 */
        t_mark = HAL_GetTick();
        st     = WIFI_WAIT_CWMODE;
        break;

    case WIFI_WAIT_CWMODE:
        if (strstr((char *)USART3_RxBuff, "OK"))
        {
            retry = 0;
            st    = WIFI_SEND_AUTOCONN;
        }
        else if (Tick_Expired(&t_mark, 5000))
        {
            if (++retry < 3) { st = WIFI_SEND_CWMODE; }
            else { st = WIFI_IDLE; t_idle = HAL_GetTick(); retry = 0; }
        }
        break;

    /* ==================== 第 3 步：CWAUTOCONN ==================== */
    case WIFI_SEND_AUTOCONN:
        ESP8266_Clear();
        ESP8266_USART("AT+CWAUTOCONN=0\r\n");   /* 0 = 关自动连接，连接权交给 MCU */
        t_mark = HAL_GetTick();
        st     = WIFI_WAIT_AUTOCONN;
        break;

    case WIFI_WAIT_AUTOCONN:
        if (strstr((char *)USART3_RxBuff, "OK"))
        {
            retry = 0;
            st    = WIFI_SEND_JAP;
        }
        else if (Tick_Expired(&t_mark, 5000))
        {
            if (++retry < 3) { st = WIFI_SEND_AUTOCONN; }
            else { st = WIFI_IDLE; t_idle = HAL_GetTick(); retry = 0; }
        }
        break;

    /* ==================== 第 4 步：CWJAP 连路由器 ==================== */
    case WIFI_SEND_JAP:
        ESP8266_Clear();
        /* SSID 和密码都要双引号括起来 */
        ESP8266_USART("AT+CWJAP=\"%s\",\"%s\"\r\n", ID, PASSWORD);
        t_mark = HAL_GetTick();
        st     = WIFI_WAIT_JAP;
        break;

    case WIFI_WAIT_JAP:
        /* ★ 这步给 30 秒。连 AP 要扫 13 个信道 + 四次握手，慢是正常的 */
        if (strstr((char *)USART3_RxBuff, "WIFI GOT IP") ||
            strstr((char *)USART3_RxBuff, "OK"))
        {
            retry = 0;
            st    = WIFI_SEND_MUX;
        }
        else if (strstr((char *)USART3_RxBuff, "FAIL") ||
                 strstr((char *)USART3_RxBuff, "ERROR"))    /* 密码错会立刻 FAIL */
        {
            if (++retry < 3) { st = WIFI_SEND_JAP; }
            else { st = WIFI_IDLE; t_idle = HAL_GetTick(); retry = 0; }
        }
        else if (Tick_Expired(&t_mark, 30000))
        {
            if (++retry < 3) { st = WIFI_SEND_JAP; }
            else { st = WIFI_IDLE; t_idle = HAL_GetTick(); retry = 0; }
        }
        break;

    /* ==================== 第 5 步：CIPMUX ==================== */
    case WIFI_SEND_MUX:
        ESP8266_Clear();
        ESP8266_USART("AT+CIPMUX=0\r\n");   /* 0 = 单连接。透传必须单连接 */
        t_mark = HAL_GetTick();
        st     = WIFI_WAIT_MUX;
        break;

    case WIFI_WAIT_MUX:
        if (strstr((char *)USART3_RxBuff, "OK"))
        {
            retry = 0;
            st    = WIFI_SEND_MODE;
        }
        else if (Tick_Expired(&t_mark, 5000))
        {
            if (++retry < 3) { st = WIFI_SEND_MUX; }
            else { st = WIFI_IDLE; t_idle = HAL_GetTick(); retry = 0; }
        }
        break;

    /* ==================== 第 6 步：CIPMODE ==================== */
    case WIFI_SEND_MODE:
        ESP8266_Clear();
        ESP8266_USART("AT+CIPMODE=1\r\n");  /* 1 = 开透传 */
        t_mark = HAL_GetTick();
        st     = WIFI_WAIT_MODE;
        break;

    case WIFI_WAIT_MODE:
        if (strstr((char *)USART3_RxBuff, "OK"))
        {
            retry = 0;
            st    = WIFI_SEND_START;
        }
        else if (Tick_Expired(&t_mark, 5000))
        {
            if (++retry < 3) { st = WIFI_SEND_MODE; }
            else { st = WIFI_IDLE; t_idle = HAL_GetTick(); retry = 0; }
        }
        break;

    /* ==================== 第 7 步：CIPSTART 建 TCP ==================== */
    case WIFI_SEND_START:
        ESP8266_Clear();
        ESP8266_USART("AT+CIPSTART=\"TCP\",\"%s\",%d\r\n", ServerIP, ServerPort);
        t_mark = HAL_GetTick();
        st     = WIFI_WAIT_START;
        break;

    case WIFI_WAIT_START:
        if (strstr((char *)USART3_RxBuff, "CONNECT") ||
            strstr((char *)USART3_RxBuff, "OK"))
        {
            retry = 0;
            st    = WIFI_SEND_SEND;
        }
        else if (strstr((char *)USART3_RxBuff, "ERROR") ||
                 strstr((char *)USART3_RxBuff, "CLOSED"))
        {
            if (++retry < 3) { st = WIFI_SEND_START; }
            else { st = WIFI_IDLE; t_idle = HAL_GetTick(); retry = 0; }
        }
        else if (Tick_Expired(&t_mark, 10000))          /* TCP 建链给 10 秒 */
        {
            if (++retry < 3) { st = WIFI_SEND_START; }
            else { st = WIFI_IDLE; t_idle = HAL_GetTick(); retry = 0; }
        }
        break;

    /* ==================== 第 8 步：CIPSEND 进透传 ==================== */
    case WIFI_SEND_SEND:
        ESP8266_Clear();
        ESP8266_USART("AT+CIPSEND\r\n");    /* ★ 不带数字才是进透传 */
        t_mark = HAL_GetTick();
        st     = WIFI_WAIT_SEND;
        break;

    case WIFI_WAIT_SEND:
        if (strchr((char *)USART3_RxBuff, '>'))         /* ★ 等 '>' 不是 "OK" */
        {
            connect_flag = 1;               /* 打标记，全工程靠它判断能不能发数据 */
            retry        = 0;
            st           = WIFI_READY;
            u1_USART("WiFi ready, transparent mode on\r\n");
        }
        else if (strstr((char *)USART3_RxBuff, "ERROR") ||
                 Tick_Expired(&t_mark, 5000))
        {
            if (++retry < 3) { st = WIFI_SEND_SEND; }
            else { st = WIFI_IDLE; t_idle = HAL_GetTick(); retry = 0; }
        }
        break;

    /* ==================== 已连上 ==================== */
    case WIFI_READY:
        /* 这里不主动发 AT。收发由 Wifi_TakeFrame() + Cloud_Task() 负责。
           但必须盯 connect_flag：Cloud 层收到 CLOSED 时会把 flag 清 0，
           如果这里不看它，状态机会永远卡在 READY，断线后再也不重连。 */
        if (!connect_flag)              /* ★ 链路被 Cloud 层判定断开 */
        {
            retry = 0;
            st    = WIFI_SEND_RST;      /* 重新走八步 */
            u1_USART("link reset, reconnecting\r\n");
        }
        break;
    }
}
/* ============================================================
 * Wifi_TakeFrame() —— 取一帧，取完立刻清
 *
 * 为什么必须"取完就清"：
 *   HAL_UARTEx_RxEventCallback 只在新帧到达时更新 USART3_RxCounter，
 *   从不归零。不清的话 strstr 会一遍遍命中上一条旧数据。
 * ============================================================ */
uint8_t Wifi_TakeFrame(char *out, uint16_t out_size)
{
    if (USART3_RxCounter == 0)          /* 没有新数据 */
        return 0;

    uint16_t n = USART3_RxCounter;
    if (n >= out_size)                  /* 帧比缓冲区长，截断 */
        n = out_size - 1;               /* 留一个字节放 '\0' */

    memcpy(out, USART3_RxBuff, n);      /* 拷到调用方的缓冲区 */
    out[n] = '\0';                      /* 手动补结束符 */

    USART3_RxCounter = 0;               /* ★★ 读走就清 */
    USART3_RxBuff[0] = '\0';

    return 1;
}