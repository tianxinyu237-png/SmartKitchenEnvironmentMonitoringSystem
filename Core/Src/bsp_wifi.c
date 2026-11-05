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
 *
 * 两条通道，别混（这是本文件的地基）：
 *   AT 阶段：SEND_xxx 发指令（阻塞几毫秒）-> WAIT_xxx 每圈只读一次缓冲区（不阻塞）
 *            回应用 At_FindLine() 按行比对，不用 strstr 子串搜索
 *   透传阶段：只写不发 AT；下行数据由 ISR 切行入队，Cloud 层用 Wifi_TakeFrame() 取
 * ==========================================================================*/

#include "bsp_wifi.h"
#include "bsp_tick.h"      /* Tick_Expired —— 非阻塞状态机的时间判断靠它 */

int connect_flag = 0;              /* 1 = TCP 已建立            */


/* ============================================================
 * 帧匹配：按行比对，不做子串搜索
 *
 * 为什么不能 strstr：
 *   WAIT_START 等 CONNECT，而模组的 WIFI DISCONNECT 里就含 CONNECT
 *   —— 断网时会被误判成 TCP 建链成功（实测复现过）。
 * 加强两点：
 *   1) 发指令前清缓冲，并把期望回显等出来，保证看到的回应属于这一条指令
 *   2) 回应按 LF 切行，行首剥掉 CRLF 再用 strncmp 整行比对
 * ============================================================ */
static uint8_t At_FindLine(const char *hay, const char *expect)
{
    const char *p    = hay;
    size_t      elen = strlen(expect);

    if (hay == NULL || *hay == '\0')
        return 0;

    while (*p != '\0')
    {
        const char *ls = p;                    /* 行首 */
        const char *nl = strchr(p, '\n');      /* 换行位置 */
        const char *content;

        while (*ls == '\r' || *ls == '\n')     /* 剥行首 CRLF */
            ls++;

        if (nl != NULL)
        {
            content = nl;                      /* 行内容末尾 */
            while (content > ls && (content[-1] == '\r' || content[-1] == '\n'))
                content--;

            if ((size_t)(content - ls) == elen && strncmp(ls, expect, elen) == 0)
                return 1;

            p = nl + 1;
        }
        else
        {
            /* 最后一段没有换行：模组可能只发了半行。要求完全相等，不做前缀匹配 */
            if (strcmp(ls, expect) == 0)
                return 1;
            break;
        }
    }
    return 0;
}

/* 清 AT 阶段缓冲区。透传阶段不调 —— 那时数据走环形队列 */
static void At_Clear(void)
{
    if (g_wifi_passthru)
        return;
    ESP8266_Clear();
}

/* ============================================================
 * Wifi_AtCmd() —— 发一条 AT 指令并等回应
 *
 * 阻塞上限 = timeout_ms，只在 SEND_xxx 里用；
 * 最长的那一步（CWJAP 30 秒）刻意不在这个函数里等，见 WIFI_WAIT_JAP。
 * ============================================================ */
uint8_t Wifi_AtCmd(const char *cmd, const char *expect, uint32_t timeout_ms)
{
    uint32_t t0        = HAL_GetTick();
    uint8_t  echo_seen = 0;

    At_Clear();                            /* 断代：清掉上一条的残留 */

    ESP8266_USART("%s", cmd);

    /* 回显断言：一条正常 AT 指令回显不超过 40 字节，115200 下 3.5ms 就到。
       等 200ms 还看不到回显，说明模组没接 / 供电不足 / 串口线错 / 还在启动。
       这时候再等回应，等到的必然是上一轮的旧数据 —— 直接判失败更省时间。 */
    while ((uint32_t)(HAL_GetTick() - t0) < timeout_ms)
    {
        /* 匹配期望回应 */
        if (At_FindLine((const char *)USART3_RxBuff, expect))
            return 0;

        /* 明确失败，别等到超时 */
        if (At_FindLine((const char *)USART3_RxBuff, "ERROR") ||
            At_FindLine((const char *)USART3_RxBuff, "FAIL"))
            return 1;

        /* 回显断言 */
        if (!echo_seen)
        {
            if (strstr((const char *)USART3_RxBuff, cmd) != NULL)
            {
                echo_seen = 1;
            }
            else if ((uint32_t)(HAL_GetTick() - t0) > 200u)
            {
                return 1;                  /* 模组没回显 = 没通 */
            }
        }
    }
    return 1;
}


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
 * SEND_xxx 里发指令是阻塞的（几毫秒）；WAIT_xxx 只读一次缓冲区，不阻塞。
 * 最坏的一步（CWJAP 等 30 秒）拆成 SEND + WAIT 两半，
 * 所以主循环、看门狗、数码管全程照转。
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
        /* 这里必须释放透传标记：上一轮可能停在半路，
           不释放的话 AT 回应全进业务队列，状态机永远读不到 */
        g_wifi_passthru = 0;
        connect_flag    = 0;

        if (Tick_Expired(&t_idle, 1000))    /* 等够 1 秒 */
        {
            /* 环形队列溢出 = 下行比主循环快，或主循环被谁拖住了。
               不报出来就永远发现不了丢指令 —— 这是最难查的一类问题。 */
            if (s_overflow != 0u)
            {
                u1_USART("WARN: rx queue overflow, dropped %lu lines\r\n",
                         (unsigned long)s_overflow);
                s_overflow = 0u;
            }
            retry = 0;
            st    = WIFI_SEND_RST;          /* 从头开始 */
        }
        break;

    /* ==================== 第 1 步：AT+RST ==================== */
    case WIFI_SEND_RST:
        /* AT+RST 之后模组要先吐启动信息，ready 通常几十毫秒后才来。
           这一步不用 Wifi_AtCmd：重启中的模组不一定来得及回显。 */
        At_Clear();
        ESP8266_USART("AT+RST\r\n");
        t_mark = HAL_GetTick();
        st     = WIFI_WAIT_RST;
        break;

    case WIFI_WAIT_RST:
        if (At_FindLine((const char *)USART3_RxBuff, "ready"))
        {
            u1_USART("1. reset OK\r\n");
            retry = 0;
            st    = WIFI_SEND_CWMODE;
        }
        else if (Tick_Expired(&t_mark, 5000))           /* 5 秒超时 */
        {
            u1_USART("1. reset FAILED\r\n");
            if (++retry < 3) { st = WIFI_SEND_RST; }    /* 重试同一步 */
            else { st = WIFI_IDLE; t_idle = HAL_GetTick(); retry = 0; }
        }
        break;

    /* ==================== 第 2 步：CWMODE ==================== */
    case WIFI_SEND_CWMODE:
        if (Wifi_AtCmd("AT+CWMODE=1\r\n", "OK", 3000) == 0)   /* 1 = STA，当客户端 */
        {
            u1_USART("2. CWMODE OK\r\n");
            retry = 0;
            st    = WIFI_SEND_AUTOCONN;
        }
        else
        {
            u1_USART("2. CWMODE FAILED\r\n");
            if (++retry < 3) { st = WIFI_SEND_CWMODE; }
            else { st = WIFI_IDLE; t_idle = HAL_GetTick(); retry = 0; }
        }
        break;

    /* ==================== 第 3 步：CWAUTOCONN ==================== */
    case WIFI_SEND_AUTOCONN:
        if (Wifi_AtCmd("AT+CWAUTOCONN=0\r\n", "OK", 3000) == 0)   /* 关自动连接 */
        {
            u1_USART("3. CWAUTOCONN OK\r\n");
            retry = 0;
            st    = WIFI_SEND_JAP;
        }
        else
        {
            u1_USART("3. CWAUTOCONN FAILED\r\n");
            if (++retry < 3) { st = WIFI_SEND_AUTOCONN; }
            else { st = WIFI_IDLE; t_idle = HAL_GetTick(); retry = 0; }
        }
        break;

    /* ==================== 第 4 步：CWJAP 连路由器 ==================== */
    case WIFI_SEND_JAP:
        At_Clear();
        /* SSID 和密码都要双引号括起来 */
        ESP8266_USART("AT+CWJAP=\"%s\",\"%s\"\r\n", ID, PASSWORD);
        t_mark = HAL_GetTick();
        st     = WIFI_WAIT_JAP;             /* ★ 发完立刻返回，这里绝不能等 */
        break;

    case WIFI_WAIT_JAP:
        /* 连 AP 要扫 13 个信道 + 四次握手，慢是正常的，给 30 秒。
           但不阻塞：每圈只读一次缓冲区，主循环照转，看门狗照喂。 */
        if (At_FindLine((const char *)USART3_RxBuff, "OK") ||
            At_FindLine((const char *)USART3_RxBuff, "WIFI GOT IP"))
        {
            u1_USART("4. join AP OK\r\n");
            retry = 0;
            st    = WIFI_SEND_MUX;
        }
        else if (At_FindLine((const char *)USART3_RxBuff, "FAIL") ||
                 At_FindLine((const char *)USART3_RxBuff, "ERROR"))   /* 密码错立刻 FAIL */
        {
            u1_USART("4. join AP FAILED (ssid or pwd)\r\n");
            if (++retry < 3) { st = WIFI_SEND_JAP; }
            else { st = WIFI_IDLE; t_idle = HAL_GetTick(); retry = 0; }
        }
        else if (Tick_Expired(&t_mark, 30000))
        {
            u1_USART("4. join AP TIMEOUT\r\n");
            if (++retry < 3) { st = WIFI_SEND_JAP; }
            else { st = WIFI_IDLE; t_idle = HAL_GetTick(); retry = 0; }
        }
        break;

    /* ==================== 第 5 步：CIPMUX ==================== */
    case WIFI_SEND_MUX:
        if (Wifi_AtCmd("AT+CIPMUX=0\r\n", "OK", 3000) == 0)   /* 0 = 单连接，透传必须 */
        {
            u1_USART("5. CIPMUX OK\r\n");
            retry = 0;
            st    = WIFI_SEND_MODE;
        }
        else
        {
            u1_USART("5. CIPMUX FAILED\r\n");
            if (++retry < 3) { st = WIFI_SEND_MUX; }
            else { st = WIFI_IDLE; t_idle = HAL_GetTick(); retry = 0; }
        }
        break;

    /* ==================== 第 6 步：CIPMODE ==================== */
    case WIFI_SEND_MODE:
        if (Wifi_AtCmd("AT+CIPMODE=1\r\n", "OK", 3000) == 0)  /* 1 = 开透传 */
        {
            u1_USART("6. CIPMODE OK\r\n");
            retry = 0;
            st    = WIFI_SEND_START;
        }
        else
        {
            u1_USART("6. CIPMODE FAILED\r\n");
            if (++retry < 3) { st = WIFI_SEND_MODE; }
            else { st = WIFI_IDLE; t_idle = HAL_GetTick(); retry = 0; }
        }
        break;

    /* ==================== 第 7 步：CIPSTART 建 TCP ==================== */
    case WIFI_SEND_START:
        /* 不用 Wifi_AtCmd：建链成功模组回 CONNECT + OK 两行，
           失败时常常只给 ERROR 或干脆不回，用整行比对更稳 */
        At_Clear();
        ESP8266_USART("AT+CIPSTART=\"TCP\",\"%s\",%d\r\n", ServerIP, ServerPort);
        t_mark = HAL_GetTick();
        st     = WIFI_WAIT_START;
        break;

    case WIFI_WAIT_START:
        /* ★ 整行相等比对 CONNECT，绕开 WIFI DISCONNECT 的子串误命中 */
        if (At_FindLine((const char *)USART3_RxBuff, "CONNECT") ||
            At_FindLine((const char *)USART3_RxBuff, "OK"))
        {
            u1_USART("7. TCP OK\r\n");
            retry = 0;
            st    = WIFI_SEND_SEND;
        }
        else if (At_FindLine((const char *)USART3_RxBuff, "ERROR") ||
                 At_FindLine((const char *)USART3_RxBuff, "CLOSED"))
        {
            u1_USART("7. TCP FAILED\r\n");
            if (++retry < 3) { st = WIFI_SEND_START; }
            else { st = WIFI_IDLE; t_idle = HAL_GetTick(); retry = 0; }
        }
        else if (Tick_Expired(&t_mark, 10000))          /* TCP 建链给 10 秒 */
        {
            u1_USART("7. TCP TIMEOUT\r\n");
            if (++retry < 3) { st = WIFI_SEND_START; }
            else { st = WIFI_IDLE; t_idle = HAL_GetTick(); retry = 0; }
        }
        break;

    /* ==================== 第 8 步：CIPSEND 进透传 ==================== */
    case WIFI_SEND_SEND:
        At_Clear();
        ESP8266_USART("AT+CIPSEND\r\n");    /* ★ 不带数字才是进透传 */
        t_mark = HAL_GetTick();
        st     = WIFI_WAIT_SEND;
        break;

    case WIFI_WAIT_SEND:
        /* ★ 等的是 '>'，不是 OK。
           而且这一帧里 '>' 之后紧跟的就是业务数据，所以按顺序做四件事：
             1) 清掉握手期的残行
             2) 把这一帧里 '>' 之后那部分先灌进业务队列，
                否则服务器在进透传的同一毫秒推来的第一条指令会丢
             3) 切透传标记，让后续帧进业务队列
             4) 置 connect_flag，开始允许发数据 */
        if (strchr((const char *)USART3_RxBuff, '>') != NULL)
        {
            const char *gt = strchr((const char *)USART3_RxBuff, '>');

            Wifi_FlushRx();                 /* 1) 清握手期残行 */

            if (gt[1] != '\0' && gt[1] != '\r' && gt[1] != '\n')
            {
                Wifi_RxChunk((const uint8_t *)(gt + 1),
                             (uint16_t)strlen(gt + 1));       /* 2) */
            }

            g_wifi_passthru = 1;            /* 3) */
            connect_flag    = 1;            /* 4) 全工程靠它判断能不能发数据 */
            retry           = 0;
            st              = WIFI_READY;
            u1_USART("8. WiFi ready, transparent mode on\r\n");
        }
        else if (At_FindLine((const char *)USART3_RxBuff, "ERROR") ||
                 Tick_Expired(&t_mark, 5000))
        {
            u1_USART("8. enter transparent FAILED\r\n");
            if (++retry < 3) { st = WIFI_SEND_SEND; }
            else { st = WIFI_IDLE; t_idle = HAL_GetTick(); retry = 0; }
        }
        break;

    /* ==================== 已连上 ==================== */
    case WIFI_READY:
        /* 这里不主动发 AT。收发由 Wifi_TakeFrame() + Cloud_Task() 负责。
           但必须盯 connect_flag：Cloud 层判定断线时会把 flag 清 0，
           如果这里不看它，状态机会永远卡在 READY，断线后再也不重连。 */
        if (!connect_flag)              /* ★ 链路被 Cloud 层判定断开 */
        {
            retry           = 0;
            g_wifi_passthru = 0;        /* 退回 AT 阶段，否则 AT 回应进错通道 */
            Wifi_FlushRx();
            st              = WIFI_SEND_RST;      /* 重新走八步 */
            u1_USART("link reset, reconnecting\r\n");
        }
        break;
    }
}

/* ============================================================
 * Wifi_TakeFrame() —— 从业务行队列取一行
 *
 * 为什么是环形队列不是一整块缓冲：
 *   透传下行是字节流，一次可能来两三条指令。用一块缓冲的话
 *   比对只能命中最前面那条，后面的要么被覆盖、要么永远读不到。
 *   按行切 + 环队，一条指令取一次，不丢不乱序。
 * ============================================================ */
uint8_t Wifi_TakeFrame(char *out, uint16_t out_size)
{
    uint8_t tail = s_tail;

    if (tail == s_head)                 /* 队列空 */
        return 0;

    /* 拷出来：ISR 只写 s_head 那一格，不会碰 tail 这格 */
    {
        uint16_t n = 0;
        while (s_line[tail][n] != '\0' && (uint16_t)(n + 1) < out_size)
        {
            out[n] = s_line[tail][n];
            n++;
        }
        out[n] = '\0';
    }

    s_tail = (uint8_t)((tail + 1) % WIFI_LINE_SLOTS);
    return 1;
}

/* 清空业务行队列（握手前后、判定断线后调） */
void Wifi_FlushRx(void)
{
    s_tail = s_head;
    s_len  = 0;
}
