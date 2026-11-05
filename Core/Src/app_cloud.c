#include "main.h"
#include "app_cloud.h"
#include "bsp_wifi.h"
#include "bsp_tick.h"
#include "bsp_led.h"
#include "bsp_fan.h"
#include "app_state.h"
#include "app_power.h"
#include "drv_sht20.h"
#include "drv_ap3216c.h"
#include <string.h>
#include <stdio.h>

/* ============================================================
 * 手动覆盖
 *
 * 为什么需要：云端下发 CMD=FAN,VAL=3 让风扇全速转，
 * 但温度没超阈值，下一圈 Alarm_Apply() 会把它关掉 ——
 * 两个源头抢同一批输出。所以手动模式下 Alarm_Apply 不碰被覆盖的对象。
 *
 * ⚠️ 蜂鸣器不在覆盖范围：那是报警的唯一听觉通道，
 *    让云端能关掉它等于把报警掐死。安全功能必须本地说了算。
 * ============================================================ */
typedef struct {
    uint8_t  active;        /* 1 = 手动模式生效中 */
    uint32_t t_last_cmd;    /* 最后一条指令时刻(ms)，用来判 5 分钟超时 */
    uint8_t  ovr_led;       /* 1 = LED 被手动接管 */
    uint8_t  ovr_fan;       /* 1 = 风扇被手动接管（报警态会被强制清零） */
} ManualOverride_t;

static ManualOverride_t g_manual = {0};

#define MANUAL_TIMEOUT_MS   300000u     /* 5 分钟无指令自动退出 */
#define UPLOAD_PERIOD_MS    2000u       /* 每 2 秒上传一次 */

/* ============================================================
 * 手动模式退出（三个入口共用）
 * ============================================================ */
void Cloud_ManualExit(void)
{
    g_manual.active  = 0;
    g_manual.ovr_led = 0;
    g_manual.ovr_fan = 0;
}

uint8_t Cloud_IsManual(void) { return g_manual.active; }

/* 给 alarm.c 用：查某个输出是否被手动接管 */
uint8_t Cloud_IsFanOverridden(void) { return g_manual.ovr_fan; }

/* ============================================================
 * 上传任务（需求 13）
 * ============================================================ */
static void Cloud_Upload(void)
{
    static uint32_t t_up  = 0;
    static uint8_t  last_st = 0xFFu;    /* 0xFF 非法值 → 上电第一圈必发基线 */

    if (!connect_flag)                  /* 没连上不发 */
        return;

    uint8_t st = App_State_Get();

    /* 状态变了 或 距上次够 2 秒 —— 任一成立就发 */
    if (st != last_st || Tick_Expired(&t_up, UPLOAD_PERIOD_MS))
    {
        last_st = st;

        char buf[96];
        snprintf(buf, sizeof(buf),
                 "T=%.2f,H=%.2f,LUX=%.1f,V=%.2f,I=%.1f,ST=%u\n",
                 Sht20_GetTemp(), Sht20_GetHumi(),
                 Ap3216_GetLux(),
                 Power_GetVoltage(), Power_GetCurrent(),
                 st);

        WIFI_SendData(buf);
    }
}

/* ============================================================
 * 指令解析（需求 12）
 *
 * 协议：CMD=LED,VAL=n   CMD=FAN,VAL=n   CMD=AUTO
 * ============================================================ */
static void OnCloudCmd(const char *frame)
{
    /* --- 必须是 "CMD=" 开头 --- */
    if (strncmp(frame, "CMD=", 4) != 0)
        return;                         /* 不是指令，忽略 */

    /* --- AUTO：交还自动控制 --- */
    if (strncmp(frame, "CMD=AUTO", 8) == 0)
    {
        Cloud_ManualExit();
        WIFI_SendData("ACK=AUTO\n");
        return;
    }

    /* --- 找逗号，切出 VAL --- */
    const char *comma = strchr(frame, ',');
    if (comma == NULL)
        return;

    const char *p = comma + 1;
    if (strncmp(p, "VAL=", 4) != 0)
        return;

    /* --- 手写解析数字。不用 atoi：atoi("abc") 返回 0，会被当成合法的 0 --- */
    const char *d = p + 4;
    if (*d < '0' || *d > '9')
        return;

    int val = 0;
    while (*d >= '0' && *d <= '9')
    {
        val = val * 10 + (*d - '0');
        if (val > 99)                   /* 上限保护，防溢出 */
            return;
        d++;
    }

    /* ---- CMD=LED : 0=全灭 1=LED1 2=LED2 3=LED3 ---- */
    if (strncmp(frame, "CMD=LED,", 8) == 0)
    {
        if (val > 3)
        {
            WIFI_SendData("ACK=ERR,CODE=1\n");
            return;
        }

        Led_Off(LED_ID_L1);
        Led_Off(LED_ID_L2);
        Led_Off(LED_ID_L3);             /* 先全灭再点指定那个，保证不同时亮 */
        if (val == 1) Led_On(LED_ID_L1);
        if (val == 2) Led_On(LED_ID_L2);
        if (val == 3) Led_On(LED_ID_L3);

        g_manual.ovr_led    = 1;
        g_manual.active     = 1;
        g_manual.t_last_cmd = HAL_GetTick();

        WIFI_SendData("ACK=LED\n");
        return;
    }

    /* ---- CMD=FAN : 0=停 1=慢 2=中 3=全速 ---- */
    if (strncmp(frame, "CMD=FAN,", 8) == 0)
    {
        if (val > 3)
        {
            WIFI_SendData("ACK=ERR,CODE=1\n");
            return;
        }

        /* ★★ 报警态拒绝手动控风扇：明火时排风是安全功能 */
        if (App_State_Get() == SYS_ALARM)
        {
            WIFI_SendData("ACK=ERR,CODE=2\n");
            return;
        }

        static const uint16_t fan_tbl[4] = {
            FAN_SPEED_STOP, FAN_SPEED_LOW, FAN_SPEED_MID, FAN_SPEED_FULL
        };
        Fan_SetSpeed(fan_tbl[val]);

        g_manual.ovr_fan    = 1;
        g_manual.active     = 1;
        g_manual.t_last_cmd = HAL_GetTick();

        WIFI_SendData("ACK=FAN\n");
        return;
    }

    /* ---- CMD=BEEP：有意拒绝，安全设计 ---- */
    if (strncmp(frame, "CMD=BEEP", 8) == 0)
    {
        WIFI_SendData("ACK=ERR,CODE=3\n");   /* 3 = 该对象不允许远程控制 */
        return;
    }
}

/* ============================================================
 * 收数据：取帧 → 判断线 → 交给解析
 * ============================================================ */
static void Cloud_Recv(void)
{
    char frame[128];

    if (!Wifi_TakeFrame(frame, sizeof(frame)))
        return;                         /* 没数据是常态，直接走 */

    if (strstr(frame, "CLOSED"))        /* 对端关了 socket */
    {
        connect_flag = 0;               /* 清标记，下一圈 Wifi_Task 会重连 */
        return;
    }

    OnCloudCmd(frame);                  /* 业务数据 */
}

/* ============================================================
 * 主任务
 * ============================================================ */
void Cloud_Init(void)
{
    Cloud_ManualExit();                 /* 上电默认自动模式 */
}

void Cloud_Task(void)
{
    /* 1. 手动模式超时检查 */
    if (g_manual.active &&
        Tick_Expired(&g_manual.t_last_cmd, MANUAL_TIMEOUT_MS))
    {
        Cloud_ManualExit();
        u1_USART("manual timeout, back to auto\r\n");
    }

    /* 2. 收指令 */
    if (connect_flag)
        Cloud_Recv();

    /* 3. 上传 */
    Cloud_Upload();
}