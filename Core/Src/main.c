/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
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
#include "main.h"
#include "adc.h"
#include "dma.h"
#include "i2c.h"
#include "spi.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "bsp_tick.h"
#include "debug_uart.h"
#include "bsp_led.h"
#include "bsp_key.h"
#include "bsp_smg.h"
#include "bsp_beep.h"
#include "bsp_flame.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */
	HAL_NVIC_SetPriority(SysTick_IRQn, 0, 0);//
  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_SPI2_Init();
  MX_I2C1_Init();
  MX_ADC1_Init();
  MX_TIM2_Init();
  MX_TIM3_Init();
  MX_USART6_UART_Init();
  /* USER CODE BEGIN 2 */
    Tick_Init();
    Log_Init();
    Log_Boot();
    Led_Init();        /* 三个灯全灭，LED1 初值是 SET 必须关 */
		Key_Init();
    Smg_Init();                 /* 启动 TIM2 1ms 中断，数码管开始扫描 */
    Beep_Init();                /* 蜂鸣器先哑着 */
    Flame_Init();               /* PB3/PB4 双边沿 EXTI */


  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
	
	
	
	uint32_t last=0;

	/* M3 自测变量（验证完删掉） */
	uint32_t t_demo = 0;
	uint8_t  demo   = 2;      /* 初值 2，第一次自增才是 0 */

  while (1)
  {
		
		/* ===== M9 EXTI 验证：打火机测哪个脚在报 ===== */
		{
			static uint32_t t_f = 0;
			if (Tick_Expired(&t_f, 1000u)) {
				printf("F:flame=%lu/%d  pir=%lu/%d\r\n",
				       (unsigned long)Flame_GetCount(), (int)Flame_GetLevel(),
				       (unsigned long)Pir_GetCount(),   (int)Pir_GetLevel());
			}
		}

		Key_Scan();

		/* ===== M6 蜂鸣器验证：上电 2 秒后响 2 声（每声 200ms，间隔 200ms） ===== */
		{
			static uint32_t t_b = 0;
			static uint8_t  done = 0;
			if (!done && Tick_Expired(&t_b, 2000u)) {
				Beep_Beep(2u, 200u, 200u);
				printf("BEEP: 2 beeps test\r\n");
				done = 1u;
			}
		}


		/* ===== M5 正式功能：1.234 → 1.111 → 0.000 → 8.888，每秒轮换 ===== */
		{
			static uint32_t t_cy = 0;
			static uint8_t  cy = 0;
			if (Tick_Expired(&t_cy, 1000)) {
				switch (cy) {
					case 0: Smg_ShowDp(1234u, 0); break;   /* 1.234 */
					case 1: Smg_ShowDp(1111u, 0); break;   /* 1.111 */
					case 2: Smg_ShowDp(0u,    0); break;   /* 0.000 */
					case 3: Smg_ShowDp(8888u, 0); break;   /* 8.888 */
				}
				cy = (uint8_t)((cy + 1u) & 3u);
			}
		}

		if(Tick_Expired(&last,500)){
				printf("[%lu]tick alive\r\n",(unsigned long)Tick_Ms());
		}

		/* ===== M3 自测：三态轮播，各 3 秒（验证完删掉） ===== */
		if (Tick_Expired(&t_demo, 3000))
		{
			Led_BlinkStop(LED_ID_L1);
			Led_BlinkStop(LED_ID_L2);
			Led_BlinkStop(LED_ID_L3);

			switch (demo) {
			case 0: Led_On(LED_ID_L1);            printf(">>> NORMAL : LED1 ON\r\n"); break;
			case 1: Led_On(LED_ID_L2);            printf(">>> WARN   : LED2 ON\r\n"); break;
			case 2: Led_BlinkStart(LED_ID_L3,500);printf(">>> ALARM  : LED3 1Hz BLINK\r\n"); break;
			}
			demo = (demo + 1) % 3;
		}

		Led_Task();      /* 每圈必须调用，否则闪不起来 */
		Beep_Task();     /* 每圈必须调用，蜂鸣器计数靠它推进 */
    /* 点灯自测：验证时间基准时先注释掉。
       留着的话一圈主循环 3 秒，串口会 3 秒一行而不是 500ms 一行。
    		LED1ON();
    HAL_Delay(1000);   // 亮1秒（单位毫秒，1000=1s）
    LED1OFF();
    
    LED2ON();
    HAL_Delay(1000);
    LED2OFF();
    
    LED3ON();
    HAL_Delay(1000);
    LED3OFF();
    */
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 8;
  RCC_OscInitStruct.PLL.PLLN = 168;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 4;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
