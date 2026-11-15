#include <stdio.h>
#include <stdint.h>
#include "debug_uart.h"
#include "main.h"
#include "usart.h"

void Log_Init(void)
{
    /* 串口已由 MX_USART6_UART_Init() 配好，这里暂时不用做事 */
}

void Log_Boot(void)
{
    printf("SYSCLK=%u HCLK=%u PCLK1=%u PCLK2=%u\r\n",
           HAL_RCC_GetSysClockFreq(), HAL_RCC_GetHCLKFreq(),
           HAL_RCC_GetPCLK1Freq(), HAL_RCC_GetPCLK2Freq());
}

void Log_ErrorUart(const char *tag, int err)
{
    printf("[ERR] %s = %d\r\n", tag, err);
}

int fputc(int ch, FILE *f)
{
    uint8_t byte = (uint8_t)ch;                    /* 显式取低 8 位，不依赖字节序 */
    HAL_UART_Transmit(&huart6, (uint8_t *)&ch, 1, 0xFFFF);     /* 一个字符送进串口，最多等 10ms */
    return ch;
}
