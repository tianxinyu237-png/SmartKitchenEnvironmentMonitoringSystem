#ifndef _APP_MSG_H
#define _APP_MSG_H

#include <stdint.h>
#include <stdint.h>
#include "cmsis_os.h"      /* ★ osMessageQueueId_t 在这里，缺了报未定义类型 */
/* 一次采样的完整结果。★ 队列项大小必须等于 sizeof(SensorData_t) */
typedef struct {
    float    temp_c;      /* 温度 ℃ */
    float    humi_rh;     /* 湿度 %RH */
    uint16_t light_lux;   /* 光照 lux */
    float    volt_v;      /* 电压 V */
    float    curr_a;      /* 电流 A */
    uint8_t  valid;       /* 1 = 这包可信，0 = 传感器还没就绪 */
} SensorData_t;

/* 队列句柄，freertos.c 里创建，两边都用这个 extern */
extern osMessageQueueId_t qSensorDataHandle;


#endif