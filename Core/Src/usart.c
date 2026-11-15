/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    usart.c
  * @brief   This file provides code for the configuration
  *          of the USART instances.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "usart.h"

/* USER CODE BEGIN 0 */
#include <string.h>       /* memset，给 ESP8266_Clear() 用 */
#include <stdio.h>        /* vsnprintf */
#include "bsp_wifi.h"
#include <stdarg.h>       /* va_list   */
/* USER CODE END 0 */

UART_HandleTypeDef huart3;
UART_HandleTypeDef huart6;

/* USART3 init function */

void MX_USART3_UART_Init(void)
{

  /* USER CODE BEGIN USART3_Init 0 */

  /* USER CODE END USART3_Init 0 */

  /* USER CODE BEGIN USART3_Init 1 */

  /* USER CODE END USART3_Init 1 */
  huart3.Instance = USART3;
  huart3.Init.BaudRate = 115200;
  huart3.Init.WordLength = UART_WORDLENGTH_8B;
  huart3.Init.StopBits = UART_STOPBITS_1;
  huart3.Init.Parity = UART_PARITY_NONE;
  huart3.Init.Mode = UART_MODE_TX_RX;
  huart3.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart3.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart3) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART3_Init 2 */

  /* USER CODE END USART3_Init 2 */

}
/* USART6 init function */

void MX_USART6_UART_Init(void)
{

  /* USER CODE BEGIN USART6_Init 0 */

  /* USER CODE END USART6_Init 0 */

  /* USER CODE BEGIN USART6_Init 1 */

  /* USER CODE END USART6_Init 1 */
  huart6.Instance = USART6;
  huart6.Init.BaudRate = 115200;
  huart6.Init.WordLength = UART_WORDLENGTH_8B;
  huart6.Init.StopBits = UART_STOPBITS_1;
  huart6.Init.Parity = UART_PARITY_NONE;
  huart6.Init.Mode = UART_MODE_TX_RX;
  huart6.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart6.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart6) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART6_Init 2 */

  /* USER CODE END USART6_Init 2 */

}

void HAL_UART_MspInit(UART_HandleTypeDef* uartHandle)
{

  GPIO_InitTypeDef GPIO_InitStruct = {0};
  if(uartHandle->Instance==USART3)
  {
  /* USER CODE BEGIN USART3_MspInit 0 */

  /* USER CODE END USART3_MspInit 0 */
    /* USART3 clock enable */
    __HAL_RCC_USART3_CLK_ENABLE();

    __HAL_RCC_GPIOB_CLK_ENABLE();
    /**USART3 GPIO Configuration
    PB10     ------> USART3_TX
    PB11     ------> USART3_RX
    */
    GPIO_InitStruct.Pin = GPIO_PIN_10|GPIO_PIN_11;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF7_USART3;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    /* USART3 interrupt Init */
    HAL_NVIC_SetPriority(USART3_IRQn, 6, 0);
    HAL_NVIC_EnableIRQ(USART3_IRQn);
  /* USER CODE BEGIN USART3_MspInit 1 */

  /* USER CODE END USART3_MspInit 1 */
  }
  else if(uartHandle->Instance==USART6)
  {
  /* USER CODE BEGIN USART6_MspInit 0 */

  /* USER CODE END USART6_MspInit 0 */
    /* USART6 clock enable */
    __HAL_RCC_USART6_CLK_ENABLE();

    __HAL_RCC_GPIOG_CLK_ENABLE();
    /**USART6 GPIO Configuration
    PG14     ------> USART6_TX
    PG9     ------> USART6_RX
    */
    GPIO_InitStruct.Pin = GPIO_PIN_14|GPIO_PIN_9;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF8_USART6;
    HAL_GPIO_Init(GPIOG, &GPIO_InitStruct);

  /* USER CODE BEGIN USART6_MspInit 1 */
    HAL_NVIC_SetPriority(USART6_IRQn, 1, 0);
    HAL_NVIC_EnableIRQ(USART6_IRQn);
  /* USER CODE END USART6_MspInit 1 */
  }
}

void HAL_UART_MspDeInit(UART_HandleTypeDef* uartHandle)
{

  if(uartHandle->Instance==USART3)
  {
  /* USER CODE BEGIN USART3_MspDeInit 0 */

  /* USER CODE END USART3_MspDeInit 0 */
    /* Peripheral clock disable */
    __HAL_RCC_USART3_CLK_DISABLE();

    /**USART3 GPIO Configuration
    PB10     ------> USART3_TX
    PB11     ------> USART3_RX
    */
    HAL_GPIO_DeInit(GPIOB, GPIO_PIN_10|GPIO_PIN_11);

    /* USART3 interrupt Deinit */
    HAL_NVIC_DisableIRQ(USART3_IRQn);
  /* USER CODE BEGIN USART3_MspDeInit 1 */

  /* USER CODE END USART3_MspDeInit 1 */
  }
  else if(uartHandle->Instance==USART6)
  {
  /* USER CODE BEGIN USART6_MspDeInit 0 */

  /* USER CODE END USART6_MspDeInit 0 */
    /* Peripheral clock disable */
    __HAL_RCC_USART6_CLK_DISABLE();

    /**USART6 GPIO Configuration
    PG14     ------> USART6_TX
    PG9     ------> USART6_RX
    */
    HAL_GPIO_DeInit(GPIOG, GPIO_PIN_14|GPIO_PIN_9);

  /* USER CODE BEGIN USART6_MspDeInit 1 */

  /* USER CODE END USART6_MspDeInit 1 */
  }
}

/* USER CODE BEGIN 1 */
/* ================================================================
 * ESP8266 接收通路
 *
 * 老师那份阻塞版 wifi.c 只会去轮询 USART3_RxBuff，从来不碰 UART 驱动，
 * 所以必须有人把数据灌进那个缓冲区 —— 就是这一段：
 * main() 里挂上唯一的一次接收，idle 回调保存这一帧并把自己重新挂上。
 * ================================================================ */
uint8_t  USART3_RxBuff[BSP_WIFI_RX_BUF_SIZE];   /* AT 阶段缓冲，1024 字节 */
uint16_t USART3_RxCounter = 0;              /* 本帧实际收到多少字节，0 表示没数据 */

/* ================================================================
 * 透传业务通道
 *
 * 为什么不能复用 USART3_RxBuff：
 *   进了透传，缓冲区里既有 AT 回显（AT+CIPSEND 后那个 > 提示符），
 *   又有服务器推来的业务数据；谁后到谁把前者覆盖掉。
 *   实测复现：握手还在等 "WIFI GOT IP"，一帧业务数据就把它冲没了。
 *
 * 做法：ISR 里按 '\n' 切成整行，塞进环形队列；主循环 Wifi_TakeFrame() 取。
 *   切行保证了「一行就是一条消息」，不会把两条指令粘成一帧。
 *   队列满就丢新行并计数 —— 宁可丢数据，也不能在 ISR 里等。
 * ================================================================ */
volatile uint8_t g_wifi_passthru = 0;       /* 0 = AT 阶段，1 = 透传阶段 */

/* 这四个 bsp_wifi.c 的 Wifi_TakeFrame / Wifi_FlushRx 要直接用，
   所以不能 static —— 声明在 bsp_wifi.h 里 */
char     s_line[WIFI_LINE_SLOTS][WIFI_LINE_MAX];
volatile uint8_t s_head = 0;                /* 生产者(ISR)写指针 */
volatile uint8_t s_tail = 0;                /* 消费者(主循环)读指针 */
volatile uint8_t s_len  = 0;                /* ISR 正在拼的当前行长度 */
volatile uint32_t s_overflow = 0;           /* 队列满丢弃的行数，调试用 */

/* ISR 每收到一段字节就调一次。只做搬字节，不做解析。 */
void Wifi_RxChunk(const uint8_t *data, uint16_t len)
{
    uint16_t i;

    for (i = 0; i < len; i++)
    {
        char c = (char)data[i];

        if (c == '\n')                      /* 一整行收齐了 */
        {
            uint8_t n = s_len;
            if (n > 0 && s_line[s_head][n - 1] == '\r')
                n--;                        /* 剥掉 CR，只留 LF */

            if (n + 1 >= WIFI_LINE_MAX)     /* 超长行截断，不越界 */
                n = WIFI_LINE_MAX - 2;

            s_line[s_head][n]     = '\n';   /* 统一用 '\n' 结尾 */
            s_line[s_head][n + 1] = '\0';

            {
                uint8_t next = (uint8_t)((s_head + 1) % WIFI_LINE_SLOTS);
                if (next != s_tail)         /* 队列没满才入队 */
                {
                    s_head = next;
                }
                else
                {
                    s_overflow++;           /* 满：丢这一行，不覆盖未读的旧行 */
                }
            }
            s_len = 0;
        }
        else if (s_len < WIFI_LINE_MAX - 2) /* 拼当前行 */
        {
            s_line[s_head][s_len++] = c;
        }
    }
}

/* 两个串口共用的阻塞式 printf，wifi.c 的调试输出全靠它。
 * 这里阻塞是安全的：只在两条 AT 指令之间调用，不占用中断时间。 */
void USART_printf(UART_HandleTypeDef *huart, const char *fmt, ...)
{
    static char pbuf[128];      /* 静态：不占栈，回调里也能用 */
    va_list ap;
    int n;

    va_start(ap, fmt);
    n = vsnprintf(pbuf, sizeof(pbuf) - 1, fmt, ap);
    va_end(ap);

    if (n > 0)                  /* vsnprintf 出错返回负数，别再往串口灌 */
        HAL_UART_Transmit(huart, (uint8_t *)pbuf, (uint16_t)n, 100);
}

/* ------------------------------------------------------------ 7- 接收 */
/* 空闲中断版本：这一帧停止接收的那一刻就触发。
 * Size 是本次实际收到的字节数。
 *
 * 两个阶段的数据必须分开走：
 *   AT 阶段  -> 覆盖 USART3_RxBuff（状态机 strstr 用，只看最后一帧）
 *   透传阶段 -> 按行切进环形队列（业务数据不能丢，也不能和 AT 回显混） */
void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size)
{
    if (huart->Instance == USART3)
    {
        if (g_wifi_passthru)
        {
            Wifi_RxChunk(USART3_RxBuff, Size);   /* 业务通道：切行入队 */
        }
        else
        {
            /* 给结尾的 '\0' 留位置：strstr 要求字符串以 0 结尾，
               不截断的话第 1024 个字节会写到缓冲区外面去。 */
            if (Size > (BSP_WIFI_RX_BUF_SIZE - 1))
                Size = BSP_WIFI_RX_BUF_SIZE - 1;

            USART3_RxBuff[Size] = '\0';     /* 手动补结束符，strstr 才能用 */
            USART3_RxCounter    = Size;     /* 告诉上层「这一帧有多少字节」 */
        }

        /* 无条件重新挂接收：漏了这一句，第一帧之后就永久静默。
           这是本工程最容易漏、也最难查的一行。 */
        HAL_UARTEx_ReceiveToIdle_IT(&huart3, USART3_RxBuff, BSP_WIFI_RX_BUF_SIZE - 1);
    }
}

/* ------------------------------------------------------------ 8- 错误 */
/* 出错只做一件事：重新挂接收。这里绝不能 printf ——
 * 一旦在中断里卡住，板子会停得毫无线索。 */
void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART3)
    {
        HAL_UARTEx_ReceiveToIdle_IT(&huart3, USART3_RxBuff, BSP_WIFI_RX_BUF_SIZE - 1);
    }
}

/* -------- 供 Wifi_Task 发 AT 指令前清缓冲用 */
void ESP8266_Clear(void)
{
    memset(USART3_RxBuff, 0, BSP_WIFI_RX_BUF_SIZE);   /* 整块清零，防上次残留被 strstr 命中 */
    USART3_RxCounter = 0;                         /* 计数一起清，否则上层以为还有数据 */
}

/* USER CODE END 1 */
