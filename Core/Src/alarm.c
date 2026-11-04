#include "main.h"
#include "alarm.h"
#include "bsp_led.h"
#include "bsp_beep.h"
#include "bsp_fan.h"
#include "bsp_tick.h"

static SysState_t s_cur      = SYS_NORMAL;
static uint8_t    s_silenced = 0;

static const uint32_t LED_BLINK_MS = 500u;   /* 500ms 亮 500ms 灭 = 1Hz */

void Alarm_Init(void){
		s_cur=SYS_NORMAL;
		s_silenced =0 ;
		
		/* 三个灯全灭、蜂鸣器静音、风扇停 */
		Fan_Off();            /* ← 新增 */
}

uint8_t Alarm_IsSilenced(void) { return s_silenced; }

/* 强制静音（单向）：置位 + 立刻停蜂鸣器。状态不变，灯照闪 */
void Alarm_Silence(void)
{
		s_silenced =1;
		Beep_Stop();
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
        Beep_Stop();                     /* 切到静音：立刻掐掉当前这一声 */
    } else if (s_cur != SYS_ALARM) {
        /* 非报警态按了第二次（s_silenced 回到 0）—— 没有 Alarm_Task 会来响，
         * 所以这里手动补两声，让每次按键都有明确反馈 */
        Beep_Beep(2u, 150u, 150u);
    }
    /* 报警态且 s_silenced==0：交给 Alarm_Task 下一圈自动重响，别在这抢 */
}


void Alarm_Apply(SysState_t st)
{
    /* TODO 8（约 20 行）
     * 先判断状态是否真的变了：
     *     if (st == s_cur) return;      ← 没变就啥也不干，省得重置闪烁
     *     s_cur = st;
     *
     * 然后按 st 分三支，每支都要做"先把上一态的痕迹清掉，再设新的"：
     *
     *   case SYS_ALARM:
     *       Led_Off(LED_ID_L1);
     *       Led_Off(LED_ID_L2);
     *       Led_BlinkStart(LED_ID_L3, LED_BLINK_MS);    ← 1Hz 闪
     *       s_silenced = 0;                              ← 新报警取消静音
     *       （蜂鸣器不在这里开 —— 见 TODO 10，由 Task 管）
     *       break;
     *
     *   case SYS_WARN:
     *       Led_BlinkStop(LED_ID_L3);    ← 先停闪，否则它一直在闪
     *       Led_Off(LED_ID_L1);
     *       Led_On(LED_ID_L2);           ← 常亮
     *       Beep_Stop();                 ← WARN 不响
     *       break;
     *
     *   case SYS_NORMAL:
     *       Led_BlinkStop(LED_ID_L3);
     *       Led_Off(LED_ID_L2);
     *       Led_On(LED_ID_L1);           ← 常亮
     *       Beep_Stop();
     *       break;
     *
     * ⚠️ 每个 case 都要把三个灯都安排到确定状态。
     *    只关自己想关的，会留下上一态的灯还亮着（比如 ALARM→NORMAL，
     *    LED3 如果没 BlinkStop 会一直闪）。
     */
			if (st == s_cur) return;     
          s_cur = st;
					
			switch(st){
					case SYS_ALARM:
					Led_Off(LED_ID_L1);
					Led_Off(LED_ID_L2);
					Led_BlinkStart(LED_ID_L3, LED_BLINK_MS);
					s_silenced = 0;
					Fan_SetSpeed(FAN_SPEED_FULL);   /* ← TODO 9：明火/用电异常 → 全速排风 */
					break;

						case SYS_WARN:
					Led_BlinkStop(LED_ID_L3);
					Led_Off(LED_ID_L1);
					Led_On(LED_ID_L2);
					Beep_Stop();
					Fan_SetSpeed(FAN_SPEED_MID);    /* ← TODO 9：温湿度超限 → 中速换气 */
					break;

						case SYS_NORMAL:
					Led_BlinkStop(LED_ID_L3);
					Led_Off(LED_ID_L2);
					Led_On(LED_ID_L1);
					Beep_Stop();
					Fan_Off();                      /* ← TODO 9：正常 → 停转 */
					break;
					}

		




    /* TODO 9（4 行）风扇
     *   风扇 PC6 = TIM3_CH1（PWM）。
     *   ALARM → 全速（compare 拉到最大）
     *   WARN  → 中速
     *   NORMAL→ 停（或低速）
     *   如果 PWM 还没调通，先用 GPIO 通断代替，标注 TODO 回头改。
     */

			
}


void Alarm_Task(void)
{
    /* TODO 10（约 12 行）蜂鸣器间歇鸣叫
     * 只在 ALARM 且没被消音时干活：
     *     if (s_cur != SYS_ALARM || s_silenced) return;
     *     if (!Beep_IsBusy()) {
     *         Beep_Beep(2u, 150u, 150u);    ← 响两声，间隔 150ms
     *     }
     * ⚠️ 为什么要用 Beep_IsBusy() 判断？
     *    如果每圈都调 Beep_Beep()，remain 被不断重置成 2，
     *    结果是"第一声响 150ms 停了又被重置"，听着像一直响。
     *    只有等它闲下来（响完一轮）才重新触发，才是"间歇"。
     *
     * ⚠️ 为什么不用 Beep_On() 一直响？
     *    需求禁止长鸣不关；而且持续尖叫在答辩现场是扣分项。
     */
	
			if (s_cur != SYS_ALARM || s_silenced) return;
      if (!Beep_IsBusy()) {
              Beep_Beep(2u, 150u, 150u);   
       }

}
	