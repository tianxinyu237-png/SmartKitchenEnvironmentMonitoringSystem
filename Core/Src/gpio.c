/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    gpio.c
  * @brief   This file provides code for the configuration
  *          of all used GPIO pins.
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
#include "gpio.h"

/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/*----------------------------------------------------------------------------*/
/* Configure GPIO                                                             */
/*----------------------------------------------------------------------------*/
/* USER CODE BEGIN 1 */

/* USER CODE END 1 */

/** Configure pins as
        * Analog
        * Input
        * Output
        * EVENT_OUT
        * EXTI
*/
void MX_GPIO_Init(void)
{

  GPIO_InitTypeDef GPIO_InitStruct = {0};

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOG_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(BEEP_GPIO_Port, BEEP_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOC, LED3_Pin|MOTOR_Pin|LED2_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(LED1_GPIO_Port, LED1_Pin, GPIO_PIN_RESET);   /* 初值改成灭：原来是 SET，上电会闪一下 */

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(SMG_LATCH_GPIO_Port, SMG_LATCH_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin : BEEP_Pin */
  GPIO_InitStruct.Pin = BEEP_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(BEEP_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : LED3_Pin MOTOR_Pin LED2_Pin LED1_Pin */
  GPIO_InitStruct.Pin = LED3_Pin|MOTOR_Pin|LED2_Pin|LED1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pin : KEY1_Pin */
  GPIO_InitStruct.Pin = KEY1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(KEY1_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : KEY2_Pin KEY3_Pin */
  GPIO_InitStruct.Pin = KEY2_Pin|KEY3_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pin : SMG_LATCH_Pin */
  GPIO_InitStruct.Pin = SMG_LATCH_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(SMG_LATCH_GPIO_Port, &GPIO_InitStruct);

}

/* USER CODE BEGIN 2 */
/*======================LED1============================*/

// 点亮LED1：引脚输出高电平 → NPN三极管导通 → 灯亮
void LED1ON(void)
{
    HAL_GPIO_WritePin(LED1_GPIO_Port, LED1_Pin, GPIO_PIN_RESET);   /* 初值改成灭：原来是 SET，上电会闪一下 */
}

// 熄灭LED1：引脚输出低电平 → 三极管截止 → 灯灭
void LED1OFF(void)
{
    HAL_GPIO_WritePin(LED1_GPIO_Port, LED1_Pin, GPIO_PIN_RESET);
}

// 翻转LED1亮灭状态（亮→灭 / 灭→亮），闪烁常用
void LED1TOGGLE(void){
    HAL_GPIO_TogglePin(LED1_GPIO_Port, LED1_Pin);
}
/* ==================== LED2 ==================== */
void LED2ON(void){
    HAL_GPIO_WritePin(LED2_GPIO_Port, LED2_Pin, GPIO_PIN_SET);
}

void LED2OFF(void){
    HAL_GPIO_WritePin(LED2_GPIO_Port, LED2_Pin, GPIO_PIN_RESET);
}

void LED2TOGGLE(void){
    HAL_GPIO_TogglePin(LED2_GPIO_Port, LED2_Pin);
}

/* ==================== LED3 ==================== */
void LED3ON(void){
    HAL_GPIO_WritePin(LED3_GPIO_Port, LED3_Pin, GPIO_PIN_SET);
}
void LED3OFF(void){
    HAL_GPIO_WritePin(LED3_GPIO_Port, LED3_Pin, GPIO_PIN_RESET);
}
void LED3TOGGLE(void){
    HAL_GPIO_TogglePin(LED3_GPIO_Port, LED3_Pin);
}
//------  ??FAN
/* PC6 在 CubeMX 里配成了 TIM3_CH1（PWM 复用），不是普通 GPIO，
   所以没有 FAN_Pin 这个宏。风扇靠 PWM 调速，M3 阶段这样写：
       HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_1);
       __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, duty);   // duty 0~999
   现在先留空占位，保证 M2 编译过。 */
void FAN_ON(void){
	/* M3 用 PWM 实现 */
}
void FAN_OFF(void){
	/* M3 用 PWM 实现 */
}
/*==========BEEP============*/
void BEEPGpioInit(void)
{
	RCC->AHB1ENR|=(1<<0);
	GPIOA->MODER&=~(1<<30);
	GPIOA->MODER|=(1<<31);
	GPIOA->OTYPER&=~(1<<15);
	GPIOA->OSPEEDR&=~(3<<30);
	GPIOA->PUPDR&=~(3U<<30);
	GPIOA->ODR&=~(1<<15);
}
void BEEP_ON(void){
	/* BSRR 是原子写，比 ODR 的"读-改-写"安全 */
#if BEEP_ACTIVE_HIGH
	GPIOA->BSRR = (1U<<BEEP_PIN_NUM);          /* 高电平 → 响 */
#else
	GPIOA->BSRR = (1U<<(BEEP_PIN_NUM+16));     /* 低电平 → 响 */
#endif
}
void BEEP_OFF(void){
#if BEEP_ACTIVE_HIGH
	GPIOA->BSRR = (1U<<(BEEP_PIN_NUM+16));     /* 拉低 → 不响 */
#else
	GPIOA->BSRR = (1U<<BEEP_PIN_NUM);          /* 拉高 → 不响 */
#endif
}
void BEEP_TOGGLE(void){
	HAL_GPIO_TogglePin(BEEP_GPIO_Port, BEEP_Pin);
}
/*========MOTOR==========*/
void MOTOR_ON(void){
	HAL_GPIO_WritePin(MOTOR_GPIO_Port,MOTOR_Pin,GPIO_PIN_SET);
}
void MOTOR_OFF(void){
	HAL_GPIO_WritePin(MOTOR_GPIO_Port,MOTOR_Pin,GPIO_PIN_RESET);
}
void MOTOR_TOGGLE(void){
	HAL_GPIO_TogglePin(MOTOR_GPIO_Port, MOTOR_Pin);
}










/* USER CODE END 2 */
