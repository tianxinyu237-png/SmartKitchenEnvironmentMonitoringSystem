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

/*========报警原因（SYS_ALARM 时才知道是哪一种）=========*/
typedef enum{
    ALARM_NONE = 0,     /* 0 → 没报警 */
    ALARM_FLAME,        /* 1 → 明火：数码管显示 9999 */
    ALARM_POWER         /* 2 → 用电异常：数码管显示 8888 */
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

/* 读阈值（管理员模式显示用） */
float      App_State_GetThreshold(ThId_t id);

#endif /* __APP_STATE_H */
