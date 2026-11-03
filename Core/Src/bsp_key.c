#include "main.h"
#include <stdint.h>
#include "bsp_key.h"
#include "bsp_tick.h"
#include "gpio.h"

#define KEY_DEBOUNCE_MS   20        /* 电平要稳定这么久才算数 */

typedef struct {
    GPIO_TypeDef *port;
    uint16_t      pin;
    uint8_t       active_level;     /* 按下时的电平，上拉+对地 = 0 */
    uint8_t       stable;           /* 已确认的稳定电平 */
    uint8_t       last_raw;         /* 上次采样值 */
    uint32_t      t_change;         /* 上次电平变化的时刻 */
    uint8_t       event;            /* 1 = 刚按下（等主循环取走） */
} Key_t;

static Key_t s_key[KEY_COUNT];
static uint32_t s_last_scan;
void Key_Init(void){
		GPIO_InitTypeDef g={0};
		uint8_t i;
		g.Mode = GPIO_MODE_INPUT;
		g.Pull = GPIO_PULLUP;
		g.Speed = GPIO_SPEED_FREQ_LOW;
    g.Pin = KEY1_Pin; HAL_GPIO_Init(KEY1_GPIO_Port, &g);   /* PC9 */
    g.Pin = KEY2_Pin; HAL_GPIO_Init(KEY2_GPIO_Port, &g);   /* PC8 */
    g.Pin = KEY3_Pin; HAL_GPIO_Init(KEY3_GPIO_Port, &g);   /* PC5 */
		
    s_key[0].port = KEY1_GPIO_Port;  s_key[0].pin = KEY1_Pin;
    s_key[1].port = KEY2_GPIO_Port;  s_key[1].pin = KEY2_Pin;
    s_key[2].port = KEY3_GPIO_Port;  s_key[2].pin = KEY3_Pin;

		for (i = 0; i < KEY_COUNT; i++) {
        s_key[i].active_level = 0;      /* 这个板子上按下 = 低电平 */
        s_key[i].stable   = 1;          /* 初值当"松着"：上拉，未按就是高 */
        s_key[i].last_raw = 1;          /* 和 stable 一致，避免第一次扫描误判成"刚变化" */
        s_key[i].t_change = Tick_Ms();  /* 从当前时刻开始计，抖动窗口从这里起算 */
        s_key[i].event    = 0;
    }
    s_last_scan = Tick_Ms();
}

void Key_Scan(void){
		uint32_t now;
		uint8_t i,raw;
		
		if (!Tick_Expired(&s_last_scan, 10)) return;
		now = Tick_Ms();
	
		for (i = 0; i < KEY_COUNT; i++)
		{
			raw = (HAL_GPIO_ReadPin(s_key[i].port, s_key[i].pin) == GPIO_PIN_SET) ? 1 : 0;
				if (raw != s_key[i].last_raw)
        {
            s_key[i].last_raw = raw;
            s_key[i].t_change = now;
        }else if ((uint32_t)(now - s_key[i].t_change) >= KEY_DEBOUNCE_MS)
        {
            if (raw != s_key[i].stable)     /* 确实和已确认的状态不一样 */
            {
                s_key[i].stable = raw;
								if (s_key[i].stable == s_key[i].active_level)
										s_key[i].event = 1;
            }
        }
    }
}



uint8_t Key_GetPressEvent(KeyId_t id){
		if(id>=KEY_COUNT)return 0;

		if(s_key[id].event){
			 s_key[id].event = 0;
			 return 1;
		}
		return 0;
}


uint8_t Key_IsHeld(KeyId_t id){
		if(id>=KEY_COUNT) return 0;
	  return (s_key[id].stable == s_key[id].active_level)?1:0;
}








