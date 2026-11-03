#ifndef __BSP_TICK_H
#define __BSP_TICK_H

#include <stdint.h>

void     Tick_Init(void);
uint32_t Tick_Ms(void);
uint8_t  Tick_Expired(uint32_t *last, uint32_t period_ms);

#endif /* __BSP_TICK_H */
