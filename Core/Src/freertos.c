/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * File Name          : freertos.c
  * Description        : Code for freertos applications
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
#include "FreeRTOS.h"
#include "task.h"
#include "main.h"
#include "cmsis_os.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "bsp_ft6336.h"
#include "bsp_ili9341_4line.h"

#include "lvgl.h"
#include "lv_port_disp.h"
#include "lv_port_indev.h"

#include "stdio.h"
#include "ui.h"
#include "ui_custom.h"      /* ★ 手写的 UI 逻辑都在这里，别写进 LVGL_CUT/UI */

/* ★ 新增：任务之间靠这几个头文件通信 */
#include "app_msg.h"        /* SensorData_t + 队列句柄 extern */
#include "app_log.h"
#include "drv_sht20.h"      /* Sht20_* */
#include "drv_ap3216c.h"    /* Ap3216_* */
#include "app_power.h"      /* Power_* */
#include "app_state.h"      /* App_State_* */
#include "bsp_key.h"        /* Key_Scan / Key_GetEvent（阶段 2 用） */
#include "alarm.h"          /* Alarm_Apply / Alarm_Task（阶段 2 用） */

#include "bsp_wifi.h"				/*Wifi_Task*/
#include "app_cloud.h"			/*Cloud_Taks*/
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
/* ---- 句柄与属性 ---- */
osThreadId_t defaultTaskHandle;   /* 留着，CubeMX 默认，空转无害 */
osThreadId_t LCDTaskHandle;       /* DisplayTask */
osThreadId_t SensorTaskHandle;
osThreadId_t AlarmTaskHandle;
osThreadId_t WifiTaskHandle;
osThreadId_t LogTaskHandle;

osMessageQueueId_t qSensorDataHandle;
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
void SensorTask(void *argument);      /* 私有实现，在文件末尾定义 */
void AlarmTask(void *argument);       /* 在 main.c 里，转调当报警任务入口 */
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN Variables */

/* USER CODE END Variables */
/* Definitions for defaultTask */
osThreadId_t defaultTaskHandle;
const osThreadAttr_t defaultTask_attributes = {
  .name = "defaultTask",
  .stack_size = 128 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};
/* Definitions for LCDTask */
osThreadId_t LCDTaskHandle;
const osThreadAttr_t LCDTask_attributes = {
  .name = "LCDTask",
  .stack_size = 1024 * 4,
  .priority = (osPriority_t) osPriorityLow,
};
/* Definitions for SensorTask */
osThreadId_t SensorTaskHandle;
const osThreadAttr_t SensorTask_attributes = {
  .name = "SensorTask",
  .stack_size = 1024 * 4,
  .priority = (osPriority_t) osPriorityAboveNormal,
};
/* Definitions for WifiTask */
osThreadId_t WifiTaskHandle;
const osThreadAttr_t WifiTask_attributes = {
  .name = "WifiTask",
  .stack_size = 1536 * 4,
  .priority = (osPriority_t) osPriorityHigh,
};
/* Definitions for LogTask */
osThreadId_t LogTaskHandle;
const osThreadAttr_t LogTask_attributes = {
  .name = "LogTask",
  .stack_size = 512 * 4,
  .priority = (osPriority_t) osPriorityBelowNormal,
};
/* Definitions for AlarmTask */
osThreadId_t AlarmTaskHandle;
const osThreadAttr_t AlarmTask_attributes = {
  .name = "AlarmTask",
  .stack_size = 512 * 4,
  .priority = (osPriority_t) osPriorityRealtime,
};
/* Definitions for qSensorData */
osMessageQueueId_t qSensorDataHandle;
const osMessageQueueAttr_t qSensorData_attributes = {
  .name = "qSensorData"
};

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */
void vApplicationTickHook(void);
/* USER CODE END FunctionPrototypes */

void StartDefaultTask(void *argument);
void DisplayTask(void *argument);
void SensorTaskEntry(void *argument);
void WifiTaskEntry(void *argument);
void LogTaskEntry(void *argument);
void AlarmTaskEntry(void *argument);

void MX_FREERTOS_Init(void); /* (MISRA C 2004 rule 8.1) */

/**
  * @brief  FreeRTOS initialization
  * @param  None
  * @retval None
  */
void MX_FREERTOS_Init(void) {
  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* USER CODE BEGIN RTOS_MUTEX */
  /* add mutexes, ... */
  /* USER CODE END RTOS_MUTEX */

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  /* add semaphores, ... */
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
  /* start timers, add new ones, ... */
  /* USER CODE END RTOS_TIMERS */

  /* Create the queue(s) */
  /* creation of qSensorData */
  qSensorDataHandle = osMessageQueueNew (4, sizeof(SensorData_t), &qSensorData_attributes);

  /* USER CODE BEGIN RTOS_QUEUES */
  /* add queues, ... */
  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* creation of defaultTask */
  defaultTaskHandle = osThreadNew(StartDefaultTask, NULL, &defaultTask_attributes);

  /* creation of LCDTask */
  LCDTaskHandle = osThreadNew(DisplayTask, NULL, &LCDTask_attributes);

  /* creation of SensorTask */
  SensorTaskHandle = osThreadNew(SensorTaskEntry, NULL, &SensorTask_attributes);

  /* creation of WifiTask */
  WifiTaskHandle = osThreadNew(WifiTaskEntry, NULL, &WifiTask_attributes);

  /* creation of LogTask */
  LogTaskHandle = osThreadNew(LogTaskEntry, NULL, &LogTask_attributes);

  /* creation of AlarmTask */
  AlarmTaskHandle = osThreadNew(AlarmTaskEntry, NULL, &AlarmTask_attributes);

  /* USER CODE BEGIN RTOS_THREADS */
  /* add threads, ... */
  /* USER CODE END RTOS_THREADS */

  /* USER CODE BEGIN RTOS_EVENTS */
  /* add events, ... */
  /* USER CODE END RTOS_EVENTS */

}

/* USER CODE BEGIN Header_StartDefaultTask */
/**
  * @brief  Function implementing the defaultTask thread.
  * @param  argument: Not used
  * @retval None
  */
/* USER CODE END Header_StartDefaultTask */
void StartDefaultTask(void *argument)
{
  /* USER CODE BEGIN StartDefaultTask */
  /* Infinite loop */
  for(;;)
  {
		
    osDelay(1);
  }
  /* USER CODE END StartDefaultTask */
}

/* USER CODE BEGIN Header_DisplayTask */
/**
* @brief Function implementing the LCDTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_DisplayTask */
void DisplayTask(void *argument)
{
  /* USER CODE BEGIN DisplayTask */
  SensorData_t d = {0};          /* 初始化：valid=0 时别留栈垃圾 */
  uint32_t     tick = 0;
  uint32_t     sec  = 0;
  uint32_t     last = HAL_GetTick();

  for (;;) {
      uint32_t now = HAL_GetTick();
      lv_tick_inc(now - last);
      last = now;

      while (osMessageQueueGet(qSensorDataHandle, &d, NULL, 0) == osOK) {
          /* ★ 每一路各看各的就绪标志，不能因为 SHT20 没跑完就把五路全刷成 "--"。
           *   d.valid 只代表「SHT20 这一轮读成功」，跟光照 / 电压 / 电流毫无关系。 */

          /* --- ① 温度 / 湿度：只看 SHT20 --- */
          if (d.valid) {
              lv_label_set_text_fmt(ui_labTempVal, "%.1f", d.temp_c);
              lv_label_set_text_fmt(ui_labHumiVal, "%.1f", d.humi_rh);
          } else {
              lv_label_set_text(ui_labTempVal, "--");
              lv_label_set_text(ui_labHumiVal, "--");
          }

          /* --- ② 光照：只看 AP3216（它要写配置寄存器，上电失败会一直不 ready） --- */
          if (Ap3216_IsReady()) {
              lv_label_set_text_fmt(ui_labLightVal, "%u", (unsigned)d.light_lux);
          } else {
              lv_label_set_text(ui_labLightVal, "--");
          }

          /* --- ③ 电压 / 电流：ADC 永远有值，但 0V 明显是没采样到，算无效 --- */
          if (d.volt_v > 1.0f) {
              lv_label_set_text_fmt(ui_labVoltVal, "%.1f", d.volt_v);
              lv_label_set_text_fmt(ui_labCurrVal, "%.2f", d.curr_a);
          } else {
              lv_label_set_text(ui_labVoltVal, "--");
              lv_label_set_text(ui_labCurrVal, "--");
          }
      }

      lv_timer_handler();
			      Ui_ScreenSwitch_Handle();        /* ← 新增这行：LVGL 处理完输入，这里切屏才安全 */
      osDelay(5);

      if (++tick >= 200) {
          tick = 0;
          sec++;
          lv_label_set_text_fmt(ui_labClock, "%02d:%02d",
                                (10 + sec / 60) % 24, sec % 60);
          Ui_RefreshStatus();        /* ★ 底栏三格 + 顶栏指示灯，1 秒一次 */
          printf("[D] sec=%lu heap=%u sht_rdy=%u ap_err=%lu act=%p\r\n",
                 (unsigned long)sec,
                 (unsigned)xPortGetFreeHeapSize(),
                 (unsigned)Sht20_IsReady(),
                 (unsigned long)Ap3216_GetErrCount(),
                 (void *)lv_scr_act());

          Ui_CfgDebugPrint();        /* 设置页阈值 + 使能位，1 秒一次 */
					}
  }
  /* USER CODE END DisplayTask */
}

/* USER CODE BEGIN Header_SensorTaskEntry */
/**
* @brief Function implementing the SensorTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_SensorTaskEntry */
void SensorTaskEntry(void *argument)
{
  /* USER CODE BEGIN SensorTaskEntry */
  SensorTask(argument);      /* 转进真正的实现，里面死循环不返回 */

  /* USER CODE END SensorTaskEntry */
}

/* USER CODE BEGIN Header_WifiTaskEntry */
/**
* @brief Function implementing the WifiTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_WifiTaskEntry */
void WifiTaskEntry(void *argument)
{
  /* USER CODE BEGIN WifiTaskEntry */
  (void)argument;

  for (;;)
  {
    Wifi_Task();       /* ① 推进 AT 状态机一步（内部用 Tick_Expired 判断节流，不阻塞） */
    Cloud_Task();      /* ② 收远程指令 + 定时上报（内部也是非阻塞的） */
    osDelay(10);       /* ③ ★ 必须给：两个函数都是"推进一步就返回"，不给 delay 会空转吃满 CPU */
  }
  /* USER CODE END WifiTaskEntry */
}

/* USER CODE BEGIN Header_LogTaskEntry */
/**
* @brief Function implementing the LogTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_LogTaskEntry */
void LogTaskEntry(void *argument)
{
  /* USER CODE BEGIN LogTaskEntry */
  /* Infinite loop */
  for(;;)
  {
    osDelay(1);
  }
  /* USER CODE END LogTaskEntry */
}

/* USER CODE BEGIN Header_AlarmTaskEntry */
/**
* @brief Function implementing the AlarmTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_AlarmTaskEntry */
void AlarmTaskEntry(void *argument)
{
  /* USER CODE BEGIN AlarmTaskEntry */
  /* Infinite loop */
  AlarmTask(argument);/* 转调 main.c 里的实现，内部自己 for(;;) 不返回 */
  for(;;)
  {
    osDelay(1);
  }
  /* USER CODE END AlarmTaskEntry */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */
void SensorTask(void *argument)
{
    SensorData_t d;
    uint32_t     cnt = 0;

    for (;;) {
        /* ① 三个驱动都是非阻塞状态机，每圈必须推 —— 少推一个就有一个传感器没读数 */
        Sht20_Task();        /* 内部节流，约 10ms 一步 */
        Ap3216_Task();       /* 内部节流 200ms */
        Power_Task();        /* ADC + DMA，内部节流 */

        /* ② 每 500ms 采一包 */
        if (++cnt >= 50) {           /* 10ms × 50 = 500ms */
            cnt = 0;

            if (Sht20_IsReady()) {
                d.temp_c  = Sht20_GetTemp();
                d.humi_rh = Sht20_GetHumi();
                d.valid   = 1;
            } else {
                d.valid   = 0;       /* 首轮没跑完，别把 0 当温度 */
            }

            d.light_lux = (uint16_t)Ap3216_GetLux();
            d.volt_v    = Power_GetVoltage();
            d.curr_a    = Power_GetCurrent();

            /* ③ 喂给状态机（App_State_Task 要用到温湿度） */
            if (d.valid) {
                App_State_SetEnv(d.temp_c, d.humi_rh);
            }
            App_State_Task();        /* 算 NORMAL / WARN / ALARM */

            /* ④ 送显示任务。timeout=0：队列满了就丢这一包，下一包 500ms 后还有 */
            (void)osMessageQueuePut(qSensorDataHandle, &d, 0, 0);
        }

        osDelay(10);
    }
}


/* USER CODE END Application */

