#include "main.h"
#include "bsp_flame.h"
#include "bsp_tick.h"
#include "gpio.h"

/* 两个事件源，别改：
 *   PB3 → 火焰传感器（板上 IO Fire 口）    ← 2026-11-04 由 PB2(IO ITR 光电) 改来
 *   PB4 → 人体红外 RDA226（板上 IO PIR 口）
 *
 * 注意 PB3 出厂是 JTDO（JTAG 数据输出），PB4 是 NJTRST。这两个脚能不能当 GPIO
 * 用，取决于 JTAG-DP 有没有被禁用：
 *   · ST-Link 走 SWD 连接时会发 JTAG-to-SWD 切换序列，自动禁用 JTAG-DP，
 *     之后 PB3/PB4 就归 GPIO 管了 —— 本工程 PB4(PIR) 实测可用即为证据。
 *   · 如果哪天 PB3 配了没反应，先确认下载器是 SWD 模式（不是 JTAG 模式）。
 */
#define FLAME_GPIO_PORT   GPIOB
#define FLAME_PIN         GPIO_PIN_3      /* EXTI3 */
#define PIR_PIN           GPIO_PIN_4      /* EXTI4 */

static volatile uint32_t s_flame_cnt;     /* 光电触发次数 */
static volatile uint32_t s_pir_cnt;       /* 人体红外触发次数 */
static volatile uint8_t  s_flame_lvl;
static volatile uint8_t  s_pir_lvl;

void Flame_Init(void)
{
    GPIO_InitTypeDef g = {0};

    __HAL_RCC_GPIOB_CLK_ENABLE();

    /* 双边沿：进入和退出都要能感知，上层才能判断"现在到底有没有" */
    g.Pin   = FLAME_PIN | PIR_PIN;
    g.Mode  = GPIO_MODE_IT_RISING_FALLING;
    g.Pull  = GPIO_PULLUP;             /* 器件开路时保持高，不会乱触发 */
    g.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(FLAME_GPIO_PORT, &g);

    /* 先读初始电平，避免把上电瞬间的假跳变当成事件
     * 必须放在 HAL_GPIO_Init 之后：配好上拉后读到的才是真实值 */
    s_flame_lvl = (uint8_t)HAL_GPIO_ReadPin(FLAME_GPIO_PORT, FLAME_PIN);
    s_pir_lvl   = (uint8_t)HAL_GPIO_ReadPin(FLAME_GPIO_PORT, PIR_PIN);
    s_flame_cnt = 0;
    s_pir_cnt   = 0;

    /* PB3 → EXTI3，PB4 → EXTI4，两个独立向量 */
    HAL_NVIC_SetPriority(EXTI3_IRQn, 5u, 0u);
    HAL_NVIC_EnableIRQ(EXTI3_IRQn);
    HAL_NVIC_SetPriority(EXTI4_IRQn, 5u, 0u);
    HAL_NVIC_EnableIRQ(EXTI4_IRQn);
}

uint8_t Flame_GetLevel(void)  { return s_flame_lvl; }
uint8_t Pir_GetLevel(void)    { return s_pir_lvl; }

uint8_t Flame_IsActive(void)
{
    return (s_flame_lvl == FLAME_ACTIVE_LEVEL) ? 1u : 0u;
}

uint8_t Pir_IsActive(void)
{
    return (s_pir_lvl == PIR_ACTIVE_LEVEL) ? 1u : 0u;
}

uint32_t Flame_GetCount(void) { return s_flame_cnt; }
uint32_t Pir_GetCount(void)   { return s_pir_cnt;   }

/* ---- 中断服务：只记录，绝不在这里 printf ---- */
void EXTI3_IRQHandler(void)
{
    HAL_GPIO_EXTI_IRQHandler(FLAME_PIN);
}

void EXTI4_IRQHandler(void)
{
    HAL_GPIO_EXTI_IRQHandler(PIR_PIN);
}

/* 覆盖 HAL 的弱函数，一次接管两个源，各记各的 */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    if (GPIO_Pin == FLAME_PIN)
    {
        s_flame_cnt++;
        s_flame_lvl = (uint8_t)HAL_GPIO_ReadPin(FLAME_GPIO_PORT, FLAME_PIN);
    }
    else if (GPIO_Pin == PIR_PIN)
    {
        s_pir_cnt++;
        s_pir_lvl = (uint8_t)HAL_GPIO_ReadPin(FLAME_GPIO_PORT, PIR_PIN);
    }
}
