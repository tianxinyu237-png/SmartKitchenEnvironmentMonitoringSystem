#ifndef __BSP_SMG_H
#define __BSP_SMG_H

#include <stdint.h>

void Smg_Init(void);                            /* 启动 TIM2 中断，只调一次 */
void Smg_ShowNum(uint32_t num);                 /* 显示 0~9999 */
void Smg_ShowDp(uint32_t num, uint8_t dp_pos);  /* 带小数点，dp_pos=1 → 25.64 */
void Smg_ShowDigitsDp(uint8_t d0, uint8_t d1, uint8_t d2, uint8_t d3, uint8_t dp_pos);
/*  ↑ 逐位显示 + 指定小数点位置。0~9 = 显示该数字，其它值（传 0xFF）= 该位熄灭
 *    dp_pos：1 → 第 1 位后带小数点，4 → 不点
 *    用途：显示「5.00」这种前导位要熄灭的数（Smg_ShowDp 会显示成 "05.00"） */
void Smg_ShowDigits(uint8_t d0, uint8_t d1, uint8_t d2, uint8_t d3);
/*  ↑ 逐位显示，0~9 = 显示该数字，其它值（传 0xFF）= 这一位熄灭
 *    用途：ADMIN 模式显示「项号 ▯ 值」，中间那位熄掉当分隔符
 *    例：温度上限 35 → Smg_ShowDigits(1, 0xFF, 3, 5) → 屏幕上是 1▯35
 *    ⚠️ 为什么不复用 Smg_ShowNum：它会把 1035 显示成 "1035"，
 *       和测量值 1035 长得一模一样，分不出是项号还是读数 */
void Smg_Clear(void);                           /* 全灭 */
void Smg_Tick(void);                            /* 内部用：TIM2 回调里调 */

#endif /* __BSP_SMG_H */