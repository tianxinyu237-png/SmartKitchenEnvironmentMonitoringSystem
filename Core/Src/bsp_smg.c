#include "main.h"
#include "bsp_smg.h"
#include "spi.h"
#include "tim.h"

#define SMG_DIGITS   4

static const uint8_t SEG[10]={
	0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F   //1-9
};
static const uint16_t DIG[SMG_DIGITS] = {0x0100,0x0200,0x0400,0x0800};
static uint8_t s_buf[SMG_DIGITS]; 	/* 4 位各自要显示的段码（缓冲区） */
static uint8_t s_idx=0;					 		/* 下次该刷第几位，0~3 循环 */
static uint8_t s_div=0;            	/* 分频计数：1ms 中断 → 2ms 刷一位 */

/* 刷一位：段码 和 位选 拼成一个 16 位帧发出去 */
static void Smg_Write16(uint16_t v){
		uint8_t b[2];
		b[0]=(uint8_t)(v>>8);						/* 高字节 = 位选 G4~G1 */
		b[1]=(uint8_t)(v&0xFFu);				/* 低字节 = 段码 dp,g~a */
		/* 锁存脚：低 → 移数据 → 高。高电平这一下(上升沿)两片才同时输出 */	
		HAL_GPIO_WritePin(SMG_LATCH_GPIO_Port,SMG_LATCH_Pin,GPIO_PIN_RESET);
		HAL_SPI_Transmit(&hspi2,b,2,10);
	  HAL_GPIO_WritePin(SMG_LATCH_GPIO_Port,SMG_LATCH_Pin,GPIO_PIN_SET);
}

static void Smg_Refresh(void){

		Smg_Write16((uint16_t)s_buf[s_idx]|DIG[s_idx]);/* 低8位段码 + 高位选 *//* 中断：只读内存 */
		s_idx=(uint8_t)((s_idx+1u)&(SMG_DIGITS-1u));/* 0→1→2→3→0；&3 等价 %4，只对 2 的幂成立 */
}


/* TIM2 每 1ms 进来一次，这里二分频 → 2ms 刷一位 → 4 位 8ms → 125Hz 不闪 */
void Smg_Tick(void)
{
    if (++s_div >= 2u) {
        s_div = 0;
        Smg_Refresh();
    }
}

void Smg_Init(void){
	uint8_t i;
	for(i=0;i<SMG_DIGITS;i++){
		s_buf[i]=0; /* 缓冲区清零，上电先显示全灭 */
	}
		s_idx = 0;
    s_div = 0;
    /* ★ 启动 TIM2 1ms 中断。少这一句屏幕永远不亮，因为没人刷 */
    HAL_TIM_Base_Start_IT(&htim2);
}


void Smg_ShowNum(uint32_t num)
{
    Smg_ShowDp(num, SMG_DIGITS);    /* 传 4 表示"没有哪一位要点小数点" */
}



/* 把数字拆成 4 个位，查段码表填进缓冲区；这里不碰硬件 */
void Smg_ShowDp(uint32_t num, uint8_t dp_pos)
{
    uint8_t d[SMG_DIGITS];
    uint8_t i;

    num %= 10000UL;                 /* 超过 9999 就截断，防止数组装不下 */

    d[0] = (uint8_t)(num / 1000UL % 10UL);   /* 千位 */
    d[1] = (uint8_t)(num /  100UL % 10UL);   /* 百位 */
    d[2] = (uint8_t)(num /   10UL % 10UL);   /* 十位 */
    d[3] = (uint8_t)(num          % 10UL);   /* 个位 */

    for (i = 0; i < SMG_DIGITS; i++) {
        s_buf[i] = SEG[d[i]];               /* 数字 → 段码 */
        if (i == dp_pos) s_buf[i] |= 0x80u; /* bit7 = 小数点，|0x80 就点亮它 */
    }
}
void Smg_ShowDigitsDp(uint8_t d0, uint8_t d1, uint8_t d2, uint8_t d3, uint8_t dp_pos)
{
    uint8_t tmp[SMG_DIGITS];
    uint8_t i;

    tmp[0] = d0; tmp[1] = d1; tmp[2] = d2; tmp[3] = d3;

    for (i = 0; i < SMG_DIGITS; i++) {
        /* 0~9 查表显示；其它值（约定传 0xFF）这一位写 0x00 = 全灭
         *  ↑ 必须先判断再查表：SEG[] 只有 10 个元素，
         *    SEG[0xFF] 会读到数组后面 245 字节的野内存，画面上是乱码 */
        s_buf[i] = (tmp[i] <= 9u) ? SEG[tmp[i]] : 0x00u;
        if (i == dp_pos) s_buf[i] |= 0x80u;  /* bit7 = 小数点 */
    }
}

void Smg_ShowDigits(uint8_t d0, uint8_t d1, uint8_t d2, uint8_t d3)
{
    Smg_ShowDigitsDp(d0, d1, d2, d3, SMG_DIGITS);   /* 4 = 不点小数点 */
}

void Smg_Clear(void)
{
    uint8_t i;
    for (i = 0; i < SMG_DIGITS; i++) s_buf[i] = 0;
}

/* ---- TIM2 1ms 回调。HAL 里是 __weak 空函数，这里重写它 ----
 * 注意：所有定时器的更新中断都进这一个函数，必须判断是哪个 */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM2) {
        Smg_Tick();
    }
}











