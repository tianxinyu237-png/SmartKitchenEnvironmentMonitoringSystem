#include "main.h"
#include "i2c.h"
#include "bsp_tick.h"
#include "drv_ap3216c.h"

#define AP3216_ADDR        0x3Cu
#define AP3216_REG_SYS     0x00u
#define AP3216_REG_ALS_L   0x0Cu
#define AP3216_REG_PS_L    0x0Eu

#define AP3216_MODE_ALS_PS 0x03u
#define AP3216_LUX_PER_CNT 0.35f
#define AP3216_PERIOD_MS   200u
#define AP3216_I2C_TMO     50u

static uint16_t s_als_raw = 0u;
static uint16_t s_ps_raw  = 0u;
static float    s_lux     = 0.0f;
static uint8_t  s_ready   = 0u;
static uint32_t s_err     = 0u;
static uint32_t s_t_last  = 0u;

void Ap3216_Init(void)
{
    uint8_t mode = AP3216_MODE_ALS_PS;

    s_als_raw = 0u;
    s_ps_raw  = 0u;
    s_lux     = 0.0f;
    s_ready   = 0u;
    s_err     = 0u;

    if (HAL_I2C_Mem_Write(&hi2c1, AP3216_ADDR, AP3216_REG_SYS,
                          I2C_MEMADD_SIZE_8BIT, &mode, 1u, AP3216_I2C_TMO) != HAL_OK) {
        s_err++;
    }
    s_t_last = Tick_Ms();
}

void Ap3216_Task(void)
{
    uint8_t buf[2];

    if (!Tick_Expired(&s_t_last, AP3216_PERIOD_MS)) {
        return;
    }

    if (HAL_I2C_Mem_Read(&hi2c1, AP3216_ADDR, AP3216_REG_ALS_L,
                         I2C_MEMADD_SIZE_8BIT, buf, 2u, AP3216_I2C_TMO) != HAL_OK) {
        s_err++;
        return;
    }

    /* ⚠️ AP3216C 的 ALS 高字节 bit7 是「本次转换无效」标志（1 = 无效）。
       不判断直接拼 16 位的话，无效时 raw 会凭空多出 32768，
       lux 瞬间跳到 11000+ —— 现象是屏幕上的光照值偶尔暴涨一下。 */
    if ((buf[1] & 0x80u) != 0u) {
        return;                         /* 本次数据无效，保留上一次的值 */
    }

    s_als_raw = (uint16_t)(((uint16_t)buf[1] << 8) | (uint16_t)buf[0]);
    s_lux     = (float)s_als_raw * AP3216_LUX_PER_CNT;

    if (HAL_I2C_Mem_Read(&hi2c1, AP3216_ADDR, AP3216_REG_PS_L,
                         I2C_MEMADD_SIZE_8BIT, buf, 2u, AP3216_I2C_TMO) != HAL_OK) {
        s_err++;
        return;
    }

    s_ps_raw = (uint16_t)((((uint16_t)buf[1] & 0x3Fu) << 4) | ((uint16_t)buf[0] & 0x0Fu));

    s_ready = 1u;
}

uint8_t  Ap3216_IsReady(void)     { return s_ready; }
float    Ap3216_GetLux(void)      { return s_lux; }
uint16_t Ap3216_GetAlsRaw(void)   { return s_als_raw; }
uint16_t Ap3216_GetPsRaw(void)    { return s_ps_raw; }
uint32_t Ap3216_GetErrCount(void) { return s_err; }
