#ifndef __APP_LOG_H
#define __APP_LOG_H

#include "cmsis_os.h"

extern osMessageQueueId_t qLog;

/* 用法和 printf 一样，但是非阻塞的：格式化后丢进队列就走 */
void Log_Printf(const char *fmt, ...);

#endif