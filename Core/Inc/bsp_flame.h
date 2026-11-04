#ifndef __BSP_FLAME_H
#define __BSP_FLAME_H

#include <stdint.h>

/*
 * 光电/火焰输入 + 人体红外输入（EXTI 外部中断）
 *
 * 硬件接口（扩展板丝印 → MCU 引脚）：
 *   IO Fire → PB3  火焰传感器（红外接收管），检测到火焰 → 0，无火焰 → 1
 *   IO ITR  → PB2  ITR9608 槽型光电开关（备用，本轮未启用）
 *   IO PIR  → PB4  人体红外 (RDA226)，检测到人体活动 → 0
 *
 * 注意：PB3 出厂功能是 JTDO、PB4 是 NJTRST（都是 JTAG 引脚）。ST-Link 用 SWD
 *       连接时会禁掉 JTAG-DP，这两个脚就归 GPIO 管；若 PB3 配了没反应，
 *       先确认下载器是 SWD 模式。
 *
 * 两个事件源各用一套计数器，绝不混在一起：
 *   光电     = 真实遮挡/火源检测 → 参与 ALARM
 *   人体红外 = 有没有人         → 不参与报警，只做辅助判断
 */

#define FLAME_ACTIVE_LEVEL   0u     /* 0 = 低电平代表检测到火焰（LM393 型火焰模块典型极性） */
#define PIR_ACTIVE_LEVEL     0u     /* 0 = 检测到人体活动 */

void     Flame_Init(void);              /* 配 PB3 / PB4 双边沿 EXTI + NVIC */

uint8_t  Flame_GetLevel(void);          /* 光电当前电平 0/1 */
uint8_t  Flame_IsActive(void);          /* 1 = 光电触发 */
uint32_t Flame_GetCount(void);          /* 光电累计触发次数 */

uint8_t  Pir_GetLevel(void);            /* 人体红外当前电平 0/1 */
uint8_t  Pir_IsActive(void);            /* 1 = 检测到人 */
uint32_t Pir_GetCount(void);            /* 人体红外累计触发次数 */

#endif /* __BSP_FLAME_H */
