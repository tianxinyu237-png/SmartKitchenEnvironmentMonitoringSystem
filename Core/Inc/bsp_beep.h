#ifndef __BSP_BEEP_H
#define __BSP_BEEP_H

#include <stdint.h>

/*
 * 蜂鸣器驱动（PA15，高电平响，有源蜂鸣器）
 *
 * 设计原则：所有接口都不阻塞。想响几声就调 Beep_Beep() 立刻返回，
 * 真正的通断时机由主循环每圈的 Beep_Task() 推进。
 *
 * 为什么不能在里面写 HAL_Delay：
 *   主循环还要跑数码管刷新、按键消抖、串口心跳，
 *   一旦 HAL_Delay(500) 卡住，这几样全跟着停半秒。
 */

void    Beep_Init(void);

void    Beep_On(void);                                  /* 持续响，直到 Beep_Off() */
void    Beep_Off(void);                                 /* 立即停 */
void    Beep_Stop(void);                                /* 停 + 清空剩下的响数 */

void    Beep_Once(uint32_t on_ms);                      /* 响一声 on_ms 毫秒后自动停 */
void    Beep_Beep(uint8_t times, uint32_t on_ms, uint32_t off_ms);
                                                        /* 响 times 声，每声 on_ms、间隔 off_ms */

void    Beep_Task(void);                                /* 主循环每圈必须调用，绝不阻塞 */
uint8_t Beep_IsBusy(void);                              /* 1 = 还在响或在间歇等待中 */

#endif /* __BSP_BEEP_H */
