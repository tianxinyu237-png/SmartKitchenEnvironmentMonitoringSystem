/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    usart.h
  * @brief   本文件是 usart.c 的对外声明：函数原型、共享宏、外部变量。
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
/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __USART_H__
#define __USART_H__

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

extern UART_HandleTypeDef huart3;

extern UART_HandleTypeDef huart6;

/* USER CODE BEGIN Private defines */
/* 和 wifi.c 共用的 ESP8266 接收缓冲区大小。
   1024 字节够装一帧完整的 AT 回显 + 服务器推来的短报文。 */
#define BSP_WIFI_RX_BUF_SIZE 1024
#define ESP_RX_BUF_SIZE  256
/* USER CODE END Private defines */

void MX_USART3_UART_Init(void);
void MX_USART6_UART_Init(void);

/* USER CODE BEGIN Prototypes */
/* 任意 UART 句柄上的阻塞式 printf，实现在 usart.c。
   wifi.c 里所有调试输出都走它。 */
void USART_printf(UART_HandleTypeDef *huart, const char *fmt, ...);

/* 清空 ESP8266 接收缓冲区和计数。
   每次发 AT 指令前都要先清，否则上一次的残留会让 strstr 误命中。 */
void ESP8266_Clear(void);
/* USER CODE END Prototypes */

#ifdef __cplusplus
}
#endif

#endif /* __USART_H__ */

