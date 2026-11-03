#ifndef __DEBUG_UART_H
#define __DEBUG_UART_H

#include <stdio.h>
#include <stdint.h>

void Log_Init(void);
void Log_Boot(void);
void Log_ErrorUart(const char *tag, int err);

int fputc(int ch, FILE *f);

#endif /* __DEBUG_UART_H */
