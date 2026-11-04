#include "main.h"
#include <stdio.h>
#include "app_state.h"
#include "bsp_flame.h"
#include "bsp_tick.h"
#include "bsp_tick.h"
#include "app_power.h"      /* ← 新增：Power_IsAbnormal() */

/* ---- 内部状态 ---- */
static SysState_t     s_state       = SYS_NORMAL;
static AlarmReason_t  s_reason      = ALARM_NONE;   /* ← 新增：报警原因 */
static uint8_t        s_alarm_latch = 0;            /* 明火锁存 */
static uint32_t       s_alarm_t     = 0;
static uint8_t        s_power_latch = 0;            /* ← 新增：用电异常锁存 */
static uint32_t       s_power_t     = 0;            /* ← 新增：用电异常计时 */
static float          s_temp_c      = 25.0f;
static float          s_humi_rh     = 50.0f;
static uint8_t        s_warn_active = 0;

/* ---- 阈值表（顺序必须和 ThId_t 一致）---- */
static const float s_th_default[TH_COUNT] = { 35.0f, 33.0f, 85.0f, 80.0f };
static float       s_th[TH_COUNT];


void App_State_Init(void)
{
    uint8_t i;

    s_state       = SYS_NORMAL;
    s_reason      = ALARM_NONE;         /* ← 新增 */
    s_alarm_latch = 0;
    s_alarm_t     = Tick_Ms();
    s_power_latch = 0;                  /* ← 新增 */
    s_power_t     = Tick_Ms();          /* ← 新增：同样不用 0 */
    s_temp_c      = 25.0f;
    s_humi_rh     = 50.0f;
    s_warn_active = 0;

    for (i = 0; i < TH_COUNT; i++) {
        s_th[i] = s_th_default[i];
    }
}

void App_State_SetEnv(float temp_c, float humi_rh)
{
    s_temp_c  = temp_c;
    s_humi_rh = humi_rh;
}


SysState_t App_State_Get(void)
{
    return s_state;
}

AlarmReason_t App_State_GetReason(void)    /* ← 整个函数新增 */
{
    return s_reason;
}


float App_State_GetThreshold(ThId_t id)
{
    if (id >= TH_COUNT) return 0.0f;      /* 越界保护 */
    return s_th[id];
}


void App_State_Task(void)
{
    /* ==================== 第一步 a：明火锁存 ==================== */
    if (Flame_IsActive() == 1u) {
        s_alarm_latch = 1;
        s_alarm_t = Tick_Ms();
    } else if (s_alarm_latch == 1u) {
        if (Tick_Expired(&s_alarm_t, ALARM_HOLD_MS)) {
            s_alarm_latch = 0;
        }
    }

    /* ==================== 第一步 b：用电异常锁存（新增）==================== */
    if (Power_IsAbnormal() == 1u) {
        s_power_latch = 1;              /* ← 越限 → 置锁存 */
        s_power_t = Tick_Ms();          /* ← 每圈刷新 → 只要还越限就一直锁 */
    } else if (s_power_latch == 1u) {
        if (Tick_Expired(&s_power_t, ALARM_HOLD_MS)) {
            s_power_latch = 0;          /* ← 恢复正常 5 秒后解锁 */
        }
    }

    /* ==================== 第二步：温湿度判 WARN（带迟滞）==================== */
    if (s_warn_active == 0u) {
        if (s_temp_c > s_th[TH_TEMP_HIGH] || s_humi_rh > s_th[TH_HUMI_HIGH]) {
            s_warn_active = 1u;
        }
    } else {
        if (s_temp_c < s_th[TH_TEMP_LOW] && s_humi_rh < s_th[TH_HUMI_LOW]) {
            s_warn_active = 0u;
        }
    }

    /* ==================== 第三步：按优先级定状态 + 记原因 ==================== */
    if (s_alarm_latch == 1u) {              /* 明火最优先 */
        s_state  = SYS_ALARM;
        s_reason = ALARM_FLAME;             /* ← 新增 */
    } else if (s_power_latch == 1u) {       /* 用电异常次之（新增） */
        s_state  = SYS_ALARM;               /* ← 复用同一个状态 */
        s_reason = ALARM_POWER;             /* ← 只是原因不同 */
    } else if (s_warn_active == 1u) {
        s_state  = SYS_WARN;
        s_reason = ALARM_NONE;              /* ← 新增：清原因 */
    } else {
        s_state  = SYS_NORMAL;
        s_reason = ALARM_NONE;              /* ← 新增：清原因 */
    }
}

















