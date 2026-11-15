#ifndef __APP_STATE_H
#define __APP_STATE_H

#include <stdint.h>

/*========状态枚举=========*/
typedef enum{
    SYS_NORMAL = 0,
    SYS_WARN,
    SYS_ALARM
}SysState_t;

/*========阈值项编号=========*/
typedef enum{
    TH_TEMP_HIGH = 0,   /* 温度进入 WARN 的门槛 (℃) */
    TH_TEMP_LOW,        /* 温度退出 WARN 的门槛 (℃) */
    TH_HUMI_HIGH,       /* 湿度进入 WARN 的门槛 (%RH) */
    TH_HUMI_LOW,        /* 湿度退出 WARN 的门槛 (%RH) */
    TH_COUNT            /* 共 4 项，别删 —— 数组长度靠它 */
}ThId_t;

/*========报警使能项编号=========*/
typedef enum{
		EN_TEMP = 0,				/* 温度报警 */
		EN_HUMI,						/* 湿度报警 */
		EN_FIRE,						/* 明火报警 */
		EN_ELEC,				 		/* 用电报警 */
		EN_COUNT						/* 共 4 项，别删 —— 数组长度靠它 */
}EnId_t;

/*========报警原因（知道是哪一种，报警页要显示）=========*/
typedef enum{
    ALARM_NONE = 0,     /* 0 → 没报警 */
    ALARM_FLAME,        /* 1 → 明火：数码管显示 9999 */
    ALARM_POWER,        /* 2 → 用电异常：数码管显示 8888 */
    ALARM_TEMP_HIGH,    /* 3 → 温度过高（SYS_WARN）：报警页显示温度值 */
    ALARM_HUMI_HIGH     /* 4 → 湿度过高（SYS_WARN）：报警页显示湿度值 */
}AlarmReason_t;



/* 报警保持时间（毫秒）—— 让灯闪、蜂鸣器响够，演示时看得见 */
#define ALARM_HOLD_MS    5000u

/*=============API===================*/
void       App_State_Init(void);
void       App_State_Task(void);
SysState_t App_State_Get(void);

/* 查报警原因：SYS_ALARM 时用来说明是明火还是用电 */
AlarmReason_t App_State_GetReason(void);

/* 把最新的温湿度喂进来 */
void       App_State_SetEnv(float temp_c, float humi_rh);

/* 读最近一次温湿度（报警页显示当前值用） */
float      App_State_GetTemp(void);
float      App_State_GetHumi(void);

/* 读本次状态从哪一刻开始（HAL_GetTick 毫秒），报警页显示发生时间用 */
uint32_t   App_State_GetStateMs(void);

/* 读阈值（管理员模式显示用） */
float      App_State_GetThreshold(ThId_t id);

/* 写阈值（设置页滑块用）—— id 越界会自动忽略 */
void       App_State_SetThreshold(ThId_t id, float v);

/* 读/写报警使能（设置页复选框用）—— 0=关 1=开 */
uint8_t    App_State_GetEnable(EnId_t id);
void       App_State_SetEnable(EnId_t id, uint8_t on);


#endif /* __APP_STATE_H */
