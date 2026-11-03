#ifndef __BSP_SMG_H
#define __BSP_SMG_H

#include <stdint.h>

void Smg_Init(void);                            /* 启动 TIM2 中断，只调一次 */
void Smg_ShowNum(uint32_t num);                 /* 显示 0~9999 */
void Smg_ShowDp(uint32_t num, uint8_t dp_pos);  /* 带小数点，dp_pos=1 → 25.64 */
void Smg_Clear(void);                           /* 全灭 */
void Smg_Tick(void);                            /* 内部用：TIM2 回调里调 */

#endif /* __BSP_SMG_H */