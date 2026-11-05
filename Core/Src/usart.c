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
		/* USART3 时钟使能：不开时钟，后面写寄存器全是空操作 */
    __HAL_RCC_USART3_CLK_ENABLE();

    __HAL_RCC_GPIOC_CLK_ENABLE();
    /**USART3 GPIO Configuration
    PC11     ------> USART3_RX
    PC10     ------> USART3_TX
    */
    GPIO_InitStruct.Pin = GPIO_PIN_11|GPIO_PIN_10;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF7_USART3;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

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
		/* 外设时钟关闭 */
    __HAL_RCC_USART3_CLK_DISABLE();

    /**USART3 GPIO Configuration
    PC11     ------> USART3_RX
    PC10     ------> USART3_TX
    */
    HAL_GPIO_DeInit(GPIOC, GPIO_PIN_11|GPIO_PIN_10);
		
		/* USART3 中断关闭 */
    HAL_NVIC_DisableIRQ(USART3_IRQn);
  /* USER CODE BEGIN USART3_MspDeInit 1 */

  /* USER CODE END USART3_MspDeInit 1 */
  }
  else if(uartHandle->Instance==USART6)
  {
  /* USER CODE BEGIN USART6_MspDeInit 0 */

  /* USER CODE END USART6_MspDeInit 0 */
    /* Peripheral clock disable */
		/* 外设时钟关闭 */
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
uint8_t  USART3_RxBuff[BSP_WIFI_RX_BUF_SIZE];   /* 模组收到的字节全倒这里，1024 字节 */
uint16_t USART3_RxCounter = 0;              /* 本帧实际收到多少字节，0 表示没数据 */

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
 * Size 是本次实际收到的字节数。 */
void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size)
{
    if (huart->Instance == USART3)
    {
        /* 给结尾的 '\0' 留位置：strstr 要求字符串以 0 结尾，
           不截断的话第 1024 个字节会写到缓冲区外面去。 */
        if (Size > (BSP_WIFI_RX_BUF_SIZE - 1))
            Size = BSP_WIFI_RX_BUF_SIZE - 1;

        USART3_RxBuff[Size] = '\0';     /* 手动补结束符，strstr 才能用 */
        USART3_RxCounter    = Size;     /* 告诉上层「这一帧有多少字节」 */

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
