#ifndef __APP_POWER_H
#define __APP_POWER_H
#include <stdint.h>
#define ADC_VREF        3.3f
#define ADC_FULL        4095.0f
#define VOLT_DIV_RATIO  3.14f   /* 实测标定：供电 5.02V ↔ PB1 1.600V ⇒ 3.1375（原 3.68 是按读数凑的，虚高 18%） */
#define CURR_MA_PER_V   100.0f  /* 本板实测算得：R54=0.1Ω × 增益100× ⇒ 10V/A ⇒ 1V↔100mA */
#define VOLT_MIN        3.3f   /* 电压下限（V）：低于这个算欠压 */
#define VOLT_MAX        5.0f   /* 电压上限（V）：高于这个算过压 */
#define CURR_MAX_MA     350.0f  /* 电流上限（mA）：硬件满量程仅 330mA，设 500 等于没保护 */



void    Power_Init(void);

void    Power_Task(void);

float   Power_GetVoltage(void);
float   Power_GetCurrent(void);
uint8_t Power_IsAbnormal(void);

#endif /* __APP_POWER_H */