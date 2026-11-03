#include "main.h"
#include "bsp_tick.h"

void Tick_Init(void)
{
    /* SysTick 已由 HAL_Init() 起好 1ms 中断，这里暂时不用做事 */
}

uint32_t Tick_Ms(void)
{
    return HAL_GetTick();
}

uint8_t Tick_Expired(uint32_t *last, uint32_t period_ms)
{
    uint32_t now;

    if (period_ms == 0) return 0;                 /* 0 = 关闭该任务 */

    now = HAL_GetTick();                          /* 只读一次，比较用同一瞬间 */
    if ((uint32_t)(now - *last) >= period_ms)
    {
        *last = now;                              /* 只在触发时更新 */
        return 1;
    }
    return 0;
}
