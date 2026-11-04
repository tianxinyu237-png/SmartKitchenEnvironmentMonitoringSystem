#ifndef __BSP_FAN_H              /* 头文件守卫①：第一次包含时 __BSP_FAN_H 未定义 → 进得来 */
#define __BSP_FAN_H              /* 守卫②：立刻定义，第二次包含就进不来了 */

#include <stdint.h>              /* 要用 uint16_t，必须包含 */

/* ==================== 转速档位（千分比）==================== */
#define FAN_SPEED_STOP     0u      /*   0%  不转：NORMAL 状态 */
#define FAN_SPEED_LOW      300u    /*  30%  低速：夜里静音用，本工程暂不用 */
#define FAN_SPEED_MID      600u    /*  60%  中速：WARN 状态（温湿度超限） */
#define FAN_SPEED_FULL     1000u   /* 100%  全速：ALARM 状态（明火/用电异常） */
/*  ↑ 为什么写成宏而不是直接在 alarm.c 里写 600：
 *    ① 集中管理，将来改档位只改这一处
 *    ② 名字有意义，alarm.c 里写 Fan_SetSpeed(FAN_SPEED_MID) 比写 600 好读
 *    ③ 后面加 u 是「无符号整数」，和 uint16_t 参数类型一致，避免隐式转换警告 */

/* ==================== 接口 ==================== */
void     Fan_Init(void);
/*  ↑ 上电调一次。作用：启动 TIM3_CH1 的 PWM 输出，占空比归零
 *    必须放在 MX_TIM3_Init() 之后，否则 htim3 还没配置就启动会失败 */

void     Fan_SetSpeed(uint16_t permille);
/*  ↑ 核心接口。参数 0~1000 千分比，超过 1000 自动钳位
 *    0 = 停，600 = 60%，1000 = 全速
 *    内部就是改 CCR 寄存器，立即生效，不阻塞 */

void     Fan_Off(void);
/*  ↑ 停转，等价于 Fan_SetSpeed(0)
 *    单独提供是语义清楚：代码里写 Fan_Off() 一眼知道要干嘛 */

uint16_t Fan_GetSpeed(void);
/*  ↑ 查当前档位，返回 0~1000
 *    用途：调试打印、以后要做「转速显示」时用 */

#endif /* __BSP_FAN_H */          /* 守卫③：结束 */