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
static uint32_t       s_state_ms    = 0;   /* 本次状态从哪一刻开始（报警页显示发生时间） */

/* ---- 阈值表（顺序必须和 ThId_t 一致）---- */
static const float s_th_default[TH_COUNT] = { 33.0f, 31.0f, 60.0f, 55.0f };
static float       s_th[TH_COUNT];

/* ---- 四类报警的使能位，默认全开 ---- */
static uint8_t s_en[EN_COUNT] = {1,1,1,1};

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
    s_state_ms    = Tick_Ms();

    for (i = 0; i < TH_COUNT; i++) {
        s_th[i] = s_th_default[i];
    }
		for(i = 0;i<EN_COUNT;i++){
				s_en [i] = 1;
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


/* ---- 读最近一次温湿度（报警页显示当前值用） ---- */
float App_State_GetTemp(void) { return s_temp_c; }
float App_State_GetHumi(void) { return s_humi_rh; }

/* ---- 读本次状态起始时刻（HAL_GetTick 毫秒） ---- */
uint32_t App_State_GetStateMs(void) { return s_state_ms; }

float App_State_GetThreshold(ThId_t id)
{
    if (id >= TH_COUNT) return 0.0f;      /* 越界保护 */
    return s_th[id];
}

void App_State_SetThreshold(ThId_t id, float v)
{
		if(id >=TH_COUNT)return;		/* 越界直接忽略，别崩 */
		
		/* 温度范围 -40~125（SHT20 量程），湿度 0~100 —— 超出就当没设 */
		if(id == TH_TEMP_HIGH || id == TH_TEMP_LOW){
				if(v<-40.f)v = -40.0f;
				if(v>125.0f)v=125.0f;
		}else{
				if(v<0.0f) v=0.0f;
				if(v>100.0f)v=100.0f;
		}
		s_th[id] = v;
}
uint8_t App_State_GetEnable(EnId_t id)
{
    if (id >= EN_COUNT) return 0;
    return s_en[id];
}

void App_State_SetEnable(EnId_t id, uint8_t on)
{
    if (id >= EN_COUNT) return;
    s_en[id] = on ? 1u : 0u;
}

void App_State_Task(void)
{
    /* ==================== 第一步 a：明火锁存 ==================== */
    if (s_en[EN_FIRE]&&(Flame_IsActive() == 1u)) {
        s_alarm_latch = 1;
        s_alarm_t = Tick_Ms();
    } else if (s_alarm_latch == 1u) {
        if (Tick_Expired(&s_alarm_t, ALARM_HOLD_MS)) {
            s_alarm_latch = 0;
        }
    }

    /* ==================== 第一步 b：用电异常锁存（新增）==================== */
    if (s_en[EN_ELEC]&&(Power_IsAbnormal() == 1u) ){
        s_power_latch = 1;              /* ← 越限 → 置锁存 */
        s_power_t = Tick_Ms();          /* ← 每圈刷新 → 只要还越限就一直锁 */
    } else if (s_power_latch == 1u) {
        if (Tick_Expired(&s_power_t, ALARM_HOLD_MS)) {
            s_power_latch = 0;          /* ← 恢复正常 5 秒后解锁 */
        }
    }
		/* 先把「这一路是否越限」算出来，使能关了就当没越限 */
    uint8_t t_over=(s_en[EN_TEMP]&&(s_temp_c  > s_th[TH_TEMP_HIGH])) ? 1u : 0u;
    uint8_t h_over=(s_en[EN_HUMI]&&(s_humi_rh > s_th[TH_HUMI_HIGH])) ? 1u : 0u;
    
		
		/* ==================== 第二步：温湿度判 WARN（带迟滞）==================== */
    if (s_warn_active == 0u) {
				/* 任一路越限 → 进 WARN */
        if (t_over || h_over) {
            s_warn_active = 1u;
        }
    } else {
        /* 两路都退下去 → 才退 WARN（迟滞的退出条件） */
        uint8_t t_ok = (!s_en[EN_TEMP]) || (s_temp_c  < s_th[TH_TEMP_LOW]);
        uint8_t h_ok = (!s_en[EN_HUMI]) || (s_humi_rh < s_th[TH_HUMI_LOW]);
        if (t_ok && h_ok) {
            s_warn_active = 0u;
        }
    }

    /* ==================== 第三步：按优先级定状态 + 记原因 ==================== */
    SysState_t     new_state;
    AlarmReason_t  new_reason;

    if (s_alarm_latch == 1u) {              /* 明火最优先 */
        new_state  = SYS_ALARM;
        new_reason = ALARM_FLAME;
    } else if (s_power_latch == 1u) {       /* 用电异常次之 */
        new_state  = SYS_ALARM;             /* 复用同一个状态 */
        new_reason = ALARM_POWER;           /* 只是原因不同 */
    } else if (s_warn_active == 1u) {
        new_state  = SYS_WARN;
        /* 温湿度同时超时，温度优先显示（跟报警优先级一致） */
        if (t_over)      new_reason = ALARM_TEMP_HIGH;
        else if (h_over) new_reason = ALARM_HUMI_HIGH;
        else {
            /* 已进 WARN 但两路都退到高门槛以下、还没跌破低门槛的迟滞区间：
               保持上次的原因，不要在这个空窗期显示"无原因" */
            new_reason = s_reason;
            if (new_reason == ALARM_NONE) new_reason = ALARM_TEMP_HIGH;
        }
    } else {
        new_state  = SYS_NORMAL;
        new_reason = ALARM_NONE;
    }

    /* 状态变了才记时刻 —— 报警页的"发生时间"要的是本次开始的时刻 */
    if (new_state != s_state || new_reason != s_reason) {
        s_state_ms = Tick_Ms();
    }

    s_state  = new_state;
    s_reason = new_reason;
}

















