#ifndef __DRV_SHT20_H
#define __DRV_SHT20_H

#include <stdint.h>

/* 初始化：状态机归位、数据清零。上电调一次。 */
void    Sht20_Init(void);

/* 状态机推进会：主循环里每圈调一次，内部自己判时间，不阻塞。 */
void    Sht20_Task(void);

/* 第一轮完整测量跑完之前返回 0，之后返回 1。 */
uint8_t Sht20_IsReady(void);

/* 最近一次测到的温度(℃) / 湿度(%RH)。未 ready 时返回 0。 */
float   Sht20_GetTemp(void);
float   Sht20_GetHumi(void);

/* I2C 出错累计次数。拔掉传感器跑一会儿，看它涨不涨就知道接线有没有问题。 */
uint32_t Sht20_GetErrCount(void);

#endif /* __DRV_SHT20_H */