#ifndef __BSP_LED_H
#define __BSP_LED_H

#include <stdint.h>

typedef enum {
    LED_ID_L1 = 0,
    LED_ID_L2,
    LED_ID_L3
} LedId_t;

void Led_Init(void);
void Led_On(LedId_t id);
void Led_Off(LedId_t id);
void Led_Toggle(LedId_t id);
void Led_AllOff(void);

void Led_BlinkStart(LedId_t id, uint32_t half_period_ms);
void Led_BlinkStop(LedId_t id);
void Led_Task(void);        /* 主循环每圈必须调用，绝不阻塞 */

#endif /* __BSP_LED_H */
