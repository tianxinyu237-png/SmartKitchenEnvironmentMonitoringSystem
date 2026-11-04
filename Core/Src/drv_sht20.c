/* ============================================================================
 * drv_sht20.c —— SHT20 温湿度传感器驱动（I2C1，非阻塞状态机）
 *
 * 一句话原理：SHT20 是"命令式"器件，不是你问它答，是你发一个命令让它去测，
 *             它内部转换完（温度 85ms / 湿度 29ms）你再回来取结果。
 *             所以这里必须用状态机：发完命令立刻走人，时间到了再回来取。
 * ==========================================================================*/

#include "drv_sht20.h"
#include "i2c.h"        /* 提供 extern I2C_HandleTypeDef hi2c1 */
#include "bsp_tick.h"   /* 提供 Tick_Ms() / Tick_Expired() */

/* ---------- 硬件常量（数据手册定死的，别改） ---------- */
#define SHT20_ADDR_W      0x80u   /* 7 位地址 0x40 左移一位 → HAL 要 8 位写地址 */
#define SHT20_ADDR_R      0x81u   /* 同上，读地址（最低位 1）                   */
#define SHT20_CMD_TEMP    0xF3u   /* 测温度，no hold master（不发 SCL，不阻塞） */
#define SHT20_CMD_HUMI    0xF5u   /* 测湿度，no hold master                     */

/* ---------- 时间常量（数据手册最大转换时间） ---------- */
#define SHT20_TEMP_MS     85u     /* 温度转换最长 85ms                          */
#define SHT20_HUMI_MS     30u     /* 湿度转换最长 29ms，留 1ms 余量             */
#define SHT20_I2C_TMO     50u     /* I2C 单次操作超时(ms)，正常 1ms 就回来      */

/* ---------- 状态机的三个状态 ---------- */
typedef enum {
    SHT_IDLE = 0,     /* 空闲：该发温度命令了                       */
    SHT_WAIT_TEMP,    /* 等温度转换完（85ms）                       */
    SHT_WAIT_HUMI     /* 等湿度转换完（30ms）                       */
} Sht20State_t;

/* ---------- 内部变量（static = 只在本文件可见） ---------- */
static Sht20State_t s_state  = SHT_IDLE;  /* 当前处在哪一步         */
static uint32_t     s_t0     = 0;         /* 上一步动作发生的时刻    */
static float        s_temp   = 0.0f;      /* 最近一次成功测到的温度  */
static float        s_humi   = 0.0f;      /* 最近一次成功测到的湿度  */
static uint8_t      s_ready  = 0;         /* 跑完过一整轮没有        */
static uint32_t     s_err    = 0;         /* I2C 出错次数（调试用）  */

/* ---------- 内部函数：把 16 位 raw 换成物理量 ---------- */
static float Sht20_RawToTemp(uint16_t raw);
static float Sht20_RawToHumi(uint16_t raw);


/* ============================================================================
 * 初始化：只复位软件状态，一行硬件操作都不用写
 * ==========================================================================*/
void Sht20_Init(void)
{
    s_state = SHT_IDLE;   /* 第一次进 Task 要发温度命令，所以起点是 IDLE */
    s_t0    = 0;          /* 还没有"上一步动作"，时刻清 0                 */
    s_temp  = 0.0f;       /* 没测过，数据清零                             */
    s_humi  = 0.0f;
    s_ready = 0;          /* 关键：还没跑完一整轮，数据不可信             */
    s_err   = 0;

    /* 为什么不初始化 I2C？
     *   main() 里 MX_I2C1_Init() 已经把 hi2c1 配好了，这里是驱动层不重复配。
     * 为什么不给 SHT20 发软复位（0xFE）？
     *   它是纯传感器，上电即用，没有"初始化命令"这一步。        */
}


/* ============================================================================
 * 状态机推进：主循环每圈调一次
 * ==========================================================================*/
void Sht20_Task(void)
{
    uint8_t  buf[3];      /* 读回 3 字节：[数据高][数据低][CRC]           */
    uint16_t raw;         /* 高低字节拼成的 16 位原始值                   */
    uint8_t  cmd;         /* 要发给 SHT20 的命令字节（跨 case 用，放顶部） */

    switch (s_state)
    {
    /* ------------------------------------------------------------------
     * 状态 0：发「测温度」命令，然后立刻走人
     * ------------------------------------------------------------------ */
    case SHT_IDLE:
    {
        cmd = SHT20_CMD_TEMP;                             /* 本轮先测温度 */

        if (HAL_I2C_Master_Transmit(&hi2c1, SHT20_ADDR_W,  /* 发给 0x80（写） */
                                    &cmd, 1u,              /* 发 1 个字节      */
                                    SHT20_I2C_TMO)         /* 50ms 超时        */
            == HAL_OK)                                     /* 发成功了吗？     */
        {
            s_t0    = Tick_Ms();        /* 记下"命令发出"的时刻          */
            s_state = SHT_WAIT_TEMP;    /* 切到等温度，下次进来判时间    */
        }
        else
        {
            s_err++;                    /* 失败只计数，留在 IDLE 下轮重试 */
        }                               /* 绝不 while 重试，绝不卡死      */
        break;
    }

    /* ------------------------------------------------------------------
     * 状态 1：等够 85ms，读温度；读完立刻发湿度命令
     * ------------------------------------------------------------------ */
    case SHT_WAIT_TEMP:
    {
        if (!Tick_Expired(&s_t0, SHT20_TEMP_MS))  /* 85ms 还没到？          */
        {
            return;                               /* 直接走人，这一圈几微秒 */
        }

        if (HAL_I2C_Master_Receive(&hi2c1, SHT20_ADDR_R,   /* 从 0x81（读）取 */
                                   buf, 3u,                /* 取 3 个字节      */
                                   SHT20_I2C_TMO)          /* 50ms 超时        */
            != HAL_OK)                                     /* 读失败？         */
        {
            s_err++;                    /* 计数                    */
            s_state = SHT_IDLE;         /* 回起点，下轮重发命令     */
            return;                     /* 这一轮就此作废           */
        }

        raw    = (uint16_t)(((uint16_t)buf[0] << 8) | buf[1]);  /* 高字节在前拼 16 位 */
        s_temp = Sht20_RawToTemp(raw);                          /* 换算成摄氏度      */

        cmd = SHT20_CMD_HUMI;                                      /* 立刻发湿度命令 */
        if (HAL_I2C_Master_Transmit(&hi2c1, SHT20_ADDR_W, &cmd, 1u, SHT20_I2C_TMO) == HAL_OK)
        {
            s_t0    = Tick_Ms();        /* 记下发湿度命令的时刻     */
            s_state = SHT_WAIT_HUMI;    /* 切到等湿度              */
        }
        else
        {
            s_err++;
            s_state = SHT_IDLE;
        }
        break;
    }

    /* ------------------------------------------------------------------
     * 状态 2：等够 30ms，读湿度，一轮结束
     * ------------------------------------------------------------------ */
    case SHT_WAIT_HUMI:
    {
        if (!Tick_Expired(&s_t0, SHT20_HUMI_MS))  /* 30ms 还没到？          */
        {
            return;                               /* 走人，下圈再来         */
        }

        if (HAL_I2C_Master_Receive(&hi2c1, SHT20_ADDR_R, buf, 3u, SHT20_I2C_TMO) != HAL_OK)
        {
            s_err++;
            s_state = SHT_IDLE;
            return;
        }

        raw     = (uint16_t)(((uint16_t)buf[0] << 8) | buf[1]);  /* 拼 16 位         */
        s_humi  = Sht20_RawToHumi(raw);                          /* 换算成 %RH       */
        s_ready = 1u;                    /* 温湿度都读到了，数据从此可信 */
        s_state = SHT_IDLE;              /* 回起点，下一圈开始新一轮     */
        break;
    }

    default:                             /* 状态值被写坏时的兜底          */
    {
        s_state = SHT_IDLE;
        break;
    }
    }
}


/* ============================================================================
 * 换算：16 位 raw → 物理量
 *   温度 = -46.85 + 175.72 × raw / 65536
 *   湿度 =  -6.00 + 125.00 × raw / 65536
 * 常数必须带 f，中间量必须是 float —— 175.72×26798=470万，塞 uint16 会归零
 * ==========================================================================*/
static float Sht20_RawToTemp(uint16_t raw)
{
    return -46.85f + 175.72f * (float)raw / 65536.0f;
}

static float Sht20_RawToHumi(uint16_t raw)
{
    float h = -6.0f + 125.0f * (float)raw / 65536.0f;
    if (h < 0.0f)   { h = 0.0f;   }   /* 手册算出来可能小于 0，钳到 0    */
    if (h > 100.0f) { h = 100.0f; }   /* 也可能超过 100，钳到 100         */
    return h;
}


/* ============================================================================
 * 对外接口
 * ==========================================================================*/
uint8_t Sht20_IsReady(void)
{
    return s_ready;                   /* 0 = 还没有可信数据 */
}

float Sht20_GetTemp(void)
{
    /* 没 ready 时返回 0，配合 IsReady() 就能区分"0℃"和"还没读到" */
    return (s_ready == 1u) ? s_temp : 0.0f;
}

float Sht20_GetHumi(void)
{
    return (s_ready == 1u) ? s_humi : 0.0f;
}

uint32_t Sht20_GetErrCount(void)
{
    return s_err;                     /* 拔掉传感器跑一会儿，看它涨不涨 */
}
