#include "bsp_callback.h"
#include "gpio.h"
#include "bsp_flame.h"
#include "bsp_ft6336.h"
#include "stm32f4xx_hal.h"

// 外部中断统一回调
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    if(GPIO_Pin == GPIO_PIN_3)          /* PE3 = 触摸 INT FT6336 */
    {
        FT6336_irq_fuc();               /* 触摸中断处理 */
    }
    else if (GPIO_Pin == FLAME_PIN)     /* 火焰传感器中断引脚 */
    {
        s_flame_cnt++;
        s_flame_lvl = (uint8_t)HAL_GPIO_ReadPin(FLAME_GPIO_PORT, FLAME_PIN);
    }
    else if (GPIO_Pin == PIR_PIN)       /* 人体红外PIR中断引脚 */
    {
        s_pir_cnt++;
        s_pir_lvl = (uint8_t)HAL_GPIO_ReadPin(FLAME_GPIO_PORT, PIR_PIN);
    }
}

// ========== 在这里继续放定时器回调，解决下一个报错：HAL_TIM_PeriodElapsedCallback ==========
// 把 main.c 和 bsp_smg.c 里面的 HAL_TIM_PeriodElapsedCallback 也迁移到这里
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if(htim->Instance == TIMx)   // 替换成你数码管使用的定时器
    {
        // 原来 bsp_smg.c 的定时器溢出逻辑
    }
    else if(htim->Instance == TIMy) // 其他定时器
    {
        // 原来 main.c 里面定时器逻辑
    }
}
