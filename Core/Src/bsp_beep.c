#include "main.h"
#include "bsp_beep.h"
#include "bsp_tick.h"
#include "gpio.h"

/* 蜂鸣器状态机 —— 只用一个结构体，因为同时只会有一段鸣叫在跑 */
typedef enum {
    BEEP_IDLE = 0,      /* 闲着，一个字节都不发 */
    BEEP_ACTIVE,        /* 正在响 */
    BEEP_GAP            /* 响完一声，等下一声的间隔 */
} BeepState_t;

typedef struct {
    uint32_t     t;       /* 本段开始时刻，交给 Tick_Expired 算过没过期 */
    uint32_t     on_ms;   /* 每声持续多久 */
    uint32_t     off_ms;  /* 两声之间停多久 */
    uint8_t      remain;  /* 还剩几声响（含正在响的这声） */
    BeepState_t  state;
} BeepCtl_t;

static BeepCtl_t s_beep;

void Beep_Init(void){
		BEEP_OFF();						/* 上电先哑着，别一上电就吵 */
		s_beep.state=BEEP_IDLE;
    s_beep.remain = 0;
    s_beep.on_ms  = 0;
    s_beep.off_ms = 0;
    s_beep.t      = 0;
}

void Beep_On(void){
		s_beep.state = BEEP_IDLE;/* 持续响模式不走状态机 */
		s_beep.remain = 0;
		BEEP_ON();
}

void Beep_Off(void)
{
    BEEP_OFF();
}


void Beep_Stop(void)
{
    BEEP_OFF();
    s_beep.state  = BEEP_IDLE;
    s_beep.remain = 0;
}


void Beep_Once(uint32_t on_ms)
{
    Beep_Beep(1u, on_ms, 0u);
}

void Beep_Beep(uint8_t times, uint32_t on_ms, uint32_t off_ms)
{
    if (times == 0u) return;

    s_beep.remain = times;
    s_beep.on_ms  = on_ms;
    s_beep.off_ms = off_ms;
	
    s_beep.state  = BEEP_ACTIVE;
    s_beep.t      = Tick_Ms();
    BEEP_ON();                      /* 立刻响第一声，不等 Task */
}
uint8_t Beep_IsBusy(void)
{
    return (s_beep.state != BEEP_IDLE) ? 1u : 0u;
}


/* ---- 主循环每圈调用一次：只看时间，绝不 Delay ---- */
void Beep_Task(void)
{
    if (s_beep.state == BEEP_IDLE) return;

    if (s_beep.state == BEEP_ACTIVE)
    {
        /* 这一声到点了？没到就返回，主循环继续跑别的 */
        if (!Tick_Expired(&s_beep.t, s_beep.on_ms)) return;

        BEEP_OFF();                 /* 用 OFF 而不是 TOGGLE：翻转两次就白翻了 */
        s_beep.remain--;

        if (s_beep.remain == 0u)
        {
            s_beep.state = BEEP_IDLE;   /* 全响完了，收工 */
            return;
        }

        s_beep.state = BEEP_GAP;    /* 还有下一声，先进间隔 */
        s_beep.t     = Tick_Ms();
    }
    else /* BEEP_GAP */
    {
        if (!Tick_Expired(&s_beep.t, s_beep.off_ms)) return;

        BEEP_ON();
        s_beep.state = BEEP_ACTIVE;
        s_beep.t     = Tick_Ms();
    }
}




