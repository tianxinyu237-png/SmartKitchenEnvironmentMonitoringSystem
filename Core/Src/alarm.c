#include "main.h"
#include "gpio.h"      /* MOTOR_ON / MOTOR_OFF —— 报警时开 PC7 马达 */
#include "alarm.h"
#include "bsp_led.h"
#include "bsp_beep.h"
#include "bsp_fan.h"
#include "bsp_tick.h"
#include "app_cloud.h"
static SysState_t s_cur      = SYS_NORMAL;
static uint8_t    s_silenced = 0;

static const uint32_t LED_BLINK_MS = 500u;   /* 500ms 亮 500ms 灭 = 1Hz */

void Alarm_Init(void){
		/* ★ 必须设成非法值，不能写 SYS_NORMAL。
		 *   原因：Alarm_Apply() 第一行是 `if (st == s_cur) return;`，
		 *   上电第一圈传进来的就是 SYS_NORMAL（App_State_Init 里 s_state 也是 NORMAL），
		 *   两边相等 → 整个 switch 被跳过 → case SYS_NORMAL 里的
		 *   Led_On(LED_ID_L1) 永远不执行 → 正常态下 LED1 不亮。
		 *   塞 0xFF 逼第一次 Apply 无条件走一遍分支，跟 Alarm_Reset() 同一个套路。 */
		s_cur=(SysState_t)0xFF;
		s_silenced =0 ;
		
		/* 三个灯全灭、蜂鸣器静音、风扇停、马达停 */
		Fan_Off();            /* ← 新增 */
		MOTOR_OFF();          /* ← 新增：PC7 马达，上电必须是停的 */
}

/* 把状态机内部记录的当前态作废，逼下一圈 Alarm_Apply() 无条件重走分支。
 *
 * 为什么需要这个：Alarm_Apply() 开头是 `if (st == s_cur) return;`，
 * 靠"状态变没变"来决定要不要动硬件。但 s_cur 一旦被错误地留在
 * SYS_ALARM（比如演示模式把它设过去、之后真实状态也是 ALARM），
 * 后面所有 Apply 调用都会被这个 early-return 吃掉，风扇马达永远关不掉。
 *
 * 塞一个非法值 0xFF 进去，下一圈无论真实状态是什么都会 st != s_cur，
 * 于是老老实实走一遍分支 —— 该停的停，该开的开。 */
void Alarm_Reset(void)
{
    s_cur      = (SysState_t)0xFF;
    s_silenced = 0u;
}

uint8_t Alarm_IsSilenced(void) { return s_silenced; }

/* 强制静音（单向）：置位 + 立刻停蜂鸣器。状态不变，灯照闪 */
void Alarm_Silence(void)
{
		s_silenced =1;
		Beep_Stop();
	
		/* ★ 只有真的在报警态才接管风扇/马达。
       WARN / NORMAL 时这两个本来就不用静音管，
       乱停会把"温湿度超限要通风"这个正常功能一起关掉。 */
		if(s_cur == SYS_ALARM){
				Fan_Off();
				MOTOR_OFF();
		}
}

/* 静音 ⇄ 报警 来回切换（绑 KEY3，演示用）
 *
 * 两种情况都管：
 *   ① 正在报警（ALARM 态）：切 s_silenced，真消音 / 恢复响
 *   ② 没在报警（NORMAL / WARN）：手动响两声当反馈 —— 答辩时想验证
 *      蜂鸣器好不好使，不用真去制造一次报警
 *
 * ⚠️ 切回「响」的时候不在这里开蜂鸣器（报警态下）。
 *    下一圈 Alarm_Task 看到 s_silenced==0 会自己 Beep_Beep()。
 *    在这里开反而会和 Task 抢，响声乱掉。
 *
 * ⚠️ 为什么用 ^1 而不是 if/else 两套赋值：
 *    s_silenced 只有 0/1 两种值，^1 就是翻转，一行搞定。
 *    （按键抖动这里不用操心 —— Key_GetPressEvent 已经在驱动层
 *      做了 20ms 去抖，一次按压只产生一个事件） */
void Alarm_ToggleSilence(void)
{
    s_silenced = (uint8_t)(s_silenced ^ 1u);

    if (s_silenced != 0u) {
        Beep_Stop();     /* 切到静音：立刻掐掉当前这一声 */
				if(s_cur == SYS_ALARM){		/*报警态才接管*/
						Fan_Off();
						MOTOR_OFF();
				}
    } else if (s_cur != SYS_ALARM) {
        /* 非报警态按了第二次（s_silenced 回到 0）—— 没有 Alarm_Task 会来响，
         * 所以这里手动补两声，让每次按键都有明确反馈 */
        Beep_Beep(2u, 150u, 150u);
    }else{
				/* ★ 报警态 + 取消静音：把风扇马达拉回来。
           蜂鸣器不在这里开 —— 交给 Alarm_Task 下一圈自己重响，
           在这里开会和 Task 抢，响声乱掉。 */
        Fan_SetSpeed(FAN_SPEED_FULL);
        MOTOR_ON();
		}
    /* 报警态且 s_silenced==0：交给 Alarm_Task 下一圈自动重响，别在这抢 */
}


void Alarm_Apply(SysState_t st)
{
    if (st == s_cur) return;        /* 状态没变什么都不做 */
    s_cur = st;

    switch (st)
    {
    case SYS_ALARM:
        Cloud_ManualExit();         /* ★ 进报警强制退出手动，安全优先 */
        Led_Off(LED_ID_L1);
        Led_Off(LED_ID_L2);
        Led_BlinkStart(LED_ID_L3, LED_BLINK_MS);
        s_silenced = 0;             /* 新报警取消静音 */
        Fan_SetSpeed(FAN_SPEED_FULL);   /* 明火/用电异常 → 全速排风 */
        MOTOR_ON();                     /* 报警：马达一起开（PC7） */
        break;

    case SYS_WARN:
        /* ★ LED 也要看手动覆盖：云端 CMD=LED,VAL=3 点亮的灯，
           下一圈状态机切到 WARN 就会被这里改掉，用户看到的是"指令没生效"。
           蜂鸣器/马达不受保护 —— 安全相关的输出必须本地说了算。 */
        if (!Cloud_IsLedOverridden())
        {
            Led_BlinkStop(LED_ID_L3);
            Led_Off(LED_ID_L1);
            Led_On(LED_ID_L2);
        }
        Beep_Stop();
        if (!Cloud_IsFanOverridden())   /* ★ 手动接管时不动风扇 */
        {
            Fan_SetSpeed(FAN_SPEED_MID);    /* 温湿度超限 → 中速换气 */
        }
        MOTOR_OFF();                    /* 预警不开发电机 */
        break;

    case SYS_NORMAL:
        if (!Cloud_IsLedOverridden())   /* ★ 同上，手动接管时不碰灯 */
        {
            Led_BlinkStop(LED_ID_L3);
            Led_Off(LED_ID_L2);
            Led_On(LED_ID_L1);
        }
        Beep_Stop();
        if (!Cloud_IsFanOverridden())   /* ★ 手动接管时不动风扇 */
        {
            Fan_Off();                      /* 正常 → 停转 */
        }
        MOTOR_OFF();                    /* 正常：马达停 */
        break;
    }
}


void Alarm_Task(void)
{
	
	
		/* ★ 静音兜底：状态保持型输出，每圈压一次。
       为什么用静态标志：MOTOR_ON/OFF 是直接写 GPIO 的函数，
       每圈无条件调会白白反复写寄存器；记住"已经压过了"就不再碰。 */
    static uint8_t s_out_muted = 0u;
		if (s_cur == SYS_ALARM && s_silenced) {
        if (!s_out_muted) {
            if (Fan_GetSpeed() != 0u) Fan_Off();
            MOTOR_OFF();
            s_out_muted = 1u;           /* 压过了，别每圈重复写 */
        }
        return;                          /* 静音态不响 */
    }
    s_out_muted = 0u;                    /* 一旦不静音，下次进来重新压 */
		
		
    /* 蜂鸣器间歇鸣叫（需求：ALARM 时响，但不能长鸣不关）
     * 只在 ALARM 且没被消音时干活。
     * ⚠️ 用 Beep_IsBusy() 判断：如果每圈都调 Beep_Beep()，remain 被不断
     *    重置成 2，结果是"第一声响 150ms 停了又被重置"，听着像一直响。
     *    只有等它闲下来（响完一轮）才重新触发，才是"间歇"。 */
    if (s_cur != SYS_ALARM || s_silenced)
        return;

    if (!Beep_IsBusy())
    {
        Beep_Beep(2u, 150u, 150u);      /* 响两声，间隔 150ms */
    }
}
