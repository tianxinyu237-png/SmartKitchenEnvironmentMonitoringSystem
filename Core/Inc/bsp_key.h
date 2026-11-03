#ifndef __BSP_KEY_H
#define __BSP_KEY_H
#include <stdint.h>

typedef enum{
		KEY_ID_1 = 0,
		KEY_ID_2,
		KEY_ID_3,
		KEY_COUNT
}KeyId_t;

void Key_Init(void);

void Key_Scan(void);  /* 主循环每圈调，内部自己限速 10ms */
uint8_t Key_GetPressEvent(KeyId_t id);
uint8_t Key_IsHeld(KeyId_t id);


#endif