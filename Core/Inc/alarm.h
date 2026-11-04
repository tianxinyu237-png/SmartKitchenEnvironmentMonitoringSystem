#ifndef __ALARM_H
#define __ALARM_H

#include <stdint.h>
#include "app_state.h"

void Alarm_Init(void);
void Alarm_Apply(SysState_t st);/* 状态变化时调 */
void Alarm_Task(void);					/* 每圈调 */

void Alarm_Silence(void);				/* 消音（可选：绑到 KEY3） */
uint8_t Alarm_IsSilenced(void);





#endif /* __ALARM_H */