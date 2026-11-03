/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    gpio.h
  * @brief   This file contains all the function prototypes for
  *          the gpio.c file
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
#ifndef __GPIO_H__
#define __GPIO_H__

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* USER CODE BEGIN Includes */
/*===========LED=========*/
void LED1ON(void);
void LED1OFF(void);
void LED1TOGGLE(void);
void LED2ON(void);
void LED2OFF(void);
void LED2TOGGLE(void);
void LED3ON(void);
void LED3OFF(void);
void LED3TOGGLE(void);
/*===========FAN===========*/
void FAN_ON(void);
void FAN_OFF(void);
/*=======BEEP==============*/
void BEEPGpioInit(void);
void BEEP_ON(void);
void BEEP_OFF(void);
void BEEP_TOGGLE(void);
/*===========MOTOR=========*/
void MOTOR_ON(void);
void MOTOR_OFF(void);
void MOTOR_TOGGLE(void);
/* USER CODE END Includes */

/* USER CODE BEGIN Private defines */
#define BEEP_PIN_NUM      15      /* BEEP 接 PA15 */
#define BEEP_ACTIVE_HIGH  1       /* 1 = 高电平响（与 gpio.c 初值 RESET 对应） */

/* USER CODE END Private defines */

void MX_GPIO_Init(void);

/* USER CODE BEGIN Prototypes */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin);   /* 定义在 gpio.c，头文件只放声明 */
/* USER CODE END Prototypes */

#ifdef __cplusplus
}
#endif
#endif /*__ GPIO_H__ */

