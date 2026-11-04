#ifndef __DRV_AP3216C_H
#define __DRV_AP3216C_H

#include <stdint.h>

void     Ap3216_Init(void);
void     Ap3216_Task(void);
uint8_t  Ap3216_IsReady(void);
float    Ap3216_GetLux(void);
uint16_t Ap3216_GetAlsRaw(void);
uint16_t Ap3216_GetPsRaw(void);
uint32_t Ap3216_GetErrCount(void);

#endif /* __DRV_AP3216C_H */
