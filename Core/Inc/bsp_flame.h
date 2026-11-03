#ifndef __BSP_FLAME_H
#define __BSP_FLAME_H

#include <stdint.h>

/*
 * 光电/火焰输入 + 人体红外输入（EXTI 外部中断）
 *
 * 硬件实测结论：
 *   PB2 = 光电/火焰 (ITR9608 槽型红外对射)，遮挡 → 0，放开 → 1
 *   PB4 = 人体红外 (RDA226)，检测到人体活动 → 0
 *
 * 注意：PB2 复位时是 BOOT1 引脚，只在复位瞬间被采样，运行期可以正常
 *       当 GPIO 输入用，不冲突。
 *
 * 两个事件源各用一套计数器，绝不混在一起：
 *   光电     = 真实遮挡/火源检测 → 参与 ALARM
 *   人体红外 = 有没有人         → 不参与报警，只做辅助判断
 */

#define FLAME_ACTIVE_LEVEL   0u     /* 0 = 低电平代表光电被遮挡 */
#define PIR_ACTIVE_LEVEL     0u     /* 0 = 检测到人体活动 */

void     Flame_Init(void);              /* 配 PB2 / PB4 双边沿 EXTI + NVIC */

uint8_t  Flame_GetLevel(void);          /* 光电当前电平 0/1 */
uint8_t  Flame_IsActive(void);          /* 1 = 光电触发 */
uint32_t Flame_GetCount(void);          /* 光电累计触发次数 */

uint8_t  Pir_GetLevel(void);            /* 人体红外当前电平 0/1 */
uint8_t  Pir_IsActive(void);            /* 1 = 检测到人 */
uint32_t Pir_GetCount(void);            /* 人体红外累计触发次数 */

#endif /* __BSP_FLAME_H */
