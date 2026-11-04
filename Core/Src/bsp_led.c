#include "main.h"
#include "bsp_led.h"
#include "bsp_tick.h"
#include "gpio.h"

/* 每个灯一份独立的闪烁状态 */
typedef struct {
    uint32_t last;      /* 上次翻转的时刻 */
    uint16_t half;      /* 半周期 ms；0 表示不闪 */
    uint8_t  on;        /* 当前亮灭 */
} LedBlink_t;

static LedBlink_t s_blink[3];

void Led_Init(void)
{
    LED1OFF(); LED2OFF(); LED3OFF();   /* LED1 初值是 SET，必须显式关 */
    s_blink[0].half = 0;
    s_blink[1].half = 0;
    s_blink[2].half = 0;
}

void Led_On(LedId_t id)
{
    switch (id) {
    case LED_ID_L1: LED1ON(); break;
    case LED_ID_L2: LED2ON(); break;
    case LED_ID_L3: LED3ON(); break;
    default: break;
    }
}

void Led_Off(LedId_t id)
{
    switch (id) {
    case LED_ID_L1: LED1OFF(); break;
    case LED_ID_L2: LED2OFF(); break;
    case LED_ID_L3: LED3OFF(); break;
    default: break;
    }
}

void Led_Toggle(LedId_t id)
{
    switch (id) {
    case LED_ID_L1: LED1TOGGLE(); break;
    case LED_ID_L2: LED2TOGGLE(); break;
    case LED_ID_L3: LED3TOGGLE(); break;
    default: break;
    }
}

void Led_AllOff(void)
{
    LED1OFF(); LED2OFF(); LED3OFF();
}

void Led_BlinkStart(LedId_t id, uint32_t half_period_ms)
{
    if (id > LED_ID_L3) return;
    s_blink[id].half = (uint16_t)half_period_ms;
    s_blink[id].last = Tick_Ms();
    s_blink[id].on   = 0;
	
}

void Led_BlinkStop(LedId_t id)
{
    if (id > LED_ID_L3) return;
    s_blink[id].half = 0;
    Led_Off(id);            /* 停下来就灭，别留个亮的挂在那 */
}

/* ---- 主循环每圈调用一次，只查时间不阻塞 ---- */
void Led_Task(void)
{
    uint32_t now = Tick_Ms();       /* 只读一次，三个灯用同一时刻 */
    uint8_t  i;

    for (i = 0; i < 3; i++)
    {
        if (s_blink[i].half == 0) continue;                 /* 不闪的跳过 */

        if ((uint32_t)(now - s_blink[i].last) >= s_blink[i].half)
        {
            s_blink[i].last = now;
            s_blink[i].on   = !s_blink[i].on;
            Led_Toggle((LedId_t)i);
        }
    }
}
