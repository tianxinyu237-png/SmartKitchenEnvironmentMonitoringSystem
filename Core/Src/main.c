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
#include "bsp_fan.h"
#include "bsp_adc.h"       
#include "app_power.h"      
#include "bsp_flame.h"
#include "app_state.h"
#include "alarm.h"
#include "drv_sht20.h"       /* 温湿度传感器，非阻塞状态机 */
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
#define PAGE_NORMAL_CNT   5u
#define PAGE_ADMIN_CNT    4u
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
static uint8_t s_demo_alarm = 0;   /* 1 = KEY3 手动演示报警中（绕过真实状态机）
                                    *   放文件作用域是因为 main() 主循环和 Ui_Task()
                                    *   都要看它：主循环决定 Alarm_Apply 传哪个状态，
                                    *   Ui_Task 决定显示报警码还是读数 */

/**
 * @brief  按键响应 + 数码管上屏。主循环每圈调一次
 * @note   必须是独立函数 —— 里面用 return 提前退出（报警态覆盖），
 *         若直接写在 while(1) 里，return 会结束 main 让系统停摆。
 */
static void Ui_Task(void)
{
    /* ---- 状态变量：static 保证跨圈保留，不加就每圈归零 ---- */
    static uint8_t  s_mode     = 0;      /* 0 = NORMAL   1 = ADMIN */
    static uint8_t  s_page     = 0;      /* 当前页号 */
    static uint32_t s_t_blink  = 0;      /* ADMIN 模式整屏闪烁计时 */
    static uint8_t  s_blink_on = 1;      /* 1 = 亮相位  0 = 灭相位 */

    float   v;                           /* 本页要显示的物理量 */
    int32_t n;                           /* ×100 后的整数 */

    /* ==================== ① 按键 ==================== */

    /* KEY1：切模式，切完回第一页（否则页号越界）*/
    if (Key_GetPressEvent(KEY_ID_1)) {
        s_mode = (uint8_t)(s_mode ^ 1u);
        s_page = 0;
    }

    /* KEY2：翻页，页数随模式变 */
    if (Key_GetPressEvent(KEY_ID_2)) {
        if (s_mode == 0u) {
            s_page = (uint8_t)((s_page + 1u) % PAGE_NORMAL_CNT);   /* 5 页 */
        } else {
            s_page = (uint8_t)((s_page + 1u) % PAGE_ADMIN_CNT);    /* 4 个阈值 */
        }
    }

    /* KEY3：报警演示 / 静音切换
     *   ① 没在报警 → 进「演示报警」：灯闪 + 蜂鸣器 + 风扇全速 + 马达开 + 数码管 8888
     *   ② 演示中    → 退出演示，蜂鸣器/风扇/马达一起停
     *   ③ 真报警中  → 静音 ⇄ 响 切换（原有功能） */
    if (Key_GetPressEvent(KEY_ID_3)) {
        if (s_demo_alarm != 0u) {
            /* ---- 退出演示 ----
             * ⚠️ 这里必须显式 Fan_Off() + MOTOR_OFF()，不能只靠下一圈
             *    Alarm_Apply(App_State_Get()) 去关。
             *    原因：Alarm_Apply 开头有 `if (st == s_cur) return;`，
             *    而演示期间 s_cur 已经被设成 SYS_ALARM；要是真实状态
             *    这会儿也是 ALARM（比如电压又跳了），传进去 st==s_cur
             *    直接返回，风扇和马达就留在报警态不动 —— 表现就是
             *    「蜂鸣器停了（Beep_Stop 是直接调的），风扇马达还转」。 */
            s_demo_alarm = 0u;
            Beep_Stop();
            printf("KEY3 EXIT demo: state=%d fan=%u\r\n",
                   (int)App_State_Get(), (unsigned)Fan_GetSpeed());

            if (App_State_Get() != SYS_ALARM) {   /* 真在报警就别硬停 */
                Fan_Off();
                MOTOR_OFF();
            }

            /* 让 alarm.c 里的 s_cur 归位到真实状态：
             * 若真实是 NORMAL/WARN，上面已停过，这里会再走一遍（无害）；
             * 若真实是 ALARM，st==s_cur 返回，硬件保持报警态（正确）。 */
            Alarm_Apply(App_State_Get());
        } else if (App_State_Get() == SYS_ALARM) {
            Alarm_ToggleSilence();           /* 真报警：只切蜂鸣器，灯/风扇/马达继续 */
        } else {
            s_demo_alarm = 1u;               /* 进演示：主循环会 Alarm_Apply(SYS_ALARM) */
            Beep_Stop();                     /* 先把上一声掐掉，让 Alarm_Task 从头响 */
        }
    }

    /* ==================== ② ADMIN 整屏闪烁计时 ==================== */
    /* 500ms 翻转一次 → 1Hz 闪。只在 ADMIN 模式推进，切回 NORMAL 立刻恢复常亮 */
    if (s_mode == 1u) {
        if (Tick_Expired(&s_t_blink, 500u)) {
            s_blink_on = (uint8_t)(s_blink_on ^ 1u);
        }
    } else {
        s_blink_on = 1u;                 /* NORMAL 保持常亮 */
        s_t_blink  = Tick_Ms();          /* 重置计时，下次进 ADMIN 从头算 */
    }

    /* ==================== ③ 报警态强制显示报警码 ==================== */
    /* s_demo_alarm 也要走这里：演示时状态机是 NORMAL，但画面必须像真报警 */
    if (s_demo_alarm != 0u || App_State_Get() == SYS_ALARM) {
        if (s_demo_alarm != 0u || App_State_GetReason() == ALARM_POWER) {
            Smg_ShowNum(8888u);          /* ← 用电异常码：四个 8（演示也用这个） */
        } else {
            Smg_ShowNum(9999u);          /*   明火报警码：四个 9 */
        }
        return;                          /* ⚠️ 退出 Ui_Task，别覆盖报警码 */
    }
		
    /* ==================== ④ 按模式选值 ==================== */
    if(s_mode == 0u) {
        /* ---- NORMAL：5 页测量值 ---- */
        switch (s_page) {
            case 0:  v = Sht20_GetTemp();       break;   /* 温度：28.43 */
            case 1:  v = Sht20_GetHumi();       break;   /* 湿度：57.60 */
            case 2:  /* 电压：5.00 → "05.00"，四位全亮（前导 0 保留，不熄灭） */
                     n = (int32_t)(Power_GetVoltage() * 100.0f + 0.5f);
                     if (n < 0)    { n = 0; }
                     if (n > 9999) { n = 9999; }
                     Smg_ShowDp((uint32_t)n, 1u);
                     return;
            case 3:  /* 电流：直接显示 mA → "0199"，四位全亮（单位 mA，答辩时说清）
                      *  ↑ 为什么不显示 A：0.199A 要显示成 "0.20"，得占 4 位且小数点在
                      *    第一位后，Smg_ShowDp 做出来是 "002.0" —— 会被读成 2A，非常危险。
                      *    显示 mA 没有歧义 */
                     n = (int32_t)(Power_GetCurrent() + 0.5f);
                     if (n < 0)    { n = 0;    }
                     if (n > 9999) { n = 9999; }
                     Smg_ShowNum((uint32_t)n);
                     return;
            default: v = 0.0f;                break;   /* 第 4 页报警码，正常时不显示 */
        }
    }else {
        /* ---- ADMIN：4 个阈值，完整显示 xx.xx + 整屏 1Hz 闪烁 ---- */
        v = App_State_GetThreshold((ThId_t)s_page);
        n = (int32_t)(v * 100.0f + 0.5f);    /* 35.0 → 3500 */
        if (n < 0)    { n = 0;    }
        if (n > 9999) { n = 9999; }

        if (s_blink_on == 0u) {
            Smg_Clear();                     /* 灭相位：整屏熄灭 */
        } else {
            Smg_ShowDp((uint32_t)n, 2u);     /* 亮相位：35.00 / 33.00 / 85.00 / 80.00 */
        }
        /*  ↑ 为什么用「整屏闪」而不是「熄灭某一位」：
         *    熄灭一位看起来就是屏幕坏了（"百位不显示"），
         *    整屏闪烁既能一眼认出是设置模式，四个数字又都完整。
         *  ↑ 为什么灭相位调 Smg_Clear 而不是显示另一个数：
         *    显示另一个数会让屏幕像在"数字乱跳"，反而看不出是闪烁。 */
        return;
    }

    /* ==================== ⑤ NORMAL 换算：×100 四舍五入 + 钳位 ==================== */
    n = (int32_t)(v * 100.0f + 0.5f);    /* 四舍五入到两位小数 */

    if (n < 0)    { n = 0; }             /* 负值钳位，不然强转成 42 亿 */
    if (n > 9999) { n = 9999; }          /* 4 位装不下 */

    /* ==================== ⑥ NORMAL 上屏：带小数点，常亮 ==================== */
    Smg_ShowDp((uint32_t)n, 1u);         /* 27.63 / 56.37 / 5.00 / 0.20 */
}
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
		Fan_Init();
    Flame_Init();               /* PB3 火焰 + PB4 人体红外，双边沿 EXTI */
		Power_Init();            /* ← 新增：启动 ADC + DMA */
		App_State_Init();           
    Alarm_Init();               
    Sht20_Init();               /* 温湿度状态机归位，不碰硬件 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */

  while (1)
  {
    /* ==================== ① 按键采样（必须早于取事件）==================== */
    Key_Scan();                          /* 产生按下事件，下面才有得取 */

    /* ==================== ② 传感器推进 ==================== */
    Sht20_Task();                        /* 温湿度状态机，每圈推进一步，不阻塞 */
    if (Sht20_IsReady()) {               /* 第一轮 114ms 没跑完前不喂，避免误判成 0 度 */
        App_State_SetEnv(Sht20_GetTemp(), Sht20_GetHumi());
    }
		Power_Task();                    /* ← 新增：必须在 App_State_Task 之前 */

    /* ==================== ③ 状态计算 ==================== */
    App_State_Task();                    /* 算状态：ALARM > WARN > NORMAL */

    /* ==================== ④ 动作执行 ==================== */
    /* s_demo_alarm 时强行传 ALARM，让 Alarm_Apply 走报警分支：
     *   灯闪 + 蜂鸣器 + 风扇全速 + 马达开。
     * Alarm_Apply 里有 `if (st == s_cur) return;`，所以只有状态真正
     * 变化那一圈才动作，不会每圈重复重置。 */
    Alarm_Apply((s_demo_alarm != 0u) ? SYS_ALARM : App_State_Get());
    Alarm_Task();                        /* 推进闪烁和间歇鸣叫 */
    Led_Task();
    Beep_Task();

    /* ==================== ⑤ 按键响应 + 数码管上屏 ==================== */
    Ui_Task();                           /* 用本圈最新状态，报警时立刻显示 9999/8888 */

    /* ==================== ⑥ 调试打印（验完删）==================== */
    {
        static uint32_t t_f = 0;
        if (Tick_Expired(&t_f, 1000u)) {
            printf("F:flame=%lu/%d  pir=%lu/%d\r\n",
                   (unsigned long)Flame_GetCount(), (int)Flame_GetLevel(),
                   (unsigned long)Pir_GetCount(),   (int)Pir_GetLevel());
        }
    }
    {
        static uint32_t s_dbg_sht = 0;
        if (Tick_Expired(&s_dbg_sht, 1000u)) {
            printf("SHT T=%.2f RH=%.2f err=%u\r\n",
                   Sht20_GetTemp(), Sht20_GetHumi(), (unsigned)Sht20_GetErrCount());
        }
    }
    {
        static uint32_t last = 0;        /* 心跳 */
        if (Tick_Expired(&last, 500u)) {
            printf("[%lu]tick alive\r\n", (unsigned long)Tick_Ms());
        }
    }
    /* ---- 电压/电流诊断（定位 8888 用，修好后可删）---- */
    {
        static uint32_t s_dbg_pwr = 0;
        if (Tick_Expired(&s_dbg_pwr, 1000u)) {
            printf("PWR run=%d raw0(I)=%u raw1(V)=%u | V=%.2f I=%.1fmA | abn=%d\r\n",
                   (int)Adc_IsRunning(),
                   (unsigned)Adc_GetRaw(ADC_CH_CURR),
                   (unsigned)Adc_GetRaw(ADC_CH_VOLT),
                   Power_GetVoltage(), Power_GetCurrent(),
                   (int)Power_IsAbnormal());
        }
    }

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
