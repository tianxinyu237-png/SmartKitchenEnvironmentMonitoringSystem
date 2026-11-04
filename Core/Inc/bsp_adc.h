#ifndef __BSP_ADC_H
#define __BSP_ADC_H

#include <stdint.h>

/* ==================== 通道下标 ==================== */
/* 下标 = DMA 数组里的位置，不是引脚号，也不是 ADC 通道号
 * 顺序由 adc.c 里的 Rank 决定：
 *   Rank 1 = ADC_CHANNEL_8 = PB0 = ADC AMP = 电流 → 下标 0
 *   Rank 2 = ADC_CHANNEL_9 = PB1 = ADC VDC = 电压 → 下标 1
 * ⚠️ 想改顺序必须去 CubeMX 改 Rank，改这里没用 */
#define ADC_CH_CURR     0u      /* 电流，对应 PB0，DMA 数组下标 0 */
#define ADC_CH_VOLT     1u      /* 电压，对应 PB1，DMA 数组下标 1 */
#define ADC_CH_COUNT    2u      /* 一共几路，必须和 adc.c 的 NbrOfConversion 一致 */

/* ==================== 参数 ==================== */
#define ADC_FILTER_N    8u
/*  ↑ 滑动平均窗口长度
 *    取值权衡：
 *      越大 → 读数越稳，但响应越慢（8 次 ≈ 2.4ms，16 次 ≈ 4.8ms）
 *      越小 → 响应越快，但读数跳动明显
 *    8 是本工程的选择：够稳、够快，而且是 2 的幂（编译器可优化成移位） */

/* ==================== 接口 ==================== */
void     Adc_Init(void);
/*  ↑ 上电调一次。启动 ADC + DMA，之后的搬运全自动
 *    必须放在 MX_ADC1_Init() 之后 */

uint16_t Adc_GetRaw(uint8_t ch);
/*  ↑ 取一次原始值（不滤波），返回 0~4095
 *    用途：标定时要看原始值、调试时确认 DMA 在跑 */

uint16_t Adc_GetAvg(uint8_t ch);
/*  ↑ 取滑动平均后的值，返回 0~4095
 *    ⚠️ 它有副作用：每调一次就往环形缓冲塞一个数
 *    所以「调得越勤，平均窗口越短」，不要在一个循环里反复调 */

uint8_t  Adc_IsRunning(void);
/*  ↑ 查 DMA 是否启动成功，1 = 正常
 *    用途：上电自检时打印一句，确认 FAILED 就能早发现问题 */

#endif /* __BSP_ADC_H */