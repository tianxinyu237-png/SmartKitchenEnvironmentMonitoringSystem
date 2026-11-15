#include "app_log.h"
#include <stdarg.h>
#include <stdio.h>

osMessageQueueId_t qLog;

/* 队列创建：放 MX_FREERTOS_Init 里也行，放 Log_Init 里更清楚 */
void Log_Init(void)
{
    qLog = osMessageQueueNew(8, 96, NULL);    /* 8 条 × 96 字节 */
}

void Log_Printf(const char *fmt, ...)
{
    char    buf[96];
    va_list ap;

    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);     /* ★ 用 vsnprintf，不能卡栈 */
    va_end(ap);

    (void)osMessageQueuePut(qLog, buf, 0, 0); /* 非阻塞，满了就丢 */
}


void LogTask(void *argument){
		char buf[96];
		for(;;){
				if(osMessageQueueGet(qLog,buf,NULL,osWaitForever)==osOK){
						printf("%s",buf); /* 这里才真正阻塞，但只阻塞 LogTask */
				}
		}
}

void WatchdogTask(void *argument)
{
    for (;;) {
        /* 核每个任务的心跳（各自在循环里更新一个计数器） */
        if (AllTasksAlive()) {
            HAL_WWDG_Refresh(&hwwdg);
        }
        osDelay(500);
    }
}